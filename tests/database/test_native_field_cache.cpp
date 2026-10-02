#include "rfc/database/native_field_cache.hpp"
#include "native_execution_fixture.hpp"
#include <array>
#include <barrier>
#include <future>
#include <iostream>
#include <thread>
#include <type_traits>

namespace {
using namespace native_execution_fixture;
using namespace rfc::database;
std::size_t checks{};
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& f, const char* message) {
    bool failed{}; try { f(); } catch (const std::exception&) { failed = true; } check(failed,message);
}
NativeFieldAnalysis analyze(const NativeAccessionDirectory& d, bool scalar = false, core::Grade grade = core::Grade{1}) {
    return NativeFieldAnalysis::inspect(anchor(d,grade),request(d,grade),scalar ? NativeFieldRealization::constant_fiber(900) : NativeFieldRealization{});
}
void emit(const std::string& name, const CachedNativeField& result) {
    const auto& plan = result.analysis().binding(); const auto& e = *result.source_publication();
    const auto tensors = temporal_endpoint_detail::tensors(e);
    std::cout << "case " << name << '\n';
    for (const auto& b : plan.inputs()) {
        std::cout << "upper " << b.cell.key << ' ' << b.dimension << '\n';
        for (std::size_t i=0;i<b.dimension;++i) std::cout << "x " << b.cell.key << ' ' << i << ' ' << *tensors.at(b.field)->tensor.state(i) << '\n';
    }
    for (const auto& b : result.carriers()) {
        std::cout << "lower " << b.cell.key << ' ' << b.dimension << '\n';
        for (std::size_t i=0;i<b.dimension;++i) std::cout << "y " << b.cell.key << ' ' << i << ' ' << result.state(b.cell,i) << '\n';
    }
    for (const auto& b : plan.occurrences()) {
        std::cout << "occ " << b.occurrence.key.value << ' ' << b.upper.key << ' ' << b.lower.key << ' ' << b.active << ' ' << b.coefficient << ' '
                  << (b.map ? (b.role==TemporalTensorRole::RealizedIncidenceMap ? "realized" : "independent") : "constant") << '\n';
        if (b.map) { const auto matrix=temporal_tensor_matrix(*tensors.at(*b.map));
            for (const auto& [ij,q] : matrix.entries())
                std::cout << "m " << b.occurrence.key.value << ' ' << ij.first << ' ' << ij.second << ' ' << q << '\n';
        }
    }
    std::cout << "end\n";
}
CachedNativeField verify(NativeAccessionDirectory& d, NativeFieldCache& cache, const std::string& name = {},
                         bool scalar = false, core::Grade grade = core::Grade{1}) {
    const auto a = analyze(d,scalar,grade); const auto result = cache.evaluate(a);
    const auto reference = execute(prepare_field_action(anchor(d,grade),request(d,grade),
        scalar ? NativeFieldRealization::constant_fiber(900) : NativeFieldRealization{}));
    NativeFieldCache parallel;
    runtime::CpuTileExecutor executor({4,4,100000});
    const auto tiled = parallel.evaluate_parallel(a,executor,1);
    check(tiled.values()==reference.values(),"parallel cache differs from independent serial reference");
    check(tiled.producer_execution().has_value(),"parallel cache hid producer execution");
    check(tiled.source_publication()==a.source_publication(),"parallel cache changed source pin");
    check(result.values()==reference.values(),"cache differs from fresh scalar reference");
    check(result.source_publication()==reference.source_publication(),"cache returned old source publication");
    check(result.carriers().size()==reference.carriers().size(),"cache changed complete lower support");
    for (std::size_t i=0;i<result.carriers().size();++i) {
        const auto& x=result.carriers()[i];const auto& y=reference.carriers()[i];
        check(x.cell==y.cell && x.carrier==y.carrier && x.coordinates==y.coordinates && x.dimension==y.dimension && x.offset==y.offset,
              "cache changed carrier meaning");
    }
    const auto warm=cache.evaluate(a);
    check(warm.reuse().result && warm.values_pin()==result.values_pin(),"warm result was not shared");
    check(warm.provenance().sequence==d.sequence(),"warm result provenance stale");
    if (!name.empty()) { emit(name,result); emit(name+"_parallel",tiled); }
    return result;
}
void positive(bool oracle) {
    NativeFieldCache cache;
    for (const auto& name : {"independent","realized","scalar","zero","zero_map","wide","empty"}) {
        const std::string n=name; auto e=endpoint(n=="scalar",n=="realized",n=="zero");
        if (n=="zero_map") for (auto& t:e.tensors) if(t.role==TemporalTensorRole::CoefficientIndependentMap)
            t.tensor=numeric::SparseRationalTensor::zero(t.tensor.domain(),t.tensor.support());
        if (n=="wide") {
            numeric::Integer big=1; big<<=1100;
            e.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q(big,numeric::Integer{37})},{1,Q{-1,43}}})};
            auto m=temporal_tensor_matrix(e.tensors[3]);m.set(2,0,Q(big,numeric::Integer{41}));e.tensors[3].tensor=temporal_matrix_tensor(m);
        }
        NativeAccessionDirectory d(e,77); const auto result=verify(d,cache,oracle?n:"",n=="scalar",core::Grade{static_cast<std::uint16_t>(n=="empty"?2:1)});
        const auto& facts=result.analysis().layout();
        check(facts.source_coordinates==result.analysis().binding().source_dimension() && facts.target_coordinates==result.values().size(),"analyzer reduced native coordinates");
        check(facts.output_cpp_object_bytes==result.values().size()*sizeof(Q) && facts.variable_output_encoding,"analyzer confused encoded and object bytes");
        if (n=="zero") check(facts.field_support==4 && facts.stored_field_values==0,"zero erased declared support");
    }
    auto e=endpoint();NativeAccessionDirectory d(e,77);NativeFieldCache c;
    const auto old=verify(d,c);const auto old_analysis=analyze(d);
    auto next=e;next.sequence=1;
    next.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{3,5}},{1,Q{-7,23}}})};
    d.advance(make_temporal_segment(e,next)); const auto changed=verify(d,c,oracle?"field_changed":"");
    check(changed.reuse().plan && c.stats().compiled_plans==1,"field value change recompiled unchanged map plan");
    check(changed.analysis().receipt().same_plan(old_analysis.receipt()) && !changed.analysis().receipt().same_result(old_analysis.receipt()),"plan and result receipts conflated");
    check(old.provenance().sequence==0 && old.source_publication()!=changed.source_publication(),"old result/source pin altered");
    auto unread=next;unread.sequence=2;TemporalTensor opaque;opaque.key=99000;opaque.interpretation="unread exact payload";unread.tensors.push_back(opaque);
    d.advance(make_temporal_segment(next,unread));const auto unchanged=verify(d,c);
    check(unchanged.reuse().result && unchanged.source_publication()==d.pin_snapshot() && unchanged.provenance().sequence==2,"unread change reused stale provenance");
    for (const bool both : {false,true}) {
        auto target=e;target.sequence=1;if(both)target.tensors[0]=next.tensors[0];
        auto m=temporal_tensor_matrix(target.tensors[3]);m.set(1,0,Q{17,29});target.tensors[3].tensor=temporal_matrix_tensor(m);
        NativeAccessionDirectory x(e,77);NativeFieldCache local;(void)verify(x,local);x.advance(make_temporal_segment(e,target));
        const auto y=verify(x,local,oracle?(both?"both_changed":"map_changed"):"");
        check(!y.reuse().plan && !y.reuse().result && local.stats().compiled_plans==2,"changed map reused plan");
    }
    auto permuted=e;permuted.sequence=1;numeric::ExactMatrix P(2,2);P.set(0,1,Q{1});P.set(1,0,Q{1});
    for(auto& carrier:permuted.carriers)if(carrier.key==101)carrier.coordinate_system=99901;
    permuted.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{13,17}},{1,Q{7,11}}})};
    for(auto& t:permuted.tensors)if(t.role==TemporalTensorRole::CoefficientIndependentMap)t.tensor=temporal_matrix_tensor(temporal_tensor_matrix(t).multiply(P));
    NativeAccessionDirectory pd(e,77);NativeFieldCache pc;const auto py=verify(pd,pc);pd.advance(make_temporal_segment(e,permuted,{{101,P}}));
    const auto perm=verify(pd,pc,oracle?"permuted":"");check(perm.values()==py.values() && !perm.reuse().plan,"equal output identified new coordinates");
    composite::AspectSchema aspects;SourceDeclarationMigration migration;migration.base_signature=e.source.signature();
    for(std::size_t i=migration.base_signature.aspects().size();i-->0;) {const auto& a=migration.base_signature.aspects().at(static_cast<composite::AspectId>(i));aspects.add(a.name,a.role,a.default_value);migration.aspects.push_back({a.name,a.name});}
    migration.target_signature=core::RelationSignature(aspects);migration.target_signature.add_type("collective",core::BoundaryPolicy::DeclaredExact,0,10);migration.types={{"collective","collective"}};
    auto recoded=e;recoded.sequence=1;recoded.source=recode_temporal_source(e.source,migration);
    NativeAccessionDirectory rd(e,77);NativeFieldCache rc;const auto ry=verify(rd,rc);rd.advance(make_temporal_segment(e,recoded,{}, {},RelationalCodeEncoding::Automatic,migration));
    const auto rec=verify(rd,rc,oracle?"recoded":"");check(rec.values()==ry.values() && !rec.reuse().plan,"source recoding identified physical source bytes");
}
TemporalEndpoint empty(bool born) {
    core::RelationSignature s;const auto type=s.add_type("free",core::BoundaryPolicy::DeclaredExact,0,10);core::RelationalComplexBuilder b(s);
    b.declare_grade(core::Grade{1});b.declare_witness_identity(10);if(born)b.add_grade_one(core::RelationKey{101},type,{});
    TemporalEndpoint e(b.freeze(),born?1:0);e.carriers.push_back({10,TemporalCarrierKind::CellStalk,core::Grade{0},StableCellKey{core::Grade{0},10},900,numeric::IndexDomain::linear(1)});
    if(born) {e.carriers.push_back({101,TemporalCarrierKind::CellStalk,core::Grade{1},StableCellKey{core::Grade{1},101},901,numeric::IndexDomain::linear(2)});
        TemporalTensor t;t.key=2101;t.role=TemporalTensorRole::Field;t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell={core::Grade{1},101};t.output=101;
        t.tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2));e.tensors.push_back(t);}
    return e;
}
void dependencies() {
    auto e=empty(false);NativeAccessionDirectory d(e,100);NativeFieldCache cache;const auto a=verify(d,cache);
    check(a.analysis().receipt().scope().upper_field_members==0 && a.analysis().receipt().scope().negative_membership,"empty selection has no negative membership");
    auto born=empty(true);d.advance(make_temporal_segment(e,born));const auto b=verify(d,cache);
    check(a.values()==b.values() && !b.reuse().plan && b.analysis().layout().source_coordinates==2 && b.analysis().receipt().scope().upper_field_members==1,"birth reused empty membership");
    auto z=endpoint(false,false,true);NativeAccessionDirectory maps(z,101);NativeFieldCache mc;const auto zy=verify(maps,mc);
    auto changed=z;changed.sequence=1;auto m=temporal_tensor_matrix(changed.tensors.back());m.set(0,1,Q{123});changed.tensors.back().tensor=temporal_matrix_tensor(m);
    maps.advance(make_temporal_segment(z,changed));const auto cy=verify(maps,mc);
    check(cy.values()==zy.values() && !cy.reuse().plan,"inactive map or equal zero result aliased plan");
    auto membership=changed;membership.sequence=2;auto field=membership.tensors[0];field.key=9911;membership.tensors.push_back(field);
    maps.advance(make_temporal_segment(changed,membership));auto selected=request(maps);
    std::erase_if(selected.fields,[](const auto& p){return p.tensor_declaration()->key==9911;});
    const auto ma=NativeFieldAnalysis::inspect(anchor(maps),selected);const auto my=mc.evaluate(ma);
    check(my.values()==cy.values() && !my.reuse().plan && ma.receipt().scope().upper_field_members==3,"new unselected matching field escaped catalog receipt");
    rejects([&]{(void)analyze(maps);},"duplicate automatic upper field ownership accepted");
    NativeAccessionDirectory foreign(changed,101);const auto f=analyze(foreign);
    check(!f.receipt().same_authority(cy.analysis().receipt()),"equal namespace authenticated another directory");
    check(!mc.evaluate(f).reuse().plan && !mc.evaluate(f).reuse().in_flight,"foreign owner cache reuse");
    const auto before=analyze(foreign);foreign.reconfigure({});const auto after=analyze(foreign);
    check(before.receipt().same_result(after.receipt()),"directory reconfiguration changed execution authority");
    auto realized=changed;realized.sequence=2;
    for(auto& t:realized.tensors)if(t.role==TemporalTensorRole::CoefficientIndependentMap) {
        t.tensor=temporal_matrix_tensor(temporal_realized_incidence(realized,t));t.role=TemporalTensorRole::RealizedIncidenceMap;
    }
    NativeAccessionDirectory roles(changed,121);NativeFieldCache role_cache;const auto original_role=verify(roles,role_cache);
    roles.advance(make_temporal_segment(changed,realized));const auto new_role=verify(roles,role_cache);
    check(original_role.values()==new_role.values() && !new_role.reuse().plan,"realized and independent map roles aliased");
    auto declaration=changed;declaration.sequence=2;TemporalDeclarationState ds;ds.namespace_key=444;
    ds.registry=composite::DeclarationRegistry({composite::Declaration::rational({444,1,1})});declaration.declarations=ds;
    NativeAccessionDirectory dd(changed,111);NativeFieldCache dc;const auto old=verify(dd,dc);dd.advance(make_temporal_segment(changed,declaration));
    const auto fresh=verify(dd,dc);check(old.values()==fresh.values() && !fresh.reuse().plan,"declaration change reused descriptor");
    auto partial=z;partial.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::sparse(2,{0}),numeric::SparseRationalVector(2)};
    NativeAccessionDirectory bad(partial,112);rejects([&]{(void)analyze(bad);},"missing support became supported zero");
    auto missing=z;missing.tensors.pop_back();NativeAccessionDirectory bd(missing,113);rejects([&]{(void)analyze(bd);},"inactive missing map bypassed admission");
    rejects([&]{(void)NativeFieldDependencyReceipt::inspect(f.binding(),anchor(maps));},"foreign receipt construction accepted");
}
void policies_and_failure() {
    auto e=endpoint();NativeAccessionDirectory d(e,777);const auto a=analyze(d);NativeFieldCache cache;const auto retained=cache.evaluate(a);
    auto policy=runtime::ExecutionHostPolicy{};policy.limits.rational_slots=a.report().requirements.rational_slots-1;
    rejects([&]{(void)cache.evaluate(a,runtime::ExecutionLease(policy));},"warm cache bypassed tighter cap");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;rejects([&]{(void)cache.evaluate(a,runtime::ExecutionLease(policy));},"warm cache invented GPU");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(cache.evaluate(a,runtime::ExecutionLease(policy)).provenance().fallback,"warm cache lost explicit fallback");
    auto cancel=std::make_shared<std::atomic_bool>(true);rejects([&]{(void)cache.evaluate(a,runtime::ExecutionLease({},cancel));},"warm cache bypassed cancellation");
    for(auto stage:{NativeFieldCacheStage::BeforeCompile,NativeFieldCacheStage::BeforeApply,NativeFieldCacheStage::BeforePublish}) {
        NativeFieldCache failed;rejects([&]{(void)failed.evaluate(a,runtime::ExecutionLease{},[&](auto event){if(event==stage)throw std::runtime_error("injected");});},"failure hook ignored");
        check(failed.stats().plans==0 && failed.stats().results==0 && failed.stats().in_flight==0 && failed.stats().failed_jobs==1,"failed job cached partial work");
        (void)failed.evaluate(a);check(failed.stats().results==1,"failed job prevented retry");
    }
    NativeFieldCache stopped;cancel->store(false);
    rejects([&]{(void)stopped.evaluate(a,runtime::ExecutionLease({},cancel),[&](auto stage){if(stage==NativeFieldCacheStage::BeforePublish)cancel->store(true);});},"late phase cancellation ignored");
    check(stopped.stats().results==0 && stopped.stats().plans==0,"cancelled producer cached work");
    NativeFieldCache tiny({1,1,1,1,1,1});rejects([&]{(void)tiny.evaluate(a);},"cache budget ignored");
    NativeFieldCache recursive;bool tested{};
    (void)recursive.evaluate(a,runtime::ExecutionLease{},[&](auto stage){if(stage==NativeFieldCacheStage::BeforeApply) {
        rejects([&]{(void)recursive.evaluate(a);},"recursive evaluation deadlocked");rejects([&]{recursive.clear();},"recursive clear deadlocked");tested=true;}});
    check(tested && recursive.stats().results==1,"reentry corrupted successful producer");
    NativeFieldCache* target{};NativeFieldCache selector({},[&](auto){if(target)(void)target->stats();return std::uint64_t{0};});target=&selector;
    rejects([&]{(void)selector.evaluate(a);},"selector reentry deadlocked");check(selector.stats().results==0,"failed selector cached");
    cache.clear();check(cache.stats().results==0 && retained.values().size()==5,"clear destroyed retained result");
    const auto preserved=cache.evaluate(a);
    auto next=e;next.sequence=1;next.tensors[0].tensor=numeric::SparseRationalTensor::zero(next.tensors[0].tensor.domain(),next.tensors[0].tensor.support());
    d.advance(make_temporal_segment(e,next));const auto newer=analyze(d);
    rejects([&]{(void)cache.evaluate(newer,runtime::ExecutionLease{},[](auto stage){if(stage==NativeFieldCacheStage::BeforePublish)throw std::runtime_error("failed new result");});},"new failure ignored");
    check(cache.stats().results==1 && cache.evaluate(a).values_pin()==preserved.values_pin(),"new failure replaced valid old entry");
    auto moved=a;auto own=std::move(moved);rejects([&]{(void)moved.report();},"moved analysis accepted");check(own.source_publication()==retained.source_publication(),"moved analysis lost owner");
    rejects([&]{(void)NativeFieldAnalysis::inspect(anchor(d),request(d),{}, {},{1});},"dependency byte budget ignored");
    static_assert(std::is_nothrow_move_constructible_v<CachedNativeField>);
}
void random(bool oracle) {
    auto e=endpoint();NativeAccessionDirectory d(e,555);NativeFieldCache cache({},[](auto){return std::uint64_t{0};});std::uint64_t seed=31337;
    const auto q=[&]{seed=seed*6364136223846793005ULL+1;return Q{static_cast<long>((seed>>12)%101)-50,static_cast<unsigned long>((seed>>32)%37+1)};};
    for(std::size_t n=0;n<96;++n) {
        auto next=e;next.sequence=n+1;
        for(auto& t:next.tensors) {
            if(t.role==TemporalTensorRole::Field) {std::vector<numeric::SparseRationalEntry> values;for(std::size_t i=0;i<t.tensor.domain().logical_size();++i)values.push_back({i,q()});t.tensor={t.tensor.domain(),t.tensor.support(),numeric::SparseRationalVector(t.tensor.domain().logical_size(),values)};}
            else if(n%3==0) {auto m=temporal_tensor_matrix(t);for(std::size_t i=0;i<m.rows();++i)for(std::size_t j=0;j<m.cols();++j)m.set(i,j,q());t.tensor=temporal_matrix_tensor(m);}
        }
        d.advance(make_temporal_segment(e,next));(void)verify(d,cache,oracle?"random_"+std::to_string(n):"");e=std::move(next);
    }
    check(cache.stats().plan_hits>=60 && cache.stats().descriptor_comparisons>100,"collisions or compiled reuse not exercised");
    NativeFieldCache bounded({1,1,64u*1024u*1024u,5,4,64u*1024u*1024u});auto saved=bounded.evaluate(analyze(d));
    auto next=e;next.sequence=97;next.tensors[0].tensor=numeric::SparseRationalTensor::zero(next.tensors[0].tensor.domain(),next.tensors[0].tensor.support());
    d.advance(make_temporal_segment(e,next));(void)bounded.evaluate(analyze(d));
    check(bounded.stats().results==1 && bounded.stats().evicted_results==1 && saved.provenance().sequence==96,"FIFO eviction lost retained owner");
}
void threads() {
    auto e=endpoint();NativeAccessionDirectory d(e,999);const auto a=analyze(d);NativeFieldCache cache;
    std::promise<void> reached,release;auto go=release.get_future();std::array<std::exception_ptr,5> errors{};
    std::array<std::optional<CachedNativeField>,5> outputs;auto cancel=std::make_shared<std::atomic_bool>(false);
    std::thread producer([&]{try{outputs[0]=cache.evaluate(a,runtime::ExecutionLease{},[&](auto stage){if(stage==NativeFieldCacheStage::BeforeApply){reached.set_value();go.wait();}});}catch(...){errors[0]=std::current_exception();}});
    reached.get_future().wait();std::vector<std::thread> readers;
    auto next=e;next.sequence=1;TemporalTensor opaque;opaque.key=99000;opaque.interpretation="unread threaded payload";next.tensors.push_back(opaque);
    d.advance(make_temporal_segment(e,next));const auto updated=analyze(d);
    for(std::size_t i=1;i<5;++i) readers.emplace_back([&,i]{try{outputs[i]=cache.evaluate(i==2?updated:a,i==4?runtime::ExecutionLease({},cancel):runtime::ExecutionLease{});}catch(...){errors[i]=std::current_exception();}});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(cache.stats().in_flight_hits<4 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    const auto hits=cache.stats().in_flight_hits;cancel->store(true);readers.back().join();release.set_value();producer.join();
    for(std::size_t i=0;i<3;++i)readers[i].join();
    check(hits==4 && errors[4]!=nullptr && !outputs[4],"waiter cancellation did not isolate producer");
    for(std::size_t i=0;i<4;++i) {if(errors[i])std::rethrow_exception(errors[i]);check(outputs[i]->values_pin()==outputs[0]->values_pin(),"in-flight result not shared");if(i)check(outputs[i]->reuse().in_flight,"waiter not reported");}
    check(cache.stats().applications==1 && cache.stats().compiled_plans==1 && cache.stats().results==1,"in-flight sharing executed twice");
    check(outputs[2]->provenance().sequence==1 && outputs[2]->source_publication()==updated.source_publication(),"joined work returned producer's old context");
    constexpr std::size_t count=8;std::barrier start(static_cast<std::ptrdiff_t>(count));std::array<std::exception_ptr,count> faults{};std::array<std::thread,count> workers;
    for(std::size_t i=0;i<count;++i)workers[i]=std::thread([&,i]{try{start.arrive_and_wait();for(std::size_t n=0;n<100;++n){const auto result=cache.evaluate(a);if(result.values()!=outputs[0]->values())throw std::runtime_error("threaded cache value mismatch");}}catch(...){faults[i]=std::current_exception();}});
    for(auto& worker:workers)worker.join();
    for(auto error:faults)if(error)std::rethrow_exception(error);
    check(cache.stats().result_hits>=800 && cache.stats().applications==1,"warm concurrent hits recomputed");
    NativeFieldCache failed;std::promise<void> blocked,unblock;auto proceed=unblock.get_future();std::array<std::exception_ptr,3> failures{};
    std::thread leader([&]{try{(void)failed.evaluate(a,runtime::ExecutionLease{},[&](auto stage){if(stage==NativeFieldCacheStage::BeforeApply){blocked.set_value();proceed.wait();throw std::runtime_error("shared failure");}});}catch(...){failures[0]=std::current_exception();}});
    blocked.get_future().wait();std::array<std::thread,2> waiting;
    for(std::size_t i=0;i<2;++i)waiting[i]=std::thread([&,i]{try{(void)failed.evaluate(a);}catch(...){failures[i+1]=std::current_exception();}});
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(failed.stats().in_flight_hits<2 && std::chrono::steady_clock::now()<until)std::this_thread::yield();
    const auto joined=failed.stats().in_flight_hits;unblock.set_value();leader.join();for(auto& waiter:waiting)waiter.join();
    check(joined==2 && failures[0] && failures[1] && failures[2],"producer failure did not reach waiters");
    check(failed.stats().results==0 && failed.stats().plans==0 && failed.stats().in_flight==0,"failed shared job published");
    (void)failed.evaluate(a);check(failed.stats().results==1,"shared failure prevented fresh retry");
    std::cout<<"threads: one producer, four joined waiters (one cancelled), 800 completed-result reads\n";
}
} // namespace
int main(int argc,char** argv) {
    try {const bool oracle=argc==2 && std::string(argv[1])=="--oracle";positive(oracle);dependencies();policies_and_failure();random(oracle);if(!oracle)threads();std::cout<<"checks "<<checks<<'\n';return 0;}
    catch(const std::exception& e){std::cerr<<"P3 cache: "<<e.what()<<'\n';return 1;}
}
