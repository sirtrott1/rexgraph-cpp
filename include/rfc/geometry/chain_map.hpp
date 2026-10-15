#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/geometry/contract.hpp"
#include "rfc/numeric/exact.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::geometry {

struct LinearMapTerm {
    std::uint32_t target{};
    numeric::Rational coefficient{0};
};

using LinearCellImage = std::vector<LinearMapTerm>;
using LinearGradeMap = std::vector<LinearCellImage>;

// Exact linear morphism between relational-chain realizations.  This is the
// correct semantic-scale primitive when one macro cell realizes as a chain of
// several micro cells rather than as one renamed cell.
class RelationalChainMap {
public:
    [[nodiscard]] static constexpr GeometryDescriptor geometry_descriptor() noexcept {
        return {GeometryKind::Coordinate, "exact relational chain realization", true};
    }
    RelationalChainMap(const core::RelationalComplex& source,
                       const core::RelationalComplex& target,
                       std::vector<LinearGradeMap> grade_maps)
        : source_(&source), target_(&target), grade_maps_(std::move(grade_maps)) {
        if (grade_maps_.size() != source.grade_count()) throw std::invalid_argument("linear morphism source grade mismatch");
        if (target.grade_count() < source.grade_count()) throw std::invalid_argument("linear morphism target has insufficient grades");
        for (std::size_t k = 0; k < grade_maps_.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            if (grade_maps_[k].size() != source.grade(grade).cell_count()) throw std::invalid_argument("linear morphism source cell-count mismatch");
            for (const auto& image : grade_maps_[k]) {
                for (const auto& term : image) {
                    if (term.target >= target.grade(grade).cell_count()) throw std::out_of_range("linear morphism target cell index");
                }
            }
        }
    }

    [[nodiscard]] const core::RelationalComplex& source() const noexcept { return *source_; }
    [[nodiscard]] const core::RelationalComplex& target() const noexcept { return *target_; }
    [[nodiscard]] const LinearCellImage& image(core::Grade grade, std::size_t source_cell) const {
        return grade_maps_.at(grade.value).at(source_cell);
    }

    [[nodiscard]] numeric::RationalVector push_field(core::Grade grade,
                                                      const numeric::RationalVector& source_field) const {
        if (source_field.size() != source_->grade(grade).cell_count()) throw std::invalid_argument("linear morphism field size mismatch");
        numeric::RationalVector result(target_->grade(grade).cell_count(), numeric::Rational{0});
        for (std::size_t source_cell = 0; source_cell < source_field.size(); ++source_cell) {
            if (source_field[source_cell] == 0) continue;
            for (const auto& term : grade_maps_[grade.value][source_cell]) {
                result[term.target] += term.coefficient * source_field[source_cell];
            }
        }
        return result;
    }

    // Transport only active numerical support. Retained source/target cells and
    // their occurrence identities remain owned by the two relational carriers;
    // cancellation here removes a stored amplitude, never a primary cell.
    [[nodiscard]] numeric::SparseRationalVector push_field(
        core::Grade grade, numeric::SparseRationalView source_field) const {
        if (source_field.size() != source_->grade(grade).cell_count())
            throw std::invalid_argument("sparse linear morphism field size mismatch");
        const auto& images = grade_maps_.at(grade.value);
        std::map<std::size_t, numeric::Rational> accumulated;
        for (const auto& entry : source_field.entries()) {
            const auto& image = images.at(entry.index);
            if (entry.value == 0) continue;
            for (const auto& term : image) {
                if (term.coefficient == 0) continue;
                numeric::Rational value = accumulated[term.target] +
                                          entry.value * term.coefficient;
                value.canonicalize();
                if (value == 0) accumulated.erase(term.target);
                else accumulated[term.target] = std::move(value);
            }
        }
        std::vector<numeric::SparseRationalEntry> entries;
        entries.reserve(accumulated.size());
        for (auto& [index, value] : accumulated)
            entries.push_back({index, std::move(value)});
        return numeric::SparseRationalVector(target_->grade(grade).cell_count(),
                                              std::move(entries));
    }

    [[nodiscard]] bool is_chain_map() const {
        using Accumulator = std::map<std::uint32_t, numeric::Rational>;
        const auto add = [](Accumulator& out, std::uint32_t index, const numeric::Rational& value) {
            if (value == 0) return;
            numeric::Rational updated = out[index] + value;
            updated.canonicalize();
            if (updated == 0) out.erase(index);
            else out[index] = std::move(updated);
        };

        for (std::size_t k = 1; k < source_->grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto lower = core::Grade{static_cast<std::uint16_t>(k - 1u)};
            const auto& source_arena = source_->grade(grade);
            const auto& target_arena = target_->grade(grade);
            const auto source_exists = source_->signature().existence_aspect();
            const auto target_exists = target_->signature().existence_aspect();

            for (std::size_t cell = 0; cell < source_arena.cell_count(); ++cell) {
                Accumulator lhs;
                const auto [sb, se] = source_arena.incidence_range(cell);
                for (std::size_t p = sb; p < se; ++p) {
                    if (!source_arena.aspects.get(source_exists, p)) continue;
                    const auto coefficient = source_->incidence_coefficient(grade, p);
                    for (const auto& term : grade_maps_[lower.value][source_arena.lower_index[p]]) {
                        add(lhs, term.target, coefficient * term.coefficient);
                    }
                }

                Accumulator rhs;
                for (const auto& upper_term : grade_maps_[grade.value][cell]) {
                    if (upper_term.coefficient == 0) continue;
                    const auto [tb, te] = target_arena.incidence_range(upper_term.target);
                    for (std::size_t p = tb; p < te; ++p) {
                        if (!target_arena.aspects.get(target_exists, p)) continue;
                        add(rhs, target_arena.lower_index[p],
                            upper_term.coefficient * target_->incidence_coefficient(grade, p));
                    }
                }
                if (lhs != rhs) return false;
            }
        }
        return true;
    }

private:
    const core::RelationalComplex* source_{};
    const core::RelationalComplex* target_{};
    std::vector<LinearGradeMap> grade_maps_;
};

inline RelationalChainMap identity_chain_map(const core::RelationalComplex& complex) {
    std::vector<LinearGradeMap> maps;
    maps.reserve(complex.grade_count());
    for (std::size_t k = 0; k < complex.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        LinearGradeMap map(complex.grade(grade).cell_count());
        for (std::size_t i = 0; i < map.size(); ++i) map[i].push_back(LinearMapTerm{static_cast<std::uint32_t>(i), numeric::Rational{1}});
        maps.push_back(std::move(map));
    }
    return RelationalChainMap(complex, complex, std::move(maps));
}

inline RelationalChainMap compose_chain_maps(const RelationalChainMap& first,
                                              const RelationalChainMap& second) {
    if (&first.target() != &second.source()) throw std::invalid_argument("chain-map composition mismatch");
    std::vector<LinearGradeMap> maps;
    maps.reserve(first.source().grade_count());
    for (std::size_t k = 0; k < first.source().grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        LinearGradeMap grade_map(first.source().grade(grade).cell_count());
        for (std::size_t source_cell = 0; source_cell < grade_map.size(); ++source_cell) {
            std::map<std::uint32_t, numeric::Rational> accumulated;
            for (const auto& mid : first.image(grade, source_cell)) {
                for (const auto& out : second.image(grade, mid.target)) {
                    numeric::Rational value = accumulated[out.target] + mid.coefficient * out.coefficient;
                    value.canonicalize();
                    if (value == 0) accumulated.erase(out.target);
                    else accumulated[out.target] = std::move(value);
                }
            }
            for (const auto& [out, value] : accumulated) grade_map[source_cell].push_back(LinearMapTerm{out, value});
        }
        maps.push_back(std::move(grade_map));
    }
    return RelationalChainMap(first.source(), second.target(), std::move(maps));
}

inline bool same_chain_map(const RelationalChainMap& lhs, const RelationalChainMap& rhs) {
    if (&lhs.source() != &rhs.source() || &lhs.target() != &rhs.target()) return false;
    for (std::size_t k = 0; k < lhs.source().grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const auto width = lhs.source().grade(grade).cell_count();
        for (std::size_t i = 0; i < width; ++i) {
            std::map<std::uint32_t, numeric::Rational> a, b;
            for (const auto& term : lhs.image(grade, i)) a[term.target] += term.coefficient;
            for (const auto& term : rhs.image(grade, i)) b[term.target] += term.coefficient;
            for (auto it = a.begin(); it != a.end();) { it->second.canonicalize(); if (it->second == 0) it = a.erase(it); else ++it; }
            for (auto it = b.begin(); it != b.end();) { it->second.canonicalize(); if (it->second == 0) it = b.erase(it); else ++it; }
            if (a != b) return false;
        }
    }
    return true;
}

} // namespace rfc::geometry
