#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_pattern_bank_codec.hpp"
#include "rfc/learning/transport_parameter_basis.hpp"
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using M=numeric::ExactMatrix;
using V=numeric::RationalVector;
using G=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid declared bank accepted");}
auto tensor(const V& values){
    std::vector<numeric::SparseRationalEntry> entries;
    for(std::size_t i=0;i<values.size();++i)if(values[i]!=0)entries.push_back({i,values[i]});
    return numeric::SparseRationalTensor(numeric::IndexDomain::linear(values.size()),numeric::StructuralSupport::full(values.size()),
        numeric::SparseRationalVector(values.size(),std::move(entries)));
}
void declared_family(bool dependent=false){
    G guard;core::RelationSignature signature;
    const auto type=signature.add_type("observed-context",core::BoundaryPolicy::DeclaredExact,1,1);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<3;++i)builder.add_grade_one(type,{{i,true,false,Q{1},{}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    for(std::size_t i=0;i<3;++i)stalks.set_dimension({core::Grade{1},static_cast<std::uint32_t>(i)},4);
    local::LinearRelationalFiberSystem fibers(stalks);std::vector<learning::TransportCoordinate> entries;
    for(std::size_t i=0;i<3;++i){fibers.set_transport(core::Grade{1},i,M(1,4));for(std::size_t j=0;j<4;++j)entries.push_back({i,0,j});}
    M basis(12,dependent?5:4);for(std::size_t i=0;i<12;++i){basis.set(i,i%4,1);if(dependent && i%4==0)basis.set(i,4,1);}
    const local::OwnedLinearFiberSystem owner(fibers);
    const learning::PreparedTransportObservations model(owner,{},entries,basis,guard);
    std::vector<learning::TransportColumnObservation> samples;
    const std::vector<V> inputs{{1,1,0,0},{1,0,1,0},{0,1,1,0}};
    const std::vector<Q> outputs{2,3,5};
    for(std::size_t i=0;i<3;++i)samples.push_back({owner,core::Grade{1},{{i,tensor(inputs[i])}},{{i,tensor(V{outputs[i]})}}});
    const learning::PreparedTransportEvidence empty(model,guard);
    const auto evidence=empty.append_columns(samples,guard);
    std::vector<std::vector<learning::TransportContextView>> atlas(2);
    for(std::size_t i=0;i<3;++i){atlas[0].push_back({1,M::identity(1),M::identity(1)});atlas[1].push_back({2,M::identity(4),M::identity(4)});}
    rejects([&]{(void)learning::acquire_context_transport_view_rules(evidence,{0,1,2},{},atlas,2,guard);});
    learning::TransportPatternProvenance provenance;provenance.source_sha256.fill(std::byte{7});provenance.evidence_sha256.fill(std::byte{11});
    rejects([&]{(void)learning::PreparedTransportPatternBank::learn(evidence,{0,1,2},{0},atlas,2,{101,102,{103}},provenance,guard);});
    auto bank=learning::PreparedTransportPatternBank::learn(evidence,{0,1,2},{0},atlas,2,{101,102,{103}},provenance,guard,
        learning::TransportContextInputScope::FullCarrier,true);
    check(bank.provenance().declared_templates,"declared provenance lost");
    check(bank.parameter_directions().cols()==1,"unobserved coordinate lost");
    const auto response=bank.response({{0,V{1,1,1,0}}},guard,false);
    check(response.origin==V{5} && response.directions.cols()==0,"observed span response differs");
    check(bank.response({{0,V{0,0,0,1}}},guard,false).directions.cols()==1,"unsupported prediction identified");
    const auto bytes=learning::encode_transport_pattern_bank(bank,996901);
    auto restored=learning::decode_transport_pattern_bank(bytes,996901,guard);
    check(restored.provenance().declared_templates,"cold template provenance lost");
    check(learning::encode_transport_pattern_bank(restored,996901)==bytes,"noncanonical cold bank");
    const auto selected=bank.select({0},201,202,guard);
    check(selected.provenance().declared_templates,"local template provenance lost");
    auto broken=bank.endpoint();
    for(auto& t:broken.tensors)if(t.key==3){t.interpretation="rfc.context-transport-pattern-bank.v1";}
    rejects([&]{(void)learning::PreparedTransportPatternBank::from_endpoint(broken,guard);});
    broken=bank.endpoint();
    for(auto& t:broken.tensors)if(t.key==3){
        std::vector<numeric::SparseRationalEntry> kept;
        for(const auto& e:t.tensor.values().entries())if(e.index!=66)kept.push_back(e);
        t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(67),numeric::StructuralSupport::full(67),
            numeric::SparseRationalVector(67,std::move(kept)));
    }
    rejects([&]{(void)learning::PreparedTransportPatternBank::from_endpoint(broken,guard);});
    check(evidence.column_observation_count()==3 && evidence.materialized_field_entries()==0,"native columns materialized");
}
void sparse_basis_rank(){
    for(std::size_t n=0;n<16;++n){
        G guard;M basis(2*n+3,n+2);
        for(std::size_t i=0;i<2*n;++i)if(i%3)basis.set(i,i%n,Q{numeric::Integer(i+1),numeric::Integer(3)});
        const auto rank=learning::transport_parameter_basis_rank(basis,guard);
        check(rank.rank==numeric::PreparedExactFrames(basis,guard).rank(),"disjoint rank differs from complete frames");
    }
    G guard;M dependent(3,3);dependent.set(0,0,1);dependent.set(0,1,2);dependent.set(1,0,2);dependent.set(1,1,4);
    check(learning::transport_parameter_basis_rank(dependent,guard).rank==1,"dependent template fallback differs");
    numeric::ExactActionLimits limits;limits.dimension=131072;limits.steps=500000;
    G bounded(limits);M large(30000,10000);
    for(std::size_t i=0;i<30000;++i)large.set(i,i%10000,Q{1,3});
    check(learning::transport_parameter_basis_rank(large,bounded).rank==10000,"bounded sparse rank failed");
    check(bounded.steps()<500000,"sparse rank exceeded its admitted work");
    G full_frame(limits);const numeric::PreparedExactFrames complete(large,full_frame);
    check(complete.original()==large && complete.rank()==10000 && complete.kernel().cols()==0,
        "complete sparse basis frame differs");
    std::cout<<"sparse_basis_rank_steps="<<bounded.steps()<<'\n';
}
void sparse_observable_image(){
    for(std::size_t n=0;n<20;++n){
        G guard;M map(n%7,37);
        for(std::size_t i=0;i<map.rows();++i){
            map.set(i,3,Q{numeric::Integer(i+1),numeric::Integer(3)});
            map.set(i,18,Q{numeric::Integer(i%2),numeric::Integer(5)});
            map.set(i,29,2*map.get(i,3)-map.get(i,18));
        }
        const numeric::PreparedExactFrames full(map,guard);
        check(learning::transport_observable_image(map,guard)==full.image(),
            "compacted observable image differs from complete frames");
    }
    numeric::ExactActionLimits limits;limits.dimension=131072;limits.steps=500000;
    G bounded(limits);M large(60,30000);
    for(std::size_t i=0;i<60;++i){
        large.set(i,2,Q{numeric::Integer(i+1),numeric::Integer(3)});
        large.set(i,17000,Q{numeric::Integer(i%2),numeric::Integer(5)});
        large.set(i,29000,2*large.get(i,2)-large.get(i,17000));
    }
    const auto image=learning::transport_observable_image(large,bounded);
    check(image.rows()==60 && image.cols()==2,"bounded sparse observable image failed");
    check(bounded.steps()<500000,"observable image exceeded its admitted work");
    G full_frame(limits);const numeric::PreparedExactFrames complete(large,full_frame);
    check(complete.original()==large && complete.image()==image && complete.kernel().cols()==29998,
        "complete sparse observable frame differs");
    limits.dimension=1024;G narrow(limits);
    rejects([&]{(void)learning::transport_observable_image(M(1,1025),narrow);});
    limits.dimension=131072;limits.steps=2;G stopped(limits);
    rejects([&]{(void)learning::transport_observable_image(large,stopped);});
    std::cout<<"sparse_observable_image_steps="<<bounded.steps()<<'\n';
}
void exclusive_row_certificates(){
    for(std::size_t n=0;n<18;++n){
        G guard;M basis(n+3,3*n+2),expected(n+3,n);
        for(std::size_t j=0;j<n;++j){
            const Q pivot{numeric::Integer(j+1),numeric::Integer(7)};
            const Q shared{numeric::Integer(j%2?3:-2),numeric::Integer(5)};
            basis.set(j,3*j+1,pivot);basis.set(n,3*j+1,shared);
            expected.set(j,j,pivot);expected.set(n,j,shared);
        }
        const numeric::PreparedExactFrames oracle(basis,guard);
        check(learning::transport_parameter_basis_rank(basis,guard).rank==oracle.rank(),"exclusive row rank differs");
        check(learning::transport_observable_image(basis,guard)==oracle.image(),"exclusive row image differs");
        check(oracle.image()==expected,"exclusive row column order differs");
    }
    G guard;M no_certificate(2,3);
    no_certificate.set(0,0,1);no_certificate.set(0,1,1);no_certificate.set(1,0,1);no_certificate.set(1,1,-1);
    check(learning::transport_parameter_basis_rank(no_certificate,guard).rank==2,"uncertified independent fallback differs");
    no_certificate.set(0,2,2);
    check(learning::transport_parameter_basis_rank(no_certificate,guard).rank==2,"partial exclusive witnesses imply full rank");
    const numeric::PreparedExactFrames fallback(no_certificate,guard);
    check(learning::transport_observable_image(no_certificate,guard)==fallback.image(),
        "uncertified image fallback differs");
    numeric::ExactActionLimits limits;limits.dimension=40000;limits.steps=500000;
    M large(10001,30000),expected(10001,10000);
    for(std::size_t j=0;j<10000;++j){
        large.set(j,3*j+1,Q{2,7});large.set(10000,3*j+1,Q{-3,5});
        expected.set(j,j,Q{2,7});expected.set(10000,j,Q{-3,5});
    }
    G rank(limits),image(limits),full(limits);
    check(learning::transport_parameter_basis_rank(large,rank).rank==10000,"bounded exclusive rank failed");
    check(learning::transport_observable_image(large,image)==expected,"bounded exclusive image failed");
    const numeric::PreparedExactFrames complete(large,full);
    check(complete.original()==large && complete.image()==expected && complete.rank()==10000 &&
        complete.kernel().rows()==30000 && complete.kernel().cols()==20000,"complete exclusive frame differs");
    limits.entries=large.nonzeros();G small(limits);
    rejects([&]{(void)learning::transport_parameter_basis_rank(large,small);});
    limits.entries=16000000;limits.integer_bits=1;G bits(limits);
    rejects([&]{(void)learning::transport_parameter_basis_rank(large,bits);});
    G stopped({},[]{throw std::runtime_error("stopped");});
    rejects([&]{(void)learning::transport_observable_image(large,stopped);});
    std::cout<<"exclusive_rank_steps="<<rank.steps()<<" exclusive_image_steps="<<image.steps()<<'\n';
}
}
int main(){try{declared_family();declared_family(true);sparse_basis_rank();sparse_observable_image();exclusive_row_certificates();std::cout<<checks<<" declared template bank checks passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}return 0;}
