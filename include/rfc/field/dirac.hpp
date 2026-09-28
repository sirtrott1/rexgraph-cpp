#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/field/boundary.hpp"
#include "rfc/field/metric.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace rfc::field {

struct TowerField {
    std::vector<numeric::RationalVector> grades;
};

inline TowerField dirac_apply(const core::RelationalComplex& complex,
                              const std::vector<DiagonalMetric>& metrics,
                              const TowerField& field) {
    if (metrics.size() != complex.grade_count() || field.grades.size() != complex.grade_count()) {
        throw std::invalid_argument("Dirac tower size mismatch");
    }
    TowerField result;
    result.grades.reserve(complex.grade_count());
    for (std::size_t k = 0; k < complex.grade_count(); ++k) {
        if (field.grades[k].size() != complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()) {
            throw std::invalid_argument("Dirac field grade size mismatch");
        }
        result.grades.emplace_back(field.grades[k].size(), numeric::Rational{0});
    }
    for (std::size_t k = 1; k < complex.grade_count(); ++k) {
        BoundaryOperator boundary(complex, core::Grade{static_cast<std::uint16_t>(k)});
        result.grades[k - 1u] = numeric::add(result.grades[k - 1u], boundary.apply(field.grades[k]));
        result.grades[k] = numeric::add(result.grades[k], boundary.adjoint(field.grades[k - 1u], metrics[k - 1u], metrics[k]));
    }
    return result;
}

} // namespace rfc::field
