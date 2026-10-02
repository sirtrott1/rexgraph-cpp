#include "rfc/backend/native_vulkan_binary.hpp"
#include "rfc/backend/mixed_binary_calibration.hpp"
#include "../storage/native_accession_fixture.hpp"
#include <iostream>
#include <random>
#include <latch>
using namespace rfc;using namespace rfc::backend;using namespace rfc::composite;
namespace {
std::atomic_size_t checks{};
void check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"expected rejection");}
void equal(const std::vector<machine::PackedBits>& a,const std::vector<machine::PackedBits>& b) {
    check(a.size()==b.size(),"output count");for(std::size_t i=0;i<a.size();++i)check(a[i].size()==b[i].size() && a[i].words()==b[i].words(),"exact output mismatch");
}
void numbers(const std::vector<machine::PackedBits>& planes) {
    std::cout<<'[';for(std::size_t a=0;a<planes.size();++a){if(a)std::cout<<',';std::cout<<'[';
        for(std::size_t w=0;w<planes[a].word_count();++w){if(w)std::cout<<',';std::cout<<planes[a].word(w);}std::cout<<']';}std::cout<<']';
}
void emit(const PreparedPackedBinary& op,const std::vector<machine::PackedBits>& output) {
    std::cout<<"{\"slots\":"<<op.planes().slots()<<",\"registers\":"<<op.program().register_count()<<",\"input\":";
    std::vector<machine::PackedBits> planes;for(std::size_t a=0;a<op.planes().aspect_count();++a)planes.push_back(op.planes().plane(static_cast<AspectId>(a)));
    numbers(planes);std::cout<<",\"instructions\":[";bool first=true;
    for(const auto& i:op.program().instructions()){if(!first)std::cout<<',';first=false;std::cout<<'['<<static_cast<unsigned>(i.opcode)<<','<<i.dst<<','<<i.a<<','<<i.b<<','<<i.c<<','<<i.aspect<<']';}
    std::cout<<"],\"outputs\":[";for(std::size_t i=0;i<op.outputs().size();++i){if(i)std::cout<<',';std::cout<<op.outputs()[i];}
    std::cout<<"],\"result\":";numbers(output);std::cout<<"}\n";
}
PreparedPackedBinary prepare(std::size_t slots,std::mt19937_64& rng) {
    AspectSchema schema;for(auto name:{"a","b","c"})schema.add(name);AspectPlanes planes(schema,slots);
    for(AspectId a=0;a<3;++a)for(std::size_t w=0;w<planes.plane(a).word_count();++w)planes.plane(a).assign_word(w,rng());
    BinaryProgram p(8);
    for(unsigned i=0;i<70;++i)p.append({static_cast<BinaryOpcode>(i<9?i:rng()%9),static_cast<std::uint16_t>(rng()%7),
        static_cast<std::uint16_t>(rng()%8),static_cast<std::uint16_t>(rng()%8),static_cast<std::uint16_t>(rng()%8),static_cast<AspectId>(rng()%3)});
    return {p,planes,{0,1,2,3,4,5,6,7,0}};
}
void stress(VulkanBinaryDevice& device,runtime::CpuResourceCoordinator& resources,std::size_t selected) {
    std::mt19937_64 rng(1337);auto op=prepare(8193,rng);auto expected=op.empty_result(1<<20);op.cpu_words(0,op.words(),expected);
    const auto available=device.available_allocation_bytes();
    std::uint64_t allocation_charge{};
    {
        VulkanBinaryWorkspace ws(device,op);const auto occupied=device.available_allocation_bytes();check(occupied<available,"GPU allocation uncharged");
        allocation_charge=available-occupied;
        MixedBinaryPolicy policy{op.words()/2,resources.capacity(),1,1<<20,{}, {}};
        policy.after_submit=[&]{rejects([&]{(void)ws.execute(op,resources,policy);});};
        equal(ws.execute(op,resources,policy).outputs,expected);check(resources.available_workers()==resources.capacity(),"CPU reservation leaked");
        policy.after_submit=[] {throw std::runtime_error("injected post-submit failure");};rejects([&]{(void)ws.execute(op,resources,policy);});
        policy.after_submit={};equal(ws.execute(op,resources,policy).outputs,expected);
        std::stop_source stop;policy.stop=stop.get_token();policy.after_submit=[&]{stop.request_stop();};rejects([&]{(void)ws.execute(op,resources,policy);});
        policy.stop={};policy.after_submit={};equal(ws.execute(op,resources,policy).outputs,expected);
        policy.gpu_words=op.words()+1;rejects([&]{(void)ws.execute(op,resources,policy);});policy.gpu_words=op.words()/2;
        policy.cpu_workers=resources.capacity()+1;rejects([&]{(void)ws.execute(op,resources,policy);});policy.cpu_workers=resources.capacity();
        policy.result_byte_limit=0;rejects([&]{(void)ws.execute(op,resources,policy);});policy.result_byte_limit=1<<20;
        auto other=prepare(8193,rng);rejects([&]{(void)ws.execute(other,resources,policy);});
        auto replaceable=op;policy.after_submit=[&]{replaceable=other;policy.gpu_words=0;};
        equal(ws.execute(replaceable,resources,policy).outputs,expected);policy.after_submit={};policy.gpu_words=op.words()/2;
        VulkanBinaryWorkspace second(device,op);
        std::latch submitted(1),release(1);std::exception_ptr error;
        auto first=policy;first.cpu_workers=1;first.after_submit=[&]{submitted.count_down();release.wait();};
        std::jthread running([&]{try{equal(ws.execute(op,resources,first).outputs,expected);}catch(...){error=std::current_exception();}});
        submitted.wait();auto parallel=policy;parallel.cpu_workers=1;
        try {auto result=second.execute(op,resources,parallel);release.count_down();running.join();if(error)std::rethrow_exception(error);equal(result.outputs,expected);}
        catch(...) {release.count_down();if(running.joinable())running.join();throw;}
        check(device.available_allocation_bytes()<occupied,"second workspace missing charge");
    }
    check(device.available_allocation_bytes()==available,"GPU allocation leak");
    VulkanBinaryOptions tiny;tiny.device_index=selected;tiny.allow_software_device=true;tiny.allocation_byte_limit=1;
    VulkanBinaryDevice limited(tiny);rejects([&]{VulkanBinaryWorkspace bad(limited,op);});check(limited.available_allocation_bytes()==1,"failed allocation budget leak");
    // Admit some real buffers, then reject a later allocation. Constructor
    // unwinding must free those buffers and return every cooperating charge.
    check(allocation_charge>1,"missing workspace allocation requirements");
    VulkanBinaryDevice partial({selected,true,allocation_charge-1});
    for(unsigned cycle=0;cycle<32;++cycle) {
        rejects([&]{VulkanBinaryWorkspace bad(partial,op);});
        check(partial.available_allocation_bytes()==allocation_charge-1,"partial construction allocation leak");
        {
            VulkanBinaryWorkspace repeated(device,op);
            equal(repeated.execute(op,resources,{op.words(),1,64,1<<20,{}, {}}).outputs,expected);
        }
        check(device.available_allocation_bytes()==available,"repeated workspace allocation leak");
        check(resources.available_workers()==resources.capacity(),"repeated CPU reservation leak");
    }
    MixedBinaryCalibration calibration(op,device,resources,{{0,1,16},{0,resources.capacity(),16},{op.words(),1,16},{op.words()/2,resources.capacity(),16}},3,1<<20);
    equal(calibration.execute(resources).outputs,expected);check(calibration.samples().size()==4,"calibration candidates lost");
    for(const auto& s:calibration.samples())check(s.complete_call_ns.size()==3 && s.median_ns>0,"calibration incomplete costs");
    {auto held=resources.reserve(resources.capacity());rejects([&]{(void)calibration.execute(resources);});}
    equal(calibration.execute(resources).outputs,expected);
    for(auto n:{2u,21u,65u,130u}) {
        using namespace storage;
        NativeAccessionRequest request;request.kind=NativeAccessionKind::SourceAspects;request.subject=400;
        request.occurrence={{core::Grade{1},core::OccurrenceKey{10000}}};
        const auto native=[&]{NativeAccessionDirectory directory(native_accession_fixture::endpoint(n),800);return PreparedNativeVulkanBinary(directory.pin(request),401);}();
        VulkanBinaryWorkspace ws(device,native.packed());auto p=MixedBinaryPolicy{native.packed().words(),1,64,1<<20,{}, {}};
        auto result=native.execute(ws,resources,p);
        check(result.program_key==401,"native program identity lost");
        check(result.execution.outputs.front().words()==result.publication.execute_source_program(401).words(),"native declared factor mapping mismatch");
        rejects([&]{PreparedNativeVulkanBinary bad(result.publication,999);});
        check(native.publication().endpoint()->source.grade(core::Grade{1}).aspects.slots()==n*3,"native complete inactive source lost");
    }
}
}
int main(int argc,char** argv) {
    try {
    bool software=false,emit_cases=false;
    for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--allow-software")software=true;else if(arg=="--emit")emit_cases=true;else throw std::invalid_argument("unknown test option");}
    const auto devices=enumerate_vulkan_devices();check(!devices.empty(),"no Vulkan device");std::size_t selected=0;
    if(!software){while(selected<devices.size() && !devices[selected].physical_gpu)++selected;check(selected<devices.size(),"no physical GPU available");}
    VulkanBinaryDevice device({selected,software,std::uint64_t{1}<<28});
    if(software && !devices[selected].physical_gpu)rejects([&]{VulkanBinaryDevice rejected({selected,false,std::uint64_t{1}<<28});});
    for(const auto& info:devices){check(info.heaps.size()>0,"device memory absent");for(const auto& type:info.memory_types)check(type.heap<info.heaps.size(),"heap identity absent");}
    runtime::CpuResourceCoordinator resources(runtime::detect_hardware(),{4,1<<24,100000,runtime::CpuWorkerBasis::AllowedLogicalThreads});
    std::mt19937_64 rng(723412);const std::array<std::size_t,14> tails{0,1,2,31,32,33,63,64,65,127,128,129,513,1025};
    for(unsigned trial=0;trial<196;++trial) {
        const auto slots=trial<112?tails[trial%tails.size()]:rng()%2049;
        auto op=prepare(slots,rng);VulkanBinaryWorkspace ws(device,op);
        auto reference=op.empty_result(1<<20);op.cpu_words(0,op.words(),reference);
        std::vector<machine::PackedBits> actual;
        for(auto gpu:{std::size_t{0},op.words()/2,op.words()}) {
            auto result=ws.execute(op,resources,{gpu,resources.capacity(),1,1<<20,{}, {}});equal(result.outputs,reference);
            check(result.gpu_words+result.cpu_words==op.words(),"partition incomplete");check(result.owner.same_owner(op),"result ownership lost");
            actual=std::move(result.outputs);
        }
        if(emit_cases)emit(op,actual);
    }
    if(!emit_cases) {
        stress(device,resources,selected);
        // Empty program, duplicate/unwritten output registers and no requested outputs.
        AspectSchema schema;AspectPlanes planes(schema,65);BinaryProgram empty(65535);
        PreparedPackedBinary zero(empty,planes,{65534,0,65534});VulkanBinaryWorkspace ws(device,zero);
        auto result=ws.execute(zero,resources,{zero.words(),1,64,1<<20,{}, {}});
        for(const auto& mask:result.outputs)check(mask.none(),"unwritten wide register nonzero");
        PreparedPackedBinary no_outputs(empty,planes,{});VulkanBinaryWorkspace no_ws(device,no_outputs);
        check(no_ws.execute(no_outputs,resources,{no_outputs.words(),1,64,1<<20,{}, {}}).outputs.empty(),"empty output contract");
        std::cout<<checks.load()<<" Vulkan/mixed checks passed; device="<<device.info().name<<"; physical_gpu="<<device.info().physical_gpu<<"\n";
    }
    } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
