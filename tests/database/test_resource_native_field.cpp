#include "rfc/database/resource_native_field.hpp"
#include "native_execution_fixture.hpp"
#include <barrier>
#include <iostream>
#include <thread>

namespace {
using namespace native_execution_fixture;
using namespace rfc::database;
std::size_t checks{};
void check(bool value,const char* message) { ++checks;if(!value)throw std::runtime_error(message); }
template<class F>void rejects(F&& function,const char* message) {
    bool rejected{};try {function();}catch(const std::exception&) {rejected=true;}check(rejected,message);
}
runtime::HardwareInventory hardware() {
    runtime::HardwareInventory h;h.platform="Linux";h.cpu_model="declared test host";h.affinity_observed=true;
    for(std::size_t i=0;i<4;++i)h.allowed_cpus.push_back({i,0,i,{"test-flags"}});
    return h;
}
NativeFieldAnalysis analyze(const NativeAccessionDirectory& d,bool scalar=false,core::Grade grade=core::Grade{1}) {
    return NativeFieldAnalysis::inspect(anchor(d,grade),request(d,grade),
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
    const auto a=analyze(d,scalar,grade);const auto h=hardware();
    const auto reference=execute(prepare_field_action(anchor(d,grade),request(d,grade),
        scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{}));
    std::optional<DerivedParallelNativeField> saved;
    for(const auto rows:{1u,2u,8u}) {
        const auto op=prepare_parallel_field(a,rows);auto workspace=make_parallel_workspace(op);
        for(const auto capacity:{1u,2u,4u}) {
            runtime::CpuResourceCoordinator coordinator(h,{capacity,0,100});
            CpuFieldCalibrationPolicy policy;policy.worker_candidates={1};policy.samples=2;policy.warmups=0;
            if(capacity>=2)policy.worker_candidates.push_back(2);
            if(capacity>=4)policy.worker_candidates.push_back(4);
            const auto tuning=calibrate_cpu_field(op,workspace,coordinator,h,policy);
            check(tuning.retained_occurrences()==a.layout().retained_occurrences && tuning.output_coordinates()==a.layout().target_coordinates,
                "calibration lost incidence/output contract");
            for(const auto& timing:tuning.timings())check(timing.nanosecond_samples.size()==2 && timing.workers<=capacity,"calibration accounting incorrect");
            const auto result=execute_scheduled_field(op,workspace,coordinator,h,tuning);
            check(result.field.values()==reference.values(),"scheduled exact values differ from fresh scalar reference");
            check(result.field.source_publication()==reference.source_publication(),"scheduled source owner changed");
            check(result.field.provenance().sequence==d.sequence(),"scheduled source sequence changed");
            check(result.field.carriers().size()==reference.carriers().size(),"scheduled output support changed");
            for(std::size_t i=0;i<result.field.carriers().size();++i) {
                const auto& x=result.field.carriers()[i];const auto& y=reference.carriers()[i];
                check(x.cell==y.cell && x.carrier==y.carrier && x.coordinates==y.coordinates && x.dimension==y.dimension && x.offset==y.offset,
                    "scheduled carrier meaning changed");
            }
            const auto best=std::min_element(tuning.timings().begin(),tuning.timings().end(),[](const auto& x,const auto& y){
                return std::pair{x.median_nanoseconds,x.workers}<std::pair{y.median_nanoseconds,y.workers};});
            check(result.selected_workers==best->workers && result.calibrated_median_nanoseconds==best->median_nanoseconds,
                "scheduler did not select lowest measured median among admitted candidates");
            check(coordinator.available_workers()==coordinator.capacity(),"successful scheduled call leaked workers");
            if(!name.empty())emit(name+(capacity==1 && rows==1?"":"@w"+std::to_string(capacity)+"r"+std::to_string(rows)),result.field,a);
            saved=result.field;
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
    const auto h=hardware();runtime::CpuResourceCoordinator service(h,{4,0,100});
    NativeAccessionDirectory d(endpoint(),77);const auto op=prepare_parallel_field(analyze(d),1);
    auto workspace=make_parallel_workspace(op);CpuFieldCalibrationPolicy p{{1,2,4},3,1,64};
    const auto tuning=calibrate_cpu_field(op,workspace,service,h,p);
    const auto old=execute_scheduled_field(op,workspace,service,h,tuning).field;
    const auto copy=op;check(execute_scheduled_field(copy,workspace,service,h,tuning).field.values()==old.values(),"copied owner lost calibration identity");
    const auto four_tuning=calibrate_cpu_field(op,workspace,service,h,{{4},1,0,3});std::atomic_size_t entered{};
    rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,four_tuning,runtime::ExecutionLease{},
        [&](auto stage,auto,auto worker) {
            if(stage==ParallelFieldStage::BeforeTile)++entered;
            if(stage==ParallelFieldStage::BeforeLaunch && worker==2)throw std::runtime_error("launch cut");
        });},"startup failure ignored");
    check(entered.load()==0 && service.available_workers()==4,"startup failure started tiles or leaked reservations");
    const auto owned_result=execute_scheduled_field(op,workspace,service,h,tuning);
    check(owned_result.calibration.hardware_facts()==runtime::cpu_execution_facts(h) &&
        owned_result.resource_policy.worker_limit==4,"scheduled result lost owning calibration/resource report");
    auto temporary_tuning=tuning;const auto moved_tuning=std::move(temporary_tuning);
    rejects([&]{(void)temporary_tuning.timings();},"moved calibration accepted");
    check(moved_tuning.timings().size()==tuning.timings().size(),"calibration move lost owned timing records");
    const auto other=prepare_parallel_field(analyze(d),1);auto other_workspace=make_parallel_workspace(other);
    rejects([&]{(void)execute_scheduled_field(other,other_workspace,service,h,tuning);},"separately prepared equal owner accepted calibration");
    auto changed=h;changed.allowed_cpus.pop_back();
    rejects([&]{(void)execute_scheduled_field(op,workspace,service,changed,tuning);},"changed affinity accepted calibration");
    changed=h;changed.cpu_model="different host";runtime::CpuResourceCoordinator other_service(changed,{4,0,100});
    rejects([&]{(void)execute_scheduled_field(op,workspace,other_service,changed,tuning);},"changed hardware accepted calibration");
    {
        auto occupied=service.reserve(3);
        const auto one=execute_scheduled_field(op,workspace,service,h,tuning);
        check(one.selected_workers==1 && one.field.values()==old.values(),"occupied service did not use measured one-worker alternative");
    }
    {
        auto occupied=service.reserve(4);
        rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,tuning);},"occupied worker budget ignored");
    }
    auto host=runtime::ExecutionHostPolicy{};host.limits.integer_bits=1;
    rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,tuning,runtime::ExecutionLease(host));},"current bit limits ignored");
    host={};host.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,tuning,runtime::ExecutionLease(host));},"unavailable GPU dispatched");
    host.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(execute_scheduled_field(op,workspace,service,h,tuning,runtime::ExecutionLease(host)).field.provenance().fallback,"explicit CPU fallback not recorded");
    auto cancel=std::make_shared<std::atomic_bool>(true);
    rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,tuning,runtime::ExecutionLease({},cancel));},"pre-cancel ignored");
    cancel->store(false);
    rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,tuning,runtime::ExecutionLease({},cancel),
        [&](auto stage,auto,auto){if(stage==ParallelFieldStage::BeforeTile)cancel->store(true);});},"mid-call cancellation ignored");
    cancel->store(false);check(service.available_workers()==4,"cancel leaked CPU reservation");
    for(const auto stage:{ParallelFieldStage::BeforeTile,ParallelFieldStage::BeforeResult}) {
        rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,tuning,runtime::ExecutionLease{},
            [&](auto at,auto,auto){if(at==stage)throw std::bad_alloc{};});},"failure cut ignored");
        check(service.available_workers()==4 && execute_scheduled_field(op,workspace,service,h,tuning).field.values()==old.values(),
            "failure leaked budget or poisoned workspace");
    }
    runtime::CpuResourceCoordinator tiny(h,{4,0,1});
    rejects([&]{(void)execute_scheduled_field(op,workspace,tiny,h,tuning);},"current tile budget ignored");
    auto invalid=p;invalid.worker_candidates={0};rejects([&]{(void)calibrate_cpu_field(op,workspace,service,h,invalid);},"zero calibration workers accepted");
    invalid=p;invalid.worker_candidates={5};rejects([&]{(void)calibrate_cpu_field(op,workspace,service,h,invalid);},"calibration workers exceeded capacity");
    invalid=p;invalid.samples=0;rejects([&]{(void)calibrate_cpu_field(op,workspace,service,h,invalid);},"empty calibration accepted");
    invalid=p;invalid.call_limit=1;rejects([&]{(void)calibrate_cpu_field(op,workspace,service,h,invalid);},"calibration call bound ignored");
    rejects([&]{(void)calibrate_cpu_field(op,other_workspace,service,h,p);},"foreign workspace accepted calibration");
    check(service.available_workers()==4,"failed calibration leaked workers");
    auto moved=std::move(workspace);rejects([&]{(void)execute_scheduled_field(op,workspace,service,h,tuning);},"moved workspace accepted scheduler");workspace=std::move(moved);
    std::barrier together(2);std::atomic_size_t inside{},peak{};std::vector<std::jthread> threads;
    runtime::CpuResourceCoordinator simultaneous(h,{2,0,100});
    const auto concurrent_tuning=calibrate_cpu_field(op,workspace,simultaneous,h,{{1},1,0,4});
    for(std::size_t i=0;i<2;++i)threads.emplace_back([&]{
        auto private_workspace=make_parallel_workspace(op);
        const auto result=execute_scheduled_field(op,private_workspace,simultaneous,h,concurrent_tuning,runtime::ExecutionLease{},
            [&](auto stage,auto tile,auto){if(stage==ParallelFieldStage::BeforeTile && tile==0) {
                const auto n=inside.fetch_add(1)+1;auto previous=peak.load();while(previous<n && !peak.compare_exchange_weak(previous,n)){}
                together.arrive_and_wait();inside.fetch_sub(1);
            }});
        if(result.field.values()!=old.values())throw std::runtime_error("concurrent scheduled field changed");
    });
    threads.clear();check(peak.load()==2 && simultaneous.available_workers()==2,"two coordinated field tasks did not run concurrently");
    std::optional<PreparedParallelNativeField> retained_operation;std::optional<CpuFieldCalibration> retained_calibration;
    {
        NativeAccessionDirectory temporary(endpoint(),900);retained_operation.emplace(prepare_parallel_field(analyze(temporary),1));
        auto private_workspace=make_parallel_workspace(*retained_operation);
        retained_calibration.emplace(calibrate_cpu_field(*retained_operation,private_workspace,service,h,p));
    }
    auto retained_workspace=make_parallel_workspace(*retained_operation);
    check(execute_scheduled_field(*retained_operation,retained_workspace,service,h,*retained_calibration).field.values()==old.values(),
        "calibration/operation did not retain source after directory destruction");
}
} // namespace
int main(int argc,char** argv) {
    try {const bool oracle=argc>1 && std::string(argv[1])=="--oracle";positive(oracle);random(oracle);
        if(!oracle)failures();
        std::cout<<"checks "<<checks<<'\n';return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
