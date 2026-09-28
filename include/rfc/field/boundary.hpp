#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/field/field.hpp"
#include "rfc/field/metric.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/numeric/exact.hpp"
#include <stdexcept>

namespace rfc::field {

class BoundaryOperator {
public:
    BoundaryOperator(const core::RelationalComplex& complex, core::Grade upper_grade)
        : complex_(&complex), upper_grade_(upper_grade) {
        if (upper_grade.value == 0 || upper_grade.value >= complex.grade_count()) throw std::out_of_range("invalid boundary grade");
    }
    [[nodiscard]] core::Grade upper_grade() const noexcept { return upper_grade_; }
    [[nodiscard]] core::Grade lower_grade() const noexcept { return core::Grade{static_cast<std::uint16_t>(upper_grade_.value - 1u)}; }
    [[nodiscard]] std::size_t domain_size() const { return complex_->grade(upper_grade_).cell_count(); }
    [[nodiscard]] std::size_t codomain_size() const { return complex_->grade(lower_grade()).cell_count(); }
    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& value) const {
        return complex_->boundary_apply(upper_grade_, value);
    }
    [[nodiscard]] numeric::SparseRationalVector apply(numeric::SparseRationalView value) const {
        return complex_->boundary_apply_sparse(upper_grade_, value);
    }
    [[nodiscard]] numeric::SparseRationalVector apply(const numeric::SparseRationalVector& value) const {
        return apply(value.view());
    }
    [[nodiscard]] ExactSparseField apply(const ExactSparseField& value) const {
        if (&value.complex() != complex_ || value.grade() != upper_grade_) {
            throw std::invalid_argument("sparse boundary field mismatch");
        }
        return ExactSparseField(*complex_, lower_grade(), apply(value.values()));
    }
    [[nodiscard]] numeric::RationalVector transpose_apply(const numeric::RationalVector& value) const {
        return complex_->transpose_apply(upper_grade_, value);
    }
    [[nodiscard]] numeric::SparseRationalVector transpose_apply(numeric::SparseRationalView value) const {
        return complex_->transpose_apply_sparse(upper_grade_, value);
    }
    [[nodiscard]] numeric::SparseRationalVector transpose_apply(const numeric::SparseRationalVector& value) const {
        return transpose_apply(value.view());
    }
    [[nodiscard]] numeric::RationalVector adjoint(const numeric::RationalVector& lower_value,
                                                   const DiagonalMetric& lower_metric,
                                                   const DiagonalMetric& upper_metric) const {
        if (lower_metric.size() != codomain_size() || upper_metric.size() != domain_size()) {
            throw std::invalid_argument("boundary adjoint metric size mismatch");
        }
        return upper_metric.inverse_apply(transpose_apply(lower_metric.apply(lower_value)));
    }
    [[nodiscard]] numeric::RationalVector adjoint(const numeric::RationalVector& lower_value,
                                                   const ExactMetric& lower_metric,
                                                   const ExactMetric& upper_metric) const {
        if (lower_metric.size() != codomain_size() || upper_metric.size() != domain_size()) {
            throw std::invalid_argument("exact boundary adjoint metric size mismatch");
        }
        return upper_metric.solve(transpose_apply(lower_metric.apply(lower_value)));
    }
    [[nodiscard]] numeric::SparseRationalVector adjoint(numeric::SparseRationalView lower_value,
                                                        const ExactMetric& lower_metric,
                                                        const ExactMetric& upper_metric) const {
        if (!lower_metric.is_diagonal() || !upper_metric.supports_sparse_solve()) {
            throw std::logic_error("sparse boundary adjoint requires identity/diagonal exact metrics");
        }
        if (lower_metric.size() != codomain_size() || upper_metric.size() != domain_size()) {
            throw std::invalid_argument("sparse exact boundary adjoint metric size mismatch");
        }
        const auto measured=lower_metric.apply(lower_value);
        const auto pulled=transpose_apply(measured.view());
        return upper_metric.solve(pulled.view());
    }
private:
    const core::RelationalComplex* complex_{};
    core::Grade upper_grade_{};
};

} // namespace rfc::field
