#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/storage/native_accession.hpp"

namespace native_update_example {
using namespace rfc;
using Matrix=numeric::ExactMatrix;
using Vector=numeric::RationalVector;
using Q=numeric::Rational;
inline storage::NativeAccessionRequest access(storage::NativeAccessionKind kind,std::uint64_t key) {
    storage::NativeAccessionRequest request;request.kind=kind;request.subject=key;return request;
}
inline core::RelationalComplex source(std::size_t n) {
    core::RelationSignature signature;
    const auto type=signature.add_type("native-update",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{1});
    for(std::size_t i=0;i<n;++i)builder.declare_witness_identity(10+i);
    for(std::size_t i=1;i<n;++i)builder.add_grade_one(core::RelationKey{100+i},type,
        {{10,true,true,1},{10+i,true,false,1},{10+i,true,true,2},
         {10+i,true,false,2},{10,false,false,7}});
    return builder.freeze();
}
inline storage::TemporalTensor map(std::uint64_t key,std::uint64_t in,std::uint64_t out,const Matrix& matrix,
        storage::TemporalTensorRole role=storage::TemporalTensorRole::SourceRealization) {
    storage::TemporalTensor tensor;tensor.key=key;tensor.role=role;tensor.input=in;tensor.output=out;
    tensor.tensor=storage::temporal_matrix_tensor(matrix);return tensor;
}
inline storage::TemporalTensor field(std::uint64_t key,std::uint64_t carrier,const Vector& values) {
    storage::TemporalTensor tensor;tensor.key=key;tensor.role=storage::TemporalTensorRole::Field;tensor.output=carrier;
    std::vector<numeric::SparseRationalEntry> entries;
    for(std::size_t i=0;i<values.size();++i)if(values[i]!=0)entries.push_back({i,values[i]});
    tensor.tensor={numeric::IndexDomain::linear(values.size()),numeric::StructuralSupport::full(values.size()),
                   numeric::SparseRationalVector(values.size(),std::move(entries))};return tensor;
}
inline storage::TemporalEndpoint endpoint(const Matrix& a,const Matrix& u,const Matrix& c,const Matrix& row,
                                         std::uint64_t sequence=7) {
    const auto n=a.rows(),rank=c.rows();storage::TemporalEndpoint result(source(n),sequence);
    result.carriers={{10,storage::TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(n)},
        {20,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},200,numeric::IndexDomain::linear(rank)},
        {30,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},300,numeric::IndexDomain::linear(n)}};
    result.tensors={map(1,10,10,a),map(2,20,10,u),map(3,20,20,c),
        map(4,10,20,row,storage::TemporalTensorRole::Accession),map(5,10,10,a.add(u.multiply(c).multiply(row)))};
    for(unsigned sample=0;sample<2;++sample){
        Vector rhs(n);for(std::size_t i=0;i<n;++i)rhs[i]=Q{static_cast<long>(i+sample)-2,static_cast<unsigned long>(i+1)};
        result.tensors.push_back(field(100+sample,10,rhs));
    }
    result.tensors.push_back(field(102,30,Vector(n,Q{1})));return result;
}
inline storage::TemporalEndpoint endpoint(std::size_t n,std::size_t rank,std::size_t seed=0) {
    Matrix a(n,n),u(n,rank),c=Matrix::identity(rank).scale(2),row(rank,n);
    for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j)
        a.set(i,j,i==j?Q{static_cast<unsigned long>(3*n+i+4)}:Q{static_cast<long>((i+2*j+seed)%5)-2,17});
    for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<rank;++j){
        u.set(i,j,Q{static_cast<long>((3*i+j+seed)%7)-3,100});
        row.set(j,i,Q{static_cast<long>((i+5*j+seed)%7)-3,100});
    }
    for(std::size_t i=1;i<rank;++i)c.set(i-1,i,Q{static_cast<unsigned long>(i+seed+1)});
    return endpoint(a,u,c,row);
}
} // namespace native_update_example
