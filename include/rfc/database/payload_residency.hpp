#pragma once

#include "rfc/storage/payload_manifest.hpp"
#include "rfc/runtime/execution_policy.hpp"
#include <mutex>

namespace rfc::database {
enum class PayloadResidencyState { Unloaded, Loading, Resident, Refused };
enum class PayloadRefusal { None, Budget, Cancelled, Integrity, Abandoned, Provider };
struct PayloadReceivingLimits {
    std::size_t max_metadata_bytes{64u << 20}, max_payload_bytes{64u << 20}, max_loading{16};
};
struct PayloadResidencyUsage {
    std::size_t metadata_bytes{}, loading_payload_bytes{}, resident_payload_bytes{}, loading{}, resident{};
    bool operator==(const PayloadResidencyUsage&) const = default;
};
struct PayloadResidencyStatus {
    PayloadResidencyState state{PayloadResidencyState::Unloaded};
    PayloadRefusal refusal{PayloadRefusal::None};
    std::uint64_t generation{};
    bool operator==(const PayloadResidencyStatus&) const = default;
};

// Payload admission and byte residency only. A resident byte image is not a decoded
// native source, observation, zero field, complete family, or certificate.
// Providers and typed preparation are separate. Read leases pin these bytes;
// eviction refuses while a reader or completion is active. There is no gluing executor.
class PayloadResidency {
    struct Slot {
        PayloadResidencyStatus status;
        std::vector<std::byte> bytes;
        bool completing{};
        std::size_t readers{};
    };
    struct State {
        storage::PayloadManifest manifest;
        PayloadReceivingLimits limits;
        PayloadResidencyUsage usage;
        std::vector<Slot> slots;
        mutable std::mutex mutex;
        State(storage::PayloadManifest m, PayloadReceivingLimits l) : manifest(std::move(m)), limits(l) {}
    };
    std::shared_ptr<State> state_;
    static void refuse_locked(State& s, std::size_t index, std::uint64_t generation, PayloadRefusal why) noexcept {
        auto& slot=s.slots[index];
        if(slot.status.state!=PayloadResidencyState::Loading || slot.status.generation!=generation) return;
        if(!slot.completing) {
            s.usage.loading_payload_bytes-=static_cast<std::size_t>(s.manifest.payloads()[index].stored_bytes);
            --s.usage.loading;
        }
        slot.status.state=PayloadResidencyState::Refused; slot.status.refusal=why;
    }
public:
    class ReadLease {
        friend class PayloadResidency;
        std::shared_ptr<State> state_;
        std::size_t index_{};
        ReadLease(std::shared_ptr<State> state,std::size_t index) :state_(std::move(state)),index_(index) {}
        void release() noexcept {
            if(state_) {std::lock_guard lock(state_->mutex);--state_->slots[index_].readers;}
        }
    public:
        ReadLease(const ReadLease& other):state_(other.state_),index_(other.index_) {
            if(state_) {std::lock_guard lock(state_->mutex);auto& n=state_->slots[index_].readers;n=storage::payload_manifest_detail::checked_size_add(n,1);}
        }
        ReadLease(ReadLease&& other) noexcept:state_(std::move(other.state_)),index_(other.index_) {}
        ReadLease& operator=(const ReadLease&)=delete;
        ReadLease& operator=(ReadLease&&)=delete;
        ~ReadLease(){release();}
        std::span<const std::byte> bytes() const & {
            if(!state_) throw std::invalid_argument("moved payload read lease");
            return state_->slots[index_].bytes;
        }
        std::span<const std::byte> bytes() const && = delete;
    };
    class Ticket {
        friend class PayloadResidency;
        std::shared_ptr<State> state_;
        std::size_t index_{};
        std::uint64_t generation_{};
        Ticket(std::shared_ptr<State> s, std::size_t i, std::uint64_t g) : state_(std::move(s)),index_(i),generation_(g) {}
        void abandon() noexcept {
            if(!state_) return;
            std::lock_guard lock(state_->mutex);
            refuse_locked(*state_,index_,generation_,PayloadRefusal::Abandoned);
        }
    public:
        Ticket(const Ticket&)=delete;
        Ticket& operator=(const Ticket&)=delete;
        Ticket(Ticket&& other) noexcept
            :state_(std::move(other.state_)),index_(other.index_),generation_(other.generation_) {}
        Ticket& operator=(Ticket&& other) noexcept {
            if(this!=&other) { abandon(); state_=std::move(other.state_); index_=other.index_; generation_=other.generation_; }
            return *this;
        }
        ~Ticket() { abandon(); }
        std::uint64_t generation() const noexcept { return generation_; }
    };
    explicit PayloadResidency(storage::PayloadManifest manifest, PayloadReceivingLimits limits={}) {
        using namespace storage::payload_manifest_detail;
        const auto metadata=checked_size_add(manifest.usage().metadata_bytes,
            checked_size_add(sizeof(State),checked_size_mul(manifest.payloads().size(),sizeof(Slot))));
        bound(metadata,limits.max_metadata_bytes,"payload residency metadata admission");
        auto s=std::make_shared<State>(std::move(manifest),limits); s->usage.metadata_bytes=metadata;
        s->slots.resize(s->manifest.payloads().size()); state_=std::move(s);
    }
    // Copies share a fixed owner. Assignment cannot redirect an in-flight
    // completion's callbacks or tickets to another manifest/table.
    PayloadResidency(const PayloadResidency&)=default;
    PayloadResidency& operator=(const PayloadResidency&)=delete;
    const storage::PayloadManifest& manifest() const & noexcept { return state_->manifest; }
    const storage::PayloadManifest& manifest() const && = delete;
    PayloadResidencyUsage usage() const { std::lock_guard lock(state_->mutex); return state_->usage; }
    PayloadResidencyStatus status(const storage::PayloadKey& key) const {
        const auto i=locate(key); std::lock_guard lock(state_->mutex); return state_->slots[i].status;
    }
    ReadLease pin(const storage::PayloadKey& key) const {
        const auto i=locate(key);std::lock_guard lock(state_->mutex);auto& slot=state_->slots[i];
        storage::payload_manifest_detail::require(slot.status.state==PayloadResidencyState::Resident,"payload is not resident");
        slot.readers=storage::payload_manifest_detail::checked_size_add(slot.readers,1);
        return ReadLease(state_,i);
    }
    bool evict(const storage::PayloadKey& key) {
        const auto i=locate(key);std::lock_guard lock(state_->mutex);auto& slot=state_->slots[i];
        if(slot.status.state!=PayloadResidencyState::Resident || slot.readers || slot.completing) return false;
        state_->usage.resident_payload_bytes-=slot.bytes.size();--state_->usage.resident;
        std::vector<std::byte>().swap(slot.bytes);
        slot.status.state=PayloadResidencyState::Unloaded;slot.status.refusal=PayloadRefusal::None;
        return true;
    }
    Ticket begin(const storage::PayloadKey& key, const storage::PayloadSourceRevision& expected_source,
                 std::function<void()> check={}) {
        using namespace storage::payload_manifest_detail;
        require(key.source==expected_source,"payload receiving source revision differs");
        const auto i=locate(key); const auto bytes=checked_size(state_->manifest.payloads()[i].stored_bytes);
        // Callbacks run outside locks. Cancellation before admission changes no state.
        if(check) check();
        std::lock_guard lock(state_->mutex); auto& slot=state_->slots[i];
        require(slot.status.state==PayloadResidencyState::Unloaded || slot.status.state==PayloadResidencyState::Refused,
                "payload already loading/resident");
        require(!slot.completing,"payload completion still owns its reservation");
        require(slot.status.generation!=std::numeric_limits<std::uint64_t>::max(),"payload generation exhausted");
        const auto occupied=checked_size_add(state_->usage.loading_payload_bytes,state_->usage.resident_payload_bytes);
        if(bytes>state_->limits.max_payload_bytes-occupied || state_->usage.loading>=state_->limits.max_loading) {
            slot.status.state=PayloadResidencyState::Refused; slot.status.refusal=PayloadRefusal::Budget;
            throw std::length_error("payload residency receiving admission");
        }
        ++slot.status.generation; slot.status.state=PayloadResidencyState::Loading; slot.status.refusal=PayloadRefusal::None;
        state_->usage.loading_payload_bytes=checked_size_add(state_->usage.loading_payload_bytes,bytes); ++state_->usage.loading;
        return Ticket(state_,i,slot.status.generation);
    }
    void refuse(Ticket& ticket, PayloadRefusal why) {
        storage::payload_manifest_detail::require(why==PayloadRefusal::Cancelled || why==PayloadRefusal::Integrity ||
            why==PayloadRefusal::Abandoned || why==PayloadRefusal::Provider,"invalid payload refusal reason");
        std::lock_guard lock(state_->mutex); require_ticket(ticket);
        refuse_locked(*state_,ticket.index_,ticket.generation_,why); ticket.state_.reset();
    }
    void commit(Ticket& ticket, std::span<const std::byte> bytes, std::function<void()> check={}) {
        using namespace storage::payload_manifest_detail;
        std::size_t i{}; std::uint64_t generation{};
        {
            std::lock_guard lock(state_->mutex); require_ticket(ticket); i=ticket.index_; generation=ticket.generation_;
            require(!state_->slots[i].completing,"payload completion is already in progress");
            state_->slots[i].completing=true;
        }
        const auto& descriptor=state_->manifest.payloads()[i];
        try {
            require(bytes.size()==descriptor.stored_bytes,"payload size differs from immutable manifest");
            // Reservation covers this owned copy; callbacks cannot mutate it.
            std::vector<std::byte> frozen(bytes.begin(),bytes.end());
            if(check) check();
            require(storage::rfcx_detail::fnv1a64(frozen)==descriptor.checksum,"payload checksum differs from immutable manifest");
            if(check) check();
            std::lock_guard lock(state_->mutex); require_ticket(ticket);
            require(ticket.index_==i && ticket.generation_==generation,"payload completion ticket changed");
            auto& slot=state_->slots[i]; slot.bytes=std::move(frozen);
            state_->usage.loading_payload_bytes-=slot.bytes.size(); --state_->usage.loading;
            state_->usage.resident_payload_bytes=checked_size_add(state_->usage.resident_payload_bytes,slot.bytes.size()); ++state_->usage.resident;
            slot.completing=false; slot.status.state=PayloadResidencyState::Resident; ticket.state_.reset();
        } catch(const runtime::ExecutionCancelled&) {
            fail_if_current(i,generation,ticket,PayloadRefusal::Cancelled); throw;
        } catch(const std::bad_alloc&) {
            fail_if_current(i,generation,ticket,PayloadRefusal::Budget); throw;
        } catch(...) {
            fail_if_current(i,generation,ticket,PayloadRefusal::Integrity); throw;
        }
    }
    // Explicit bounded output copy, not a resident lease or an eviction API.
    std::vector<std::byte> copy_payload(const storage::PayloadKey& key, std::size_t max_output_bytes) const {
        const auto i=locate(key); std::lock_guard lock(state_->mutex); const auto& slot=state_->slots[i];
        storage::payload_manifest_detail::require(slot.status.state==PayloadResidencyState::Resident,"payload is not resident");
        storage::payload_manifest_detail::bound(slot.bytes.size(),max_output_bytes,"payload output copy admission");
        return slot.bytes;
    }
private:
    std::size_t locate(const storage::PayloadKey& key) const {
        const auto i=state_->manifest.find(key);
        if(!i) throw std::out_of_range("payload is absent from the declared manifest");
        return *i;
    }
    void require_ticket(const Ticket& ticket) const {
        using namespace storage::payload_manifest_detail;
        require(ticket.state_==state_,"foreign or consumed payload ticket");
        const auto& status=state_->slots[ticket.index_].status;
        require(status.state==PayloadResidencyState::Loading && status.generation==ticket.generation_,"stale payload generation");
    }
    void fail_if_current(std::size_t i, std::uint64_t generation, Ticket& ticket, PayloadRefusal why) noexcept {
        std::lock_guard lock(state_->mutex);
        auto& slot=state_->slots[i];
        if(slot.status.generation==generation && slot.completing) {
            state_->usage.loading_payload_bytes-=static_cast<std::size_t>(state_->manifest.payloads()[i].stored_bytes);
            --state_->usage.loading; slot.completing=false;
            if(slot.status.state==PayloadResidencyState::Loading) {
                slot.status.state=PayloadResidencyState::Refused; slot.status.refusal=why;
            }
        }
        if(ticket.state_==state_ && ticket.index_==i && ticket.generation_==generation) ticket.state_.reset();
    }
};
} // namespace rfc::database
