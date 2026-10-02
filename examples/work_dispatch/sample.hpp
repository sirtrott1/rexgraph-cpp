#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/database/field_work.hpp"
namespace sample {
using namespace rfc;
inline database::NativeFieldAnalysis field_analysis(std::size_t count) {
    core::RelationSignature signature;
    const auto type=signature.add_type("load",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{1});
    for(std::uint64_t i=0;i<256;++i)builder.declare_witness_identity(10+i);
    for(std::size_t i=0;i<count;++i) {
        std::vector<core::BoundaryOccurrenceInput> occurrences;
        for(std::size_t j=0;j<8;++j)occurrences.push_back({10+(i*7+j)%256,true,j%2!=0,numeric::Rational{1},{},core::OccurrenceKey{50000+8*i+j}});
        builder.add_grade_one(core::RelationKey{1000+i},type,occurrences);
    }
    storage::TemporalEndpoint endpoint(builder.freeze());
    for(std::size_t k=0;k<endpoint.source.grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        for(std::size_t i=0;i<endpoint.source.grade(grade).cell_count();++i) {
            const auto key=storage::stable_cell_key(endpoint.source,{grade,static_cast<std::uint32_t>(i)}).key;
            endpoint.carriers.push_back({key,storage::TemporalCarrierKind::CellStalk,grade,storage::StableCellKey{grade,key},900,numeric::IndexDomain::linear(8)});
            if(k) {
                storage::TemporalTensor tensor;tensor.key=1000000+i;tensor.role=storage::TemporalTensorRole::Field;
                tensor.owner.kind=storage::TemporalOwnerKind::Cell;tensor.owner.cell={grade,key};tensor.output=key;
                std::vector<numeric::SparseRationalEntry> values;
                for(std::size_t j=0;j<8;++j)values.push_back({j,numeric::Rational{static_cast<long>((i+j)%7+1),11}});
                tensor.tensor={numeric::IndexDomain::linear(8),numeric::StructuralSupport::full(8),numeric::SparseRationalVector(8,values)};
                endpoint.tensors.push_back(tensor);
            }
        }
    }
    storage::NativeAccessionLimits limits;limits.max_action_work=64000000;
    storage::NativeAccessionDirectory directory(std::move(endpoint),888,{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},limits);
    storage::NativeFieldRequest request;
    for(const auto& carrier:directory.pin_snapshot()->carriers)request.stalk_carriers.push_back(carrier.key);
    for(const auto& tensor:directory.pin_snapshot()->tensors) {
        storage::NativeAccessionRequest q;q.kind=storage::NativeAccessionKind::Field;q.subject=tensor.key;request.fields.push_back(directory.pin(q));
    }
    const auto anchor=directory.pin(storage::NativeAccessionRequest::sparse_chain(core::Grade{1},{},{}));
    return database::NativeFieldAnalysis::inspect(anchor,request,storage::NativeFieldRealization::constant_fiber(900));
}
inline database::PreparedParallelNativeField field(std::size_t count) {
    return database::prepare_parallel_field(field_analysis(count),128);
}
}
