#pragma once
#include "rfc/storage/declaration_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
namespace compact_fixture {
using namespace rfc;
using namespace composite;
using V=DeclaredValue;
using Bytes=std::vector<std::byte>;
inline DeclarationId id(std::uint64_t key){return {901,key,1};}
inline DeclarationRegistry registry(std::size_t size=3){
    std::vector<std::uint64_t> names=size==3?std::vector<std::uint64_t>{30,10,20}:std::vector<std::uint64_t>{};
    return DeclarationRegistry({Declaration::boolean(id(1)),Declaration::rational_tensor(id(2),{{42,size,names}}),
        Declaration::optional(id(3),id(2)),Declaration::repeated(id(4),id(3),0,4),
        Declaration::alternative(id(5),{{11,id(2),0},{12,id(4),0}}),
        Declaration::product(id(6),{{91,id(1),0},{92,id(5),2},{93,id(1),0}})});
}
inline V tensor(std::size_t size=3,bool full=true,bool zeros=false){
    const numeric::IndexDomain d({size});
    std::vector<std::size_t> ix;
    std::vector<numeric::SparseRationalEntry> vs;
    if(size){ix.push_back(0);if(size>1)ix.push_back(size-1);}
    if(size && !zeros){vs.push_back({0,numeric::Rational(-2,3)});if(size>1)vs.push_back({size-1,numeric::Rational(4,7)});}
    return V::tensor(numeric::SparseRationalTensor(d,full?numeric::StructuralSupport::full(size):numeric::StructuralSupport::sparse(size,ix),numeric::SparseRationalVector(size,vs)));
}
inline V nested(){return V::product({V::boolean(true),V::alternative(12,V::repeated({V::absent(),V::present(tensor()),V::present(tensor(3,false))})),V::boolean(false)});}
inline void save(const std::filesystem::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!f)throw std::runtime_error("write fixture");}
inline Bytes load(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("read fixture: "+p.string());const std::vector<char> b{std::istreambuf_iterator<char>(f),{}};Bytes out;for(unsigned char x:b)out.push_back(std::byte{x});return out;}
}
