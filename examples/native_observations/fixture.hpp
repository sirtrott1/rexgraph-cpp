#pragma once
#include "rfc/inference/native_selected_observation.hpp"
#include "../native_green/fixture.hpp"

namespace native_observation_example {
using namespace rfc;
using Matrix=numeric::ExactMatrix;
using Vector=numeric::RationalVector;
using Q=numeric::Rational;
using Pin=storage::PinnedNativeAccession;
using native_update_example::access;
inline Matrix selector(std::size_t n,const std::vector<std::size_t>& coordinates){
    Matrix result(coordinates.size(),n);for(std::size_t i=0;i<coordinates.size();++i)result.set(i,coordinates[i],1);return result;
}
inline storage::TemporalEndpoint endpoint(core::RelationalComplex source,core::Grade grade,const Matrix& operation,
        const std::vector<std::size_t>& coordinates,std::uint64_t sequence=7){
    const auto n=operation.rows();storage::TemporalEndpoint result(std::move(source),sequence);
    result.carriers={{10,storage::TemporalCarrierKind::SourceGrade,grade,{},0,numeric::IndexDomain::linear(n)},
        {20,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},200,numeric::IndexDomain::linear(coordinates.size())},
        {30,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},300,numeric::IndexDomain::linear(n)}};
    result.tensors={native_update_example::map(1,10,10,operation),
        native_update_example::map(2,10,20,selector(n,coordinates),storage::TemporalTensorRole::Accession)};
    Vector rhs(n);if(n)rhs[0]=1;result.tensors.push_back(native_update_example::field(100,10,rhs));
    for(std::size_t i=0;i<n;++i)rhs[i]=Q{static_cast<long>(i)-2,static_cast<unsigned long>(i+1)};
    result.tensors.push_back(native_update_example::field(101,10,rhs));
    result.tensors.push_back(native_update_example::field(102,30,Vector(n,Q{1})));return result;
}
inline storage::TemporalEndpoint endpoint(const Matrix& operation,const std::vector<std::size_t>& coordinates,std::uint64_t sequence=7){
    return endpoint(native_update_example::source(operation.rows()),core::Grade{0},operation,coordinates,sequence);
}
inline Pin op(const storage::NativeAccessionDirectory& directory,std::uint64_t key=1){return directory.pin(access(storage::NativeAccessionKind::Operator,key));}
inline Pin input(const storage::NativeAccessionDirectory& directory,std::uint64_t key=100){return directory.pin(access(storage::NativeAccessionKind::Field,key));}
inline inference::NativeSelectedObservationRequest request(const storage::NativeAccessionDirectory& directory,std::vector<std::size_t> coordinates){
    return {op(directory,2),std::move(coordinates)};
}
inline std::pair<storage::TemporalEndpoint,storage::TemporalEndpoint> square(){
    const auto sources=native_green_example::square();const std::vector<std::size_t> coordinates{2,0,2};
    const auto B=field::materialize_boundary_matrix(sources.first.source,core::Grade{1});
    const auto C=field::materialize_boundary_matrix(sources.second.source,core::Grade{2});
    const auto old_operation=Matrix::identity(4).add(B.transpose().multiply(B));
    return {endpoint(sources.first.source,core::Grade{1},old_operation,coordinates,7),
        endpoint(sources.second.source,core::Grade{1},old_operation.add(C.multiply(C.transpose())),coordinates,8)};
}
} // namespace native_observation_example
