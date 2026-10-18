#pragma once

#include "rfc/learning/relational_available_evidence.hpp"

namespace rfc::learning {
namespace available_evidence_codec_detail {
using namespace available_evidence_detail;
inline constexpr std::array<std::byte, 8> evidence_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'E'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr auto occurrence_evidence_magic=[] {auto out=evidence_magic;out[5]=std::byte{2};return out;}();
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
    std::optional<RelationalOccurrenceTargetDeclaration> port{};
    std::vector<std::uint64_t> observed_coordinates{};
};
struct PassiveEvidence {
    inference::value_admission_codec_detail::PassiveAvailableProgram program;
    RelationalFieldParameterDeclaration declaration;
    numeric::ExactActionLimits limits;
    Origin root;
    std::vector<std::vector<PassiveObservation>> updates;
    storage::ByteRange native;
    std::vector<Origin> obligations;
};
inline PassiveEvidence preflight_evidence_image(storage::ByteRange bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check = {}) {
    const bool version2=bytes.size()>5&&bytes.subspan(5,1).first_span()[0]==std::byte{2};
    Reader r(bytes,policy);header(r,bytes,version2?occurrence_evidence_magic:evidence_magic);
    auto program = inference::value_admission_codec_detail::preflight_program_image(r.view_blob(), context, policy, check);
    auto declaration = relational_affine_codec_detail::parameters(r);
    const numeric::ExactActionLimits limits{inference::field_interface_codec_detail::size(r), inference::field_interface_codec_detail::size(r),
        inference::field_interface_codec_detail::size(r), inference::field_interface_codec_detail::size(r)};
    inference::value_admission_detail::require(limits.dimension <= cap.dimension && limits.entries <= cap.entries &&
        limits.steps <= cap.steps && limits.integer_bits <= cap.integer_bits, "available evidence policy exceeds caller cap");
    const auto root_key = r.u64(); const auto receipt = r.view_blob();
    const auto root_payload = relational_evidence_root_payload(program.native, declaration, limits);
    const auto root = context.require(root_key, Role::Evidence, root_payload); canonical(receipt, root.receipt());
    auto obligations = program.obligations; obligations.push_back(root);
    PassiveEvidence out{std::move(program), std::move(declaration), limits, root, {}, {}, std::move(obligations)};
    const auto updates = r.count(8);
    for (std::size_t i = 0; i < updates; ++i) {
        const auto count = r.count(66); std::vector<PassiveObservation> suffix;
        for (std::size_t j = 0; j < count; ++j) {
            if (check) check();
            const auto key = r.u64(); auto input = inference::value_admission_codec_detail::preflight_result_image(r.view_blob(), context, policy, check);
            canonical(input.program, out.program.native);
            const auto asset = r.blob(); const auto use = r.u64(); auto target = storage::temporal_codec_detail::tensor(r);
            const auto mask = r.count(6); std::vector<core::CellId> observed;
            for (std::size_t k = 0; k < mask; ++k) observed.push_back(inference::field_interface_codec_detail::local_cell(r));
            std::optional<RelationalOccurrenceTargetDeclaration> port;std::vector<std::uint64_t> coordinates;
            if(version2&&r.flag()) {
                port=relational_affine_codec_detail::occurrence_port(r);const auto n=r.count(8);
                for(std::size_t k=0;k<n;++k)coordinates.push_back(r.u64());
            }
            RelationalTargetOrigins keys{r.u64(),r.u64()};if(port)keys.port=r.u64();
            const auto asset_receipt=r.view_blob(),value_receipt=r.view_blob();
            storage::ByteRange port_receipt;if(port)port_receipt=r.view_blob();
            const Bytes asset_bytes(asset.begin(), asset.end());
            std::vector<Bytes> input_accessions;
            if(port)for(const auto& item:input.inputs)input_accessions.push_back(inference::relational_input_accession_payload(
                out.program.native,item.asset,item.declaration,item.use,item.field));
            const auto payload=port?relational_occurrence_target_value_payload(out.program.native,*port,asset_bytes,use,target,coordinates,input_accessions):
                relational_target_value_payload(out.program.native,out.declaration.node,asset_bytes,use,target);
            const auto target_asset = context.require(keys.asset, Role::TargetAsset, asset_bytes);
            const auto target_value = context.require(keys.value, Role::TargetValue, payload);
            canonical(asset_receipt, target_asset.receipt()); canonical(value_receipt, target_value.receipt());
            out.obligations.insert(out.obligations.end(), input.obligations.begin(), input.obligations.end());
            out.obligations.push_back(target_asset); out.obligations.push_back(target_value);
            std::vector<Origin> targets{target_asset,target_value};
            if(port) {
                const auto actual=context.require(keys.port,Role::Interface,relational_occurrence_target_port_payload(out.program.native,*port));
                canonical(port_receipt,actual.receipt());out.obligations.push_back(actual);targets.push_back(actual);
            }
            suffix.push_back({key,std::move(input),asset_bytes,use,std::move(target),std::move(observed),keys,std::move(targets),std::move(port),std::move(coordinates)});
        }
        out.updates.push_back(std::move(suffix));
    }
    out.native = r.view_blob();
    inference::value_admission_detail::require(!r.remaining(), "trailing available evidence bytes"); return out;
}
inline PassiveEvidence preflight_evidence(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check = {}) {
    return preflight_evidence_image(storage::ByteRange(bytes), context, policy, cap, check);
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
            std::optional<OwnedRelationalOccurrenceTargetPort> port;
            if(o.port)port=OwnedRelationalOccurrenceTargetPort(program.program(),*o.port,parsed.limits,check);
            suffix.push_back({o.key,std::move(input),std::move(asset),o.use,o.target,o.observed,o.keys,std::move(port),o.observed_coordinates});
        }
        evidence = evidence.append(std::move(suffix), context, check);
    }
    relational_affine_codec_detail::PacketContext packets(std::numeric_limits<std::size_t>::max(),check);
    canonical(parsed.native,relational_affine_codec_detail::prepare_evidence(evidence.evidence(),packets),packets.check);return evidence;
}
struct PassiveFamily {
    PassiveEvidence evidence;
    inference::value_admission_codec_detail::PassiveAvailableResult input;
    std::span<const std::byte> native;
};
inline PassiveFamily preflight_family(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check = {}) {
    Reader r(bytes, policy); header(r, bytes, family_magic);
    auto evidence = preflight_evidence(r.blob(), context, policy, cap, check);
    auto input = inference::value_admission_codec_detail::preflight_result(r.blob(), context, policy, check);
    canonical(input.program, evidence.program.native); const auto witness = r.blob();
    inference::value_admission_detail::require(!r.remaining(), "trailing available family bytes");
    return {std::move(evidence), std::move(input), witness};
}
inline AvailableRelationalAffineResult replay_family(const PassiveFamily& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check = {}) {
    recheck(parsed.evidence.obligations, context, check); recheck(parsed.input.obligations, context, check);
    const auto evidence = replay_evidence(parsed.evidence, context, policy, check);
    const auto program = evidence.program();
    auto input = inference::value_admission_codec_detail::replay_result(parsed.input, context, policy, cap, check, program.program());
    auto result = evidence.execute(std::move(input), context, cap, check);
    relational_affine_codec_detail::PacketContext packets(std::numeric_limits<std::size_t>::max(),check);
    canonical(parsed.native,relational_affine_codec_detail::prepare_affine(result.result(),packets),packets.check);return result;
}
struct PassiveEvaluation {
    PassiveFamily family;
    numeric::RationalVector free;
    Origin setting;
    std::span<const std::byte> native;
};
inline PassiveEvaluation preflight_evaluation(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check = {}) {
    Reader r(bytes, policy); header(r, bytes, choice_magic);
    auto family = preflight_family(r.blob(), context, policy, limits, check);
    auto free = relational_affine_codec_detail::vector(r); const auto key = r.u64(); const auto receipt = r.blob();
    const auto payload = relational_parameter_setting_payload(family.evidence.native.span(), family.evidence.obligations, free);
    auto setting = context.require(key, Role::Prospective, payload); canonical(receipt, setting.receipt());
    const auto witness = r.blob(); inference::value_admission_detail::require(!r.remaining(), "trailing available evaluation bytes");
    return {std::move(family), std::move(free), std::move(setting), witness};
}
inline AvailableRelationalAffineEvaluation replay_evaluation(const PassiveEvaluation& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits, const std::function<void()>& check = {}) {
    const auto replayed = replay_family(parsed.family, context, policy, limits, check);
    auto out = replayed.evaluate(parsed.free, parsed.setting.key(), context, limits, check);
    canonical(parsed.native, encode_relational_affine_evaluation(out.result())); return out;
}
} // namespace available_evidence_codec_detail

namespace available_evidence_codec_detail {
inline storage::PreparedBytePacket prepare_evidence(const PreparedAvailableRelationalEvidence& evidence,relational_affine_codec_detail::PacketContext& context){
    using namespace available_evidence_codec_detail;
    std::vector<PreparedAvailableRelationalEvidence> lineage; auto current = evidence;
    while (true) { lineage.push_back(current); const auto parent = current.parent(); if (!parent) break; current = *parent; }
    std::reverse(lineage.begin(),lineage.end());const auto root=lineage.front();const bool version2=evidence.evidence().has_occurrence_observations();
    storage::BytePacketBuilder w(version2?occurrence_evidence_magic:evidence_magic,context.max_bytes,context.byte_guard.get());
    w.blob(inference::value_admission_codec_detail::prepare_program(root.program(),context));w.write([&](auto& raw){relational_affine_codec_detail::parameters(raw,root.root_declaration());});
    const auto limits = root.limits(); w.u64(limits.dimension); w.u64(limits.entries); w.u64(limits.steps); w.u64(limits.integer_bits);
    const auto origin=root.root_origin();w.u64(origin.key());w.receipt(origin);w.u64(lineage.size()-1);
    for (std::size_t i = 1; i < lineage.size(); ++i) {
        const auto& suffix = lineage[i].suffix(); w.u64(suffix.size());
        std::size_t offset = lineage[i-1].obligations().size();
        for (const auto& o : suffix) {
            if(context.check){context.check();}w.u64(o.key);w.blob(inference::value_admission_codec_detail::prepare_result(o.input,context));w.blob(context.asset(o.target_asset));
            w.write([&](auto& raw){raw.u64(o.target_use);storage::temporal_codec_detail::tensor(raw,o.target);raw.u64(o.observed.size());for(const auto c:o.observed){inference::field_interface_codec_detail::local_cell(raw,c);}
                if(version2){raw.u8(o.port?1:0);if(o.port){relational_affine_codec_detail::occurrence_port(raw,o.port->declaration());raw.u64(o.observed_coordinates.size());for(const auto key:o.observed_coordinates)raw.u64(key);}}});
            w.u64(o.origins.asset);w.u64(o.origins.value);if(o.port)w.u64(o.origins.port);offset = machine::checked_size_add(offset, o.input.obligations().size());
            w.receipt(lineage[i].obligations().at(offset));w.receipt(lineage[i].obligations().at(offset+1));if(o.port)w.receipt(lineage[i].obligations().at(offset+2));offset+=o.port?3u:2u;
        }
    }
    w.blob(relational_affine_codec_detail::prepare_evidence(evidence.evidence(),context));return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_family(const AvailableRelationalAffineResult& result,relational_affine_codec_detail::PacketContext& context){
    const auto pin=result;storage::BytePacketBuilder w(family_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_evidence(pin.evidence(),context));w.blob(inference::value_admission_codec_detail::prepare_result(pin.input(),context));
    w.blob(relational_affine_codec_detail::prepare_affine(pin.result(),context));return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_evaluation(const AvailableRelationalAffineEvaluation& evaluation,relational_affine_codec_detail::PacketContext& context){
    const auto pin=evaluation;storage::BytePacketBuilder w(choice_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_family(pin.family(),context));const auto native=pin.result();w.write([&](auto& raw){relational_affine_codec_detail::vector(raw,native.free_values());});
    const auto setting=pin.setting();w.u64(setting.key());w.receipt(setting);w.blob(relational_affine_codec_detail::prepare_evaluation(native,context));return std::move(w).finish(context.check);
}
} // namespace available_evidence_codec_detail
inline std::vector<std::byte> encode_available_relational_evidence(const PreparedAvailableRelationalEvidence& evidence){
    relational_affine_codec_detail::PacketContext context;return available_evidence_codec_detail::prepare_evidence(evidence,context).bytes();
}
inline PreparedAvailableRelationalEvidence decode_available_relational_evidence(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    const auto policy=caller_policy;
    inference::value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = available_evidence_codec_detail::preflight_evidence(frozen, authority, policy, limits, callback);
    auto out = available_evidence_codec_detail::replay_evidence(parsed, authority, policy, callback);
    relational_affine_codec_detail::PacketContext packets(policy.max_file_bytes,callback);
    inference::field_program_codec_detail::canonical(frozen,available_evidence_codec_detail::prepare_evidence(out,packets),packets.check);return out;
}
inline std::vector<std::byte> encode_available_relational_affine_result(const AvailableRelationalAffineResult& result) {
    relational_affine_codec_detail::PacketContext context;return available_evidence_codec_detail::prepare_family(result,context).bytes();
}
inline AvailableRelationalAffineResult decode_available_relational_affine_result(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    const auto policy=caller_policy;
    inference::value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = available_evidence_codec_detail::preflight_family(frozen, authority, policy, limits, callback);
    auto out = available_evidence_codec_detail::replay_family(parsed, authority, policy, limits, callback);
    relational_affine_codec_detail::PacketContext packets(policy.max_file_bytes,callback);
    inference::field_program_codec_detail::canonical(frozen,available_evidence_codec_detail::prepare_family(out,packets),packets.check);return out;
}
inline std::vector<std::byte> encode_available_relational_affine_evaluation(const AvailableRelationalAffineEvaluation& evaluation) {
    relational_affine_codec_detail::PacketContext context;return available_evidence_codec_detail::prepare_evaluation(evaluation,context).bytes();
}
inline AvailableRelationalAffineEvaluation decode_available_relational_affine_evaluation(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    const auto policy=caller_policy;
    using namespace available_evidence_codec_detail;
    inference::value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    const auto parsed = preflight_evaluation(frozen, authority, policy, limits, callback);
    auto out = replay_evaluation(parsed, authority, policy, limits, callback);
    relational_affine_codec_detail::PacketContext packets(policy.max_file_bytes,callback);canonical(frozen,prepare_evaluation(out,packets),packets.check);return out;
}
} // namespace rfc::learning
