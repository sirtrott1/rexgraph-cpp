#pragma once

#include "rfc/inference/relational_value_admission.hpp"

namespace rfc::inference {
namespace value_admission_codec_detail {
inline constexpr std::array<std::byte, 8> program_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'P'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> result_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'R'}, std::byte{1}, std::byte{0}, std::byte{0}};
} // namespace value_admission_codec_detail
inline std::vector<std::byte> encode_available_relational_program(const PreparedAvailableRelationalProgram& program) {
    using namespace field_program_codec_detail;
    const auto pin = program; auto w = envelope(value_admission_codec_detail::program_magic);
    blob(w, encode_relational_field_program(pin.program())); const auto keys = pin.program_origins();
    w.u64(keys.program); w.u64(keys.maps); w.u64(keys.metrics);
    for (const auto& origin : pin.obligations()) blob(w, origin.receipt());
    return finish(std::move(w));
}
inline PreparedAvailableRelationalProgram decode_available_relational_program(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace field_program_codec_detail;
    value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); bytes = frozen;
    const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    Reader r(bytes, policy); header(r, bytes, value_admission_codec_detail::program_magic);
    const auto program_bytes = r.blob(); const std::vector<std::byte> payload(program_bytes.begin(), program_bytes.end());
    const RelationalProgramOrigins keys{r.u64(), r.u64(), r.u64()};
    const std::array<std::pair<std::uint64_t, RelationalDependencyRole>, 3> roles{{
        {keys.program, RelationalDependencyRole::Program}, {keys.maps, RelationalDependencyRole::Maps},
        {keys.metrics, RelationalDependencyRole::Metrics}}};
    for (const auto [key, role] : roles) {
        if (callback) callback();
        const auto receipt = r.blob(); const auto actual = authority.require(key, role, payload);
        canonical(receipt, actual.receipt());
    }
    value_admission_detail::require(!r.remaining(), "trailing available program bytes");
    if (callback) callback();
    auto parent = decode_relational_field_program(program_bytes, policy, limits);
    PreparedAvailableRelationalProgram out(std::move(parent), keys, authority);
    canonical(bytes, encode_available_relational_program(out)); return out;
}
inline std::vector<std::byte> encode_available_relational_result(const AvailableRelationalFieldResult& result) {
    using namespace field_program_codec_detail;
    const auto pin = result; auto w = envelope(value_admission_codec_detail::result_magic);
    blob(w, encode_accession_field_result(pin.result())); const auto keys = pin.program_origins();
    w.u64(keys.program); w.u64(keys.maps); w.u64(keys.metrics); w.u64(pin.input_origins().size());
    for (const auto input : pin.input_origins()) {
        w.u64(input.asset); w.u64(input.value); w.u64(input.accession); w.u64(input.reconstruction);
    }
    w.u64(pin.obligations().size());
    for (const auto& origin : pin.obligations()) blob(w, origin.receipt());
    return finish(std::move(w));
}
inline AvailableRelationalFieldResult decode_available_relational_result(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace field_program_codec_detail;
    value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); bytes = frozen;
    const auto authority = context; const auto callback = std::move(check); if (callback) callback();
    Reader r(bytes, policy); header(r, bytes, value_admission_codec_detail::result_magic);
    // Admission precedes replay, including admission of the selected parent.
    const auto native = r.blob();
    const RelationalProgramOrigins program_keys{r.u64(), r.u64(), r.u64()};
    const auto count = r.count(32); std::vector<RelationalInputOrigins> input_keys;
    for (std::size_t i = 0; i < count; ++i) input_keys.push_back({r.u64(), r.u64(), r.u64(), r.u64()});
    const auto obligations = r.count(8); std::vector<std::vector<std::byte>> receipts;
    for (std::size_t i = 0; i < obligations; ++i) { const auto image = r.blob(); receipts.emplace_back(image.begin(), image.end()); }
    value_admission_detail::require(!r.remaining(), "trailing available result bytes");
    // Before numerical decoding, resolve every receipt against the independent
    // authority. The encoded origin is never constructed as a trusted owner.
    std::vector<std::pair<std::uint64_t, RelationalDependencyRole>> roles{
        {program_keys.program, RelationalDependencyRole::Program}, {program_keys.maps, RelationalDependencyRole::Maps},
        {program_keys.metrics, RelationalDependencyRole::Metrics}};
    for (const auto keys : input_keys) {
        roles.push_back({keys.asset, RelationalDependencyRole::InputAsset}); roles.push_back({keys.value, RelationalDependencyRole::InputValue});
        roles.push_back({keys.accession, RelationalDependencyRole::InputAccession}); roles.push_back({keys.reconstruction, RelationalDependencyRole::Reconstruction});
    }
    value_admission_detail::require(roles.size() == receipts.size(), "complete original availability obligations required");
    for (std::size_t i = 0; i < roles.size(); ++i) {
        if (callback) callback();
        Reader origin(receipts[i], policy); header(origin, receipts[i], value_admission_detail::origin_magic);
        const auto key = origin.u64(); const auto role = origin.u8(); (void)origin.blob(); const auto payload = origin.blob();
        value_admission_detail::require(!origin.remaining() && key == roles[i].first && role == static_cast<unsigned>(roles[i].second),
            "saved value obligation role/key differs");
        const auto actual = authority.require(key, roles[i].second, {payload.begin(), payload.end()});
        value_admission_detail::require(actual.receipt() == receipts[i], "saved value origin publication or receipt differs");
    }
    // Passively extract actual dependencies from the nested archive. Do not
    // replay claimed represented inputs: even a valid receipt list can be grafted
    // onto another valid native result. Only admitted original inputs are run.
    Reader source(native, policy); header(source, native, field_interface_codec_detail::accession_magic);
    const auto result_bytes = source.blob(); Reader result(result_bytes, policy);
    header(result, result_bytes, field_program_codec_detail::result_magic);
    const auto program_bytes = result.blob();
    const std::vector<std::byte> program_payload(program_bytes.begin(), program_bytes.end());
    (void)authority.require(program_keys.program, RelationalDependencyRole::Program, program_payload);
    (void)authority.require(program_keys.maps, RelationalDependencyRole::Maps, program_payload);
    (void)authority.require(program_keys.metrics, RelationalDependencyRole::Metrics, program_payload);
    struct PassiveInput {
        std::vector<std::byte> asset;
        RelationalInputAccessionDeclaration declaration;
        std::uint64_t use;
        numeric::SparseRationalTensor field;
    };
    std::vector<PassiveInput> original;
    const auto input_count = source.count(70);
    value_admission_detail::require(input_count == input_keys.size(), "complete input origin order required");
    for (std::size_t i = 0; i < input_count; ++i) {
        if (callback) callback();
        const auto asset_image = source.blob();
        auto declaration = field_interface_codec_detail::accession(source); const auto use = source.u64();
        auto field = storage::temporal_codec_detail::tensor(source);
        std::vector<std::byte> asset(asset_image.begin(), asset_image.end());
        const auto value = relational_input_value_payload(asset, declaration, use, field);
        const auto accession = relational_input_accession_payload(program_bytes, asset, declaration, use, field);
        std::vector<OwnedRelationalValueOrigin> checked;
        value_admission_detail::input_origins(checked, input_keys[i], asset, value, accession, authority);
        original.push_back({std::move(asset), std::move(declaration), use, std::move(field)});
    }
    value_admission_detail::require(!source.remaining(), "trailing native accession result bytes");
    if (callback) callback();
    // Legacy program/asset readers are bounded atomic phases. Their internal
    // structural parsing/preparation currently has no checkpoint overload.
    auto parent = decode_relational_field_program(program_bytes, policy, limits);
    const PreparedAvailableRelationalProgram program(std::move(parent), program_keys, authority);
    std::vector<RelationalAvailableInput> inputs;
    for (std::size_t i = 0; i < original.size(); ++i) {
        if (callback) callback();
        auto& value = original[i];
        auto asset = decode_relational_input_use_asset(value.asset, policy, limits);
        inputs.push_back(program.admit_input({std::move(asset), std::move(value.declaration), value.use, std::move(value.field)},
            input_keys[i], authority, limits, callback));
    }
    auto out = program.execute(std::move(inputs), authority, limits, callback);
    canonical(native, encode_accession_field_result(out.result())); canonical(bytes, encode_available_relational_result(out));
    return out;
}
} // namespace rfc::inference
