#pragma once
#include "rfc/storage/algebraic_graph_snapshot_codec.hpp"
#include "rfc/storage/algebraic_archive_history.hpp"
#include "rfc/storage/native_series_prefix.hpp"

namespace rfc::storage {
struct AlgebraicGraphChannel {
    std::uint64_t namespace_key,type_key,value_key;
    composite::DeclarationId type() const {return {namespace_key,type_key,2};}
    composite::DeclarationId chunk_type() const {return {namespace_key,type_key,1};}
};
namespace algebraic_graph_history_detail {
inline AlgebraicArchiveChannel octets(AlgebraicGraphChannel channel) {return {channel.namespace_key,channel.type_key,channel.value_key};}
inline void validate(AlgebraicGraphChannel channel) {algebraic_archive_history_detail::validate_channel(octets(channel));}
inline composite::Declaration declaration(AlgebraicGraphChannel channel) {return algebraic_archive_history_detail::sequence_declaration(octets(channel));}
inline void registry(const TemporalEndpoint& endpoint,AlgebraicGraphChannel channel) {
    using algebraic_archive_detail::require;
    require(endpoint.declarations && endpoint.declarations->namespace_key==channel.namespace_key,"native graph declaration namespace absent or different");
    const auto& registry=endpoint.declarations->registry;
    require(registry.at(channel.type())==declaration(channel) && registry.at(channel.chunk_type())==composite::Declaration::integer(channel.chunk_type()),
        "native graph octet declaration differs");
}
inline void binding(const TemporalDeclaredValue& value,AlgebraicGraphChannel channel) {
    algebraic_archive_detail::require(value.type==channel.type() && value.block==0 && value.branch==0 && !value.cell && !value.occurrence,
        "native graph typed channel binding differs");
}
} // namespace algebraic_graph_history_detail

inline TemporalEndpoint with_algebraic_graph_snapshot(const TemporalEndpoint& endpoint,AlgebraicGraphChannel channel,
    const AlgebraicGraphSnapshot& snapshot,const AlgebraicGraphSnapshotLimits& limits={}) {
    using algebraic_archive_detail::require;
    algebraic_graph_history_detail::validate(channel);validate_temporal_endpoint(endpoint);
    // These bytes check the explicitly selected source declaration. They do
    // not discover an anchor, intern graph owners or declare an inverse map.
    require(encode_rfcx(endpoint.source).bytes==encode_rfcx(snapshot.source()).bytes,"native graph anchor differs from the complete endpoint source");
    const auto bytes=encode_algebraic_graph_snapshot(snapshot,limits);(void)decode_algebraic_graph_snapshot(bytes,limits);
    auto target=endpoint;
    if (!target.declarations) {
        target.declarations.emplace();target.declarations->namespace_key=channel.namespace_key;
        target.declarations->registry=composite::DeclarationRegistry({composite::Declaration::integer(channel.chunk_type()),
            algebraic_graph_history_detail::declaration(channel)});
    }
    algebraic_graph_history_detail::registry(target,channel);auto& values=target.declarations->values;
    const auto encoded=algebraic_archive_history_detail::chunks(bytes);
    auto found=std::find_if(values.begin(),values.end(),[&](const auto& row){return row.key==channel.value_key;});
    if (found==values.end()) values.push_back({channel.value_key,channel.type(),0,0,{},{},encoded});
    else {algebraic_graph_history_detail::binding(*found,channel);found->value=encoded;}
    validate_temporal_endpoint(target);return target;
}
inline AlgebraicGraphSnapshot read_algebraic_graph_snapshot(const TemporalEndpoint& endpoint,AlgebraicGraphChannel channel,
    const AlgebraicGraphSnapshotLimits& limits={}) {
    algebraic_graph_history_detail::validate(channel);validate_temporal_endpoint(endpoint);
    algebraic_graph_history_detail::registry(endpoint,channel);
    const auto& values=endpoint.declarations->values;
    const auto found=std::find_if(values.begin(),values.end(),[&](const auto& row){return row.key==channel.value_key;});
    algebraic_archive_detail::require(found!=values.end(),"native graph channel value absent");
    algebraic_graph_history_detail::binding(*found,channel);
    auto octet_limits=limits.graph.archive;octet_limits.max_file_bytes=limits.max_file_bytes;
    auto snapshot=decode_algebraic_graph_snapshot(algebraic_archive_history_detail::concatenate(found->value,octet_limits),limits);
    algebraic_archive_detail::require(encode_rfcx(endpoint.source).bytes==encode_rfcx(snapshot.source()).bytes,"restored native graph anchor differs from endpoint");
    return snapshot;
}

// One selected native endpoint and one restored graph. Aliasing archive pins
// preserve all graph sharing after the revision or coordinator is released.
class AlgebraicGraphRevision {
    struct State {std::shared_ptr<const TemporalEndpoint> endpoint;AlgebraicGraphSnapshot snapshot;};
public:
    static AlgebraicGraphRevision capture(const NativeSeriesEndpoint& selected,AlgebraicGraphChannel channel,
        const AlgebraicGraphSnapshotLimits& limits={}) {
        auto snapshot=read_algebraic_graph_snapshot(selected.endpoint(),channel,limits);
        return AlgebraicGraphRevision(std::make_shared<const State>(State{selected.pin(),std::move(snapshot)}),selected.source_namespace());
    }
    std::uint64_t source_namespace() const {valid();return namespace_;}
    std::uint64_t sequence() const {valid();return state_->endpoint->sequence;}
    std::shared_ptr<const TemporalEndpoint> source_publication() const {valid();return state_->endpoint;}
    const AlgebraicGraphSnapshot& snapshot() const & {valid();return state_->snapshot;}
    const AlgebraicGraphSnapshot& snapshot() const &&=delete;
    std::shared_ptr<const AlgebraicArchive> archive_pin(std::uint64_t key) const {return {state_,&snapshot().graph().archive(key)};}
    bool same_owner(const AlgebraicGraphRevision& other) const noexcept {return state_ && state_==other.state_;}
private:
    void valid() const {algebraic_archive_detail::require(static_cast<bool>(state_),"moved native graph revision");}
    AlgebraicGraphRevision(std::shared_ptr<const State> state,std::uint64_t ns):state_(std::move(state)),namespace_(ns){}
    std::shared_ptr<const State> state_;std::uint64_t namespace_;
};

// The captured prefix owns no future tail, frames, checkpoints, or graph owner
// tables. Its revision ordinals are not an availability or event-time clock.
class AlgebraicGraphHistoryWindow {
public:
    static AlgebraicGraphHistoryWindow capture(const NativeTemporalSeries& series,std::uint64_t cutoff,AlgebraicGraphChannel channel,
        const AlgebraicGraphSnapshotLimits& limits={}) {
        algebraic_graph_history_detail::validate(channel);
        if (cutoff<series.first_sequence() || cutoff>series.last_sequence()) throw std::out_of_range("native graph cutoff outside history");
        const auto count=machine::checked_size_add(machine::checked_size(cutoff-series.first_sequence()),1);
        algebraic_archive_detail::bound(count,limits.max_history_revisions,"native graph prefix revision budget");
        auto prefix=NativeSeriesPrefix::capture(series,cutoff);std::vector<AlgebraicGraphRevision> revisions;revisions.reserve(count);
        for (std::size_t i=0;i<count;++i) revisions.push_back(AlgebraicGraphRevision::capture(prefix.at(prefix.first_sequence()+i),channel,limits));
        return AlgebraicGraphHistoryWindow(std::move(prefix),std::move(revisions));
    }
    std::uint64_t source_namespace() const {return prefix_.source_namespace();}
    std::uint64_t first_sequence() const {return prefix_.first_sequence();}
    std::uint64_t cutoff() const {return prefix_.cutoff();}
    std::size_t revision_count() const noexcept {return revisions_.size();}
    AlgebraicGraphRevision at(std::uint64_t sequence) const {
        if (sequence<first_sequence() || sequence>cutoff()) throw std::out_of_range("graph revision outside selected prefix");
        return revisions_.at(machine::checked_size(sequence-first_sequence()));
    }
    const NativeSeriesPrefix& native_prefix() const & noexcept {return prefix_;}
    const NativeSeriesPrefix& native_prefix() const &&=delete;
private:
    AlgebraicGraphHistoryWindow(NativeSeriesPrefix prefix,std::vector<AlgebraicGraphRevision> revisions)
        :prefix_(std::move(prefix)),revisions_(std::move(revisions)){}
    NativeSeriesPrefix prefix_;std::vector<AlgebraicGraphRevision> revisions_;
};
} // namespace rfc::storage
