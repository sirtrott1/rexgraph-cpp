#include "rfc/field/passive_dynamics.hpp"
#include <iostream>
using namespace rfc::field;
namespace {
std::size_t checks{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void rejects(F f,const char* reason){bool hit=false;try{f();}catch(const std::exception&){hit=true;}check(hit,reason);}
void certificate(const PassiveStepResult& result){
    check(result.exact && rfc::numeric::is_zero(result.equation_residual) && rfc::numeric::is_zero(result.balance_residual),"exact step satisfies both finite rule and retained passive balance");
    check(result.source_work==result.energy_after-result.energy_before+result.resistance_loss+result.step_dissipation && result.energy_balance_residual==0,"passive energy identity holds exactly");
    check(result.resistance_loss>=0 && result.step_dissipation>=0 && result.execution.state_solves==1 && result.execution.expanded_response_columns==0,"one actual state solve certifies passivity without response-column expansion");
}
}
int main(){try{
    for(long mass=1;mass<=3;++mass)for(long resistance=0;resistance<=3;++resistance)for(long numerator=1;numerator<=3;++numerator){
        ExactMatrix m(1,1),r(1,1),b(1,1);m.set(0,0,mass);r.set(0,0,resistance);b.set(0,0,2);
        const PassiveLinearDynamics model(m,ExactMatrix(1,1),r,b);const Rational h{numerator,5},x{-7,3},u{2,7};
        const auto midpoint=model.midpoint({x},h,{u});certificate(midpoint);
        check(midpoint.xnext==RationalVector{((mass-h*resistance/2)*x+2*h*u)/(mass+h*resistance/2)},"scalar midpoint agrees with independent finite equation");
        const auto backward=model.backwardEuler({x},h,{u});certificate(backward);
        check(backward.xnext==RationalVector{(mass*x+2*h*u)/(mass+h*resistance)},"scalar backward Euler agrees with independent finite equation");
    }
    ExactMatrix m(2,2),j(2,2),r(2,2),b(2,0);
    m.set(0,0,2);m.set(0,1,1);m.set(1,0,1);m.set(1,1,2);j.set(0,1,3);j.set(1,0,-3);
    const PassiveLinearDynamics lossless(m,j,r,b);const auto plan=lossless.prepare(Rational{1,7},PassiveIntegrationMethod::ImplicitMidpoint);
    RationalVector x{1,Rational{2,3}};
    for(unsigned step=0;step<20;++step){const auto result=plan.apply(x);certificate(result);check(result.energy_after==result.energy_before,"non-diagonal metric midpoint preserves exact skew energy");x=result.xnext;}
    check(plan.profile().state_solves==0 && plan.profile().expanded_response_columns==0,"immutable preparation profile is separate from per-action work");
    const auto again=lossless.prepare(Rational{1,7},PassiveIntegrationMethod::ImplicitMidpoint);
    check(&again.relation()==&plan.relation(),"same exact rule reuses immutable prepared relation");
    rejects([&]{(void)lossless.prepare(0,PassiveIntegrationMethod::ImplicitMidpoint);},"nonpositive time step rejects");
    rejects([&]{auto bad=r;bad.set(0,0,-1);(void)PassiveLinearDynamics(m,j,bad,b);},"negative dissipation rejects");
    rejects([&]{auto bad=j;bad.set(0,0,1);(void)PassiveLinearDynamics(m,bad,r,b);},"non-skew structure rejects");
    std::cout<<checks<<" native passive dynamics checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
