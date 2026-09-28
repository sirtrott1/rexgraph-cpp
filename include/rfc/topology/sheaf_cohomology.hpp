#pragma once
#include "rfc/local/cellular_sheaf.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::topology {

struct SheafCohomologyGroup {
    core::Grade grade{};
    std::size_t cochain_dimension{};
    std::size_t cocycle_dimension{};
    std::size_t coboundary_dimension{};
    std::size_t dimension{};
    numeric::ExactMatrix cocycle_frame;
    numeric::ExactMatrix coboundary_frame;
    numeric::ExactMatrix representative_frame;
};

inline numeric::ExactMatrix matrix_column(const numeric::ExactMatrix& matrix, std::size_t column) {
    if (column >= matrix.cols()) throw std::out_of_range("matrix column");
    numeric::ExactMatrix out(matrix.rows(), 1u);
    for (std::size_t row = 0; row < matrix.rows(); ++row) {
        const auto value = matrix.get(row, column);
        if (value != 0) out.set(row, 0u, value);
    }
    return out;
}

inline SheafCohomologyGroup sheaf_cohomology(const local::LinearCellularSheaf& sheaf,
                                              core::Grade grade) {
    if (grade.value >= sheaf.complex().grade_count()) throw std::out_of_range("sheaf cohomology grade");
    if (!sheaf.is_cochain_complex()) throw std::invalid_argument("sheaf restrictions do not define a cochain complex");

    const auto delta = sheaf.coboundary(grade);
    const auto cocycles = delta.kernel_frame();
    numeric::ExactMatrix coboundaries(sheaf.cochain_dimension(grade), 0u);
    if (grade.value > 0) {
        const auto previous = sheaf.coboundary(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)});
        coboundaries = previous.image_frame();
        if (delta.multiply(coboundaries).nonzeros() != 0u) {
            throw std::logic_error("sheaf coboundaries are not cocycles");
        }
    }

    numeric::ExactMatrix span = coboundaries;
    numeric::ExactMatrix representatives(sheaf.cochain_dimension(grade), 0u);
    std::size_t span_rank = span.rank();
    for (std::size_t column = 0; column < cocycles.cols(); ++column) {
        const auto candidate_column = matrix_column(cocycles, column);
        const auto candidate = numeric::ExactMatrix::horizontal_concat(span, candidate_column);
        const auto candidate_rank = candidate.rank();
        if (candidate_rank > span_rank) {
            representatives = numeric::ExactMatrix::horizontal_concat(representatives, candidate_column);
            span = candidate;
            span_rank = candidate_rank;
        }
    }

    const auto quotient_dimension = representatives.cols();
    const auto expected = cocycles.cols() - coboundaries.cols();
    if (quotient_dimension != expected) throw std::logic_error("sheaf cohomology quotient basis dimension mismatch");

    return SheafCohomologyGroup{
        grade,
        sheaf.cochain_dimension(grade),
        cocycles.cols(),
        coboundaries.cols(),
        quotient_dimension,
        cocycles,
        coboundaries,
        representatives};
}

inline std::vector<SheafCohomologyGroup> sheaf_cohomology(const local::LinearCellularSheaf& sheaf) {
    std::vector<SheafCohomologyGroup> groups;
    groups.reserve(sheaf.complex().grade_count());
    for (std::size_t q = 0; q < sheaf.complex().grade_count(); ++q) {
        groups.push_back(sheaf_cohomology(sheaf, core::Grade{static_cast<std::uint16_t>(q)}));
    }
    return groups;
}

} // namespace rfc::topology
