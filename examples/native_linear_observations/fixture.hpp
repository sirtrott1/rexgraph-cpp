#pragma once
#include "rfc/inference/native_linear_observation.hpp"
#include "../native_observations/fixture.hpp"

namespace native_linear_observation_example {
using namespace rfc;
using Matrix=numeric::ExactMatrix;
using Vector=numeric::RationalVector;
using Q=numeric::Rational;
using Pin=storage::PinnedNativeAccession;
using native_observation_example::op;
using native_observation_example::input;
inline storage::TemporalEndpoint endpoint(core::RelationalComplex source,core::Grade grade,const Matrix& operation,
                                         const Matrix& accession,std::uint64_t sequence=7){
    const auto n=operation.rows();storage::TemporalEndpoint result(std::move(source),sequence);
    result.carriers={{10,storage::TemporalCarrierKind::SourceGrade,grade,{},0,numeric::IndexDomain::linear(n)},
        {20,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},200,numeric::IndexDomain::linear(accession.rows())},
        {30,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},300,numeric::IndexDomain::linear(n)}};
    result.tensors={native_update_example::map(1,10,10,operation),
        native_update_example::map(2,10,20,accession,storage::TemporalTensorRole::Accession)};
    Vector rhs(n);if(n)rhs[0]=1;result.tensors.push_back(native_update_example::field(100,10,rhs));
    for(std::size_t i=0;i<n;++i)rhs[i]=Q{static_cast<long>(i)-2,static_cast<unsigned long>(i+1)};
    result.tensors.push_back(native_update_example::field(101,10,rhs));
    result.tensors.push_back(native_update_example::field(102,30,Vector(n,Q{1})));return result;
}
inline storage::TemporalEndpoint endpoint(const Matrix& operation,const Matrix& accession,std::uint64_t sequence=7){
    return endpoint(native_update_example::source(operation.rows()),core::Grade{0},operation,accession,sequence);
}
inline inference::NativeLinearObservationRequest request(const storage::NativeAccessionDirectory& directory){return {op(directory,2)};}
inline Matrix square_accession(){return Matrix::from_rows({{1,1,0,0},{0,1,-1,0},{1,1,0,0},{0,0,0,0},{2,2,0,0}});}
inline std::pair<storage::TemporalEndpoint,storage::TemporalEndpoint> square(){
    const auto sources=native_green_example::square();
    const auto B=field::materialize_boundary_matrix(sources.first.source,core::Grade{1});
    const auto C=field::materialize_boundary_matrix(sources.second.source,core::Grade{2});
    const auto old_operation=Matrix::identity(4).add(B.transpose().multiply(B));const auto S=square_accession();
    return {endpoint(sources.first.source,core::Grade{1},old_operation,S,7),
        endpoint(sources.second.source,core::Grade{1},old_operation.add(C.multiply(C.transpose())),S,8)};
}
} // namespace native_linear_observation_example
