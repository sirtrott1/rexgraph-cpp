#pragma once

#include "rfc/database/native_field_region.hpp"
#include "rfc/runtime/staged_resources.hpp"

namespace rfc::database {
struct StagedNativeFieldResult {
    std::shared_ptr<const NativeFieldRegionResult> output;
    std::vector<runtime::ResourceStageReceipt> stages;
    static std::shared_ptr<const NativeFieldRegionResult> finish_output(NativeFieldRegionResult result,
        runtime::StagedResourceLease& phases) {
        const auto bytes=machine::checked_size_mul(result.values_->size(),sizeof(numeric::Rational));
        // The numerical payload owns the charge, so copied results and extracted
        // values_pin() handles retain it independently of this outer wrapper.
        const auto owner=phases.finish(result.values_,bytes);
        result.values_={owner,owner->get()};
        return std::make_shared<const NativeFieldRegionResult>(std::move(result));
    }
};
// Shared immutable preparation/source pins are held by the declared operation.
// Phase charges below cover per-request Rational scratch/output objects only.
inline runtime::WorkJob make_staged_region_work(runtime::WorkKey key,const PreparedNativeFieldRegion& region,
    std::vector<runtime::WorkKey> after={},std::optional<runtime::WorkAvailability> cutoff={},
    const runtime::ExecutionHostPolicy& host={}) {
    runtime::admit_execution(region.operation().report().requirements,host);
    runtime::admit_work_availability(region.operation().report().source_namespace,region.operation().report().sequence,cutoff);
    const auto bytes=machine::checked_size_mul(region.output_dimension(),sizeof(numeric::Rational));
    const auto peak=machine::checked_size_add(bytes,sizeof(numeric::Rational));
    return runtime::WorkJob::make_phased<StagedNativeFieldResult>(key,{1,0,bytes},{1,peak,bytes},std::move(after),
        [region,cutoff,host,bytes](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& resources,
            runtime::CpuResourceCoordinator::Reservation reservation,std::stop_token stop) {
            auto cancel=std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel]{cancel->store(true,std::memory_order_release);});
            runtime::ExecutionLease lease(host,cancel);
            runtime::StagedResourceLease phases(resources,std::move(reservation),{"resolve-input",1,0,0,0,0},lease);
            lease.check_cancelled();
            phases.enter({"boundary-action",1,0,0,sizeof(numeric::Rational),bytes});
            auto result=region.execute(lease,cutoff);
            phases.enter({"publish-view",1,0,bytes,0,0});
            auto output=StagedNativeFieldResult::finish_output(std::move(result),phases);
            return StagedNativeFieldResult{std::move(output),phases.receipts()};
        });
}
} // namespace rfc::database
