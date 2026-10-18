#pragma once

#include "rfc/learning/relational_available_literal_finite_field.hpp"
#include "rfc/learning/relational_available_finite_field_codec.hpp"

namespace rfc::learning {
namespace available_literal_codec_detail {
using namespace available_literal_detail;
using inference::field_program_detail::require;
struct PassiveRule {
    std::uint64_t key{};
    std::vector<std::uint64_t> bindings, symbols;
    std::optional<std::size_t> target;
    std::string unsupported;
    std::vector<inference::literal_input_codec_detail::PassiveInput> inputs;
};
struct PassiveModel {
    available_finite_codec_detail::PassiveModel base;
    std::uint64_t key{}, revision{};
    std::vector<PassiveRule> rules;
    RelationalLiteralFiniteFieldOrigins keys;
    std::vector<Origin> required;
};
inline PassiveModel preflight_model(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, available_literal_detail::model_magic);
    auto base = available_finite_codec_detail::preflight_model(r.blob(), context, policy, limits, check);
    const auto reader = r.blob(); Reader raw(reader, policy);
    header(raw, reader, literal_finite_field_codec_detail::map_magic); canonical(raw.blob(), base.source.native);
    const auto count = raw.count(9); require(count == base.targets.size(), "available literal complete target count");
    for (std::size_t i = 0; i < count; ++i) {
        const auto kind = raw.u8(); const auto target = raw.blob();
        require(kind == base.targets[i].index(), "available literal complete target kind");
        std::visit([&](const auto& t) {
            if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::value_admission_codec_detail::PassiveAvailableProgram>) canonical(target, t.native);
            else canonical(target, t.native.span());
        }, base.targets[i]);
    }
    const auto key = raw.u64(), revision = raw.u64(); const auto nrules = raw.count(41);
    require(key == base.declaration.key && revision == base.declaration.revision && nrules == base.declaration.rules.size(),
        "available literal complete rule declaration differs");
    const auto rows = r.count(8); require(rows == nrules, "available literal origin rows omit rules");
    std::vector<std::vector<std::uint64_t>> keys;
    for (std::size_t i = 0; i < rows; ++i) keys.push_back(finite_keys(r));
    const auto interface = r.u64(); std::vector<Origin> inputs; std::vector<PassiveRule> rules;
    numeric::ExactActionGuard guard(limits, check); guard.dimension(nrules); std::size_t retained = nrules;
    for (std::size_t i = 0; i < nrules; ++i) {
        guard.step(); PassiveRule row; row.key = raw.u64(); row.bindings = finite_keys(raw); row.symbols = finite_keys(raw);
        if (raw.flag()) row.target = size(raw);
        row.unsupported = raw.string(); const auto n = raw.count(8);
        retained = machine::checked_size_add(retained, machine::checked_size_add(n, machine::checked_size_add(row.bindings.size(), row.symbols.size())));
        guard.entries(retained);
        require(n == keys[i].size() && (!row.target || *row.target < base.targets.size()) && (row.target || !n), "available literal input origin shape");
        const auto& expected = base.declaration.rules[i];
        require(row.key == expected.key && row.bindings == expected.bindings && row.symbols == expected.symbols &&
            row.target == expected.target && row.unsupported == expected.unsupported && n == expected.inputs.size(), "available literal rule changes generic declaration");
        for (std::size_t j = 0; j < n; ++j) {
            guard.step(); const auto packet = raw.blob();
            inputs.push_back(context.require_bytes(keys[i][j], Role::Interface, packet));
            auto input = inference::literal_input_codec_detail::passive(packet, policy, limits, check);
            canonical(input.asset, base.source.asset);
            canonical(input.program, available_finite_codec_detail::program_bytes(base.targets[*row.target]));
            row.inputs.push_back(std::move(input));
        }
        rules.push_back(std::move(row));
    }
    require(!raw.remaining(), "trailing passive available literal reader");
    const auto payload = interface_payload(base.archive, storage::PreparedBytePacket::freeze(reader), keys, inputs, policy.max_file_bytes, check);
    auto required = base.required; required.insert(required.end(), inputs.begin(), inputs.end());
    required.push_back(context.require_bytes(interface, Role::Interface, payload.bytes(check)));
    const auto receipts = r.count(8); require(receipts == inputs.size() + 1, "available literal complete receipt count");
    for (const auto& input : inputs) canonical(r.blob(), input.receipt());
    canonical(r.blob(), required.back().receipt()); require(!r.remaining(), "trailing available literal model");
    return {std::move(base), key, revision, std::move(rules), {std::move(keys), interface}, std::move(required)};
}
inline PreparedAvailableRelationalLiteralFiniteFieldMap replay_model(const PassiveModel& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    // All available-map, per-literal and whole-reader obligations precede replay.
    recheck(parsed.required, context, check);
    auto base = available_finite_codec_detail::replay_model(parsed.base, context, policy, limits, check);
    RelationalLiteralFiniteFieldDeclaration d{parsed.key, parsed.revision, {}};
    const auto native = base.map(); const auto finite = base.source().finite();
    for (const auto& row : parsed.rules) {
        RelationalLiteralFiniteFieldRule rule{row.key, row.bindings, row.symbols, row.target, {}, row.unsupported};
        for (const auto& input : row.inputs) {
            const auto program = relational_finite_field_detail::program(native.targets().at(*row.target));
            inference::OwnedRelationalInputAccession accession(finite.asset(), program, input.accession, limits, check);
            rule.inputs.emplace_back(std::move(accession), input.declaration, limits, check);
        }
        d.rules.push_back(std::move(rule));
    }
    PreparedRelationalLiteralFiniteFieldMap reader(finite, native.targets(), std::move(d), limits, check);
    return {std::move(base), std::move(reader), parsed.keys, context, policy.max_file_bytes, check};
}
struct PassiveResult { PassiveModel model; Bytes native; };
inline PassiveResult preflight_result(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, available_literal_detail::result_magic);
    auto model = preflight_model(r.blob(), context, policy, limits, check); const auto native = r.blob();
    require(!r.remaining(), "trailing available literal result");
    return {std::move(model), Bytes(native.begin(), native.end())};
}
inline AvailableRelationalLiteralFiniteFieldResult replay_result(const PassiveResult& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    const auto model = replay_model(parsed.model, context, policy, limits, check); auto result = model.execute(context, limits, check);
    canonical(parsed.native, encode_relational_finite_field_result(result.raw_result())); return result;
}
inline storage::PreparedBytePacket evaluation_bytes(const AvailableRelationalLiteralFiniteFieldEvaluation& evaluation,
    std::size_t max_bytes, const std::function<void()>& check) {
    const auto pin = evaluation; storage::BytePacketBuilder w(available_literal_detail::choice_magic, max_bytes);
    w.blob(available_literal_detail::result_bytes(pin.family(), max_bytes, check)); const auto raw = pin.raw_result(); w.u64(raw.ordinal());
    w.write([&](auto& writer) { relational_affine_codec_detail::vector(writer, raw.free_values()); });
    const auto setting = pin.parameter_setting(); w.u64(setting ? setting->key() : 0); w.u8(setting ? 1 : 0);
    if (setting) w.receipt(*setting);
    const auto origin = pin.origin(); w.u64(origin.key()); w.receipt(origin); w.blob(encode_relational_finite_field_evaluation(raw));
    return std::move(w).finish(check);
}
} // namespace available_literal_codec_detail

inline storage::PreparedBytePacket prepare_available_relational_literal_finite_field_map(
    const PreparedAvailableRelationalLiteralFiniteFieldMap& map, std::size_t max_bytes, std::function<void()> check = {}) {
    return available_literal_detail::model_bytes(map, max_bytes, check);
}
inline std::vector<std::byte> encode_available_relational_literal_finite_field_map(
    const PreparedAvailableRelationalLiteralFiniteFieldMap& map, std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_available_relational_literal_finite_field_map(map, max_bytes, check).bytes(check);
}
inline PreparedAvailableRelationalLiteralFiniteFieldMap decode_available_relational_literal_finite_field_map(
    std::span<const std::byte> bytes, const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_literal_codec_detail; const auto authority = context; const auto read_policy = policy;
    const auto image = frozen_bytes(bytes, read_policy); const auto parsed = preflight_model(image, authority, read_policy, limits, check);
    auto model = replay_model(parsed, authority, read_policy, limits, check);
    canonical(image, prepare_available_relational_literal_finite_field_map(model, read_policy.max_file_bytes, check)); return model;
}
inline storage::PreparedBytePacket prepare_available_relational_literal_finite_field_result(
    const AvailableRelationalLiteralFiniteFieldResult& result, std::size_t max_bytes, std::function<void()> check = {}) {
    return available_literal_detail::result_bytes(result, max_bytes, check);
}
inline std::vector<std::byte> encode_available_relational_literal_finite_field_result(
    const AvailableRelationalLiteralFiniteFieldResult& result, std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_available_relational_literal_finite_field_result(result, max_bytes, check).bytes(check);
}
inline AvailableRelationalLiteralFiniteFieldResult decode_available_relational_literal_finite_field_result(
    std::span<const std::byte> bytes, const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_literal_codec_detail; const auto authority = context; const auto read_policy = policy;
    const auto image = frozen_bytes(bytes, read_policy); const auto parsed = preflight_result(image, authority, read_policy, limits, check);
    auto result = replay_result(parsed, authority, read_policy, limits, check);
    canonical(image, prepare_available_relational_literal_finite_field_result(result, read_policy.max_file_bytes, check)); return result;
}
inline storage::PreparedBytePacket prepare_available_relational_literal_finite_field_evaluation(
    const AvailableRelationalLiteralFiniteFieldEvaluation& evaluation, std::size_t max_bytes, std::function<void()> check = {}) {
    return available_literal_codec_detail::evaluation_bytes(evaluation, max_bytes, check);
}
inline std::vector<std::byte> encode_available_relational_literal_finite_field_evaluation(
    const AvailableRelationalLiteralFiniteFieldEvaluation& evaluation, std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_available_relational_literal_finite_field_evaluation(evaluation, max_bytes, check).bytes(check);
}
inline AvailableRelationalLiteralFiniteFieldEvaluation decode_available_relational_literal_finite_field_evaluation(
    std::span<const std::byte> bytes, const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_literal_codec_detail; const auto authority = context; const auto read_policy = policy;
    const auto image = frozen_bytes(bytes, read_policy); Reader r(image, read_policy); header(r, image, available_literal_detail::choice_magic);
    const auto family = r.blob(); const auto parsed = preflight_result(family, authority, read_policy, limits, check);
    const auto ordinal = size(r); auto free = relational_affine_codec_detail::vector(r); const auto parameter = r.u64(); const auto has_setting = r.flag();
    const available_finite_codec_detail::PassiveResult shadow{{}, parsed.model.base, parsed.native};
    const auto rule_key = available_finite_codec_detail::claimed_rule(shadow, ordinal, read_policy);
    const auto& rules = parsed.model.base.declaration.rules;
    const auto rule = std::find_if(rules.begin(), rules.end(), [&](const auto& row) { return row.key == rule_key; });
    require(rule != rules.end() && rule->target, "available literal choice loses target");
    std::optional<Origin> setting;
    if (const auto* evidence = std::get_if<available_evidence_codec_detail::PassiveEvidence>(&parsed.model.base.targets.at(*rule->target))) {
        require(parameter && has_setting, "available literal affine choice needs setting origin");
        setting = authority.require(parameter, Role::Prospective,
            relational_parameter_setting_payload(evidence->native.span(), evidence->obligations, free));
        canonical(r.blob(), setting->receipt());
    } else require(!parameter && !has_setting && free.empty(), "available literal fixed choice has affine setting");
    storage::BytePacketBuilder payload(available_literal_detail::completion_magic, read_policy.max_file_bytes);
    payload.blob(family); payload.u64(ordinal); payload.write([&](auto& writer) { relational_affine_codec_detail::vector(writer, free); });
    payload.u8(setting ? 1 : 0); if (setting) payload.receipt(*setting);
    const auto prepared = std::move(payload).finish(check); const auto choice = r.u64();
    const auto origin = authority.require_bytes(choice, Role::Prospective, prepared.bytes(check)); canonical(r.blob(), origin.receipt());
    const auto native = r.blob(); require(!r.remaining(), "trailing available literal evaluation");
    // Both the common setting and this complete reader-specific choice have
    // been admitted before replaying the family or executing a field.
    if (setting) authority.recheck(*setting);
    authority.recheck(origin);
    const auto restored = replay_result(parsed, authority, read_policy, limits, check);
    auto result = restored.evaluate(ordinal, std::move(free), parameter, choice, authority, read_policy.max_file_bytes, limits, check);
    canonical(native, encode_relational_finite_field_evaluation(result.raw_result()));
    canonical(image, prepare_available_relational_literal_finite_field_evaluation(result, read_policy.max_file_bytes, check)); return result;
}
} // namespace rfc::learning
