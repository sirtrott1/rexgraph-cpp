#pragma once

#include "rfc/learning/relational_available_tensor_square_evidence.hpp"
#include "rfc/learning/relational_available_evidence_codec.hpp"
#include "rfc/inference/relational_available_tensor_square_codec.hpp"
#include "rfc/inference/relational_initial_input_lineage_codec.hpp"
#include "rfc/storage/shared_packet_codec.hpp"

namespace rfc::learning {
namespace tensor_square_evidence_codec_detail {
using namespace available_evidence_detail;
inline constexpr std::array<std::byte, 8> evidence_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'E'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> family_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'F'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline storage::PreparedBytePacket prepare_evidence(const PreparedAvailableRelationalTensorSquareEvidence& evidence,relational_affine_codec_detail::PacketContext& context);
struct PassiveInput {
    storage::ByteRange packet, expected;
    inference::available_tensor_square_codec_detail::PassiveInput input;
};
inline PassiveInput preflight_input_image(storage::ByteRange bytes, const Context& context,
    const storage::TemporalReadLimits& policy, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, inference::available_tensor_square_codec_detail::result_magic);
    const auto input = r.view_blob(); const auto expected = r.view_blob();
    inference::value_admission_detail::require(!r.remaining(), "trailing compound observation result bytes");
    auto parsed = inference::available_tensor_square_codec_detail::preflight_image(input, context, policy, check);
    return {bytes, expected, std::move(parsed)};
}
inline PassiveInput preflight_input(std::span<const std::byte> bytes,const Context& context,
    const storage::TemporalReadLimits& policy,const std::function<void()>& check) {
    return preflight_input_image(storage::ByteRange(bytes),context,policy,check);
}
struct PassiveObservation {
    PassiveInput input;
    Origin interface, evidence;
    std::optional<inference::initial_input_lineage_codec_detail::PassiveLineage> lineage;
    std::span<const std::byte> lineage_bytes;
};
struct PassiveEvidence {
    storage::ByteRange base;
    available_evidence_codec_detail::PassiveEvidence parsed;
    std::vector<std::vector<PassiveObservation>> updates;
};
inline PassiveEvidence preflight_evidence_image(storage::ByteRange bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check) {
    Reader r(bytes, policy); header(r, bytes, evidence_magic); const auto base = r.view_blob();
    auto parsed = available_evidence_codec_detail::preflight_evidence_image(base, context, policy, cap, check);
    PassiveEvidence out{base, std::move(parsed), {}};
    const auto count = r.count(8);
    inference::value_admission_detail::require(count == out.parsed.updates.size(), "complete compound update lineage required");
    for (std::size_t generation = 0; generation < count; ++generation) {
        const auto n = r.count(24); const auto& original = out.parsed.updates[generation];
        inference::value_admission_detail::require(n == original.size(), "complete ordered compound observations required");
        std::vector<PassiveObservation> suffix;
        for (std::size_t j = 0; j < n; ++j) {
            auto input = preflight_input_image(r.view_blob(), context, policy, check);
            const auto has_lineage = r.u8(); inference::value_admission_detail::require(has_lineage <= 1, "initial lineage presence bit");
            std::optional<inference::initial_input_lineage_codec_detail::PassiveLineage> lineage; std::span<const std::byte> lineage_bytes;
            auto input_dependencies = input.input.obligations;
            if (has_lineage) {
                const auto image = r.blob(); lineage_bytes = image;
                lineage = inference::initial_input_lineage_codec_detail::preflight(image, context, policy, check);
                input_dependencies.insert(input_dependencies.end(), lineage->obligations.begin(), lineage->obligations.end());
            }
            const auto interface_key = r.u64(), evidence_key = r.u64();
            const auto interface_receipt = r.view_blob(), evidence_receipt = r.view_blob();
            const auto& observation = original[j]; canonical(input.expected, observation.input.native);
            const auto declaration = relational_tensor_square_observation_payload(observation.key, out.parsed.root,
                input_dependencies, observation.targets.at(0), observation.targets.at(1), observation.use, observation.observed);
            const auto interface = context.require(interface_key, Role::Interface, declaration);
            const auto evidence = context.require(evidence_key, Role::Evidence, declaration);
            canonical(interface_receipt, interface.receipt()); canonical(evidence_receipt, evidence.receipt());
            suffix.push_back({std::move(input), interface, evidence, std::move(lineage), std::move(lineage_bytes)});
        }
        out.updates.push_back(std::move(suffix));
    }
    inference::value_admission_detail::require(!r.remaining(), "trailing compound evidence bytes"); return out;
}
inline PassiveEvidence preflight_evidence(std::span<const std::byte> bytes, const Context& context,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap, const std::function<void()>& check) {
    return preflight_evidence_image(storage::ByteRange(bytes), context, policy, cap, check);
}
inline PreparedAvailableRelationalTensorSquareEvidence replay_evidence(const PassiveEvidence& parsed, const Context& context,
    const storage::TemporalReadLimits& policy, const std::function<void()>& check) {
    recheck(parsed.parsed.obligations, context, check);
    for (const auto& generation : parsed.updates) for (const auto& o : generation) {
        inference::available_tensor_square_detail::recheck(o.input.input.obligations, context, check);
        context.recheck(o.interface); context.recheck(o.evidence);
        if (o.lineage) inference::available_tensor_square_detail::recheck(o.lineage->obligations, context, check);
    }
    const auto program = inference::value_admission_codec_detail::replay_program(
        parsed.parsed.program, context, policy, parsed.parsed.limits, check);
    PreparedAvailableRelationalTensorSquareEvidence evidence(program, parsed.parsed.declaration,
        parsed.parsed.root.key(), context, parsed.parsed.limits, check);
    for (std::size_t i = 0; i < parsed.updates.size(); ++i) {
        std::vector<RelationalAvailableTensorSquareObservation> suffix;
        for (std::size_t j = 0; j < parsed.updates[i].size(); ++j) {
            const auto& value = parsed.updates[i][j]; const auto& original = parsed.parsed.updates[i][j];
            auto input = inference::available_tensor_square_codec_detail::decode_image_result(value.input.packet, context, policy,
                parsed.parsed.limits, check, program.program());
            std::optional<inference::AvailableRelationalInitialInputLineage> lineage;
            if (value.lineage) {
                const auto compound = input.input(); const auto native = compound.native_input(); const auto square = native.square();
                lineage = inference::decode_available_relational_initial_input_lineage(value.lineage_bytes, context, policy,
                    parsed.parsed.limits, check, square.parent().accession().asset());
            }
            auto asset = inference::decode_relational_input_use_asset(original.asset, policy, parsed.parsed.limits);
            suffix.push_back({original.key, std::move(input), std::move(asset), original.use, original.target,
                original.observed, original.keys, value.interface.key(), value.evidence.key(), std::move(lineage)});
        }
        evidence = evidence.append(std::move(suffix), context, check);
    }
    relational_affine_codec_detail::PacketContext packets(policy.max_file_bytes,check);
    canonical(parsed.base,available_evidence_codec_detail::prepare_evidence(evidence.base(),packets),packets.check);return evidence;
}
} // namespace tensor_square_evidence_codec_detail
namespace tensor_square_evidence_codec_detail {
inline storage::PreparedBytePacket prepare_evidence(const PreparedAvailableRelationalTensorSquareEvidence& evidence,relational_affine_codec_detail::PacketContext& context){
    using namespace tensor_square_evidence_codec_detail;
    const auto pin=evidence;storage::BytePacketBuilder w(tensor_square_evidence_codec_detail::evidence_magic,context.max_bytes,context.byte_guard.get());
    w.blob(available_evidence_codec_detail::prepare_evidence(pin.base(),context));
    std::vector<PreparedAvailableRelationalTensorSquareEvidence> lineage;
    auto current = pin;
    while (true) { lineage.push_back(current); const auto parent = current.parent(); if (!parent) break; current = *parent; }
    w.u64(lineage.size() - 1);
    for (std::size_t i = lineage.size() - 1; i != 0; --i) {
        const auto& generation = lineage[i - 1]; const auto& suffix = generation.suffix(); w.u64(suffix.size());
        for (const auto& o : suffix) {
            if(context.check){context.check();}w.blob(inference::available_tensor_square_codec_detail::prepare_result(o.input,context));
            w.u8(o.lineage?1:0);if(o.lineage){w.blob(inference::encode_available_relational_initial_input_lineage(*o.lineage));}
            w.u64(o.interface_origin); w.u64(o.evidence_origin);
            // The ordered complete closure ends with these two exact origins.
            const auto& obligations = generation.obligations();
            const auto find = [&](std::uint64_t key, Role role) -> Origin {
                for (const auto& origin : obligations) if (origin.key() == key && origin.role() == role) return origin;
                throw std::logic_error("missing compound observation declaration origin");
            };
            const auto interface = find(o.interface_origin, Role::Interface);
            const auto evidence_origin = find(o.evidence_origin, Role::Evidence);
            w.receipt(interface);w.receipt(evidence_origin);
        }
    }
    return std::move(w).finish(context.check);
}
} // namespace tensor_square_evidence_codec_detail
template<class Consume> void stream_available_relational_tensor_square_evidence(const PreparedAvailableRelationalTensorSquareEvidence& evidence,Consume&& consume,std::function<void()> check={}){
    const auto pin=evidence;relational_affine_codec_detail::PacketContext packets(std::numeric_limits<std::size_t>::max(),std::move(check));
    tensor_square_evidence_codec_detail::prepare_evidence(pin,packets).stream(std::forward<Consume>(consume),packets.check);
}
inline std::vector<std::byte> encode_available_relational_tensor_square_evidence(const PreparedAvailableRelationalTensorSquareEvidence& evidence){
    relational_affine_codec_detail::PacketContext context;return tensor_square_evidence_codec_detail::prepare_evidence(evidence,context).bytes();
}
namespace tensor_square_evidence_codec_detail {
// The caller owns immutable bytes for the entire preflight/replay. Shared
// decoders use their already pinned expanded buffer without another full copy.
inline PreparedAvailableRelationalTensorSquareEvidence decode_image_evidence(
    storage::ByteRange frozen,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    const std::function<void(const PassiveEvidence&)>& image_check={}) {
    inference::value_admission_detail::bound(frozen.size(),policy.max_file_bytes);
    const auto authority=context;const auto callback=std::move(check);if(callback)callback();
    const auto parsed=preflight_evidence_image(frozen,authority,policy,limits,callback);
    if(image_check)image_check(parsed);
    auto out=replay_evidence(parsed,authority,policy,callback);
    relational_affine_codec_detail::PacketContext packets(policy.max_file_bytes,callback);
    inference::field_program_codec_detail::canonical(frozen,prepare_evidence(out,packets),packets.check);return out;
}
inline PreparedAvailableRelationalTensorSquareEvidence decode_frozen_evidence(
    std::span<const std::byte> frozen,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    const std::function<void(const PassiveEvidence&)>& image_check={}) {
    return decode_image_evidence(storage::ByteRange(frozen),context,policy,limits,std::move(check),image_check);
}
} // namespace tensor_square_evidence_codec_detail
inline PreparedAvailableRelationalTensorSquareEvidence decode_available_relational_tensor_square_evidence(
    std::span<const std::byte> bytes,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    const auto policy=caller_policy;
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;
    return tensor_square_evidence_codec_detail::decode_frozen_evidence(frozen,authority,policy,limits,std::move(check));
}
namespace tensor_square_evidence_codec_detail {
inline storage::PreparedBytePacket prepare_family(const AvailableRelationalTensorSquareAffineResult& result,relational_affine_codec_detail::PacketContext& context){
    using namespace tensor_square_evidence_codec_detail;
    const auto pin=result;storage::BytePacketBuilder w(tensor_square_evidence_codec_detail::family_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_evidence(pin.evidence(),context));w.blob(inference::available_tensor_square_codec_detail::prepare_result(pin.input(),context));
    const auto lineage = pin.lineage(); w.u8(lineage ? 1 : 0);
    if(lineage){w.blob(inference::encode_available_relational_initial_input_lineage(*lineage));}
    w.blob(relational_affine_codec_detail::prepare_affine(pin.result(),context));return std::move(w).finish(context.check);
}
} // namespace tensor_square_evidence_codec_detail
template<class Consume> void stream_available_relational_tensor_square_affine_result(const AvailableRelationalTensorSquareAffineResult& result,Consume&& consume,std::function<void()> check={}){
    const auto pin=result;relational_affine_codec_detail::PacketContext context(std::numeric_limits<std::size_t>::max(),std::move(check));
    tensor_square_evidence_codec_detail::prepare_family(pin,context).stream(std::forward<Consume>(consume),context.check);
}
inline std::vector<std::byte> encode_available_relational_tensor_square_affine_result(const AvailableRelationalTensorSquareAffineResult& result){
    relational_affine_codec_detail::PacketContext context;return tensor_square_evidence_codec_detail::prepare_family(result,context).bytes();
}
namespace tensor_square_evidence_codec_detail {
struct PassiveFamily {
    PassiveEvidence evidence;
    PassiveInput input;
    std::span<const std::byte> lineage_bytes;
    storage::ByteRange expected;
    std::optional<inference::initial_input_lineage_codec_detail::PassiveLineage> lineage;
};
inline PassiveFamily preflight_family_image(storage::ByteRange bytes,const Context& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits,const std::function<void()>& check={}) {
    Reader r(bytes,policy);header(r,bytes,family_magic);const auto evidence_bytes=r.view_blob();const auto input_bytes=r.view_blob();
    const auto has_lineage=r.u8();inference::value_admission_detail::require(has_lineage<=1,"held initial lineage presence bit");
    std::span<const std::byte> lineage_bytes;if(has_lineage)lineage_bytes=r.blob();const auto expected=r.view_blob();
    inference::value_admission_detail::require(!r.remaining(),"trailing compound affine family bytes");
    auto parsed=preflight_evidence_image(evidence_bytes,context,policy,limits,check);auto input=preflight_input_image(input_bytes,context,policy,check);
    std::optional<inference::initial_input_lineage_codec_detail::PassiveLineage> lineage;
    if(has_lineage)lineage=inference::initial_input_lineage_codec_detail::preflight(lineage_bytes,context,policy,check);
    return {std::move(parsed),std::move(input),lineage_bytes,expected,std::move(lineage)};
}
inline PassiveFamily preflight_family(std::span<const std::byte> bytes,const Context& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits,const std::function<void()>& check={}) {
    return preflight_family_image(storage::ByteRange(bytes),context,policy,limits,check);
}
inline AvailableRelationalTensorSquareAffineResult decode_image_family(
    storage::ByteRange frozen,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    const std::function<void(const PassiveFamily&)>& image_check={}) {
    inference::value_admission_detail::bound(frozen.size(),policy.max_file_bytes);
    const auto authority=context;const auto callback=std::move(check);if(callback)callback();
    const auto all=preflight_family_image(frozen,authority,policy,limits,callback);
    if(image_check)image_check(all);
    const auto& parsed=all.evidence;const auto& input=all.input;const auto lineage_bytes=all.lineage_bytes;const auto expected=all.expected;
    const bool has_lineage=all.lineage.has_value();
    inference::available_tensor_square_detail::recheck(input.input.obligations, authority, callback);
    const auto evidence = replay_evidence(parsed, authority, policy, callback);
    const auto program = evidence.program();
    auto supplied = inference::available_tensor_square_codec_detail::decode_image_result(input.packet, authority, policy,
        limits, callback, program.program());
    std::optional<inference::AvailableRelationalInitialInputLineage> lineage;
    if (has_lineage) {
        const auto compound = supplied.input(); const auto native = compound.native_input(); const auto square = native.square();
        lineage = inference::decode_available_relational_initial_input_lineage(lineage_bytes, authority, policy,
            limits, callback, square.parent().accession().asset());
    }
    auto out = evidence.execute(std::move(supplied), authority, limits, callback, std::move(lineage));
    relational_affine_codec_detail::PacketContext packets(policy.max_file_bytes,callback);
    canonical(expected,relational_affine_codec_detail::prepare_affine(out.result(),packets),packets.check);
    canonical(frozen,prepare_family(out,packets),packets.check);return out;
}
inline AvailableRelationalTensorSquareAffineResult decode_frozen_family(
    std::span<const std::byte> frozen,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    const std::function<void(const PassiveFamily&)>& image_check={}) {
    return decode_image_family(storage::ByteRange(frozen),context,policy,limits,std::move(check),image_check);
}
} // namespace tensor_square_evidence_codec_detail
inline AvailableRelationalTensorSquareAffineResult decode_available_relational_tensor_square_affine_result(
    std::span<const std::byte> bytes,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    const auto policy=caller_policy;
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;
    return tensor_square_evidence_codec_detail::decode_frozen_family(frozen,authority,policy,limits,std::move(check));
}
} // namespace rfc::learning

namespace rfc::learning {
// Passive, lossless shared-byte storage. Expanded mathematical/provenance
// packets are unchanged; equal storage chunks never identify source owners.
inline std::vector<std::byte> encode_shared_available_relational_tensor_square_evidence(
    const PreparedAvailableRelationalTensorSquareEvidence& evidence, storage::SharedPacketLimits limits = {},
    std::function<void()> check = {}) {
    const auto pin=evidence;relational_affine_codec_detail::PacketContext context(limits,std::move(check));
    const auto packet=tensor_square_evidence_codec_detail::prepare_evidence(pin,context);
    limits.max_work_bytes-=context.byte_guard->work_bytes();
    return storage::encode_shared_packet_stream([&](auto consume){packet.stream(consume);},limits,context.check);
}
inline PreparedAvailableRelationalTensorSquareEvidence decode_shared_available_relational_tensor_square_evidence(
    std::span<const std::byte> bytes, const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& caller_policy, numeric::ExactActionLimits limits = {},
    storage::SharedPacketLimits storage_limits = {}, std::function<void()> check = {}) {
    const auto policy=caller_policy;
    const auto authority = context; const auto callback = std::move(check);
    storage_limits.max_expanded_bytes = std::min(storage_limits.max_expanded_bytes, policy.max_file_bytes);
    const auto image = storage::view_shared_packet(bytes, storage_limits, callback);
    return tensor_square_evidence_codec_detail::decode_image_evidence(image, authority, policy, limits, callback);
}
inline std::vector<std::byte> encode_shared_available_relational_tensor_square_affine_result(
    const AvailableRelationalTensorSquareAffineResult& result, storage::SharedPacketLimits limits = {},
    std::function<void()> check = {}) {
    const auto pin=result;relational_affine_codec_detail::PacketContext context(limits,std::move(check));
    const auto packet=tensor_square_evidence_codec_detail::prepare_family(pin,context);
    limits.max_work_bytes-=context.byte_guard->work_bytes();
    return storage::encode_shared_packet_stream([&](auto consume){packet.stream(consume);},limits,context.check);
}
inline AvailableRelationalTensorSquareAffineResult decode_shared_available_relational_tensor_square_affine_result(
    std::span<const std::byte> bytes, const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& caller_policy, numeric::ExactActionLimits limits = {},
    storage::SharedPacketLimits storage_limits = {}, std::function<void()> check = {}) {
    const auto policy=caller_policy;
    const auto authority = context; const auto callback = std::move(check);
    storage_limits.max_expanded_bytes = std::min(storage_limits.max_expanded_bytes, policy.max_file_bytes);
    const auto image = storage::view_shared_packet(bytes, storage_limits, callback);
    return tensor_square_evidence_codec_detail::decode_image_family(image, authority, policy, limits, callback);
}
} // namespace rfc::learning
