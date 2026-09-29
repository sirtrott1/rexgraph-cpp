#pragma once
#include "rfc/algebra/subcomplex.hpp"
#include "rfc/core/relational_complex.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::local {

// Structure-only local carrier over one retained relational complex.  It owns
// no vector-space dimension, matrix, scalar domain, or metric.  Linear stalks,
// finite domains, set-valued systems, and future local carriers share this
// identity/patch substrate without being forced into one algebra.
class RelationalLocalCarrier {
public:
    explicit RelationalLocalCarrier(const core::RelationalComplex& complex) noexcept : complex_(&complex) {}

    RelationalLocalCarrier(const core::RelationalComplex&&) = delete;
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }

    void validate_cell(core::CellId cell) const {
        if (cell.grade.value >= complex_->grade_count()) throw std::out_of_range("local carrier cell grade");
        if (cell.index >= complex_->grade(cell.grade).cell_count()) throw std::out_of_range("local carrier cell index");
    }

    void require_closed_patch(const algebra::SubcomplexMask& patch) const {
        if (!patch.boundary_closed(*complex_)) throw std::invalid_argument("local patch must be a boundary-closed relational subcomplex");
    }

    [[nodiscard]] algebra::SubcomplexMask closure(std::vector<core::CellId> generators) const {
        algebra::SubcomplexMask patch(*complex_);
        for (const auto cell : generators) {
            validate_cell(cell);
            patch.select(cell);
        }
        patch.close_boundaries(*complex_);
        return patch;
    }

    [[nodiscard]] algebra::SubcomplexMask cell_patch(core::CellId cell) const {
        return closure({cell});
    }

private:
    const core::RelationalComplex* complex_{};
};

// Minimal structural concepts.  They deliberately say nothing about addition,
// dimension, matrices, or linearity.
template <class Map, class Input>
concept LocalMap = requires(const Map& map, const Input& input) {
    map.apply(input);
};

template <class Compatibility, class Left, class Right>
concept LocalCompatibilityPredicate = requires(const Compatibility& compatibility,
                                                const Left& left,
                                                const Right& right) {
    { compatibility(left, right) } -> std::convertible_to<bool>;
};

template <class Gluing, class Left, class Right>
concept LocalGluingOperation = requires(const Gluing& gluing, const Left& left, const Right& right) {
    gluing(left, right);
};

} // namespace rfc::local
