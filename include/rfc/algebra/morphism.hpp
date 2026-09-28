#pragma once
#include "rfc/core/relational_complex.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rfc::algebra {

// Structural morphism of retained relational complexes.  This is not merely a
// chain-map between coordinate spaces: it preserves relation types, declared
// aspect semantics, lower-identity attachment, and the full occurrence fiber.
// The induced exact linear chain map is therefore a certificate/consequence of
// this stronger source-level mapping.
class RelationalMorphism {
public:
    static constexpr std::uint32_t absent = std::numeric_limits<std::uint32_t>::max();

    RelationalMorphism(const core::RelationalComplex& source,
                       const core::RelationalComplex& target,
                       std::vector<std::vector<std::uint32_t>> cell_maps)
        : source_(&source), target_(&target), cell_maps_(std::move(cell_maps)) {
        validate_cell_maps();
        validate_signature();
        build_occurrence_maps();
    }

    [[nodiscard]] const core::RelationalComplex& source() const noexcept { return *source_; }
    [[nodiscard]] const core::RelationalComplex& target() const noexcept { return *target_; }
    [[nodiscard]] bool is_strict() const noexcept { return true; }

    [[nodiscard]] std::uint32_t image(core::CellId source_cell) const {
        return cell_maps_.at(source_cell.grade.value).at(source_cell.index);
    }

    [[nodiscard]] std::size_t occurrence_image(core::Grade grade, std::size_t source_slot) const {
        if (grade.value == 0 || grade.value >= occurrence_maps_.size()) {
            throw std::out_of_range("morphism occurrence grade");
        }
        return occurrence_maps_[grade.value].at(source_slot);
    }

    [[nodiscard]] const std::vector<std::vector<std::uint32_t>>& cell_maps() const noexcept { return cell_maps_; }
    [[nodiscard]] const std::vector<std::vector<std::size_t>>& occurrence_maps() const noexcept { return occurrence_maps_; }

    [[nodiscard]] numeric::RationalVector push_field(core::Grade grade,
                                                      const numeric::RationalVector& source_field) const {
        if (source_field.size() != source_->grade(grade).cell_count()) throw std::invalid_argument("morphism field size mismatch");
        numeric::RationalVector result(target_->grade(grade).cell_count(), numeric::Rational{0});
        for (std::size_t source_cell = 0; source_cell < source_field.size(); ++source_cell) {
            const auto target_cell = cell_maps_[grade.value][source_cell];
            result[target_cell] += source_field[source_cell];
        }
        return result;
    }

    // A strict structural morphism preserves each exact boundary occurrence,
    // so its coordinate realization commutes with boundary without needing a
    // dense basis sweep.  Keep this predicate for callers that explicitly need
    // the chain-map certificate.
    [[nodiscard]] bool is_chain_map() const noexcept { return true; }

private:
    [[nodiscard]] static bool same_rule(const core::RelationRule& a,
                                        const core::RelationRule& b) {
        return a.kind == b.kind && a.aspect == b.aspect && a.count == b.count && a.value == b.value;
    }

    [[nodiscard]] static bool same_constraints(const core::RelationConstraints& a,
                                               const core::RelationConstraints& b) {
        if (a.shape != b.shape || a.distinguished_role != b.distinguished_role || a.rules.size() != b.rules.size()) return false;
        for (std::size_t i = 0; i < a.rules.size(); ++i) if (!same_rule(a.rules[i], b.rules[i])) return false;
        return true;
    }

    [[nodiscard]] static bool same_type(const core::RelationType& a,
                                        const core::RelationType& b) {
        return a.name == b.name &&
               a.boundary_policy == b.boundary_policy &&
               a.min_arity == b.min_arity &&
               a.max_arity == b.max_arity &&
               a.constructor == b.constructor &&
               same_constraints(a.constraints, b.constraints);
    }

    void validate_cell_maps() const {
        if (cell_maps_.size() != source_->grade_count()) throw std::invalid_argument("morphism source grade mismatch");
        if (target_->grade_count() < source_->grade_count()) throw std::invalid_argument("morphism target has insufficient grades");
        for (std::size_t k = 0; k < cell_maps_.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            if (cell_maps_[k].size() != source_->grade(grade).cell_count()) throw std::invalid_argument("morphism source cell-count mismatch");
            for (const auto mapped : cell_maps_[k]) {
                if (mapped == absent) throw std::invalid_argument("strict relational morphism cannot omit a retained source cell");
                if (mapped >= target_->grade(grade).cell_count()) throw std::out_of_range("morphism target cell index");
            }
        }
    }

    void validate_signature() const {
        const auto& a = source_->signature();
        const auto& b = target_->signature();
        if (a.aspects().size() != b.aspects().size()) throw std::invalid_argument("strict morphism aspect schema mismatch");
        for (std::size_t i = 0; i < a.aspects().size(); ++i) {
            const auto id = static_cast<composite::AspectId>(i);
            const auto& da = a.aspects().at(id);
            const auto& db = b.aspects().at(id);
            if (da.name != db.name || da.role != db.role || da.default_value != db.default_value) {
                throw std::invalid_argument("strict morphism must preserve declared aspect semantics");
            }
        }
        if (a.type_count() != b.type_count()) throw std::invalid_argument("strict morphism relation signature mismatch");
        for (std::size_t i = 0; i < a.type_count(); ++i) {
            if (!same_type(a.type(static_cast<core::RelationTypeId>(i)),
                           b.type(static_cast<core::RelationTypeId>(i)))) {
                throw std::invalid_argument("strict morphism must preserve relation type semantics");
            }
        }
    }

    [[nodiscard]] bool same_occurrence_semantics(core::Grade grade,
                                                 std::size_t source_slot,
                                                 std::size_t target_slot) const {
        const auto& source_arena = source_->grade(grade);
        const auto& target_arena = target_->grade(grade);
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(grade.value - 1u)};
        if (image(core::CellId{lower_grade, source_arena.lower_index[source_slot]}) != target_arena.lower_index[target_slot]) return false;
        if (source_->incidence_coefficient(grade, source_slot) != target_->incidence_coefficient(grade, target_slot)) return false;
        for (std::size_t a = 0; a < source_->signature().aspects().size(); ++a) {
            const auto aspect = static_cast<composite::AspectId>(a);
            if (source_arena.aspects.get(aspect, source_slot) != target_arena.aspects.get(aspect, target_slot)) return false;
        }
        return true;
    }

    void build_occurrence_maps() {
        occurrence_maps_.resize(source_->grade_count());
        for (std::size_t k = 1; k < source_->grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& source_arena = source_->grade(grade);
            const auto& target_arena = target_->grade(grade);
            auto& occurrence_map = occurrence_maps_[k];
            occurrence_map.assign(source_arena.incidence_count(), std::numeric_limits<std::size_t>::max());

            for (std::size_t source_cell = 0; source_cell < source_arena.cell_count(); ++source_cell) {
                const auto target_cell = cell_maps_[k][source_cell];
                if (!same_type(source_->signature().type(source_arena.relation_types[source_cell]),
                               target_->signature().type(target_arena.relation_types[target_cell]))) {
                    throw std::invalid_argument("strict morphism cell type mismatch");
                }

                const auto [sb, se] = source_arena.incidence_range(source_cell);
                const auto [tb, te] = target_arena.incidence_range(target_cell);
                if (se - sb != te - tb) throw std::invalid_argument("strict morphism boundary occurrence-count mismatch");
                if (source_->signature().type(source_arena.relation_types[source_cell]).is_attaching_loop()) {
                    for (std::size_t s = sb; s < se; ++s) {
                        const auto t = tb + (s - sb);
                        if (!same_occurrence_semantics(grade, s, t))
                            throw std::invalid_argument("strict morphism changes declared attaching order");
                        occurrence_map[s] = t;
                    }
                    continue;
                }
                std::vector<bool> used(te - tb, false);
                for (std::size_t s = sb; s < se; ++s) {
                    bool found = false;
                    for (std::size_t t = tb; t < te; ++t) {
                        const auto local = t - tb;
                        if (used[local] || !same_occurrence_semantics(grade, s, t)) continue;
                        occurrence_map[s] = t;
                        used[local] = true;
                        found = true;
                        break;
                    }
                    if (!found) throw std::invalid_argument("strict morphism does not preserve boundary occurrence attachment");
                }
            }
        }
    }

    const core::RelationalComplex* source_{};
    const core::RelationalComplex* target_{};
    std::vector<std::vector<std::uint32_t>> cell_maps_;
    std::vector<std::vector<std::size_t>> occurrence_maps_;
};

} // namespace rfc::algebra
