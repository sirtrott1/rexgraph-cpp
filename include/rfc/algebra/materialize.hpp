#pragma once
#include "rfc/algebra/morphism.hpp"
#include "rfc/algebra/subcomplex.hpp"
#include "rfc/core/builder.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rfc::algebra {

struct MaterializedSubcomplex {
    core::RelationalComplex complex;
    std::vector<std::vector<std::uint32_t>> to_ambient;
    std::vector<std::vector<std::uint32_t>> from_ambient;
};

inline std::vector<core::AspectValue> declared_aspects_for_incidence(const core::RelationalComplex& ambient,
                                                                     core::Grade grade,
                                                                     std::size_t incidence) {
    std::vector<core::AspectValue> result;
    const auto& schema = ambient.signature().aspects();
    const auto& arena = ambient.grade(grade);
    for (std::size_t aspect_index = 0; aspect_index < schema.size(); ++aspect_index) {
        const auto aspect = static_cast<composite::AspectId>(aspect_index);
        const auto role = schema.at(aspect).role;
        if (role == composite::AspectRole::Existence ||
            role == composite::AspectRole::Orientation) continue;
        result.push_back({aspect, arena.aspects.get(aspect, incidence)});
    }
    return result;
}

inline MaterializedSubcomplex materialize_subcomplex(const core::RelationalComplex& ambient,
                                                       const SubcomplexMask& mask) {
    if (!mask.boundary_closed(ambient)) throw std::invalid_argument("materialization requires a boundary-closed subcomplex");
    constexpr auto absent = RelationalMorphism::absent;
    std::vector<std::vector<std::uint32_t>> from_ambient(ambient.grade_count());
    std::vector<std::vector<std::uint32_t>> to_ambient(ambient.grade_count());
    for (std::size_t k = 0; k < ambient.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        from_ambient[k].assign(ambient.grade(grade).cell_count(), absent);
        mask.grade(grade).for_each_set([&](std::size_t old_index) {
            const auto new_index = static_cast<std::uint32_t>(to_ambient[k].size());
            from_ambient[k][old_index] = new_index;
            to_ambient[k].push_back(static_cast<std::uint32_t>(old_index));
        });
    }

    core::RelationalComplexBuilder builder(ambient.signature(), ambient.semantic_scale());
    builder.declare_grade(core::Grade{static_cast<std::uint16_t>(ambient.grade_count() - 1u)});
    const auto& g0 = ambient.grade(core::Grade{0});
    for (const auto old_index : to_ambient[0]) builder.declare_witness_identity(g0.identities.at(old_index));

    const auto exists = ambient.signature().existence_aspect();
    const auto orientation = ambient.signature().orientation_aspect();
    for (std::size_t k = 1; k < ambient.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const auto& arena = ambient.grade(grade);
        for (const auto old_cell : to_ambient[k]) {
            const auto [begin, end] = arena.incidence_range(old_cell);
            if (k == 1) {
                std::vector<core::BoundaryOccurrenceInput> occurrences;
                occurrences.reserve(end - begin);
                for (std::size_t p = begin; p < end; ++p) {
                    const auto lower_old = arena.lower_index[p];
                    if (from_ambient[k - 1u][lower_old] == absent) throw std::logic_error("closed subcomplex lost a boundary identity");
                    const auto coefficient = ambient.incidence_coefficient(grade, p);
                    occurrences.push_back({g0.identities.at(lower_old), arena.aspects.get(exists, p), arena.aspects.get(orientation, p),
                                           coefficient < 0 ? -coefficient : coefficient,
                                           declared_aspects_for_incidence(ambient, grade, p),
                                           arena.occurrence_identities.at(p)});
                }
                builder.add_grade_one(arena.relation_identities.at(old_cell), arena.relation_types.at(old_cell), occurrences);
            } else {
                std::vector<core::HigherIncidenceInput> incidences;
                incidences.reserve(end - begin);
                for (std::size_t p = begin; p < end; ++p) {
                    const auto lower_old = arena.lower_index[p];
                    const auto lower_new = from_ambient[k - 1u][lower_old];
                    if (lower_new == absent) throw std::logic_error("closed subcomplex lost a lower relation");
                    const auto coefficient = ambient.incidence_coefficient(grade, p);
                    incidences.push_back({lower_new, arena.aspects.get(exists, p), arena.aspects.get(orientation, p),
                                          coefficient < 0 ? -coefficient : coefficient,
                                          declared_aspects_for_incidence(ambient, grade, p),
                                          arena.occurrence_identities.at(p)});
                }
                builder.add_higher(grade, arena.relation_identities.at(old_cell), arena.relation_types.at(old_cell), incidences);
            }
        }
    }

    return MaterializedSubcomplex{builder.freeze(), std::move(to_ambient), std::move(from_ambient)};
}

struct AmbientPushout {
    SubcomplexMask overlap;
    SubcomplexMask union_mask;
    MaterializedSubcomplex glued;
};

inline AmbientPushout pushout_inside(const core::RelationalComplex& ambient,
                                     const SubcomplexMask& left,
                                     const SubcomplexMask& right) {
    if (!left.boundary_closed(ambient) || !right.boundary_closed(ambient)) {
        throw std::invalid_argument("pushout inputs must be boundary-closed subcomplexes");
    }
    auto overlap = meet(left, right);
    auto union_mask = join(left, right);
    return AmbientPushout{overlap, union_mask, materialize_subcomplex(ambient, union_mask)};
}

} // namespace rfc::algebra
