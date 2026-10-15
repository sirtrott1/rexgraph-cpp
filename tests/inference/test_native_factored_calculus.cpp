#include "../../examples/native_frames/fixture.hpp"
#include "rfc/inference/native_source_green.hpp"
#include "rfc/inference/native_tower_operators.hpp"
#include "rfc/field/exact_dirac.hpp"
#include "rfc/field/connection.hpp"
#include "rfc/field/differential.hpp"
#include <iostream>

using namespace rfc;
using namespace native_frame_example;
using namespace inference;
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"expected invalid native operation rejection");}
PreparedNativeExactTower tower(const storage::NativeAccessionDirectory& directory,unsigned mode){
    const auto req=request(directory,core::Grade{0},mode);NativeExactTowerRequest out;
    out.metrics=req.metrics;out.grade_carriers=req.grade_carriers;out.metric_tensors=req.metric_tensors;
    out.profile=field::PreparedExactProfile::GlobalActions;
    return PreparedNativeExactTower::prepare(input(directory,core::Grade{0}),out);
}
field::ExactTowerField plain(const NativeTowerField& value){field::ExactTowerField out;for(const auto& g:value.grades)out.grades.push_back(g.values());return out;}
int main(){try{
    const NativeActionAvailability availability{{900,7},{901,8}};
    auto old_endpoint=endpoint(pair(),2),new_endpoint=endpoint(branch(),2,8);
    const Matrix I=Matrix::from_rows({{1,2}}),Ip=Matrix::from_rows({{1,0,2,0},{0,1,0,3},{1,1,0,1}});
    const Matrix A=Matrix::from_rows({{2},{-1}}),Ap=Matrix::from_rows({{1,0,0},{0,2,0},{0,0,3},{1,1,1}});
    old_endpoint.tensors.push_back(native_update_example::map(900,10,11,I));
    new_endpoint.tensors.push_back(native_update_example::map(900,10,11,Ip));
    old_endpoint.tensors.push_back(native_update_example::map(901,11,10,A,storage::TemporalTensorRole::Accession));
    new_endpoint.tensors.push_back(native_update_example::map(901,11,10,Ap,storage::TemporalTensorRole::Accession));
    storage::NativeAccessionDirectory old_directory(old_endpoint,900),new_directory(new_endpoint,901),foreign(old_endpoint,900);
    const auto old_tower=tower(old_directory,2),new_tower=tower(new_directory,2);
    const auto old_family=PreparedNativeOperatorFamily::prepare(old_tower,availability),new_family=PreparedNativeOperatorFamily::prepare(new_tower,availability);
    check(old_family.publications().size()==old_tower.metric_pins().size()+1 && new_family.publications().size()==new_tower.metric_pins().size()+1,
        "every actual declared metric pin is retained for limits and availability");
    const auto op=[](const auto& d,std::uint64_t key){return d.pin(access(storage::NativeAccessionKind::Operator,key));};
    const Matrix J=Matrix::from_rows({{1},{2},{-1}}),K=Matrix::from_rows({{1,0},{0,1},{1,1},{2,-1}});
    const auto ambient=NativeFactoredAction::correspondence(old_family.carrier(core::Grade{1}),new_family.carrier(core::Grade{1}),J);
    const auto source=NativeFactoredAction::correspondence(old_family.carrier(core::Grade{0}),new_family.carrier(core::Grade{0}),K);
    const auto plan=PreparedNativeSourceGreenResponse::prepare(old_family,new_family,core::Grade{1},op(old_directory,900),op(new_directory,900),ambient,source,Q{1,2},availability);
    const auto green_owner=&plan.old_green().numeric_action();const auto prep=old_family.preparation_steps();
    for(unsigned s=0;s<2;++s){
        const auto old_pin=input(old_directory,core::Grade{0},s),new_pin=input(new_directory,core::Grade{0},1-s);
        const auto result=plan.execute(old_pin,new_pin,availability);
        const field::ExactHodgeOperator Lo(old_endpoint.source,core::Grade{1},old_tower.tower().metrics()),Ln(new_endpoint.source,core::Grade{1},new_tower.tower().metrics());
        const auto expected=temporal::green_response_delta(Lo.materialize(),Ln.materialize(),J,I,Ip,K,
            native_field_detail::dense(*old_pin.tensor()),native_field_detail::dense(*new_pin.tensor()),Q{1,2});
        check(result.old_response.values()==expected.old_response && result.new_response.values()==expected.new_response,"factored source Green direct solve");
        check(result.realization_term.values()==expected.realization_term && result.innovation_term.values()==expected.innovation_term &&
            result.operator_term.values()==expected.operator_term && result.aggregate.values()==expected.aggregate,"all source Green terms");
        const auto observed=plan.observe(result,NativeFactoredAction::tensor(op(old_directory,901)),NativeFactoredAction::tensor(op(new_directory,901)),source,availability);
        const auto expected_observed=temporal::observed_green_response_delta(expected,A,Ap,J,K);
        check(observed.aggregate.values()==expected_observed.aggregate && observed.accession_term.values()==expected_observed.accession_term,"all observed source Green terms");
        check(result.aggregate.inputs().size()==2 && result.aggregate.retained_action_owners()>0,"native source Green retains both actual roots and action owners");
        check(green_owner==&plan.old_green().numeric_action() && old_family.preparation_steps()==prep,"Green and metric factors reused");
    }
    auto old_field=NativeFieldValue::stored(input(old_directory,core::Grade{0}));
    const auto action=old_family.action({field::OperatorKind::Adjoint,core::Grade{1},0},availability);
    const auto values=action.execute(old_field,availability);
    for(const auto& pin:old_tower.metric_pins()){
        bool retained=false;for(const auto& dependency:values.publications())if(dependency.kind()==storage::NativeAccessionKind::Tensor &&
            dependency.tensor_declaration().get()==pin.tensor_declaration().get() && dependency.same_execution_authority(pin))retained=true;
        check(retained,"native field ancestry retains each actual metric pin");
    }
    const std::array<std::size_t,1> selected{0};
    check(action.selected(old_field,selected,availability)==values.values(),"selected transpose action agrees");
    reject([&]{action.execute(input(foreign,core::Grade{0}),availability);});
    reject([&]{ambient.then(ambient);});
    reject([&]{plan.execute(input(old_directory,core::Grade{0}),input(new_directory,core::Grade{0}),{{900,6},{901,8}});});
    runtime::ExecutionHostPolicy limited;limited.limits.scalar_work=1;
    reject([&]{plan.execute(input(old_directory,core::Grade{0}),input(new_directory,core::Grade{0}),availability,runtime::ExecutionLease(limited));});
    auto stop=std::make_shared<std::atomic_bool>(true);reject([&]{action.execute(old_field,availability,runtime::ExecutionLease({},stop));});
    // General positive metrics on a nontrivial retained four-grade source.
    storage::NativeAccessionDirectory higher_directory(endpoint(higher(),2),900);
    const auto ht=tower(higher_directory,2);const auto hf=PreparedNativeOperatorFamily::prepare(ht,availability);
    std::vector<storage::PinnedNativeAccession> real_pins,imaginary_pins;
    for(std::size_t k=0;k<ht.publication().endpoint()->source.grade_count();++k){real_pins.push_back(input(higher_directory,core::Grade{static_cast<std::uint16_t>(k)}));imaginary_pins.push_back(input(higher_directory,core::Grade{static_cast<std::uint16_t>(k)},1));}
    const auto real=NativeTowerField::stored(real_pins),imaginary=NativeTowerField::stored(imaginary_pins);
    for(const auto kind:{NativeTowerOperatorKind::Dirac,NativeTowerOperatorKind::AntiDirac}){
        const auto d=PreparedNativeTowerOperator::prepare(hf,kind,0,availability);const auto got=d.execute(real,availability);
        const auto expected=kind==NativeTowerOperatorKind::Dirac?field::exact_dirac_apply(higher_directory.pin(access(storage::NativeAccessionKind::Field,100)).endpoint()->source,ht.tower().metrics(),plain(real)):
            field::exact_antidirac_apply(ht.publication().endpoint()->source,ht.tower().metrics(),plain(real));
        check(plain(got).grades==expected.grades,"native full tower Dirac/anti-Dirac");
    }
    for(const Q& t:{Q{0},Q{1,2},Q{-2,3}}){
        const auto anti=PreparedNativeTowerOperator::prepare(hf,NativeTowerOperatorKind::AntiDiracCayley,t,availability);
        const auto y=anti.execute(real,availability);
        check(plain(y).grades==field::exact_antidirac_cayley_apply(ht.publication().endpoint()->source,ht.tower().metrics(),t,plain(real)).grades,"native rational Cayley reference");
        check(anti.quadrance(y,availability)==anti.quadrance(real,availability),"native rational Cayley exact isometry");
        const auto complex=PreparedNativeTowerOperator::prepare(hf,NativeTowerOperatorKind::DiracCayley,t,availability);
        const auto z=complex.execute(NativeComplexTowerField{real,imaginary},availability);
        const auto expected=field::exact_dirac_cayley_apply(ht.publication().endpoint()->source,ht.tower().metrics(),t,{plain(real),plain(imaginary)});
        check(plain(z.real).grades==expected.real.grades && plain(z.imaginary).grades==expected.imaginary.grades,"native Q(i) Cayley reference");
        check(complex.quadrance(z.real,availability)+complex.quadrance(z.imaginary,availability)==
            complex.quadrance(real,availability)+complex.quadrance(imaginary,availability),"native Q(i) exact isometry");
    }
    // New connection scalar path supports an external metric without any materializer.
    const auto c=higher();const auto ideal=field::ExactConnectionTower::ideal(c);
    std::vector<field::ExactMetric> external;for(std::size_t k=0;k<c.grade_count();++k){const auto n=c.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
        external.push_back(field::ExactMetric::certified_external(n,[](const Vector& x){return x;},[](const Vector& x){return x;}));}
    check(ideal.curvature_magnitude(core::Grade{2},external)==0,"connection trace needs no metric inverse/materialization");
    std::vector<field::ExactMetric> sparse_external;
    for(std::size_t k=0;k<c.grade_count();++k){const auto n=c.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
        const auto no_dense=[](const Vector&)->Vector{throw std::logic_error("dense callback must not be called");};
        const auto sparse_identity=[](numeric::SparseRationalView x){return numeric::SparseRationalVector(x.size(),{x.entries().begin(),x.entries().end()});};
        sparse_external.push_back(field::ExactMetric::certified_external(n,no_dense,no_dense,{},sparse_identity,sparse_identity));}
    check(ideal.curvature_magnitude(core::Grade{2},sparse_external)==0,"curvature scalar uses sparse callbacks without a full coordinate probe");
    auto deformed_map=ideal.map(core::Grade{2});deformed_map.set(0,0,deformed_map.get(0,0)+1);
    const auto curved=ideal.with_map(core::Grade{2},deformed_map);
    const auto F=curved.curvature(core::Grade{2}).defect;
    const auto full_reference=ht.tower().metrics()[2].materialize().inverse().multiply(F.transpose())
        .multiply(ht.tower().metrics()[0].materialize()).multiply(F);
    Q curvature_reference=0;for(std::size_t i=0;i<full_reference.rows();++i)curvature_reference+=full_reference.get(i,i);
    check(curved.curvature_magnitude(core::Grade{2},ht.tower().metrics())==curvature_reference,"sparse curvature contraction equals independent matrix trace");
    const auto B=field::boundary_matrix(c,core::Grade{1});
    check(field::metric_adjoint_variation(B,sparse_external[1],sparse_external[0],B,Matrix(B.cols(),B.cols()),Matrix(B.rows(),B.rows()))==B.transpose(),
        "metric adjoint variation uses sparse probes while realizing its requested map");
    // An empty retained grade is a real carrier; every full-tower profile must
    // preserve all four declarations even though there are no numeric entries.
    storage::NativeAccessionDirectory empty_directory(endpoint(empty(3),2),900);
    const auto et=tower(empty_directory,2);
    const auto ef=PreparedNativeOperatorFamily::prepare(et,availability);std::vector<storage::PinnedNativeAccession> empty_pins;
    for(std::uint16_t k=0;k<4;++k)empty_pins.push_back(input(empty_directory,core::Grade{k}));
    const auto empty_field=NativeTowerField::stored(empty_pins);
    for(const auto kind:{NativeTowerOperatorKind::Dirac,NativeTowerOperatorKind::AntiDirac,NativeTowerOperatorKind::AntiDiracCayley}){
        const auto d=PreparedNativeTowerOperator::prepare(ef,kind,kind==NativeTowerOperatorKind::AntiDiracCayley?Q{1,2}:Q{0},availability);
        const auto result=d.execute(empty_field,availability);check(result.grades.size()==4,"retained empty full tower grades");
        for(std::uint16_t k=0;k<4;++k)check(result.grades[k].carrier().same(ef.carrier(core::Grade{k})) && result.grades[k].sparse_values().size()==0 &&
            result.grades[k].retained_action_owners()>0,"empty grade actual owner and sparse field");
        check(d.quadrance(result,availability)==0,"empty tower exact energy");
    }
    const auto ec=PreparedNativeTowerOperator::prepare(ef,NativeTowerOperatorKind::DiracCayley,Q{1,2},availability);
    check(ec.execute(NativeComplexTowerField{empty_field,empty_field},availability).real.grades.size()==4,"retained empty complex tower");
    const auto zero_step=PreparedNativeTowerOperator::prepare(hf,NativeTowerOperatorKind::AntiDiracCayley,Q{0},availability);
    runtime::ExecutionHostPolicy no_work;no_work.limits.scalar_work=0;
    reject([&]{zero_step.execute(real,availability,runtime::ExecutionLease(no_work));});
    std::cout<<"native factored calculus: "<<checks<<" exact checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
