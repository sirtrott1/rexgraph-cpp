#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_view_rules.hpp"
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
template<class F> void rejects(F f) {bool caught=false;try {f();}catch(const std::exception&) {caught=true;}check(caught,"invalid view rule accepted");}
auto atlas() {
    std::vector<std::vector<View>> out(2);
    for(std::size_t i=0;i<8;++i) out[0].push_back({1,M::identity(1),M::identity(1)});
    for(const auto n:{3u,2u,2u,3u}) {
        M a(4,n),r(n,4);
        for(std::size_t i=0;i<n;++i) {a.set(i,i,1);r.set(i,i,1);}
        out[1].push_back({2,std::move(a),std::move(r)});
    }
    out[1][3].accession.set(3,0,1);
    out[1][3].reconstruction.set(0,0,Q{1,2});out[1][3].reconstruction.set(0,3,Q{1,2});
    return out;
}
auto fixture(G& guard,bool inactive=false,bool repeated=false) {
    core::RelationSignature signature;
    const auto role=signature.aspects().ensure("role",composite::AspectRole::RelationRole,false);
    const auto type=signature.add_type("clause",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<4;++i) builder.add_grade_one(type,
        {{2*i,true,false,Q{1},{{role,true}}},{repeated?2*i:2*i+1,!inactive,true,Q{1,2},{{role,false}}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    const std::vector<std::size_t> dimensions{3,2,2,3};
    for(std::size_t i=0;i<4;++i) stalks.set_dimension({core::Grade{1},static_cast<std::uint32_t>(i)},dimensions[i]);
    local::LinearRelationalFiberSystem fibers(stalks);std::vector<learning::TransportCoordinate> variables;
    for(std::size_t p=0;p<8;++p) {
        M map(1,dimensions[p/2]);
        for(std::size_t j=0;j<map.cols();++j) variables.push_back({p,0,j});
        fibers.set_transport(core::Grade{1},p,map);
    }
    return learning::PreparedTransportObservations(local::OwnedLinearFiberSystem(fibers),{},variables,guard);
}
auto samples(bool partial=false,bool conflict=false,bool repeated=false) {
    std::vector<learning::TransportObservation> out;const std::vector<std::size_t> offsets{0,3,5,7,10};
    for(std::size_t row=0;row<3;++row) for(std::size_t j=0;j<offsets[row+1]-offsets[row];++j) {
        if(partial && row==2 && j==1) continue;
        learning::TransportObservation observation;observation.input.resize(10);observation.input[offsets[row]+j]=1;
        observation.target.resize(repeated?4:8);observation.observed.resize(observation.target.size());
        const Q value=j==0?Q{2}:j==1?Q{3}:Q{5};
        if(repeated) {observation.target[row]=value-Q{1,2}*2*value;observation.observed[row]=1;}
        else {
            observation.target[2*row]=value+(conflict && row==2?1:0);observation.target[2*row+1]=-value;
            observation.observed[2*row]=observation.observed[2*row+1]=1;
        }
        out.push_back(std::move(observation));
    }
    return out;
}
void faithful_coverage() {
    G guard;const auto owner=fixture(guard);auto views=atlas();
    // Redundant target coordinates retain e0+e3, rather than inventing e3.
    views[1][0].accession.set(3,0,1);views[1][0].reconstruction.set(0,0,Q{1,2});views[1][0].reconstruction.set(0,3,Q{1,2});
    const auto out=learning::acquire_context_transport_view_rules(owner,samples(),{0,1,2},{3},views,2,guard);
    check(out.proof && out.rules.size()==3,"owning native evidence missing");
    check(out.proof->free_columns().size()==6,"unobserved original map entries lost");
    check(std::all_of(out.rules.begin(),out.rules.end(),[](const auto& r){return r.admitted;}),"agreeing independent stars rejected");
    check(out.selected_blocks.size()==1,"conditional target selection incomplete");
    const auto selected=out.selected_blocks[0];const auto& block=out.comparisons.acquisition.hypotheses[selected];
    check(block.origin==1 && block.target==3,"uncovered input direction was extrapolated");
    check(out.covering_peers[selected].size()==2,"independent target input coverage missing");
    check(out.rejections.at("insufficient-independent-input-coverage")==1,"uncovered origin not retained as a rejection");
    const auto family=out.proof->condition(block.equations,guard);
    check(family.consistent && family.kernel.cols()==4,"target novel and redundant image directions confused");
    // The two-dimensional training peers do not represent the redundant e3 leg.
    check(out.comparisons.witnesses[selected].common_input.cols()==1,"actual represented overlap replaced by dimensions");
    for(std::size_t i=0;i<out.comparisons.acquisition.hypotheses.size();++i) {
        const auto& equations=out.comparisons.acquisition.hypotheses[i].equations;
        const auto status=out.proof->condition_status(equations,guard);
        const auto complete=out.proof->condition(equations,guard);
        check(status.consistent==complete.consistent && status.rank==complete.rank,"status-only intersection differs from full family");
    }
    views=atlas();views[1][3].accession.set(3,0,0);views[1][3].reconstruction.set(0,0,1);views[1][3].reconstruction.set(0,3,0);
    const auto plain=learning::acquire_context_transport_view_rules(owner,samples(),{0,1,2},{3},views,2,guard);
    check(plain.selected_blocks.size()==1,"plain rectangular overlap not learned");
    const auto& choice=plain.comparisons.acquisition.hypotheses[plain.selected_blocks[0]];
    const auto conditional=plain.proof->condition(choice.equations,guard);
    check(conditional.kernel.cols()==2 && conditional.rank==4,"new target direction collapsed");
    check(conditional.origin[14]==2 && conditional.origin[15]==3 && conditional.origin[17]==4 && conditional.origin[18]==6,
        "signed rational source coefficient applied incorrectly");
    const auto observed=owner.assemble(samples(),guard);
    check(numeric::checked_apply(observed.design,conditional.origin,guard)==observed.target,"conditioned family changed training evidence");
    check(numeric::checked_multiply(observed.design,conditional.kernel,guard).nonzeros()==0,"conditioned free family changed evidence");
    std::vector<std::future<std::size_t>> jobs;
    for(int i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[owner,views]{G g;return learning::acquire_context_transport_view_rules(
        owner,samples(),{0,1,2},{3},views,2,g).selected_blocks.size();}));
    for(auto& job:jobs) check(job.get()==1,"parallel native view rules changed");
}
void negative_evidence() {
    G guard;const auto owner=fixture(guard);auto views=atlas();
    for(const auto kind:{0,1,2}) {
        const auto out=learning::acquire_context_transport_view_rules(owner,samples(kind==1,kind==0),{0,1,2},{3},views,kind==2?3:2,guard);
        check(out.selected_blocks.empty(),"incomplete or contradictory peers admitted a target");
    }
    const auto inactive=fixture(guard,true);
    auto data=samples();for(auto& sample:data) for(std::size_t i=1;i<sample.target.size();i+=2) sample.target[i]=0;
    const auto zero=learning::acquire_context_transport_view_rules(inactive,data,{0,1,2},{3},views,2,guard);
    check(zero.proof->free_columns().size()==13,"inactive independent entries discarded");
    const auto repeated=fixture(guard,false,true);auto repeated_views=views;repeated_views[0].resize(4);
    const auto ambiguous=learning::acquire_context_transport_view_rules(repeated,samples(false,false,true),{0,1,2},{3},repeated_views,2,guard);
    check(ambiguous.selected_blocks.empty(),"cancelled shared aggregate identified occurrence maps");
}
void admission() {
    G g;const auto owner=fixture(g);const auto views=atlas();
    for(int kind=0;kind<6;++kind) rejects([&]{G guard;auto a=views;
        if(kind==0)a[1][0].reconstruction.set(0,0,2);
        if(kind==1)a[1][0].frame=0;
        if(kind==2)a[0].pop_back();
        (void)learning::acquire_context_transport_view_rules(owner,samples(),kind==3?std::vector<std::size_t>{0,0,1}:
            std::vector<std::size_t>{0,1,2},kind==4?std::vector<std::size_t>{2}:std::vector<std::size_t>{3},a,kind==5?0:2,guard);});
    for(int kind=0;kind<4;++kind) rejects([&]{auto limits=numeric::ExactActionLimits{};
        if(kind==0)limits.dimension=1;
        if(kind==1)limits.entries=1;
        if(kind==2)limits.steps=1;
        if(kind==3)limits.integer_bits=1;
        G guard(limits);(void)learning::acquire_context_transport_view_rules(owner,samples(),{0,1,2},{3},views,2,guard);});
    rejects([&]{G guard({},[]{throw std::runtime_error("cancelled");});(void)learning::acquire_context_transport_view_rules(owner,samples(),{0,1,2},{3},views,2,guard);});
}
void observed_field_spans() {
    G guard;const auto owner=fixture(guard);auto views=atlas();
    views[1][3].accession.set(3,0,0);views[1][3].reconstruction.set(0,0,1);views[1][3].reconstruction.set(0,3,0);
    std::vector<learning::TransportObservation> data;
    const std::vector<std::size_t> offsets{0,3,5,7,10};
    for(std::size_t row=0;row<3;++row) {
        learning::TransportObservation o;o.input.resize(10);o.input[offsets[row]]=o.input[offsets[row]+1]=1;
        o.target.resize(8);o.observed.resize(8);o.target[2*row]=5;o.target[2*row+1]=-5;
        o.observed[2*row]=o.observed[2*row+1]=1;data.push_back(std::move(o));
    }
    const auto full=learning::acquire_context_transport_view_rules(owner,data,{0,1,2},{3},views,2,guard);
    check(full.selected_blocks.empty(),"one observed field identified complete original maps");
    const auto scope=learning::TransportContextInputScope::ObservedFields;
    const auto observed=learning::acquire_context_transport_view_rules(owner,data,{0,1,2},{3},views,2,guard,scope);
    check(observed.selected_blocks.size()==1,"observed common field span was not learned");
    const auto selected=observed.selected_blocks[0];const auto& block=observed.comparisons.acquisition.hypotheses[selected];
    const auto& witness=observed.comparisons.witnesses[selected];
    check(witness.common_input.cols()==1 && witness.origin_input.rows()==3 && witness.target_input.rows()==3,
        "observed image replaced the original stalk carriers");
    check(witness.origin_input.get(0,0)==witness.origin_input.get(1,0) && witness.origin_input.get(2,0)==0,
        "observed mixed field direction was flattened");
    check(observed.covering_peers[selected].size()==2,"observed input lacks independent peers");
    const auto family=observed.proof->condition(block.equations,guard);
    check(family.consistent && family.kernel.rows()==20 && family.kernel.cols()==12,
        "unobserved original transport parameters were lost");
    const auto assembled=owner.assemble(data,guard);
    check(numeric::checked_apply(assembled.design,family.origin,guard)==assembled.target,
        "observed-span transfer changed original evidence");
    check(numeric::checked_multiply(assembled.design,family.kernel,guard).nonzeros()==0,
        "observed-span directions changed original evidence");
    auto conflicting=data;conflicting[2].target[4]=6;
    check(learning::acquire_context_transport_view_rules(owner,conflicting,{0,1,2},{3},views,2,guard,scope).selected_blocks.empty(),
        "contradictory observed semantic fields admitted a rule");
    auto partial=data;partial[2].input[6]=0;partial[2].target[4]=2;partial[2].target[5]=-2;
    check(learning::acquire_context_transport_view_rules(owner,partial,{0,1,2},{3},views,2,guard,scope).selected_blocks.empty(),
        "nonintersecting observed input provided independent coverage");
    auto redundant=data;redundant.insert(redundant.end(),data.begin(),data.end());
    const auto repeated=learning::acquire_context_transport_view_rules(owner,redundant,{0,1,2},{3},views,2,guard,scope);
    check(repeated.selected_blocks.size()==1 && repeated.comparisons.witnesses[repeated.selected_blocks[0]].common_input.cols()==1,
        "repeated observations added field directions");
    rejects([&]{G g;(void)learning::acquire_context_transport_view_rules(owner,data,{0,1,2},{3},views,2,g,
        static_cast<learning::TransportContextInputScope>(99));});
}
void equal_carrier_certificates(){
    for(std::size_t n=0;n<20;++n){
        G guard;M a(n+2,n),r(n,n+2),s(n,n+2);
        for(std::size_t j=0;j<n;++j){
            const Q value{numeric::Integer(j+1),numeric::Integer(7)};
            a.set(j,j,value);a.set(n,j,Q{-2,3});r.set(j,j,1/value);s.set(j,j,1/value);s.set(j,n+1,Q{5,3});
        }
        M joined(a.rows(),2*n);
        for(const auto& [key,q]:a.entries()){joined.set(key.first,key.second,q);joined.set(key.first,n+key.second,-q);}
        const numeric::PreparedExactFrames oracle(joined,guard);
        const auto left=learning::context_view_detail::slice(oracle.kernel(),0,n,guard);
        const auto right=learning::context_view_detail::slice(oracle.kernel(),n,n,guard);
        const auto out=learning::context_view_detail::intersection({17,a,r},{17,a,s},guard,0);
        check(out.common_input==numeric::checked_multiply(a,left,guard),"equal carrier common field differs");
        check(out.origin_input==left && out.target_input==right,"equal carrier coordinate order differs");
        check(out.common_input==a && out.origin_input==M::identity(n),"equal carrier original directions lost");
        if(n){s.set(0,0,0);rejects([&]{(void)learning::context_view_detail::intersection({17,a,r},{17,a,s},guard,0);});}
    }
    numeric::ExactActionLimits limits;limits.dimension=1024;limits.steps=20000;
    const M a=M::identity(256);G fast(limits);
    check(learning::context_view_detail::intersection({17,a,a},{17,a,a},fast,0).common_input==a,"bounded equal carrier failed");
    auto work_limits=limits;work_limits.steps=fast.steps()-1;G work(work_limits);
    rejects([&]{(void)learning::context_view_detail::intersection({17,a,a},{17,a,a},work,0);});
    M scaled=a,reconstruction=a;scaled.set(0,0,2);reconstruction.set(0,0,Q{1,2});
    G complete;check(learning::context_view_detail::intersection({17,scaled,reconstruction},
        {17,scaled,reconstruction},complete,0).common_input==scaled,"scaled equal carrier failed");
    auto bit_limits=limits;bit_limits.integer_bits=1;G bits(bit_limits);
    rejects([&]{(void)learning::context_view_detail::intersection({17,scaled,reconstruction},
        {17,scaled,reconstruction},bits,0);});
    limits.dimension=256;G dimension(limits);
    rejects([&]{(void)learning::context_view_detail::intersection({17,a,a},{17,a,a},dimension,0);});
    limits.dimension=1024;limits.entries=256;G entries(limits);
    rejects([&]{(void)learning::context_view_detail::intersection({17,a,a},{17,a,a},entries,0);});
    G stopped({},[]{throw std::runtime_error("stopped");});
    rejects([&]{(void)learning::context_view_detail::intersection({17,a,a},{17,a,a},stopped,0);});
    std::cout<<"equal_carrier_steps="<<fast.steps()<<'\n';
}
}
int main() {try {faithful_coverage();negative_evidence();admission();observed_field_spans();equal_carrier_certificates();std::cout<<checks<<" exact context view rule assertions passed\n";}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
