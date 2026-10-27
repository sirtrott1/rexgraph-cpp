// Compiled once against the unchanged model-envelope headers to emit predecessor bytes,
// then compiled as a profile-aware regression against the current headers.
#include "rfc/composite/declaration_evolution.hpp"
#include "rfc/storage/declaration_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace rfc;using namespace composite;using namespace storage;
namespace {
DeclarationId id(unsigned v){return {4203,1,v};}
std::vector<std::byte> bytes(unsigned which,BooleanLayoutProfile profile){
    auto axes=std::vector<DeclarationAxis>{{11,2,{10,20}},{22,3,{30,40,50}}};
    auto target=std::vector<DeclarationAxis>{{22,3,{50,30,40}},{11,2,{20,10}}};
    if(which==4)target={{22,4,{50,30,40,99}},{11,3,{20,10,88}}};
    if(which==5){axes.clear();target.clear();}
    DeclarationRegistry a({Declaration::rational_tensor(id(1),axes)}),b({Declaration::rational_tensor(id(2),target)});
    const auto d=DeclarationRegistry::tensor_domain(a.at(id(1)));const auto n=d.logical_size();
    auto support=numeric::StructuralSupport::full(n);
    if(which==1)support=numeric::StructuralSupport::sparse(n,{0,2,5});
    if(which==3)support=numeric::StructuralSupport::sparse(n,{});
    std::vector<numeric::SparseRationalEntry> es;
    if(which!=2 && which!=3)es=which==5?std::vector<numeric::SparseRationalEntry>{{0,numeric::Rational{3,11}}}:
        std::vector<numeric::SparseRationalEntry>{{0,numeric::Rational{-2,3}},{5,numeric::Rational{4,7}}};
    const auto x=DeclaredValue::tensor(numeric::SparseRationalTensor(d,support,numeric::SparseRationalVector(n,es)));
    const std::vector<DeclarationMap> maps{{id(1),id(2),which==4?DeclarationMapKind::Embedding:DeclarationMapKind::Recode,{}}};
#ifdef RFC_RECODE1_PROFILE_AWARE
    const DeclarationRecoding f(a,b,maps,{}, {profile,profile});
#else
    const DeclarationRecoding f(a,b,maps);
#endif
    const auto y=f.apply(id(1),x);
    return encode_declared_archive(CompiledLayout(b,id(2),profile),std::span(&y,1));
}
std::vector<std::byte> load(const std::filesystem::path& p){
    std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("missing golden file");
    std::vector<char> c{std::istreambuf_iterator<char>(in),{}};std::vector<std::byte>b;
    for(unsigned char x:c)b.push_back(std::byte{x});
    return b;
}
}
int main(int argc,char** argv){try{
    const bool emit=argc==3 && std::string_view(argv[1])=="--emit";
#ifdef RFC_RECODE1_GOLDENS
    const std::filesystem::path dir=emit?argv[2]:RFC_RECODE1_GOLDENS;
#else
    if(!emit)return 2;
    const std::filesystem::path dir=argv[2];
#endif
    if(emit)std::filesystem::create_directories(dir);
    std::size_t checks=0;
    for(unsigned p=1;p<=2;++p)for(unsigned k=0;k<6;++k){
        const auto out=bytes(k,static_cast<BooleanLayoutProfile>(p));
        const auto path=dir/("case-"+std::to_string(k)+"-v"+std::to_string(p)+".rfcxv");
        if(emit){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(out.data()),out.size());if(!f)return 3;}
        else if(out!=load(path))throw std::runtime_error("predecessor recoding bytes changed");
        const auto decoded=decode_declared_archive(out);
        if(encode_declared_archive(decoded.layout,decoded.values)!=out)throw std::runtime_error("noncanonical recoding bytes");
        ++checks;
    }
    std::cout<<"recoding golden archives="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
