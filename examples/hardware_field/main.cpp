#include "rfc/database/resource_native_field.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>

using namespace rfc;
using namespace rfc::storage;
using namespace rfc::database;
namespace {
void quoted(const std::string& value) {
    static constexpr char hex[]="0123456789abcdef";std::cout<<'"';
    for(const unsigned char c:value) {
        if(c=='"' || c=='\\')std::cout<<'\\'<<static_cast<char>(c);
        else if(c<32)std::cout<<"\\u00"<<hex[c>>4]<<hex[c&15];
        else std::cout<<static_cast<char>(c);
    }
    std::cout<<'"';
}
void optional_count(const std::optional<std::size_t>& n) {if(n)std::cout<<*n;else std::cout<<"null";}
void ids(const std::vector<std::size_t>& values) {
    std::cout<<'[';for(std::size_t i=0;i<values.size();++i){if(i)std::cout<<',';std::cout<<values[i];}std::cout<<']';
}
void inventory(const runtime::HardwareInventory& h) {
    std::cout<<"{\"record\":\"hardware\",\"platform\":";quoted(h.platform);std::cout<<",\"cpu_model\":";quoted(h.cpu_model);
    std::cout<<",\"affinity_observed\":"<<(h.affinity_observed?"true":"false")<<",\"physical_cores\":";optional_count(h.physical_cores());
    std::cout<<",\"allowed_logical_cpus\":[";
    for(std::size_t i=0;i<h.allowed_cpus.size();++i) {
        if(i)std::cout<<',';
        const auto& cpu=h.allowed_cpus[i];
        std::cout<<"{\"id\":"<<cpu.logical_id<<",\"package\":";optional_count(cpu.package_id);std::cout<<",\"core\":";optional_count(cpu.core_id);
        std::cout<<",\"instruction_flags\":[";
        for(std::size_t j=0;j<cpu.instruction_flags.size();++j){if(j)std::cout<<',';quoted(cpu.instruction_flags[j]);}
        std::cout<<"]}";
    }
    std::cout<<"],\"caches\":[";
    for(std::size_t i=0;i<h.caches.size();++i) {
        if(i)std::cout<<',';
        const auto& cache=h.caches[i];std::cout<<"{\"level\":"<<cache.level<<",\"type\":";quoted(cache.type);
        std::cout<<",\"bytes\":"<<cache.bytes<<",\"line_bytes\":"<<cache.line_bytes<<",\"shared_cpus\":";ids(cache.shared_logical_ids);std::cout<<'}';
    }
    std::cout<<"],\"stacked_3d_cache\":null,\"ram_total_bytes\":";optional_count(h.ram_total_bytes);
    std::cout<<",\"ram_available_bytes\":";optional_count(h.ram_available_bytes);std::cout<<",\"advisory_memory_headroom_bytes\":";optional_count(h.advisory_memory_headroom());
    std::cout<<",\"visible_cgroups\":[";
    for(std::size_t i=0;i<h.cgroups.size();++i) {
        if(i)std::cout<<',';
        const auto& group=h.cgroups[i];std::cout<<"{\"path\":";quoted(group.path);
        std::cout<<",\"cpu_unlimited\":"<<(group.cpu_unlimited?"true":"false")<<",\"cpu_quota_us\":";
        optional_count(group.cpu_quota?std::optional(group.cpu_quota->microseconds):std::nullopt);
        std::cout<<",\"cpu_period_us\":";optional_count(group.cpu_quota?std::optional(group.cpu_quota->period_microseconds):std::nullopt);
        std::cout<<",\"memory_max_bytes\":";optional_count(group.memory_max);std::cout<<",\"memory_current_bytes\":";optional_count(group.memory_current);std::cout<<'}';
    }
    std::cout<<"],\"pci_devices\":[";
    for(std::size_t i=0;i<h.pci_devices.size();++i) {
        if(i)std::cout<<',';
        const auto& device=h.pci_devices[i];std::cout<<"{\"address\":";quoted(device.address);
        std::cout<<",\"vendor\":"<<device.vendor<<",\"device\":"<<device.device<<",\"class_code\":";optional_count(device.class_code);std::cout<<'}';
    }
    std::cout<<"],\"visible_compute_nodes\":[";
    for(std::size_t i=0;i<h.visible_compute_nodes.size();++i){if(i)std::cout<<',';quoted(h.visible_compute_nodes[i]);}
    std::cout<<"],\"usable_gpu_count\":null,\"vram_bytes\":null,\"gpu_host_memory_coherence\":null,\"observations\":[";
    for(std::size_t i=0;i<h.observations.size();++i) {
        if(i)std::cout<<',';
        const auto& observation=h.observations[i];std::cout<<"{\"source\":";quoted(observation.source);
        std::cout<<",\"status\":";quoted(observation.status==runtime::ObservationStatus::Observed?"observed":
            observation.status==runtime::ObservationStatus::Malformed?"malformed":"unavailable");
        std::cout<<",\"detail\":";quoted(observation.detail);std::cout<<'}';
    }
    std::cout<<"]}\n";
}
TemporalEndpoint source() {
    constexpr std::size_t relations=1024,cells=256;
    core::RelationSignature signature;const auto type=signature.add_type("transfer",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<relations;++i)builder.add_grade_one(core::RelationKey{10000+i},type,{
        {10+i%cells,true,true,numeric::Rational{1},{},core::OccurrenceKey{100000+2*i}},
        {10+(i+1)%cells,true,false,numeric::Rational{1},{},core::OccurrenceKey{100001+2*i}}});
    TemporalEndpoint e(builder.freeze());
    for(std::size_t grade=0;grade<2;++grade)for(std::size_t i=0;i<(grade?relations:cells);++i) {
        const auto g=core::Grade{static_cast<std::uint16_t>(grade)};const auto key=(grade?10000u:10u)+i;
        e.carriers.push_back({key,TemporalCarrierKind::CellStalk,g,StableCellKey{g,key},900,numeric::IndexDomain::linear(1)});
        if(!grade)continue;
        TemporalTensor t;t.key=20000+i;t.role=TemporalTensorRole::Field;t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell={g,key};
        t.output=key;t.interpretation="declared rational transfer";
        t.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{static_cast<long>(i%7+1),11}}})};
        e.tensors.push_back(std::move(t));
    }
    return e;
}
}
int main(int argc,char** argv) {
    try {
        if(argc>2 || (argc==2 && std::string(argv[1])!="--inventory-only"))throw std::invalid_argument("usage: hardware_field [--inventory-only]");
        const auto hardware=runtime::detect_hardware();inventory(hardware);if(argc==2)return 0;
        const auto basis=hardware.physical_cores()?runtime::CpuWorkerBasis::ObservedPhysicalCoreCount:runtime::CpuWorkerBasis::AllowedLogicalThreads;
        runtime::CpuResourceCoordinator coordinator(hardware,{4,0,10000,basis});NativeAccessionDirectory directory(source(),700);
        NativeFieldRequest request;request.upper_grade=core::Grade{1};
        for(const auto& c:directory.pin_snapshot()->carriers)request.stalk_carriers.push_back(c.key);
        for(const auto& t:directory.pin_snapshot()->tensors) {
            NativeAccessionRequest accession;accession.kind=NativeAccessionKind::Field;accession.subject=t.key;request.fields.push_back(directory.pin(accession));
        }
        const auto analysis=NativeFieldAnalysis::inspect(directory.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{},{})),request,NativeFieldRealization::constant_fiber(900));
        const auto op=prepare_parallel_field(analysis,16);auto workspace=make_parallel_workspace(op);
        CpuFieldCalibrationPolicy policy;policy.worker_candidates={1};policy.samples=7;
        for(const auto workers:{2u,4u})if(workers<=coordinator.capacity())policy.worker_candidates.push_back(workers);
        const auto tuning=calibrate_cpu_field(op,workspace,coordinator,hardware,policy);
        for(const auto& timing:tuning.timings()) {
            std::cout<<"{\"record\":\"warm_cpu_calibration\",\"workers\":"<<timing.workers<<",\"samples_ns\":";ids(timing.nanosecond_samples);
            std::cout<<",\"median_ns\":"<<timing.median_nanoseconds<<",\"retained_occurrences\":"<<tuning.retained_occurrences()
                <<",\"arithmetic_entries\":"<<tuning.retained_entries()<<",\"output_coordinates\":"<<tuning.output_coordinates()<<"}\n";
        }
        const auto result=execute_scheduled_field(op,workspace,coordinator,runtime::detect_hardware(),tuning);
        const auto reference=execute(prepare_field_action(directory.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{},{})),request,NativeFieldRealization::constant_fiber(900)));
        if(result.field.values()!=reference.values() || result.field.carriers().size()!=reference.carriers().size())throw std::logic_error("fresh scalar comparison failed");
        std::cout<<"{\"record\":\"exact_field\",\"selected_workers\":"<<result.selected_workers<<",\"worker_capacity\":"<<coordinator.capacity()
            <<",\"completed_tiles\":"<<result.field.execution().completed_tiles<<",\"output_coordinates\":"<<result.field.values().size()
            <<",\"fresh_scalar_equal\":true,\"os_thread_pinning\":false,\"gpu_selected\":false,\"reserved_gmp_bytes\":null}\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
