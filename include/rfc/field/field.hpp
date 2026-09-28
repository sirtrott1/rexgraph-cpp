#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/exact.hpp"
#include "rfc/numeric/index_domain.hpp"
#include "rfc/numeric/sparse_tensor.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/numeric/structural_support.hpp"
#include <stdexcept>
#include <utility>

namespace rfc::field {

// Dense small-grade exact field.  The relational carrier owns existence;
// numerical zero here never means that a primary relation is absent.
class ExactField {
public:
    ExactField(const core::RelationalComplex& complex, core::Grade grade, numeric::RationalVector values)
        : complex_(&complex), grade_(grade), values_(std::move(values)) {
        if (values_.size() != complex.grade(grade).cell_count()) throw std::invalid_argument("field size mismatch");
    }
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const numeric::RationalVector& values() const noexcept { return values_; }
    [[nodiscard]] numeric::RationalVector& values() noexcept { return values_; }
private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    numeric::RationalVector values_;
};

// Sparse numerical state on the full retained primary-cell carrier of one
// grade.  The full structural support is implicit and allocation-free; only
// nonzero amplitudes are stored.  Therefore a missing stored value means zero
// amplitude on an existing relation, not structural absence.
class ExactSparseField {
public:
    ExactSparseField(const core::RelationalComplex& complex,
                     core::Grade grade,
                     numeric::SparseRationalVector values)
        : complex_(&complex), grade_(grade), tensor_(make_tensor(complex, grade, std::move(values))) {}

    [[nodiscard]] static ExactSparseField zero(const core::RelationalComplex& complex,
                                               core::Grade grade) {
        return ExactSparseField(complex, grade,
                                numeric::SparseRationalVector(complex.grade(grade).cell_count()));
    }

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const numeric::SparseRationalTensor& tensor() const noexcept { return tensor_; }
    [[nodiscard]] const numeric::SparseRationalVector& values() const noexcept { return tensor_.values(); }
    [[nodiscard]] std::size_t stored_nonzeros() const noexcept { return tensor_.stored_nonzeros(); }

private:
    [[nodiscard]] static numeric::SparseRationalTensor make_tensor(
        const core::RelationalComplex& complex,
        core::Grade grade,
        numeric::SparseRationalVector values) {
        const auto cells = complex.grade(grade).cell_count();
        if (values.size() != cells) throw std::invalid_argument("sparse field size mismatch");
        return numeric::SparseRationalTensor(
            numeric::IndexDomain::linear(cells),
            numeric::StructuralSupport::full(cells),
            std::move(values));
    }

    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    numeric::SparseRationalTensor tensor_;
};

} // namespace rfc::field
