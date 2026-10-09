#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_views.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using M=numeric::ExactMatrix;
using G=numeric::ExactActionGuard;
using View=learning::TransportContextView;
namespace {
std::size_t checks{};
void check(bool value,const char* text) {++checks;if(!value) throw std::runtime_error(text);}
template<class F> void rejects(F f) {bool caught=false;try {f();}catch(const std::exception&) {caught=true;}check(caught,"invalid context view accepted");}
auto fixture(G& guard,bool inactive=false,bool fixed=false,Q magnitude=1) {
    core::RelationSignature signature;const auto role=signature.aspects().ensure("role",composite::AspectRole::RelationRole,false);
    const auto type=signature.add_type("context",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<2;++i) builder.add_grade_one(type,
        {{i,true,false,magnitude,{{role,true}}},{i,!inactive,false,magnitude,{{role,true}}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    stalks.set_dimension({core::Grade{1},0},2);stalks.set_dimension({core::Grade{1},1},3);
    local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> variables,constants;
    for(std::size_t p=0;p<4;++p) {
        M map(1,p<2?2:3);
        for(std::size_t j=0;j<map.cols();++j) {
            if(fixed) {constants.push_back({p,0,j});if(p%2==0 || !inactive) map.set(0,j,p<2?1:2);}
            else variables.push_back({p,0,j});
        }
        fibers.set_transport(core::Grade{1},p,map);
    }
    return learning::PreparedTransportObservations(local::OwnedLinearFiberSystem(fibers),constants,variables,guard);
}
auto atlas() {
    M a(3,2),r(2,3);a.set(0,0,1);a.set(1,1,1);r.set(0,0,1);r.set(1,1,1);
    return std::vector<std::vector<View>>{{{1,M::identity(1),M::identity(1)},{1,M::identity(1),M::identity(1)}},
        {{2,a,r},{2,M::identity(3),M::identity(3)}}};
}
auto samples() {return std::vector<learning::TransportObservation>{{{1,0,0,0,0},{4,0},{1,0}}};}
void common_inputs() {
    G guard;const auto owner=fixture(guard);auto views=atlas();
    const auto acquired=learning::acquire_context_transport_views(owner,samples(),{0,1},views,guard);
    const auto& out=acquired.acquisition;check(out.seed_rows==std::vector<std::size_t>{0},"original input evidence changed");
    check(out.attempts==2 && out.hypotheses.size()==1 && out.relations.size()==4,"rectangular candidate family incomplete");
    const auto& h=out.hypotheses[0];const auto& w=acquired.witnesses[0];
    check(h.cells==std::vector<std::pair<std::size_t,std::size_t>>{{0,1}},"original cell map changed");
    check(h.occurrences==std::vector<std::pair<std::size_t,std::size_t>>{{0,2},{1,3}},"repeated flags collapsed");
    check(w.common_input.rows()==3 && w.common_input.cols()==2,"intersection shape lost");
    check(w.origin_input==M::identity(2) && w.target_input.rows()==3,"original reconstruction incomplete");
    check(w.response_shapes==std::vector<std::pair<std::size_t,std::size_t>>{{1,2},{1,2}},"full response shapes lost");
    learning::PreparedTransportParameterRelations proof(owner,samples(),out.relations,guard);
    check(proof.evidence_kernel().cols()==9 && proof.condition(h.equations,guard).consistent,"unshared parameter directions lost");
    check(proof.condition(h.equations,guard).kernel.cols()==5,"whole shared input conditioning differs");
    views[1][0].accession.set(2,0,1);views[1][0].reconstruction.set(0,0,Q{1,2});views[1][0].reconstruction.set(0,2,Q{1,2});
    const auto redundant=learning::acquire_context_transport_views(owner,samples(),{1},views,guard);
    check(redundant.witnesses[0].common_input.cols()==2,"redundant visible direction became a new original parameter");
    check(numeric::checked_multiply(views[1][0].accession,redundant.witnesses[0].origin_input,guard)==
        redundant.witnesses[0].common_input,"nontranspose reconstruction failed");
    auto disjoint=atlas();disjoint[1][0].accession=M(4,2);disjoint[1][0].reconstruction=M(2,4);
    disjoint[1][1].accession=M(4,3);disjoint[1][1].reconstruction=M(3,4);
    for(std::size_t i=0;i<2;++i){disjoint[1][0].accession.set(i,i,1);disjoint[1][0].reconstruction.set(i,i,1);}
    for(std::size_t i=0;i<3;++i){disjoint[1][1].accession.set(i+1,i,1);disjoint[1][1].reconstruction.set(i,i+1,1);}
    const auto partial=learning::acquire_context_transport_views(owner,samples(),{1},disjoint,guard);
    check(partial.witnesses[0].common_input.cols()==1 && partial.acquisition.relations.size()==2,"partial input overlap flattened");
    disjoint[1][0].accession=M(5,2);disjoint[1][0].reconstruction=M(2,5);
    disjoint[1][1].accession=M(5,3);disjoint[1][1].reconstruction=M(3,5);
    for(std::size_t i=0;i<2;++i){disjoint[1][0].accession.set(i,i,1);disjoint[1][0].reconstruction.set(i,i,1);}
    for(std::size_t i=0;i<3;++i){disjoint[1][1].accession.set(i+2,i,1);disjoint[1][1].reconstruction.set(i,i+2,1);}
    check(learning::acquire_context_transport_views(owner,samples(),{1},disjoint,guard).acquisition.rejections.at("empty-input-intersection")==1,"disjoint inputs supplied a false comparison");
    const auto inactive=fixture(guard,true);const auto inactive_result=learning::acquire_context_transport_views(inactive,samples(),{1},atlas(),guard);
    check(inactive_result.acquisition.relations.size()==4 && inactive_result.acquisition.relations[2].terms[0].coefficient==0,"inactive support erased");
    const auto scaled=fixture(guard,false,false,Q{2,3});
    check(learning::acquire_context_transport_views(scaled,samples(),{1},atlas(),guard).acquisition.relations[0].terms[0].coefficient==Q{2,3},"source coefficient omitted or applied twice");
    const auto constants=fixture(guard,false,true);const auto fixed=learning::acquire_context_transport_views(constants,{}, {1},atlas(),guard);
    check(fixed.acquisition.hypotheses.empty(),"fixed maps invented observations");
    auto observed=samples();observed[0].target[0]=2;
    const auto fixed_result=learning::acquire_context_transport_views(constants,observed,{1},atlas(),guard);
    rejects([&]{learning::PreparedTransportParameterRelations p(constants,observed,fixed_result.acquisition.relations,guard);});
    learning::PreparedTransportParameterRelations fixed_proof(constants,observed,fixed_result.acquisition.relations,guard,
        learning::TransportParameterSupportPolicy::IncludeConstants);
    check(std::all_of(fixed_proof.statuses().begin(),fixed_proof.statuses().end(),[](auto s){return s==learning::TransportParameterRelationStatus::Excluded;}),"constant contradiction treated as a learned variable");
    check(!fixed_proof.condition(fixed_result.acquisition.hypotheses[0].equations,guard).consistent,"constant block contradiction ignored");
    for(long numerator=1;numerator<=25;++numerator) {
        auto changed=atlas();Q value{numerator,29};value.canonicalize();
        changed[1][0].accession.set(0,0,value);changed[1][0].reconstruction.set(0,0,1/value);
        const auto changed_result=learning::acquire_context_transport_views(owner,samples(),{1},changed,guard);
        check(changed_result.witnesses[0].origin_input.get(0,0)==1/value,"rational input reconstruction changed");
        check(changed_result.acquisition.relations[0].terms[0].coefficient==1/value &&
            changed_result.acquisition.relations[0].terms[1].coefficient==-1,"shared response equation ignored rational reconstruction");
        learning::PreparedTransportParameterRelations certified(owner,samples(),changed_result.acquisition.relations,guard);
        check(certified.condition(changed_result.acquisition.hypotheses[0].equations,guard).consistent,"rational chart comparison lost a feasible original family");
    }
    const std::vector<learning::TransportParameterRelation> known{{"supported-zero",{{0,0}},0},{"constant-zero",{},0},{"constant-conflict",{},1}};
    learning::PreparedTransportParameterRelations constant_policy(owner,samples(),known,guard,
        learning::TransportParameterSupportPolicy::IncludeConstants);
    check(constant_policy.statuses()==std::vector<learning::TransportParameterRelationStatus>{
        learning::TransportParameterRelationStatus::Forced,learning::TransportParameterRelationStatus::Forced,
        learning::TransportParameterRelationStatus::Excluded},"supported zero and absent variable support confused");
    check(constant_policy.evidence_kernel()==proof.evidence_kernel(),"constant policy changed original evidence");
    std::vector<std::future<std::size_t>> jobs;
    for(int i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[owner]{G g;return learning::acquire_context_transport_views(owner,samples(),{1},atlas(),g).acquisition.relations.size();}));
    for(auto& job:jobs) check(job.get()==4,"concurrent view comparison changed");
}
void admission() {
    G g;const auto owner=fixture(g);
    for(int kind=0;kind<5;++kind) rejects([&]{auto a=atlas();G guard;
        if(kind==0)a[1][0].reconstruction.set(0,0,2);
        if(kind==1)a[1][0].frame=0;
        if(kind==2)a[0].pop_back();
        if(kind==3)a[1][0].accession=M(4,2);
        if(kind==4)a[1][0].reconstruction=M(2,2);
        (void)learning::acquire_context_transport_views(owner,samples(),{1},a,guard);});
    for(int kind=0;kind<4;++kind) rejects([&]{auto limits=numeric::ExactActionLimits{};
        if(kind==0)limits.dimension=1;
        if(kind==1)limits.entries=1;
        if(kind==2)limits.steps=1;
        if(kind==3)limits.integer_bits=1;
        G guard(limits);(void)learning::acquire_context_transport_views(owner,samples(),{1},atlas(),guard);});
    rejects([&]{G guard({},[]{throw std::runtime_error("cancelled");});(void)learning::acquire_context_transport_views(owner,samples(),{1},atlas(),guard);});
}
}
int main() {try {common_inputs();admission();std::cout<<checks<<" exact context view assertions passed\n";}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
