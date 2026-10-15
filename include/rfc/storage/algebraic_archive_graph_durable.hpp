#pragma once
#include "rfc/storage/algebraic_archive_graph_history.hpp"
#include "rfc/storage/native_durable_store.hpp"

namespace rfc::storage {
// Typed graph publication through the existing exclusive Linux coordinator.
// Snapshot replacement supplies no cross-revision field or carrier transport.
class AlgebraicGraphDurableHistory {
    struct Create {};struct Open {};
public:
    static AlgebraicGraphDurableHistory create(const std::filesystem::path& path,const AlgebraicGraphSnapshot& base,
        AlgebraicGraphChannel channel,std::uint64_t source_namespace,std::uint64_t first_sequence=0,
        AlgebraicGraphSnapshotLimits limits={},DurableNativeLimits durable_limits={}) {
        const auto endpoint=with_algebraic_graph_snapshot(TemporalEndpoint(base.source(),first_sequence),channel,base,limits);
        return AlgebraicGraphDurableHistory(Create{},path,endpoint,channel,source_namespace,limits,durable_limits);
    }
    static AlgebraicGraphDurableHistory open(const std::filesystem::path& path,AlgebraicGraphChannel channel,
        AlgebraicGraphSnapshotLimits limits={},DurableNativeLimits durable_limits={}) {
        algebraic_graph_history_detail::validate(channel);
        return AlgebraicGraphDurableHistory(Open{},path,channel,limits,durable_limits);
    }
    DurableNativeRoot root() const {return store_.root();}
    std::optional<DurableNativeReceipt> status(std::uint64_t transaction) const {return store_.status(transaction);}
    NativeTemporalSeries snapshot() const {return store_.snapshot();}
    AlgebraicGraphRevision at(std::uint64_t sequence) const {return AlgebraicGraphRevision::capture(store_.snapshot().at(sequence),channel_,limits_);}
    AlgebraicGraphHistoryWindow prefix(std::uint64_t cutoff) const {return AlgebraicGraphHistoryWindow::capture(store_.snapshot(),cutoff,channel_,limits_);}
    DurableNativeReceipt append(std::uint64_t transaction,const DurableNativeRoot& expected,
        std::span<const AlgebraicGraphSnapshot> revisions,DurableFaultHook hook={}) {
        using algebraic_archive_detail::require;using algebraic_archive_detail::bound;
        require(!revisions.empty(),"empty native graph transaction");
        const auto history=store_.snapshot();const auto predecessor=history.at(expected.sequence);auto endpoint=predecessor.endpoint();
        bound(revisions.size(),history.limits().max_segments,"native graph transaction frame budget");
        std::vector<std::vector<std::byte>> frames;frames.reserve(revisions.size());std::size_t bytes{};
        for (const auto& revision:revisions) {
            auto next=with_algebraic_graph_snapshot(endpoint,channel_,revision,limits_);
            if (endpoint.sequence==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("native graph publication sequence exhausted");
            next.sequence=endpoint.sequence+1;
            auto frame=encode_temporal_segment(make_temporal_segment(endpoint,next));bytes=machine::checked_size_add(bytes,frame.size());
            bound(bytes,history.limits().max_archive_bytes,"native graph transaction byte budget");
            frames.push_back(std::move(frame));endpoint=std::move(next);
        }
        return store_.append(transaction,expected,frames,std::move(hook));
    }
private:
    AlgebraicGraphDurableHistory(Create,const std::filesystem::path& path,const TemporalEndpoint& endpoint,AlgebraicGraphChannel channel,
        std::uint64_t ns,AlgebraicGraphSnapshotLimits limits,DurableNativeLimits durable_limits)
        :store_(DurableNativeStore::create(path,endpoint,ns,durable_limits)),channel_(channel),limits_(limits){}
    AlgebraicGraphDurableHistory(Open,const std::filesystem::path& path,AlgebraicGraphChannel channel,
        AlgebraicGraphSnapshotLimits limits,DurableNativeLimits durable_limits)
        :store_(DurableNativeStore::open(path,durable_limits)),channel_(channel),limits_(limits) {
        store_.snapshot().visit_endpoints([&](const auto& selected){(void)AlgebraicGraphRevision::capture(selected,channel_,limits_);});
    }
    DurableNativeStore store_;AlgebraicGraphChannel channel_;AlgebraicGraphSnapshotLimits limits_;
};
} // namespace rfc::storage
