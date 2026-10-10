#include "rfc/core/builder.hpp"
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
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
M column(std::initializer_list<Q> values){M out(values.size(),1);std::size_t i{};for(const auto& q:values)out.set(i++,0,q);return out;}
Q dot(const V& a,const V& b,G& guard){Q out;for(std::size_t i=0;i<a.size();++i)out=guard.add(out,guard.mul(a[i],b[i]));return out;}
template<class F>void rejects(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"invalid bank accepted");}
auto learn(std::size_t repetitions=1,bool observed_inputs=false) {
    G guard;core::RelationSignature signature;
    const auto a=signature.add_type("context",core::BoundaryPolicy::DeclaredExact,0,1);
    const auto b=signature.add_type("anchor",core::BoundaryPolicy::DeclaredExact,0,1);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<6;++i) builder.add_grade_one(i<3?a:b,{{0,true,false,Q{1},{}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    const auto dimension=observed_inputs?2u:1u;
    for(std::size_t row=0;row<6;++row)stalks.set_dimension({core::Grade{1},static_cast<std::uint32_t>(row)},dimension);
    local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> entries;
    for(std::size_t p=0;p<6;++p){fibers.set_transport(core::Grade{1},p,M(1,dimension));
        for(std::size_t j=0;j<dimension;++j)entries.push_back({p,0,j});}
    learning::PreparedTransportObservations observations(local::OwnedLinearFiberSystem(fibers),{},entries,guard);
    std::vector<learning::TransportObservation> samples;
    for(std::size_t r=0;r<repetitions;++r) {
        for(const auto i:{1u,2u}) {learning::TransportObservation s;s.input=V(6);s.input[0]=-1;s.input[i]=1;s.target={0};s.observed={1};samples.push_back(std::move(s));}
        for(std::size_t i=3;i<6;++i){learning::TransportObservation s;s.input=V(6);s.input[i]=Q{1,2};s.target={1};s.observed={1};samples.push_back(std::move(s));}
    }
    if(observed_inputs)for(auto& s:samples) {
        V input(12);for(std::size_t i=0;i<6;++i)input[2*i]=input[2*i+1]=s.input[i];s.input=std::move(input);
    }
    std::vector<std::vector<learning::TransportContextView>> atlas(2);atlas[0].push_back({1,M::identity(1),M::identity(1)});
    for(std::size_t i=0;i<6;++i)atlas[1].push_back({2,M::identity(dimension),M::identity(dimension)});
    learning::TransportPatternProvenance p;p.source_sha256.fill(std::byte{7});p.evidence_sha256.fill(std::byte{11});
    return learning::PreparedTransportPatternBank::learn(observations,samples,{0,1,2,3,4,5},{0,1,3},atlas,2,{101,102,{100,103,104}},p,guard,
        observed_inputs?learning::TransportContextInputScope::ObservedFields:learning::TransportContextInputScope::FullCarrier)
        .with_frame_labels({{1,"reading",{"value"}},{2,"context",observed_inputs?std::vector<std::string>{"signal","context"}:std::vector<std::string>{"signal"}}},guard);
}
auto comparison(std::string name,std::size_t left,std::size_t right,Q right_gain=1) {
    return learning::TransportPatternComparison{std::move(name),left,right,1,1,1,column({1}),column({right_gain})};
}
void observed_fields() {
    G guard;const auto bank=learn(1,true);
    for(const auto& domain:bank.input_images())check(domain.rows()==2 && domain.cols()==1 &&
        domain.get(0,0)!=0 && domain.get(0,0)==domain.get(1,0),"observed bank discarded complete input carriers");
    check(bank.parameter_directions().cols()==1 && bank.response({{2,{1,1}}},guard).origin==V{Q{2}},"observable correlation changed");
    const auto cancelled=bank.response({{0,{1,1}},{1,{-1,-1}}},guard);
    check(cancelled.origin==V{Q{0}} && !cancelled.directions.cols(),"observed joint directions were independently filled");
    check(bank.response({{0,{1,1}}},guard).directions.cols()==1,"observable unknown became a known weight");
    rejects([&]{(void)bank.response({{0,{1,0}}},guard);});
    const auto glued=bank.glue({{0,{1,1}},{2,{1,1}}},{comparison("anchor",0,2)},guard);
    check(glued.section->origin==V{Q{2}} && glued.boundary_response->origin==V{Q{4}},"observed gluing replaced boundary addition");
    const auto bytes=learning::encode_transport_pattern_bank(bank,476);
    const auto loaded=learning::decode_transport_pattern_bank(bytes,476,guard);
    check(loaded.parameter_directions()==bank.parameter_directions() && loaded.input_images()==bank.input_images(),"cold observed bank changed domains or parameters");
    check(loaded.frame_labels()[1].axes==std::vector<std::string>({"signal","context"}),"unobserved direction label was discarded");
    rejects([&]{(void)loaded.response({{0,{1,0}}},guard);});
    const auto larger=learn(100,true);
    check(larger.weights()==bank.weights() && larger.parameter_directions()==bank.parameter_directions(),"repeated fields changed observable parameters");
    check(learning::encode_transport_pattern_bank(larger,476).size()<=bytes.size()+8,"observed bank stored its samples");
}
learning::TransportPatternSparseInput sparse(std::size_t pattern,std::size_t size,
    std::vector<numeric::SparseRationalEntry> entries,std::uint64_t frame=2) {
    return {pattern,frame,numeric::SparseRationalTensor(numeric::IndexDomain::linear(size),
        numeric::StructuralSupport::full(size),numeric::SparseRationalVector(size,std::move(entries)))};
}
void sparse_fields() {
    const auto bank=learn(1,true);G guard;
    const auto a=sparse(0,2,{{0,Q{3,7}},{1,Q{3,7}}}),b=sparse(1,2,{{0,Q{-3,7}},{1,Q{-3,7}}});
    const auto dense=bank.response({{0,{Q{3,7},Q{3,7}}},{1,{Q{-3,7},Q{-3,7}}}},guard);
    const auto field=bank.response_sparse({a,b},guard);
    check(field.origin==dense.origin && field.directions==dense.directions && field.coordinates==dense.coordinates && field.offsets==dense.offsets,
        "sparse input changed the complete joint family");
    const auto zero=bank.response_sparse({sparse(0,2,{})},guard);
    check(zero.origin==V{Q{0}} && !zero.directions.cols(),"complete supported sparse zero became unknown");
    const auto anchor=sparse(2,2,{{0,1},{1,1}}),context=sparse(0,2,{{0,1},{1,1}});
    const auto glued=bank.glue_sparse({context,anchor},{comparison("anchor",0,2)},guard);
    const auto old=bank.glue({{0,{1,1}},{2,{1,1}}},{comparison("anchor",0,2)},guard);
    check(glued.parameter_origin==old.parameter_origin && glued.parameter_directions==old.parameter_directions &&
        glued.section->origin==old.section->origin && glued.boundary_response->origin==old.boundary_response->origin,
        "sparse gluing changed conditional sections or boundary assembly");
    const auto conflict=bank.glue_sparse({context,anchor},{comparison("first",0,2),comparison("second",0,2,Q{1,2})},guard);
    check(!conflict.consistent && !conflict.section && dot(conflict.defect_origin,conflict.contradiction,guard)!=0,
        "sparse contradiction lost its exact witness");
    const std::vector<std::vector<learning::TransportPatternSparseInput>> batch{{a,b},{anchor},{sparse(0,2,{})}};
    const auto all=bank.response_sparse_batch(batch,guard);
    check(all.size()==3 && all[0].origin==field.origin && all[1].origin==V{Q{2}} && all[2].directions.cols()==0,
        "sparse batch changed a field response");
    rejects([&]{G g;(void)bank.response_sparse_batch({},g);});
    rejects([&]{G g;(void)bank.response_sparse_batch({{a},{sparse(0,2,{{0,1}})}},g);});
    rejects([&]{G g;(void)bank.response_sparse({sparse(0,2,{{0,1},{1,1}},99)},g);});
    rejects([&]{G g;(void)bank.response_sparse({sparse(0,1,{{0,1}})},g);});
    rejects([&]{G g;(void)bank.response_sparse({a,a},g);});
    rejects([&]{auto partial=a;partial.value=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(2),
        numeric::StructuralSupport::sparse(2,{}));G g;(void)bank.response_sparse({partial},g);});
    G counted;(void)bank.response_sparse({a},counted);
    numeric::ExactActionLimits limits;limits.steps=counted.steps()+4;
    rejects([&]{G g(limits);(void)bank.response_sparse_batch({{a},{a}},g);});
    limits={};limits.entries=bank.retained_entries()+10;
    rejects([&]{G g(limits);(void)bank.response_sparse_batch({{a},{a},{a},{a}},g);});
    rejects([&]{G g({},[]{throw std::runtime_error("cancelled");});(void)bank.response_sparse({a},g);});
    const auto bytes=learning::encode_transport_pattern_bank(bank,479);
    const auto loaded=learning::decode_transport_pattern_bank(bytes,479,guard);
    check(loaded.response_sparse({a,b},guard).directions==field.directions &&
        learning::encode_transport_pattern_bank(loaded,479)==bytes,"sparse execution changed canonical bank storage");
    std::vector<std::future<V>> jobs;
    for(int i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[loaded,anchor]{G g;return loaded.response_sparse({anchor},g).origin;}));
    for(auto& job:jobs)check(job.get()==V{Q{2}},"concurrent sparse owning inference changed");
    auto endpoint=bank.endpoint();const std::size_t width=65536;
    endpoint.tensors.erase(std::remove_if(endpoint.tensors.begin(),endpoint.tensors.end(),[](const auto& t){return t.key==4;}),endpoint.tensors.end());
    for(auto& c:endpoint.carriers)if(c.key>=6 && c.key<=8){c.domain=numeric::IndexDomain::linear(width);c.coordinate_system=999;}
    for(auto& t:endpoint.tensors)if(t.key>=5){auto old_image=storage::temporal_tensor_matrix(t);M image(width,old_image.cols());
        for(const auto& [key,q]:old_image.entries())image.set(key.first?width-1:0,key.second,q);t.tensor=storage::temporal_matrix_tensor(image);}
    limits={};limits.dimension=width;G wide_guard(limits);
    const auto wide=learning::PreparedTransportPatternBank::from_endpoint(std::move(endpoint),wide_guard);
    G small_work(limits);const auto answer=wide.response_sparse({sparse(2,width,{{0,Q{3,7}},{width-1,Q{3,7}}},999)},small_work);
    check(answer.origin==V{Q{6,7}} && small_work.steps()<1000,"sparse inference scanned the ambient carrier");
    std::cout<<"sparse_carrier="<<width<<" nonzeros=2 action_steps="<<small_work.steps()<<'\n';
}
void family() {
    const auto bank=learn();G guard;
    check(bank.endpoint().source.grade(core::Grade{1}).cell_count()==3 && bank.endpoint().source.grade(core::Grade{0}).cell_count()==1,
        "shared source boundary was duplicated");
    check(bank.weights()==V({0,0,2}) && bank.parameter_directions()==column({1,1,0}),"learned joint correlation lost");
    const auto cancelled=bank.response({{0,{1}},{1,{-1}}},guard);
    check(cancelled.origin==V{Q{0}} && cancelled.directions.cols()==0,"independent local families invented a nonzero response");
    const auto field=bank.response({{0,{1}},{1,{1}},{2,{1}}},guard);
    check(field.origin==V{Q{2}} && field.directions.cols()==1,"complete field family missing");
    const auto unscaled=bank.response({{0,{1}},{1,{1}},{2,{1}}},guard,false);
    check(unscaled.origin==V({0,0,2}) && unscaled.directions==column({1,1,0}),"unscaled occurrence readings merged");
    check(bank.response({{0,{0}},{1,{0}},{2,{0}}},guard).directions.cols()==0,"zero input acquired unknowns");
    std::vector<std::future<V>> jobs;
    for(int i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[bank]{G g;return bank.response({{2,{Q{3,7}}}},g).origin;}));
    for(auto& job:jobs)check(job.get()==V({Q{6,7}}),"owning concurrent bank inference changed");
}
void gluing() {
    const auto bank=learn();G guard;
    const auto forced=bank.glue({{0,{1}},{1,{1}}},{comparison("shared",0,1)},guard,false);
    check(forced.consistent && forced.forced && forced.parameter_directions==M::identity(1),"forced common reading changed its kernel");
    check(forced.section && forced.section->directions==column({1}),"global glued section retained duplicate copies");
    const auto conditional=bank.glue({{0,{1}},{2,{1}}},{comparison("anchor",0,2)},guard);
    check(conditional.consistent && !conditional.forced && conditional.parameter_origin==V{Q{2}} && conditional.parameter_directions.cols()==0,
        "context conditioning did not retain the exact solution family");
    check(conditional.boundary_response && conditional.boundary_response->origin==V{Q{4}} && !conditional.boundary_response->directions.cols(),"glued prospective response wrong");
    check(conditional.section && conditional.section->origin==V{Q{2}} && conditional.section_cells==std::vector<std::size_t>{0},"shared section was added instead of identified once");
    const auto impossible=bank.glue({{0,{1}},{2,{1}}},{comparison("first",0,2),comparison("second",0,2,Q{1,2})},guard);
    check(!impossible.consistent && !impossible.forced && !impossible.boundary_response,"contradictory context produced a model answer");
    check(impossible.defect_origin==V({-2,-2,-1}) && impossible.defect_directions==column({1,1,1}),"complete contradiction fields missing");
    check(numeric::checked_transpose_apply(impossible.defect_directions,impossible.contradiction,guard)==V{Q{0}} &&
        dot(impossible.defect_origin,impossible.contradiction,guard)!=0,"contradiction witness does not certify unsolvability");
    const auto fixed=bank.glue({{2,{1}}},{comparison("fixed",2,2,2)},guard);
    check(!fixed.consistent && fixed.contradiction==V{Q{1}},"fixed incompatible reading was discarded");
    const auto identical=bank.glue({{0,{1}}},{comparison("identity",0,0)},guard);
    check(identical.forced && identical.boundary_response->directions.cols()==1,"identical restriction erased free parameters");
    const auto after=bank.response({{0,{1}}},guard);
    check(after.directions.cols()==1 && bank.weights()[0]==0,"prospective gluing mutated the learned model");
    for(int kind=0;kind<6;++kind)rejects([&]{auto c=comparison("invalid",0,2);G g;
        if(kind==0)c.left_frame=99;if(kind==1)c.right_frame=99;if(kind==2)c.interface_frame=0;
        if(kind==3)c.left=99;if(kind==4)c.left_map=M(2,1);if(kind==5)c.right_map=M(1,2);
        (void)bank.glue({{0,{1}},{2,{1}}},{c},g);});
    rejects([&]{G g;(void)bank.glue({{0,{1}}},{comparison("missing",0,2)},g);});
    rejects([&]{G g;(void)bank.glue({{0,{1}},{2,{1}}},{comparison("same",0,2),comparison("same",0,2)},g);});
}
void persistence() {
    const auto bank=learn();G guard;const auto bytes=learning::encode_transport_pattern_bank(bank,471);
    const auto loaded=learning::decode_transport_pattern_bank(bytes,471,guard);
    check(learning::encode_transport_pattern_bank(loaded,471)==bytes,"canonical bank bytes changed");
    check(loaded.weights()==bank.weights() && loaded.parameter_directions()==bank.parameter_directions(),"stored joint family changed");
    check(loaded.frame_labels().size()==2 && loaded.frame_labels()[0].axes==std::vector<std::string>({"value"}),"named bank carriers lost");
    check(loaded.provenance().observations==5 && loaded.provenance().peers==6,"joint proof population lost");
    check(loaded.response({{0,{1}},{1,{-1}}},guard).directions.cols()==0,"cold load lost parameter correlation");
    check(loaded.glue({{0,{1}},{2,{1}}},{comparison("anchor",0,2)},guard).boundary_response->origin==V{Q{4}},"cold gluing changed");
    storage::NativeSeriesLimits policy;policy.temporal.retain_encoded_values=true;
    const auto encoded=learning::decode_transport_pattern_bank(bytes,471,guard,policy);
    check(learning::encode_transport_pattern_bank(encoded,471)==bytes,"encoded coefficient lanes changed bank bytes");
    check(encoded.response({{0,{1}},{1,{-1}}},guard).directions.cols()==0,"encoded lanes erased correlations");
    for(std::size_t i=0;i<bytes.size();i+=89)rejects([&]{G g;(void)learning::decode_transport_pattern_bank(std::span(bytes).first(i),471,g);});
    rejects([&]{G g;(void)learning::decode_transport_pattern_bank(bytes,472,g);});
    auto corrupt=bytes;corrupt.back()^=std::byte{1};rejects([&]{G g;(void)learning::decode_transport_pattern_bank(corrupt,471,g);});
    const auto larger=learn(100);const auto large_bytes=learning::encode_transport_pattern_bank(larger,471);
    check(larger.weights()==bank.weights() && larger.parameter_directions()==bank.parameter_directions(),"compatible observations changed joint learning");
    check(large_bytes.size()<=bytes.size()+8,"model bank retained its observation history");
    std::cout<<"bank_bytes="<<bytes.size()<<" observations=5,500 patterns=3 weights=3 joint_parameters=1\n";
}
void admission() {
    const auto bank=learn();
    rejects([&]{G g;(void)bank.response({},g);});rejects([&]{G g;(void)bank.response({{0,{1}},{0,{2}}},g);});
    rejects([&]{G g;(void)bank.response({{3,{1}}},g);});rejects([&]{G g;(void)bank.response({{0,{1,2}}},g);});
    for(int kind=0;kind<7;++kind)rejects([&]{auto endpoint=bank.endpoint();G g;
        if(kind==0)endpoint.sequence=1;
        if(kind==1)endpoint.carriers[2].coordinate_system=101;
        if(kind==2)endpoint.tensors[1].input=1;
        if(kind==3)endpoint.tensors.pop_back();
        if(kind==4)endpoint.tensors[0].interpretation="other";
        if(kind==5)endpoint.tensors[2].tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(66),numeric::StructuralSupport::full(66));
        if(kind==6)endpoint.carriers.back().coordinate_system=102;
        (void)learning::PreparedTransportPatternBank::from_endpoint(std::move(endpoint),g);});
    for(int kind=0;kind<4;++kind)rejects([&]{numeric::ExactActionLimits limits;
        if(kind==0)limits.dimension=1;if(kind==1)limits.entries=1;if(kind==2)limits.steps=1;if(kind==3)limits.integer_bits=1;
        G g(limits);(void)bank.response({{0,{Q{3,5}}}},g);});
    rejects([&]{G g({},[]{throw std::runtime_error("cancelled");});(void)bank.response({{0,{1}}},g);});
}
void occurrence_states() {
    G guard;core::RelationSignature signature;
    const auto type=signature.add_type("cancellation",core::BoundaryPolicy::DeclaredExact,4,4);
    core::RelationalComplexBuilder builder(signature);
    for(int i=0;i<3;++i)builder.add_grade_one(type,{{0,true,true,Q{1},{}},{0,true,false,Q{1},{}},
        {0,true,false,Q{0},{}},{0,false,false,Q{7},{}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> entries;
    for(std::size_t p=0;p<12;++p){fibers.set_transport(core::Grade{1},p,M(1,1));entries.push_back({p,0,0});}
    learning::PreparedTransportObservations observations(local::OwnedLinearFiberSystem(fibers),{},entries,guard);
    std::vector<learning::TransportObservation> samples;
    for(std::size_t row=0;row<3;++row){learning::TransportObservation s;s.input=V(3);s.input[row]=1;s.target={0};s.observed={1};samples.push_back(std::move(s));}
    std::vector<std::vector<learning::TransportContextView>> atlas(2);atlas[0].push_back({1,M::identity(1),M::identity(1)});
    for(int i=0;i<3;++i)atlas[1].push_back({2,M::identity(1),M::identity(1)});
    learning::TransportPatternProvenance provenance;provenance.source_sha256.fill(std::byte{7});provenance.evidence_sha256.fill(std::byte{11});
    rejects([&]{G g;(void)learning::PreparedTransportPatternBank::learn(observations,samples,{0,1,2},{0,1},atlas,2,{101,102,{100,103}},provenance,g);});
    entries.clear();std::vector<learning::TransportCoordinate> fixed_support;
    for(std::size_t p=0;p<12;++p){M fixed(1,1);if(p%4<2){fixed.set(0,0,p%4==0?-3:3);fixed_support.push_back({p,0,0});}else entries.push_back({p,0,0});fibers.set_transport(core::Grade{1},p,std::move(fixed));}
    learning::PreparedTransportObservations declared(local::OwnedLinearFiberSystem(fibers),fixed_support,entries,guard);
    const auto bank=learning::PreparedTransportPatternBank::learn(declared,samples,{0,1,2},{0,1},atlas,2,{101,102,{100,103}},provenance,guard);
    const auto& retained=bank.endpoint().source;const auto& arena=retained.grade(core::Grade{1});
    check(arena.incidence_count()==8 && retained.grade(core::Grade{0}).cell_count()==1,"repeated boundary occurrence was expanded or dropped");
    check(arena.occurrence_identities==std::vector<core::OccurrenceKey>(source.grade(core::Grade{1}).occurrence_identities.begin(),
        source.grade(core::Grade{1}).occurrence_identities.begin()+8),"original occurrence identities changed");
    check(arena.aspects.get(retained.signature().orientation_aspect(),0) && !arena.aspects.get(retained.signature().orientation_aspect(),1),"signed orientation changed");
    check(arena.aspects.get(retained.signature().existence_aspect(),2) && !arena.aspects.get(retained.signature().existence_aspect(),3),"supported zero and inactive occurrence were merged");
    check(retained.incidence_coefficient(core::Grade{1},2)==0 && retained.incidence_coefficient(core::Grade{1},3)==0 &&
        arena.lattice_coefficient[3]==source.grade(core::Grade{1}).lattice_coefficient[3],"canonical inactive coefficient or supported zero changed");
    check(bank.weights().size()==8 && bank.parameter_directions().cols()==4,"unobserved occurrence maps acquired weights");
    const auto boundary=bank.response({{0,{1}},{1,{2}}},guard);
    check(boundary.origin==V{Q{0}} && !boundary.directions.cols(),"signed cancellation invented a boundary reading");
    const auto unscaled=bank.response({{0,{1}},{1,{2}}},guard,false);
    check(unscaled.origin==V({3,3,0,0,6,6,0,0}) && unscaled.directions.cols()==4,"zero or inactive map uncertainty was erased");
    const auto bytes=learning::encode_transport_pattern_bank(bank,473);const auto loaded=learning::decode_transport_pattern_bank(bytes,473,guard);
    check(learning::encode_transport_pattern_bank(loaded,473)==bytes && loaded.response({{0,{1}},{1,{2}}},guard,false).directions==unscaled.directions,
        "stored occurrence states lost their full field family");
}
}
int main(){try{family();gluing();persistence();admission();occurrence_states();observed_fields();sparse_fields();std::cout<<checks<<" exact pattern bank assertions passed\n";}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
