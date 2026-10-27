#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/inference/native_action_moments.hpp"
#include "rfc/storage/native_accession.hpp"
#include <array>

namespace action_moment_example {
using namespace rfc;
using namespace rfc::inference;
using Q = numeric::Rational;
using Matrix = numeric::ExactMatrix;
inline storage::NativeAccessionRequest access(storage::NativeAccessionKind kind, std::uint64_t key) {
    storage::NativeAccessionRequest r; r.kind=kind; r.subject=key; return r;
}
inline storage::TemporalTensor field(std::uint64_t key, std::uint64_t carrier,
    std::size_t dimension, std::vector<numeric::SparseRationalEntry> values) {
    storage::TemporalTensor t; t.key=key; t.role=storage::TemporalTensorRole::Field; t.output=carrier;
    t.tensor={numeric::IndexDomain::linear(dimension),numeric::StructuralSupport::full(dimension),
              numeric::SparseRationalVector(dimension,std::move(values))}; return t;
}
inline storage::TemporalEndpoint make(std::size_t dimension=4, std::uint64_t sequence=7) {
    if(dimension<4) throw std::invalid_argument("example requires four named positions");
    core::RelationSignature sig;
    const auto type=sig.add_type("pair",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(sig);
    const std::array<std::pair<std::uint64_t,std::uint64_t>,3> ends{{{10,11},{12,13},{11,12}}};
    for(std::size_t i=0;i<3;++i) builder.add_grade_one(core::RelationKey{100+i},type,
        {{ends[i].first,true,true,1},{ends[i].second,true,false,1}});
    storage::TemporalEndpoint e(builder.freeze(),sequence);
    e.carriers={{10,dimension==4?storage::TemporalCarrierKind::SourceGrade:storage::TemporalCarrierKind::Explicit,
        core::Grade{0},{},dimension==4?0u:1000u,numeric::IndexDomain::linear(dimension)},
        {20,storage::TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(3)}};
    Matrix full(3,dimension);
    for(std::size_t i=0;i<3;++i) {
        Matrix local(3,dimension);
        local.set(i,ends[i].first-10,-1); local.set(i,ends[i].second-10,1);
        full.set(i,ends[i].first-10,-1); full.set(i,ends[i].second-10,1);
        storage::TemporalTensor t; t.key=200+i; t.role=storage::TemporalTensorRole::Observation;
        t.input=10; t.output=20; t.tensor=storage::temporal_matrix_tensor(local); e.tensors.push_back(std::move(t));
    }
    storage::TemporalTensor t; t.key=203; t.role=storage::TemporalTensorRole::Observation;
    t.input=10; t.output=20; t.tensor=storage::temporal_matrix_tensor(full); e.tensors.push_back(std::move(t));
    e.tensors.push_back(field(1000,10,dimension,{{0,1}}));
    return e;
}
inline NativeFieldValue input(const storage::NativeAccessionDirectory& directory) {
    return NativeFieldValue::stored(directory.pin(access(storage::NativeAccessionKind::Field,1000)));
}
inline NativeFactoredAction read(const storage::NativeAccessionDirectory& directory, std::uint64_t key,
    std::size_t dimension=4096) {
    return NativeFactoredAction::tensor(directory.pin(access(storage::NativeAccessionKind::Operator,key)),dimension);
}
inline NativeBilinearForm unit(const NativeCarrier& carrier, std::size_t dimension=4096) {
    numeric::ExactActionLimits limits; limits.dimension=dimension;
    return NativeBilinearForm::named_unit(carrier,limits);
}
struct Plans {
    NativeFactoredAction old_laplacian, new_laplacian;
    PreparedNativeActionMoments old_third, new_third;
};
inline Plans plans(const storage::NativeAccessionDirectory& directory, std::size_t dimension=4096) {
    const auto a=read(directory,200,dimension), b=read(directory,201,dimension), c=read(directory,202,dimension);
    const auto all=read(directory,203,dimension);
    const auto old_read=a.plus(b);
    const auto old_l=old_read.then(old_read.algebraic_transpose());
    const auto new_l=all.then(all.algebraic_transpose());
    auto metric=unit(all.output(),dimension);
    // mu_3(x) = || B^T L x ||^2 for the declared unit metrics. Each
    // row-restricted reading keeps its actual relation coordinate in grade one.
    PreparedNativeActionMoments before({{"A",old_l.then(a)},{"B",old_l.then(b)}},metric,
                                        old_l.then(old_read));
    PreparedNativeActionMoments after({{"A",new_l.then(a)},{"B",new_l.then(b)},{"C",new_l.then(c)}},metric,
                                       new_l.then(all));
    return {old_l,new_l,std::move(before),std::move(after)};
}
} // namespace action_moment_example
