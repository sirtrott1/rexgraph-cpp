#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_rules.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool value,const char* text) {++checks;if(!value) throw std::runtime_error(text);}
template<class F> void rejects(F action) {
    bool rejected=false;try {action();}catch(const std::exception&) {rejected=true;}
    check(rejected,"invalid rule admission accepted");
}
auto fixture(Guard& guard,bool inactive=false,bool repeated=false) {
    core::RelationSignature sig;
    const auto head=sig.aspects().ensure("head",composite::AspectRole::RelationRole,false);
    const auto tail=sig.aspects().ensure("tail",composite::AspectRole::RelationRole,false);
    const auto type=sig.add_type("context",core::BoundaryPolicy::DeclaredExact,0,8);
    const auto other=sig.add_type("other",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(sig);
    for(std::uint64_t row=0;row<6;++row) builder.add_grade_one(row==5?other:type,
        {{2*row,true,true,Q{1},{{head,true}}},
         {2*row+(repeated?0:1),!inactive,false,Q{1,2},{{tail,true}}}});
    const auto source=builder.freeze();local::LinearStalkBundle bundle(source,2);
    local::LinearRelationalFiberSystem fibers(bundle);std::vector<learning::TransportCoordinate> variables;
    for(std::size_t p=0;p<12;++p) {
        fibers.set_transport(core::Grade{1},p,M(2,2));
        for(std::size_t i=0;i<2;++i) for(std::size_t j=0;j<2;++j) variables.push_back({p,i,j});
    }
    return learning::PreparedTransportObservations(local::OwnedLinearFiberSystem(fibers),{},variables,guard);
}
auto atlas(const learning::PreparedTransportObservations& owner) {
    const auto n=owner.owner().fibers().complex().grade(core::Grade{0}).cell_count();
    return std::vector<std::vector<learning::TransportContextFrame>>{
        std::vector<learning::TransportContextFrame>(n,{1,{0,1}}),
        std::vector<learning::TransportContextFrame>(6,{2,{0,1}})};
}
auto samples(const learning::PreparedTransportObservations& owner,std::size_t count=3,bool different=false,bool repeated=false) {
    const auto n=owner.owner().fibers().complex().grade(core::Grade{0}).cell_count();
    const auto& source=owner.owner().fibers().complex();const auto& arena=source.grade(core::Grade{1});
    std::vector<learning::TransportObservation> out;
    for(std::size_t row=0;row<count;++row) for(std::size_t j=0;j<2;++j) {
        learning::TransportObservation s;s.input.resize(12);s.input[2*row+j]=1;
        s.target.resize(2*n);s.observed.resize(n);
        const auto lower=repeated?row:2*row;s.observed[lower]=1;
        if(!repeated)s.observed[lower+1]=1;
        for(std::size_t i=0;i<2;++i) {
            const Q a=i==j?Q{2}:Q{1,3},b=i==j?Q{4}:Q{-2,5};
            const auto offset=different && row==count-1?Q{1}:Q{0};
            s.target[2*lower+i]=-a-offset;
            if(repeated) s.target[2*lower+i]+=b/2;
            else if(arena.aspects.get(source.signature().existence_aspect(),2*row+1)) s.target[2*(lower+1)+i]=b/2;
        }
        out.push_back(std::move(s));
    }
    return out;
}
void learning_rules() {
    Guard g;const auto owner=fixture(g);const auto fields=samples(owner);const auto frames=atlas(owner);
    const auto result=learning::acquire_context_transport_rules(owner,fields,{0,1,2},{3,4,5},frames,2,g);
    check(result.rules.size()==3 && result.selected_blocks.size()==2,"complete learned transfer family differs");
    for(const auto& r:result.rules) check(r.admitted && r.forced.size()==2 && r.excluded.empty() && r.open.empty(),"independent peer witnesses lost");
    std::vector<std::size_t> equations;
    for(const auto b:result.selected_blocks) {
        const auto& block=result.comparisons.hypotheses[b];
        check(block.origin==0 && (block.target==3 || block.target==4),"target selected a different original map");
        check(result.block_statuses[b]==learning::TransportParameterRelationStatus::Open,"unobserved target became evidence");
        check(block.occurrences.size()==2 && block.equations.size()==8,"whole supported block was truncated");
        equations.insert(equations.end(),block.equations.begin(),block.equations.end());
    }
    learning::PreparedTransportParameterRelations proof(owner,fields,result.comparisons.relations,g);
    const auto conditional=proof.condition(equations,g);
    check(proof.evidence_kernel().cols()==24 && conditional.kernel.cols()==8,"evidence or remaining target directions were erased");
    check(conditional.consistent,"learned joint family inconsistent");
    for(std::size_t i=0;i<8;++i) {
        check(conditional.origin[24+i]==proof.evidence_origin()[i],"first conditional target map differs");
        check(conditional.origin[32+i]==proof.evidence_origin()[i],"second conditional target map differs");
    }
    auto counter=samples(owner,4,true);
    const auto contradicted=learning::acquire_context_transport_rules(owner,counter,{0,1,2,3},{4},frames,2,g);
    check(contradicted.selected_blocks.empty(),"compatible counterexample was discarded");
    for(const auto& rule:contradicted.rules) check(!rule.admitted && !rule.excluded.empty(),"counterexample rule admitted");
    const auto target_conflict=learning::acquire_context_transport_rules(owner,counter,{0,1,2},{3},frames,2,g);
    check(target_conflict.selected_blocks.empty(),"contradicted target admitted");
    auto partial=fields;partial.pop_back();
    const auto unresolved=learning::acquire_context_transport_rules(owner,partial,{0,1,2},{3},frames,2,g);
    check(unresolved.selected_blocks.empty(),"unresolved independent peer became identified evidence");
    const auto few=learning::acquire_context_transport_rules(owner,fields,{0,1},{3},frames,2,g);
    check(few.selected_blocks.empty(),"minimum peer admission ignored");
    auto no_frame=frames;no_frame[0][6]={0,{}};
    check(learning::acquire_context_transport_rules(owner,fields,{0,1,2},{3},no_frame,2,g).selected_blocks.empty(),"undeclared target frame invented");
    Guard h;const auto inactive=fixture(h,true);
    check(learning::acquire_context_transport_rules(inactive,samples(inactive),{0,1,2},{3},atlas(inactive),2,h).selected_blocks.empty(),"inactive free support erased");
    Guard k;const auto repeated=fixture(k,false,true);
    const auto aggregate=learning::acquire_context_transport_rules(repeated,samples(repeated,3,false,true),{0,1,2},{3},atlas(repeated),2,k);
    check(aggregate.selected_blocks.empty(),"shared aggregate evidence identified individual occurrence maps");
    const auto mismatch=learning::acquire_context_transport_rules(repeated,samples(repeated,3,true,true),{0,1,2},{3},atlas(repeated),2,k);
    check(!mismatch.rules[0].excluded.empty(),"joint block inconsistency missed when entries were open");
    std::vector<std::future<std::size_t>> jobs;
    for(std::size_t i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[owner,fields,frames]{Guard q;
        return learning::acquire_context_transport_rules(owner,fields,{0,1,2},{3,4},frames,2,q).selected_blocks.size();}));
    for(auto& job:jobs) check(job.get()==2,"parallel owning rule inference differs");
    rejects([&]{Guard q;(void)learning::acquire_context_transport_rules(owner,fields,{0,0},{3},frames,2,q);});
    rejects([&]{Guard q;(void)learning::acquire_context_transport_rules(owner,fields,{0,1,2},{2},frames,2,q);});
    rejects([&]{Guard q;(void)learning::acquire_context_transport_rules(owner,fields,{0,1,3},{4},frames,2,q);});
    rejects([&]{Guard q;(void)learning::acquire_context_transport_rules(owner,fields,{0,1,2},{3},frames,0,q);});
    for(const auto kind:{0,1,2,3}) {
        auto limits=numeric::ExactActionLimits{};
        if(kind==0)limits.dimension=1;
        if(kind==1)limits.entries=1;
        if(kind==2)limits.steps=2;
        if(kind==3)limits.integer_bits=1;
        rejects([&]{Guard q(limits);(void)learning::acquire_context_transport_rules(owner,fields,{0,1,2},{3},frames,2,q);});
    }
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});
        (void)learning::acquire_context_transport_rules(owner,fields,{0,1,2},{3},frames,2,q);});
}
}
int main() {try {learning_rules();std::cout<<checks<<" exact context rule assertions passed\n";}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
