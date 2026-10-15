#pragma once
#include "rfc/storage/phase_fold_history.hpp"
#include "rfc/storage/native_durable_store.hpp"

namespace rfc::storage {
// Exclusive Linux durable publication of complete original words and separate
// proposals. Revision ordinals do not declare continuous motion or operation
// transport between independently restored owners. A selected at() reads only
// that native endpoint; observed_prefix() drops its proposals and suffix.
class PhaseFoldDurableHistory {
    struct Create {};
    struct Open {};
public:
    static PhaseFoldDurableHistory create(const std::filesystem::path& path,const PhaseFoldArchive& base,
        PhaseFoldChannel channel,std::uint64_t source_namespace,std::uint64_t first_sequence=0,
        PhaseFoldLimits limits={},DurableNativeLimits durable_limits={}) {
        const auto endpoint=with_phase_fold_archive(TemporalEndpoint(base.source(),first_sequence),channel,base,limits);
        return PhaseFoldDurableHistory(Create{},path,endpoint,channel,source_namespace,limits,durable_limits);
    }
    static PhaseFoldDurableHistory open(const std::filesystem::path& path,PhaseFoldChannel channel,
        PhaseFoldLimits limits={},DurableNativeLimits durable_limits={}) {
        phase_fold_history_detail::validate(channel);
        return PhaseFoldDurableHistory(Open{},path,channel,limits,durable_limits);
    }
    DurableNativeRoot root() const {return store_.root();}
    std::optional<DurableNativeReceipt> status(std::uint64_t transaction) const {return store_.status(transaction);}
    NativeTemporalSeries snapshot() const {return store_.snapshot();}
    PhaseFoldArchive at(std::uint64_t sequence) const {
        const auto selected=store_.snapshot().at(sequence);return read_phase_fold_archive(selected.endpoint(),channel_,limits_);
    }
    PhaseFoldArchive observed_prefix(std::uint64_t sequence,std::size_t operations) const {return at(sequence).observed_prefix(operations);}
    DurableNativeReceipt append(std::uint64_t transaction,const DurableNativeRoot& expected,
        std::span<const PhaseFoldArchive> revisions,DurableFaultHook hook={}) {
        using algebraic_archive_detail::require;using algebraic_archive_detail::bound;
        require(!revisions.empty(),"empty phase/fold transaction");
        const auto history=store_.snapshot();const auto predecessor=history.at(expected.sequence);auto endpoint=predecessor.endpoint();
        bound(revisions.size(),history.limits().max_segments,"phase/fold transaction frame budget");
        std::vector<std::vector<std::byte>> frames;frames.reserve(revisions.size());std::size_t frame_bytes{};
        for (const auto& revision:revisions) {
            auto next=with_phase_fold_archive(endpoint,channel_,revision,limits_);
            if (endpoint.sequence==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("phase/fold revision sequence exhausted");
            next.sequence=endpoint.sequence+1;
            auto frame=encode_temporal_segment(make_temporal_segment(endpoint,next));frame_bytes=machine::checked_size_add(frame_bytes,frame.size());
            bound(frame_bytes,history.limits().max_archive_bytes,"phase/fold transaction byte budget");
            frames.push_back(std::move(frame));endpoint=std::move(next);
        }
        return store_.append(transaction,expected,frames,std::move(hook));
    }
private:
    PhaseFoldDurableHistory(Create,const std::filesystem::path& path,const TemporalEndpoint& endpoint,PhaseFoldChannel channel,
        std::uint64_t source_namespace,PhaseFoldLimits limits,DurableNativeLimits durable_limits)
        :store_(DurableNativeStore::create(path,endpoint,source_namespace,durable_limits)),channel_(channel),limits_(limits) {}
    PhaseFoldDurableHistory(Open,const std::filesystem::path& path,PhaseFoldChannel channel,PhaseFoldLimits limits,DurableNativeLimits durable_limits)
        :store_(DurableNativeStore::open(path,durable_limits)),channel_(channel),limits_(limits) {
        phase_fold_history_detail::validate(channel_);
        store_.snapshot().visit_endpoints([&](const auto& selected){(void)read_phase_fold_archive(selected.endpoint(),channel_,limits_);});
    }
    DurableNativeStore store_;
    PhaseFoldChannel channel_;
    PhaseFoldLimits limits_;
};
} // namespace rfc::storage
