#include "rfc/composite/declaration_evolution.hpp"
#include "rfc/storage/declaration_codec.hpp"
#include <iostream>
int main(){try{
    using namespace rfc;using namespace composite;
    const DeclarationId old_type{4204,1,1},new_type{4204,1,2};const std::size_t n=1000003;
    const DeclarationRegistry a({Declaration::rational_tensor(old_type,{{11,n,{}},{22,2,{101,102}}})});
    const DeclarationRegistry b({Declaration::rational_tensor(new_type,{{22,2,{102,101}},{11,n,{}}})});
    const auto value=DeclaredValue::tensor(numeric::SparseRationalTensor(numeric::IndexDomain({n,2}),
        numeric::StructuralSupport::full(n*2),numeric::SparseRationalVector(n*2,{{0,numeric::Rational{-2,3}},{n*2-1,numeric::Rational{4,7}}})));
    const auto v2=BooleanLayoutProfile::Lsb0OctetsV2;
    const DeclarationRecoding map(a,b,{{old_type,new_type,DeclarationMapKind::Recode,{}}},{},{v2,v2});
    const auto recoded=map.apply(old_type,value);
    const auto wire=storage::encode_declared_archive(CompiledLayout(b,new_type,v2),std::span(&recoded,1));
    const auto reopened=storage::decode_declared_archive(wire);
    const DeclarationRecoding inverse(b,a,{{new_type,old_type,DeclarationMapKind::Recode,{}}},{},{v2,v2});
    if(!declared_value_equal(inverse.apply(new_type,reopened.values.front()),value))return 1;
    const auto& t=std::get<numeric::SparseRationalTensor>(reopened.values.front().data);
    if(t.support().storage()!=numeric::StructuralSupport::Storage::Full)return 2;
    std::cout<<"coordinates="<<t.domain().logical_size()<<" full_support=symbolic nonzeros="<<t.stored_nonzeros()<<" archive_bytes="<<wire.size()<<'\n';
    for(const auto& e:t.values().entries())std::cout<<"index="<<e.index<<" value="<<e.value<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
