#include "rfc/core/builder.hpp"
#include "rfc/learning/graded_transport_fit.hpp"
#include <iostream>
using namespace rfc;
using M=numeric::ExactMatrix;
using V=numeric::RationalVector;
using Q=numeric::Rational;
using Guard=numeric::ExactActionGuard;
using Fit=learning::PreparedGradedTransportFit;
namespace {
std::size_t checks{};
void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,why);}
core::RelationalComplex source(bool paired) {
    core::RelationSignature signature;auto type=signature.add_type("boundary",core::BoundaryPolicy::DeclaredExact,0,16);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(type,{{10,true,true,1},{20,true,false,1}});
    b.add_grade_one(type,{{20,true,true,1},{30,true,false,1}});
    b.add_grade_one(type,{{10,true,true,1},{30,true,false,1}});
    b.add_higher(core::Grade{2},type,{{0,true,false,1},{1,true,false,1},{2,true,true,1},{1,true,false,1},{1,true,true,1}});
    if(paired) {
        b.add_higher(core::Grade{2},type,{{0,true,false,1},{1,true,false,1},{2,true,true,1},{1,true,false,1},{1,true,true,1}});
        b.add_higher(core::Grade{3},type,{{0,true,false,1},{1,true,true,1}});
    }
    return b.freeze();
}
learning::PreparedTransportObservations model(bool paired,Guard& guard,bool unrelated=false) {
    const auto c=source(paired);local::LinearStalkBundle stalks(c,1);local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> fixed,variable;
    const std::size_t selected=unrelated?1:2;
    for(std::size_t k=1;k<c.grade_count();++k) {
        const core::Grade g{static_cast<std::uint16_t>(k)};
        for(std::size_t p=0;p<c.grade(g).incidence_count();++p) {
            M map(1,1);
            if(k==selected) {
                if(!unrelated && p==0){map.set(0,0,1);fixed.push_back({p,0,0});}
                else variable.push_back({p,0,0});
            } else if(!(unrelated && k==2 && c.grade(g).upper_index[p]==1))map.set(0,0,c.incidence_coefficient(g,p));
            fibers.set_transport(g,p,std::move(map));
        }
    }
    return learning::PreparedTransportObservations::at_grade(local::OwnedLinearFiberSystem(fibers),
        core::Grade{static_cast<std::uint16_t>(selected)},fixed,variable,guard);
}
void exact_completion(bool paired) {
    Guard g;Fit fit(model(paired,g),{},g);
    check(!numeric::is_zero(fit.chain_target()),"partial boundary curvature not retained");
    check(fit.kernel().cols()==(paired?4u:2u),"occurrence ambiguity directions were removed");
    check(numeric::checked_apply(fit.chain(),fit.particular(),g)==fit.chain_target(),"affine closure RHS differs");
    for(int n=-2;n<=2;++n) {
        const V free(fit.kernel().cols(),Q{n,3});const auto owned=fit.materialize(free,g);
        const auto& f=owned.fibers();std::optional<M> before;
        for(std::size_t k=1;k<f.complex().grade_count();++k) {
            const auto boundary=local::GradeTensorField(f,core::Grade{static_cast<std::uint16_t>(k)}).matrix();
            if(before)check(before->multiply(boundary).nonzeros()==0,"completed adjacent product nonzero");
            before=boundary;
        }
        const auto two=local::GradeTensorField(f,core::Grade{2}).matrix();
        V first(two.cols());first[0]=1;
        check(two.apply(first)==V({1,1,-1}),"fixed leg was not preserved");
        if(paired){V second(two.cols());second[1]=1;check(two.apply(second)==two.apply(first),"fixed upper boundary was not enforced");}
    }
    const auto input=V(paired?2:1,Q{1});
    const auto realized=fit.materialize(V(fit.kernel().cols()),g);
    const auto target=local::GradeTensorField(realized.fibers(),core::Grade{2}).apply(input);
    const learning::TransportObservation observation{input,target,{1,1,1}};
    Guard child_guard;const auto child=fit.append({observation},child_guard);
    Guard replay_guard;const Fit replay(fit.model(),{observation},replay_guard);
    check(child.particular()==replay.particular() && child.kernel()==replay.kernel(),"affine append differs from complete replay");
    check(&child.chain()==&fit.chain() && &child.chain_target()==&fit.chain_target(),"fixed affine data was copied or changed");
    check(fit.observations().empty(),"append changed parent");
    auto conflict=observation;conflict.target[1]+=1;
    rejects([&]{Guard h;(void)child.append({conflict},h);},"correlated inconsistent suffix accepted");
}
}
int main(){try{
    exact_completion(false);exact_completion(true);
    rejects([]{Guard g;Fit bad(model(true,g,true),{},g);},"nonadjacent fixed defect accepted");
    rejects([]{Guard g;const auto m=model(false,g);learning::PreparedTransportEvidence bad(m,M(1,m.parameter_count()),V{1},g);},
        "unrepairable fixed affine equation accepted");
    std::cout<<checks<<" affine completion checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
