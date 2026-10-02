#include "rfc/backend/vulkan_work.hpp"
#include "rfc/database/field_work.hpp"
#include "../database/native_execution_fixture.hpp"
#include "../storage/native_accession_fixture.hpp"
#include <iostream>
#include <latch>
#include <barrier>
using namespace rfc;using namespace rfc::runtime;using namespace rfc::backend;
namespace {
unsigned checks{};void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"expected rejection");}
void equal(const PackedWorkResult& actual,const PreparedPackedBinary& source) {
    auto ref=source.empty_result(1<<24);source.cpu_words(0,source.words(),ref);
    check(actual.outputs.size()==ref.size(),"GPU graph output arity changed");for(std::size_t i=0;i<ref.size();++i)check(actual.outputs[i].words()==ref[i].words(),"GPU graph exact bits changed");
}
PreparedPackedBinary root(std::size_t slots) {
    auto schema=composite::AspectSchema::canonical();composite::AspectPlanes planes(schema,slots);
    for(std::size_t i=0;i<slots;++i){planes.set(0,i,i%3!=0);planes.set(1,i,i%5==0);}
    auto program=composite::compile_active_orientation_query(0,1,true);return {program,planes,{2}};
}
void pooled(const VulkanBinaryDevice& device) {
    CpuResourceCoordinator resources(detect_hardware(),{4,1<<27,100000});
    const auto available=device.available_allocation_bytes();const auto operation=root(65537);
    const auto carrier=PackedWorkCarrier::declare_slots(operation.planes().slots());
    auto cache=std::make_shared<VulkanBinaryWorkspaceCache>(device);
    const auto policy=MixedBinaryPolicy{operation.words()/2,1,64,1<<24,{}, {}};
    for(unsigned i=0;i<3;++i) {
        auto batch=WorkGraph({make_vulkan_binary_work({20,1},operation,carrier,cache,policy)}).submit(resources);
        batch.wait();equal(*batch.completion({20,1}).result<PackedWorkResult>(),operation);
    }
    check(cache->stats().creations==1 && cache->stats().hits==2 && cache->stats().idle==1,"Vulkan workspace was not reused");
    check(available-device.available_allocation_bytes()==cache->stats().allocation_bytes,"retained Vulkan bytes were mischarged");
    const auto workspace_bytes=cache->stats().allocation_bytes;
    const auto equal_bits=root(65537);
    (void)cache->execute(equal_bits,resources,policy);
    check(cache->stats().hits==2 && cache->stats().creations==2,"equal bits authenticated a different prepared owner");
    {
        std::barrier submitted(2);auto concurrent=policy;concurrent.after_submit=[&] { submitted.arrive_and_wait(); };
        auto batch=WorkGraph({make_vulkan_binary_work({21,1},operation,carrier,cache,concurrent),
                             make_vulkan_binary_work({22,1},operation,carrier,cache,concurrent)}).submit(resources);
        batch.wait();equal(*batch.completion({21,1}).result<PackedWorkResult>(),operation);
        equal(*batch.completion({22,1}).result<PackedWorkResult>(),operation);
        check(cache->stats().active==0 && cache->stats().idle==2,"concurrent Vulkan loans overlapped one scratch owner");
    }
    cache->clear();check(device.available_allocation_bytes()==available,"Vulkan clear did not release residency");
    VulkanBinaryWorkspaceCache exact(device,{1,workspace_bytes});
    (void)exact.execute(operation,resources,policy);
    check(exact.stats().allocation_bytes==workspace_bytes,"exact Vulkan pool byte boundary rejected");exact.clear();
    VulkanBinaryWorkspaceCache short_pool(device,{2,workspace_bytes-1});
    rejects([&] { (void)short_pool.execute(operation,resources,policy); });
    check(short_pool.stats().allocation_bytes==0 && device.available_allocation_bytes()==available,"partial pool allocation rollback leaked");
    auto single=std::make_shared<VulkanBinaryWorkspaceCache>(device,VulkanWorkspaceCacheLimits{1,1<<26});
    {
        std::latch submitted(1),finish(1);auto held=policy;
        held.after_submit=[&] { submitted.count_down();finish.wait(); };
        auto batch=WorkGraph({make_vulkan_binary_work({23,1},operation,carrier,single,held)}).submit(resources);
        submitted.wait();
        auto rejected=WorkGraph({make_vulkan_binary_work({24,1},operation,carrier,single,policy)}).submit(resources);
        rejected.wait();check(rejected.completion({24,1}).receipt().status==WorkStatus::Failed,"Vulkan active loan limit ignored");
        single->clear();finish.count_down();batch.wait();equal(*batch.completion({23,1}).result<PackedWorkResult>(),operation);
        check(single->stats().active==0 && single->stats().idle==0 && single->stats().allocation_bytes==0,"clear revived an old active loan");
    }
    {
        std::latch submitted(1),finish(1);auto held=policy;
        held.after_submit=[&] { submitted.count_down();finish.wait(); };
        auto batch=WorkGraph({make_vulkan_binary_work({25,1},operation,carrier,single,held)}).submit(resources);
        submitted.wait();batch.completion({25,1}).cancel();finish.count_down();batch.wait();
        check(batch.completion({25,1}).receipt().status==WorkStatus::Cancelled,"pooled submitted cancellation failed");
        check(single->stats().active==0 && single->stats().idle==0 && single->stats().allocation_bytes==0,"cancelled Vulkan loan retained resources");
    }
    auto tiny=std::make_shared<VulkanBinaryWorkspaceCache>(device,VulkanWorkspaceCacheLimits{1,1});
    rejects([&] { (void)tiny->execute(operation,resources,policy); });
    check(tiny->stats().allocation_bytes==0 && tiny->stats().active==0,"pool allocation rejection leaked");
    auto failed=policy;failed.after_submit=[] { throw std::runtime_error("after submitted pool failure"); };
    rejects([&] { (void)single->execute(operation,resources,failed); });
    check(single->stats().idle==0 && single->stats().active==0,"failed fence-joined loan was cached");
    check(device.available_allocation_bytes()==available && resources.available_workers()==resources.capacity(),"pooled Vulkan cleanup leaked");
}
void run(const VulkanBinaryDevice& device,bool software,std::size_t selected) {
    CpuResourceCoordinator resources(detect_hardware(),{4,1<<27,100000});const auto capacity=resources.capacity();
    if(capacity<2)throw std::runtime_error("Vulkan work qualification requires two admitted caller workers");
    const auto operation=root(65537);const auto carrier=PackedWorkCarrier::declare_slots(operation.planes().slots());
    const auto available=device.available_allocation_bytes();
    std::latch gpu_submitted(1),cpu_computed(1);
    using namespace native_execution_fixture;NativeAccessionDirectory directory(endpoint(),77);
    const auto field=database::prepare_parallel_field(database::NativeFieldAnalysis::inspect(anchor(directory),request(directory)),1);
    auto gpu_policy=MixedBinaryPolicy{operation.words(),1,64,1<<24,{},[&]{gpu_submitted.count_down();cpu_computed.wait();}};
    auto cpu_hook=[&](auto stage,auto,auto){if(stage==database::ParallelFieldStage::BeforeResult){gpu_submitted.wait();cpu_computed.count_down();}};
    {
        WorkGraph graph({make_vulkan_binary_work({1,1},operation,carrier,device,gpu_policy),
            database::make_field_work({2,1},field,capacity-1,{},WorkAvailability{77,field.report().sequence},{},cpu_hook)});
        auto batch=graph.submit(resources);batch.wait();const auto output=batch.completion({1,1}).result<PackedWorkResult>();equal(*output,operation);
        check(output->device && output->device->physical_gpu==device.info().physical_gpu,"GPU graph route metadata changed");
        check(output->device->selected_index==selected,"selected device route lost");
        check(batch.peak_jobs_workers()==std::pair<std::size_t,std::size_t>{2,capacity},"independent GPU/field jobs failed to overlap");
        const auto numeric=batch.completion({2,1}).result<database::DerivedParallelNativeField>();CpuTileExecutor serial;auto ws=database::make_parallel_workspace(field);
        check(numeric->values()==database::execute_parallel(field,ws,serial).values(),"concurrent rational field changed");
    }
    check(resources.available_workers()==capacity && device.available_allocation_bytes()==available,"independent graph resource leak");
    {
        std::barrier submitted_together(2);
        const auto policy=MixedBinaryPolicy{operation.words(),1,64,1<<24,{},[&]{submitted_together.arrive_and_wait();}};
        auto batch=WorkGraph({make_vulkan_binary_work({14,1},operation,carrier,device,policy),
            make_vulkan_binary_work({15,1},operation,carrier,device,policy)}).submit(resources);
        batch.wait();equal(*batch.completion({14,1}).result<PackedWorkResult>(),operation);
        equal(*batch.completion({15,1}).result<PackedWorkResult>(),operation);
        check(batch.peak_jobs_workers()==std::pair<std::size_t,std::size_t>{2,2},"shared device submissions failed to overlap");
    }
    check(device.available_allocation_bytes()==available,"shared device workspaces leaked allocations");
    composite::AspectSchema schema;schema.add("previous");composite::BinaryProgram flip(2);flip.append({composite::BinaryOpcode::LoadAspect,0,0,0,0,0});flip.append({composite::BinaryOpcode::Not,1,0,0,0,0});
    PackedWorkRecipe recipe(carrier,flip,schema,{{{3,1},0}},{1});
    const auto devices=enumerate_vulkan_devices();auto second_index=selected;
    for(std::size_t i=0;i<devices.size();++i)if(i!=selected && devices[i].compute && devices[i].physical_gpu){second_index=i;break;}
    VulkanBinaryDevice second({second_index,software,1<<27});
    {
        WorkGraph graph({make_vulkan_binary_work({3,1},operation,carrier,device,{operation.words(),1,64,1<<24,{}, {}}),
            make_vulkan_binary_work({4,1},recipe,second,{operation.words()/2,1,64,1<<24,{}, {}})});
        auto batch=graph.submit(resources);batch.wait();auto output=batch.completion({4,1}).result<PackedWorkResult>();
        auto ref=operation.empty_result(1<<24);operation.cpu_words(0,operation.words(),ref);
        for(std::size_t i=0;i<operation.planes().slots();++i)check(output->outputs[0].get(i)==!ref[0].get(i),"dependent device/host transfer changed bits");
        check(output->predecessors.size()==1 && output->input_bindings[0].producer==WorkKey{3,1},"dependent operation history lost");
        check(output->device->selected_index==second_index,"second context route changed");
    }
    check(second.available_allocation_bytes()==(1<<27) && device.available_allocation_bytes()==available,"dependent context allocations leaked");
    VulkanBinaryDevice tiny({selected,software,1});
    {
        WorkGraph graph({make_vulkan_binary_work({5,1},operation,carrier,tiny,{operation.words(),1,64,1<<24,{}, {}}),
            make_cpu_binary_work({6,1},operation,carrier,1,64,{{5,1}}),make_cpu_binary_work({7,1},operation,carrier)});
        auto batch=graph.submit(resources);batch.wait();check(batch.completion({5,1}).receipt().status==WorkStatus::Failed,"GPU allocation failure lost");
        check(batch.completion({6,1}).receipt().status==WorkStatus::Blocked,"failed GPU descendant ran");equal(*batch.completion({7,1}).result<PackedWorkResult>(),operation);
    }
    check(tiny.available_allocation_bytes()==1 && resources.available_workers()==capacity,"failed GPU graph leaked resources");
    std::latch submitted(1),finish(1);
    {
        WorkGraph graph({make_vulkan_binary_work({8,1},operation,carrier,device,{operation.words()/2,1,64,1<<24,{},[&]{submitted.count_down();finish.wait();}})});
        auto batch=graph.submit(resources);submitted.wait();batch.completion({8,1}).cancel();finish.count_down();batch.wait();
        check(batch.completion({8,1}).receipt().status==WorkStatus::Cancelled,"in-flight GPU cancellation failed");rejects([&]{(void)batch.completion({8,1}).result<PackedWorkResult>();});
    }
    check(resources.available_workers()==capacity && device.available_allocation_bytes()==available,"cancelled GPU graph leaked resources");
    {
        NativeAccessionRequest request;request.kind=NativeAccessionKind::SourceAspects;request.subject=400;request.occurrence={{core::Grade{1},core::OccurrenceKey{10000}}};
        auto endpoint=native_accession_fixture::endpoint(130);endpoint.sequence=10;
        NativeAccessionDirectory source(std::move(endpoint),800);PreparedNativeVulkanBinary native(source.pin(request),401);
        const auto c=PackedWorkCarrier::declare_slots(native.packed().planes().slots());const auto sequence=native.publication().endpoint()->sequence;
        auto native_cache=std::make_shared<VulkanBinaryWorkspaceCache>(device);
        auto job=make_native_vulkan_work({9,1},native,c,native_cache,{native.packed().words(),1,64,1<<24,{}, {}},WorkAvailability{800,sequence});
        rejects([&]{(void)make_native_vulkan_work({10,1},native,c,device,{native.packed().words(),1,64,1<<24,{}, {}},WorkAvailability{801,sequence});});
        auto batch=WorkGraph({job}).submit(resources);auto result=batch.completion({9,1}).result<PackedWorkResult>();batch.wait();
        check(result->outputs[0].words()==native.publication().execute_source_program(401).words(),"native work declaration changed");
        check(result->sources.size()==1 && result->sources[0].publication.as<PinnedNativeAccession>()->source_namespace()==800,"native source publication lost");
        composite::AspectSchema one;one.add("previous");
        PackedWorkRecipe next(c,flip,one,{{{9,1},0}},{1});
        auto future=WorkGraph({job,make_vulkan_binary_work({11,1},next,native_cache,{native.packed().words(),1,64,1<<24,{},{}},WorkAvailability{800,sequence-1})}).submit(resources);
        future.wait();check(future.completion({11,1}).receipt().status==WorkStatus::Failed,"future source leaked through predecessor output");
        auto admitted=WorkGraph({job,make_cpu_binary_work({13,1},next,1,64,1<<24,WorkAvailability{800,sequence})}).submit(resources);
        admitted.wait();auto accepted=admitted.completion({13,1}).result<PackedWorkResult>();
        for(std::size_t i=0;i<c.slots();++i)check(accepted->outputs[0].get(i)==!result->outputs[0].get(i),"available predecessor rejected or changed");
        auto external=std::stop_source{};external.request_stop();
        auto cancelled=WorkGraph({make_vulkan_binary_work({12,1},operation,carrier,device,{operation.words(),1,64,1<<24,external.get_token(),{}})}).submit(resources);
        cancelled.wait();check(cancelled.completion({12,1}).receipt().status==WorkStatus::Cancelled,"external cancellation not classified");
    }
}
}
int main(int argc,char**argv){try{bool software=argc==2 && std::string(argv[1])=="--allow-software";if(argc>1 && !software)throw std::invalid_argument("unknown work qualifier option");
    const auto devices=enumerate_vulkan_devices();std::size_t selected=0;if(!software)while(selected<devices.size() && (!devices[selected].physical_gpu || !devices[selected].compute))++selected;
    if(selected==devices.size())throw std::runtime_error("no physical compute GPU for work qualification");
    VulkanBinaryDevice device({selected,software,1<<28});run(device,software,selected);pooled(device);
    std::size_t physical{};for(const auto& d:devices)if(d.physical_gpu && d.compute)++physical;
    std::cout<<checks<<" Vulkan work checks passed; device="<<device.info().name<<"; physical_gpu="<<device.info().physical_gpu<<"; physical_device_count="<<physical<<"\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
