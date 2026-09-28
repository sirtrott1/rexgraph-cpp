#pragma once
#include "rfc/learning/operator_basis.hpp"
#include "rfc/learning/tower_operator.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

struct TowerOperatorBasis {
    const core::RelationalComplex* complex{};
    std::vector<TowerOperatorChannelKind> candidates;
    std::vector<std::size_t> independent_indices;
    std::vector<std::vector<numeric::Rational>> coordinates;

    [[nodiscard]] std::size_t rank() const noexcept { return independent_indices.size(); }
    [[nodiscard]] std::vector<TowerOperatorChannelKind> basis_channels() const {
        std::vector<TowerOperatorChannelKind> out;
        out.reserve(independent_indices.size());
        for (const auto index : independent_indices) out.push_back(candidates.at(index));
        return out;
    }
    [[nodiscard]] bool all_coordinates_integral() const {
        for (const auto& row : coordinates) for (const auto& value : row) if (value.get_den() != 1) return false;
        return true;
    }
};

namespace tower_detail {

inline std::vector<std::size_t> grade_offsets(const core::RelationalComplex& complex) {
    std::vector<std::size_t> offsets(complex.grade_count() + 1u, 0u);
    for (std::size_t k = 0; k < complex.grade_count(); ++k) {
        offsets[k + 1u] = offsets[k] + complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
    }
    return offsets;
}

inline std::map<std::size_t, std::vector<numeric::Rational>> sparse_tower_operator_rows(
    const core::RelationalComplex& complex,
    const std::vector<TowerOperatorChannelKind>& channels) {
    const auto offsets = grade_offsets(complex);
    const std::size_t total = offsets.back();
    std::map<std::size_t, std::vector<numeric::Rational>> rows;

    auto add = [&](std::size_t channel, std::size_t output, std::size_t input, const numeric::Integer& value) {
        if (value == 0) return;
        const std::size_t key = input * total + output;
        auto [it, inserted] = rows.try_emplace(key, channels.size(), numeric::Rational{0});
        (void)inserted;
        it->second[channel] += numeric::Rational(value);
    };

    for (std::size_t c = 0; c < channels.size(); ++c) {
        const auto channel = channels[c];
        if (channel == TowerOperatorChannelKind::Identity) {
            for (std::size_t i = 0; i < total; ++i) add(c, i, i, numeric::Integer{1});
            continue;
        }
        const bool lowering = channel == TowerOperatorChannelKind::Lowering || channel == TowerOperatorChannelKind::Dirac;
        const bool raising = channel == TowerOperatorChannelKind::Raising || channel == TowerOperatorChannelKind::Dirac;
        for (std::size_t k = 1; k < complex.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = complex.grade(grade);
            const auto exists_id = complex.signature().existence_aspect();
            for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
                if (!arena.aspects.get(exists_id, p)) continue;
                const std::size_t upper = offsets[k] + arena.upper_index[p];
                const std::size_t lower = offsets[k - 1u] + arena.lower_index[p];
                if (lowering) add(c, lower, upper, arena.lattice_coefficient[p]);
                if (raising) add(c, upper, lower, arena.lattice_coefficient[p]);
            }
        }
    }
    return rows;
}

} // namespace tower_detail

inline TowerOperatorBasis reduce_tower_operator_channels(
    const core::RelationalComplex& complex,
    std::vector<TowerOperatorChannelKind> candidates) {
    if (candidates.empty()) throw std::invalid_argument("tower operator reduction requires candidates");
    const auto sparse_rows = tower_detail::sparse_tower_operator_rows(complex, candidates);
    std::vector<std::vector<numeric::Rational>> matrix;
    matrix.reserve(sparse_rows.size());
    for (const auto& [key, row] : sparse_rows) { (void)key; matrix.push_back(row); }
    const auto pivots = detail::rref_pivot_columns(matrix);
    std::vector<std::vector<numeric::Rational>> candidate_columns(candidates.size(),
        std::vector<numeric::Rational>(matrix.size(), numeric::Rational{0}));
    for (std::size_t r = 0; r < matrix.size(); ++r) {
        for (std::size_t c = 0; c < candidates.size(); ++c) candidate_columns[c][r] = matrix[r][c];
    }
    std::vector<std::vector<numeric::Rational>> basis_columns;
    basis_columns.reserve(pivots.size());
    for (const auto index : pivots) basis_columns.push_back(candidate_columns[index]);

    TowerOperatorBasis result;
    result.complex = &complex;
    result.candidates = std::move(candidates);
    result.independent_indices = pivots;
    result.coordinates.reserve(result.candidates.size());
    for (const auto& signature : candidate_columns) result.coordinates.push_back(detail::solve_full_column_rank(basis_columns, signature));
    return result;
}

inline std::vector<std::int64_t> reduce_machine_tower_channel_gains(
    const TowerOperatorBasis& basis,
    const std::vector<std::int64_t>& original_gains) {
    if (original_gains.size() != basis.candidates.size()) throw std::invalid_argument("tower operator gain count mismatch");
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
