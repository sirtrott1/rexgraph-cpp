#pragma once

#include "rfc/database/parallel_native_field.hpp"
#include "rfc/runtime/resource_budget.hpp"
#include <chrono>

namespace rfc::database {
struct CpuFieldTiming {
    std::size_t workers{};
    std::vector<std::size_t> nanosecond_samples;
    std::size_t median_nanoseconds{};
};
struct CpuFieldCalibrationPolicy {
    std::vector<std::size_t> worker_candidates{1};
    std::size_t samples{3}, warmups{1}, call_limit{64};
};
class CpuFieldCalibration {
    struct Data {
        PreparedParallelNativeField owner;
        runtime::CpuExecutionFacts facts;
        std::vector<CpuFieldTiming> timings;
        Data(PreparedParallelNativeField o,runtime::CpuExecutionFacts f,std::vector<CpuFieldTiming> t)
            : owner(std::move(o)),facts(std::move(f)),timings(std::move(t)) {}
    };
public:
    const std::vector<CpuFieldTiming>& timings() const & { return data().timings; }
    const std::vector<CpuFieldTiming>& timings() const && = delete;
    const runtime::CpuExecutionFacts& hardware_facts() const & { return data().facts; }
    const runtime::CpuExecutionFacts& hardware_facts() const && = delete;
    std::size_t retained_entries() const { return data().owner.ranges().action().entries().size(); }
    std::size_t output_coordinates() const { return data().owner.analysis().layout().target_coordinates; }
    std::size_t retained_occurrences() const { return data().owner.analysis().layout().retained_occurrences; }
    void admit(const PreparedParallelNativeField& operation,const runtime::CpuResourceCoordinator& coordinator) const {
        if(&operation.ranges().action()!=&data().owner.ranges().action())
            throw std::invalid_argument("calibration belongs to another prepared field owner");
        if(coordinator.facts()!=data().facts)throw std::invalid_argument("calibration CPU observations changed");
    }
private:
    CpuFieldCalibration(PreparedParallelNativeField owner,runtime::CpuExecutionFacts facts,std::vector<CpuFieldTiming> timings)
        : data_(std::make_shared<const Data>(std::move(owner),std::move(facts),std::move(timings))) {}
    const Data& data() const { if(!data_)throw std::invalid_argument("moved CPU field calibration");return *data_; }
    std::shared_ptr<const Data> data_;
    friend CpuFieldCalibration calibrate_cpu_field(const PreparedParallelNativeField&,ParallelNativeFieldWorkspace&,
        runtime::CpuResourceCoordinator&,const runtime::HardwareInventory&,const CpuFieldCalibrationPolicy&,const runtime::ExecutionLease&);
};
namespace resource_field_detail {
inline DerivedParallelNativeField run(const PreparedParallelNativeField& operation,ParallelNativeFieldWorkspace& workspace,
    runtime::CpuResourceCoordinator& coordinator,std::size_t workers,const runtime::ExecutionLease& lease,const ParallelFieldHook& hook={}) {
    lease.check_cancelled();
    auto reservation=coordinator.reserve(workers);
    runtime::CpuTileExecutor executor({reservation.workers(),coordinator.capacity(),coordinator.policy().tile_limit});
    return execute_parallel(operation,workspace,executor,lease,hook);
}
} // namespace resource_field_detail
// Measures complete warm owning calls: reservation, executor construction,
// admission, compute, joins and result allocation. Preparation, independent
// comparison, output destruction and inventory refresh are excluded.
inline CpuFieldCalibration calibrate_cpu_field(const PreparedParallelNativeField& operation,ParallelNativeFieldWorkspace& workspace,
    runtime::CpuResourceCoordinator& coordinator,const runtime::HardwareInventory& current,
    const CpuFieldCalibrationPolicy& policy={},const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
    coordinator.readmit(current);lease.check_cancelled();
    runtime::admit_execution(operation.report().requirements,lease.policy());
    if(!policy.samples || policy.samples>31 || policy.warmups>31 || policy.worker_candidates.empty() ||
        policy.worker_candidates.size()>policy.call_limit)
        throw std::invalid_argument("CPU calibration needs bounded samples and candidates");
    std::set<std::size_t> candidates;
    for(const auto workers:policy.worker_candidates) {
        if(!workers || workers>coordinator.capacity())throw std::invalid_argument("CPU calibration worker candidate exceeds admitted capacity");
        candidates.insert(std::max<std::size_t>(1,std::min(workers,operation.ranges().tiles().size())));
    }
    const auto calls=machine::checked_size_add(1,machine::checked_size_mul(candidates.size(),machine::checked_size_add(policy.samples,policy.warmups)));
    if(calls>policy.call_limit)throw runtime::ExecutionLimitExceeded("CPU calibration calls",calls,policy.call_limit);
    const auto reference=resource_field_detail::run(operation,workspace,coordinator,1,lease);
    std::vector<CpuFieldTiming> timings;
    for(const auto workers:candidates) {
        for(std::size_t sample=0;sample<policy.warmups;++sample) {
            const auto result=resource_field_detail::run(operation,workspace,coordinator,workers,lease);
            if(result.values()!=reference.values())throw std::logic_error("CPU warmup changed exact output");
        }
        CpuFieldTiming timing;timing.workers=workers;
        for(std::size_t sample=0;sample<policy.samples;++sample) {
            const auto start=std::chrono::steady_clock::now();
            const auto result=resource_field_detail::run(operation,workspace,coordinator,workers,lease);
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
            if(elapsed<0)throw std::runtime_error("CPU calibration clock moved backwards");
            timing.nanosecond_samples.push_back(machine::checked_size(static_cast<std::uint64_t>(elapsed)));
            if(result.values()!=reference.values())throw std::logic_error("CPU calibration changed exact output");
        }
        auto sorted=timing.nanosecond_samples;std::sort(sorted.begin(),sorted.end());timing.median_nanoseconds=sorted[sorted.size()/2];
        timings.push_back(std::move(timing));
    }
    lease.check_cancelled();
    return CpuFieldCalibration(operation,coordinator.facts(),std::move(timings));
}
struct ScheduledNativeFieldResult {
    DerivedParallelNativeField field;
    std::size_t selected_workers{}, calibrated_median_nanoseconds{}, calibration_samples{};
    CpuFieldCalibration calibration;
    runtime::CpuResourcePolicy resource_policy;
};
inline ScheduledNativeFieldResult execute_scheduled_field(const PreparedParallelNativeField& operation,
    ParallelNativeFieldWorkspace& workspace,runtime::CpuResourceCoordinator& coordinator,
    const runtime::HardwareInventory& current,const CpuFieldCalibration& calibration,
    const runtime::ExecutionLease& lease=runtime::ExecutionLease{},const ParallelFieldHook& hook={}) {
    coordinator.readmit(current);calibration.admit(operation,coordinator);lease.check_cancelled();
    runtime::admit_execution(operation.report().requirements,lease.policy());
    const auto available=coordinator.available_workers();
    const CpuFieldTiming* selected{};
    for(const auto& timing:calibration.timings())if(timing.workers<=available &&
        (!selected || std::pair{timing.median_nanoseconds,timing.workers}<std::pair{selected->median_nanoseconds,selected->workers}))selected=&timing;
    if(!selected)throw runtime::CpuResourceBusy{};
    // A race for reservations rejects cleanly. It does not silently choose an
    // unmeasured worker count, wait indefinitely or change arithmetic domains.
    auto field=resource_field_detail::run(operation,workspace,coordinator,selected->workers,lease,hook);
    return {std::move(field),selected->workers,selected->median_nanoseconds,selected->nanosecond_samples.size(),calibration,coordinator.policy()};
}
} // namespace rfc::database
