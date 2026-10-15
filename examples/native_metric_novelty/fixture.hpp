#pragma once
#include "rfc/inference/native_metric_novelty.hpp"
#include "../native_updates/fixture.hpp"
#include "../native_green/fixture.hpp"

namespace native_metric_example {
using Matrix=rfc::numeric::ExactMatrix;
using Vector=rfc::numeric::RationalVector;
using Q=rfc::numeric::Rational;
using Pin=rfc::storage::PinnedNativeAccession;
using native_update_example::access;
inline rfc::storage::TemporalEndpoint endpoint(const Matrix& metric,const Vector& x,std::uint64_t sequence=7,
                                              const std::vector<Vector>& causes={}) {
    using namespace rfc;
    storage::TemporalEndpoint e(native_update_example::source(x.size()),sequence);
    e.carriers={{10,storage::TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(x.size())},
        {30,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},300,numeric::IndexDomain::linear(x.size())}};
    e.tensors={native_update_example::map(1,10,10,metric,storage::TemporalTensorRole::Metric),
        native_update_example::field(100,10,x),native_update_example::field(101,10,Vector(x.size())),
        native_update_example::field(102,30,x)};
    for(std::size_t i=0;i<causes.size();++i)e.tensors.push_back(native_update_example::field(200+i,10,causes[i]));
    return e;
}
inline Pin input(const rfc::storage::NativeAccessionDirectory& directory,std::uint64_t key=100) {
    return directory.pin(access(rfc::storage::NativeAccessionKind::Field,key));
}
inline Pin metric(const rfc::storage::NativeAccessionDirectory& directory) {
    return directory.pin(access(rfc::storage::NativeAccessionKind::Tensor,1));
}
inline rfc::inference::NativeMetricNoveltyRequest request(const rfc::storage::NativeAccessionDirectory& old_directory,
        const rfc::storage::NativeAccessionDirectory& new_directory,const Matrix& comparison) {
    return {input(old_directory),input(new_directory),10,10,rfc::storage::temporal_matrix_tensor(comparison),metric(new_directory)};
}
inline std::pair<rfc::storage::TemporalEndpoint,rfc::storage::TemporalEndpoint> square() {
    auto sources=native_green_example::square();
    auto old=endpoint(Matrix::identity(4),{2,2,0,0});
    auto next=endpoint(Matrix::identity(4),{0,0,-3,-3},8,{{-2,-2,-2,-2},{0,0,-1,-1}});
    old.source=std::move(sources.first.source);next.source=std::move(sources.second.source);
    old.carriers[0].grade=rfc::core::Grade{1};next.carriers[0].grade=rfc::core::Grade{1};
    return {std::move(old),std::move(next)};
}
inline constexpr rfc::inference::NativeMetricAvailability available{{900,7},{901,8}};
} // namespace native_metric_example
