#pragma once
#include "rfc/storage/algebraic_archive_history.hpp"
#include "rfc/storage/native_durable_store.hpp"

namespace rfc::storage {
// Linux adapter over the existing store. Full canonical archives are validated
// before publication; a raw opaque-channel frame is not a typed archive proof.
class AlgebraicDurableHistory {
    struct Create {};
    struct Open {};
public:
    static AlgebraicDurableHistory create(const std::filesystem::path& path,const AlgebraicArchive& base,
        AlgebraicArchiveChannel channel,std::uint64_t source_namespace,std::uint64_t first_sequence=0,
        AlgebraicArchiveLimits limits={},DurableNativeLimits durable_limits={}) {
        // Prove admission under this reader policy before creating a filesystem root.
        const auto admitted=decode_algebraic_archive(encode_algebraic_archive(base,limits),limits);
        const auto endpoint=with_algebraic_archive(TemporalEndpoint(admitted.bundle().source(),first_sequence),channel,admitted,limits);
        return AlgebraicDurableHistory(Create{},path,endpoint,channel,source_namespace,limits,durable_limits);
    }
    static AlgebraicDurableHistory open(const std::filesystem::path& path,AlgebraicArchiveChannel channel,
        AlgebraicArchiveLimits limits={},DurableNativeLimits durable_limits={}) {
        return AlgebraicDurableHistory(Open{},path,channel,limits,durable_limits);
    }
    DurableNativeRoot root() const { return store_.root(); }
    std::optional<DurableNativeReceipt> status(std::uint64_t transaction) const { return store_.status(transaction); }
    NativeTemporalSeries snapshot() const { return store_.snapshot(); }
    AlgebraicArchive at(std::uint64_t sequence) const {
        const auto selected=store_.snapshot().at(sequence); return read_algebraic_archive(selected.endpoint(),channel_,limits_);
    }
    DurableNativeReceipt append(std::uint64_t transaction,const DurableNativeRoot& expected,
        std::span<const AlgebraicArchive> revisions,DurableFaultHook hook={}) {
        algebraic_archive_detail::require(!revisions.empty(),"empty algebraic archive transaction");
        const auto history=store_.snapshot(); const auto predecessor=history.at(expected.sequence); auto endpoint=predecessor.endpoint();
        algebraic_archive_detail::bound(revisions.size(),history.limits().max_segments,"archive transaction frame budget");
        std::vector<std::vector<std::byte>> frames; frames.reserve(revisions.size());
        std::size_t frame_bytes{};
        for (const auto& revision:revisions) {
            const auto admitted=decode_algebraic_archive(encode_algebraic_archive(revision,limits_),limits_);
            auto next=with_algebraic_archive(endpoint,channel_,admitted,limits_);
            if (endpoint.sequence==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("archive history sequence exhausted");
            next.sequence=endpoint.sequence+1;
            auto frame=encode_temporal_segment(make_temporal_segment(endpoint,next));
            frame_bytes=machine::checked_size_add(frame_bytes,frame.size());
            algebraic_archive_detail::bound(frame_bytes,history.limits().max_archive_bytes,"archive transaction byte budget");
            frames.push_back(std::move(frame)); endpoint=std::move(next);
        }
        return store_.append(transaction,expected,frames,std::move(hook));
    }
private:
    AlgebraicDurableHistory(Create,const std::filesystem::path& path,const TemporalEndpoint& endpoint,
        AlgebraicArchiveChannel channel,std::uint64_t source_namespace,AlgebraicArchiveLimits limits,DurableNativeLimits durable_limits)
        : store_(DurableNativeStore::create(path,endpoint,source_namespace,durable_limits)),channel_(channel),limits_(limits) {}
    AlgebraicDurableHistory(Open,const std::filesystem::path& path,AlgebraicArchiveChannel channel,
        AlgebraicArchiveLimits limits,DurableNativeLimits durable_limits)
        : store_(DurableNativeStore::open(path,durable_limits)),channel_(channel),limits_(limits) {
        algebraic_archive_history_detail::validate_channel(channel_);
        store_.snapshot().visit_endpoints([&](const auto& selected){(void)read_algebraic_archive(selected.endpoint(),channel_,limits_);});
    }
    DurableNativeStore store_;
    AlgebraicArchiveChannel channel_;
    AlgebraicArchiveLimits limits_;
};
} // namespace rfc::storage
