#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_evidence.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Evidence=learning::PreparedTransportEvidence;
namespace {
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message){
    bool refused=false;try{action();}catch(const std::exception&){refused=true;}check(refused,message);
}
learning::PreparedTransportObservations fixture(Guard& guard,bool tied=false,bool zero=false) {
    core::RelationSignature signature;const auto type=signature.add_type("transport",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
    builder.add_grade_one(type,{{0,true,true,Q{1,2}},{0,true,false,Q{1,3}},
        {1,false,false,Q{7}},{1,true,false,Q{0}},{1,true,false,Q{2}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    stalks.set_dimension({core::Grade{0},0},2);stalks.set_dimension({core::Grade{1},0},3);
    local::LinearRelationalFiberSystem fibers(stalks);
    for(std::size_t p=0;p<5;++p){M map(p<2?2:1,3);if(p==0)map.set(1,2,Q{-3,2});fibers.set_transport(core::Grade{1},p,std::move(map));}
    local::OwnedLinearFiberSystem owner(fibers);
    const std::vector<learning::TransportCoordinate> fixed{{0,1,2}},variables{{0,0,0},{1,0,0},{1,1,1},{2,0,0},{3,0,1},{4,0,1}};
    if(zero)return learning::PreparedTransportObservations(owner,fixed,variables,M(6,0),guard);
    if(tied){M basis(6,3);basis.set(0,0,1);basis.set(0,1,1);basis.set(1,0,2);basis.set(1,1,2);return learning::PreparedTransportObservations(owner,fixed,variables,basis,guard);}
    return learning::PreparedTransportObservations(owner,fixed,variables,guard);
}
learning::TransportObservation sample(const learning::PreparedTransportObservations& model,V input,std::vector<std::size_t> observed={1,1}) {
    Guard g;const auto action=model.observation_map(input,true,g);V parameters(action.cols());
    for(std::size_t i=0;i<parameters.size();++i)parameters[i]=Q{static_cast<long>(i+2),7};
    auto target=numeric::checked_apply(action,parameters,g);const auto fixed=model.fixed_response(input,g);
    for(std::size_t i=0;i<target.size();++i)target[i]+=fixed.boundary[i];
    if(!observed[0])target[0]=target[1]=0;
    if(!observed[1])target[2]=0;
    return {std::move(input),std::move(target),std::move(observed)};
}
void oracle(const Evidence& evidence) {
    Guard guard;const auto system=evidence.model().assemble(evidence.observations(),guard);
    check(system.design==evidence.system().design && system.augmented==evidence.system().augmented && system.target==evidence.system().target,"original equations differ");
    check(system.constraints.size()==evidence.system().constraints.size(),"constraint count differs");
    for(std::size_t i=0;i<system.constraints.size();++i){
        const auto& a=system.constraints[i];const auto& b=evidence.system().constraints[i];
        check(a.sample==b.sample && a.cell==b.cell && a.axis==b.axis,"original constraint identity differs");
    }
    numeric::PreparedExactFrames design(system.design,guard),joint(system.augmented,guard);
    check(evidence.rank()==design.rank() && design.rank()==joint.rank(),"exact rank differs");
    check(evidence.pivots()==design.pivots() && evidence.free_columns()==design.free_columns(),"complete parameter axes differ");
    check(evidence.kernel()==design.kernel(),"complete original kernel differs");
    V particular(system.design.cols());for(std::size_t i=0;i<joint.rank();++i)particular[joint.pivots()[i]]=joint.reduced().get(i,system.design.cols());
    check(evidence.particular()==particular,"canonical affine origin differs");
}
void families() {
    for(const bool tied:{false,true}) {
        Guard g;Evidence empty(fixture(g,tied),g);oracle(empty);
        auto state=empty;std::vector<learning::TransportObservation> all;
        for(long i=-12;i<=12;++i) {
            const auto field=V{Q{i,5},Q{i*i+1,7},Q{i+3,11}};auto observation=sample(state.model(),field,{1,static_cast<std::size_t>(i%2==0)});
            all.push_back(observation);Guard next;state=state.append({observation},next);oracle(state);
            Guard cold;const auto replay=empty.append(all,cold);
            check(replay.particular()==state.particular() && replay.kernel()==state.kernel(),"cold complete family differs");
            check(replay.preparation_steps()==state.preparation_steps() && replay.retained_entries()==state.retained_entries(),"canonical replay accounting differs");
            check(state.update().reused_observations==all.size()-1 && state.update().assembled_observations==1,"prefix assembly was not reused");
            check(empty.observations().empty() && empty.rank()==0,"parent changed");
            check(state.model().owner().same_owner(empty.model().owner()),"original fiber owner changed");
        }
        std::reverse(all.begin(),all.end());Guard reverse;oracle(empty.append(all,reverse));
    }
    Guard g;Evidence empty(fixture(g,false,true),g);const auto observation=sample(empty.model(),{1,2,3});
    const auto zero=empty.append({observation},g);oracle(zero);check(zero.kernel().rows()==0 && zero.rank()==0,"zero parameter carrier lost");
}
void branches_and_admission() {
    Guard g;Evidence empty(fixture(g),g);const auto first=sample(empty.model(),{0,0,1});
    const auto parent=empty.append({first},g);auto conflict=first;conflict.target[0]+=1;
    rejects([&]{Guard q;(void)parent.append({conflict},q);},"contradictory suffix accepted");
    oracle(parent);const auto second=sample(parent.model(),{1,0,0});
    Guard a;const auto valid=parent.append({second,first},a);oracle(valid);
    check(valid.observations().size()==3 && parent.observations().size()==1,"atomic branch changed its parent");
    rejects([&]{Guard q;(void)parent.append({},q);},"empty suffix accepted");
    auto bad=first;bad.observed={0,0};rejects([&]{Guard q;(void)parent.append({bad},q);},"empty mask accepted");
    bad=first;bad.observed={1,0};bad.target[2]=1;rejects([&]{Guard q;(void)parent.append({bad},q);},"unobserved target accepted");
    bad=first;bad.input={1};rejects([&]{Guard q;(void)parent.append({bad},q);},"partial input accepted");
    auto limits=numeric::ExactActionLimits{};limits.steps=1;
    rejects([&]{Guard q(limits);(void)parent.append({second},q);},"changed append admission accepted");
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});(void)parent.append({second},q);},"cancellation ignored");
    limits={};limits.entries=1;rejects([&]{Guard q(limits);Evidence denied(fixture(q),q);},"entry admission ignored");
    bad=first;bad.input[0]=Q{numeric::Integer(1)<<16777216};
    rejects([&]{Guard q;(void)parent.append({bad},q);},"bit admission ignored");
    auto jobs=std::vector<std::future<V>>{};
    for(std::size_t i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[parent,second]{Guard q;const auto child=parent.append({second},q);return child.particular();}));
    Guard q;const auto expected_child=parent.append({second},q);const auto expected=expected_child.particular();
    for(auto& job:jobs)check(job.get()==expected,"concurrent immutable branch differs");
}
}
int main(){try{families();branches_and_admission();std::cout<<checks<<" incremental transport evidence assertions passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
