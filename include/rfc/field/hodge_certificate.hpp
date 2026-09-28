#pragma once
#include "rfc/field/exact_hodge.hpp"
#include "rfc/field/low_rank_update.hpp"
#include "rfc/geometry/structural_certificate.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

inline numeric::ExactMatrix closed_frame(const geometry::ChainStructureCertificate& structural,
                                         core::Grade grade) {
    if (grade.value == 0) return numeric::ExactMatrix::identity(structural.grade(grade).cell_count);
    return structural.boundary(grade).kernel_frame;
}

inline numeric::ExactMatrix curl_frame(const geometry::ChainStructureCertificate& structural,
                                       core::Grade grade) {
    if (static_cast<std::size_t>(grade.value) + 1u >= structural.grade_count()) {
        return numeric::ExactMatrix(structural.grade(grade).cell_count, 0u);
    }
    return structural.boundary(core::Grade{static_cast<std::uint16_t>(grade.value + 1u)}).image_frame;
}


inline numeric::ExactMatrix metric_gradient_frame_from_structure(
    const geometry::ChainStructureCertificate& structural,
    core::Grade grade,
    const ExactMetric& metric) {
    const auto K = closed_frame(structural, grade);
    if (K.rows() != metric.size()) throw std::invalid_argument("gradient certificate metric size mismatch");
    if (K.cols() == 0u) return numeric::ExactMatrix::identity(metric.size());

    // im(B_k^dagger) = (ker B_k)^{perp_M}.  Build only the exact
    // annihilator constraints K^T M through metric actions; no B_k or metric
    // matrix materialization is required.
    numeric::ExactMatrix constraints(K.cols(), metric.size());
    for (std::size_t j = 0; j < K.cols(); ++j) {
        const auto measured = metric.apply(matrix_column(K, j));
        for (std::size_t i = 0; i < measured.size(); ++i) {
            if (measured[i] != 0) constraints.set(j, i, measured[i]);
        }
    }
    return constraints.kernel_frame();
}

inline numeric::ExactMatrix metric_harmonic_frame_from_structure(
    const geometry::ChainStructureCertificate& structural,
    core::Grade grade,
    const ExactMetric& metric) {
    const auto Z = closed_frame(structural, grade);
    const auto F = curl_frame(structural, grade);
    if (metric.size() != Z.rows()) throw std::invalid_argument("harmonic certificate metric size mismatch");
    if (F.rows() != Z.rows()) throw std::logic_error("curl/closed frame ambient mismatch");

    // Choose an exact complement W so [F W] spans ker(B_k).
    numeric::ExactMatrix basis = F;
    std::vector<numeric::RationalVector> complement_columns;
    for (std::size_t j = 0; j < Z.cols(); ++j) {
        const auto z = matrix_column(Z, j);
        if (geometry::coordinates_in_independent_frame(basis, z)) continue;
        complement_columns.push_back(z);
        basis = numeric::ExactMatrix::horizontal_concat(basis, geometry::column_matrix(z));
    }
    numeric::ExactMatrix W(Z.rows(), complement_columns.size());
    for (std::size_t j = 0; j < complement_columns.size(); ++j) {
        for (std::size_t i = 0; i < Z.rows(); ++i) if (complement_columns[j][i] != 0) W.set(i,j,complement_columns[j][i]);
    }
    if (F.cols() + W.cols() != Z.cols()) throw std::logic_error("failed to construct closed-space complement");
    if (F.cols() == 0u) return W;
    if (W.cols() == 0u) return numeric::ExactMatrix(Z.rows(), 0u);

    const auto gram = metric_gram(F, metric, F);
    const auto cross = metric_gram(F, metric, W);
    const auto coefficients = solve_columns_exact(gram, cross);
    return W.add(F.multiply(coefficients).scale(numeric::Rational{-1}));
}

struct MetricHodgeCertificate {
    core::Grade grade{};
    ExactMetric metric;
    numeric::ExactMatrix gradient_frame;
    numeric::ExactMatrix curl_frame;
    numeric::ExactMatrix harmonic_frame;
    std::size_t zero_multiplicity{};

    [[nodiscard]] numeric::RationalVector project_gradient(const numeric::RationalVector& x) const {
        return projector_apply(gradient_frame, metric, x);
    }
    [[nodiscard]] numeric::RationalVector project_curl(const numeric::RationalVector& x) const {
        return projector_apply(curl_frame, metric, x);
    }
    [[nodiscard]] numeric::RationalVector project_harmonic(const numeric::RationalVector& x) const {
        return projector_apply(harmonic_frame, metric, x);
    }
};

inline MetricHodgeCertificate build_hodge_certificate(
    const core::RelationalComplex& complex,
    const geometry::ChainStructureCertificate& structural,
    core::Grade grade,
    const std::vector<ExactMetric>& metrics) {
    if (!structural.matches(complex)) throw std::invalid_argument("Hodge certificate structural source mismatch");
    if (metrics.size() != complex.grade_count()) throw std::invalid_argument("Hodge certificate metric count mismatch");
    MetricHodgeCertificate out{
        grade,
        metrics.at(grade.value),
        metric_gradient_frame_from_structure(structural, grade, metrics.at(grade.value)),
        curl_frame(structural, grade),
        metric_harmonic_frame_from_structure(structural, grade, metrics.at(grade.value)),
        structural.hodge_zero_multiplicity(grade)};
    if (out.harmonic_frame.cols() != out.zero_multiplicity) {
        throw std::logic_error("harmonic frame dimension disagrees with exact structural zero multiplicity");
    }
    return out;
}

struct MetricAttachmentCertificateUpdate {
    MetricHodgeCertificate certificate;
    numeric::RationalVector harmonic_residual;
    std::size_t residual_rank{};
};

inline numeric::ExactMatrix harmonic_frame_after_single_residual(
    const MetricHodgeCertificate& previous,
    const numeric::RationalVector& residual) {
    if (residual.size() != previous.metric.size()) throw std::invalid_argument("harmonic residual size mismatch");
    bool nonzero = false;
    for (const auto& value : residual) if (value != 0) { nonzero = true; break; }
    if (!nonzero) return previous.harmonic_frame;
    if (previous.harmonic_frame.cols() == 0u) throw std::logic_error("nonzero residual in empty harmonic space");

    numeric::ExactMatrix constraint(1u, previous.harmonic_frame.cols());
    const auto measured_residual = previous.metric.apply(residual);
    for (std::size_t j = 0; j < previous.harmonic_frame.cols(); ++j) {
        const auto h = matrix_column(previous.harmonic_frame, j);
        const auto value = numeric::dot(h, measured_residual);
        if (value != 0) constraint.set(0u, j, value);
    }
    const auto coefficient_kernel = constraint.kernel_frame();
    return previous.harmonic_frame.multiply(coefficient_kernel);
}

inline MetricAttachmentCertificateUpdate update_hodge_certificate_after_attachment(
    const MetricHodgeCertificate& previous,
    const geometry::ChainStructureCertificate& updated_structural,
    const geometry::StructuralAttachmentDelta& delta) {
    if (delta.attached_grade.value == 0) throw std::invalid_argument("attachment grade must be positive");
    const auto affected_grade = core::Grade{static_cast<std::uint16_t>(delta.attached_grade.value - 1u)};
    if (previous.grade != affected_grade) throw std::invalid_argument("Hodge certificate is not for attachment's lower grade");

    const auto residual = previous.project_harmonic(delta.boundary_column);
    bool residual_nonzero = false;
    for (const auto& value : residual) if (value != 0) { residual_nonzero = true; break; }
    const std::size_t residual_rank = residual_nonzero ? 1u : 0u;
    if (residual_rank != delta.boundary_rank_gain) {
        throw std::logic_error("harmonic residual rank disagrees with exact boundary rank gain");
    }

    auto harmonic = harmonic_frame_after_single_residual(previous, residual);
    MetricHodgeCertificate next{
        previous.grade,
        previous.metric,
        previous.gradient_frame,
        curl_frame(updated_structural, previous.grade),
        std::move(harmonic),
        updated_structural.hodge_zero_multiplicity(previous.grade)};
    if (next.harmonic_frame.cols() != next.zero_multiplicity) {
        throw std::logic_error("incremental harmonic frame dimension mismatch");
    }
    return {std::move(next), residual, residual_rank};
}

} // namespace rfc::field
