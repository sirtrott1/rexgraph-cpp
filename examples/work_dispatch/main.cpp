#include "sample.hpp"
#include "rfc/backend/binary_work.hpp"
#ifdef RFC_WORK_VULKAN
#include "rfc/backend/vulkan_work.hpp"
#endif
#include "locality.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
using namespace rfc;using namespace rfc::runtime;using namespace rfc::backend;
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t elapsed(Clock::time_point start){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());}
void write_json_string(const std::string& text){std::cout<<'"';for(char raw:text){const auto c=static_cast<unsigned char>(raw);if(c=='"'||c=='\\')std::cout<<'\\'<<c;else if(c>=32)std::cout<<c;else std::cout<<'?';}std::cout<<'"';}
PreparedPackedBinary packed(std::size_t slots,std::size_t extra) {
    const auto schema=composite::AspectSchema::canonical();composite::AspectPlanes planes(schema,slots);
    for(std::size_t i=0;i<slots;++i){planes.set(0,i,i%5!=0);planes.set(1,i,i%3==0);}
    auto program=composite::compile_active_orientation_query(0,1,true);
    for(std::size_t i=0;i<extra;++i)program.append({composite::BinaryOpcode::Xor,2,2,1,0,0});
    return {program,planes,{2}};
}
void verify(const PackedWorkResult& actual,const std::vector<machine::PackedBits>& expected){
    if(actual.outputs.size()!=expected.size())throw std::runtime_error("output arity changed");
    for(std::size_t i=0;i<expected.size();++i)if(actual.outputs[i].words()!=expected[i].words())throw std::runtime_error("exact packed result changed");
}
void reuse_batches(CpuResourceCoordinator& resources,const database::PreparedParallelNativeField& field,
    const PreparedPackedBinary& operation,const PackedWorkCarrier& carrier,
    const std::vector<machine::PackedBits>& binary_expected,const numeric::RationalVector& field_expected,
    std::size_t relations,std::uint64_t field_prepare_ns,std::uint64_t binary_prepare_ns,bool profile
#ifdef RFC_WORK_VULKAN
    ,const VulkanBinaryDevice& device
#endif
) {
    std::vector<std::string> modes{"fresh","field_cache"};
#ifdef RFC_WORK_VULKAN
    modes.push_back("field_cache_vulkan_residency");
#endif
    for(const auto& mode:modes) {
        auto start=Clock::now();auto cache=std::make_shared<database::NativeFieldCache>();
#ifdef RFC_WORK_VULKAN
        auto gpu_cache=std::make_shared<VulkanBinaryWorkspaceCache>(device);
#endif
        const auto service_prepare_ns=elapsed(start);std::vector<std::uint64_t> samples;
        for(unsigned i=0;i<(profile?7u:3u);++i) {
            start=Clock::now();auto binary=make_cpu_binary_work({1,1},operation,carrier);
#ifdef RFC_WORK_VULKAN
            const auto policy=MixedBinaryPolicy{operation.words(),1,256,1<<26,{}, {}};
            binary=mode=="field_cache_vulkan_residency"?
                make_vulkan_binary_work({1,1},operation,carrier,gpu_cache,policy):
                make_vulkan_binary_work({1,1},operation,carrier,device,policy);
#endif
            const auto workers=resources.capacity()>1?resources.capacity()-1:1;
            auto numeric=mode=="fresh"?database::make_field_work({2,1},field,workers):
                database::make_cached_field_work({2,1},field,cache,workers,{},
                    WorkAvailability{field.report().source_namespace,field.report().sequence});
            auto batch=WorkGraph({std::move(binary),std::move(numeric)}).submit(resources);batch.wait();samples.push_back(elapsed(start));
            verify(*batch.completion({1,1}).result<PackedWorkResult>(),binary_expected);
            const auto actual=mode=="fresh"?batch.completion({2,1}).result<database::DerivedParallelNativeField>()->values():
                batch.completion({2,1}).result<database::CachedNativeField>()->values();
            if(actual!=field_expected)throw std::runtime_error("reused field output changed");
            if(resources.available_workers()!=resources.capacity())throw std::runtime_error("reused work leaked workers");
        }
        auto warm=std::vector<std::uint64_t>(samples.begin()+1,samples.end());std::sort(warm.begin(),warm.end());
        const auto facts=cache->stats();
        std::cout<<"{\"phase\":\"reuse_batch\",\"mode\":";write_json_string(mode);
        std::cout<<",\"slots\":"<<operation.planes().slots()<<",\"relations\":"<<relations
            <<",\"field_prepare_ns\":"<<field_prepare_ns<<",\"binary_prepare_ns\":"<<binary_prepare_ns
            <<",\"service_prepare_ns\":"<<service_prepare_ns<<",\"cold_batch_ns\":"<<samples.front()
            <<",\"warm_median_ns\":"<<warm[warm.size()/2]<<",\"field_compilations\":"<<facts.compiled_plans
            <<",\"field_adoptions\":"<<facts.adopted_plans<<",\"prepared_input_uses\":"<<facts.prepared_input_uses
            <<",\"field_applications\":"<<facts.applications<<",\"field_result_hits\":"<<facts.result_hits
            <<",\"cpu_scratch_creations\":"<<facts.workspace_creations<<",\"cpu_idle_slots\":"<<facts.idle_workspace_slots;
#ifdef RFC_WORK_VULKAN
        const auto gpu=gpu_cache->stats();
        std::cout<<",\"gpu_scratch_creations\":"<<gpu.creations<<",\"gpu_scratch_hits\":"<<gpu.hits
            <<",\"gpu_retained_allocation_bytes\":"<<gpu.allocation_bytes;
#endif
        std::cout<<",\"samples_ns\":[";for(std::size_t i=0;i<samples.size();++i)std::cout<<(i?",":"")<<samples[i];std::cout<<"]}\n";
    }
}
void locality_batches(CpuResourceCoordinator& resources,const database::PreparedParallelNativeField& field,
    const PreparedPackedBinary& operation,const PackedWorkCarrier& carrier,const std::vector<machine::PackedBits>& binary_expected,
    const numeric::RationalVector& field_expected,std::size_t relations,std::uint64_t source_ns,std::uint64_t binary_ns,
    std::uint64_t calibration_ns,bool profile
#ifdef RFC_WORK_VULKAN
    ,const VulkanBinaryDevice& device
#endif
) {
    std::vector<std::string> modes{"cpu_cached"};
#ifdef RFC_WORK_VULKAN
    modes.insert(modes.end(),{"gpu_cached","mixed_cached"});
#endif
    std::string selected_mode;std::uint64_t selected_total=std::numeric_limits<std::uint64_t>::max();
    for(const auto& mode:modes) {
        auto start=Clock::now();auto cache=std::make_shared<database::NativeFieldCache>();
#ifdef RFC_WORK_VULKAN
        auto gpu_cache=std::make_shared<VulkanBinaryWorkspaceCache>(device);
#endif
        const auto service_ns=elapsed(start);std::vector<std::uint64_t> times;
        for(unsigned repetition=0;repetition<(profile?7u:3u);++repetition) {
            start=Clock::now();auto binary=make_cpu_binary_work({1,1},operation,carrier);
#ifdef RFC_WORK_VULKAN
            if(mode!="cpu_cached")binary=make_vulkan_binary_work({1,1},operation,carrier,gpu_cache,
                {mode=="mixed_cached"?operation.words()/2:operation.words(),1,256,1<<26,{}, {}});
#endif
            const auto workers=resources.capacity()>1?resources.capacity()-1:1;
            auto run=WorkGraph({std::move(binary),database::make_cached_field_work({2,1},field,cache,workers,{},
                WorkAvailability{field.report().source_namespace,field.report().sequence})}).submit(resources);
            run.wait();times.push_back(elapsed(start));verify(*run.completion({1,1}).result<PackedWorkResult>(),binary_expected);
            if(run.completion({2,1}).result<database::CachedNativeField>()->values()!=field_expected)throw std::runtime_error("matched cached field changed");
        }
        const auto facts=cache->stats();auto warm=std::vector<std::uint64_t>(times.begin()+1,times.end());std::sort(warm.begin(),warm.end());
        auto total=service_ns;for(const auto t:times)total+=t;
        if(total<selected_total){selected_total=total;selected_mode=mode;}
        std::cout<<"{\"phase\":\"matched_reuse_batch\",\"mode\":";write_json_string(mode);
        std::cout<<",\"relations\":"<<relations<<",\"slots\":"<<operation.planes().slots()<<",\"source_prepare_ns\":"<<source_ns
            <<",\"binary_prepare_ns\":"<<binary_ns<<",\"field_calibration_ns\":"<<calibration_ns<<",\"service_prepare_ns\":"<<service_ns
            <<",\"cold_batch_ns\":"<<times.front()<<",\"warm_median_ns\":"<<warm[warm.size()/2]<<",\"measured_uses\":"<<times.size()
            <<",\"uses_plus_service_ns\":"<<total<<",\"field_compilations\":"<<facts.compiled_plans<<",\"field_adoptions\":"<<facts.adopted_plans
            <<",\"field_applications\":"<<facts.applications<<",\"field_result_hits\":"<<facts.result_hits;
#ifdef RFC_WORK_VULKAN
        const auto gpu=gpu_cache->stats();std::cout<<",\"gpu_scratch_creations\":"<<gpu.creations<<",\"gpu_scratch_hits\":"<<gpu.hits
            <<",\"gpu_retained_allocation_bytes\":"<<gpu.allocation_bytes;
#endif
        locality_sample::samples(times);
    }
    std::cout<<"{\"phase\":\"matched_reuse_selection\",\"relations\":"<<relations<<",\"selected_mode\":";write_json_string(selected_mode);
    std::cout<<",\"uses_plus_service_ns\":"<<selected_total<<",\"scope\":\"fixed source and full outputs, identical field cache availability, measured uses\"}\n";
    const auto prepare_start=Clock::now();std::vector<database::PreparedNativeFieldRegion> regions;
    const auto& outputs=field.analysis().binding().outputs();
    for(std::size_t first=0;first<outputs.size();first+=64) {
        std::vector<storage::StableCellKey> cells;
        for(std::size_t i=first;i<std::min(first+64,outputs.size());++i)cells.push_back(outputs[i].cell);
        regions.push_back(database::PreparedNativeFieldRegion::select(field,cells));
    }
    const auto region_prepare_ns=elapsed(prepare_start);
    for(const auto& mode:modes) {
        std::vector<std::uint64_t> times;std::size_t stage_count{},peak_jobs{},worker_bound{};
#ifdef RFC_WORK_VULKAN
        auto gpu_cache=std::make_shared<VulkanBinaryWorkspaceCache>(device);
#endif
        for(unsigned repetition=0;repetition<(profile?5u:3u);++repetition) {
            auto start=Clock::now();auto binary=make_cpu_binary_work({1,1},operation,carrier);
#ifdef RFC_WORK_VULKAN
            if(mode!="cpu_cached")binary=make_vulkan_binary_work({1,1},operation,carrier,gpu_cache,
                {mode=="mixed_cached"?operation.words()/2:operation.words(),1,256,1<<26,{}, {}});
#endif
            std::vector<WorkJob> jobs;jobs.push_back(std::move(binary));
            for(std::size_t i=0;i<regions.size();++i)jobs.push_back(database::make_staged_region_work({2+i,1},regions[i],{},
                WorkAvailability{field.report().source_namespace,field.report().sequence}));
            WorkGraphLimits limits;limits.in_flight=resources.capacity();
            auto run=WorkGraph(std::move(jobs),limits).submit(resources);run.wait();times.push_back(elapsed(start));
            verify(*run.completion({1,1}).result<PackedWorkResult>(),binary_expected);numeric::RationalVector assembled(field_expected.size());
            std::map<storage::StableCellKey,std::size_t> offsets;for(const auto& b:outputs)offsets.emplace(b.cell,b.offset);
            std::size_t stages{};
            for(std::size_t i=0;i<regions.size();++i) {
                const auto result=run.completion({2+i,1}).result<database::StagedNativeFieldResult>();stages+=result->stages.size();
                for(const auto& b:result->output->plan().outputs())for(std::size_t j=0;j<b.dimension;++j)
                    assembled[offsets.at(b.cell)+j]=result->output->values()[b.offset+j];
            }
            if(assembled!=field_expected)throw std::runtime_error("independent region reconstruction changed full field");
            stage_count=stages;const auto peaks=run.peak_jobs_workers();peak_jobs=std::max(peak_jobs,peaks.first);worker_bound=std::max(worker_bound,peaks.second);
            if(resources.available_workers()!=resources.capacity())throw std::runtime_error("staged region work leaked workers");
        }
        if(resources.available_declared_buffer_bytes()!=(std::size_t{1}<<29))throw std::runtime_error("staged region output charges leaked");
        auto sorted=times;std::sort(sorted.begin(),sorted.end());std::cout<<"{\"phase\":\"region_batch\",\"mode\":";write_json_string(mode);
        std::cout<<",\"relations\":"<<relations<<",\"regions\":"<<regions.size()<<",\"completed_stages\":"<<stage_count
            <<",\"region_prepare_ns\":"<<region_prepare_ns<<",\"median_ns\":"<<sorted[sorted.size()/2]<<",\"peak_jobs\":"<<peak_jobs
            <<",\"admitted_worker_bound\":"<<worker_bound<<",\"reconstructed_coordinates\":"<<field_expected.size();locality_sample::samples(times);
    }
}
}
int main(int argc,char**argv){try{
    bool profile=false,software=false,reuse=false,locality=false;for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--profile")profile=true;else if(arg=="--allow-software")software=true;else if(arg=="--reuse")reuse=true;else if(arg=="--locality")locality=true;else throw std::invalid_argument("usage: work_dispatch [--profile] [--reuse | --locality] [--allow-software]");}
    if(reuse && locality)throw std::invalid_argument("choose --reuse or --locality");
#ifndef RFC_WORK_VULKAN
    if(software)throw std::invalid_argument("software Vulkan requested from a CPU-only build");
#endif
    const auto hardware=detect_hardware();CpuResourceCoordinator resources(hardware,{4,std::size_t{1}<<29,1000000});const auto capacity=resources.capacity();
    std::cout<<"{\"phase\":\"inventory\",\"allowed_logical_threads\":"<<hardware.allowed_cpus.size()<<",\"admitted_workers\":"<<capacity;
#ifdef RFC_WORK_VULKAN
    const auto devices=enumerate_vulkan_devices();std::size_t selected=0;
    while(selected<devices.size() && (!devices[selected].compute || (!software && !devices[selected].physical_gpu)))++selected;
    if(selected==devices.size())throw std::runtime_error("no admitted Vulkan compute device");
    VulkanBinaryDevice device({selected,software,std::uint64_t{1}<<29});
    std::cout<<",\"device_count\":"<<devices.size()<<",\"selected_index\":"<<selected<<",\"device\":";write_json_string(device.info().name);
    std::cout<<",\"physical_gpu\":"<<(device.info().physical_gpu?"true":"false");
#endif
    std::cout<<"}\n";
    const std::vector<std::pair<std::size_t,std::size_t>> cases=profile?std::vector<std::pair<std::size_t,std::size_t>>{{65537,64},{1048577,1024},{4194305,4096}}:std::vector<std::pair<std::size_t,std::size_t>>{{65537,64}};
    for(const auto& [slots,relations]:cases){
        const std::size_t extra=slots>65537?64:0;auto start=Clock::now();const auto operation=packed(slots,extra);const auto binary_prepare_ns=elapsed(start);
        start=Clock::now();const auto analysis=sample::field_analysis(relations);const auto source_prepare_ns=elapsed(start);
        start=Clock::now();const auto field=locality?locality_sample::field_profiles(analysis,hardware,capacity,relations,source_prepare_ns,profile):database::prepare_parallel_field(analysis,128);
        const auto field_selection_ns=elapsed(start);const auto field_prepare_ns=source_prepare_ns+field_selection_ns;
        const auto carrier=PackedWorkCarrier::declare_slots(slots);auto binary_expected=operation.empty_result(1<<26);operation.cpu_words(0,operation.words(),binary_expected);
        // Per-bit oracle is independent of the packed executor, including the tail.
        for(std::size_t i=0;i<slots;++i){bool expected=i%5!=0 && i%3==0;for(std::size_t j=0;j<extra;++j)expected=expected!=(i%3==0);if(binary_expected[0].get(i)!=expected)throw std::runtime_error("Boolean oracle mismatch");}
        const auto field_expected=locality_sample::expected(analysis,relations);
        if(locality) {
            locality_batches(resources,field,operation,carrier,binary_expected,field_expected,relations,source_prepare_ns,binary_prepare_ns,field_selection_ns,profile
#ifdef RFC_WORK_VULKAN
                ,device
#endif
            );continue;
        }
        if(reuse) {
            reuse_batches(resources,field,operation,carrier,binary_expected,field_expected,relations,field_prepare_ns,binary_prepare_ns,profile
#ifdef RFC_WORK_VULKAN
                ,device
#endif
            );continue;
        }
        std::vector<std::string> modes{"cpu_serial","cpu_shared","cpu_independent"};
#ifdef RFC_WORK_VULKAN
        modes.insert(modes.end(),{"gpu_serial","heterogeneous","mixed_independent"});
#endif
        std::string selected_mode;std::uint64_t selected_ns=std::numeric_limits<std::uint64_t>::max();
        for(const auto& mode:modes){
            std::vector<std::uint64_t> samples;std::size_t peak_jobs{},peak_workers{};std::uint64_t queue_ns{},execution_ns{};
            std::size_t gpu_partition{},binary_workers=mode=="cpu_shared"?capacity:1;
            std::uint64_t upload_ns{},submit_ns{},binary_cpu_ns{},completion_ns{};
            const bool sequential=mode=="cpu_serial" || mode=="gpu_serial";
            const bool independent=mode=="cpu_independent" || mode=="heterogeneous";
            const auto field_workers=independent && capacity>1?capacity-1:(mode=="mixed_independent"?std::size_t{1}:(mode=="cpu_shared"||mode=="gpu_serial"?capacity:std::size_t{1}));
            for(unsigned repetition=0;repetition<(profile?5u:1u);++repetition){
                start=Clock::now();
                auto binary=make_cpu_binary_work({1,1},operation,carrier,mode=="cpu_shared"?capacity:1,256);
#ifdef RFC_WORK_VULKAN
                if(mode=="gpu_serial"||mode=="heterogeneous"||mode=="mixed_independent"){
                    const auto gpu_words=mode=="mixed_independent"?operation.words()/2:operation.words();
                    const auto cpu_workers=mode=="mixed_independent" && capacity>1?capacity-1:std::size_t{1};
                    binary=make_vulkan_binary_work({1,1},operation,carrier,device,{gpu_words,cpu_workers,256,1<<26,{}, {}});
                }
#endif
                WorkGraphLimits limits;limits.in_flight=sequential?1:2;
                auto batch=WorkGraph({std::move(binary),database::make_field_work({2,1},field,field_workers)},limits).submit(resources);batch.wait();samples.push_back(elapsed(start));
                const auto result=batch.completion({1,1}).result<PackedWorkResult>();verify(*result,binary_expected);
                if(result->device){const auto& d=*result->device;gpu_partition=d.gpu_words;binary_workers=d.cpu_workers;
                    upload_ns+=d.upload_ns;submit_ns+=d.submit_ns;binary_cpu_ns+=d.cpu_ns;completion_ns+=d.completion_ns;}
                if(batch.completion({2,1}).result<database::DerivedParallelNativeField>()->values()!=field_expected)throw std::runtime_error("exact field result changed");
                const auto peaks=batch.peak_jobs_workers();peak_jobs=std::max(peak_jobs,peaks.first);peak_workers=std::max(peak_workers,peaks.second);
                for(const auto key:{WorkKey{1,1},WorkKey{2,1}}){const auto receipt=batch.completion(key).receipt();queue_ns+=receipt.queue_ns;execution_ns+=receipt.execution_ns;}
                if(resources.available_workers()!=capacity || resources.available_declared_buffer_bytes()!=(std::size_t{1}<<29))throw std::runtime_error("shared resource budget leaked");
            }
            auto sorted=samples;std::sort(sorted.begin(),sorted.end());
            if(sorted[sorted.size()/2]<selected_ns){selected_ns=sorted[sorted.size()/2];selected_mode=mode;}
            std::cout<<"{\"phase\":\"complete_batch\",\"mode\":";write_json_string(mode);
            std::cout<<",\"slots\":"<<slots<<",\"relations\":"<<relations<<",\"instructions\":"<<operation.program().instruction_count()
                <<",\"field_workers\":"<<field_workers<<",\"binary_workers\":"<<binary_workers<<",\"gpu_words\":"<<gpu_partition<<",\"cpu_words\":"<<operation.words()-gpu_partition
                <<",\"binary_prepare_ns\":"<<binary_prepare_ns<<",\"field_prepare_ns\":"<<field_prepare_ns<<",\"median_ns\":"<<sorted[sorted.size()/2]
                <<",\"peak_jobs\":"<<peak_jobs<<",\"peak_workers\":"<<peak_workers<<",\"sum_queue_ns\":"<<queue_ns<<",\"sum_execution_ns\":"<<execution_ns
                <<",\"sum_upload_ns\":"<<upload_ns<<",\"sum_submit_ns\":"<<submit_ns<<",\"sum_binary_cpu_phase_ns\":"<<binary_cpu_ns<<",\"sum_completion_readback_ns\":"<<completion_ns<<",\"samples_ns\":[";
            for(std::size_t i=0;i<samples.size();++i){std::cout<<(i?",":"")<<samples[i];}
            std::cout<<"]}\n";
        }
        std::cout<<"{\"phase\":\"selection\",\"slots\":"<<slots<<",\"relations\":"<<relations<<",\"selected_mode\":";write_json_string(selected_mode);std::cout<<",\"median_ns\":"<<selected_ns<<"}\n";
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
