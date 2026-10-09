#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_parameter_relations.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Prepared=learning::PreparedTransportParameterRelations;
using Status=learning::TransportParameterRelationStatus;
namespace {
std::size_t checks{};
void check(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message) {
    bool refused=false;try{action();}catch(const std::exception&){refused=true;}check(refused,message);
}
learning::PreparedTransportObservations fixture(Guard& guard,bool tied=false) {
    core::RelationSignature signature;
    const auto type=signature.add_type("transport",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
    builder.add_grade_one(type,{{0,true,false,Q{1}},{0,true,false,Q{1}},{1,false,false,Q{9}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    local::LinearRelationalFiberSystem fibers(stalks);
    for(std::size_t p=0;p<3;++p) fibers.set_transport(core::Grade{1},p,M(1,1));
    local::OwnedLinearFiberSystem owner(fibers);
    const std::vector<learning::TransportCoordinate> variables{{0,0,0},{1,0,0},{2,0,0}};
    if(tied) {
        M basis(3,2);basis.set(0,0,1);basis.set(0,1,1);basis.set(1,0,1);basis.set(1,1,1);
        return learning::PreparedTransportObservations(owner,{},variables,basis,guard);
    }
    return learning::PreparedTransportObservations(owner,{},variables,guard);
}
const std::vector<learning::TransportParameterRelation> relations{
    {"sum",{{0,Q{1}},{1,Q{1}}},Q{4}},
    {"wrong-sum",{{0,Q{1}},{1,Q{1}}},Q{5}},
    {"tie",{{0,Q{1}},{1,Q{-1}}},Q{0}},
    {"first-two",{{0,Q{1}}},Q{2}},
    {"first-three",{{0,Q{1}}},Q{3}},
    {"inactive",{{2,Q{1}}},Q{0}},
    {"supported-zero",{{2,Q{0}}},Q{0}}
};
void evidence() {
    Guard g;const auto observations=fixture(g);
    Prepared owner(observations,{{{1},{4,0},{1,0}}},relations,g);
    check(owner.observations().owner().same_owner(observations.owner()),"actual fiber owner lost");
    check(owner.evidence_origin()==V({4,0,0}),"canonical origin differs");
    check(owner.rank()==1 && owner.free_columns()==std::vector<std::size_t>({1,2}),"complete free family lost");
    check(owner.residual_directions().cols()==2,"inactive free direction removed");
    check(owner.statuses()==std::vector<Status>({Status::Forced,Status::Excluded,Status::Open,
        Status::Open,Status::Open,Status::Open,Status::Forced}),"full-family classification differs");
    check(owner.relations().back().terms.size()==1 && owner.relations().back().terms[0].coefficient==0,"supported zero erased");
    const auto selected=owner.condition({2},g);
    check(selected.consistent && selected.origin==V({2,2,0}),"tie intersection differs");
    check(selected.kernel.cols()==1 && selected.kernel.get(2,0)==1,"inactive parameter was fixed silently");
    check(owner.condition({},g).kernel==owner.evidence_kernel(),"empty selection changed evidence");
    check(!owner.condition({1},g).consistent,"excluded relation accepted");
    const auto impossible=owner.condition({3,4},g);
    check(!impossible.consistent && impossible.origin.empty() && impossible.kernel.cols()==0,"joint contradiction has a false family");
    check(owner.evidence_origin()==V({4,0,0}),"condition rewrote evidence");
    auto jobs=std::vector<std::future<V>>{};
    for(std::size_t i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[owner]{Guard local;return owner.condition({2},local).origin;}));
    for(auto& job:jobs) check(job.get()==selected.origin,"concurrent conditional workspace differs");
    for(long i=-25;i<=25;++i) {
        Q target{i,7};target.canonicalize();auto declarations=relations;declarations[0].expected=target;
        Guard q;Prepared rational(observations,{{{Q{2,3}},{target*Q{2,3},0},{1,0}}},declarations,q);
        check(rational.statuses()[0]==Status::Forced,"rational full-family sum not forced");
        check(rational.statuses()[2]==Status::Open,"selected equality mistaken for evidence");
        const auto tie=rational.condition({2},q);
        check(tie.consistent && tie.origin[0]==target/2 && tie.origin[1]==target/2,"rational joint intersection differs");
    }
    Guard q;Prepared empty(observations,{},relations,q);
    check(empty.evidence_kernel()==M::identity(3),"no-evidence complete family lost");
    check(empty.statuses()[0]==Status::Open && empty.statuses()[6]==Status::Forced,"no-evidence status differs");
    Prepared named(fixture(q,true),{{{1},{4,0},{1,0}}},relations,q);
    check(named.statuses()[2]==Status::Forced,"declared basis tie not retained");
    check(named.statuses()[5]==Status::Forced && named.evidence_kernel().cols()==1,"zero row or redundant parameter lost");
    const auto huge=Q{numeric::Integer("123456789012345678901234567890123456789"),numeric::Integer(97)};
    auto large=relations;large[0].expected=huge;
    Prepared big(observations,{{{1},{huge,0},{1,0}}},large,q);
    check(big.statuses()[0]==Status::Forced,"large exact input lost");
    Prepared unreduced(observations,{{{Q{2,2}},{Q{8,2},0},{1,0}}},relations,q);
    check(unreduced.evidence_origin()==owner.evidence_origin(),"noncanonical rational observation changed exact family");
    learning::PreparedTransportObservations zero_parameters(observations.owner(),{},
        observations.variable_coordinates(),M(3,0),q);
    Prepared zero(zero_parameters,{{{1},{0,0},{1,0}}},relations,q);
    check(zero.evidence_origin().empty() && zero.residual_directions().cols()==0,"zero-dimensional parameter carrier lost");
    check(zero.statuses()[2]==Status::Forced && zero.statuses()[0]==Status::Excluded,"zero-dimensional statuses differ");
    check(zero.condition({2},q).consistent && zero.condition({2},q).origin.empty(),"zero-dimensional intersection differs");
}
void admission() {
    Guard g;auto observed=fixture(g);Prepared owner(observed,{{{1},{4,0},{1,0}}},relations,g);
    rejects([&]{Guard q;(void)owner.condition({2,2},q);},"duplicate selection accepted");
    rejects([&]{Guard q;(void)owner.condition({7},q);},"unknown selection accepted");
    rejects([&]{Guard q;Prepared bad(observed,{}, {},q);},"empty relation family accepted");
    rejects([&]{Guard q;Prepared bad(observed,{},{{"x",{{0,1},{0,0}},0}},q);},"duplicate supported zero accepted");
    rejects([&]{Guard q;Prepared bad(observed,{},{{"x",{{3,1}},0}},q);},"foreign map entry accepted");
    rejects([&]{Guard q;Prepared bad(observed,{},{{"x",{},0}},q);},"absent support accepted");
    rejects([&]{Guard q;auto duplicate=relations;duplicate[1].name="sum";Prepared bad(observed,{},duplicate,q);},"duplicate names accepted");
    rejects([&]{Guard q;Prepared bad(observed,{{{0},{1,0},{1,0}}},relations,q);},"inconsistent evidence accepted");
    for(auto limit:{0u,1u,2u,3u}) {
        auto limits=numeric::ExactActionLimits{};
        if(limit==0) limits.entries=1;
        if(limit==1) limits.steps=2;
        if(limit==2) limits.dimension=1;
        if(limit==3) limits.integer_bits=1;
        rejects([&]{Guard q(limits);Prepared bad(observed,{{{1},{4,0},{1,0}}},relations,q);},"preparation admission ignored");
        rejects([&]{Guard q(limits);(void)owner.condition({2},q);},"condition admission ignored");
    }
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});Prepared bad(observed,{},relations,q);},"preparation cancellation ignored");
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});(void)owner.condition({},q);},"condition cancellation ignored");
}
}
int main() {try {evidence();admission();std::cout<<checks<<" exact parameter relation assertions passed\n";}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
