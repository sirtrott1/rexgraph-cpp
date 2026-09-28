#pragma once
#include "rfc/algebra/morphism.hpp"
#include "rfc/core/relational_complex.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::local {

// Strict finite relational covering.  The map is grade preserving, a chain
// map, surjective on retained cells, and locally bijective on active adjacent
// incidence flags both downward and upward.  This is a covering of the
// retained relational incidence structure, not a graph projection substituted
// for it.
class StrictRelationalCovering {
public:
    StrictRelationalCovering(const core::RelationalComplex& source,
                             const core::RelationalComplex& target,
                             std::vector<std::vector<std::uint32_t>> cell_maps)
        : morphism_(source, target, std::move(cell_maps)) {
        if (!validate()) throw std::invalid_argument("map is not a strict relational covering");
    }

    [[nodiscard]] const core::RelationalComplex& source() const noexcept { return morphism_.source(); }
    [[nodiscard]] const core::RelationalComplex& target() const noexcept { return morphism_.target(); }
    [[nodiscard]] const algebra::RelationalMorphism& morphism() const noexcept { return morphism_; }
    [[nodiscard]] std::uint32_t image(core::CellId source_cell) const { return morphism_.image(source_cell); }

    [[nodiscard]] std::vector<core::CellId> fiber(core::CellId target_cell) const {
        if (target_cell.grade.value >= target().grade_count() || target_cell.index >= target().grade(target_cell.grade).cell_count()) {
            throw std::out_of_range("covering target fiber cell");
        }
        std::vector<core::CellId> result;
        const auto& map = morphism_.cell_maps().at(target_cell.grade.value);
        for (std::size_t i = 0; i < map.size(); ++i) {
            if (map[i] == target_cell.index) {
                result.push_back(core::CellId{target_cell.grade, static_cast<std::uint32_t>(i)});
            }
        }
        return result;
    }

    [[nodiscard]] bool validate() const {
        if (source().grade_count() != target().grade_count()) return false;
        if (!same_signature_shape()) return false;
        if (!morphism_.is_chain_map()) return false;
        if (!surjective_on_cells()) return false;
        for (std::size_t k = 0; k < source().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            for (std::size_t cell = 0; cell < source().grade(grade).cell_count(); ++cell) {
                if (!local_star_bijection(core::CellId{grade, static_cast<std::uint32_t>(cell)})) return false;
            }
        }
        return true;
    }

private:
    [[nodiscard]] bool same_signature_shape() const {
        const auto& a = source().signature();
        const auto& b = target().signature();
        if (a.aspects().size() != b.aspects().size() || a.type_count() != b.type_count()) return false;
        for (std::size_t i = 0; i < a.aspects().size(); ++i) {
            const auto id = static_cast<composite::AspectId>(i);
            const auto& da = a.aspects().at(id);
            const auto& db = b.aspects().at(id);
            if (da.name != db.name || da.role != db.role || da.default_value != db.default_value) return false;
        }
        for (std::size_t i = 0; i < a.type_count(); ++i) {
            const auto& ta = a.type(static_cast<core::RelationTypeId>(i));
            const auto& tb = b.type(static_cast<core::RelationTypeId>(i));
            if (ta.name != tb.name || ta.boundary_policy != tb.boundary_policy ||
                ta.min_arity != tb.min_arity || ta.max_arity != tb.max_arity ||
                ta.constraints.shape != tb.constraints.shape) return false;
        }
        return true;
    }

    [[nodiscard]] bool surjective_on_cells() const {
        for (std::size_t k = 0; k < target().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            std::vector<bool> hit(target().grade(grade).cell_count(), false);
            for (const auto mapped : morphism_.cell_maps().at(k)) {
                if (mapped == algebra::RelationalMorphism::absent || mapped >= hit.size()) return false;
                hit[mapped] = true;
            }
            for (const auto value : hit) if (!value) return false;
        }
        return true;
    }

    [[nodiscard]] bool same_active_flag(core::Grade upper_grade,
                                        std::size_t source_slot,
                                        std::size_t target_slot) const {
        const auto& sa = source().grade(upper_grade);
        const auto& ta = target().grade(upper_grade);
        if (source().incidence_coefficient(upper_grade, source_slot) !=
            target().incidence_coefficient(upper_grade, target_slot)) return false;
        const auto count = source().signature().aspects().size();
        for (std::size_t a = 0; a < count; ++a) {
            const auto aspect = static_cast<composite::AspectId>(a);
            if (sa.aspects.get(aspect, source_slot) != ta.aspects.get(aspect, target_slot)) return false;
        }
        return true;
    }

    [[nodiscard]] bool same_cell_type(core::CellId source_cell) const {
        if (source_cell.grade.value == 0) return true;
        const auto mapped = image(source_cell);
        const auto source_type = source().grade(source_cell.grade).relation_types.at(source_cell.index);
        const auto target_type = target().grade(source_cell.grade).relation_types.at(mapped);
        return source().signature().type(source_type).name == target().signature().type(target_type).name;
    }

    [[nodiscard]] bool downward_star_bijection(core::CellId source_cell) const {
        if (source_cell.grade.value == 0) return true;
        const auto grade = source_cell.grade;
        const auto mapped_upper = image(source_cell);
        const auto& sa = source().grade(grade);
        const auto& ta = target().grade(grade);
        const auto exists = source().signature().existence_aspect();
        const auto [sb, se] = sa.incidence_range(source_cell.index);
        const auto [tb, te] = ta.incidence_range(mapped_upper);
        std::vector<std::size_t> source_slots;
        std::vector<std::size_t> target_slots;
        for (std::size_t s = sb; s < se; ++s) if (sa.aspects.get(exists, s)) source_slots.push_back(s);
        for (std::size_t t = tb; t < te; ++t) if (ta.aspects.get(exists, t)) target_slots.push_back(t);
        if (source_slots.size() != target_slots.size()) return false;
        std::vector<bool> used(target_slots.size(), false);
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(grade.value - 1u)};
        for (const auto s : source_slots) {
            const auto mapped_lower = morphism_.image(core::CellId{lower_grade, sa.lower_index[s]});
            bool found = false;
            for (std::size_t j = 0; j < target_slots.size(); ++j) {
                if (used[j]) continue;
                const auto t = target_slots[j];
                if (ta.lower_index[t] != mapped_lower) continue;
                if (!same_active_flag(grade, s, t)) continue;
                used[j] = true;
                found = true;
                break;
            }
            if (!found) return false;
        }
        return true;
    }

    [[nodiscard]] bool upward_star_bijection(core::CellId source_cell) const {
        if (static_cast<std::size_t>(source_cell.grade.value) + 1u >= source().grade_count()) return true;
        const auto upper_grade = core::Grade{static_cast<std::uint16_t>(source_cell.grade.value + 1u)};
        const auto mapped_lower = image(source_cell);
        const auto& sa = source().grade(upper_grade);
        const auto& ta = target().grade(upper_grade);
        const auto exists = source().signature().existence_aspect();
        std::vector<std::size_t> source_slots;
        std::vector<std::size_t> target_slots;
        for (std::size_t s = 0; s < sa.incidence_count(); ++s) {
            if (sa.lower_index[s] == source_cell.index && sa.aspects.get(exists, s)) source_slots.push_back(s);
        }
        for (std::size_t t = 0; t < ta.incidence_count(); ++t) {
            if (ta.lower_index[t] == mapped_lower && ta.aspects.get(exists, t)) target_slots.push_back(t);
        }
        if (source_slots.size() != target_slots.size()) return false;
        std::vector<bool> used(target_slots.size(), false);
        for (const auto s : source_slots) {
            const auto source_upper = core::CellId{upper_grade, sa.upper_index[s]};
            const auto mapped_upper = image(source_upper);
            bool found = false;
            for (std::size_t j = 0; j < target_slots.size(); ++j) {
                if (used[j]) continue;
                const auto t = target_slots[j];
                if (ta.upper_index[t] != mapped_upper) continue;
                if (!same_active_flag(upper_grade, s, t)) continue;
                used[j] = true;
                found = true;
                break;
            }
            if (!found) return false;
        }
        return true;
    }

    [[nodiscard]] bool local_star_bijection(core::CellId source_cell) const {
        if (!same_cell_type(source_cell)) return false;
        return downward_star_bijection(source_cell) && upward_star_bijection(source_cell);
    }

    algebra::RelationalMorphism morphism_;
};

} // namespace rfc::local
