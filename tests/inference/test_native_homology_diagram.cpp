#include "rfc/inference/native_homology_diagram_codec.hpp"
#include <iostream>

using namespace rfc;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static storage::PinnedNativeAccession endpoint(std::uint64_t space,long order){
    core::RelationSignature signature;
    const auto loop=signature.add_type("loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_type("face",core::BoundaryPolicy::DeclaredExact,1,1);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(loop,{{42,true,true,false,1},{42,true,false,false,1}});
    b.add_higher(core::Grade{2},face,{{0,true,false,false,numeric::Rational{order}}});
    storage::TemporalEndpoint e(b.freeze(),7);
    for(std::size_t k=0;k<e.source.grade_count();++k){storage::TemporalCarrier c;c.key=10+k;c.kind=storage::TemporalCarrierKind::SourceGrade;
        c.grade=core::Grade{static_cast<std::uint16_t>(k)};c.domain=numeric::IndexDomain::linear(e.source.grade(c.grade).cell_count());e.carriers.push_back(c);}
    storage::NativeAccessionDirectory directory(e,space);
    return directory.pin(storage::NativeAccessionRequest::sparse_chain(core::Grade{0},{},{}));
}
static inference::NativeHomologyDiagramRequest request(inference::HomologyDiagramProduct product){
    inference::NativeHomologyDiagramRequest r;r.product=product;r.grade=core::Grade{1};
    r.vertices={{endpoint(900,4),{10,11,12}},{endpoint(901,2),{10,11,12}}};
    auto one=numeric::ExactMatrix::identity(1);auto two=one;two.set(0,0,2);r.arrows={{true,{one,one,two}}};return r;
}
int main(int argc,char** argv){try{
    using Product=inference::HomologyDiagramProduct;
    if(argc==3 && std::string_view(argv[1])=="read"){
        for(const auto* suffix:{".q",".z"}){const auto bytes=storage::read_file_exact(std::string(argv[2])+suffix,10000000);
            auto cold=inference::decode_native_homology_diagram(bytes);
            require(inference::encode_native_homology_diagram(cold,10000000)==bytes,"fresh process full replay differs");}
        std::cout<<"fresh process complete native replay passed\n";return 0;
    }
    for(auto product:{Product::RationalZigzag,Product::IntegralModules}){
        auto original=request(product);const auto first=original.vertices[0].publication.endpoint();
        auto d=inference::PreparedNativeHomologyDiagram::prepare(original);
        require(d.declaration().vertices[0].publication.endpoint()==first,"source owner lost");
        require(d.declaration().vertices[0].publication.source_namespace()!=d.declaration().vertices[1].publication.source_namespace(),"independent equal carriers identified");
        if(product==Product::IntegralModules){require(d.integral().modules()[0].torsion==std::vector<numeric::Integer>{4},"Z4 presentation");
            require(d.integral().modules()[1].torsion==std::vector<numeric::Integer>{2} && d.integral().induced()[0].smith_map.get(0,0)==1,"Z4 to Z2 induced map");}
        else require(d.rational().zigzag().intervals().empty(),"torsion retained as rational homology");
        const auto bytes=inference::encode_native_homology_diagram(d,10000000);
        if(argc==3 && std::string_view(argv[1])=="write")storage::write_file_exact(std::string(argv[2])+(product==Product::RationalZigzag?".q":".z"),bytes);
        auto cold=inference::decode_native_homology_diagram(bytes);
        require(inference::encode_native_homology_diagram(cold,10000000)==bytes,"full native cold replay differs");
        for(std::size_t i=0;i<original.vertices.size();++i){const auto old=original.vertices[i].publication.endpoint(),now=cold.declaration().vertices[i].publication.endpoint();
            require(storage::encode_rfcx(old->source).bytes==storage::encode_rfcx(now->source).bytes && storage::encode_temporal_endpoint(*old)==storage::encode_temporal_endpoint(*now),"complete source or channels changed");
            require(cold.declaration().vertices[i].publication.sequence()==7,"cutoff changed");}
        bool refused=false;auto damaged=bytes;damaged.back()^=std::byte{1};
        try{auto bad=inference::decode_native_homology_diagram(damaged);(void)bad;}catch(const std::invalid_argument&){refused=true;}require(refused,"corrupt native image accepted");
        const auto hash=storage::rfcx_detail::fnv1a64(std::span<const std::byte>(damaged).subspan(32));
        for(std::size_t i=0;i<8;++i)damaged[24+i]=static_cast<std::byte>((hash>>(8*i))&255);
        refused=false;try{auto bad=inference::decode_native_homology_diagram(damaged);(void)bad;}catch(const std::invalid_argument&){refused=true;}require(refused,"changed certificate accepted with corrected envelope hash");
        refused=false;try{auto bad=inference::encode_native_homology_diagram(d,1);(void)bad;}catch(const std::length_error&){refused=true;}require(refused,"one byte output policy ignored");
        auto omitted=original;omitted.vertices[0].grade_carriers.clear();refused=false;
        try{auto bad=inference::PreparedNativeHomologyDiagram::prepare(omitted);(void)bad;}catch(const std::invalid_argument&){refused=true;}require(refused,"missing carrier owner accepted");
        auto malformed=original;malformed.arrows[0].maps[0]=numeric::ExactMatrix(2,1);refused=false;
        try{auto bad=inference::PreparedNativeHomologyDiagram::prepare(malformed);(void)bad;}catch(const std::invalid_argument&){refused=true;}require(refused,"malformed chain map accepted");
        runtime::ExecutionHostPolicy tiny;tiny.limits.rational_slots=1;refused=false;
        try{auto bad=inference::PreparedNativeHomologyDiagram::prepare(original,runtime::ExecutionLease(tiny));(void)bad;}catch(const std::length_error&){refused=true;}require(refused,"native resource denial ignored");
        for(bool bits:{false,true}){runtime::ExecutionHostPolicy small;if(bits)small.limits.integer_bits=1;else small.limits.scalar_work=1;refused=false;
            try{auto bad=inference::PreparedNativeHomologyDiagram::prepare(original,runtime::ExecutionLease(small));(void)bad;}catch(const std::length_error&){refused=true;}require(refused,"native scalar or integer policy ignored");}
        storage::TemporalReadLimits read;read.max_dimension=0;refused=false;
        try{auto bad=inference::decode_native_homology_diagram(bytes,read);(void)bad;}catch(const std::invalid_argument&){refused=true;}require(refused,"source dimension read policy ignored");
        auto stop=std::make_shared<std::atomic_bool>(true);refused=false;
        try{auto bad=inference::PreparedNativeHomologyDiagram::prepare(original,runtime::ExecutionLease({},stop));(void)bad;}catch(const runtime::ExecutionCancelled&){refused=true;}require(refused,"native cancellation ignored");
    }
    auto fractional=request(Product::IntegralModules);fractional.arrows[0].maps[0].set(0,0,numeric::Rational{1,2});bool refused=false;
    try{auto bad=inference::PreparedNativeHomologyDiagram::prepare(fractional);(void)bad;}catch(const std::invalid_argument&){refused=true;}require(refused,"nonintegral map accepted");
    std::cout<<"native source ownership, torsion, complete codec replay and refusals passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
