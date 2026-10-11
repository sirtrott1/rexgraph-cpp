#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_pattern_scope.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using M=numeric::ExactMatrix;
using V=numeric::RationalVector;
using G=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"invalid scope accepted");}
auto fixture(G& guard) {
    core::RelationSignature signature;
    const auto type=signature.add_type("observed",core::BoundaryPolicy::DeclaredExact,0,3);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<7;++i)builder.add_grade_one(type,{
        {0,true,false,numeric::Rational{1},{}},{0,false,true,numeric::Rational{0},{}},
        {i<3?1u:i<6?2u:3u,true,false,numeric::Rational{1},{}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    for(std::size_t i=0;i<7;++i)stalks.set_dimension({core::Grade{1},static_cast<std::uint32_t>(i)},2);
    local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> variables;
    for(std::size_t i=0;i<21;++i) {
        fibers.set_transport(core::Grade{1},i,M(1,2));
        if(i%3==0)for(std::size_t j=0;j<2;++j)variables.push_back({i,0,j});
    }
    learning::PreparedTransportObservations observations(local::OwnedLinearFiberSystem(fibers),{},variables,guard);
    std::vector<learning::TransportObservation> samples;
    for(std::size_t i=0;i<7;++i) {
        V input(14);input[2*i]=input[2*i+1]=1;
        samples.push_back({std::move(input),{2,0,0,0},{1,0,0,0}});
    }
    std::vector<std::vector<learning::TransportContextView>> atlas(2);
    for(std::size_t i=0;i<4;++i)atlas[0].push_back({i==0?1u:2u+i,M::identity(1),M::identity(1)});
    for(std::size_t i=0;i<7;++i)atlas[1].push_back({2,M::identity(2),M::identity(2)});
    learning::TransportPatternProvenance p;p.source_sha256.fill(std::byte{7});p.evidence_sha256.fill(std::byte{11});
    learning::PreparedTransportEvidence evidence(observations,guard);evidence=evidence.append(samples,guard);
    auto scope=learning::PreparedTransportPatternScope::acquire(learning::TransportContextSamples(evidence),atlas,p,guard);
    auto bank=learning::PreparedTransportPatternBank::learn(evidence,{0,1,2,3,4,5,6},{0,3},atlas,2,
        {100,101,{102,103}},p,guard,learning::TransportContextInputScope::ObservedFields);
    return std::pair{std::move(scope),std::move(bank)};
}
void coverage() {
    G guard;const auto [scope,bank]=fixture(guard);
    const auto expected=std::vector<std::vector<std::size_t>>{{0},{0},{0},{1},{1},{1},{}};
    check(scope.coverage(bank,guard).patterns==expected,"unadmitted original branch disappeared");
    check(scope.provenance().observations==7 && scope.input_images().size()==7,"original observation scope narrowed");
    const auto& arena=scope.endpoint().source.grade(core::Grade{1});
    check(arena.incidence_count()==21 && arena.lower_index[0]==arena.lower_index[1],"repeated boundary identity collapsed");
    check(!arena.aspects.get(scope.endpoint().source.signature().existence_aspect(),1),"inactive occurrence erased");
    check(scope.endpoint().source.incidence_coefficient(core::Grade{1},1)==0,"supported zero coefficient changed");
    for(const auto& image:scope.input_images())check(image.rows()==2 && image.cols()==1,"observed input span flattened");
    const auto bytes=learning::encode_transport_pattern_scope(scope,500);
    auto loaded=learning::decode_transport_pattern_scope(bytes,500,guard);
    check(learning::encode_transport_pattern_scope(loaded,500)==bytes,"cold scope bytes differ");
    check(loaded.coverage(bank,guard).patterns==expected,"cold source coverage differs");
    const auto local=bank.select({0},110,111,guard);
    const auto local_expected=std::vector<std::vector<std::size_t>>{{0},{0},{0},{},{},{},{}};
    check(loaded.coverage(local,guard).patterns==local_expected,"local export claimed excluded branch coverage");
    const auto local_scope=loaded.for_bank(local,guard);
    check(local_scope.input_images().size()==7,"local scope omitted a branch of its retained primary type");
    check(local_scope.coverage(local,guard).patterns==local_expected,"local scope invented branch coverage");
    check(learning::encode_transport_pattern_scope(local_scope,501)!=bytes,"scope namespace was ignored");
    const auto other=bank.select({1},112,113,guard);
    check(loaded.coverage(other,guard).patterns==std::vector<std::vector<std::size_t>>{{},{},{},{0},{0},{0},{}},"local pattern indices are stale");
    auto narrower=bank.endpoint();
    for(auto& t:narrower.tensors)if(t.key>=5){M input(2,1);input.set(0,0,1);t.tensor=storage::temporal_matrix_tensor(input);}
    const auto narrow=learning::PreparedTransportPatternBank::from_endpoint(std::move(narrower),guard);
    for(const auto& patterns:loaded.coverage(narrow,guard).patterns)check(patterns.empty(),"unobserved direction supplied input coverage");
    auto altered=bank.endpoint();altered.source=bank.endpoint().source;
    for(auto& t:altered.tensors)if(t.key==3) {
        auto values=t.tensor.values().dense();values[0]=8;std::vector<numeric::SparseRationalEntry> entries;
        for(std::size_t i=0;i<values.size();++i)if(values[i]!=0)entries.push_back({i,values[i]});
        t.tensor=numeric::SparseRationalTensor(t.tensor.domain(),numeric::StructuralSupport::full(values.size()),
            numeric::SparseRationalVector(values.size(),std::move(entries)));
    }
    const auto foreign=learning::PreparedTransportPatternBank::from_endpoint(std::move(altered),guard);
    rejects([&]{loaded.coverage(foreign,guard);});
    auto carrier=bank.endpoint();
    for(auto& c:carrier.carriers)if(c.key>=3+2*bank.input_images().size() && c.coordinate_system==3)c.coordinate_system=9;
    const auto renamed=learning::PreparedTransportPatternBank::from_endpoint(std::move(carrier),guard);
    rejects([&]{loaded.coverage(renamed,guard);});
    for(std::size_t i=0;i<8;++i) {
        auto job=std::async(std::launch::async,[&]{G g;return loaded.coverage(bank,g).patterns;});
        check(job.get()==expected,"concurrent scope differs");
    }
    rejects([&]{G g;learning::decode_transport_pattern_scope(bytes,501,g);});
    auto truncated=bytes;truncated.pop_back();rejects([&]{G g;learning::decode_transport_pattern_scope(truncated,500,g);});
    auto trailing=bytes;trailing.push_back(std::byte{0});rejects([&]{G g;learning::decode_transport_pattern_scope(trailing,500,g);});
    for(const auto limits:std::vector<numeric::ExactActionLimits>{{1,16000000,64000000,16384},{4096,1,64000000,16384},
        {4096,16000000,1,16384},{4096,16000000,64000000,1}}) {
        rejects([&]{G g(limits);learning::decode_transport_pattern_scope(bytes,500,g);});
        if(limits.integer_bits!=1)rejects([&]{G g(limits);loaded.coverage(bank,g);});
    }
    rejects([&]{G g({},[]{throw std::runtime_error("cancelled");});loaded.coverage(bank,g);});
    for(std::size_t change=0;change<7;++change) {
        auto endpoint=scope.endpoint();
        if(change==0)endpoint.tensors.pop_back();
        if(change==1)endpoint.carriers.pop_back();
        if(change==2)endpoint.sequence=1;
        if(change==3)endpoint.tensors.back().interpretation="foreign";
        if(change==4)endpoint.tensors.front().tensor=storage::temporal_matrix_tensor(M(0,0));
        if(change==5)endpoint.carriers.front().kind=storage::TemporalCarrierKind::Explicit;
        if(change==6)endpoint.tensors.back().tensor=storage::temporal_matrix_tensor(M(2,1));
        rejects([&]{G g;learning::PreparedTransportPatternScope::from_endpoint(std::move(endpoint),g);});
    }
}
}
int main(){try{coverage();std::cout<<checks<<" observed scope checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
