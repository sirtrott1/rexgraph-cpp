#pragma once

#include "rfc/learning/relational_available_finite_field.hpp"

namespace rfc::learning {
namespace available_finite_codec_detail {
using namespace available_finite_detail;
struct PassiveSource {
    Bytes archive, native, asset;
    inference::RelationalFiniteSourceDeclaration declaration;
    Status status;
    RelationalFiniteSourceOrigins keys;
    std::vector<Origin> obligations;
};
inline PassiveSource preflight_source(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, source_magic); const auto native = r.blob();
    Reader raw(native, policy); header(raw, native, inference::relational_finite_source_codec_detail::source_magic);
    const auto asset = raw.blob(); auto d = inference::relational_finite_source_codec_detail::declaration(raw);
    inference::field_program_detail::require(!raw.remaining(), "trailing passive finite source");
    const auto status = static_cast<Status>(r.u8()); const RelationalFiniteSourceOrigins keys{r.u64(), r.u64()};
    const Bytes asset_bytes(asset.begin(), asset.end()); std::vector<Origin> origins;
    if (check) { check(); } origins.push_back(context.require(keys.asset, Role::InputAsset, asset_bytes)); canonical(r.blob(), origins.back().receipt());
    if (check) { check(); } origins.push_back(context.require(keys.constraints, relationship_role(status), relational_finite_constraint_payload(asset_bytes, d, status))); canonical(r.blob(), origins.back().receipt());
    inference::field_program_detail::require(!r.remaining(), "trailing available finite source");
    return {Bytes(bytes.begin(), bytes.end()), Bytes(native.begin(), native.end()), asset_bytes, std::move(d), status, keys, std::move(origins)};
}
inline PreparedAvailableRelationalFiniteSource replay_source(const PassiveSource& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    recheck(parsed.obligations, context, check);
    auto asset = inference::decode_relational_input_use_asset(parsed.asset, policy, limits);
    PreparedAvailableRelationalFiniteSource source(std::move(asset), parsed.declaration, parsed.status, parsed.keys, context, limits, check);
    canonical(parsed.native, inference::encode_relational_finite_source(source.finite())); canonical(parsed.archive, source_bytes(source)); return source;
}
using PassiveTarget = std::variant<inference::value_admission_codec_detail::PassiveAvailableProgram,
    available_evidence_codec_detail::PassiveEvidence>;
inline Bytes program_bytes(const PassiveTarget& target) {
    return std::visit([](const auto& t) -> Bytes {
        if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::value_admission_codec_detail::PassiveAvailableProgram>) return t.native;
        else return t.program.native;
    }, target);
}
inline std::vector<Origin> obligations(const PassiveTarget& target) { return std::visit([](const auto& t) { return t.obligations; }, target); }
struct PassiveModel {
    Bytes archive;
    PassiveSource source;
    std::vector<PassiveTarget> targets;
    std::vector<std::pair<std::uint8_t, Bytes>> target_archives;
    PassiveDeclaration declaration;
    Status status;
    RelationalFiniteMapOrigins keys;
    std::vector<Origin> required;
    Bytes native;
};
inline PassiveModel preflight_model(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, model_magic); auto source = preflight_source(r.blob(), context, policy, check);
    auto required = source.obligations; std::vector<PassiveTarget> targets; std::vector<std::pair<std::uint8_t, Bytes>> archives; std::vector<Bytes> programs;
    const auto count = r.count(9);
    for (std::size_t i = 0; i < count; ++i) {
        if (check) { check(); } const auto kind = r.u8(); const auto data = r.blob();
        inference::field_program_detail::require(kind < 2, "available finite target discriminant");
        targets.push_back(!kind ? PassiveTarget{inference::value_admission_codec_detail::preflight_program(data, context, policy, check)} :
            PassiveTarget{available_evidence_codec_detail::preflight_evidence(data, context, policy, limits, check)});
        archives.push_back({kind, Bytes(data.begin(), data.end())}); auto origins = obligations(targets.back());
        required.insert(required.end(), origins.begin(), origins.end()); programs.push_back(program_bytes(targets.back()));
    }
    auto d = schema(r); const auto status = static_cast<Status>(r.u8()); const RelationalFiniteMapOrigins keys{r.u64(), r.u64()};
    const auto inputs = input_origins(d, source.asset, programs, context, check); required.insert(required.end(), inputs.begin(), inputs.end());
    const auto payload = interpretation_payload(source.archive, archives, d, status, inputs);
    if (check) { check(); } required.push_back(context.require(keys.maps, Role::Maps, payload));
    if (check) { check(); } required.push_back(context.require(keys.relationship, relationship_role(status), payload));
    const auto receipts = r.count(8); inference::field_program_detail::require(receipts == required.size(), "available finite complete receipt count");
    for (const auto& origin : required) canonical(r.blob(), origin.receipt());
    const auto native = r.blob(); inference::field_program_detail::require(!r.remaining(), "trailing available finite model");
    return {Bytes(bytes.begin(), bytes.end()), std::move(source), std::move(targets), std::move(archives), std::move(d), status, keys,
        std::move(required), Bytes(native.begin(), native.end())};
}
inline PreparedAvailableRelationalFiniteFieldMap replay_model(const PassiveModel& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    // Every rule and target has been admitted before the first nested replay.
    recheck(parsed.required, context, check); auto source = replay_source(parsed.source, context, policy, limits, check);
    std::vector<AvailableRelationalFiniteFieldTarget> targets;
    for (const auto& target : parsed.targets) {
        targets.push_back(std::visit([&](const auto& t) -> AvailableRelationalFiniteFieldTarget {
            if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::value_admission_codec_detail::PassiveAvailableProgram>)
                return inference::value_admission_codec_detail::replay_program(t, context, policy, limits, check);
            else return available_evidence_codec_detail::replay_evidence(t, context, policy, check);
        }, target));
    }
    RelationalAvailableFiniteFieldMapDeclaration d{parsed.declaration.key, parsed.declaration.revision, {}};
    const auto finite = source.finite(); const auto asset = finite.asset();
    for (const auto& rule : parsed.declaration.rules) {
        RelationalAvailableFiniteFieldRule row{rule.key, rule.bindings, rule.symbols, rule.target, {}, rule.unsupported};
        for (const auto& input : rule.inputs) row.inputs.push_back({{asset, input.declaration, input.use, input.field}, input.keys});
        d.rules.push_back(std::move(row));
    }
    PreparedAvailableRelationalFiniteFieldMap model(std::move(source), std::move(targets), std::move(d), parsed.status, parsed.keys, context, limits, check);
    canonical(parsed.native, encode_relational_finite_field_map(model.map())); canonical(parsed.archive, model_bytes(model)); return model;
}
struct PassiveResult { Bytes archive; PassiveModel model; Bytes native; };
inline PassiveResult preflight_result(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, result_magic); auto model = preflight_model(r.blob(), context, policy, limits, check);
    const auto native = r.blob(); inference::field_program_detail::require(!r.remaining(), "trailing available finite result");
    return {Bytes(bytes.begin(), bytes.end()), std::move(model), Bytes(native.begin(), native.end())};
}
inline AvailableRelationalFiniteFieldResult replay_result(const PassiveResult& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    const auto model = replay_model(parsed.model, context, policy, limits, check); auto result = model.execute(context, limits, check);
    canonical(parsed.native, encode_relational_finite_field_result(result.result())); canonical(parsed.archive, result_bytes(result)); return result;
}
inline std::uint64_t claimed_rule(const PassiveResult& family, std::size_t ordinal, const storage::TemporalReadLimits& policy) {
    using inference::field_program_detail::require;
    Reader r(family.native, policy); header(r, family.native, relational_finite_field_codec_detail::result_magic);
    canonical(r.blob(), family.model.native); const auto witness = r.blob(); require(!r.remaining(), "trailing claimed finite native result");
    Reader w(witness, policy); (void)size(w); const auto sections = w.count(8);
    for (std::size_t i = 0; i < sections; ++i) (void)finite_keys(w);
    const auto branches = w.count(18); require(ordinal < branches, "claimed finite choice ordinal"); std::optional<std::uint64_t> selected;
    for (std::size_t i = 0; i < branches; ++i) {
        const auto actual_ordinal = size(w); (void)size(w); std::optional<std::uint64_t> rule;
        if (w.flag()) rule = w.u64();
        const auto status = w.u8(); const auto value = w.flag(); if (value) { (void)w.u8(); (void)w.blob(); }
        if (i == ordinal) { require(actual_ordinal == ordinal && rule && value && status == static_cast<std::uint8_t>(RelationalFiniteFieldStatus::Interpreted), "claimed finite choice is not interpreted"); selected = rule; }
    }
    require(!w.remaining() && selected.has_value(), "claimed finite choice loses branch metadata"); return *selected;
}
struct PassiveEvaluation {
    PassiveResult family;
    std::size_t ordinal;
    numeric::RationalVector free;
    std::optional<Origin> setting;
    Origin choice;
    Bytes native;
};
inline PassiveEvaluation preflight_evaluation(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, choice_magic); auto family = preflight_result(r.blob(), context, policy, limits, check);
    const auto ordinal = size(r); auto free = relational_affine_codec_detail::vector(r); const auto parameter = r.u64(); const auto has_setting = r.flag();
    const auto key = claimed_rule(family, ordinal, policy); const auto& rules = family.model.declaration.rules;
    const auto rule = std::find_if(rules.begin(), rules.end(), [&](const auto& row) { return row.key == key; });
    inference::field_program_detail::require(rule != rules.end() && rule->target && *rule->target < family.model.targets.size(), "claimed finite choice loses target");
    std::optional<Origin> setting;
    if (const auto* evidence = std::get_if<available_evidence_codec_detail::PassiveEvidence>(&family.model.targets[*rule->target])) {
        inference::field_program_detail::require(parameter && has_setting, "affine finite choice needs global setting receipt"); if (check) { check(); }
        setting = context.require(parameter, Role::Prospective, relational_parameter_setting_payload(evidence->native.span(), evidence->obligations, free)); canonical(r.blob(), setting->receipt());
    } else inference::field_program_detail::require(!parameter && !has_setting && free.empty(), "fixed finite choice has a spurious parameter setting");
    const auto choice_key = r.u64(); const auto receipt = r.blob(); if (check) { check(); }
    auto origin = context.require(choice_key, Role::Prospective, completion_payload(family.archive, ordinal, free, setting)); canonical(receipt, origin.receipt());
    const auto native = r.blob(); inference::field_program_detail::require(!r.remaining(), "trailing available finite evaluation");
    return {std::move(family), ordinal, std::move(free), std::move(setting), std::move(origin), Bytes(native.begin(), native.end())};
}
inline AvailableRelationalFiniteFieldEvaluation replay_evaluation(const PassiveEvaluation& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check) {
    // Setting and input-specific choice were checked before family replay.
    if (parsed.setting) { context.recheck(*parsed.setting); } context.recheck(parsed.choice);
    const auto family = replay_result(parsed.family, context, policy, limits, check);
    auto result = family.evaluate(parsed.ordinal, parsed.free, parsed.setting ? parsed.setting->key() : 0, parsed.choice.key(), context, limits, check);
    canonical(parsed.native, encode_relational_finite_field_evaluation(result.result())); return result;
}
inline Bytes sections_witness(const inference::RelationalFiniteSections& sections) {
    Writer w; w.u64(sections.candidate_count()); w.u64(sections.size());
    for (std::size_t i = 0; i < sections.size(); ++i) { finite_keys(w, sections.assignment(i)); } return std::move(w).take_bytes();
}
inline Bytes evaluation_bytes(const AvailableRelationalFiniteFieldEvaluation& evaluation) {
    const auto pin = evaluation; auto w = envelope(choice_magic); blob(w, result_bytes(pin.family())); const auto native = pin.result(); w.u64(native.ordinal());
    relational_affine_codec_detail::vector(w, native.free_values()); const auto setting = pin.parameter_setting(); w.u64(setting ? setting->key() : 0); w.u8(setting ? 1 : 0);
    if (setting) { blob(w, setting->receipt()); } const auto origin = pin.origin(); w.u64(origin.key()); blob(w, origin.receipt());
    blob(w, encode_relational_finite_field_evaluation(native)); return finish(std::move(w));
}
} // namespace available_finite_codec_detail
inline std::vector<std::byte> encode_available_relational_finite_source(const PreparedAvailableRelationalFiniteSource& source) { return available_finite_detail::source_bytes(source); }
inline PreparedAvailableRelationalFiniteSource decode_available_relational_finite_source(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_finite_codec_detail; const auto authority = context; const auto read_policy = policy; const auto frozen = frozen_bytes(bytes, read_policy);
    const auto parsed = preflight_source(frozen, authority, read_policy, check); auto source = replay_source(parsed, authority, read_policy, limits, check);
    canonical(frozen, source_bytes(source)); return source;
}
inline std::vector<std::byte> encode_available_relational_finite_sections(const AvailableRelationalFiniteSections& sections) {
    using namespace available_finite_codec_detail; const auto pin = sections; auto w = envelope(sections_magic); blob(w, source_bytes(pin.source())); blob(w, sections_witness(pin.sections())); return finish(std::move(w));
}
inline AvailableRelationalFiniteSections decode_available_relational_finite_sections(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_finite_codec_detail; const auto authority = context; const auto read_policy = policy; const auto frozen = frozen_bytes(bytes, read_policy);
    Reader r(frozen, read_policy); header(r, frozen, sections_magic); const auto parsed = preflight_source(r.blob(), authority, read_policy, check); const auto expected = r.blob();
    inference::field_program_detail::require(!r.remaining(), "trailing available finite sections"); const auto source = replay_source(parsed, authority, read_policy, limits, check);
    auto sections = source.sections(authority, limits, check); canonical(expected, sections_witness(sections.sections())); canonical(frozen, encode_available_relational_finite_sections(sections)); return sections;
}
inline std::vector<std::byte> encode_available_relational_finite_field_map(const PreparedAvailableRelationalFiniteFieldMap& model) { return available_finite_detail::model_bytes(model); }
inline PreparedAvailableRelationalFiniteFieldMap decode_available_relational_finite_field_map(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_finite_codec_detail; const auto authority = context; const auto read_policy = policy; const auto frozen = frozen_bytes(bytes, read_policy);
    const auto parsed = preflight_model(frozen, authority, read_policy, limits, check); auto model = replay_model(parsed, authority, read_policy, limits, check);
    canonical(frozen, model_bytes(model)); return model;
}
inline std::vector<std::byte> encode_available_relational_finite_field_result(const AvailableRelationalFiniteFieldResult& result) { return available_finite_detail::result_bytes(result); }
inline AvailableRelationalFiniteFieldResult decode_available_relational_finite_field_result(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_finite_codec_detail; const auto authority = context; const auto read_policy = policy; const auto frozen = frozen_bytes(bytes, read_policy);
    const auto parsed = preflight_result(frozen, authority, read_policy, limits, check); auto result = replay_result(parsed, authority, read_policy, limits, check);
    canonical(frozen, result_bytes(result)); return result;
}
inline std::vector<std::byte> encode_available_relational_finite_field_evaluation(const AvailableRelationalFiniteFieldEvaluation& evaluation) { return available_finite_codec_detail::evaluation_bytes(evaluation); }
inline AvailableRelationalFiniteFieldEvaluation decode_available_relational_finite_field_evaluation(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_finite_codec_detail; const auto authority = context; const auto read_policy = policy; const auto frozen = frozen_bytes(bytes, read_policy);
    const auto parsed = preflight_evaluation(frozen, authority, read_policy, limits, check); auto result = replay_evaluation(parsed, authority, read_policy, limits, check);
    canonical(frozen, evaluation_bytes(result)); return result;
}
} // namespace rfc::learning
