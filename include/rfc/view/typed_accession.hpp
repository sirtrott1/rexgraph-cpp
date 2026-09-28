#pragma once
#include "rfc/field/exact_metric.hpp"
#include "rfc/geometry/typed_realization.hpp"
#include <stdexcept>

namespace rfc::view {

using geometry::TypedAccession;
using geometry::ReconstructingTypedView;
using geometry::ReconstructingTypedFamily;
using geometry::DescentCertificate;
using geometry::EffectiveImageCoordinates;
using geometry::EffectiveInducedMap;
using geometry::vertical_stack;
using geometry::horizontal_stack;
using geometry::represented_map;
using geometry::descent_certificate;
using geometry::represented_block;
using geometry::effective_image_coordinates;
using geometry::induced_effective_map;

inline numeric::ExactMatrix physical_cross_metric(const TypedAccession& lhs,
                                                   const TypedAccession& rhs,
                                                   const field::ExactMetric& ambient_metric) {
    if (lhs.ambient_dim() != ambient_metric.size() || rhs.ambient_dim() != ambient_metric.size()) {
        throw std::invalid_argument("typed view ambient metric mismatch");
    }
    return lhs.realization.transpose().multiply(ambient_metric.materialize()).multiply(rhs.realization);
}

inline numeric::ExactMatrix represented_metric(const ReconstructingTypedFamily& family,
                                                const field::ExactMetric& ambient_metric) {
    if (ambient_metric.size() != family.ambient_dim()) throw std::invalid_argument("typed represented metric ambient mismatch");
    const auto& R = family.stacked_reconstruction();
    return R.transpose().multiply(ambient_metric.materialize()).multiply(R);
}

inline bool metric_pullback_certificate(const ReconstructingTypedFamily& family,
                                        const field::ExactMetric& ambient_metric) {
    const auto Mrep = represented_metric(family, ambient_metric);
    const auto& A = family.stacked_accession();
    return A.transpose().multiply(Mrep).multiply(A) == ambient_metric.materialize();
}

} // namespace rfc::view
