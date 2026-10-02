#include "native_execution_fixture.hpp"
#include <atomic>
#include <iostream>
#include <limits>
#include <sstream>
#include <type_traits>

using namespace native_execution_fixture;
using namespace rfc::database;
static_assert(std::is_same_v<decltype(std::declval<DerivedNativeField&&>().values()),numeric::RationalVector>);
static_assert(std::is_same_v<decltype(std::declval<DerivedNativeField&&>().state(StableCellKey{},0)),Q>);
static_assert(!std::is_constructible_v<PreparedNativeField,numeric::RationalVector&>);
static_assert(!std::is_copy_constructible_v<NativeFieldWorkspace>);
static_assert(!std::is_default_constructible_v<NativeFieldWorkspace>);
namespace {
std::size_t checks = 0;
void check(bool ok,const char* message) { ++checks; if(!ok) throw std::runtime_error(message); }
template<class F> void rejects(F&& f,const char* message) {
    ++checks;try { f(); } catch(const std::exception&) { return; } throw std::runtime_error(message);
}
PreparedNativeField prepare(const NativeAccessionDirectory& d, bool scalar=false,core::Grade grade=core::Grade{1}) {
    auto op=prepare_field_action(anchor(d,grade),request(d,grade),scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{});
    auto workspace=make_workspace(op);
    const auto reference=execute(op);
    for(std::size_t repetition=0;repetition<3;++repetition) {
        const auto compiled=execute_prepared(op,workspace);
        check(compiled.values()==reference.values(),"prepared action differs from reference");
        check(compiled.source_publication()==reference.source_publication(),"prepared action changed source owner");
        check(compiled.carriers().size()==reference.carriers().size(),"prepared action changed complete support");
    }
    return op;
}
void emit(const char* name,const TemporalEndpoint& e,const PreparedNativeField& op) {
    auto workspace=make_workspace(op);
    const auto result=execute_prepared(op,workspace);
    std::cout<<"case "<<name<<'\n';
    for(const auto& b:op.binding().inputs()) {
        std::cout<<"upper "<<b.cell.key<<' '<<b.dimension<<'\n';
        const auto t=temporal_endpoint_detail::tensors(e).at(b.field);
        for(std::size_t i=0;i<b.dimension;++i)std::cout<<"x "<<b.cell.key<<' '<<i<<' '<<*t->tensor.state(i)<<'\n';
    }
    for(const auto& b:result.carriers()) {
        std::cout<<"lower "<<b.cell.key<<' '<<b.dimension<<'\n';
        for(std::size_t i=0;i<b.dimension;++i)std::cout<<"y "<<b.cell.key<<' '<<i<<' '<<result.state(b.cell,i)<<'\n';
    }
    const auto tensors=temporal_endpoint_detail::tensors(e);
    for(const auto& b:op.binding().occurrences()) {
        std::cout<<"occ "<<b.occurrence.key.value<<' '<<b.upper.key<<' '<<b.lower.key<<' '<<b.active<<' '<<b.coefficient<<' '
                 <<(b.map?(b.role==TemporalTensorRole::RealizedIncidenceMap?"realized":"independent"):"constant")<<'\n';
        if(b.map) {
            const auto matrix=temporal_tensor_matrix(*tensors.at(*b.map));
            for(const auto& [ij,q]:matrix.entries())std::cout<<"m "<<b.occurrence.key.value<<' '<<ij.first<<' '<<ij.second<<' '<<q<<'\n';
        }
    }
    std::cout<<"end\n";
}
void positive(bool oracle) {
    auto e=endpoint();NativeAccessionDirectory d(e,77);
    auto op=prepare(d);const auto y=execute(op);
    check(op.binding().occurrences().size()==4,"retained occurrences lost");
    check(!op.binding().occurrences()[3].active,"inactive occurrence activated");
    for(std::size_t i=0;i<4;++i)check(op.binding().occurrences()[i].coefficient==std::array<Q,4>{Q{2},Q{-3},Q{-2},Q{0}}[i],"source coefficient changed");
    check(op.binding().inputs().size()==2 && op.binding().inputs()[1].field==2102,"empty relation field lost");
    check(y.carriers().size()==2 && y.values().size()==5,"heterogeneous lower support changed");
    check(y.state({core::Grade{0},10},0)==0 && y.state({core::Grade{0},10},1)==0,"repeated contribution cancellation failed");
    check(y.state({core::Grade{0},11},0)==Q{-7,11},"rectangular exact output mismatch");
    check(y.source_publication().get()==d.pin_snapshot().get(),"result did not pin publication");
    check(y.provenance().source_namespace==77 && y.provenance().decisions.size()==8,"resolved provenance missing");
    if(oracle)emit("independent",e,op);
    auto r=endpoint(false,true);NativeAccessionDirectory rd(r,77);auto rop=prepare(rd);
    check(execute(rop).values()==y.values(),"realized map coefficient applied twice");
    if(oracle)emit("realized",r,rop);
    auto s=endpoint(true);NativeAccessionDirectory sd(s,77);auto sop=prepare(sd,true);
    check(execute(sop).values()==numeric::RationalVector({Q{0},Q{-21,11}}),"named scalar constant fiber mismatch");
    if(oracle)emit("scalar",s,sop);
    auto z=endpoint(false,false,true);
    for(auto& t:z.tensors)if(t.role==TemporalTensorRole::CoefficientIndependentMap)
        t.tensor=numeric::SparseRationalTensor::zero(t.tensor.domain(),numeric::StructuralSupport::full(t.tensor.domain().logical_size()));
    NativeAccessionDirectory zd(z,77);auto zop=prepare(zd);
    check(execute(zop).values()==numeric::RationalVector(5,Q{0}),"supported zero field/map changed output support");
    if(oracle)emit("zero",z,zop);
    auto zero_map=endpoint();
    for(auto& t:zero_map.tensors)if(t.role==TemporalTensorRole::CoefficientIndependentMap)
        t.tensor=numeric::SparseRationalTensor::zero(t.tensor.domain(),numeric::StructuralSupport::full(t.tensor.domain().logical_size()));
    NativeAccessionDirectory zm(zero_map,77);auto zmop=prepare(zm);
    check(execute(zmop).values()==numeric::RationalVector(5,Q{0}),"explicit zero maps did not retain nonzero input and complete output support");
    if(oracle)emit("zero_map",zero_map,zmop);
    auto empty=prepare(d,false,core::Grade{2});
    check(execute(empty).values()==numeric::RationalVector(4,Q{0}),"explicit empty positive grade failed");
    if(oracle)emit("empty",e,empty);
    auto wide=endpoint();numeric::Integer n=1;mpz_mul_2exp(n.get_mpz_t(),n.get_mpz_t(),521);n+=17;
    wide.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q(n,numeric::Integer{37})},{1,Q{13,17}}})};
    auto wm=temporal_tensor_matrix(wide.tensors[3]);wm.set(2,0,Q(n,numeric::Integer{41}));wide.tensors[3].tensor=temporal_matrix_tensor(wm);
    NativeAccessionDirectory wd(wide,77);auto wop=prepare(wd);const auto wy=execute(wop);
    check(wy.observed_integer_bits()>1000,"wide exact growth was truncated");
    check(wy.observed_integer_bits()<=wop.report().requirements.integer_bits,"growth estimate too small");
    if(oracle)emit("wide",wide,wop);
}
void lifetime_and_publication() {
    auto e=endpoint();auto* alias=e.tensors.data();NativeAccessionDirectory d(std::move(e),77);
    auto req=request(d);auto* selected_alias=req.stalk_carriers.data();const auto op=prepare_field_action(anchor(d),std::move(req));
    const auto expected=execute(op).values();
    alias[0].tensor=numeric::SparseRationalTensor::zero(alias[0].tensor.domain(),numeric::StructuralSupport::full(2));*selected_alias=999;
    check(execute(op).values()==expected,"caller vector alias mutated bound operation");
    const auto h=d.intern(field_request(2101));auto next=*d.pin_snapshot();next.sequence=1;
    next.tensors[0].tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2));
    const auto before_bytes=encode_temporal_endpoint(*op.binding().endpoint());
    d.advance(make_temporal_segment(*d.pin_snapshot(),next));
    rejects([&]{d.resolve(h);},"stale handle accepted after publication");
    check(execute(prepare(d)).values()!=expected,"new field publication ignored");
    check(execute(op).values()==expected && op.report().sequence==0,"old prepared operation rebound");
    check(encode_temporal_endpoint(*op.binding().endpoint())==before_bytes,"execution changed canonical source");
    auto copied=op;auto moved=std::move(copied);
    check(execute(copied).values()==execute(moved).values(),"moved operation invalidated pinned aliases");
    const auto escaped=[] {auto t=endpoint();NativeAccessionDirectory local(t,90);return prepare(local);}();
    check(execute(escaped).values()==expected,"operation did not outlive source/directory");
    const auto result=[] {auto t=endpoint();NativeAccessionDirectory local(t,90);return execute(prepare(local));}();
    check(result.values()==expected && result.source_publication()->source.grade_count()==3,"result owner lifetime failed");
    const auto pin=d.pin_snapshot();const auto stats=d.stats();auto bad=request(d);bad.fields.pop_back();
    rejects([&]{prepare_field_action(anchor(d),bad);},"incomplete field ownership accepted");
    check(d.pin_snapshot()==pin && d.stats().cold_bindings==stats.cold_bindings,"failed preparation changed publication or directory");
    const auto bytes=encode_temporal_endpoint(next);
    auto disk=decode_temporal_endpoint(next.source,bytes);NativeAccessionDirectory diskdir(disk,77);
    check(execute(prepare(diskdir)).values()==execute(prepare(d)).values(),"endpoint codec changed prepared action");
    check(encode_temporal_endpoint(disk)==bytes,"canonical endpoint bytes changed");
}
void negative() {
    auto e=endpoint();NativeAccessionDirectory d(e,77);
    auto req=request(d);auto a=anchor(d);
    auto nonfield=req;nonfield.fields[0]=a;rejects([&]{prepare_field_action(a,nonfield);},"non-field accession admitted as a rational field");
    auto missing_stalk=req;missing_stalk.stalk_carriers.pop_back();rejects([&]{prepare_field_action(a,missing_stalk);},"retained stalk owner silently defaulted");
    auto badgrade=req;badgrade.upper_grade=core::Grade{0};rejects([&]{prepare_field_action(a,badgrade);},"grade zero admitted as downward field action");
    for(auto domain:{NativeFieldDomain::Finite,NativeFieldDomain::Modular,NativeFieldDomain::Approximate,static_cast<NativeFieldDomain>(999)}) {
        NativeFieldRealization r;r.domain=domain;rejects([&]{prepare_field_action(a,req,r);},"unsupported arithmetic accepted");
    }
    NativeFieldRealization unknown;unknown.maps=static_cast<NativeFieldMaps>(999);
    rejects([&]{prepare_field_action(a,req,unknown);},"unknown realization accepted");
    auto other=endpoint();NativeAccessionDirectory od(other,77);auto foreign=req;foreign.fields[0]=request(od).fields[0];
    rejects([&]{prepare_field_action(a,foreign);},"equal values from different publications accepted");
    auto wrong=endpoint();wrong.tensors[0].owner.cell={core::Grade{1},102};NativeAccessionDirectory wrongd(wrong,77);
    rejects([&]{prepare(wrongd);},"duplicate/wrong field owner accepted");
    auto wrongcarrier=e;wrongcarrier.carriers.push_back({999,TemporalCarrierKind::CellStalk,core::Grade{1},StableCellKey{core::Grade{1},101},123456,numeric::IndexDomain::linear(2)});
    wrongcarrier.tensors[0].output=999;NativeAccessionDirectory wc(wrongcarrier,77);auto wcr=request(wc);std::erase(wcr.stalk_carriers,999);
    rejects([&]{prepare_field_action(anchor(wc),wcr);},"equal dimension wrong carrier accepted");
    auto partial=e;partial.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::sparse(2,{1}),numeric::SparseRationalVector(2,{{1,Q{13,17}}})};
    NativeAccessionDirectory pd(partial,77);rejects([&]{prepare(pd);},"partial field silently extended by zero");
    auto partialmap=e;partialmap.tensors[2].tensor={numeric::IndexDomain({2,2}),numeric::StructuralSupport::sparse(4,{0}),numeric::SparseRationalVector(4,{{0,Q{5}}})};
    NativeAccessionDirectory pmd(partialmap,77);rejects([&]{prepare(pmd);},"partial map silently completed");
    for(auto missing:std::array<std::size_t,2>{3,5}) {
        auto z=endpoint(false,false,true);z.tensors.erase(z.tensors.begin()+static_cast<std::ptrdiff_t>(missing));
        NativeAccessionDirectory zd(z,77);rejects([&]{prepare(zd);},"missing active rectangular/inactive map accepted for zero input");
    }
    auto scalar=endpoint(true);scalar.carriers[0].coordinate_system=123;NativeAccessionDirectory sd(scalar,77);
    rejects([&]{prepare(sd,true);},"equal sizes identified incompatible coordinates");
    auto scalar_ok=endpoint(true);NativeAccessionDirectory sk(scalar_ok,77);
    rejects([&]{prepare(sk);},"implicit constant-fiber interpretation accepted");
    auto oversized=endpoint(true,true,true);
    const auto maximum=std::numeric_limits<std::size_t>::max();
    for(auto& c:oversized.carriers)if(c.key==102)c.domain=numeric::IndexDomain::linear(maximum);
    for(auto& t:oversized.tensors)if(t.key==2102)t.tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(maximum),numeric::StructuralSupport::full(maximum));
    NativeAccessionDirectory huge(oversized,77);rejects([&]{prepare(huge,true);},"unchecked extent addition overflow accepted");
    rejects([&]{auto stale=field_request(999999);d.pin(stale);},"stale tensor key accepted");
    auto invalid=e;invalid.tensors[2].owner.occurrence.key=core::OccurrenceKey{999};
    rejects([&]{NativeAccessionDirectory bad(invalid,77);},"wrong occurrence key admitted");
}
void policies() {
    auto e=endpoint();NativeAccessionDirectory d(e,77);const auto op=prepare(d);const auto req=request(d);const auto a=anchor(d);
    runtime::ExecutionHostPolicy policy;policy.limits=op.report().requirements;
    check(execute(prepare_field_action(a,req,{},policy),runtime::ExecutionLease(policy)).values()==execute(op).values(),"at-requirement admission failed");
    const std::array<std::size_t runtime::ExecutionRequirements::*,5> members={
        &runtime::ExecutionRequirements::records,&runtime::ExecutionRequirements::rational_slots,
        &runtime::ExecutionRequirements::sparse_entries,&runtime::ExecutionRequirements::scalar_work,&runtime::ExecutionRequirements::integer_bits};
    for(auto member:members) {
        auto small=policy;--(small.limits.*member);
        rejects([&]{prepare_field_action(a,req,{},small);},"below-requirement preparation admitted");
        rejects([&]{execute(op,runtime::ExecutionLease(small));},"below-requirement execution admitted");
    }
    policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{prepare_field_action(a,req,{},policy);},"required absent GPU silently fell back");
    rejects([&]{execute(op,runtime::ExecutionLease(policy));},"execution required GPU silently fell back");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=prepare_field_action(a,req,{},policy);
    check(fallback.report().fallback && execute(fallback,runtime::ExecutionLease(policy)).provenance().fallback,"explicit CPU fallback missing provenance");
    policy.backend=static_cast<runtime::ExecutionBackend>(999);rejects([&]{prepare_field_action(a,req,{},policy);},"unknown backend accepted");
    policy={};policy.fallback=static_cast<runtime::BackendFallback>(999);rejects([&]{prepare_field_action(a,req,{},policy);},"unknown fallback accepted");
    policy={};policy.allow_allocation=false;rejects([&]{prepare_field_action(a,req,{},policy);},"allocation ban ignored");
    rejects([&]{execute(op,runtime::ExecutionLease(policy));},"lease allocation ban ignored");
    auto cancel=std::make_shared<std::atomic_bool>(true);const auto pin=d.pin_snapshot();
    rejects([&]{execute(op,runtime::ExecutionLease({},cancel));},"cancelled execution returned result");
    check(pin==d.pin_snapshot(),"failed execution changed publication");cancel->store(false);
    check(execute(op,runtime::ExecutionLease({},cancel)).values()==execute(op).values(),"cancelled execution poisoned bound operation");
    NativeAccessionLimits inherited;inherited.max_action_work=1;
    NativeAccessionDirectory bounded(e,77,{},[](auto b){return rfcx_detail::fnv1a64(b);},inherited);
    rejects([&]{prepare(bounded);},"field execution bypassed inherited accession work limit");
}
void prepared_layout_and_workspace() {
    auto e=endpoint();NativeAccessionDirectory d(e,77);const auto op=prepare(d);
    const auto& kernel=op.kernel();
    check(kernel.blocks().size()==4 && !kernel.blocks()[3].active,"compiled layout lost inactive block");
    check(kernel.statistics().active_occurrences==3,"compiled activity population changed");
    std::vector<bool> visited(kernel.entries().size());
    for(std::size_t row=0;row<kernel.statistics().target_dimension;++row)
        for(std::size_t i=kernel.target_offsets()[row];i<kernel.target_offsets()[row+1];++i) {
            const auto physical=kernel.target_order()[i];
            check(!visited.at(physical),"entry duplicated in target schedule");visited[physical]=true;
            check(kernel.entries().at(physical).target==row,"target schedule crossed a coordinate");
            check(kernel.physical_entry(kernel.logical_entry(physical))==physical,"logical/physical entry mapping not reversible");
        }
    check(std::all_of(visited.begin(),visited.end(),[](bool v){return v;}),"target schedule discarded an entry");
    rejects([&]{kernel.physical_entry({3,0,0});},"inactive arithmetic entry synthesized");
    rejects([&]{kernel.physical_entry({0,99,0});},"out-of-domain coordinate admitted");
    auto workspace=make_workspace(op);const auto prior=execute_prepared(op,workspace);
    const auto expected=prior.values();const auto bytes=encode_temporal_endpoint(*d.pin_snapshot());
    auto copy=op;
    check(execute_prepared(copy,workspace).values()==expected,"copied operation rejected its shared workspace");
    const auto another=prepare(d);
    rejects([&]{execute_prepared(another,workspace);},"separately prepared operation accepted foreign scratch");
    NativeAccessionDirectory equal(e,77);const auto foreign=prepare(equal);
    rejects([&]{execute_prepared(foreign,workspace);},"equal bytes established workspace ownership");
    auto moved=std::move(workspace);
    rejects([&]{execute_prepared(op,workspace);},"moved-from workspace admitted");
    check(execute_prepared(op,moved).values()==expected,"moved workspace lost its owner");
    runtime::ExecutionHostPolicy host;host.limits=op.report().requirements;
    const std::array<std::size_t runtime::ExecutionRequirements::*,5> members={
        &runtime::ExecutionRequirements::records,&runtime::ExecutionRequirements::rational_slots,
        &runtime::ExecutionRequirements::sparse_entries,&runtime::ExecutionRequirements::scalar_work,&runtime::ExecutionRequirements::integer_bits};
    for(auto member:members) {
        auto small=host;--(small.limits.*member);
        rejects([&]{execute_prepared(op,moved,runtime::ExecutionLease(small));},"prepared execution bypassed budget");
        rejects([&]{make_workspace(op,runtime::ExecutionLease(small));},"workspace construction bypassed budget");
    }
    host.allow_allocation=false;
    rejects([&]{execute_prepared(op,moved,runtime::ExecutionLease(host));},"prepared result allocation ban ignored");
    host={};host.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{execute_prepared(op,moved,runtime::ExecutionLease(host));},"prepared required GPU silently fell back");
    host.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(execute_prepared(op,moved,runtime::ExecutionLease(host)).provenance().fallback,"prepared fallback provenance lost");
    auto cancel=std::make_shared<std::atomic_bool>(true);
    rejects([&]{execute_prepared(op,moved,runtime::ExecutionLease({},cancel));},"cancelled prepared execution returned a field");
    cancel->store(false);
    check(execute_prepared(op,moved).values()==expected && prior.values()==expected,"failed execution damaged scratch reuse or prior result");
    check(encode_temporal_endpoint(*d.pin_snapshot())==bytes,"prepared execution changed canonical publication");
    check(prior.provenance().implementation=="PreparedGradeFieldAction/GMP/serial-prepared-v1","compiled provenance claimed reference execution");
    auto escaped=[] {NativeAccessionDirectory local(endpoint(),90);auto operation=prepare(local);return std::pair{operation,make_workspace(operation)};}();
    check(execute_prepared(escaped.first,escaped.second).values()==expected,"workspace did not outlive directory/source");
    auto next=e;next.sequence=1;next.tensors[0].tensor=numeric::SparseRationalTensor::zero(next.tensors[0].tensor.domain(),numeric::StructuralSupport::full(2));
    d.advance(make_temporal_segment(e,next));const auto current=prepare(d);
    rejects([&]{execute_prepared(current,moved);},"publication replacement rebound old scratch");
    check(execute_prepared(op,moved).values()==expected,"publication replacement corrupted old prepared schedule");
}
void randomized(bool oracle) {
    std::uint64_t state=0x524643;
    const auto rational=[&]() {
        state=state*6364136223846793005ULL+1;
        const auto numerator=static_cast<long>((state>>16)%41)-20;
        state=state*6364136223846793005ULL+1;
        return Q{numerator,static_cast<unsigned long>((state>>16)%31+1)};
    };
    for(std::size_t trial=0;trial<32;++trial) {
        auto e=endpoint();
        for(auto& t:e.tensors) {
            if(t.role==TemporalTensorRole::Field) {
                const auto size=t.tensor.domain().logical_size();std::vector<numeric::SparseRationalEntry> values;
                for(std::size_t i=0;i<size;++i)values.push_back({i,rational()});
                t.tensor={t.tensor.domain(),numeric::StructuralSupport::full(size),numeric::SparseRationalVector(size,values)};
            } else if(t.role==TemporalTensorRole::CoefficientIndependentMap) {
                const auto shape=t.tensor.domain().extents();numeric::ExactMatrix m(shape[0],shape[1]);
                for(std::size_t row=0;row<shape[0];++row)for(std::size_t col=0;col<shape[1];++col)m.set(row,col,rational());
                t.tensor=temporal_matrix_tensor(m);
            }
        }
        NativeAccessionDirectory d(e,77);const auto op=prepare(d);const auto y=execute(op);
        check(y.observed_integer_bits()<=op.report().requirements.integer_bits,"random exact growth envelope failed");
        if(oracle)emit(("random_"+std::to_string(trial)).c_str(),e,op);
    }
}
void temporal_changes(bool oracle) {
    auto e=endpoint();NativeAccessionDirectory d(e,77);const auto old=prepare(d);const auto y=execute(old).values();
    auto next=e;next.sequence=1;
    next.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{3,5}},{1,Q{-7,23}}})};
    auto m=temporal_tensor_matrix(next.tensors[3]);m.set(1,0,Q{17,29});next.tensors[3].tensor=temporal_matrix_tensor(m);
    auto field_only=e;field_only.sequence=1;field_only.tensors[0]=next.tensors[0];
    auto map_only=e;map_only.sequence=1;map_only.tensors[3]=next.tensors[3];
    NativeAccessionDirectory field_dir(e,77),map_dir(e,77);
    field_dir.advance(make_temporal_segment(e,field_only));map_dir.advance(make_temporal_segment(e,map_only));
    if(oracle) {emit("field_changed",field_only,prepare(field_dir));emit("map_changed",map_only,prepare(map_dir));}
    const auto segment=make_temporal_segment(e,next);d.advance(decode_temporal_segment(encode_temporal_segment(segment)));
    const auto current=prepare(d);const auto ny=execute(current).values();
    const auto eta=temporal_field_innovation(e,next,2101);
    const auto defect=temporal_tensor_map_defect(e,next,4002,{},true);
    numeric::RationalVector x{Q{7,11},Q{13,17}},dense_eta(2,Q{0});
    for(const auto& q:eta.entries())dense_eta[q.index]=q.value;
    const auto dx=defect.apply(x),innovation=temporal_realized_incidence(next,next.tensors[3]).apply(dense_eta);
    for(std::size_t i=0;i<3;++i)check(ny[2+i]-y[2+i]==dx[i]+innovation[i],"finite map/field interaction term missing");
    check(execute(old).values()==y,"temporal publication mutated old prepared source");
    if(oracle)emit("both_changed",next,current);
    auto permuted=e;permuted.sequence=1;
    for(auto& c:permuted.carriers)if(c.key==101)c.coordinate_system=99901;
    permuted.tensors[0].tensor={numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{13,17}},{1,Q{7,11}}})};
    numeric::ExactMatrix P(2,2);P.set(0,1,Q{1});P.set(1,0,Q{1});
    for(auto& t:permuted.tensors)if(t.role==TemporalTensorRole::CoefficientIndependentMap)t.tensor=temporal_matrix_tensor(temporal_tensor_matrix(t).multiply(P));
    const std::vector<TemporalCorrespondence> maps{{101,P}};
    const auto ps=make_temporal_segment(e,permuted,maps);NativeAccessionDirectory perm(e,77);perm.advance(ps);
    const auto pop=prepare(perm);check(execute(pop).values()==y,"explicit coordinate permutation changed action");
    check(temporal_field_innovation(e,permuted,2101,maps).nonzeros()==0,"explicit coordinate correspondence lost");
    if(oracle)emit("permuted",permuted,pop);
    composite::AspectSchema aspects;SourceDeclarationMigration migration;migration.base_signature=e.source.signature();
    for(std::size_t i=migration.base_signature.aspects().size();i-->0;) {
        const auto& a=migration.base_signature.aspects().at(static_cast<composite::AspectId>(i));aspects.add(a.name,a.role,a.default_value);
        migration.aspects.push_back({a.name,a.name});
    }
    migration.target_signature=core::RelationSignature(aspects);migration.target_signature.add_type("collective",core::BoundaryPolicy::DeclaredExact,0,10);
    migration.types={{"collective","collective"}};
    auto recoded=e;recoded.sequence=1;recoded.source=recode_temporal_source(e.source,migration);
    NativeAccessionDirectory rec(e,77);rec.advance(make_temporal_segment(e,recoded,{}, {},RelationalCodeEncoding::Automatic,migration));
    auto rec_op=prepare(rec);check(execute(rec_op).values()==y,"source aspect recoding changed meaning");
    if(oracle)emit("recoded",recoded,rec_op);
}
}
int main(int argc,char** argv) {
    try { const bool oracle=argc==2 && std::string(argv[1])=="--oracle";
        positive(oracle);lifetime_and_publication();negative();policies();prepared_layout_and_workspace();temporal_changes(oracle);randomized(oracle);
        std::cout<<"checks "<<checks<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<"native execution failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
