#pragma once

#include "rfc/storage/native_accession_view.hpp"

namespace rfc::storage {

// An execution directory, not persisted mathematical identity. Construction and
// refresh validate/index an endpoint; known-handle resolution does neither.
// One directory is externally synchronized. Pinned immutable views can outlive
// it and are independent of subsequent publications.
class NativeAccessionDirectory {
public:
    using Selector=std::function<std::uint64_t(std::span<const std::byte>)>;
    explicit NativeAccessionDirectory(const TemporalEndpoint& endpoint,std::uint64_t source_namespace,
        std::vector<RelationalCodeObservationQuery> queries={},
        Selector selector=[](auto bytes){return rfcx_detail::fnv1a64(bytes);},
        NativeAccessionLimits limits={},RelationalCodeObservationLimits observation_limits={})
        :namespace_(source_namespace),selector_(std::move(selector)),limits_(limits) {
        native_accession_detail::require(static_cast<bool>(selector_),"empty accession bucket selector");
        Guard guard(busy_);
        // Intentionally copy from a const reference before pinning. Moving a
        // caller's vectors could leave their old references as mutable aliases.
        auto store=std::make_unique<RelationalCodeObservationStore>(endpoint,std::move(queries),
            [](auto bytes){return rfcx_detail::fnv1a64(bytes);},observation_limits);
        auto next=build(*store,{});store_=std::move(store);published_=std::move(next);
    }
    explicit NativeAccessionDirectory(const NativeSeriesEndpoint& endpoint,
        std::vector<RelationalCodeObservationQuery> queries={},
        Selector selector=[](auto bytes){return rfcx_detail::fnv1a64(bytes);},
        NativeAccessionLimits limits={},RelationalCodeObservationLimits observation_limits={})
        :namespace_(endpoint.source_namespace()),selector_(std::move(selector)),limits_(limits) {
        native_accession_detail::require(static_cast<bool>(selector_),"empty accession bucket selector");
        Guard guard(busy_);
        auto store=std::make_unique<RelationalCodeObservationStore>(endpoint,std::move(queries),
            [](auto bytes){return rfcx_detail::fnv1a64(bytes);},observation_limits);
        auto next=build(*store,{});store_=std::move(store);published_=std::move(next);
    }
    NativeAccessionDirectory(const NativeAccessionDirectory&)=delete;
    NativeAccessionDirectory& operator=(const NativeAccessionDirectory&)=delete;
    NativeAccessionDirectory(NativeAccessionDirectory&&)=delete;
    NativeAccessionDirectory& operator=(NativeAccessionDirectory&&)=delete;

    // Cold, non-interning accession over this exact publication. The returned
    // immutable pin is reusable by multi-source atlases without copying sources.
    PinnedNativeAccession pin(const NativeAccessionRequest& input) const {
        Guard guard(busy_);
        native_accession_detail::preflight(input,limits_);
        auto request=input; native_accession_detail::canonicalize(request,limits_);
        auto bytes=native_accession_detail::request_bytes(request,limits_);
        return {published_->snapshot,native_accession_detail::bind(*published_->snapshot,
                    std::move(request),std::move(bytes))};
    }
    NativeAccessionHandle intern(const NativeAccessionRequest& input) {
        Guard guard(busy_);native_accession_detail::preflight(input,limits_);auto request=input;native_accession_detail::canonicalize(request,limits_);
        auto bytes=native_accession_detail::request_bytes(request,limits_);
        const auto hash=selector_(bytes);
        auto next=std::make_unique<Published>(*published_);
        ++next->stats.descriptor_hashes;
        if(const auto i=find_slot(*next,bytes,hash)) {
            published_.swap(next);return {published_->generation,*i};
        }
        const auto slot=insert(*next,std::move(request),std::move(bytes),hash);
        published_.swap(next);return {published_->generation,slot};
    }
    // Explicit context rebinding compares complete witnesses, not fingerprints.
    // Keys/slots alone never authorize reuse in a different endpoint or namespace.
    NativeAccessionHandle intern(const NativeAccessionDescriptor& descriptor) {
        readable();
        native_accession_detail::require(descriptor.binding_==published_->snapshot->binding ||
            *descriptor.binding_==*published_->snapshot->binding,"foreign/stale accession descriptor context");
        return intern(descriptor.request_);
    }
    std::optional<NativeAccessionHandle> find(const NativeAccessionRequest& input) const {
        Guard guard(busy_);native_accession_detail::preflight(input,limits_);auto request=input;native_accession_detail::canonicalize(request,limits_);
        const auto bytes=native_accession_detail::request_bytes(request,limits_);
        const auto hash=selector_(bytes);++published_->stats.descriptor_hashes;
        const auto i=find_slot(*published_,bytes,hash);
        if(!i)return std::nullopt;
        return NativeAccessionHandle{published_->generation,*i};
    }
    NativeAccessionHandle handle_at(std::size_t slot) const {
        readable();if(slot>=published_->entries.size())throw std::out_of_range("accession handle slot");
        return {published_->generation,slot};
    }
    PinnedNativeAccession resolve(const NativeAccessionHandle& handle) const {
        readable();const auto i=validate_handle(handle);
        PinnedNativeAccession view{published_->snapshot,published_->entries.at(i)};
        ++published_->stats.handle_resolutions;return view;
    }
    PinnedNativeAccession resolve(const NativeAccessionHandle& handle,NativeAccessionKind expected) const {
        readable();const auto i=validate_handle(handle);
        native_accession_detail::require(published_->entries[i]->request.kind==expected,"wrong accession handle kind");
        PinnedNativeAccession view{published_->snapshot,published_->entries[i]};
        ++published_->stats.handle_resolutions;return view;
    }
    bool active(const NativeAccessionHandle& handle) const { readable();return published_->entries.at(validate_handle(handle))->active; }
    NativeAccessionDescriptor descriptor(const NativeAccessionHandle& handle) const {
        readable();const auto& e=*published_->entries.at(validate_handle(handle));
        return {published_->snapshot->binding,e.request,e.bytes};
    }
    PinnedNativeCell cell(StableCellKey key) const {
        readable();return {published_->snapshot,published_->snapshot->cell(key)};
    }
    PinnedNativeOccurrence occurrence(StableOccurrenceKey key) const {
        readable();return {published_->snapshot,key.upper_grade,published_->snapshot->occurrence(key)};
    }
    std::shared_ptr<const TemporalEndpoint> pin_snapshot() const { readable();return published_->snapshot->endpoint; }
    std::vector<RelationalCodeObservationQuery> queries() const { readable();return store_->queries(); }
    std::uint64_t sequence() const { readable();return published_->snapshot->endpoint->sequence; }
    std::size_t size() const { readable();return published_->entries.size(); }
    NativeAccessionStats stats() const { readable();return published_->stats; }
    RelationalCodeObservationStats observation_stats() const { readable();return store_->stats(); }

    // Prepare E replay/invalidation and all directory bindings before publishing
    // either. Failure leaves endpoint, observations, handles and counters intact.
    // Every successful refresh issues a new generation; old pinned views survive,
    // while old handles cannot silently resolve against the new endpoint.
    NativeAccessionStats advance(const TemporalSegment& segment,
        std::optional<std::vector<NativeAccessionRequest>> requests=std::nullopt,
        std::optional<std::vector<RelationalCodeObservationQuery>> queries=std::nullopt) {
        Guard guard(busy_);auto candidate=store_->fork();candidate->advance(segment,std::move(queries));
        auto next=build(*candidate,requests?*requests:current_requests());
        store_.swap(candidate);published_.swap(next);return published_->stats;
    }
    NativeAccessionStats reconfigure(std::vector<NativeAccessionRequest> requests,
        std::optional<std::vector<RelationalCodeObservationQuery>> queries=std::nullopt) {
        Guard guard(busy_);auto candidate=store_->fork();if(queries)candidate->reconfigure(std::move(*queries));
        auto next=build(*candidate,requests);store_.swap(candidate);published_.swap(next);return published_->stats;
    }
private:
    using Snapshot=native_accession_detail::Snapshot;using Entry=native_accession_detail::Entry;
    struct Published {
        std::shared_ptr<const Snapshot> snapshot;
        std::shared_ptr<const native_accession_detail::Generation> generation=std::make_shared<native_accession_detail::Generation>();
        std::vector<std::shared_ptr<const Entry>> entries;
        std::unordered_map<std::uint64_t,std::vector<std::size_t>> buckets;
        NativeAccessionStats stats;
    };
    struct Guard {
        bool& flag;
        explicit Guard(bool& f):flag(f){native_accession_detail::require(!flag,"accession directory callback reentry");flag=true;}
        ~Guard(){flag=false;}
        Guard(const Guard&)=delete;Guard& operator=(const Guard&)=delete;
    };
    void readable() const { native_accession_detail::require(!busy_,"accession directory callback reentry"); }
    std::size_t validate_handle(const NativeAccessionHandle& h) const {
        const auto generation=h.generation_.lock();
        native_accession_detail::require(generation && generation==published_->generation,"stale/foreign accession handle");
        native_accession_detail::require(h.slot_<published_->entries.size(),"invalid accession handle slot");return h.slot_;
    }
    std::optional<std::size_t> find_slot(Published& p,std::span<const std::byte> bytes,std::uint64_t hash) const {
        const auto it=p.buckets.find(hash);if(it==p.buckets.end())return std::nullopt;
        for(auto slot:it->second) {
            ++p.stats.bucket_comparisons;const auto& b=p.entries.at(slot)->bytes;
            if(b.size()==bytes.size() && std::equal(b.begin(),b.end(),bytes.begin()))return slot;
        }
        return std::nullopt;
    }
    std::size_t insert(Published& p,NativeAccessionRequest request,native_accession_detail::Bytes bytes,std::uint64_t hash) const {
        native_accession_detail::budget(machine::checked_size_add(p.entries.size(),1),limits_.max_handles,"accession handle budget");
        const auto it=p.buckets.find(hash);const auto bucket_size=it==p.buckets.end()?0:it->second.size();
        native_accession_detail::budget(machine::checked_size_add(bucket_size,1),limits_.max_bucket_entries,"accession collision bucket budget");
        const auto total=machine::checked_size_add(p.stats.descriptor_bytes,bytes.size());
        native_accession_detail::budget(total,limits_.max_total_descriptor_bytes,"accession aggregate descriptor budget");
        auto entry=native_accession_detail::bind(*p.snapshot,std::move(request),std::move(bytes));
        const auto slot=p.entries.size();p.entries.push_back(std::move(entry));p.buckets[hash].push_back(slot);
        p.stats.descriptor_bytes=total;p.stats.handles=p.entries.size();++p.stats.cold_bindings;
        if(!p.entries.back()->active)++p.stats.dormant;
        return slot;
    }
    std::vector<NativeAccessionRequest> current_requests() const {
        std::vector<NativeAccessionRequest> out;out.reserve(published_->entries.size());
        for(const auto& e:published_->entries)out.push_back(e->request);
        return out;
    }
    std::unique_ptr<Published> build(const RelationalCodeObservationStore& store,std::vector<NativeAccessionRequest> requests) const {
        native_accession_detail::budget(requests.size(),limits_.max_handles,"accession subscription budget");
        auto next=std::make_unique<Published>();next->snapshot=std::make_shared<Snapshot>(store,namespace_,limits_,authority_);
        next->stats=next->snapshot->stats;
        // All reads/bindings use a pinned immutable endpoint, including when a
        // user selector invokes external code. Reentry into this owner rejects.
        for(auto& r:requests) {
            native_accession_detail::canonicalize(r,limits_);auto bytes=native_accession_detail::request_bytes(r,limits_);
            const auto hash=selector_(bytes);++next->stats.descriptor_hashes;
            native_accession_detail::require(!find_slot(*next,bytes,hash),"duplicate accession subscription");
            (void)insert(*next,std::move(r),std::move(bytes),hash);
        }
        return next;
    }
    std::uint64_t namespace_{};Selector selector_;NativeAccessionLimits limits_;
    std::shared_ptr<const native_accession_detail::ExecutionAuthority> authority_{
        std::make_shared<const native_accession_detail::ExecutionAuthority>()};
    std::unique_ptr<RelationalCodeObservationStore> store_;
    std::unique_ptr<Published> published_;
    mutable bool busy_{};
};
} // namespace rfc::storage
