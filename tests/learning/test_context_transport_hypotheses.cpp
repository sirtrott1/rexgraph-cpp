#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_hypotheses.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Frame=learning::TransportContextFrame;
namespace {
std::size_t checks{};
void check(bool value,const char* text) {++checks;if(!value)throw std::runtime_error(text);}
template<class F> void rejects(F action) {bool rejected=false;try {action();}catch(const std::exception&){rejected=true;}check(rejected,"invalid acquisition accepted");}
learning::PreparedTransportObservations fixture(Guard& guard,bool split=false,bool inactive=false,bool fixed=false) {
    core::RelationSignature sig;const auto role=sig.aspects().ensure("participant",composite::AspectRole::RelationRole,false);
    const auto type=sig.add_type("context",core::BoundaryPolicy::DeclaredExact,0,8);
    const auto other=sig.add_type("other",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(sig);builder.declare_grade(core::Grade{2});
    for(std::uint64_t row=0;row<3;++row) builder.add_grade_one(row==2?other:type,
        {{10*row,true,false,Q{1},{{role,true}}},{10*row+(split&&row==1?1:0),!inactive,false,Q{1},{{role,true}}}});
    const auto source=builder.freeze();local::LinearStalkBundle bundle(source,2);local::LinearRelationalFiberSystem fibers(bundle);
    std::vector<learning::TransportCoordinate> variables,constants;
    for(std::size_t p=0;p<6;++p) {
        M map(2,2);
        for(std::size_t i=0;i<2;++i) for(std::size_t j=0;j<2;++j) {
            if(fixed && i==1 && j==1) {constants.push_back({p,i,j});if(p%2==0 || !inactive)map.set(i,j,3);}
            else variables.push_back({p,i,j});
        }
        fibers.set_transport(core::Grade{1},p,map);
    }
    return learning::PreparedTransportObservations(local::OwnedLinearFiberSystem(fibers),constants,variables,guard);
}
auto atlas(const learning::PreparedTransportObservations& owner,bool permute=false) {
    const auto n=owner.owner().fibers().complex().grade(core::Grade{0}).cell_count();
    std::vector<std::vector<Frame>> frames(2);frames[0]=std::vector<Frame>(n,{1,{0,1}});
    frames[1]=std::vector<Frame>(3,{2,{0,1}});
    if(permute) {frames[0][1].axes={1,0};frames[1][1].axes={1,0};}
    return frames;
}
std::vector<learning::TransportObservation> samples() {return {{{1,0,0,0,0,0},{4,6,0,0,0,0},{1,0,0}}};}
void discovery() {
    Guard guard;auto owner=fixture(guard);auto frames=atlas(owner);
    const auto acquired=learning::acquire_context_transport_hypotheses(owner,samples(),{0,1,2},frames,guard);
    check(acquired.seed_rows==std::vector<std::size_t>{0},"seed not acquired from actual field input");
    check(acquired.attempts==3 && acquired.hypotheses.size()==1,"complete bounded candidate family differs");
    check(acquired.rejections.at("original-row")==1 && acquired.rejections.at("primary-type")==1,"rejection scope lost");
    const auto& candidate=acquired.hypotheses[0];
    check(candidate.origin==0 && candidate.target==1 && candidate.cells==std::vector<std::pair<std::size_t,std::size_t>>{{0,1}},"original cell map differs");
    check(candidate.occurrences==std::vector<std::pair<std::size_t,std::size_t>>{{0,2},{1,3}},"repeated flags collapsed");
    check(candidate.evidence_samples==std::vector<std::size_t>{0} && candidate.equations.size()==8,"complete block or evidence accession lost");
    learning::PreparedTransportParameterRelations proof(owner,samples(),acquired.relations,guard);
    check(proof.condition(candidate.equations,guard).consistent,"held out block rejected without target evidence");
    check(std::all_of(proof.statuses().begin(),proof.statuses().end(),[](auto status){return status==learning::TransportParameterRelationStatus::Open;}),"unobserved target treated as learned truth");
    auto no_evidence=learning::acquire_context_transport_hypotheses(owner,{}, {1},frames,guard);
    check(no_evidence.hypotheses.empty() && no_evidence.relations.empty() && no_evidence.attempts==0,"no evidence invented a seed");
    auto unobserved=samples();unobserved[0].observed={0,0,0};unobserved[0].target={0,0,0,0,0,0};
    check(learning::acquire_context_transport_hypotheses(owner,unobserved,{1},frames,guard).seed_rows.empty(),"unobserved field became evidence");
    auto no_input=samples();no_input[0].input={0,0,0,0,0,0};
    check(learning::acquire_context_transport_hypotheses(owner,no_input,{1},frames,guard).seed_rows.empty(),"zero input became contextual evidence");
    const auto permuted=learning::acquire_context_transport_hypotheses(owner,samples(),{1},atlas(owner,true),guard);
    check(permuted.relations[0].terms[1].entry==11,"declared frame permutation ignored");
    frames[0][1]={0,{}};
    check(learning::acquire_context_transport_hypotheses(owner,samples(),{1},frames,guard).hypotheses.empty(),"equal dimensions supplied an undeclared frame");
    auto split=fixture(guard,true);auto split_samples=samples();split_samples[0].observed.push_back(0);split_samples[0].target.insert(split_samples[0].target.end(),2,0);
    check(learning::acquire_context_transport_hypotheses(split,split_samples,{1},atlas(split),guard).rejections.at("shared-identity-pattern")==1,"shared identity split admitted");
    auto inactive=fixture(guard,false,true);const auto inactive_result=learning::acquire_context_transport_hypotheses(inactive,samples(),{1},atlas(inactive),guard);
    check(inactive_result.relations.size()==8 && inactive_result.hypotheses[0].occurrences.size()==2,"inactive support erased");
    learning::PreparedTransportParameterRelations inactive_proof(inactive,samples(),inactive_result.relations,guard);
    check(inactive_proof.evidence_kernel().cols()==22,"inactive free parameters lost");
    auto constants=fixture(guard,false,false,true);auto constant_samples=samples();
    const auto constant_result=learning::acquire_context_transport_hypotheses(constants,constant_samples,{1},atlas(constants),guard);
    check(constant_result.relations.size()==6,"fixed support became variable evidence");
    for(long numerator=-20;numerator<=20;++numerator) {
        auto observed=samples();observed[0].target[0]=Q{numerator,7};observed[0].target[0].canonicalize();
        const auto result=learning::acquire_context_transport_hypotheses(owner,observed,{1},atlas(owner),guard);
        check(result.relations[0].terms[0].entry==0 && result.relations[0].terms[1].entry==8,"proposal depended on target values");
        learning::PreparedTransportParameterRelations certified(owner,observed,result.relations,guard);
        check(certified.condition(result.hypotheses[0].equations,guard).consistent,"rational exact block lost");
    }
    std::vector<std::future<std::size_t>> jobs;
    for(std::size_t i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[owner]{Guard g;return learning::acquire_context_transport_hypotheses(owner,samples(),{1},atlas(owner),g).relations.size();}));
    for(auto& job:jobs) check(job.get()==8,"concurrent discovery differs");
}
void admission() {
    Guard g;const auto owner=fixture(g);const auto frames=atlas(owner);
    rejects([&]{Guard q;auto a=frames;a.pop_back();(void)learning::acquire_context_transport_hypotheses(owner,samples(),{1},a,q);});
    rejects([&]{Guard q;auto a=frames;a[0][0].axes={0,0};(void)learning::acquire_context_transport_hypotheses(owner,samples(),{1},a,q);});
    rejects([&]{Guard q;auto a=frames;a[0][0].frame=0;(void)learning::acquire_context_transport_hypotheses(owner,samples(),{1},a,q);});
    rejects([&]{Guard q;(void)learning::acquire_context_transport_hypotheses(owner,samples(),{1,1},frames,q);});
    rejects([&]{Guard q;(void)learning::acquire_context_transport_hypotheses(owner,samples(),{3},frames,q);});
    rejects([&]{Guard q;auto s=samples();s[0].observed={2,0,0};(void)learning::acquire_context_transport_hypotheses(owner,s,{1},frames,q);});
    rejects([&]{Guard q;auto s=samples();s[0].input.pop_back();(void)learning::acquire_context_transport_hypotheses(owner,s,{1},frames,q);});
    for(auto kind:{0,1,2,3}) {auto bounds=numeric::ExactActionLimits{};
        if(kind==0)bounds.dimension=1;
        if(kind==1)bounds.entries=1;
        if(kind==2)bounds.steps=2;
        if(kind==3)bounds.integer_bits=1;
        rejects([&]{Guard q(bounds);(void)learning::acquire_context_transport_hypotheses(owner,samples(),{1},frames,q);});
    }
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});(void)learning::acquire_context_transport_hypotheses(owner,samples(),{1},frames,q);});
}
void indexed_scope() {
    Guard guard;core::RelationSignature signature;std::vector<core::RelationTypeId> types;
    for(std::size_t i=0;i<1000;++i)types.push_back(signature.add_type("type-"+std::to_string(i),core::BoundaryPolicy::DeclaredExact,0,1));
    core::RelationalComplexBuilder builder(signature);
    for(const auto type:types)builder.add_grade_one(type,{{0,true,false,Q{1},{}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> entries;
    for(std::size_t i=0;i<1000;++i){fibers.set_transport(core::Grade{1},i,M(1,1));entries.push_back({i,0,0});}
    learning::PreparedTransportObservations owner(local::OwnedLinearFiberSystem(fibers),{},entries,guard);
    learning::TransportObservation sample;sample.input=numeric::RationalVector(1000,Q{1});sample.target={1};sample.observed={1};
    std::vector<std::vector<Frame>> frames{{{1,{0}}},std::vector<Frame>(1000,{2,{0}})};
    std::vector<std::size_t> targets;for(std::size_t i=0;i<1000;++i)targets.push_back(i);
    Guard work;const auto result=learning::acquire_context_transport_hypotheses(owner,{sample},targets,frames,work);
    check(result.attempts==1000000 && result.rejections.at("primary-type")==999000 && result.rejections.at("original-row")==1000,
        "indexed context scope changed complete rejection counts");
    check(result.hypotheses.empty() && result.relations.empty() && work.steps()<20000,
        "incompatible context pairs were materialized or scanned");
    auto absent=frames;for(auto& frame:absent[1])frame={};Guard q;
    const auto empty=learning::acquire_context_transport_hypotheses(owner,{sample},{0,1},absent,q);
    check(empty.attempts==2000 && empty.rejections.at("original-row")==2 && empty.rejections.at("primary-type")==1998 &&
        !empty.rejections.contains("undeclared-upper-frame"),"original-row rejection precedence changed for absent frames");
    std::cout<<"indexed_context_scope="<<result.attempts<<" steps="<<work.steps()<<'\n';
}
}
int main() {try {discovery();admission();indexed_scope();std::cout<<checks<<" exact contextual hypothesis assertions passed\n";}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
