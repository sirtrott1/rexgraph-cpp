#pragma once

#include "rfc/database/parallel_native_field.hpp"
#include "rfc/database/native_field_cache.hpp"
#include "rfc/runtime/work_graph.hpp"

namespace rfc::database {
inline runtime::WorkJob make_cached_field_work(runtime::WorkKey key, const PreparedParallelNativeField& operation,
    std::shared_ptr<NativeFieldCache> cache, std::size_t workers,
    std::vector<runtime::WorkKey> after = {}, std::optional<runtime::WorkAvailability> cutoff = {},
    const runtime::ExecutionHostPolicy& host = {}, NativeFieldCacheHook hook = {}) {
    if (!cache || !workers) throw std::invalid_argument("cached prepared field work needs a service and caller worker");
    const auto report = native_field_cache_detail::parallel_report(operation.analysis(),operation.ranges().rows_per_tile(),
        operation.ranges().tiles().size());
    runtime::admit_execution(report.requirements,host);
    runtime::admit_work_availability(report.source_namespace,report.sequence,cutoff);
    const auto buffers = machine::checked_size_mul(report.requirements.rational_slots,sizeof(numeric::Rational));
    const auto result_bytes = machine::checked_size_mul(operation.analysis().layout().target_coordinates,sizeof(numeric::Rational));
    return runtime::WorkJob::make<CachedNativeField>(key,{workers,buffers,result_bytes},std::move(after),
        [operation,cache=std::move(cache),workers,host,hook=std::move(hook)](
            const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto cancel = std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel] { cancel->store(true,std::memory_order_release); });
            runtime::ExecutionLease lease(host,cancel); auto reservation = assigned.reserve(workers);
            runtime::CpuTileExecutor executor({workers,assigned.capacity(),assigned.policy().tile_limit});
            return cache->evaluate_parallel(operation,executor,lease,hook);
        });
}
// The service and authentic analysis are owned by the job. Each request admits
// its cutoff and host requirements even when numeric values can be shared.
inline runtime::WorkJob make_cached_field_work(runtime::WorkKey key, const NativeFieldAnalysis& analysis,
    std::shared_ptr<NativeFieldCache> cache, std::size_t workers, std::size_t rows_per_tile = 128,
    std::vector<runtime::WorkKey> after = {}, std::optional<runtime::WorkAvailability> cutoff = {},
    const runtime::ExecutionHostPolicy& host = {}, NativeFieldCacheHook hook = {}) {
    if (!cache || !workers) throw std::invalid_argument("cached field work needs a service and caller worker");
    const auto report = native_field_cache_detail::parallel_report(analysis,rows_per_tile);
    runtime::admit_execution(report.requirements,host);
    runtime::admit_work_availability(report.source_namespace,report.sequence,cutoff);
    const auto buffers = machine::checked_size_mul(report.requirements.rational_slots,sizeof(numeric::Rational));
    const auto result_bytes = machine::checked_size_mul(analysis.layout().target_coordinates,sizeof(numeric::Rational));
    return runtime::WorkJob::make<CachedNativeField>(key,{workers,buffers,result_bytes},std::move(after),
        [analysis,cache=std::move(cache),workers,rows_per_tile,host,hook=std::move(hook)](
            const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto cancel = std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel] { cancel->store(true,std::memory_order_release); });
            runtime::ExecutionLease lease(host,cancel); auto reservation = assigned.reserve(workers);
            runtime::CpuTileExecutor executor({workers,assigned.capacity(),assigned.policy().tile_limit});
            return cache->evaluate_parallel(analysis,executor,rows_per_tile,lease,hook);
        });
}
// A native action is bound and authorized before submission. Readiness edges do
// not substitute predecessor values for its declared native field inputs.
inline runtime::WorkJob make_field_work(runtime::WorkKey key,const PreparedParallelNativeField& operation,
    std::size_t workers,std::vector<runtime::WorkKey> after={},
    std::optional<runtime::WorkAvailability> cutoff={},const runtime::ExecutionHostPolicy& host={},
    ParallelFieldHook hook={}) {
    runtime::admit_execution(operation.report().requirements,host);
    runtime::admit_work_availability(operation.report().source_namespace,operation.report().sequence,cutoff);
    if(!workers)throw std::invalid_argument("field work needs a caller worker");
    const auto result_bytes=machine::checked_size_mul(operation.analysis().layout().target_coordinates,sizeof(numeric::Rational));
    // Container accounting is explicit; GMP allocations and source retention are
    // governed by the existing exact growth/slot limits, not a process heap quota.
    const auto buffers=machine::checked_size_mul(operation.report().requirements.rational_slots,sizeof(numeric::Rational));
    return runtime::WorkJob::make<DerivedParallelNativeField>(key,{workers,buffers,result_bytes},std::move(after),
        [operation,workers,host,hook=std::move(hook)](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            const auto cancel=std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel]{cancel->store(true,std::memory_order_release);});
            runtime::ExecutionLease lease(host,cancel);auto workspace=make_parallel_workspace(operation,lease);
            auto reservation=assigned.reserve(workers);
            runtime::CpuTileExecutor executor({workers,assigned.capacity(),assigned.policy().tile_limit});
            return execute_parallel(operation,workspace,executor,lease,hook);
        });
}
} // namespace rfc::database
