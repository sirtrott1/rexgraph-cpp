#pragma once
#include "rfc/database/prepared_native_query.hpp"
#include "rfc/database/native_tensor_window.hpp"

namespace rfc::database {
// Fixed owning read jobs. Readiness edges do not rebind source publications or
// infer input identity from another job's values. Charges cover graph-retained
// result/container envelopes, separately from shared source and GMP allocation.
inline runtime::WorkJob make_query_work(runtime::WorkKey key,const PreparedNativeQuery& operation,
    std::size_t workers,runtime::WorkAvailability availability,std::vector<runtime::WorkKey> after={},
    const runtime::ExecutionHostPolicy& host={}) {
    if (!workers) throw std::invalid_argument("native query needs a caller worker");
    runtime::admit_work_availability(operation.source_namespace(),operation.source_publication()->sequence,availability);
    runtime::admit_execution(operation.requirements(),host);
    return runtime::WorkJob::make<DerivedNativeQuery>(key,{workers,operation.execution_object_bytes(),operation.result_object_bytes()},std::move(after),
        [operation,workers,availability,host](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& resources,std::stop_token stop) {
            const auto cancel=std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel]{cancel->store(true,std::memory_order_release);});
            auto reservation=resources.reserve(workers);runtime::CpuTileExecutor executor({workers,resources.capacity(),resources.policy().tile_limit});
            return operation.execute(availability,executor,runtime::ExecutionLease(host,cancel));
        });
}
inline runtime::WorkJob make_tensor_summary_work(runtime::WorkKey key,const NativeTensorWindow& window,
    runtime::WorkAvailability availability,std::vector<runtime::WorkKey> after={},const runtime::ExecutionHostPolicy& host={}) {
    runtime::admit_work_availability(window.source_namespace(),window.source_publication()->sequence,availability);
    runtime::admit_execution(window.requirements(),host);
    return runtime::WorkJob::make<NativeTensorWindowSummary>(key,{1,sizeof(NativeTensorWindowSummary)+sizeof(numeric::Rational),sizeof(NativeTensorWindowSummary)},std::move(after),
        [window,availability,host](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& resources,std::stop_token stop) {
            const auto cancel=std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel]{cancel->store(true,std::memory_order_release);});
            auto reservation=resources.reserve(1);
            return window.summarize(availability,runtime::ExecutionLease(host,cancel));
        });
}
} // namespace rfc::database
