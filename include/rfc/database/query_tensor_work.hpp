#pragma once
#include "rfc/database/prepared_native_query.hpp"
#include "rfc/database/native_tensor_window.hpp"
#include "rfc/runtime/staged_resources.hpp"

namespace rfc::database {
// Fixed owning read jobs. Readiness edges do not rebind source publications or
// infer input identity from another job's values. Escaped result copies and key
// aliases retain their container/limb charges. Shared source/preparation pins and
// allocator metadata remain separate from these per-request charges.
struct QueryWorkOwnership {
    static DerivedNativeQuery finish(DerivedNativeQuery result,runtime::StagedResourceLease& stages,std::size_t limit) {
        const auto bytes=result.retained_object_bytes();
        if(bytes>limit)throw runtime::ExecutionLimitExceeded("query retained bytes",bytes,limit);
        const auto owner=stages.finish(result.data_,bytes);
        result.data_={owner,owner->get()};return result;
    }
};
struct TensorWorkOwnership {
    static NativeTensorWindowSummary finish(NativeTensorWindowSummary result,runtime::StagedResourceLease& stages,std::size_t limit) {
        const auto bytes=result.retained_object_bytes();
        if(bytes>limit)throw runtime::ExecutionLimitExceeded("tensor retained bytes",bytes,limit);
        const auto owner=stages.finish(result.sum_,bytes);
        result.sum_={owner,owner->get()};return result;
    }
};
inline runtime::WorkJob make_query_work(runtime::WorkKey key,const PreparedNativeQuery& operation,
    std::size_t workers,runtime::WorkAvailability availability,std::vector<runtime::WorkKey> after={},
    const runtime::ExecutionHostPolicy& host={}) {
    if (!workers) throw std::invalid_argument("native query needs a caller worker");
    runtime::admit_work_availability(operation.source_namespace(),operation.source_publication()->sequence,availability);
    runtime::admit_execution(operation.requirements(),host);
    const auto bytes=operation.execution_object_bytes(),retained=operation.result_object_bytes();
    return runtime::WorkJob::make_phased<DerivedNativeQuery>(key,{workers,bytes,retained},{workers,bytes,retained},std::move(after),
        [operation,workers,availability,host,bytes,retained](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& resources,
            runtime::CpuResourceCoordinator::Reservation reservation,std::stop_token stop) {
            const auto cancel=std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel]{cancel->store(true,std::memory_order_release);});
            runtime::ExecutionLease lease(host,cancel);
            runtime::StagedResourceLease stages(resources,std::move(reservation),{"query-action",workers,0,0,bytes,0},lease);
            runtime::CpuTileExecutor executor({workers,resources.capacity(),resources.policy().tile_limit});
            return QueryWorkOwnership::finish(operation.execute(availability,executor,lease),stages,retained);
        });
}
inline runtime::WorkJob make_tensor_summary_work(runtime::WorkKey key,const NativeTensorWindow& window,
    runtime::WorkAvailability availability,std::vector<runtime::WorkKey> after={},const runtime::ExecutionHostPolicy& host={}) {
    runtime::admit_work_availability(window.source_namespace(),window.source_publication()->sequence,availability);
    runtime::admit_execution(window.requirements(),host);
    const auto retained=window.result_object_bytes();
    const auto bytes=machine::checked_size_add(retained,numeric::rational_limb_envelope(window.requirements().integer_bits));
    return runtime::WorkJob::make_phased<NativeTensorWindowSummary>(key,{1,bytes,retained},{1,bytes,retained},std::move(after),
        [window,availability,host,bytes,retained](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& resources,
            runtime::CpuResourceCoordinator::Reservation reservation,std::stop_token stop) {
            const auto cancel=std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel]{cancel->store(true,std::memory_order_release);});
            runtime::ExecutionLease lease(host,cancel);
            runtime::StagedResourceLease stages(resources,std::move(reservation),{"tensor-sum",1,0,0,bytes,0},lease);
            return TensorWorkOwnership::finish(window.summarize(availability,lease),stages,retained);
        });
}
} // namespace rfc::database
