#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::path {

struct TraversalStep {
    std::uint32_t relation{};
    numeric::Rational coefficient{1};
};

struct RealizedPath {
    core::Grade grade{1};
    std::vector<TraversalStep> ordered_steps;
    numeric::RationalVector field;
    numeric::RationalVector boundary;
};

inline RealizedPath realize(const core::RelationalComplex& complex,
                            core::Grade grade,
                            std::vector<TraversalStep> ordered_steps) {
    if (grade.value == 0 || grade.value >= complex.grade_count()) throw std::out_of_range("path realization grade");
    numeric::RationalVector field(complex.grade(grade).cell_count(), numeric::Rational{0});
    for (const auto& step : ordered_steps) {
        if (step.relation >= field.size()) throw std::out_of_range("path relation index");
        field[step.relation] += step.coefficient;
    }
    auto boundary = complex.boundary_apply(grade, field);
    return {grade, std::move(ordered_steps), std::move(field), std::move(boundary)};
}

inline numeric::ExactMatrix realization_matrix(const std::vector<RealizedPath>& paths) {
    if (paths.empty()) return numeric::ExactMatrix{};
    const auto grade = paths.front().grade;
    const auto rows = paths.front().field.size();
    numeric::ExactMatrix out(rows, paths.size());
    for (std::size_t j = 0; j < paths.size(); ++j) {
        if (paths[j].grade != grade || paths[j].field.size() != rows) throw std::invalid_argument("path realization family mismatch");
        for (std::size_t i = 0; i < rows; ++i) if (paths[j].field[i] != 0) out.set(i, j, paths[j].field[i]);
    }
    return out;
}

inline numeric::ExactMatrix inherited_gram(const std::vector<RealizedPath>& paths,
                                           const field::ExactMetric& metric) {
    const auto P = realization_matrix(paths);
    if (P.rows() != metric.size()) throw std::invalid_argument("path metric size mismatch");
    return field::metric_gram(P,metric,P);
}

} // namespace rfc::path
