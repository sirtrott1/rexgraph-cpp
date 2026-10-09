#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_diagrams.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using G=numeric::ExactActionGuard;
using M=numeric::ExactMatrix;
namespace {
std::size_t checks{};
void check(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F action) {bool caught=false;try {action();}catch(const std::exception&) {caught=true;}check(caught,"invalid diagram accepted");}
auto fixture(G& guard,bool repeat=false,bool inactive=false) {
    core::RelationSignature signature;
    const auto role=signature.aspects().ensure("role",composite::AspectRole::RelationRole,false);
    const auto type=signature.add_type("context",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<4;++i) {
        const auto a=i<2?i:i+1;
        std::vector<core::BoundaryOccurrenceInput> flags{{a,true,true,numeric::Rational{1},{{role,true}}},{a+1,!inactive,false,numeric::Rational{1},{{role,false}}}};
        if(repeat) flags.push_back({a+1,false,false,numeric::Rational{1},{{role,false}}});
        builder.add_grade_one(type,flags);
    }
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> variables;
    for(std::size_t p=0;p<source.grade(core::Grade{1}).incidence_count();++p) {
        fibers.set_transport(core::Grade{1},p,M(1,1));variables.push_back({p,0,0});
    }
    return learning::PreparedTransportObservations(local::OwnedLinearFiberSystem(fibers),{},variables,guard);
}
auto atlas() {
    using V=learning::TransportContextView;
    return std::vector<std::vector<V>>{std::vector<V>(6,{1,M::identity(1),M::identity(1)}),std::vector<V>(4,{2,M::identity(1),M::identity(1)})};
}
auto samples() {return std::vector<learning::TransportObservation>{{{1,1,0,0},{-2,0,3,0,0,0},{1,0,1,0,0,0}}};}
void diagrams() {
    G guard;const auto owner=fixture(guard);
    const auto result=learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},atlas(),guard);
    check(result.stars.acquisition.hypotheses.size()==4,"local star candidates lost");
    check(result.diagrams.size()==1,"shared boundary compatibility failed");
    const auto& d=result.diagrams[0];
    check(d.rows==std::vector<std::pair<std::size_t,std::size_t>>{{0,2},{1,3}},"whole row map changed");
    check(d.cells==std::vector<std::pair<std::size_t,std::size_t>>{{0,3},{1,4},{2,5}},"boundary union map changed");
    check(d.shared_cells==std::vector<std::pair<std::size_t,std::size_t>>{{1,4}},"shared original boundary lost");
    check(d.occurrences==std::vector<std::pair<std::size_t,std::size_t>>{{0,4},{1,5},{2,6},{3,7}},"complete flag map lost");
    check(d.equations.size()==4 && d.blocks.size()==2,"whole equation union incomplete");
    check(result.rejections.at("shared-upper-identity")==2 && result.rejections.at("shared-boundary-identity")==1,"incompatible local assignments hidden");
    learning::PreparedTransportParameterRelations proof(owner,samples(),result.stars.acquisition.relations,guard);
    check(proof.condition(d.equations,guard).consistent,"valid whole comparison excluded");
    check(proof.evidence_kernel().cols()==6,"original evidence narrowed implicitly");
    check(proof.condition(d.equations,guard).kernel.cols()==2,"original unresolved directions lost");
    const auto& fibers=owner.owner().fibers();
    check(fibers.complex().grade(core::Grade{1}).cell_count()==4,"comparison changed source relations");
    auto observations=samples();observations.push_back({{1,0,0,0},{0,2,0,0,0,0},{0,1,0,0,0,0}});
    observations.push_back({{0,1,0,0},{0,-3,0,0,0,0},{0,1,0,0,0,0}});
    observations.push_back({{0,0,1,1},{0,0,0,0,5,0},{0,0,0,0,1,0}});
    const auto joint=learning::acquire_context_transport_diagrams(owner,observations,{{2,3}},atlas(),guard);
    learning::PreparedTransportParameterRelations joint_proof(owner,observations,joint.stars.acquisition.relations,guard);
    check(joint.diagrams.size()==1,"joint obstruction erased structural candidate");
    for(const auto block:joint.diagrams[0].blocks)
        check(joint_proof.condition(joint.stars.acquisition.hypotheses[block].equations,guard).consistent,"individual local candidate is not feasible");
    check(!joint_proof.condition(joint.diagrams[0].equations,guard).consistent,"joint contradiction ignored");
    for(const auto repeat:{false,true}) for(const auto inactive:{false,true}) {
        G g;const auto f=fixture(g,repeat,inactive);
        auto evidence=samples();
        if(inactive) evidence={{{1,1,0,0},{-2,-3,0,0,0,0},{1,1,0,0,0,0}}};
        const auto acquired=learning::acquire_context_transport_diagrams(f,evidence,{{3,2}},atlas(),g);
        check(acquired.diagrams.size()==1,"retained inactive connectivity or patch ordering changed");
        check(acquired.diagrams[0].occurrences.size()==(repeat?6:4),"repeated or inactive flags collapsed");
        check(acquired.diagrams[0].shared_cells.size()==1,"shared stalk inferred from activity only");
    }
    auto changes=atlas();changes[0][4].frame=7;
    const auto absent=learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},changes,guard);
    check(absent.diagrams.empty() && absent.rejections.at("no-observed-star-cover")==1,"missing chart invented a global map");
    const auto empty=learning::acquire_context_transport_diagrams(owner,{},{{2,3}},atlas(),guard);
    check(empty.diagrams.empty() && empty.stars.acquisition.relations.empty(),"empty evidence invented a diagram");
    std::vector<std::future<std::size_t>> jobs;
    for(int i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[owner]{G g;return learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},atlas(),g).diagrams.size();}));
    for(auto& job:jobs) check(job.get()==1,"concurrent immutable acquisition differs");
    for(std::size_t i=0;i<30;++i) {
        G g;const auto acquired=learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},atlas(),g);
        check(acquired.diagrams[0].cells==d.cells,"stable original map changed");
    }
}
void admission() {
    G g;const auto owner=fixture(g);
    for(const auto& patch:std::vector<std::vector<std::size_t>>{{2},{2,2},{2,9},{0,2}})
        rejects([&]{G guard;(void)learning::acquire_context_transport_diagrams(owner,samples(),{patch},atlas(),guard);});
    rejects([&]{G guard;(void)learning::acquire_context_transport_diagrams(owner,samples(),{{2,3},{3,2}},atlas(),guard);});
    for(const auto kind:{0,1,2}) rejects([&]{numeric::ExactActionLimits limits;
        if(kind==0) limits.steps=1;
        if(kind==1) limits.entries=1;
        if(kind==2) limits.dimension=1;
        G guard(limits);(void)learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},atlas(),guard);});
    G measure;const auto expected=learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},atlas(),measure);
    rejects([&]{numeric::ExactActionLimits limits;limits.steps=expected.steps-1;G guard(limits);
        (void)learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},atlas(),guard);});
    std::size_t calls{};
    rejects([&]{G guard({},[&]{if(++calls==expected.steps-1) throw std::runtime_error("cancelled");});
        (void)learning::acquire_context_transport_diagrams(owner,samples(),{{2,3}},atlas(),guard);});
    check(calls==expected.steps-1,"cancellation checkpoint was not honored");
}
}
int main() {try {diagrams();admission();std::cout<<checks<<" checks passed\n";}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
