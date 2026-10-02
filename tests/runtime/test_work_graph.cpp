#include "rfc/runtime/work_graph.hpp"
#include <barrier>
#include <iostream>
#include <latch>
using namespace rfc::runtime;
namespace {
unsigned checks{};void check(bool yes,const char* text){++checks;if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"expected rejection");}
HardwareInventory hardware(){HardwareInventory h;h.platform="test";h.affinity_observed=true;for(std::size_t i=0;i<4;++i)h.allowed_cpus.push_back({i,0,i,{}});return h;}
WorkJob constant(WorkKey key,int value,std::vector<WorkKey> after={}) {return WorkJob::make<int>(key,{1,8,4},std::move(after),[value](const auto&,auto& assigned,auto){auto lease=assigned.reserve(1,8);return value;});}
void basic() {
    CpuResourceCoordinator service(hardware(),{4,1024,100});
    auto a=constant({1,1},3),b=constant({1,2},9);
    auto c=WorkJob::make<int>({2,1},{2,16,4},{{1,1},{1,2}},[](const auto& inputs,auto& assigned,auto){auto lease=assigned.reserve(2,16);return *inputs.at({1,1}).template as<int>()+*inputs.at({1,2}).template as<int>();});
    WorkGraph graph({c,b,a});auto run=graph.submit(service);auto old=run.completion({1,1});check(*old.result<int>()==3,"old generation overwritten");check(*run.completion({1,2}).result<int>()==9,"new generation lost");check(*run.completion({2,1}).result<int>()==12,"dependent inputs lost");run.wait();
    check(service.available_workers()==4 && service.available_declared_buffer_bytes()==1024,"graph resources leaked");
    rejects([&]{(void)run.completion({1,3});});rejects([&]{(void)old.result<long>();});
    check(run.peak_jobs_workers().second<=4,"worker budget exceeded");
    rejects([&]{WorkGraph bad({a,a});});rejects([&]{WorkGraph bad({constant({3,1},0,{{9,1}})});});
    rejects([&]{WorkGraph bad({constant({3,1},0,{{4,1}}),constant({4,1},0,{{3,1}})});});
    rejects([&]{WorkGraph bad({a,constant({3,1},0,{{1,1},{1,1}})});});
    rejects([&]{WorkGraph bad({a},{0,10,2,100});});rejects([&]{WorkGraph bad({a},{10,10,2,0});});
    rejects([&]{WorkGraph bad({WorkJob::make<int>({9,1},{5,1,1},{},[](const auto&,auto&,auto){return 0;})});(void)bad.submit(service);});
    WorkGraph empty({});auto nothing=empty.submit(service);nothing.wait();check(nothing.peak_jobs_workers().first==0,"empty graph started work");
}
void concurrent_and_delegate() {
    CpuResourceCoordinator service(hardware(),{4,1024,100});std::barrier gate(2);
    auto make=[&](WorkKey key){return WorkJob::make<int>(key,{2,32,4},{},[&](const auto&,auto& assigned,auto){auto lease=assigned.reserve(2,32);gate.arrive_and_wait();return 7;});};
    auto run=WorkGraph({make({10,1}),make({11,1})}).submit(service);run.wait();check(run.peak_jobs_workers()==std::pair<std::size_t,std::size_t>{2,4},"independent work did not overlap");check(*run.completion({10,1}).result<int>()==7,"concurrent value changed");
    {
        auto delegated=CpuResourceCoordinator::delegate(service.reserve(3,80));check(service.available_workers()==1,"parent budget not held");
        auto a=delegated.reserve(2,40);auto b=delegated.reserve(1,40);rejects([&]{(void)delegated.reserve(1);});
        check(service.available_workers()==1,"delegation double charged");
    }
    check(service.available_workers()==4 && service.available_declared_buffer_bytes()==1024,"delegation leaked budget");
    auto shared=service.share();auto held=shared.reserve(4);auto waiting=WorkGraph({constant({12,1},2)}).submit(service);
    waiting.completion({12,1}).cancel();waiting.wait();check(waiting.completion({12,1}).receipt().status==WorkStatus::Cancelled,"occupied-budget cancellation stuck");
}
void failure_cancel_lifetime() {
    CpuResourceCoordinator service(hardware(),{2,1024,100});std::latch entered(1),release(1);
    auto blocked=WorkJob::make<int>({20,1},{1,8,4},{},[&](const auto&,auto& assigned,std::stop_token stop){auto lease=assigned.reserve(1,8);entered.count_down();release.wait();if(stop.stop_requested())throw ExecutionCancelled{};return 3;});
    auto bad=WorkJob::make<int>({21,1},{1,8,4},{},[](const auto&,auto&,auto)->int{throw std::bad_alloc{};});
    auto run=WorkGraph({blocked,bad,constant({22,1},8,{{21,1}}),constant({23,1},9)}).submit(service);
    entered.wait();run.completion({20,1}).cancel();release.count_down();run.wait();
    check(run.completion({20,1}).receipt().status==WorkStatus::Cancelled,"running cancel lost");
    check(run.completion({21,1}).receipt().status==WorkStatus::Failed,"failure not retained");
    check(run.completion({22,1}).receipt().status==WorkStatus::Blocked,"descendant ran after failure");
    check(*run.completion({23,1}).result<int>()==9,"unrelated work aborted");rejects([&]{(void)run.completion({21,1}).result<int>();});
    check(service.available_workers()==2,"failed work leaked workers");
    auto retained=[&]{auto short_run=WorkGraph({constant({30,1},5)}).submit(service);auto result=short_run.completion({30,1});result.wait();return result;}();
    check(*retained.result<int>()==5,"completion lost result after run destruction");
    std::latch start(1),finish(1);std::optional<WorkCompletion> future;
    {auto r=WorkGraph({WorkJob::make<int>({31,1},{1,0,4},{},[&](const auto&,auto&,auto){start.count_down();finish.wait();return 6;})}).submit(service);future=r.completion({31,1});start.wait();finish.count_down();}
    check(work_terminal(future->receipt().status),"run destruction did not quiesce");
    for(unsigned i=0;i<100;++i){auto r=WorkGraph({constant({40,1},1),constant({40,2},2)}).submit(service);r.wait();check(*r.completion({40,1}).result<int>()==1 && service.available_workers()==2,"repeated graph leak");}
}
void generations_and_waiting() {
    CpuResourceCoordinator service(hardware(),{4,1024,100});std::latch entered(1),release(1);
    auto old=WorkJob::make<int>({50,1},{1,0,4},{},[&](const auto&,auto&,auto){entered.count_down();release.wait();return 1;});
    auto run=WorkGraph({old,constant({50,2},2)}).submit(service);entered.wait();
    WorkResultSlot selected(50);selected.expect(run.completion({50,1}));selected.expect(run.completion({50,2}));
    auto current=run.completion({50,2});current.wait();check(selected.accept(current),"expected completion rejected");
    release.count_down();run.wait();check(!selected.accept(run.completion({50,1})),"late old generation replaced current result");
    check(*selected.result<int>()==2,"current prospective value changed");
    auto other=WorkGraph({constant({50,2},99)}).submit(service);other.wait();
    check(!selected.accept(other.completion({50,2})),"foreign same-key completion accepted");rejects([&]{selected.expect(current);});
    auto moved=std::move(run);rejects([&]{run.wait();});moved.wait();auto copy=std::move(current);rejects([&]{current.wait();});copy.wait();
    std::optional held{service.reserve(4)};auto queued=WorkGraph({constant({51,1},7)}).submit(service);held.reset();queued.wait();
    check(*queued.completion({51,1}).result<int>()==7,"budget release failed to wake queued job");
    rejects([&]{WorkGraph bad({constant({1,1},1),constant({2,1},1,{{1,1}})},{10,10,4,100,1});});
}
}
int main(){basic();concurrent_and_delegate();failure_cancel_lifetime();generations_and_waiting();std::cout<<checks<<" owned work graph checks passed\n";}
