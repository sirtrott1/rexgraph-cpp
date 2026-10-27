#pragma once
#include "rfc/database/payload_residency.hpp"
#include "rfc/storage/payload_archive.hpp"
#include "rfc/storage/rfcx_payload.hpp"
#include <exception>

namespace rfc::database {
struct NativePayloadLimits {
    PayloadReceivingLimits raw;
    storage::RfcxPayloadLimits preparation;
    std::size_t max_metadata_bytes{128u<<20},max_decoded_bytes{256u<<20},max_coefficient_bits{64u<<20};
    std::size_t max_scratch_bytes{512u<<20},max_io_bytes{1u<<30},max_entries{64},max_selection_records{4096};
};
struct NativePayloadUsage {
    std::size_t metadata_bytes{},selection_bytes{},pending_decoded_bytes{},resident_decoded_bytes{};
    std::size_t pending_coefficient_bits{},resident_coefficient_bits{},scratch_bytes{},admitted_io_bytes{};
    std::size_t loading{},resident{},hits{},loads{},failures{},evictions{},peak_scratch_bytes{};
};
enum class NativePayloadFailure {None,Budget,Cancelled,Stale,Provider,Integrity};
struct NativePayloadStatus {
    PayloadResidencyState state{PayloadResidencyState::Unloaded};NativePayloadFailure failure{NativePayloadFailure::None};std::uint64_t generation{};
};
class PayloadGenerationChanged:public std::runtime_error {
public:PayloadGenerationChanged():std::runtime_error("payload load generation changed"){}
};

// First typed demand profile: complete canonical RFCX sources. The payload manifest
// and raw table remain authoritative for byte admission; this layer additionally
// verifies actual source structure and measured retained native/GMP storage.
// It returns the WHOLE declared dependency closure, never a product of marginals
// or a global gluing/learning certificate. Other typed codecs require their own gate.
class NativePayloadCache {
    using Key=storage::PayloadKey;
    struct Entry {
        core::RelationalComplex source;
        PayloadResidency::ReadLease raw;
        storage::RfcxPayloadFootprint footprint;
        Key key;std::uint64_t generation{};
        Entry(storage::PreparedRfcxPayload value,PayloadResidency::ReadLease bytes,Key identity,std::uint64_t gen)
            :source(std::move(value.source)),raw(std::move(bytes)),footprint(value.footprint),key(identity),generation(gen){}
    };
    struct Slot {std::shared_ptr<const Entry> entry;NativePayloadStatus status;bool in_flight{};std::uint64_t touched{};};
    struct State {
        storage::PayloadArchiveProvider provider;PayloadResidency raw;NativePayloadLimits limits;
        NativePayloadUsage usage;std::vector<Slot> slots;std::uint64_t clock{};mutable std::mutex mutex;
        State(storage::PayloadArchiveProvider p,NativePayloadLimits l):provider(std::move(p)),raw(provider.manifest(),l.raw),limits(l){}
    };
    std::shared_ptr<State> state_;
    static bool evict_locked(State& s,std::size_t i) {
        auto& slot=s.slots[i];if(slot.in_flight || !slot.entry || slot.entry.use_count()!=1)return false;
        const auto footprint=slot.entry->footprint;
        slot.entry.reset(); // Destroy actual source and read pin before releasing budgets.
        if(!s.raw.evict(s.provider.manifest().payloads()[i].identity))throw std::logic_error("unaccounted native payload read pin");
        s.usage.resident_decoded_bytes-=footprint.decoded_bytes;s.usage.resident_coefficient_bits-=footprint.coefficient_bits;
        --s.usage.resident;++s.usage.evictions;slot.status.state=PayloadResidencyState::Unloaded;slot.status.failure=NativePayloadFailure::None;
        return true;
    }
    static void room(State& s,std::size_t raw_bytes,std::size_t decoded,std::size_t bits,std::size_t entries) {
        using namespace storage::payload_manifest_detail;
        bound(raw_bytes,s.limits.raw.max_payload_bytes,"native raw payload admission");
        bound(decoded,s.limits.max_decoded_bytes,"native decoded payload admission");
        bound(bits,s.limits.max_coefficient_bits,"native coefficient payload admission");
        bound(entries,s.limits.max_entries,"native payload entry admission");
        for(;;) {
            const auto raw=s.raw.usage();
            const bool fits=raw_bytes<=s.limits.raw.max_payload_bytes-checked_size_add(raw.loading_payload_bytes,raw.resident_payload_bytes) &&
                decoded<=s.limits.max_decoded_bytes-checked_size_add(s.usage.pending_decoded_bytes,s.usage.resident_decoded_bytes) &&
                bits<=s.limits.max_coefficient_bits-checked_size_add(s.usage.pending_coefficient_bits,s.usage.resident_coefficient_bits) &&
                entries<=s.limits.max_entries-checked_size_add(s.usage.loading,s.usage.resident);
            if(fits)return;
            std::optional<std::size_t> victim;
            for(std::size_t i=0;i<s.slots.size();++i)if(s.slots[i].entry && !s.slots[i].in_flight && s.slots[i].entry.use_count()==1 &&
                (!victim || s.slots[i].touched<s.slots[*victim].touched))victim=i;
            if(!victim || !evict_locked(s,*victim))throw std::length_error("native payload cache occupied by pinned/pending work");
        }
    }
    static void touch(State& s,Slot& slot) {
        if(s.clock==std::numeric_limits<std::uint64_t>::max())throw std::length_error("payload cache clock exhausted");
        slot.touched=++s.clock;
    }
public:
    class Lease {
        friend class NativePayloadCache;
        std::shared_ptr<State> state_;std::shared_ptr<const Entry> entry_;
        Lease(std::shared_ptr<State> state,std::shared_ptr<const Entry> entry):state_(std::move(state)),entry_(std::move(entry)){}
    public:
        // The owner/pin accompanies every copy; only borrowing source/byte views
        // are exposed. No uncharged owning pointer to the retained allocation escapes.
        const core::RelationalComplex& source() const & {return entry_->source;}
        const core::RelationalComplex& source() const &&=delete;
        std::span<const std::byte> bytes() const & {return entry_->raw.bytes();}
        std::span<const std::byte> bytes() const &&=delete;
        Key key() const{return entry_->key;}
        std::uint64_t generation() const{return entry_->generation;}
        storage::RfcxPayloadFootprint footprint() const{return entry_->footprint;}
        bool same_owner(const Lease& other) const{return entry_==other.entry_;}
    };
    class Selection {
        friend class NativePayloadCache;
        struct Held {
            std::shared_ptr<State> state;std::vector<Lease> members;std::size_t charge{};
            explicit Held(std::shared_ptr<State> s):state(std::move(s)){}
            ~Held(){std::vector<Lease>().swap(members);std::lock_guard lock(state->mutex);state->usage.selection_bytes-=charge;}
        };
        std::shared_ptr<Held> held_;
        explicit Selection(std::shared_ptr<Held> held):held_(std::move(held)){}
    public:
        const Lease& root() const & {return held_->members.front();}
        const Lease& root() const &&=delete;
        const std::vector<Lease>& members() const & {return held_->members;}
        const std::vector<Lease>& members() const &&=delete;
        const Lease& at(const Key& key) const & {
            for(const auto& item:held_->members)if(item.key()==key)return item;
            throw std::out_of_range("payload outside selected dependency closure");
        }
        const Lease& at(const Key&) const &&=delete;
    };
    explicit NativePayloadCache(storage::PayloadArchiveProvider provider,NativePayloadLimits limits={}) {
        using namespace storage::payload_manifest_detail;
        const auto count=provider.manifest().payloads().size();
        const auto metadata=checked_size_add(provider.usage().metadata_bytes,
            checked_size_add(provider.manifest().usage().metadata_bytes,checked_size_add(sizeof(State),checked_size_mul(count,sizeof(Slot)+256))));
        bound(metadata,limits.max_metadata_bytes,"native payload metadata admission");
        auto state=std::make_shared<State>(std::move(provider),limits);state->slots.resize(count);
        state->usage.metadata_bytes=checked_size_add(state->provider.usage().metadata_bytes,
            checked_size_add(state->raw.usage().metadata_bytes,checked_size_add(sizeof(State),checked_size_mul(count,sizeof(Slot)+256))));
        bound(state->usage.metadata_bytes,limits.max_metadata_bytes,"native payload metadata admission");state_=std::move(state);
    }
    NativePayloadCache(const NativePayloadCache&)=default;
    NativePayloadCache& operator=(const NativePayloadCache&)=delete;
    const storage::PayloadManifest& manifest() const & {return state_->provider.manifest();}
    const storage::PayloadManifest& manifest() const &&=delete;
    NativePayloadUsage usage() const {std::lock_guard lock(state_->mutex);return state_->usage;}
    PayloadResidencyUsage raw_usage() const{return state_->raw.usage();}
    storage::PayloadArchiveUsage archive_usage() const{return state_->provider.usage();}
    NativePayloadStatus status(const Key& key) const {auto i=locate(key);std::lock_guard lock(state_->mutex);return state_->slots[i].status;}
    bool evict(const Key& key){const auto i=locate(key);std::lock_guard lock(state_->mutex);return evict_locked(*state_,i);}
    bool cancel(const Key& key) {
        const auto i=locate(key);std::lock_guard lock(state_->mutex);auto& slot=state_->slots[i];
        if(!slot.in_flight)return false;
        if(slot.status.generation==std::numeric_limits<std::uint64_t>::max())throw std::length_error("payload generation exhausted");
        ++slot.status.generation;return true;
    }
    Selection acquire(const Key& key,const storage::PayloadSourceRevision& expected_source,std::function<void()> check={}) {
        using namespace storage::payload_manifest_detail;
        require(key.source==expected_source,"native payload receiving source revision differs");
        const auto root=locate(key);if(check){check();}
        const auto charge=checked_size_add(sizeof(typename Selection::Held),checked_size_mul(state_->limits.max_selection_records,sizeof(Lease)+128));
        std::shared_ptr<typename Selection::Held> held;
        {
            std::lock_guard lock(state_->mutex);
            bound(checked_size_add(state_->usage.metadata_bytes,checked_size_add(state_->usage.selection_bytes,charge)),state_->limits.max_metadata_bytes,"payload selection metadata admission");
            state_->usage.selection_bytes=checked_size_add(state_->usage.selection_bytes,charge);
        }
        try {held=std::make_shared<typename Selection::Held>(state_);held->charge=charge;}
        catch(...) {std::lock_guard lock(state_->mutex);state_->usage.selection_bytes-=charge;throw;}
        std::vector<std::size_t> closure;std::set<std::size_t> seen;
        const auto append=[&](std::size_t i){if(seen.contains(i))return;bound(checked_size_add(closure.size(),1),state_->limits.max_selection_records,"payload selection record admission");seen.insert(i);closure.push_back(i);};
        append(root);
        for(std::size_t i=0;i<closure.size();++i) {
            if(check){check();}const auto& descriptor=manifest().payloads()[closure[i]];
            require(descriptor.format=="RFCX-1" && descriptor.scope==storage::PayloadScope::BoundaryClosed,"unsupported typed payload profile/scope");
            for(const auto& dependency:descriptor.dependencies)append(*manifest().find(dependency));
        }
        held->members.reserve(closure.size());
        for(auto i:closure)held->members.push_back(load(i,check));
        if(check){check();}return Selection(std::move(held));
    }
private:
    std::size_t locate(const Key& key) const {const auto i=manifest().find(key);if(!i)throw std::out_of_range("undeclared native payload/source revision");return *i;}
    Lease load(std::size_t i,const std::function<void()>& check) {
        using namespace storage::payload_manifest_detail;
        const auto key=manifest().payloads()[i].identity;const auto n=checked_size(manifest().payloads()[i].stored_bytes);
        std::uint64_t generation{};std::optional<PayloadResidency::Ticket> ticket;
        std::size_t scratch{},decoded{},bits{};NativePayloadFailure failure=NativePayloadFailure::Integrity;
        try {
            std::lock_guard lock(state_->mutex);auto& slot=state_->slots[i];
            if(slot.entry){touch(*state_,slot);++state_->usage.hits;return Lease(state_,slot.entry);}
            require(!slot.in_flight,"native payload load already in progress");
            require(slot.status.generation!=std::numeric_limits<std::uint64_t>::max(),"native payload generation exhausted");
            bound(n,state_->limits.preparation.max_file_bytes,"native payload receiving file admission");
            room(*state_,n,0,0,1);
            scratch=checked_size_mul(n,128);bound(checked_size_add(state_->usage.scratch_bytes,scratch),state_->limits.max_scratch_bytes,"native payload inspection scratch admission");
            bound(checked_size_add(state_->usage.admitted_io_bytes,n),state_->limits.max_io_bytes,"native payload I/O admission");
            ticket.emplace(state_->raw.begin(key,key.source));
            generation=++slot.status.generation;slot.in_flight=true;slot.status.state=PayloadResidencyState::Loading;slot.status.failure=NativePayloadFailure::None;
            ++state_->usage.loading;state_->usage.scratch_bytes=checked_size_add(state_->usage.scratch_bytes,scratch);
            state_->usage.peak_scratch_bytes=std::max(state_->usage.peak_scratch_bytes,state_->usage.scratch_bytes);
            state_->usage.admitted_io_bytes=checked_size_add(state_->usage.admitted_io_bytes,n);
        } catch(const std::length_error&) {
            std::lock_guard lock(state_->mutex);auto& slot=state_->slots[i];
            if(!slot.in_flight && !slot.entry){slot.status.state=PayloadResidencyState::Refused;slot.status.failure=NativePayloadFailure::Budget;++state_->usage.failures;}
            throw;
        }
        const auto tick=[&] {
            if(check){check();}std::lock_guard lock(state_->mutex);
            if(state_->slots[i].status.generation!=generation)throw PayloadGenerationChanged();
        };
        std::exception_ptr cause;
        try {
            auto buffer=state_->provider.read(key,state_->limits.preparation.max_file_bytes,tick);
            const auto plan=storage::rfcx_payload_detail::inspect(buffer,state_->limits.preparation);tick();
            bound(plan.decoded_bytes,checked_size(manifest().payloads()[i].decoded_bytes),"native payload decoded declaration admission");
            bound(plan.coefficient_bits,checked_size(manifest().payloads()[i].coefficient_bits),"native payload coefficient declaration admission");
            {
                std::lock_guard lock(state_->mutex);room(*state_,0,plan.decoded_bytes,plan.coefficient_bits,0);
                const auto required=std::max(scratch,plan.scratch_bytes);
                bound(checked_size_add(state_->usage.scratch_bytes-scratch,required),state_->limits.max_scratch_bytes,"native preparation scratch admission");
                state_->usage.scratch_bytes=checked_size_add(state_->usage.scratch_bytes-scratch,required);scratch=required;
                state_->usage.peak_scratch_bytes=std::max(state_->usage.peak_scratch_bytes,state_->usage.scratch_bytes);
                decoded=plan.decoded_bytes;bits=plan.coefficient_bits;
                state_->usage.pending_decoded_bytes=checked_size_add(state_->usage.pending_decoded_bytes,decoded);
                state_->usage.pending_coefficient_bits=checked_size_add(state_->usage.pending_coefficient_bits,bits);
            }
            state_->raw.commit(*ticket,buffer,tick);ticket.reset();std::vector<std::byte>().swap(buffer);
            auto raw=state_->raw.pin(key);
            auto prepared=storage::prepare_rfcx_payload(raw.bytes(),manifest(),key,state_->limits.preparation,tick);
            auto entry=std::make_shared<const Entry>(std::move(prepared),std::move(raw),key,generation);tick();
            {
                std::lock_guard lock(state_->mutex);auto& slot=state_->slots[i];
                if(slot.status.generation!=generation)throw PayloadGenerationChanged();
                touch(*state_,slot);slot.entry=entry;slot.in_flight=false;slot.status.state=PayloadResidencyState::Resident;
                state_->usage.pending_decoded_bytes-=decoded;state_->usage.pending_coefficient_bits-=bits;state_->usage.scratch_bytes-=scratch;
                state_->usage.resident_decoded_bytes=checked_size_add(state_->usage.resident_decoded_bytes,entry->footprint.decoded_bytes);
                state_->usage.resident_coefficient_bits=checked_size_add(state_->usage.resident_coefficient_bits,entry->footprint.coefficient_bits);
                --state_->usage.loading;++state_->usage.resident;++state_->usage.loads;
            }
            return Lease(state_,std::move(entry));
        } catch(const PayloadGenerationChanged&) {failure=NativePayloadFailure::Stale;cause=std::current_exception();}
          catch(const runtime::ExecutionCancelled&) {failure=NativePayloadFailure::Cancelled;cause=std::current_exception();}
          catch(const std::length_error&) {failure=NativePayloadFailure::Budget;cause=std::current_exception();}
          catch(const std::bad_alloc&) {failure=NativePayloadFailure::Budget;cause=std::current_exception();}
          catch(const storage::PayloadArchiveReadError&) {failure=NativePayloadFailure::Provider;cause=std::current_exception();}
          catch(...) {failure=NativePayloadFailure::Integrity;cause=std::current_exception();}
        // All local byte/native objects have unwound before budgets become free.
        ticket.reset();(void)state_->raw.evict(key);
        {
            std::lock_guard lock(state_->mutex);auto& slot=state_->slots[i];slot.in_flight=false;
            slot.status.state=PayloadResidencyState::Refused;slot.status.failure=failure;
            state_->usage.pending_decoded_bytes-=decoded;state_->usage.pending_coefficient_bits-=bits;state_->usage.scratch_bytes-=scratch;
            --state_->usage.loading;++state_->usage.failures;
        }
        std::rethrow_exception(cause);
    }
};
} // namespace rfc::database
