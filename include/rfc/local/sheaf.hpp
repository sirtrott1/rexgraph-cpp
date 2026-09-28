#pragma once
#include "rfc/local/fiber_system.hpp"
#include "rfc/local/section.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace rfc::local {

// Linear section restriction/gluing over the boundary-closed relational
// topology.  The common restriction/overlap/gluing substrate is value-neutral;
// only transport_compatible and cell_section below depend on rational-vector
// stalks and exact linear flag maps.
class LinearRelationalSheaf {
public:
    explicit LinearRelationalSheaf(const LinearRelationalFiberSystem& fibers)
        : fibers_(&fibers), carrier_(fibers.complex()) {}

    [[nodiscard]] const LinearRelationalFiberSystem& fibers() const noexcept { return *fibers_; }
    [[nodiscard]] const RelationalLocalCarrier& carrier() const noexcept { return carrier_; }

    [[nodiscard]] LinearSection restrict_to(const LinearSection& section,
                                            const algebra::SubcomplexMask& patch) const {
        validate_section_values(section);
        return restrict_section(carrier_, section, patch);
    }

    [[nodiscard]] bool compatible_on_overlap(const LinearSection& left,
                                             const algebra::SubcomplexMask& left_patch,
                                             const LinearSection& right,
                                             const algebra::SubcomplexMask& right_patch) const {
        validate_section_values(left);
        validate_section_values(right);
        return sections_compatible_on_overlap(carrier_, left, left_patch, right, right_patch,
                                              OverlapCoverage::RequireComplete);
    }

    [[nodiscard]] LinearSection glue(const LinearSection& left,
                                     const algebra::SubcomplexMask& left_patch,
                                     const LinearSection& right,
                                     const algebra::SubcomplexMask& right_patch) const {
        validate_section_values(left);
        validate_section_values(right);
        return glue_sections(carrier_, left, left_patch, right, right_patch,
                             OverlapCoverage::RequireComplete);
    }

    [[nodiscard]] bool transport_compatible(const LinearSection& section,
                                            const algebra::SubcomplexMask& patch) const {
        carrier_.require_closed_patch(patch);
        const auto& complex = fibers_->complex();
        const auto exists_id = complex.signature().existence_aspect();
        for (std::size_t k = 1; k < complex.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = complex.grade(grade);
            bool ok = true;
            patch.grade(grade).for_each_set([&](std::size_t upper_index) {
                if (!ok) return;
                const core::CellId upper{grade, static_cast<std::uint32_t>(upper_index)};
                const auto up = section.find(upper);
                if (up == section.end()) { ok = false; return; }
                require_value_dimension(upper, up->second);
                const auto [begin, end] = arena.incidence_range(upper_index);
                for (std::size_t slot = begin; slot < end; ++slot) {
                    if (!arena.aspects.get(exists_id, slot)) continue;
                    const core::CellId lower{core::Grade{static_cast<std::uint16_t>(k - 1u)}, arena.lower_index[slot]};
                    const auto lo = section.find(lower);
                    if (lo == section.end()) { ok = false; return; }
                    require_value_dimension(lower, lo->second);
                    if (fibers_->flag_map(grade, slot).transport.apply(up->second) != lo->second) { ok = false; return; }
                }
            });
            if (!ok) return false;
        }
        return true;
    }

    [[nodiscard]] LinearSection cell_section(const RelationLocalSection& local) const {
        LinearSection result;
        require_value_dimension(local.relation, local.upper_value);
        result.emplace(local.relation, local.upper_value);
        for (const auto& occurrence : local.boundary_values) {
            require_value_dimension(occurrence.lower, occurrence.value);
            const auto [it, inserted] = result.emplace(occurrence.lower, occurrence.value);
            if (!inserted && it->second != occurrence.value) {
                throw std::invalid_argument("one local relation realizes conflicting copies of the same lower stalk");
            }
        }
        return result;
    }

private:
    void require_value_dimension(core::CellId cell, const numeric::RationalVector& value) const {
        carrier_.validate_cell(cell);
        if (value.size() != fibers_->stalks().dimension(cell)) throw std::invalid_argument("section value does not match linear stalk dimension");
    }

    void validate_section_values(const LinearSection& section) const {
        for (const auto& [cell, value] : section) require_value_dimension(cell, value);
    }

    const LinearRelationalFiberSystem* fibers_{};
    RelationalLocalCarrier carrier_;
};

// Compatibility alias retained for pre-realignment code.  New code should use
// LinearRelationalSheaf so that a generic relational local system is not
// mistaken for a vector-space sheaf.
using RelationalSheaf = LinearRelationalSheaf;

} // namespace rfc::local
