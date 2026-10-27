#include "rfc/storage/declaration_codec.hpp"
#include <iostream>
int main(){
    using namespace rfc;using namespace composite;
    const DeclarationId type{901,2,1};const std::size_t n=1000003;
    const DeclarationRegistry registry({Declaration::rational_tensor(type,{{42,n,{}}})});
    const CompiledLayout layout(registry,type,BooleanLayoutProfile::Lsb0OctetsV2);
    const auto value=DeclaredValue::tensor(numeric::SparseRationalTensor(
        numeric::IndexDomain::linear(n),numeric::StructuralSupport::full(n),
        numeric::SparseRationalVector(n,{{0,numeric::Rational(-2,3)},{n-1,numeric::Rational(4,7)}})));
    const auto payload=storage::encode_declared_bits(layout,value);
    const auto archive=storage::encode_declared_archive(layout,std::span(&value,1));
    const auto reopened=storage::decode_declared_archive(archive);
    const auto& tensor=std::get<numeric::SparseRationalTensor>(reopened.values.at(0).data);
    if(reopened.layout.profile()!=BooleanLayoutProfile::Lsb0OctetsV2 || tensor.support().count()!=n ||
       tensor.support().storage()!=numeric::StructuralSupport::Storage::Full || tensor.stored_nonzeros()!=2 ||
       tensor.values().entries()!=std::get<numeric::SparseRationalTensor>(value.data).values().entries() ||
       storage::encode_declared_archive(reopened.layout,reopened.values)!=archive)return 1;
    std::cout<<"coordinates="<<n<<" nonzeros="<<tensor.stored_nonzeros()
             <<" payload_bytes="<<payload.bytes.size()<<" archive_bytes="<<archive.size()<<'\n';
}
