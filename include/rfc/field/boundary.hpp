#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/field/field.hpp"
#include "rfc/field/metric.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/numeric/exact.hpp"
#include <algorithm>
#include <functional>
#include <span>
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
    // Cells are contiguous rows of sample coefficients. Scratch belongs to the
    // caller and must be disjoint from both field buffers.
    void apply_batch_into(std::span<const numeric::Rational> input,
                          std::span<numeric::Rational> output, std::size_t samples,
                          std::span<numeric::Rational> scratch) const {
        validate_batch(input, output, samples, domain_size(), codomain_size(), scratch, samples);
        std::fill(output.begin(), output.end(), numeric::Rational{0});
        const auto& upper = complex_->grade(upper_grade_);
        const auto& lower = complex_->grade(lower_grade());
        const auto exists = complex_->signature().existence_aspect();
        for (std::size_t cell = 0; cell < domain_size(); ++cell) {
            bool active = false;
            for (std::size_t s = 0; s < samples; ++s) {
                scratch[s] = input[cell * samples + s] / upper.cell_scales[cell];
                active = active || scratch[s] != 0;
            }
            if (!active) continue;
            const auto [begin, end] = upper.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!upper.aspects.get(exists, p) || upper.lattice_coefficient[p] == 0) continue;
                const auto row = upper.lower_index[p] * samples;
                for (std::size_t s = 0; s < samples; ++s)
                    if (scratch[s] != 0) output[row + s] += scratch[s] * upper.lattice_coefficient[p];
            }
        }
        for (std::size_t cell = 0; cell < codomain_size(); ++cell)
            for (std::size_t s = 0; s < samples; ++s)
                output[cell * samples + s] *= lower.cell_scales[cell];
    }
    void transpose_batch_into(std::span<const numeric::Rational> input,
                              std::span<numeric::Rational> output, std::size_t samples,
                              std::span<numeric::Rational> scratch) const {
        const auto slots = machine::checked_size_mul(codomain_size(), samples);
        validate_batch(input, output, samples, codomain_size(), domain_size(), scratch, slots);
        std::fill(output.begin(), output.end(), numeric::Rational{0});
        const auto& upper = complex_->grade(upper_grade_);
        const auto& lower = complex_->grade(lower_grade());
        const auto exists = complex_->signature().existence_aspect();
        for (std::size_t cell = 0; cell < codomain_size(); ++cell)
            for (std::size_t s = 0; s < samples; ++s)
                scratch[cell * samples + s] = input[cell * samples + s] * lower.cell_scales[cell];
        for (std::size_t cell = 0; cell < domain_size(); ++cell) {
            const auto [begin, end] = upper.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!upper.aspects.get(exists, p) || upper.lattice_coefficient[p] == 0) continue;
                const auto row = upper.lower_index[p] * samples;
                for (std::size_t s = 0; s < samples; ++s)
                    if (scratch[row + s] != 0)
                        output[cell * samples + s] += upper.lattice_coefficient[p] * scratch[row + s];
            }
            for (std::size_t s = 0; s < samples; ++s)
                output[cell * samples + s] /= upper.cell_scales[cell];
        }
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
    static bool overlaps(std::span<const numeric::Rational> a,
                         std::span<const numeric::Rational> b) {
        if (a.empty() || b.empty()) return false;
        const std::less<const numeric::Rational*> before;
        return before(a.data(), b.data() + b.size()) && before(b.data(), a.data() + a.size());
    }
    static void validate_batch(std::span<const numeric::Rational> input,
                               std::span<numeric::Rational> output, std::size_t samples,
                               std::size_t rows_in, std::size_t rows_out,
                               std::span<numeric::Rational> scratch, std::size_t scratch_size) {
        if (!samples || input.size() != machine::checked_size_mul(rows_in, samples) ||
            output.size() != machine::checked_size_mul(rows_out, samples) || scratch.size() < scratch_size)
            throw std::invalid_argument("rational boundary batch buffer shape mismatch");
        if (overlaps(input, output) || overlaps(input, scratch) || overlaps(output, scratch))
            throw std::invalid_argument("rational boundary batch requires disjoint buffers");
    }
    const core::RelationalComplex* complex_{};
    core::Grade upper_grade_{};
};

} // namespace rfc::field
