#pragma once

#include "rfc/learning/relational_available_evidence.hpp"

namespace rfc::learning {
namespace available_evidence_codec_detail {
using namespace available_evidence_detail;
inline constexpr std::array<std::byte, 8> evidence_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'E'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> family_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'F'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> choice_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'Q'}, std::byte{1}, std::byte{0}, std::byte{0}};
struct PassiveObservation {
    std::uint64_t key;
    inference::value_admission_codec_detail::PassiveAvailableResult input;
    Bytes asset;
    std::uint64_t use;
    numeric::SparseRationalTensor target;
    std::vector<core::CellId> observed;
    RelationalTargetOrigins keys;
    std::vector<Origin> targets;
};
struct PassiveEvidence {
    inference::value_admission_codec_detail::PassiveAvailableProgram program;
    RelationalFieldParameterDeclaration declaration;
    numeric::ExactActionLimits limits;
    Origin root;
    std::vector<std::vector<PassiveObservation>> updates;
    Bytes native;
    std::vector<Origin> obligations;
};
inline PassiveEvidence preflight_evidence(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check = {}) {
    Reader r(bytes, policy); header(r, bytes, evidence_magic);
    auto program = inference::value_admission_codec_detail::preflight_program(r.blob(), context, policy, check);
    auto declaration = relational_affine_codec_detail::parameters(r);
    const numeric::ExactActionLimits limits{inference::field_interface_codec_detail::size(r), inference::field_interface_codec_detail::size(r),
        inference::field_interface_codec_detail::size(r), inference::field_interface_codec_detail::size(r)};
    inference::value_admission_detail::require(limits.dimension <= cap.dimension && limits.entries <= cap.entries &&
        limits.steps <= cap.steps && limits.integer_bits <= cap.integer_bits, "available evidence policy exceeds caller cap");
    const auto root_key = r.u64(); const auto receipt = r.blob();
    const auto root_payload = relational_evidence_root_payload(program.native, declaration, limits);
    const auto root = context.require(root_key, Role::Evidence, root_payload); canonical(receipt, root.receipt());
    auto obligations = program.obligations; obligations.push_back(root);
    PassiveEvidence out{std::move(program), std::move(declaration), limits, root, {}, {}, std::move(obligations)};
    const auto updates = r.count(8);
    for (std::size_t i = 0; i < updates; ++i) {
        const auto count = r.count(66); std::vector<PassiveObservation> suffix;
        for (std::size_t j = 0; j < count; ++j) {
            if (check) check();
            const auto key = r.u64(); auto input = inference::value_admission_codec_detail::preflight_result(r.blob(), context, policy, check);
            canonical(input.program, out.program.native);
            const auto asset = r.blob(); const auto use = r.u64(); auto target = storage::temporal_codec_detail::tensor(r);
            const auto mask = r.count(6); std::vector<core::CellId> observed;
            for (std::size_t k = 0; k < mask; ++k) observed.push_back(inference::field_interface_codec_detail::local_cell(r));
            const RelationalTargetOrigins keys{r.u64(), r.u64()}; const auto asset_receipt = r.blob(), value_receipt = r.blob();
            const Bytes asset_bytes(asset.begin(), asset.end());
            const auto payload = relational_target_value_payload(out.program.native, out.declaration.node, asset_bytes, use, target);
            const auto target_asset = context.require(keys.asset, Role::TargetAsset, asset_bytes);
            const auto target_value = context.require(keys.value, Role::TargetValue, payload);
            canonical(asset_receipt, target_asset.receipt()); canonical(value_receipt, target_value.receipt());
            out.obligations.insert(out.obligations.end(), input.obligations.begin(), input.obligations.end());
            out.obligations.push_back(target_asset); out.obligations.push_back(target_value);
            suffix.push_back({key, std::move(input), asset_bytes, use, std::move(target), std::move(observed), keys, {target_asset, target_value}});
        }
        out.updates.push_back(std::move(suffix));
    }
    const auto witness = r.blob(); out.native.assign(witness.begin(), witness.end());
    inference::value_admission_detail::require(!r.remaining(), "trailing available evidence bytes"); return out;
}
inline PreparedAvailableRelationalEvidence replay_evidence(const PassiveEvidence& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, const std::function<void()>& check = {}) {
    recheck(parsed.obligations, context, check);
    auto program = inference::value_admission_codec_detail::replay_program(parsed.program, context, policy, parsed.limits, check);
    PreparedAvailableRelationalEvidence evidence(program, parsed.declaration, parsed.root.key(), context, parsed.limits, check);
    for (const auto& update : parsed.updates) {
        std::vector<RelationalAvailableTrainingObservation> suffix;
        for (const auto& o : update) {
            auto input = inference::value_admission_codec_detail::replay_result(o.input, context, policy, parsed.limits, check, program.program());
            if (check) check();
            auto asset = inference::decode_relational_input_use_asset(o.asset, policy, parsed.limits);
            suffix.push_back({o.key, std::move(input), std::move(asset), o.use, o.target, o.observed, o.keys});
        }
        evidence = evidence.append(std::move(suffix), context, check);
    }
    canonical(parsed.native, encode_relational_field_evidence(evidence.evidence())); return evidence;
}
struct PassiveFamily {
    PassiveEvidence evidence;
    inference::value_admission_codec_detail::PassiveAvailableResult input;
    Bytes native;
};
inline PassiveFamily preflight_family(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check = {}) {
    Reader r(bytes, policy); header(r, bytes, family_magic);
    auto evidence = preflight_evidence(r.blob(), context, policy, cap, check);
    auto input = inference::value_admission_codec_detail::preflight_result(r.blob(), context, policy, check);
    canonical(input.program, evidence.program.native); const auto witness = r.blob();
    inference::value_admission_detail::require(!r.remaining(), "trailing available family bytes");
    return {std::move(evidence), std::move(input), {witness.begin(), witness.end()}};
}
inline AvailableRelationalAffineResult replay_family(const PassiveFamily& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check = {}) {
    recheck(parsed.evidence.obligations, context, check); recheck(parsed.input.obligations, context, check);
    const auto evidence = replay_evidence(parsed.evidence, context, policy, check);
    const auto program = evidence.program();
    auto input = inference::value_admission_codec_detail::replay_result(parsed.input, context, policy, cap, check, program.program());
    auto result = evidence.execute(std::move(input), context, cap, check);
    canonical(parsed.native, encode_relational_affine_result(result.result())); return result;
}
struct PassiveEvaluation {
    PassiveFamily family;
    numeric::RationalVector free;
    Origin setting;
    Bytes native;
};
inline PassiveEvaluation preflight_evaluation(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check = {}) {
    Reader r(bytes, policy); header(r, bytes, choice_magic);
    auto family = preflight_family(r.blob(), context, policy, limits, check);
    auto free = relational_affine_codec_detail::vector(r); const auto key = r.u64(); const auto receipt = r.blob();
    const auto payload = relational_parameter_setting_payload(family.evidence.native, family.evidence.obligations, free);
    auto setting = context.require(key, Role::Prospective, payload); canonical(receipt, setting.receipt());
    const auto witness = r.blob(); inference::value_admission_detail::require(!r.remaining(), "trailing available evaluation bytes");
    return {std::move(family), std::move(free), std::move(setting), Bytes(witness.begin(), witness.end())};
}
inline AvailableRelationalAffineEvaluation replay_evaluation(const PassiveEvaluation& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check = {}) {
    const auto replayed = replay_family(parsed.family, context, policy, limits, check);
    auto out = replayed.evaluate(parsed.free, parsed.setting.key(), context, limits, check);
    canonical(parsed.native, encode_relational_affine_evaluation(out.result())); return out;
}
} // namespace available_evidence_codec_detail

inline std::vector<std::byte> encode_available_relational_evidence(const PreparedAvailableRelationalEvidence& evidence) {
    using namespace available_evidence_codec_detail;
    std::vector<PreparedAvailableRelationalEvidence> lineage; auto current = evidence;
    while (true) { lineage.push_back(current); const auto parent = current.parent(); if (!parent) break; current = *parent; }
    std::reverse(lineage.begin(), lineage.end()); const auto root = lineage.front(); auto w = envelope(evidence_magic);
    blob(w, inference::encode_available_relational_program(root.program())); relational_affine_codec_detail::parameters(w, root.root_declaration());
    const auto limits = root.limits(); w.u64(limits.dimension); w.u64(limits.entries); w.u64(limits.steps); w.u64(limits.integer_bits);
    const auto origin = root.root_origin(); w.u64(origin.key()); blob(w, origin.receipt()); w.u64(lineage.size()-1);
    for (std::size_t i = 1; i < lineage.size(); ++i) {
        const auto& suffix = lineage[i].suffix(); w.u64(suffix.size());
        std::size_t offset = lineage[i-1].obligations().size();
        for (const auto& o : suffix) {
            w.u64(o.key); blob(w, inference::encode_available_relational_result(o.input));
            blob(w, inference::encode_relational_input_use_asset(o.target_asset)); w.u64(o.target_use);
            storage::temporal_codec_detail::tensor(w, o.target); w.u64(o.observed.size());
            for (const auto c : o.observed) inference::field_interface_codec_detail::local_cell(w, c);
            w.u64(o.origins.asset); w.u64(o.origins.value); offset = machine::checked_size_add(offset, o.input.obligations().size());
            blob(w, lineage[i].obligations().at(offset).receipt()); blob(w, lineage[i].obligations().at(offset+1).receipt()); offset += 2;
        }
    }
    blob(w, encode_relational_field_evidence(evidence.evidence())); return finish(std::move(w));
}
inline PreparedAvailableRelationalEvidence decode_available_relational_evidence(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    inference::value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = available_evidence_codec_detail::preflight_evidence(frozen, authority, policy, limits, callback);
    auto out = available_evidence_codec_detail::replay_evidence(parsed, authority, policy, callback);
    inference::field_program_codec_detail::canonical(frozen, encode_available_relational_evidence(out)); return out;
}
inline std::vector<std::byte> encode_available_relational_affine_result(const AvailableRelationalAffineResult& result) {
    using namespace available_evidence_codec_detail;
    const auto pin = result; auto w = envelope(family_magic); blob(w, encode_available_relational_evidence(pin.evidence()));
    blob(w, inference::encode_available_relational_result(pin.input())); blob(w, encode_relational_affine_result(pin.result())); return finish(std::move(w));
}
inline AvailableRelationalAffineResult decode_available_relational_affine_result(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    inference::value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = available_evidence_codec_detail::preflight_family(frozen, authority, policy, limits, callback);
    auto out = available_evidence_codec_detail::replay_family(parsed, authority, policy, limits, callback);
    inference::field_program_codec_detail::canonical(frozen, encode_available_relational_affine_result(out)); return out;
}
inline std::vector<std::byte> encode_available_relational_affine_evaluation(const AvailableRelationalAffineEvaluation& evaluation) {
    using namespace available_evidence_codec_detail;
    const auto pin = evaluation; auto w = envelope(choice_magic); blob(w, encode_available_relational_affine_result(pin.family()));
    const auto native = pin.result(); relational_affine_codec_detail::vector(w, native.free_values()); const auto setting = pin.setting();
    w.u64(setting.key()); blob(w, setting.receipt()); blob(w, encode_relational_affine_evaluation(native)); return finish(std::move(w));
}
inline AvailableRelationalAffineEvaluation decode_available_relational_affine_evaluation(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_evidence_codec_detail;
    inference::value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = preflight_evaluation(frozen, authority, policy, limits, callback);
    auto out = replay_evaluation(parsed, authority, policy, limits, callback);
    canonical(frozen, encode_available_relational_affine_evaluation(out)); return out;
}
} // namespace rfc::learning
