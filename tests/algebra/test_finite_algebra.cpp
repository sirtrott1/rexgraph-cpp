#include "../../examples/finite_algebra/fixture.hpp"
#include <iostream>
using namespace rfc;
using Algebra=algebra::PreparedFiniteAlgebra;
using Law=algebra::FiniteAlgebraLaw;
using Status=algebra::FiniteLawStatus;
using Guard=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,why);}
void evidence(const core::RelationalComplex& source,const algebra::FiniteOperationEvaluation& proof) {
    const auto& arena=source.grade(core::Grade{1});
    for(const auto& row:proof.evidence) {
        for(std::size_t i=0;i<3;++i) {
            const auto p=row.slots[i];check(source.occurrence_identity(core::Grade{1},p)==row.occurrences[i],"retained occurrence certificate differs");
            check(source.grade(core::Grade{0}).identities[arena.lower_index[p]]==row.values[i],"witness value not in retained occurrence");
            check(source.relation_identity({core::Grade{1},arena.upper_index[p]})==row.relation,"witness relation identity differs");
        }
    }
}
void fields_and_ring() {
    Guard guard;
    for(const unsigned p:{2u,3u,5u,7u}) {
        const auto d=finite_algebra_example::modular(p);
        const auto table=Algebra::prepare(d.source,d.operations,guard);
        const auto report=table.check_field(0,1,d.values[0],d.values[1],guard);
        check(report.valid() && table.owns(report),"prime modular relation tables did not certify a field");
        check(report.laws().size()==10 && report.additive_inverses().size()==p && report.multiplicative_inverses().size()==p-1,"field law or inverse witnesses incomplete");
        for(const auto& law:report.laws())check(law.result().status==Status::Satisfied && table.owns(law),"field law not certified on original owner");
        for(const auto& inverse:report.additive_inverses()){evidence(table.source(),inverse.left);evidence(table.source(),inverse.right);}
        for(const auto& inverse:report.multiplicative_inverses()){evidence(table.source(),inverse.left);evidence(table.source(),inverse.right);}
        const auto self=table.apply(0,d.values[0],d.values[0],guard);
        check(self.value==d.values[0] && self.evidence[0].values[0]==self.evidence[0].values[1] &&
            self.evidence[0].occurrences[0]!=self.evidence[0].occurrences[1],"repeated operand identity collapsed its distinct role occurrences");
    }
    const auto d=finite_algebra_example::modular(4);const auto ring=Algebra::prepare(d.source,d.operations,guard);
    const auto invalid=ring.check_field(0,1,d.values[0],d.values[1],guard);
    check(!invalid.valid() && invalid.missing_multiplicative_inverses()==std::vector{d.values[2]},"zero divisor incorrectly certified as a field");
    check(std::all_of(invalid.laws().begin(),invalid.laws().end(),[](const auto& law){return law.result().status==Status::Satisfied;}),"Z4 ring laws were incorrectly refuted");
}
void duplicates_conflicts_and_partial() {
    Guard guard;
    auto d=finite_algebra_example::modular(3,true);auto table=Algebra::prepare(d.source,d.operations,guard);
    check(table.profiles()[0].functional && table.profiles()[0].total && table.profiles()[0].retained_relations==10,"duplicate matching assertion changed functionality");
    const auto repeated=table.apply(0,d.values[0],d.values[0],guard);
    check(repeated.value==d.values[0] && repeated.evidence.size()==2,"duplicate original assertion evidence lost");
    check(repeated.evidence[0].relation!=repeated.evidence[1].relation,"different relation witnesses merged");
    check(table.check_field(0,1,d.values[0],d.values[1],guard).valid(),"duplicate matching relation invalidated field");
    d=finite_algebra_example::modular(3,true,true);table=Algebra::prepare(d.source,d.operations,guard);
    check(!table.profiles()[0].functional && table.profiles()[0].total && table.profiles()[0].conflict.has_value(),"conflicting original assertions did not fail functionality");
    const auto conflict=table.apply(0,d.values[0],d.values[0],guard);
    check(!conflict.value && conflict.evidence.size()==3,"ambiguous output silently selected");
    const auto undefined=table.check_law({Law::Commutative,0},guard);
    check(undefined.result().status==Status::Undetermined && undefined.result().witness.has_value(),"multivalued table certified universal law");
    check(!table.check_field(0,1,d.values[0],d.values[1],guard).valid(),"nonfunctional field certified");
    d=finite_algebra_example::modular(3,false,false,true);table=Algebra::prepare(d.source,d.operations,guard);
    check(table.profiles()[0].functional && !table.profiles()[0].total && table.profiles()[0].missing==std::pair{d.values[2],d.values[2]},"missing retained pair not reported");
    const auto incomplete=table.check_law({Law::Associative,0},guard);
    check(incomplete.result().status==Status::Undetermined && incomplete.result().assignments_checked==27,"partial operation law did not preserve uncertainty");
}
void counterexample_and_ownership() {
    Guard guard;const auto d=finite_algebra_example::modular(3,false,false,false,true);
    const auto table=Algebra::prepare(d.source,d.operations,guard);
    const auto law=table.check_law({Law::Associative,0},guard);
    check(law.result().status==Status::Refuted && law.result().witness.has_value(),"nonassociative operation not refuted");
    const auto& witness=*law.result().witness;
    check(witness.assignment.size()==3 && witness.left.value && witness.right.value && witness.left.value!=witness.right.value,"counterexample does not witness unequal defined values");
    evidence(law.source(),witness.left);evidence(law.source(),witness.right);
    const auto copy=table;check(copy.same_owner(table) && copy.owns(law),"copied law owner changed");
    const auto same_values=Algebra::prepare(d.source,d.operations,guard);check(!same_values.owns(law),"equal source values identified different law receipt owners");
    const auto retained=[] {
        Guard local;const auto sample=finite_algebra_example::modular(3);const auto algebra=Algebra::prepare(sample.source,sample.operations,local);
        return algebra.check_law({Law::Associative,0},local);
    }();
    check(retained.result().status==Status::Satisfied && retained.source().grade(core::Grade{1}).cell_count()==18,"law receipt lost its source lifetime");
}
void refusals() {
    Guard guard;const auto d=finite_algebra_example::modular(3);
    const auto malformed=finite_algebra_example::modular(3,false,false,false,false,true);
    rejects([&]{(void)Algebra::prepare(malformed.source,malformed.operations,guard);},"multiple semantic roles accepted");
    auto roles=d.operations;roles[0].right=roles[0].left;
    rejects([&]{(void)Algebra::prepare(d.source,roles,guard);},"coincident role aspects accepted");
    roles=d.operations;roles[0].left=d.source.signature().existence_aspect();
    rejects([&]{(void)Algebra::prepare(d.source,roles,guard);},"existence flag inferred as operand role");
    roles=d.operations;roles[1].type=roles[0].type;
    rejects([&]{(void)Algebra::prepare(d.source,roles,guard);},"one relation type assigned two operation meanings");
    const auto table=Algebra::prepare(d.source,d.operations,guard);
    rejects([&]{(void)table.apply(0,9999,d.values[0],guard);},"foreign carrier identity accepted");
    rejects([&]{(void)table.check_law({Law::LeftIdentity,0},guard);},"undeclared law identity accepted");
    rejects([&]{(void)table.check_field(0,0,d.values[0],d.values[1],guard);},"one operation implicitly reused as both field operations");
    rejects([&]{Guard canceled({},[]{throw std::runtime_error("cancel");});(void)Algebra::prepare(d.source,d.operations,canceled);},"preparation cancellation ignored");
    rejects([&]{auto limits=numeric::ExactActionLimits{};limits.entries=1;Guard small(limits);(void)Algebra::prepare(d.source,d.operations,small);},"retained table admission ignored");
    rejects([&]{Guard canceled({},[]{throw std::runtime_error("cancel");});(void)table.check_law({Law::Associative,0},canceled);},"law enumeration cancellation ignored");
}
}
int main(){try{fields_and_ring();duplicates_conflicts_and_partial();counterexample_and_ownership();refusals();
    std::cout<<checks<<" finite algebra checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
