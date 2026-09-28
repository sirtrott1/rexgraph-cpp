#pragma once
#include "rfc/composite/planes.hpp"
#include "rfc/core/grade.hpp"
#include "rfc/core/identity.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rfc::core {

struct GradeArena {
    Grade grade{};
    std::vector<RelationTypeId> relation_types;
    std::vector<IdentityKey> identities; // derived/shared object identities at grade zero
    std::vector<RelationKey> relation_identities; // primary source identity for positive-grade cells
    std::vector<numeric::Rational> cell_scales;

    // Boundary is stored structure-of-arrays by upper cell. Grade zero has no boundary.
    std::vector<std::size_t> offsets;
    std::vector<std::uint32_t> lower_index;
    std::vector<std::uint32_t> upper_index;
    std::vector<OccurrenceKey> occurrence_identities;
    std::vector<numeric::Integer> lattice_coefficient;
    composite::AspectPlanes aspects;

    // Reverse sparse index over the same retained occurrence slots.  For a
    // lower cell i, incidence_by_lower[lower_offsets[i]..lower_offsets[i+1])
    // lists the occurrence slots whose lower_index is i.
    std::vector<std::size_t> lower_offsets;
    std::vector<std::size_t> incidence_by_lower;

    [[nodiscard]] std::size_t cell_count() const noexcept { return cell_scales.size(); }
    [[nodiscard]] std::size_t incidence_count() const noexcept { return lower_index.size(); }

    [[nodiscard]] std::pair<std::size_t, std::size_t> incidence_range(std::size_t cell_index) const {
        if (grade.value == 0) throw std::logic_error("grade zero has no boundary");
        if (cell_index + 1u >= offsets.size()) throw std::out_of_range("upper relation index");
        return {offsets[cell_index], offsets[cell_index + 1u]};
    }

    [[nodiscard]] std::pair<std::size_t, std::size_t> coface_incidence_range(std::size_t lower_cell_index) const {
        if (grade.value == 0) throw std::logic_error("grade zero has no upper incidence index");
        if (lower_cell_index + 1u >= lower_offsets.size()) throw std::out_of_range("lower cell index");
        return {lower_offsets[lower_cell_index], lower_offsets[lower_cell_index + 1u]};
    }
};

} // namespace rfc::core
