#pragma once
#include "rfc/storage/phase_fold_codec.hpp"
#include "rfc/storage/algebraic_archive_history.hpp"

namespace rfc::storage {
struct PhaseFoldChannel {
    std::uint64_t namespace_key,type_key,value_key;
    composite::DeclarationId type() const {return {namespace_key,type_key,2};}
    composite::DeclarationId chunk_type() const {return {namespace_key,type_key,1};}
};
namespace phase_fold_history_detail {
// The existing native physical-octet declaration is reused. Its meaning is
// established only by the complete RFCXW decoder, not this shared chunk shape.
inline AlgebraicArchiveChannel octets(PhaseFoldChannel channel) {return {channel.namespace_key,channel.type_key,channel.value_key};}
inline void validate(PhaseFoldChannel channel) {algebraic_archive_history_detail::validate_channel(octets(channel));}
inline composite::Declaration declaration(PhaseFoldChannel channel) {return algebraic_archive_history_detail::sequence_declaration(octets(channel));}
inline void registry(const TemporalEndpoint& endpoint,PhaseFoldChannel channel) {
    using algebraic_archive_detail::require;
    require(endpoint.declarations && endpoint.declarations->namespace_key==channel.namespace_key,"phase/fold declaration namespace absent or different");
    const auto& declarations=endpoint.declarations->registry;
    require(declarations.at(channel.type())==declaration(channel) &&
        declarations.at(channel.chunk_type())==composite::Declaration::integer(channel.chunk_type()),"phase/fold octet declaration differs");
}
inline void binding(const TemporalDeclaredValue& value,PhaseFoldChannel channel) {
    algebraic_archive_detail::require(value.type==channel.type() && value.block==0 && value.branch==0 && !value.cell && !value.occurrence,
        "phase/fold channel typed value binding differs");
}
} // namespace phase_fold_history_detail

// This explicitly named snapshot channel records a whole word, not a carrier
// comparison or an event clock. All other native endpoint channels are retained.
inline TemporalEndpoint with_phase_fold_archive(const TemporalEndpoint& endpoint,PhaseFoldChannel channel,
    const PhaseFoldArchive& archive,const PhaseFoldLimits& limits={}) {
    using algebraic_archive_detail::require;
    phase_fold_history_detail::validate(channel);validate_temporal_endpoint(endpoint);
    require(encode_rfcx(endpoint.source).bytes==encode_rfcx(archive.source()).bytes,"phase/fold channel requires complete endpoint source");
    const auto bytes=encode_phase_fold_archive(archive,limits);(void)decode_phase_fold_archive(bytes,limits);
    auto target=endpoint;
    if (!target.declarations) {
        target.declarations.emplace();target.declarations->namespace_key=channel.namespace_key;
        target.declarations->registry=composite::DeclarationRegistry({composite::Declaration::integer(channel.chunk_type()),phase_fold_history_detail::declaration(channel)});
    }
    phase_fold_history_detail::registry(target,channel);auto& values=target.declarations->values;
    const auto found=std::find_if(values.begin(),values.end(),[&](const auto& value){return value.key==channel.value_key;});
    auto encoded=algebraic_archive_history_detail::chunks(bytes);
    if (found==values.end()) values.push_back({channel.value_key,channel.type(),0,0,{},{},std::move(encoded)});
    else {phase_fold_history_detail::binding(*found,channel);found->value=std::move(encoded);}
    validate_temporal_endpoint(target);return target;
}
inline PhaseFoldArchive read_phase_fold_archive(const TemporalEndpoint& endpoint,PhaseFoldChannel channel,const PhaseFoldLimits& limits={}) {
    using algebraic_archive_detail::require;
    phase_fold_history_detail::validate(channel);validate_temporal_endpoint(endpoint);phase_fold_history_detail::registry(endpoint,channel);
    const auto& values=endpoint.declarations->values;
    const auto found=std::find_if(values.begin(),values.end(),[&](const auto& value){return value.key==channel.value_key;});
    require(found!=values.end(),"phase/fold channel absent");phase_fold_history_detail::binding(*found,channel);
    auto archive=decode_phase_fold_archive(algebraic_archive_history_detail::concatenate(found->value,limits.wire),limits);
    require(encode_rfcx(endpoint.source).bytes==encode_rfcx(archive.source()).bytes,"phase/fold channel source differs from endpoint");return archive;
}
} // namespace rfc::storage
