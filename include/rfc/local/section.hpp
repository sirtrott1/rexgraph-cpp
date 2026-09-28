#pragma once
#include "rfc/core/identity.hpp"
#include "rfc/local/carrier.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace rfc::local {

struct CellIdHash {
    std::size_t operator()(const core::CellId& cell) const noexcept {
        return (static_cast<std::size_t>(cell.grade.value) << 32u) ^ cell.index;
    }
};

template <class Value>
using BasicSection = std::unordered_map<core::CellId, Value, CellIdHash>;

using LinearSection = BasicSection<numeric::RationalVector>;
// Compatibility alias retained for the pre-realignment public API.  New code
// should prefer LinearSection when the value carrier is a rational vector.
using Section = LinearSection;

enum class OverlapCoverage : std::uint8_t {
    // Every cell of the overlap must be represented by both sections.
    RequireComplete,
    // Only values represented by both sections are compared.  This is useful
    // for partial finite assignments during exact constraint search.
    CompareShared
};

enum class LocalObstructionKind : std::uint8_t {
    None,
    LocalPatchUnsatisfiable,
    NoCompatibleGlobalSection,
    CoverOmitsConstraint,
    RestrictionIncompatible
};

struct LocalObstructionCertificate {
    LocalObstructionKind kind{LocalObstructionKind::None};
    std::vector<std::size_t> patch_indices;
    std::vector<std::size_t> chart_indices;
    std::vector<core::CellId> cells;
};

template <class Value>
[[nodiscard]] BasicSection<Value> restrict_section(const RelationalLocalCarrier& carrier,
                                                   const BasicSection<Value>& section,
                                                   const algebra::SubcomplexMask& patch) {
    carrier.require_closed_patch(patch);
    BasicSection<Value> result;
    result.reserve(section.size());
    for (const auto& [cell, value] : section) {
        carrier.validate_cell(cell);
        if (patch.contains(cell)) result.emplace(cell, value);
    }
    return result;
}

template <class Value, class Equal = std::equal_to<Value>>
[[nodiscard]] bool sections_compatible_on_overlap(const RelationalLocalCarrier& carrier,
                                                   const BasicSection<Value>& left,
                                                   const algebra::SubcomplexMask& left_patch,
                                                   const BasicSection<Value>& right,
                                                   const algebra::SubcomplexMask& right_patch,
                                                   OverlapCoverage coverage,
                                                   Equal equal = {}) {
    carrier.require_closed_patch(left_patch);
    carrier.require_closed_patch(right_patch);
    const auto overlap = meet(left_patch, right_patch);
    for (std::size_t k = 0; k < overlap.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        bool ok = true;
        overlap.grade(grade).for_each_set([&](std::size_t index) {
            if (!ok) return;
            const core::CellId cell{grade, static_cast<std::uint32_t>(index)};
            const auto li = left.find(cell);
            const auto ri = right.find(cell);
            if (coverage == OverlapCoverage::RequireComplete &&
                (li == left.end() || ri == right.end())) {
                ok = false;
                return;
            }
            if (li != left.end() && ri != right.end() && !equal(li->second, ri->second)) ok = false;
        });
        if (!ok) return false;
    }
    return true;
}

template <class Value, class Equal = std::equal_to<Value>>
[[nodiscard]] BasicSection<Value> glue_sections(const RelationalLocalCarrier& carrier,
                                                const BasicSection<Value>& left,
                                                const algebra::SubcomplexMask& left_patch,
                                                const BasicSection<Value>& right,
                                                const algebra::SubcomplexMask& right_patch,
                                                OverlapCoverage coverage,
                                                Equal equal = {}) {
    if (!sections_compatible_on_overlap(carrier, left, left_patch, right, right_patch, coverage, equal)) {
        throw std::invalid_argument("local sections disagree on the relational overlap");
    }
    BasicSection<Value> result;
    result.reserve(left.size() + right.size());
    for (const auto& [cell, value] : left) result.emplace(cell, value);
    for (const auto& [cell, value] : right) {
        const auto [it, inserted] = result.emplace(cell, value);
        if (!inserted && !equal(it->second, value)) throw std::logic_error("overlap check/glue disagreement");
    }
    return result;
}

} // namespace rfc::local
