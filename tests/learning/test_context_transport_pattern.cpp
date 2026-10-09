#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_pattern_codec.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using G=numeric::ExactActionGuard;
using View=learning::TransportContextView;
namespace {
std::size_t checks{};
void check(bool ok,const char* message) {++checks;if(!ok) throw std::runtime_error(message);}
template<class F> void rejects(F&& f) {bool rejected=false;try {f();}catch(const std::exception&) {rejected=true;}
    check(rejected,"invalid pattern accepted");}
auto atlas(bool oblique=false) {
    std::vector<std::vector<View>> views(2);
    for(std::size_t i=0;i<8;++i) views[0].push_back({1,M::identity(1),M::identity(1)});
    for(const auto n:{3u,2u,2u,3u}) {
        M a(4,n),r(n,4);for(std::size_t j=0;j<n;++j) {a.set(j,j,1);r.set(j,j,1);}
        if(oblique) {a.set(3,0,1);r.set(0,0,Q{1,2});r.set(0,3,Q{1,2});}
        views[1].push_back({2,std::move(a),std::move(r)});
    }
    return views;
}
auto observations(G& guard,bool inactive=false) {
    core::RelationSignature signature;const auto type=signature.add_type("clause",core::BoundaryPolicy::DeclaredExact,0,8);
    const auto role=signature.aspects().ensure("participant",composite::AspectRole::RelationRole,false);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<4;++i) builder.add_grade_one(type,
        {{2*i,true,false,Q{1},{{role,true}}},{2*i+1,!inactive,true,Q{1,2},{{role,false}}}});
    const auto source=builder.freeze();local::LinearStalkBundle bundle(source);
    const std::vector<std::size_t> dimensions{3,2,2,3};
    for(std::size_t i=0;i<4;++i) bundle.set_dimension({core::Grade{1},static_cast<std::uint32_t>(i)},dimensions[i]);
    local::LinearRelationalFiberSystem fibers(bundle);std::vector<learning::TransportCoordinate> entries;
    for(std::size_t p=0;p<8;++p) {
        fibers.set_transport(core::Grade{1},p,M(1,dimensions[p/2]));
        for(std::size_t j=0;j<dimensions[p/2];++j) entries.push_back({p,0,j});
    }
    return learning::PreparedTransportObservations(local::OwnedLinearFiberSystem(fibers),{},entries,guard);
}
auto samples(bool inactive=false,std::size_t repetitions=1) {
    std::vector<learning::TransportObservation> out;const std::vector<std::size_t> offsets{0,3,5,7,10};
    for(std::size_t repeat=0;repeat<repetitions;++repeat) for(std::size_t row=0;row<3;++row)
        for(std::size_t j=0;j<offsets[row+1]-offsets[row];++j) {
            learning::TransportObservation s;s.input.resize(10);s.target.resize(8);s.observed.resize(8);
            const Q scale(numeric::Integer(std::to_string(repeat+1)),numeric::Integer(std::to_string(repeat+2)));
            s.input[offsets[row]+j]=scale;const Q value=j==0?Q{2}:j==1?Q{3}:Q{5};
            s.target[2*row]=scale*value;s.target[2*row+1]=inactive?Q{0}:-scale*value;
            s.observed[2*row]=s.observed[2*row+1]=1;out.push_back(std::move(s));
        }
    return out;
}
auto provenance(std::size_t count) {
    learning::TransportPatternProvenance p;p.observations=count;p.peers=2;
    p.source_sha256.fill(std::byte{7});p.evidence_sha256.fill(std::byte{11});return p;
}
auto learn(bool inactive=false,bool oblique=false,std::size_t repetitions=1) {
    G guard;const auto owner=observations(guard,inactive);const auto data=samples(inactive,repetitions);
    return learning::PreparedTransportPattern::learn(owner,data,{0,1,2},0,atlas(oblique),2,{100,101,102},provenance(data.size()),guard);
}
void learned_family() {
    const auto pattern=learn();G guard;
    check(pattern.endpoint().source.grade(core::Grade{1}).cell_count()==1 &&
        pattern.endpoint().source.grade(core::Grade{0}).cell_count()==2,"training source retained instead of its pattern");
    check(pattern.weights()==V({2,3,4,6}),"unscaled learned occurrence weights changed");
    check(pattern.parameter_directions().cols()==0,"irrelevant original unknown parameters retained");
    const V x{Q{2,3},Q{-3,7},0,0};const Q y=2*x[0]+3*x[1];
    const auto response=pattern.response(x,guard);check(response.origin==V({y,-y}),"signed source coefficient applied incorrectly");
    check(response.directions.cols()==0 && response.offsets==std::vector<std::size_t>({0,1,2}),"complete known response family missing");
    check(pattern.response(x,guard,false).origin==V({y,2*y}),"coefficient independent maps discarded");
    rejects([&]{(void)pattern.response({0,0,1,0},guard);});
    rejects([&]{(void)pattern.response({0,0,0,1},guard);});
    rejects([&]{(void)pattern.response({0,0},guard);});
    const auto oblique=learn(false,true);check(oblique.response({x[0],x[1],0,x[0]},guard).origin==response.origin,
        "rectangular represented basis replaced by coordinate dimensions");
    rejects([&]{(void)oblique.response(x,guard);});
    const auto inactive=learn(true);
    check(inactive.parameter_directions().cols()==2,"inactive unscaled map uncertainty collapsed");
    const auto canceled=inactive.response(x,guard);check(canceled.origin==V({y,0}) && !canceled.directions.cols(),
        "inactive boundary coefficient was reapplied to unscaled uncertainty");
    const auto unscaled=inactive.response(x,guard,false);
    check(unscaled.origin==V({y,0}) && unscaled.directions.cols()==1,"unobserved occurrence map was assigned a zero weight");
    check(inactive.response({0,0,0,0},guard,false).directions.cols()==0,"zero input acquired uncertainty");
    std::vector<std::future<V>> workers;
    for(int i=0;i<4;++i) workers.push_back(std::async(std::launch::async,[pattern,x]{G g;return pattern.response(x,g).origin;}));
    for(auto& worker:workers) check(worker.get()==response.origin,"immutable concurrent inference changed");
}
void persistence() {
    const auto model=learn(true);const auto bytes=learning::encode_transport_pattern(model,471);G guard;
    const auto loaded=learning::decode_transport_pattern(bytes,471,guard);
    check(learning::encode_transport_pattern(loaded,471)==bytes,"canonical pattern bytes changed");
    check(loaded.weights()==model.weights() && loaded.parameter_directions()==model.parameter_directions(),"observable affine family changed");
    check(loaded.provenance().source_sha256==model.provenance().source_sha256 && loaded.provenance().peers==2,"external provenance lost");
    const V input{Q{3,5},Q{-8,11},0,0};
    check(loaded.response(input,guard,false).directions==model.response(input,guard,false).directions,"unknown response changed after loading");
    for(std::size_t i=0;i<bytes.size();i+=73) rejects([&]{G g;(void)learning::decode_transport_pattern(std::span(bytes).first(i),471,g);});
    auto corrupt=bytes;corrupt.back()^=std::byte{1};rejects([&]{G g;(void)learning::decode_transport_pattern(corrupt,471,g);});
    rejects([&]{G g;(void)learning::decode_transport_pattern(bytes,472,g);});
    const auto larger=learn(true,false,8);const auto large_bytes=learning::encode_transport_pattern(larger,471);
    check(larger.weights()==model.weights() && larger.parameter_directions()==model.parameter_directions(),"additional compatible probes changed the model");
    check(large_bytes.size()==bytes.size(),"inference artifact grew with observation history");
    std::cout<<"pattern_bytes="<<bytes.size()<<" observations=7,56 weights="<<model.weights().size()
        <<" free_parameters="<<model.parameter_directions().cols()<<'\n';
}
void labels() {
    G guard;const auto original=learn();
    const std::vector<learning::TransportPatternFrameLabel> names{{1,"participant",{"reading"}},
        {2,"clause",{"agency","affectedness","context","redundant"}}};
    const auto named=original.with_frame_labels(names,guard);
    const auto bytes=learning::encode_transport_pattern(named,471);const auto loaded=learning::decode_transport_pattern(bytes,471,guard);
    check(loaded.frame_labels().size()==2 && loaded.frame_labels()[1].axes==names[1].axes,"common coordinate labels lost");
    check(loaded.response({Q{2,3},Q{-3,7},0,0},guard).origin==original.response({Q{2,3},Q{-3,7},0,0},guard).origin,
        "frame labels changed exact inference");
    storage::NativeSeriesLimits encoded;encoded.temporal.retain_encoded_values=true;
    const auto retained=learning::decode_transport_pattern(bytes,471,guard,encoded);
    check(learning::encode_transport_pattern(retained,471)==bytes,"retained encoded coefficient lanes changed canonical model bytes");
    check(retained.response({Q{2,3},Q{-3,7},0,0},guard).origin==loaded.response({Q{2,3},Q{-3,7},0,0},guard).origin,
        "encoded coefficient lanes changed inference");
    for(int kind=0;kind<5;++kind) rejects([&]{auto invalid=names;G g;
        if(kind==0)invalid.pop_back();
        if(kind==1)invalid[0].axes.push_back("other");
        if(kind==2)invalid[1].frame=3;
        if(kind==3)invalid[1].axes[1]=invalid[1].axes[0];
        if(kind==4)invalid[1].name=invalid[0].name;
        (void)original.with_frame_labels(std::move(invalid),g);});
    rejects([&]{G g;(void)named.with_frame_labels(names,g);});
}
void admission() {
    for(int kind=0;kind<8;++kind) rejects([&]{G g;const auto o=observations(g);auto data=samples();auto p=provenance(data.size());auto views=atlas();
        if(kind==0)p.source_sha256={};
        if(kind==1)p.evidence_sha256={};
        if(kind==2)++p.observations;
        if(kind==3)++p.peers;
        if(kind==4) data.pop_back();
        if(kind==5)data.back().target[4]+=1;
        if(kind==6)views[1][0].reconstruction.set(0,0,2);
        (void)learning::PreparedTransportPattern::learn(o,data,{0,1,2},kind==7?3:0,views,2,{100,101,102},p,g);});
    for(int kind=0;kind<6;++kind) rejects([&]{G g;const auto original=learn();auto e=original.endpoint();
        if(kind==0)e.tensors[2].input=2;
        if(kind==1)e.carriers[0].coordinate_system=2;
        if(kind==2)e.tensors[0].interpretation="other";
        if(kind==3)e.tensors[1].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(4),
            numeric::StructuralSupport::sparse(4,{0}),numeric::SparseRationalVector(4,{{0,Q{2}}}));
        if(kind==4)e.sequence=1;
        if(kind==5)e.tensors.pop_back();
        (void)learning::PreparedTransportPattern::from_endpoint(std::move(e),g);});
    const auto p=learn();for(int kind=0;kind<4;++kind) rejects([&]{numeric::ExactActionLimits limits;
        if(kind==0)limits.dimension=1;
        if(kind==1)limits.entries=1;
        if(kind==2)limits.steps=1;
        if(kind==3)limits.integer_bits=1;
        G g(limits);(void)p.response({Q{2,3},Q{-3,7},0,0},g);});
    rejects([&]{G g({},[]{throw std::runtime_error("cancelled");});(void)p.response({0,0,0,0},g);});
}
}
int main() {try {learned_family();persistence();labels();admission();std::cout<<checks<<" exact learned pattern assertions passed\n";}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
