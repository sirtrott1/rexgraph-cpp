#pragma once

#include "rfc/runtime/hardware_inventory.hpp"
#include "rfc/runtime/execution_policy.hpp"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <stop_token>
#include <tuple>

namespace rfc::runtime {
enum class CpuWorkerBasis { AllowedLogicalThreads, ObservedPhysicalCoreCount };
struct CpuResourcePolicy {
    std::size_t worker_limit{1}, declared_buffer_byte_limit{}, tile_limit{1000000};
    CpuWorkerBasis basis{CpuWorkerBasis::AllowedLogicalThreads};
};
struct CpuExecutionFacts {
    std::string platform, model;
    bool affinity_observed{};
    std::vector<CpuThreadInfo> cpus;
    std::vector<CpuCacheInfo> caches;
    std::vector<std::tuple<std::string,std::optional<CpuTimeQuota>,bool>> visible_quotas;
    bool operator==(const CpuExecutionFacts&) const = default;
};
inline CpuExecutionFacts cpu_execution_facts(const HardwareInventory& hardware) {
    CpuExecutionFacts facts{hardware.platform,hardware.cpu_model,hardware.affinity_observed,hardware.allowed_cpus,hardware.caches,{}};
    for(const auto& group:hardware.cgroups)facts.visible_quotas.emplace_back(group.path,group.cpu_quota,group.cpu_unlimited);
    return facts;
}
inline std::size_t admitted_cpu_workers(const HardwareInventory& hardware,const CpuResourcePolicy& policy) {
    if(!policy.worker_limit)throw std::invalid_argument("resource CPU worker limit must be positive");
    if(!hardware.affinity_observed || hardware.allowed_cpus.empty())throw std::invalid_argument("CPU resource admission needs observed affinity");
    for(const auto& observation:hardware.observations)
        if(observation.status==ObservationStatus::Malformed && observation.source.ends_with("/cpu.max"))
            throw std::invalid_argument("malformed observed CPU quota");
    auto count=std::min(policy.worker_limit,hardware.allowed_cpus.size());
    switch(policy.basis) {
    case CpuWorkerBasis::AllowedLogicalThreads: break;
    case CpuWorkerBasis::ObservedPhysicalCoreCount:
        if(const auto cores=hardware.physical_cores())count=std::min(count,*cores);
        else throw std::invalid_argument("physical CPU core count is unknown");
        break;
    default: throw std::invalid_argument("unknown CPU worker basis");
    }
    for(const auto& group:hardware.cgroups)if(group.cpu_quota) {
        if(!group.cpu_quota->microseconds || !group.cpu_quota->period_microseconds)
            throw std::invalid_argument("invalid observed CPU quota");
        count=std::min(count,machine::ceil_div(group.cpu_quota->microseconds,group.cpu_quota->period_microseconds));
    }
    return count;
}
class CpuResourceBusy : public std::runtime_error {
public:
    CpuResourceBusy() : std::runtime_error("cooperating CPU resource budget is occupied") {}
};
// One coordinator covers only its cooperating calls. Reservations do not pin
// CPUs, allocate memory, exclude external processes or cap GMP/process heap.
class CpuResourceCoordinator {
public:
    class Reservation;
private:
    struct State {
        CpuExecutionFacts facts;
        CpuResourcePolicy policy;
        std::size_t workers{}, used_workers{}, used_bytes{};
        std::mutex mutex;
        std::condition_variable_any changed;
        std::uint64_t epoch{};
        std::shared_ptr<Reservation> parent;
        State(CpuExecutionFacts f,CpuResourcePolicy p,std::size_t n) : facts(std::move(f)),policy(p),workers(n) {}
    };
public:
    class Reservation {
    public:
        Reservation(const Reservation&)=delete;
        Reservation& operator=(const Reservation&)=delete;
        Reservation(Reservation&& other) noexcept : state_(std::move(other.state_)),workers_(other.workers_),bytes_(other.bytes_) {}
        Reservation& operator=(Reservation&& other) noexcept {
            if(this!=&other) { release(); state_=std::move(other.state_);workers_=other.workers_;bytes_=other.bytes_; }
            return *this;
        }
        ~Reservation() { release(); }
        std::size_t workers() const { require(); return workers_; }
        std::size_t declared_buffer_bytes() const { require(); return bytes_; }
    private:
        Reservation(std::shared_ptr<State> state,std::size_t workers,std::size_t bytes)
            : state_(std::move(state)),workers_(workers),bytes_(bytes) {}
        void require() const { if(!state_)throw std::invalid_argument("moved CPU resource reservation"); }
        void release() noexcept {
            if(state_) {
                { std::lock_guard lock(state_->mutex);state_->used_workers-=workers_;state_->used_bytes-=bytes_;++state_->epoch; }
                state_->changed.notify_all();
            }
            state_.reset();
        }
        std::shared_ptr<State> state_;
        std::size_t workers_{},bytes_{};
        friend class CpuResourceCoordinator;
    };
    CpuResourceCoordinator(const HardwareInventory& hardware,CpuResourcePolicy policy={})
        : state_(std::make_shared<State>(cpu_execution_facts(hardware),policy,admitted_cpu_workers(hardware,policy))) {}
    CpuResourceCoordinator(const CpuResourceCoordinator&)=delete;
    CpuResourceCoordinator& operator=(const CpuResourceCoordinator&)=delete;
    // Explicit sharing preserves one cooperating budget; it creates no capacity.
    CpuResourceCoordinator share() const { return CpuResourceCoordinator(state_); }
    // The parent charge survives until every delegated reservation has quiesced.
    static CpuResourceCoordinator delegate(Reservation reservation) {
        reservation.require();
        auto policy=reservation.state_->policy;
        policy.worker_limit=reservation.workers();policy.declared_buffer_byte_limit=reservation.declared_buffer_bytes();
        auto child=std::make_shared<State>(reservation.state_->facts,policy,reservation.workers());
        child->parent=std::make_shared<Reservation>(std::move(reservation));
        return CpuResourceCoordinator(std::move(child));
    }
    std::uint64_t change_epoch() const { std::lock_guard lock(state_->mutex);return state_->epoch; }
    void notify_waiters() const {
        { std::lock_guard lock(state_->mutex);++state_->epoch; }
        state_->changed.notify_all();
    }
    void wait_for_change(std::uint64_t epoch,std::stop_token stop={}) const {
        std::unique_lock lock(state_->mutex);
        state_->changed.wait(lock,stop,[&]{return state_->epoch!=epoch;});
    }
    const CpuExecutionFacts& facts() const & noexcept { return state_->facts; }
    const CpuExecutionFacts& facts() const && = delete;
    const CpuResourcePolicy& policy() const & noexcept { return state_->policy; }
    const CpuResourcePolicy& policy() const && = delete;
    std::size_t capacity() const noexcept { return state_->workers; }
    std::size_t available_workers() const { std::lock_guard lock(state_->mutex);return state_->workers-state_->used_workers; }
    std::size_t available_declared_buffer_bytes() const {
        std::lock_guard lock(state_->mutex);return state_->policy.declared_buffer_byte_limit-state_->used_bytes;
    }
    void readmit(const HardwareInventory& current) const {
        if(cpu_execution_facts(current)!=state_->facts || admitted_cpu_workers(current,state_->policy)!=state_->workers)
            throw std::invalid_argument("CPU observations changed; construct a new coordinator and calibration");
    }
    Reservation reserve(std::size_t workers,std::size_t declared_bytes=0) {
        if(!workers)throw std::invalid_argument("reservation includes at least the caller");
        if(workers>state_->workers)throw ExecutionLimitExceeded("coordinated CPU workers",workers,state_->workers);
        if(declared_bytes>state_->policy.declared_buffer_byte_limit)
            throw ExecutionLimitExceeded("declared buffer bytes",declared_bytes,state_->policy.declared_buffer_byte_limit);
        std::lock_guard lock(state_->mutex);
        if(workers>state_->workers-state_->used_workers || declared_bytes>state_->policy.declared_buffer_byte_limit-state_->used_bytes)
            throw CpuResourceBusy{};
        state_->used_workers+=workers;state_->used_bytes+=declared_bytes;++state_->epoch;
        state_->changed.notify_all();
        return Reservation(state_,workers,declared_bytes);
    }
    // Atomic phase change: a failed upgrade keeps the previous charge intact.
    // Zero workers is permitted here for an explicitly retained output owner.
    void resize(Reservation& reservation,std::size_t workers,std::size_t bytes) {
        reservation.require();
        if(reservation.state_!=state_)throw std::invalid_argument("phase reservation belongs to another coordinator");
        if(workers>state_->workers || bytes>state_->policy.declared_buffer_byte_limit)
            throw std::length_error("phase exceeds cooperating resource capacity");
        std::lock_guard lock(state_->mutex);
        const auto other_workers=state_->used_workers-reservation.workers_;
        const auto other_bytes=state_->used_bytes-reservation.bytes_;
        if(workers>state_->workers-other_workers || bytes>state_->policy.declared_buffer_byte_limit-other_bytes)throw CpuResourceBusy{};
        state_->used_workers=other_workers+workers;state_->used_bytes=other_bytes+bytes;
        reservation.workers_=workers;reservation.bytes_=bytes;++state_->epoch;state_->changed.notify_all();
    }
private:
    explicit CpuResourceCoordinator(std::shared_ptr<State> state):state_(std::move(state)){}
    std::shared_ptr<State> state_;
};
} // namespace rfc::runtime
