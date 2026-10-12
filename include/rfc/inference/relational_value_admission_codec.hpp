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
namespace value_admission_codec_detail {
struct PassiveAvailableProgram {
    std::vector<std::byte> native;
    RelationalProgramOrigins keys;
    std::vector<OwnedRelationalValueOrigin> obligations;
};
struct PassiveInput {
    std::vector<std::byte> asset;
    RelationalInputAccessionDeclaration declaration;
    std::uint64_t use;
    numeric::SparseRationalTensor field;
};
struct PassiveAvailableResult {
    std::vector<std::byte> native, program;
    RelationalProgramOrigins keys;
    std::vector<RelationalInputOrigins> input_keys;
    std::vector<PassiveInput> inputs;
    std::vector<OwnedRelationalValueOrigin> obligations;
};
inline PassiveAvailableProgram preflight_program(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    const std::function<void()>& check = {}) {
    using namespace field_program_codec_detail;
    Reader r(bytes, policy); header(r, bytes, value_admission_codec_detail::program_magic);
    const auto native = r.blob(); const RelationalProgramOrigins keys{r.u64(), r.u64(), r.u64()};
    PassiveAvailableProgram out{{native.begin(), native.end()}, keys, {}};
    const std::array<std::pair<std::uint64_t, RelationalDependencyRole>, 3> roles{{
        {keys.program, RelationalDependencyRole::Program}, {keys.maps, RelationalDependencyRole::Maps},
        {keys.metrics, RelationalDependencyRole::Metrics}}};
    for (const auto& [key, role] : roles) {
        if (check) check();
        const auto receipt = r.blob(); const auto actual = context.require(key, role, out.native);
        canonical(receipt, actual.receipt()); out.obligations.push_back(actual);
    }
    value_admission_detail::require(!r.remaining(), "trailing available program bytes"); return out;
}
inline PreparedAvailableRelationalProgram replay_program(const PassiveAvailableProgram& parsed,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, const std::function<void()>& check = {}) {
    for (const auto& origin : parsed.obligations) { if (check) check(); context.recheck(origin); }
    if (check) check();
    auto parent = decode_relational_field_program(parsed.native, policy, limits);
    return PreparedAvailableRelationalProgram(std::move(parent), parsed.keys, context);
}
inline PassiveAvailableResult preflight_result(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    const std::function<void()>& check = {}) {
    using namespace field_program_codec_detail;
    Reader r(bytes, policy); header(r, bytes, value_admission_codec_detail::result_magic);
    const auto native = r.blob(); const RelationalProgramOrigins keys{r.u64(), r.u64(), r.u64()};
    PassiveAvailableResult out{{native.begin(), native.end()}, {}, keys, {}, {}, {}};
    const auto count = r.count(32);
    for (std::size_t i = 0; i < count; ++i) out.input_keys.push_back({r.u64(), r.u64(), r.u64(), r.u64()});
    const auto n = r.count(8); std::vector<std::vector<std::byte>> receipts;
    for (std::size_t i = 0; i < n; ++i) { const auto image = r.blob(); receipts.emplace_back(image.begin(), image.end()); }
    value_admission_detail::require(!r.remaining(), "trailing available result bytes");
    std::vector<std::pair<std::uint64_t, RelationalDependencyRole>> roles{
        {keys.program, RelationalDependencyRole::Program}, {keys.maps, RelationalDependencyRole::Maps}, {keys.metrics, RelationalDependencyRole::Metrics}};
    for (const auto input : out.input_keys) {
        roles.push_back({input.asset, RelationalDependencyRole::InputAsset}); roles.push_back({input.value, RelationalDependencyRole::InputValue});
        roles.push_back({input.accession, RelationalDependencyRole::InputAccession}); roles.push_back({input.reconstruction, RelationalDependencyRole::Reconstruction});
    }
    value_admission_detail::require(roles.size() == receipts.size(), "complete original availability obligations required");
    for (std::size_t i = 0; i < roles.size(); ++i) {
        if (check) check();
        Reader origin(receipts[i], policy); header(origin, receipts[i], value_admission_detail::origin_magic);
        const auto key = origin.u64(); const auto role = origin.u8(); (void)origin.blob(); const auto payload = origin.blob();
        value_admission_detail::require(!origin.remaining() && key == roles[i].first && role == static_cast<unsigned>(roles[i].second), "saved value obligation role/key differs");
        const auto actual = context.require(key, roles[i].second, {payload.begin(), payload.end()});
        canonical(receipts[i], actual.receipt()); out.obligations.push_back(actual);
    }
    Reader source(native, policy); header(source, native, field_interface_codec_detail::accession_magic);
    const auto result_bytes = source.blob(); Reader result(result_bytes, policy);
    header(result, result_bytes, field_program_codec_detail::result_magic);
    const auto program = result.blob(); out.program.assign(program.begin(), program.end());
    (void)context.require(keys.program, RelationalDependencyRole::Program, out.program);
    (void)context.require(keys.maps, RelationalDependencyRole::Maps, out.program);
    (void)context.require(keys.metrics, RelationalDependencyRole::Metrics, out.program);
    const auto inputs = source.count(70); value_admission_detail::require(inputs == out.input_keys.size(), "complete input origin order required");
    for (std::size_t i = 0; i < inputs; ++i) {
        if (check) check();
        const auto image = source.blob(); auto declaration = field_interface_codec_detail::accession(source);
        const auto use = source.u64(); auto field = storage::temporal_codec_detail::tensor(source);
        std::vector<std::byte> asset(image.begin(), image.end());
        const auto value = relational_input_value_payload(asset, declaration, use, field);
        const auto accession = relational_input_accession_payload(out.program, asset, declaration, use, field);
        std::vector<OwnedRelationalValueOrigin> checked;
        value_admission_detail::input_origins(checked, out.input_keys[i], asset, value, accession, context);
        out.inputs.push_back({std::move(asset), std::move(declaration), use, std::move(field)});
    }
    value_admission_detail::require(!source.remaining(), "trailing native accession result bytes"); return out;
}
inline AvailableRelationalFieldResult replay_result(const PassiveAvailableResult& parsed,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, const std::function<void()>& check = {},
    std::optional<OwnedRelationalFieldProgram> declared_parent = {}) {
    for (const auto& origin : parsed.obligations) { if (check) check(); context.recheck(origin); }
    if (check) check();
    if (declared_parent) field_program_codec_detail::canonical(parsed.program, encode_relational_field_program(*declared_parent));
    // Explicit archive adoption into one checked parent, never a live coercion.
    auto parent = declared_parent ? *declared_parent : decode_relational_field_program(parsed.program, policy, limits);
    const PreparedAvailableRelationalProgram program(std::move(parent), parsed.keys, context);
    std::vector<RelationalAvailableInput> inputs;
    for (std::size_t i = 0; i < parsed.inputs.size(); ++i) {
        if (check) check();
        const auto& value = parsed.inputs[i]; auto asset = decode_relational_input_use_asset(value.asset, policy, limits);
        inputs.push_back(program.admit_input({std::move(asset), value.declaration, value.use, value.field}, parsed.input_keys[i], context, limits, check));
    }
    auto out = program.execute(std::move(inputs), context, limits, check);
    field_program_codec_detail::canonical(parsed.native, encode_accession_field_result(out.result())); return out;
}
} // namespace value_admission_codec_detail
inline PreparedAvailableRelationalProgram decode_available_relational_program(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = value_admission_codec_detail::preflight_program(frozen, authority, policy, callback);
    auto out = value_admission_codec_detail::replay_program(parsed, authority, policy, limits, callback);
    field_program_codec_detail::canonical(frozen, encode_available_relational_program(out)); return out;
}
inline AvailableRelationalFieldResult decode_available_relational_result(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = value_admission_codec_detail::preflight_result(frozen, authority, policy, callback);
    auto out = value_admission_codec_detail::replay_result(parsed, authority, policy, limits, callback);
    field_program_codec_detail::canonical(frozen, encode_available_relational_result(out)); return out;
}
} // namespace rfc::inference
