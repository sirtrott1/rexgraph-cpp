#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/machine/packed_bits.hpp"
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace rfc::algebra {

class SubcomplexMask {
public:
    explicit SubcomplexMask(const core::RelationalComplex& ambient) : cells_() {
        cells_.reserve(ambient.grade_count());
        for (std::size_t k = 0; k < ambient.grade_count(); ++k) {
            cells_.emplace_back(ambient.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count(), false);
        }
    }

    [[nodiscard]] std::size_t grade_count() const noexcept { return cells_.size(); }
    [[nodiscard]] const machine::PackedBits& grade(core::Grade grade) const { return cells_.at(grade.value); }
    [[nodiscard]] machine::PackedBits& grade(core::Grade grade) { return cells_.at(grade.value); }
    [[nodiscard]] bool contains(core::CellId cell) const { return cells_.at(cell.grade.value).get(cell.index); }
    void select(core::CellId cell, bool value = true) { cells_.at(cell.grade.value).set(cell.index, value); }

    void close_boundaries(const core::RelationalComplex& ambient) { close_impl(ambient, false); }
    void close_active_boundaries(const core::RelationalComplex& ambient) { close_impl(ambient, true); }
    [[nodiscard]] bool boundary_closed(const core::RelationalComplex& ambient) const { return closed_impl(ambient, false); }
    [[nodiscard]] bool active_boundary_closed(const core::RelationalComplex& ambient) const { return closed_impl(ambient, true); }

private:
    void close_impl(const core::RelationalComplex& ambient, bool active_only) {
        require_ambient_shape(ambient);
        const auto exists_id = ambient.signature().existence_aspect();
        for (std::size_t k = cells_.size(); k-- > 1u;) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = ambient.grade(grade);
            cells_[k].for_each_set([&](std::size_t upper_cell) {
                const auto [begin, end] = arena.incidence_range(upper_cell);
                for (std::size_t p = begin; p < end; ++p) {
                    if (!active_only || arena.aspects.get(exists_id, p)) cells_[k - 1u].set(arena.lower_index[p], true);
                }
            });
        }
    }

    [[nodiscard]] bool closed_impl(const core::RelationalComplex& ambient, bool active_only) const {
        require_ambient_shape(ambient);
        const auto exists_id = ambient.signature().existence_aspect();
        for (std::size_t k = 1; k < cells_.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = ambient.grade(grade);
            bool ok = true;
            cells_[k].for_each_set([&](std::size_t upper_cell) {
                if (!ok) return;
                const auto [begin, end] = arena.incidence_range(upper_cell);
                for (std::size_t p = begin; p < end; ++p) {
                    if ((!active_only || arena.aspects.get(exists_id, p)) && !cells_[k - 1u].get(arena.lower_index[p])) {
                        ok = false;
                        return;
                    }
                }
            });
            if (!ok) return false;
        }
        return true;
    }

public:
    friend SubcomplexMask meet(const SubcomplexMask& lhs, const SubcomplexMask& rhs) {
        lhs.require_same_shape(rhs);
        SubcomplexMask result = lhs;
        for (std::size_t k = 0; k < result.cells_.size(); ++k) result.cells_[k] = lhs.cells_[k] & rhs.cells_[k];
        return result;
    }

    friend SubcomplexMask join(const SubcomplexMask& lhs, const SubcomplexMask& rhs) {
        lhs.require_same_shape(rhs);
        SubcomplexMask result = lhs;
        for (std::size_t k = 0; k < result.cells_.size(); ++k) result.cells_[k] = lhs.cells_[k] | rhs.cells_[k];
        return result;
    }

private:
    void require_same_shape(const SubcomplexMask& other) const {
        if (cells_.size() != other.cells_.size()) throw std::invalid_argument("subcomplex grade mismatch");
        for (std::size_t k = 0; k < cells_.size(); ++k) {
            if (cells_[k].size() != other.cells_[k].size()) throw std::invalid_argument("subcomplex cell-count mismatch");
        }
    }
    void require_ambient_shape(const core::RelationalComplex& ambient) const {
        if (cells_.size() != ambient.grade_count()) throw std::invalid_argument("subcomplex ambient grade mismatch");
        for (std::size_t k = 0; k < cells_.size(); ++k) {
            if (cells_[k].size() != ambient.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()) {
                throw std::invalid_argument("subcomplex ambient cell-count mismatch");
            }
        }
    }

    std::vector<machine::PackedBits> cells_;
};

struct RelativeComplex {
    const core::RelationalComplex* ambient{};
    SubcomplexMask subcomplex;
};

} // namespace rfc::algebra
