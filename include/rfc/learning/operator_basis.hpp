#pragma once
#include "rfc/learning/operator_channel.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

struct OperatorChannelBasis {
    const core::RelationalComplex* complex{};
    core::Grade grade{};
    std::vector<OperatorChannelKind> candidates;
    std::vector<std::size_t> independent_indices;
    // coordinates[j][i] means candidate j = sum_i coordinates[j][i] * basis_i.
    std::vector<std::vector<numeric::Rational>> coordinates;

    [[nodiscard]] std::size_t rank() const noexcept { return independent_indices.size(); }
    [[nodiscard]] std::vector<OperatorChannelKind> basis_channels() const {
        std::vector<OperatorChannelKind> out;
        out.reserve(independent_indices.size());
        for (const auto index : independent_indices) out.push_back(candidates.at(index));
        return out;
    }
    [[nodiscard]] bool all_coordinates_integral() const {
        for (const auto& row : coordinates) {
            for (const auto& value : row) if (value.get_den() != 1) return false;
        }
        return true;
    }
};

namespace detail {

inline std::vector<std::size_t> rref_pivot_columns(std::vector<std::vector<numeric::Rational>> matrix) {
    if (matrix.empty()) return {};
    const std::size_t rows = matrix.size();
    const std::size_t cols = matrix.front().size();
    for (const auto& row : matrix) if (row.size() != cols) throw std::invalid_argument("ragged rational matrix");
    std::vector<std::size_t> pivots;
    std::size_t pivot_row = 0;
    for (std::size_t col = 0; col < cols && pivot_row < rows; ++col) {
        std::size_t selected = pivot_row;
        while (selected < rows && matrix[selected][col] == 0) ++selected;
        if (selected == rows) continue;
        if (selected != pivot_row) std::swap(matrix[selected], matrix[pivot_row]);
        numeric::Rational pivot = matrix[pivot_row][col];
        for (std::size_t j = col; j < cols; ++j) matrix[pivot_row][j] /= pivot;
        for (std::size_t r = 0; r < rows; ++r) {
            if (r == pivot_row || matrix[r][col] == 0) continue;
            numeric::Rational factor = matrix[r][col];
            for (std::size_t j = col; j < cols; ++j) matrix[r][j] -= factor * matrix[pivot_row][j];
        }
        pivots.push_back(col);
        ++pivot_row;
    }
    return pivots;
}

inline std::vector<numeric::Rational> solve_full_column_rank(
    const std::vector<std::vector<numeric::Rational>>& basis_columns,
    const std::vector<numeric::Rational>& target) {
    const std::size_t rank = basis_columns.size();
    if (rank == 0) {
        for (const auto& value : target) if (value != 0) throw std::logic_error("nonzero target in zero-rank operator space");
        return {};
    }
    const std::size_t rows = target.size();
    for (const auto& column : basis_columns) if (column.size() != rows) throw std::invalid_argument("operator signature size mismatch");

    std::vector<std::vector<numeric::Rational>> aug(rows, std::vector<numeric::Rational>(rank + 1u));
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < rank; ++c) aug[r][c] = basis_columns[c][r];
        aug[r][rank] = target[r];
    }

    std::vector<std::size_t> pivot_row_for_col(rank, rows);
    std::size_t pivot_row = 0;
    for (std::size_t col = 0; col < rank && pivot_row < rows; ++col) {
        std::size_t selected = pivot_row;
        while (selected < rows && aug[selected][col] == 0) ++selected;
        if (selected == rows) throw std::logic_error("operator basis lost full column rank");
        if (selected != pivot_row) std::swap(aug[selected], aug[pivot_row]);
        numeric::Rational pivot = aug[pivot_row][col];
        for (std::size_t j = col; j <= rank; ++j) aug[pivot_row][j] /= pivot;
        for (std::size_t r = 0; r < rows; ++r) {
            if (r == pivot_row || aug[r][col] == 0) continue;
            numeric::Rational factor = aug[r][col];
            for (std::size_t j = col; j <= rank; ++j) aug[r][j] -= factor * aug[pivot_row][j];
        }
        pivot_row_for_col[col] = pivot_row;
        ++pivot_row;
    }

    for (std::size_t r = 0; r < rows; ++r) {
        bool zero = true;
        for (std::size_t c = 0; c < rank; ++c) if (aug[r][c] != 0) { zero = false; break; }
        if (zero && aug[r][rank] != 0) throw std::logic_error("candidate operator is outside computed span");
    }

    std::vector<numeric::Rational> result(rank, numeric::Rational{0});
    for (std::size_t c = 0; c < rank; ++c) result[c] = aug[pivot_row_for_col[c]][rank];
    return result;
}

using SparseIntegerColumn = std::map<std::size_t, numeric::Integer>;

inline std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>> reverse_incidence(
    const core::RelationalComplex& complex,
    core::Grade upper_grade) {
    if (upper_grade.value == 0 || upper_grade.value >= complex.grade_count()) throw std::out_of_range("reverse incidence grade");
    const auto& arena = complex.grade(upper_grade);
    const auto lower_grade = core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)};
    std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>> reverse(complex.grade(lower_grade).cell_count());
    const auto exists_id = complex.signature().existence_aspect();
    for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
        if (!arena.aspects.get(exists_id, p)) continue;
        reverse.at(arena.lower_index[p]).push_back({arena.upper_index[p], arena.lattice_coefficient[p]});
    }
    return reverse;
}

inline void add_sparse_scaled(SparseIntegerColumn& destination,
                              const SparseIntegerColumn& source,
                              const numeric::Integer& scale = numeric::Integer{1}) {
    for (const auto& [index, value] : source) {
        auto& slot = destination[index];
        slot += scale * value;
        if (slot == 0) destination.erase(index);
    }
}

inline SparseIntegerColumn sparse_operator_column(
    const core::RelationalComplex& complex,
    core::Grade grade,
    OperatorChannelKind channel,
    std::size_t input_cell,
    const std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>>* down_reverse,
    const std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>>* up_reverse) {
    const std::size_t width = complex.grade(grade).cell_count();
    if (input_cell >= width) throw std::out_of_range("operator input cell");
    SparseIntegerColumn result;
    if (channel == OperatorChannelKind::Identity) {
        result[input_cell] = 1;
        return result;
    }

    if (channel == OperatorChannelKind::HodgeDown || channel == OperatorChannelKind::Hodge) {
        if (grade.value > 0) {
            if (down_reverse == nullptr) throw std::logic_error("missing down reverse incidence");
            const auto& arena = complex.grade(grade);
            const auto [begin, end] = arena.incidence_range(input_cell);
            const auto exists_id = complex.signature().existence_aspect();
            for (std::size_t p = begin; p < end; ++p) {
                if (!arena.aspects.get(exists_id, p)) continue;
                const auto lower = arena.lower_index[p];
                const auto& input_coeff = arena.lattice_coefficient[p];
                for (const auto& [other_cell, other_coeff] : down_reverse->at(lower)) {
                    auto& slot = result[other_cell];
                    slot += other_coeff * input_coeff;
                    if (slot == 0) result.erase(other_cell);
                }
            }
        }
    }

    if (channel == OperatorChannelKind::HodgeUp || channel == OperatorChannelKind::Hodge) {
        if (grade.value < complex.max_grade().value) {
            if (up_reverse == nullptr) throw std::logic_error("missing up reverse incidence");
            const auto upper_grade = core::Grade{static_cast<std::uint16_t>(grade.value + 1u)};
            const auto& arena = complex.grade(upper_grade);
            const auto exists_id = complex.signature().existence_aspect();
            for (const auto& [higher_cell, input_coeff] : up_reverse->at(input_cell)) {
                const auto [begin, end] = arena.incidence_range(higher_cell);
                for (std::size_t p = begin; p < end; ++p) {
                    if (!arena.aspects.get(exists_id, p)) continue;
                    const auto output_cell = static_cast<std::size_t>(arena.lower_index[p]);
                    auto& slot = result[output_cell];
                    slot += arena.lattice_coefficient[p] * input_coeff;
                    if (slot == 0) result.erase(output_cell);
                }
            }
        }
    }
    return result;
}

inline std::map<std::size_t, std::vector<numeric::Rational>> sparse_operator_rows(
    const core::RelationalComplex& complex,
    core::Grade grade,
    const std::vector<OperatorChannelKind>& channels) {
    const std::size_t width = complex.grade(grade).cell_count();
    std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>> down_reverse;
    std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>> up_reverse;
    const auto* down_ptr = static_cast<const std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>>*>(nullptr);
    const auto* up_ptr = static_cast<const std::vector<std::vector<std::pair<std::size_t, numeric::Integer>>>*>(nullptr);
    if (grade.value > 0) {
        down_reverse = reverse_incidence(complex, grade);
        down_ptr = &down_reverse;
    }
    if (grade.value < complex.max_grade().value) {
        up_reverse = reverse_incidence(complex, core::Grade{static_cast<std::uint16_t>(grade.value + 1u)});
        up_ptr = &up_reverse;
    }

    std::map<std::size_t, std::vector<numeric::Rational>> rows;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        for (std::size_t input = 0; input < width; ++input) {
            const auto column = sparse_operator_column(complex, grade, channels[c], input, down_ptr, up_ptr);
            for (const auto& [output, value] : column) {
                const std::size_t key = input * width + output;
                auto [it, inserted] = rows.try_emplace(key, channels.size(), numeric::Rational{0});
                (void)inserted;
                it->second[c] = numeric::Rational(value);
            }
        }
    }
    return rows;
}

} // namespace detail

inline OperatorChannelBasis reduce_operator_channels(
    const core::RelationalComplex& complex,
    core::Grade grade,
    std::vector<OperatorChannelKind> candidates) {
    if (candidates.empty()) throw std::invalid_argument("operator channel reduction requires candidates");
    if (grade.value >= complex.grade_count()) throw std::out_of_range("operator channel reduction grade");

    const auto sparse_rows = detail::sparse_operator_rows(complex, grade, candidates);
    std::vector<std::vector<numeric::Rational>> matrix;
    matrix.reserve(sparse_rows.size());
    for (const auto& [key, row] : sparse_rows) {
        (void)key;
        matrix.push_back(row);
    }
    const auto pivots = detail::rref_pivot_columns(matrix);
    std::vector<std::vector<numeric::Rational>> candidate_columns(candidates.size(),
        std::vector<numeric::Rational>(matrix.size(), numeric::Rational{0}));
    for (std::size_t r = 0; r < matrix.size(); ++r) {
        for (std::size_t c = 0; c < candidates.size(); ++c) candidate_columns[c][r] = matrix[r][c];
    }
    std::vector<std::vector<numeric::Rational>> basis_columns;
    basis_columns.reserve(pivots.size());
    for (const auto index : pivots) basis_columns.push_back(candidate_columns[index]);

    OperatorChannelBasis result;
    result.complex = &complex;
    result.grade = grade;
    result.candidates = std::move(candidates);
    result.independent_indices = pivots;
    result.coordinates.reserve(result.candidates.size());
    for (const auto& signature : candidate_columns) result.coordinates.push_back(detail::solve_full_column_rank(basis_columns, signature));
    return result;
}

inline std::int64_t rational_to_i64_exact(const numeric::Rational& value) {
    if (value.get_den() != 1) throw std::domain_error("reduced operator coefficient is rational, not machine integer");
    const auto numerator = value.get_num();
    if (!numerator.fits_slong_p()) throw std::overflow_error("reduced operator coefficient exceeds signed long");
    const auto raw = numerator.get_si();
    if constexpr (sizeof(long) > sizeof(std::int64_t)) {
        if (raw < std::numeric_limits<std::int64_t>::min() || raw > std::numeric_limits<std::int64_t>::max()) {
            throw std::overflow_error("reduced operator coefficient exceeds int64");
        }
    }
    return static_cast<std::int64_t>(raw);
}

inline std::vector<std::int64_t> reduce_machine_channel_gains(
    const OperatorChannelBasis& basis,
    const std::vector<std::int64_t>& original_gains) {
    if (original_gains.size() != basis.candidates.size()) throw std::invalid_argument("operator gain count mismatch");
    std::vector<numeric::Rational> reduced(basis.rank(), numeric::Rational{0});
    for (std::size_t j = 0; j < original_gains.size(); ++j) {
        for (std::size_t i = 0; i < basis.rank(); ++i) reduced[i] += original_gains[j] * basis.coordinates[j][i];
    }
    std::vector<std::int64_t> result;
    result.reserve(reduced.size());
    for (const auto& value : reduced) result.push_back(rational_to_i64_exact(value));
    return result;
}

} // namespace rfc::learning
