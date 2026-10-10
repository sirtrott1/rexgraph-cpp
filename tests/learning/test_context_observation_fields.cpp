#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_pattern_codec.hpp"
#include "rfc/learning/context_transport_pattern_bank_codec.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using G=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid local observation read accepted");}
auto fixture(G& guard) {
    core::RelationSignature signature;const auto role=signature.aspects().ensure("role",composite::AspectRole::RelationRole,false);
    const auto type=signature.add_type("clause",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<4;++i)builder.add_grade_one(type,
        {{2*i,true,false,Q{1},{{role,true}}},{2*i+1,true,true,Q{1,2},{{role,false}}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    const std::vector<std::size_t> widths{3,2,2,3};
    for(std::size_t i=0;i<widths.size();++i)stalks.set_dimension({core::Grade{1},static_cast<std::uint32_t>(i)},widths[i]);
    local::LinearRelationalFiberSystem fibers(stalks);std::vector<learning::TransportCoordinate> slots;
    for(std::size_t p=0;p<8;++p){M map(1,widths[p/2]);for(std::size_t j=0;j<map.cols();++j)slots.push_back({p,0,j});fibers.set_transport(core::Grade{1},p,map);}
    return learning::PreparedTransportObservations(local::OwnedLinearFiberSystem(fibers),{},slots,guard);
}
auto atlas() {
    std::vector<std::vector<learning::TransportContextView>> out(2);
    for(std::size_t i=0;i<8;++i)out[0].push_back({1,M::identity(1),M::identity(1)});
    for(const auto n:{3u,2u,2u,3u}){M a(4,n),r(n,4);for(std::size_t i=0;i<n;++i){a.set(i,i,1);r.set(i,i,1);}out[1].push_back({2,a,r});}
    return out;
}
numeric::SparseRationalTensor tensor(const V& values) {
    std::vector<numeric::SparseRationalEntry> entries;
    for(std::size_t i=0;i<values.size();++i)if(values[i]!=0)entries.push_back({i,values[i]});
    return {numeric::IndexDomain::linear(values.size()),numeric::StructuralSupport::full(values.size()),numeric::SparseRationalVector(values.size(),std::move(entries))};
}
void compare(const learning::TransportContextViewRuleAcquisition& a,const learning::TransportContextViewRuleAcquisition& b) {
    const auto& x=a.comparisons;const auto& y=b.comparisons;
    check(x.acquisition.seed_rows==y.acquisition.seed_rows && x.acquisition.rejections==y.acquisition.rejections,"original seeds or rejection reasons changed");
    check(a.selected_blocks==b.selected_blocks && a.block_statuses==b.block_statuses && a.block_ranks==b.block_ranks &&
        a.covering_peers==b.covering_peers && a.rejections==b.rejections,"conditional coverage or statuses changed");
    check(x.witnesses.size()==y.witnesses.size() && a.rules.size()==b.rules.size(),"original comparison family changed");
    for(std::size_t i=0;i<x.witnesses.size();++i){const auto& u=x.witnesses[i];const auto& v=y.witnesses[i];
        check(u.common_input==v.common_input && u.origin_input==v.origin_input && u.target_input==v.target_input &&
            u.equation_coordinates==v.equation_coordinates && u.response_shapes==v.response_shapes,"exact local view witness changed");
        const auto& h=x.acquisition.hypotheses[i];const auto& k=y.acquisition.hypotheses[i];
        check(h.origin==k.origin && h.target==k.target && h.cells==k.cells && h.occurrences==k.occurrences &&
            h.equations==k.equations && h.evidence_samples==k.evidence_samples,"occurrence identities or evidence order changed");}
    check(a.proof && b.proof && a.proof->evidence_origin()==b.proof->evidence_origin() &&
        a.proof->evidence_kernel()==b.proof->evidence_kernel() && a.proof->residual_origin()==b.proof->residual_origin() &&
        a.proof->residual_directions()==b.proof->residual_directions(),"complete fitted or conditional parameter family changed");
}
void local_fields() {
    G guard;const auto owner=fixture(guard);learning::PreparedTransportEvidence empty(owner,guard);
    learning::PreparedColumnTransportObservations columns(owner,guard);
    std::vector<learning::TransportObservation> dense;std::vector<learning::TransportColumnObservation> sparse;
    const std::vector<std::size_t> offsets{0,3,5,7,10};
    for(std::size_t row=0;row<3;++row)for(std::size_t axis=0;axis<offsets[row+1]-offsets[row];++axis) {
        const Q value=axis==0?Q{2}:axis==1?Q{3}:Q{5};
        learning::TransportObservation d{V(10),V(8),std::vector<std::size_t>(8)};
        d.input[offsets[row]+axis]=1;d.target[2*row]=value;d.target[2*row+1]=-value;d.observed[2*row]=d.observed[2*row+1]=1;
        V local(offsets[row+1]-offsets[row]);local[axis]=1;
        sparse.push_back(columns.observation({{row,tensor(local)}},{{2*row,tensor({value})},{2*row+1,tensor({-value})}}));dense.push_back(d);
    }
    const auto evidence=empty.append_columns(sparse,guard);check(!evidence.materialized_field_entries(),"acquisition expanded whole fields");
    for(std::size_t sample=0;sample<dense.size();++sample){const auto row=sample<3?0:sample<5?1:2;
        check(evidence.active_input_stalks(sample,guard)==std::vector<std::size_t>{static_cast<std::size_t>(row)},"local active input identity changed");
        for(std::size_t cell=0;cell<4;++cell){const auto local=evidence.input_stalk(sample,cell,guard);
            for(std::size_t i=0;i<local.size();++i)check(local[i]==dense[sample].input[offsets[cell]+i],"local field or supported zero changed");}
        for(std::size_t cell=0;cell<8;++cell)check(evidence.observed_stalk(sample,cell,guard)==bool(dense[sample].observed[cell]),"unobserved stalk became known zero");}
    const auto views=atlas();
    for(const auto scope:{learning::TransportContextInputScope::FullCarrier,learning::TransportContextInputScope::ObservedFields}) {
        G old,newer;const auto a=learning::acquire_context_transport_view_rules(owner,dense,{0,1,2},{3},views,1,old,scope);
        const auto b=learning::acquire_context_transport_view_rules(evidence,{0,1,2},{3},views,1,newer,scope);compare(a,b);
        G first,second;learning::TransportPatternProvenance provenance;
        provenance.source_sha256[0]=std::byte{1};provenance.evidence_sha256[0]=std::byte{2};
        const auto x=learning::PreparedTransportPattern::learn(owner,dense,{0,1,2},0,views,1,{100,101,102},provenance,first,scope);
        const auto y=learning::PreparedTransportPattern::learn(evidence,{0,1,2},0,views,1,{100,101,102},provenance,second,scope);
        check(learning::encode_transport_pattern(x,905)==learning::encode_transport_pattern(y,905),"compact local pattern archive changed");
        G g1,g2;const auto u=learning::PreparedTransportPatternBank::learn(owner,dense,{0,1,2},{0,1},views,1,{101,102,{100,103}},provenance,g1,scope);
        const auto v=learning::PreparedTransportPatternBank::learn(evidence,{0,1,2},{0,1},views,1,{101,102,{100,103}},provenance,g2,scope);
        const auto bytes=learning::encode_transport_pattern_bank(v,905);
        check(bytes==learning::encode_transport_pattern_bank(u,905),"local bank weights, source or joint directions changed");
        check(v.endpoint().source.grade(core::Grade{1}).cell_count()==2 && v.endpoint().source.grade(core::Grade{0}).cell_count()==4,"bank retained unrelated original stalks");
        G cold;const auto loaded=learning::decode_transport_pattern_bank(bytes,905,cold);
        const auto input=V{Q{3,7},Q{-2,5},0,0};G read;const auto response=loaded.response({{0,input}},read);
        G oracle;check(response.origin==u.response({{0,input}},oracle).origin && response.directions==u.response({{0,input}},oracle).directions,"local cold inference differs");
        auto jobs=std::vector<std::future<M>>{};for(std::size_t i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[evidence,views,scope]{G q;return learning::acquire_context_transport_view_rules(evidence,{0,1,2},{3},views,1,q,scope).proof->residual_directions();}));
        for(auto& job:jobs)check(job.get()==b.proof->residual_directions(),"concurrent local comparisons differ");
    }
    check(!evidence.materialized_field_entries(),"local context or export expanded whole fields");
    G mixed;const auto mixed_evidence=empty.append({dense.front()},mixed).append_columns({sparse.begin()+1,sparse.end()},mixed);
    G a,b;compare(learning::acquire_context_transport_view_rules(owner,dense,{0,1,2},{3},views,1,a),
        learning::acquire_context_transport_view_rules(mixed_evidence,{0,1,2},{3},views,1,b));
    M fixed(1,owner.parameter_count());fixed.set(0,0,1);fixed.set(0,14,-1);
    G constrained;const auto pinned=learning::PreparedTransportEvidence(owner,fixed,constrained).append_columns(sparse,constrained);
    const std::vector<learning::TransportParameterRelation> relations{{"held-map",{{14,Q{1}}},Q{2}}};
    G original,conditioned;const learning::PreparedTransportParameterRelations open(evidence,relations,original),forced(pinned,relations,conditioned);
    check(open.statuses()[0]==learning::TransportParameterRelationStatus::Open &&
        forced.statuses()[0]==learning::TransportParameterRelationStatus::Forced,"context proof lost original fixed family constraints");
    check(!pinned.materialized_field_entries(),"conditioned original family expanded observations");
    rejects([&]{G q;(void)evidence.input_stalk(100,0,q);});rejects([&]{G q;(void)evidence.observed_stalk(0,100,q);});
    rejects([&]{G q({},[]{throw std::runtime_error("cancelled");});(void)evidence.input_stalk(0,0,q);});
    auto limits=numeric::ExactActionLimits{};limits.entries=1;rejects([&]{G q(limits);(void)evidence.input_stalk(0,0,q);});
}
}
int main(){try{local_fields();std::cout<<checks<<" local context observation assertions passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
