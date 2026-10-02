#include "rfc/database/field_work.hpp"
#include "rfc/backend/binary_work.hpp"
#include "native_execution_fixture.hpp"
#include <barrier>
#include <iostream>
#include <random>
#include <future>
using namespace rfc;using namespace rfc::runtime;using namespace rfc::backend;using namespace native_execution_fixture;
namespace {
unsigned checks{};void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"expected rejection");}
HardwareInventory hardware(){HardwareInventory h;h.platform="test";h.affinity_observed=true;for(std::size_t i=0;i<4;++i)h.allowed_cpus.push_back({i,0,i,{}});return h;}
void fields() {
    auto e=endpoint();e.sequence=10;NativeAccessionDirectory directory(e,77);
    const auto analysis=database::NativeFieldAnalysis::inspect(anchor(directory),request(directory));
    const auto operation=database::prepare_parallel_field(analysis,1);
    CpuResourceCoordinator service(hardware(),{4,1<<24,100000});
    std::barrier gate(2);auto hook=[&](auto stage,auto,auto){if(stage==database::ParallelFieldStage::BeforeResult)gate.arrive_and_wait();};
    WorkGraph graph({database::make_field_work({1,1},operation,2,{},WorkAvailability{77,10},{},hook),
                     database::make_field_work({2,1},operation,2,{},WorkAvailability{77,10},{},hook)});
    e.sequence=11;directory.advance(make_temporal_segment(*directory.pin_snapshot(),e));
    auto run=graph.submit(service);run.wait();auto a=run.completion({1,1}).result<database::DerivedParallelNativeField>();
    auto b=run.completion({2,1}).result<database::DerivedParallelNativeField>();
    CpuTileExecutor serial;auto ws=database::make_parallel_workspace(operation);
    const auto reference=database::execute_parallel(operation,ws,serial);
    check(a->values()==reference.values() && b->values()==reference.values(),"concurrent native values changed");
    check(a->provenance().sequence==10 && a->source_publication()->sequence==10,"queued native input advanced");
    check(a->carriers().size()==reference.carriers().size(),"native complete lower support erased");
    check(run.peak_jobs_workers()==std::pair<std::size_t,std::size_t>{2,4},"native jobs did not overlap");
    rejects([&]{(void)database::make_field_work({3,1},operation,1,{},WorkAvailability{77,9});});
    rejects([&]{(void)database::make_field_work({3,1},operation,1,{},WorkAvailability{78,10});});
    check(service.available_workers()==4 && service.available_declared_buffer_bytes()==(1<<24),"native graph resource leak");
}
void cached_fields() {
    auto e=endpoint();NativeAccessionDirectory directory(e,77);
    const auto inspect=[&] { return database::NativeFieldAnalysis::inspect(anchor(directory),request(directory)); };
    const auto a=inspect();
    auto cache=std::make_shared<database::NativeFieldCache>(database::NativeFieldCacheLimits{},[](auto) { return std::uint64_t{0}; });
    CpuResourceCoordinator resources(hardware(),{4,1<<24,100000});
    auto run_field=[&](const auto& analysis,std::size_t rows=1) {
        auto run=WorkGraph({database::make_cached_field_work({1,1},analysis,cache,2,rows,{},WorkAvailability{77,analysis.report().sequence})}).submit(resources);
        run.wait();auto result=run.completion({1,1}).result<database::CachedNativeField>();
        const auto ref=database::execute(database::prepare_field_action(anchor(directory),request(directory)));
        check(result->values()==ref.values(),"cached field dispatch changed exact values");return result;
    };
    const auto original=run_field(a);check(original->producer_execution()->worker_slots==2,"cached producer ignored multicore policy");
    check(cache->stats().workspace_creations==1 && cache->stats().idle_workspaces==1,"CPU scratch was not retained");
    for(unsigned i=1;i<=4;++i) {
        auto next=e;next.sequence=i;
        next.tensors[0].tensor=numeric::SparseRationalTensor::zero(next.tensors[0].tensor.domain(),next.tensors[0].tensor.support());
        if(i%2==0) next.tensors[0]=endpoint().tensors[0];
        directory.advance(make_temporal_segment(e,next));e=std::move(next);
        const auto result=run_field(inspect());check(result->provenance().sequence==i,"warm cached work returned producer publication");
    }
    check(cache->stats().workspace_creations==1 && cache->stats().workspace_hits==1,"same-plan changed values did not reuse exclusive scratch");
    auto b=inspect();auto next=e;next.sequence=5;next.tensors[1].tensor=numeric::SparseRationalTensor::zero(next.tensors[1].tensor.domain(),next.tensors[1].tensor.support());
    directory.advance(make_temporal_segment(e,next));e=std::move(next);const auto c=inspect();
    std::promise<void> reached,release;auto ready=release.get_future();
    auto hook=[&](auto stage) {if(stage==database::NativeFieldCacheStage::BeforeApply) { reached.set_value();ready.wait(); }};
    auto run=WorkGraph({database::make_cached_field_work({2,1},c,cache,2,1,{},WorkAvailability{77,5},{},hook)}).submit(resources);
    reached.get_future().wait();
    auto waiting=WorkGraph({database::make_cached_field_work({3,1},c,cache,2,1)}).submit(resources);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(cache->stats().in_flight_hits==0 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    release.set_value();run.wait();waiting.wait();
    const auto one=run.completion({2,1}).result<database::CachedNativeField>();
    const auto two=waiting.completion({3,1}).result<database::CachedNativeField>();
    check(one->values_pin()==two->values_pin() && cache->stats().in_flight_hits==1,"graph duplicated matching pure work");
    check(one->analysis().receipt().same_plan(b.receipt()) && !one->analysis().receipt().same_result(b.receipt()),"plan/result dependencies conflated");
    std::vector<database::NativeFieldAnalysis> changes;
    for(unsigned i=6;i<=7;++i) {
        next=e;next.sequence=i;
        next.tensors[0].tensor={next.tensors[0].tensor.domain(),next.tensors[0].tensor.support(),
            numeric::SparseRationalVector(2,{{0,Q{static_cast<long>(i),13}},{1,Q{static_cast<long>(i+1),17}}})};
        directory.advance(make_temporal_segment(e,next));e=std::move(next);changes.push_back(inspect());
    }
    std::barrier publish(2);auto both=[&](auto stage) {if(stage==database::NativeFieldCacheStage::BeforePublish)publish.arrive_and_wait();};
    auto different=WorkGraph({database::make_cached_field_work({6,1},changes[0],cache,2,1,{}, {},{},both),
                             database::make_cached_field_work({7,1},changes[1],cache,2,1,{}, {},{},both)}).submit(resources);
    different.wait();
    for(std::size_t i=0;i<changes.size();++i) {
        const auto result=different.completion({6+i,1}).result<database::CachedNativeField>();
        const auto& p=changes[i].binding();auto fibers=temporal_linear_fiber_system(*p.endpoint(),p.selected_carriers());
        numeric::RationalVector input(p.source_dimension());const auto tensors=temporal_endpoint_detail::tensors(*p.endpoint());
        for(const auto& binding:p.inputs())for(const auto& value:tensors.at(binding.field)->tensor.values().entries())input[binding.offset+value.index]=value.value;
        local::PreparedGradeFieldAction reference(fibers,p.upper_grade());
        auto workspace=reference.make_workspace();
        check(result->values()==reference.apply(input,workspace),"concurrent same-plan scratch corrupted changed inputs");
    }
    check(cache->stats().workspace_creations==2 && cache->stats().idle_workspaces==2,"concurrent CPU producers did not loan separate scratch");
    rejects([&]{(void)database::make_cached_field_work({4,1},c,cache,1,1,{},WorkAvailability{77,4});});
    rejects([&]{(void)database::make_cached_field_work({4,1},c,cache,1,1,{},WorkAvailability{78,5});});
    auto tight=ExecutionHostPolicy{};tight.limits.rational_slots=c.report().requirements.rational_slots;
    rejects([&]{(void)database::make_cached_field_work({4,1},c,cache,1,1,{},WorkAvailability{77,5},tight);});
    check(original->provenance().sequence==0 && original->source_publication()->sequence==0,"old cache result was rebound");
    NativeAccessionDirectory foreign(*c.source_publication(),77);const auto f=database::NativeFieldAnalysis::inspect(anchor(foreign),request(foreign));
    CpuTileExecutor cpu({2,2,100000});const auto distinct=cache->evaluate_parallel(f,cpu,1);
    check(!distinct.reuse().plan && !distinct.reuse().result,"colliding foreign authority shared compiled plan");
    auto historical=WorkGraph({database::make_cached_field_work({8,1},a,cache,1,1,{},WorkAvailability{77,0})}).submit(resources);
    historical.wait();const auto past=historical.completion({8,1}).result<database::CachedNativeField>();
    check(past->source_publication()==a.source_publication() && past->provenance().sequence==0,"future cache contents leaked publication metadata into past work");
    database::NativeFieldCacheLimits no_idle;no_idle.idle_workspaces=0;
    database::NativeFieldCache transient(no_idle);(void)transient.evaluate_parallel(c,cpu,1);
    check(transient.stats().idle_workspaces==0,"disabled CPU scratch retention ignored");
    database::NativeFieldCache failed;
    rejects([&]{(void)failed.evaluate_parallel(c,cpu,1,ExecutionLease{},[](auto stage) {
        if(stage==database::NativeFieldCacheStage::BeforePublish)throw std::runtime_error("failed after joined tiles");
    });});
    check(failed.stats().results==0 && failed.stats().idle_workspaces==0 && failed.stats().in_flight==0,"failed CPU loan retained partial work");
    cache->clear();check(cache->stats().idle_workspace_slots==0 && cache->stats().idle_workspaces==0,"clear retained idle CPU scratch");
    check(resources.available_workers()==4 && resources.available_declared_buffer_bytes()==(1<<24),"cached graph leaked delegated resources");
}
composite::BinaryProgram binary(composite::BinaryOpcode opcode) {
    composite::BinaryProgram p(3);p.append({composite::BinaryOpcode::LoadAspect,0,0,0,0,0});p.append({composite::BinaryOpcode::LoadAspect,1,0,0,0,1});p.append({opcode,2,0,1,0,0});return p;
}
void packed() {
    CpuResourceCoordinator service(hardware(),{4,1<<24,100000});std::mt19937_64 random(63421);
    const std::array<std::size_t,13> tails{0,1,31,32,33,63,64,65,127,128,129,513,1025};
    for(unsigned trial=0;trial<100;++trial) {
        const auto slots=tails[trial%tails.size()];composite::AspectSchema schema;schema.add("x");schema.add("b");schema.add("c");
        composite::AspectPlanes planes(schema,slots);for(composite::AspectId a=0;a<3;++a)for(std::size_t w=0;w<planes.plane(a).word_count();++w)planes.plane(a).assign_word(w,random());
        auto original=planes;composite::BinaryProgram loads(3);for(composite::AspectId a=0;a<3;++a)loads.append({composite::BinaryOpcode::LoadAspect,a,0,0,0,a});
        PreparedPackedBinary root(loads,planes,{0,1,2});const auto carrier=PackedWorkCarrier::declare_slots(slots);
        composite::AspectSchema pair;pair.add("left");pair.add("right");
        PackedWorkRecipe f(carrier,binary(composite::BinaryOpcode::And),pair,{{{1,1},0},{{1,1},1}},{2});
        PackedWorkRecipe g(carrier,binary(composite::BinaryOpcode::Xor),pair,{{{2,1},0},{{1,1},2}},{2});
        PackedWorkRecipe reverse_g(carrier,binary(composite::BinaryOpcode::Xor),pair,{{{1,1},0},{{1,1},2}},{2});
        PackedWorkRecipe reverse_f(carrier,binary(composite::BinaryOpcode::And),pair,{{{4,1},0},{{1,1},1}},{2});
        auto root_job=make_cpu_binary_work({1,1},root,carrier,1,3);if(slots)planes.set(0,0,!planes.get(0,0));
        WorkGraph graph({make_cpu_binary_work({3,1},g,2,1),make_cpu_binary_work({5,1},reverse_f),root_job,make_cpu_binary_work({2,1},f),make_cpu_binary_work({4,1},reverse_g)});
        auto run=graph.submit(service);run.wait();const auto forward=run.completion({3,1}).result<PackedWorkResult>();const auto reverse=run.completion({5,1}).result<PackedWorkResult>();
        for(std::size_t i=0;i<slots;++i){
            check(forward->outputs[0].get(i)==((original.get(0,i)&&original.get(1,i))!=original.get(2,i)),"ordered Boolean data flow changed");
            check(reverse->outputs[0].get(i)==((original.get(0,i)!=original.get(2,i))&&original.get(1,i)),"reverse Boolean data flow changed");
        }
        check(forward->predecessors.size()==2 && forward->input_bindings.size()==2,"operation history erased");
        check(forward->predecessors[1]->operation.same_owner(root),"root complete source not retained");
        check(service.available_workers()==4 && service.available_declared_buffer_bytes()==(1<<24),"packed graph budget leak");
    }
    composite::AspectSchema one;one.add("bit");composite::AspectPlanes planes(one,65);composite::BinaryProgram copy(1);copy.append({composite::BinaryOpcode::LoadAspect,0,0,0,0,0});PreparedPackedBinary root(copy,planes,{0});
    const auto good=PackedWorkCarrier::declare_slots(65),wrong=PackedWorkCarrier::declare_slots(65);
    PackedWorkRecipe forged(wrong,copy,one,{{{1,1},0}},{0});auto rejected=WorkGraph({make_cpu_binary_work({1,1},root,good),make_cpu_binary_work({2,1},forged)}).submit(service);rejected.wait();
    check(rejected.completion({2,1}).receipt().status==WorkStatus::Failed,"equal dimensions authenticated carrier");rejects([&]{(void)rejected.completion({2,1}).result<PackedWorkResult>();});
    PackedWorkRecipe missing(good,copy,one,{{{1,1},1}},{0});auto bad_output=WorkGraph({make_cpu_binary_work({1,1},root,good),make_cpu_binary_work({2,1},missing)}).submit(service);bad_output.wait();check(bad_output.completion({2,1}).receipt().status==WorkStatus::Failed,"unwritten output binding accepted");
    rejects([&]{PackedWorkRecipe bad(good,copy,one,{}, {0});});
}
}
int main(){fields();cached_fields();packed();std::cout<<checks<<" native independent/ordered/cached work checks passed\n";}
