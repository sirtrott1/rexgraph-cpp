#include "rfc/database/parallel_native_field.hpp"
#include "native_execution_fixture.hpp"
#include <array>
#include <barrier>
#include <future>
#include <iostream>
#include <set>

namespace {
using namespace native_execution_fixture;
using namespace rfc::database;
std::size_t checks{};
void check(bool value,const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& f,const char* message) {
    bool failed{}; try { f(); } catch(const std::exception&) { failed=true; } check(failed,message);
}
NativeFieldAnalysis analyze(const NativeAccessionDirectory& directory,bool scalar=false,core::Grade grade=core::Grade{1}) {
    return NativeFieldAnalysis::inspect(anchor(directory,grade),request(directory,grade),
        scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{});
}
void emit(const std::string& name, const DerivedParallelNativeField& result, const NativeFieldAnalysis& analysis) {
    const auto& plan = analysis.binding(); const auto& e = *result.source_publication();
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
DerivedParallelNativeField verify(const NativeAccessionDirectory& d,const std::string& name={},bool scalar=false,core::Grade grade=core::Grade{1}) {
    const auto a=analyze(d,scalar,grade);
    const auto reference=execute(prepare_field_action(anchor(d,grade),request(d,grade),
        scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{}));
    std::optional<DerivedParallelNativeField> saved;
    for(const auto rows:{1u,2u,8u}) {
        const auto operation=prepare_parallel_field(a,rows);auto workspace=make_parallel_workspace(operation);
        std::size_t begin{},entries{};
        for(const auto& tile:operation.ranges().tiles()) {
            check(tile.begin==begin && tile.end>tile.begin && tile.end-tile.begin<=rows,"tile coordinates not complete/disjoint");
            check(tile.entry_begin==entries && tile.entry_end>=entries,"tile schedule lost entries");begin=tile.end;entries=tile.entry_end;
        }
        check(operation.ranges().action().blocks().size()==a.layout().retained_occurrences &&
            operation.ranges().action().statistics().active_occurrences==a.layout().active_occurrences,"retained/inactive schedule changed");
        check(begin==a.layout().target_coordinates && entries==operation.ranges().action().entries().size(),"range schedule incomplete");
        check(workspace.scratch_slots()==reference.values().size()+operation.ranges().tiles().size(),"scratch owners not charged");
        for(const auto workers:{1u,2u,4u}) {
            runtime::CpuTileExecutor executor({workers,4,100});
            const auto result=execute_parallel(operation,workspace,executor,runtime::ExecutionLease{},
                [](auto stage,auto tile,auto worker) { if(stage==ParallelFieldStage::BeforeTile && (tile+worker)%2==0)std::this_thread::yield(); });
            check(result.values()==reference.values(),"parallel values differ from fresh scalar reference");
            check(result.source_publication()==reference.source_publication() && result.provenance().sequence==d.sequence(),"parallel provenance stale");
            check(result.carriers().size()==reference.carriers().size(),"parallel support changed");
            for(std::size_t i=0;i<result.carriers().size();++i) {
                const auto& x=result.carriers()[i];const auto& y=reference.carriers()[i];
                check(x.cell==y.cell && x.carrier==y.carrier && x.coordinates==y.coordinates && x.dimension==y.dimension && x.offset==y.offset,"parallel carrier meaning changed");
            }
            check(result.execution().completed_tiles==operation.ranges().tiles().size() &&
                result.execution().worker_slots==std::min<std::size_t>(workers,operation.ranges().tiles().size()) &&
                result.execution().launched_threads==(result.execution().worker_slots?result.execution().worker_slots-1:0),"worker/tile accounting wrong");
            const auto repeated=execute_parallel(operation,workspace,executor);
            check(repeated.values()==result.values(),"reused workspace changed output");
            if(!name.empty())emit(name+(workers==1 && rows==1?"":"@w"+std::to_string(workers)+"r"+std::to_string(rows)),result,a);
            saved=result;
        }
    }
    return *saved;
}
void positive(bool oracle) {
    for(const auto& name:{"independent","realized","scalar","zero","zero_map","wide","empty"}) {
        const std::string n=name;auto e=endpoint(n=="scalar",n=="realized",n=="zero");
        if(n=="zero_map")for(auto& t:e.tensors)if(t.role==TemporalTensorRole::CoefficientIndependentMap)
            t.tensor=numeric::SparseRationalTensor::zero(t.tensor.domain(),t.tensor.support());
        if(n=="wide") {
            numeric::Integer big=1;big<<=1100;
            e.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q(big,numeric::Integer{37})},{1,Q{-1,43}}})};
            auto m=temporal_tensor_matrix(e.tensors[3]);m.set(2,0,Q(big,numeric::Integer{41}));e.tensors[3].tensor=temporal_matrix_tensor(m);
        }
        NativeAccessionDirectory d(e,77);(void)verify(d,oracle?n:"",n=="scalar",core::Grade{static_cast<std::uint16_t>(n=="empty"?2:1)});
    }
    auto e=endpoint();auto changed=e;changed.sequence=1;
    changed.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{3,5}},{1,Q{-7,23}}})};
    NativeAccessionDirectory d(e,77);const auto old=verify(d);d.advance(make_temporal_segment(e,changed));(void)verify(d,oracle?"field_changed":"");
    check(old.provenance().sequence==0,"old result altered after advance");
    for(const bool both:{false,true}) {
        auto target=e;target.sequence=1;if(both)target.tensors[0]=changed.tensors[0];
        auto m=temporal_tensor_matrix(target.tensors[3]);m.set(1,0,Q{17,29});target.tensors[3].tensor=temporal_matrix_tensor(m);
        NativeAccessionDirectory x(e,77);x.advance(make_temporal_segment(e,target));(void)verify(x,oracle?(both?"both_changed":"map_changed"):"");
    }
    auto permuted=e;permuted.sequence=1;numeric::ExactMatrix P(2,2);P.set(0,1,Q{1});P.set(1,0,Q{1});
    for(auto& carrier:permuted.carriers)if(carrier.key==101)carrier.coordinate_system=99901;
    permuted.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{13,17}},{1,Q{7,11}}})};
    for(auto& t:permuted.tensors)if(t.role==TemporalTensorRole::CoefficientIndependentMap)t.tensor=temporal_matrix_tensor(temporal_tensor_matrix(t).multiply(P));
    NativeAccessionDirectory pd(e,77);pd.advance(make_temporal_segment(e,permuted,{{101,P}}));(void)verify(pd,oracle?"permuted":"");
    composite::AspectSchema aspects;SourceDeclarationMigration migration;migration.base_signature=e.source.signature();
    for(std::size_t i=migration.base_signature.aspects().size();i-->0;) {const auto& a=migration.base_signature.aspects().at(static_cast<composite::AspectId>(i));aspects.add(a.name,a.role,a.default_value);migration.aspects.push_back({a.name,a.name});}
    migration.target_signature=core::RelationSignature(aspects);migration.target_signature.add_type("collective",core::BoundaryPolicy::DeclaredExact,0,10);migration.types={{"collective","collective"}};
    auto recoded=e;recoded.sequence=1;recoded.source=recode_temporal_source(e.source,migration);
    NativeAccessionDirectory rd(e,77);rd.advance(make_temporal_segment(e,recoded,{}, {},RelationalCodeEncoding::Automatic,migration));(void)verify(rd,oracle?"recoded":"");
}
void random(bool oracle) {
    auto e=endpoint();NativeAccessionDirectory d(e,555);std::uint64_t seed=31337;
    const auto q=[&]{seed=seed*6364136223846793005ULL+1;return Q{static_cast<long>((seed>>12)%101)-50,static_cast<unsigned long>((seed>>32)%37+1)};};
    for(std::size_t n=0;n<96;++n) {
        auto next=e;next.sequence=n+1;
        for(auto& t:next.tensors) {
            if(t.role==TemporalTensorRole::Field) {std::vector<numeric::SparseRationalEntry> values;for(std::size_t i=0;i<t.tensor.domain().logical_size();++i)values.push_back({i,q()});t.tensor={t.tensor.domain(),t.tensor.support(),numeric::SparseRationalVector(t.tensor.domain().logical_size(),values)};}
            else if(n%3==0) {auto m=temporal_tensor_matrix(t);for(std::size_t i=0;i<m.rows();++i)for(std::size_t j=0;j<m.cols();++j)m.set(i,j,q());t.tensor=temporal_matrix_tensor(m);}
        }
        d.advance(make_temporal_segment(e,next));(void)verify(d,oracle?"random_"+std::to_string(n):"");e=std::move(next);
    }
}
void failures() {
    NativeAccessionDirectory d(endpoint(),77);const auto a=analyze(d);const auto op=prepare_parallel_field(a,1);auto workspace=make_parallel_workspace(op);
    runtime::CpuTileExecutor executor({4,4,10});const auto saved=execute_parallel(op,workspace,executor);
    auto other=make_parallel_workspace(prepare_parallel_field(a,1));rejects([&]{(void)execute_parallel(op,other,executor);},"foreign workspace accepted");
    const auto copy=op;check(execute_parallel(copy,workspace,executor).values()==saved.values(),"operation copy changed workspace identity");
    auto moved=std::move(workspace);rejects([&]{(void)execute_parallel(op,workspace,executor);},"moved workspace accepted");workspace=std::move(moved);
    rejects([&]{runtime::CpuTileExecutor bad({0,4,10});},"zero workers accepted");
    rejects([&]{runtime::CpuTileExecutor bad({5,4,10});},"worker cap ignored");
    rejects([&]{(void)prepare_parallel_field(a,0);},"zero tile rows accepted");
    runtime::CpuTileExecutor tiny({2,2,1});rejects([&]{(void)execute_parallel(op,workspace,tiny);},"tile cap ignored");
    auto host=runtime::ExecutionHostPolicy{};host.limits.rational_slots=op.report().requirements.rational_slots-1;
    rejects([&]{(void)make_parallel_workspace(op,runtime::ExecutionLease(host));},"workspace caps ignored");
    rejects([&]{(void)execute_parallel(op,workspace,executor,runtime::ExecutionLease(host));},"execution caps ignored");
    host={};host.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{(void)execute_parallel(op,workspace,executor,runtime::ExecutionLease(host));},"required unavailable GPU accepted");
    host.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(execute_parallel(op,workspace,executor,runtime::ExecutionLease(host)).provenance().fallback,"explicit fallback lost");
    for(const auto stage:{ParallelFieldStage::BeforeLaunch,ParallelFieldStage::BeforeTile,ParallelFieldStage::AfterTile,ParallelFieldStage::BeforeResult}) {
        std::atomic_size_t callbacks{};
        rejects([&]{(void)execute_parallel(op,workspace,executor,runtime::ExecutionLease{},[&](auto s,auto tile,auto worker) {
            if(s==ParallelFieldStage::BeforeTile)++callbacks;
            if(s==stage && (stage!=ParallelFieldStage::BeforeLaunch || worker==2) && (stage!=ParallelFieldStage::AfterTile || tile==0))throw std::runtime_error("injected tile/launch failure");
        });},"failure cut ignored");
        if(stage==ParallelFieldStage::BeforeLaunch)check(callbacks.load()==0,"tile started before all launches succeeded");
        check(execute_parallel(op,workspace,executor).values()==saved.values(),"failed work poisoned retry/workspace");
    }
    auto stop=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease lease({},stop);
    rejects([&]{(void)execute_parallel(op,workspace,executor,lease);},"pre-cancellation ignored");
    for(const auto stage:{ParallelFieldStage::BeforeTile,ParallelFieldStage::AfterTile,ParallelFieldStage::BeforeResult}) {
        stop->store(false);rejects([&]{(void)execute_parallel(op,workspace,executor,lease,[&](auto s,auto,auto) {if(s==stage)stop->store(true);});},"late cancellation ignored");
        stop->store(false);check(execute_parallel(op,workspace,executor).values()==saved.values(),"cancelled work poisoned retry");
    }
    bool reentered{};
    (void)execute_parallel(op,workspace,executor,runtime::ExecutionLease{},[&](auto stage,auto tile,auto) {
        if(stage==ParallelFieldStage::BeforeTile && tile==0) {
            runtime::CpuTileExecutor another({1,1,10});
            rejects([&]{(void)execute_parallel(op,workspace,another);},"workspace callback reentry accepted");
            rejects([&]{(void)executor.run_tiles(1,[](auto,auto,const auto&){});},"executor callback reentry accepted");reentered=true;
        }
    });check(reentered,"reentry not exercised");
    core::RelationalComplexBuilder builder(core::RelationSignature{});builder.declare_grade(core::Grade{1});NativeAccessionDirectory empty(TemporalEndpoint(builder.freeze()),88);
    const auto none=prepare_parallel_field(analyze(empty,true),1);auto ws=make_parallel_workspace(none);runtime::CpuTileExecutor zero({4,4,0});
    const auto y=execute_parallel(none,ws,zero);check(y.values().empty() && y.execution().worker_slots==0 && y.execution().tiles==0,"empty output launched or reduced incorrectly");
    const auto survivor=[] {
        NativeAccessionDirectory temporary(endpoint(),99);
        return prepare_parallel_field(analyze(temporary),1);
    }();auto pinned=make_parallel_workspace(survivor);
    check(execute_parallel(survivor,pinned,executor).values()==saved.values(),"operation borrowed destroyed directory");
    bool pressure{};
    try { (void)execute_parallel(op,workspace,executor,runtime::ExecutionLease{},[](auto stage,auto,auto) {
        if(stage==ParallelFieldStage::BeforeResult)throw std::bad_alloc();
    }); } catch(const std::bad_alloc&) { pressure=true; }
    check(pressure && execute_parallel(op,workspace,executor).values()==saved.values(),"allocation failure did not preserve/release scratch");
    runtime::CpuTileExecutor deterministic({3,3,3});std::barrier together(3);std::string message;
    try { (void)deterministic.run_tiles(3,[&](auto tile,auto,const auto&) {
        together.arrive_and_wait();throw std::runtime_error(std::to_string(tile));
    }); } catch(const std::runtime_error& error) { message=error.what(); }
    check(message=="0","joined errors did not select lowest recorded tile");
    std::atomic_size_t entered{};bool startup{};
    try { (void)deterministic.run_tiles(3,[&](auto,auto,const auto&){++entered;},[](auto worker) {
        if(worker==2)throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
    }); } catch(const std::system_error&) { startup=true; }
    check(startup && entered.load()==0,"startup error started tasks or changed exception");
    numeric::RationalVector input(a.binding().source_dimension());
    const auto missing=[](auto,const auto&){};
    auto local_scratch=op.ranges().make_workspace();
    rejects([&]{(void)op.ranges().apply(input,local_scratch,missing,op.report().requirements.integer_bits,[] {},[] {});},"incomplete synchronous runner accepted");
    std::weak_ptr<const TemporalEndpoint> source_pin;std::optional<ParallelNativeFieldWorkspace> scratch_pin;
    {
        NativeAccessionDirectory owner(endpoint(),991);const auto prepared=prepare_parallel_field(analyze(owner),1);
        scratch_pin.emplace(make_parallel_workspace(prepared));source_pin=prepared.analysis().source_publication();
    }
    check(!source_pin.expired() && scratch_pin->source_publication()==source_pin.lock(),"workspace lost complete source after operation destruction");
    scratch_pin.reset();check(source_pin.expired(),"unowned workspace source not released");
    auto owned=saved;const auto retained=std::move(owned);rejects([&]{(void)owned.values();},"moved result accepted");check(retained.values()==saved.values(),"moved result lost source");
}
void threads() {
    NativeAccessionDirectory d(endpoint(),77);const auto op=prepare_parallel_field(analyze(d),1);auto ws=make_parallel_workspace(op);
    runtime::CpuTileExecutor executor({4,4,10});std::barrier start(4);std::array<std::atomic_bool,4> observed{};
    const auto y=execute_parallel(op,ws,executor,runtime::ExecutionLease{},[&](auto s,auto tile,auto worker) {
        if(s==ParallelFieldStage::BeforeTile && tile<4) {observed.at(worker).store(true);start.arrive_and_wait();}
    });for(const auto& seen:observed)check(seen.load(),"four actual workers did not overlap");
    std::promise<void> reached,release;auto go=release.get_future();std::exception_ptr failure;
    std::thread producer([&]{try{(void)execute_parallel(op,ws,executor,runtime::ExecutionLease{},[&](auto s,auto tile,auto) {
        if(s==ParallelFieldStage::BeforeTile && tile==0){reached.set_value();go.wait();}
    });}catch(...){failure=std::current_exception();}});
    reached.get_future().wait();runtime::CpuTileExecutor alternate({2,2,10});auto separate=make_parallel_workspace(op);
    rejects([&]{(void)execute_parallel(op,ws,alternate);},"concurrent workspace lease accepted");
    rejects([&]{(void)execute_parallel(op,separate,executor);},"concurrent executor lease accepted");
    release.set_value();producer.join();if(failure)std::rethrow_exception(failure);
    std::array<std::exception_ptr,4> errors{};std::array<std::thread,4> callers;
    for(std::size_t i=0;i<callers.size();++i)callers[i]=std::thread([&,i]{try{
        auto workspace=make_parallel_workspace(op);runtime::CpuTileExecutor cpu({2,2,10});
        for(std::size_t n=0;n<20;++n)if(execute_parallel(op,workspace,cpu).values()!=y.values())throw std::runtime_error("parallel callers changed values");
    }catch(...){errors[i]=std::current_exception();}});
    for(auto& caller:callers)caller.join();
    for(const auto& e:errors)if(e)std::rethrow_exception(e);
    check(true,"separate concurrent workspaces failed");
    std::cout<<"threads: four overlapping owner workers; rejected overlapping leases; 80 separate-workspace executions\n";
}
} // namespace
int main(int argc,char** argv) {
    try {const bool oracle=argc==2 && std::string(argv[1])=="--oracle";positive(oracle);random(oracle);if(!oracle){failures();threads();}std::cout<<"checks "<<checks<<'\n';return 0;}
    catch(const std::exception& e){std::cerr<<"Parallel field: "<<e.what()<<'\n';return 1;}
}
