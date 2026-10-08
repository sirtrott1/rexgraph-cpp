#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_observations.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Coordinate=learning::TransportCoordinate;
using Prepared=learning::PreparedTransportObservations;
namespace {
std::size_t checks{};
void check(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message) {
    bool refused=false;try{action();}catch(const std::exception&){refused=true;}check(refused,message);
}
local::OwnedLinearFiberSystem fixture() {
    core::RelationSignature signature;
    const auto type=signature.add_type("transport",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
    builder.add_grade_one(type,{{0,true,true,Q{1,2}},{0,true,false,Q{1,3}},
        {1,false,false,Q{7}},{1,true,false,Q{0}},{1,true,false,Q{2}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    stalks.set_dimension({core::Grade{0},0},2);stalks.set_dimension({core::Grade{1},0},3);
    local::LinearRelationalFiberSystem fibers(stalks);
    for(std::size_t p=0;p<5;++p) {
        M map(p<2 ? 2 : 1,3);
        if(p==0) map.set(1,2,Q{-3,2});
        fibers.set_transport(core::Grade{1},p,std::move(map));
    }
    return local::OwnedLinearFiberSystem(fibers);
}
const std::vector<Coordinate> fixed{{0,1,2},{4,0,2}};
const std::vector<Coordinate> variables{{0,0,0},{1,0,0},{1,1,1},{2,0,0},{3,0,1},{4,0,1}};
void actions() {
    const auto owner=fixture();Guard guard;Prepared model(owner,fixed,variables,guard);
    check(model.owner().same_owner(owner),"transport observations changed fiber owner");
    check(model.owner().fibers().complex().grade_count()==3,"empty higher grade lost");
    check(model.owner().fibers().complex().grade(core::Grade{1}).incidence_count()==5,"original flags lost");
    for(long seed=-20;seed<=20;++seed) {
        const V input{Q{seed,5},Q{seed+1,7},Q{seed-1,11}};
        Guard g;const auto shared=model.observation_map(input,true,g), local=model.observation_map(input,false,g);
        M expected(3,6), copies(7,6);
        expected.set(0,0,-input[0]/2);expected.set(0,1,input[0]/3);
        expected.set(1,2,input[1]/3);expected.set(2,5,2*input[1]);
        copies.set(0,0,-input[0]/2);copies.set(2,1,input[0]/3);
        copies.set(3,2,input[1]/3);copies.set(6,5,2*input[1]);
        check(shared==expected,"shared map differs from independent incidence oracle");
        check(local==copies,"original occurrence map differs");
        const auto response=model.fixed_response(input,g);
        check(response.boundary==V({0,-3*input[2]/2,0}),"fixed coefficient applied more than once");
        check(response.occurrences==V({0,-3*input[2]/2,0,0,0,0,0}),"fixed occurrence field differs");
        M basis(6,2);basis.set(0,0,2);basis.set(1,0,3);basis.set(2,1,Q{-1,2});basis.set(5,1,7);
        Prepared named(owner,fixed,variables,basis,g);
        check(named.observation_map(input,true,g)==expected.multiply(basis),"shared parameter aliases differ");
        check(named.observation_map(input,false,g)==copies.multiply(basis),"occurrence parameter aliases differ");
    }
    const V x{2,3,5};learning::TransportObservation observation{x,{7,Q{-11,2},0},{1,0}};
    Guard g;const auto system=model.assemble({observation},g);
    check(system.design.rows()==2 && system.design.cols()==6,"masked equation carrier differs");
    check(system.target==V({7,2}),"fixed response was not subtracted exactly");
    check(system.design.get(0,0)==-1 && system.design.get(0,1)==Q{2,3} && system.design.get(1,2)==1,"observation incidence equations differ");
    check(system.augmented.get(0,6)==7 && system.augmented.get(1,6)==2,"augmented target differs");
    check(system.constraints.size()==2 && system.constraints[1].sample==0 && system.constraints[1].cell==0 && system.constraints[1].axis==1,"constraint identities differ");
    const auto empty=model.assemble({},g);check(empty.design.rows()==0 && empty.design.cols()==6,"empty observations lost parameter carrier");
    auto jobs=std::vector<std::future<M>>{};
    for(std::size_t i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[model,x]{Guard local_guard;return model.observation_map(x,true,local_guard);}));
    const auto expected=model.observation_map(x,true,g);
    for(auto& job:jobs) check(job.get()==expected,"concurrent observation workspace differs");
}
void admission() {
    const auto owner=fixture();Guard g;Prepared model(owner,fixed,variables,g);
    rejects([&]{Guard q;Prepared bad(owner,{},variables,q);},"undeclared fixed numerical support accepted");
    rejects([&]{Guard q;Prepared bad(owner,fixed,{{4,0,2}},q);},"variable overlaps supported fixed zero");
    rejects([&]{Guard q;Prepared bad(owner,fixed,{{2,0,0},{2,0,0}},q);},"duplicate inactive coordinate accepted");
    rejects([&]{Guard q;Prepared bad(owner,fixed,{{3,0,3}},q);},"rectangular coordinate out of bounds");
    rejects([&]{Guard q;Prepared bad(owner,fixed,variables,M(5,2),q);},"partial parameter basis accepted");
    rejects([&]{Guard q;(void)model.observation_map({1},true,q);},"partial original input accepted");
    rejects([&]{Guard q;(void)model.assemble({{{1,2,3},{1,2,1},{1,0}}},q);},"unobserved target accepted");
    rejects([&]{Guard q;(void)model.assemble({{{1,2,3},{0,0,0},{0,0}}},q);},"empty mask accepted");
    rejects([&]{Guard q;(void)model.assemble({{{1,2,3},{0,0,0},{2,0}}},q);},"nonboolean mask accepted");
    auto limits=numeric::ExactActionLimits{};limits.entries=1;
    rejects([&]{Guard q(limits);Prepared bad(owner,fixed,variables,q);},"preparation entry admission ignored");
    limits={};limits.integer_bits=2;
    rejects([&]{Guard q(limits);(void)model.observation_map({8,0,0},true,q);},"input bit admission ignored");
    limits={};limits.steps=2;
    rejects([&]{Guard q(limits);(void)model.fixed_response({1,2,3},q);},"step admission ignored");
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});(void)model.assemble({{{1,2,3},{0,0,0},{1,0}}},q);},"cancellation ignored");
    M basis(6,0);Prepared zero(owner,fixed,variables,basis,g);
    check(zero.observation_map({1,2,3},true,g).cols()==0,"zero parameter carrier lost");
}
}
int main() {try{actions();admission();std::cout<<checks<<" exact transport observation assertions passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
