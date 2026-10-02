#pragma once

#include "rfc/runtime/resource_budget.hpp"
#include <chrono>

namespace rfc::runtime {
struct ResourceStage {
    std::string name;
    std::size_t workers{1},input_bytes{},retained_bytes{},scratch_bytes{},output_bytes{};
    std::size_t bytes() const {
        return machine::checked_size_add(machine::checked_size_add(input_bytes,retained_bytes),
            machine::checked_size_add(scratch_bytes,output_bytes));
    }
};
struct ResourceStageReceipt {ResourceStage stage;std::uint64_t elapsed_ns{};};
// Stage demands describe simultaneously live named containers. They do not
// claim to reserve the process heap, GMP limbs, VRAM or mathematical carriers.
class StagedResourceLease {
public:
    StagedResourceLease(CpuResourceCoordinator& resources,ResourceStage first,const ExecutionLease& lease=ExecutionLease{})
        :StagedResourceLease(resources,resources.reserve(first.workers,first.bytes()),std::move(first),lease){}
    StagedResourceLease(CpuResourceCoordinator& resources,CpuResourceCoordinator::Reservation reservation,
        ResourceStage first,const ExecutionLease& lease=ExecutionLease{})
        :resources_(resources.share()),reservation_(std::move(reservation)),current_(std::move(first)),lease_(lease),started_(Clock::now()) {
        validate(current_);lease_.check_cancelled();
        if(reservation_.workers()!=current_.workers || reservation_.declared_buffer_bytes()!=current_.bytes())
            throw std::invalid_argument("initial resource stage disagrees with assigned reservation");
    }
    StagedResourceLease(const StagedResourceLease&)=delete;
    StagedResourceLease& operator=(const StagedResourceLease&)=delete;
    void enter(ResourceStage next) {
        validate(next);lease_.check_cancelled();const auto now=Clock::now();
        // Reserve receipt capacity before changing the live resource charge.
        receipts_.reserve(machine::checked_size_add(receipts_.size(),1));
        resources_.resize(reservation_,next.workers,next.bytes());
        receipts_.push_back({std::move(current_),nanoseconds(started_,now)});current_=std::move(next);started_=now;
    }
    template<class T> std::shared_ptr<const T> finish(T result,std::size_t retained_object_bytes) {
        lease_.check_cancelled();receipts_.reserve(machine::checked_size_add(receipts_.size(),1));
        resources_.resize(reservation_,0,retained_object_bytes);
        receipts_.push_back({std::move(current_),nanoseconds(started_,Clock::now())});
        struct Owner {CpuResourceCoordinator::Reservation charge;T value;Owner(CpuResourceCoordinator::Reservation r,T v):charge(std::move(r)),value(std::move(v)){} };
        auto owner=std::make_shared<const Owner>(std::move(reservation_),std::move(result));
        return {owner,&owner->value};
    }
    const std::vector<ResourceStageReceipt>& receipts() const & {return receipts_;}
    const std::vector<ResourceStageReceipt>& receipts() const &&=delete;
private:
    using Clock=std::chrono::steady_clock;
    static std::uint64_t nanoseconds(Clock::time_point a,Clock::time_point b) {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count());
    }
    static void validate(const ResourceStage& stage) {
        if(stage.name.empty() || !stage.workers)throw std::invalid_argument("resource stage needs a name and caller worker");
        (void)stage.bytes();
    }
    CpuResourceCoordinator resources_;CpuResourceCoordinator::Reservation reservation_;
    ResourceStage current_;ExecutionLease lease_;Clock::time_point started_;
    std::vector<ResourceStageReceipt> receipts_;
};
} // namespace rfc::runtime
