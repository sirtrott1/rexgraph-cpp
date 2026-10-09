#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_field_objective.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool value,const char* text) {++checks;if(!value)throw std::runtime_error(text);}
template<class F> void rejects(F action,const char* text) {
    bool refused=false;try{action();}catch(const std::exception&){refused=true;}check(refused,text);
}
learning::PreparedTransportObservations fixture(Guard& guard) {
    core::RelationSignature signature;
    const auto type=signature.add_type("objective",core::BoundaryPolicy::DeclaredExact,0,2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(type,{{0,true,false,Q{1}}});
    builder.add_grade_one(type,{{0,true,true,Q{1}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    local::LinearRelationalFiberSystem fibers(stalks);
    fibers.set_transport(core::Grade{1},0,M(1,1));fibers.set_transport(core::Grade{1},1,M(1,1));
    return {local::OwnedLinearFiberSystem(fibers),{},{{0,0,0},{1,0,0}},guard};
}
using Objective=learning::PreparedTransportFieldObjective;
const std::vector<learning::TransportFieldView> views{{"reading",{{0,0,Q{1}},{1,0,Q{1}}}}};
Objective select(const learning::PreparedTransportObservations& observations,Q f,Q scale,Guard& guard) {
    M k(2,1);k.set(0,0,scale);k.set(1,0,scale);
    learning::TransportObjectiveSpecification spec;spec.channels[2]=f;
    return {observations,{1,0},k,{{1,1}},views,{1,1},field::CoparticipationReading::Share,spec,guard};
}
void families() {
    Guard guard;const auto observations=fixture(guard);
    for(long factor=1;factor<21;++factor) {
        Guard g;const auto off=select(observations,0,Q{factor,3},g),on=select(observations,Q{factor,7},Q{factor,3},g);
        check(off.rank()==0 && off.kernel()==off.evidence_kernel(),"zero objective removed unresolved directions");
        check(off.origin()==V({1,0}),"zero objective changed evidence origin");
        check(on.observations().owner().same_owner(observations.owner()),"objective changed actual fiber owner");
        check(on.origin()==V({Q{1,2},Q{-1,2}}),"F compatible minimum differs");
        check(on.rank()==1 && on.kernel().cols()==0,"F stationary family differs");
        check(on.evidence_origin()==V({1,0}) && on.evidence_kernel().cols()==1,"evidence family discarded");
        check(on.minimum_energy()==0 && on.initial_energy()==numeric::canonical_rational(Q{factor,7}),"exact F energy differs");
        check(on.fields().size()==1 && on.fields()[0].origin==V({1,0}),"full relation field origin differs");
        check(on.fields()[0].directions.get(1,0)==numeric::canonical_rational(Q{-factor,3}),"original incidence coefficient differs");
        const auto map=observations.observation_map({1,1},true,g);
        check(numeric::checked_apply(map,on.origin(),g)==V({1}),"objective violated training observation");
        const auto query=observations.observation_map({2,1},true,g);
        check(numeric::checked_apply(query,on.origin(),g)==V({Q{3,2}}),"conditional held out response differs");
    }
    Guard g;const auto base=select(observations,1,1,g);
    std::vector<std::future<V>> jobs;
    for(int i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[base]{Guard q;return numeric::checked_apply(base.hessian(),base.coordinate_origin(),q);}));
    for(auto& job:jobs)check(job.get()==base.target(),"concurrent stationary certificate differs");
    // A declared zero view retains support and the full unobserved family.
    M k(2,1);k.set(0,0,1);k.set(1,0,1);learning::TransportObjectiveSpecification spec;spec.channels[2]=1;
    Objective zero(observations,{1,0},k,{{1,1}},{{"zero",{{0,0,0},{1,0,0}}}},{1,1},field::CoparticipationReading::Share,spec,g);
    check(zero.rank()==0 && zero.kernel()==k,"supported zero view removed unknown directions");
    check(zero.views()[0].terms.size()==2,"supported zero view declarations lost");
    Objective unique(observations,{Q{1,2},Q{-1,2}},M(2,0),{{1,1}},views,{1,1},field::CoparticipationReading::Share,spec,g);
    check(unique.origin()==V({Q{1,2},Q{-1,2}}) && unique.kernel().cols()==0,"already identified family changed");
}
void admission() {
    Guard g;const auto o=fixture(g);M k(2,1);k.set(0,0,1);k.set(1,0,1);
    learning::TransportObjectiveSpecification spec;spec.channels[2]=1;
    const auto build=[&](auto v,auto p,auto s,Guard& q){return Objective(o,{1,0},k,std::move(p),std::move(v),{1,1},field::CoparticipationReading::Share,s,q);};
    rejects([&]{Guard q;auto s=spec;s.mass=-1;(void)build(views,std::vector<V>{{1,1}},s,q);},"negative mass accepted");
    rejects([&]{Guard q;auto s=spec;s.channels[2]=-1;(void)build(views,std::vector<V>{{1,1}},s,q);},"negative channel coefficient accepted");
    rejects([&]{Guard q;(void)build(views,std::vector<V>{{1}},spec,q);},"partial upper field accepted");
    rejects([&]{Guard q;(void)build(std::vector<learning::TransportFieldView>{{"bad",{{2,0,1}}}},std::vector<V>{{1,1}},spec,q);},"foreign occurrence accepted");
    rejects([&]{Guard q;(void)build(std::vector<learning::TransportFieldView>{{"bad",{{0,1,1}}}},std::vector<V>{{1,1}},spec,q);},"foreign stalk axis accepted");
    rejects([&]{Guard q;(void)build(std::vector<learning::TransportFieldView>{{"bad",{{0,0,1},{0,0,0}}}},std::vector<V>{{1,1}},spec,q);},"duplicate supported zero accepted");
    rejects([&]{Guard q;(void)build(std::vector<learning::TransportFieldView>{views[0],views[0]},std::vector<V>{{1,1}},spec,q);},"duplicate view name accepted");
    rejects([&]{Guard q;(void)build(views,std::vector<V>{},spec,q);},"empty probes accepted");
    rejects([&]{Guard q;(void)build(std::vector<learning::TransportFieldView>{},std::vector<V>{{1,1}},spec,q);},"empty views accepted");
    auto limits=numeric::ExactActionLimits{};limits.steps=100;
    rejects([&]{Guard q(limits);(void)build(views,std::vector<V>{{1,1}},spec,q);},"aggregate step admission ignored");
    limits={};limits.entries=10;
    rejects([&]{Guard q(limits);(void)build(views,std::vector<V>{{1,1}},spec,q);},"entry admission ignored");
    limits={};limits.integer_bits=1;
    rejects([&]{Guard q(limits);(void)build(views,std::vector<V>{{1,1}},spec,q);},"intermediate bit admission ignored");
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});(void)build(views,std::vector<V>{{1,1}},spec,q);},"cancellation ignored");
}
void higher_rank() {
    core::RelationSignature signature;
    const auto type=signature.add_type("reading",core::BoundaryPolicy::DeclaredExact,0,2);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t j=0;j<3;++j) builder.add_grade_one(type,{{0,true,j!=0,Q{1}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    local::LinearRelationalFiberSystem fibers(stalks);
    for(std::size_t p=0;p<3;++p) fibers.set_transport(core::Grade{1},p,M(1,1));
    Guard g;M basis(3,4);for(std::size_t i=0;i<3;++i)basis.set(i,i,1);
    learning::PreparedTransportObservations observations(local::OwnedLinearFiberSystem(fibers),{},
        {{0,0,0},{1,0,0},{2,0,0}},basis,g);
    M kernel(4,3);kernel.set(0,0,1);kernel.set(1,0,1);kernel.set(0,1,1);kernel.set(2,1,1);kernel.set(3,2,1);
    const std::vector<learning::TransportFieldView> fields{{"reading",{{0,0,1},{1,0,1},{2,0,1}}}};
    learning::TransportObjectiveSpecification spec;spec.channels[2]=1;
    Objective selected(observations,{1,0,0,7},kernel,{{1,1,1}},fields,{1,1,1},field::CoparticipationReading::Share,spec,g);
    check(selected.rank()==2 && selected.kernel().cols()==1,"higher rank objective lost residual family");
    check(selected.origin()==V({Q{1,3},Q{-1,3},Q{-1,3},7}),"three relation F minimum differs");
    M expected(4,1);expected.set(3,0,1);check(selected.kernel()==expected,"unused named parameter was selected");
    check(selected.free_columns()==std::vector<std::size_t>{2},"original affine coordinate identity lost");
    check(&selected.channel_plan().source()==&observations.owner().fibers().complex(),"channel action changed actual source owner");
    check(selected.minimum_energy()==0 && selected.initial_energy()==2,"three relation F energy differs");
    const auto h=selected.hessian();check(h.rows()==3 && h.cols()==3 && h.get(2,2)==0,"zero Hessian direction lost");
    const auto a=observations.observation_map({1,1,1},true,g);
    check(numeric::checked_apply(a,selected.origin(),g)==V({1}),"higher rank selection changed observed field");
    check(numeric::checked_multiply(a,selected.kernel(),g).nonzeros()==0,"residual direction changed observation");
}
}
int main(){try{families();admission();higher_rank();std::cout<<checks<<" exact transport field objective assertions passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
