#pragma once
#include "rfc/geometry/semantic_scale.hpp"
#include "rfc/local/constraint_system.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rfc::local {

// Set-valued semantic transport is defined here only for a strict structural
// SemanticScaleMap.  A general linear SemanticScaleRealization may send one
// cell to a chain of cells and therefore does not define a nonlinear value
// transfer without an additional declared policy.
template <class Value>
[[nodiscard]] BasicSection<Value> push_assignment(const BasicSection<Value>& source,
                                                  const geometry::SemanticScaleMap& scale_map) {
    BasicSection<Value> target;
    const auto& morphism = scale_map.realization();
    for (const auto& [cell, value] : source) {
        if (cell.grade.value >= morphism.source().grade_count() || cell.index >= morphism.source().grade(cell.grade).cell_count()) {
            throw std::out_of_range("semantic source assignment cell");
        }
        const core::CellId mapped{cell.grade, morphism.image(cell)};
        const auto [it, inserted] = target.emplace(mapped, value);
        if (!inserted && !(it->second == value)) {
            throw std::invalid_argument("semantic pushforward identifies incompatible local values");
        }
    }
    return target;
}

template <class Value>
[[nodiscard]] BasicSection<Value> pull_assignment(const BasicSection<Value>& target,
                                                  const geometry::SemanticScaleMap& scale_map) {
    BasicSection<Value> source;
    const auto& morphism = scale_map.realization();
    for (std::size_t k = 0; k < morphism.source().grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        for (std::size_t i = 0; i < morphism.source().grade(grade).cell_count(); ++i) {
            const core::CellId source_cell{grade, static_cast<std::uint32_t>(i)};
            const core::CellId target_cell{grade, morphism.image(source_cell)};
            const auto it = target.find(target_cell);
            if (it != target.end()) source.emplace(source_cell, it->second);
        }
    }
    return source;
}

namespace detail {
inline algebra::SubcomplexMask push_patch(const algebra::SubcomplexMask& source_patch,
                                          const algebra::RelationalMorphism& morphism) {
    if (!source_patch.boundary_closed(morphism.source())) throw std::invalid_argument("semantic source patch must be boundary-closed");
    algebra::SubcomplexMask target_patch(morphism.target());
    for (std::size_t k = 0; k < source_patch.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        source_patch.grade(grade).for_each_set([&](std::size_t index) {
            target_patch.select(core::CellId{grade, morphism.image(core::CellId{grade, static_cast<std::uint32_t>(index)})});
        });
    }
    if (!target_patch.boundary_closed(morphism.target())) {
        throw std::logic_error("strict semantic image of a closed patch was not closed");
    }
    return target_patch;
}

inline std::vector<std::vector<std::uint32_t>> inverse_cell_map(const algebra::RelationalMorphism& morphism) {
    constexpr auto absent = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::vector<std::uint32_t>> inverse;
    inverse.reserve(morphism.source().grade_count());
    for (std::size_t k = 0; k < morphism.source().grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        std::vector<std::uint32_t> map(morphism.target().grade(grade).cell_count(), absent);
        for (std::size_t i = 0; i < morphism.source().grade(grade).cell_count(); ++i) {
            const auto out = morphism.image(core::CellId{grade, static_cast<std::uint32_t>(i)});
            if (map[out] != absent) throw std::invalid_argument("semantic constraint transport requires an injective cell map on the transported carrier");
            map[out] = static_cast<std::uint32_t>(i);
        }
        inverse.push_back(std::move(map));
    }
    return inverse;
}

inline algebra::SubcomplexMask pull_patch_bijective(const algebra::SubcomplexMask& target_patch,
                                                     const algebra::RelationalMorphism& morphism,
                                                     const std::vector<std::vector<std::uint32_t>>& inverse) {
    constexpr auto absent = std::numeric_limits<std::uint32_t>::max();
    if (!target_patch.boundary_closed(morphism.target())) throw std::invalid_argument("semantic target patch must be boundary-closed");
    algebra::SubcomplexMask source_patch(morphism.source());
    for (std::size_t k = 0; k < target_patch.grade_count(); ++k) {
        if (k >= inverse.size()) {
            bool any = false;
            target_patch.grade(core::Grade{static_cast<std::uint16_t>(k)}).for_each_set([&](std::size_t) { any = true; });
            if (any) throw std::invalid_argument("semantic target patch contains grades outside the source image");
            continue;
        }
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        target_patch.grade(grade).for_each_set([&](std::size_t index) {
            if (index >= inverse[k].size() || inverse[k][index] == absent) {
                throw std::invalid_argument("semantic target patch is not contained in the strict source image");
            }
            source_patch.select(core::CellId{grade, inverse[k][index]});
        });
    }
    if (!source_patch.boundary_closed(morphism.source())) throw std::logic_error("semantic pullback patch was not boundary-closed");
    return source_patch;
}
} // namespace detail

template <class Value>
[[nodiscard]] FiniteConstraintSystem<Value> pushforward_constraints_bijective(
    const FiniteConstraintSystem<Value>& source,
    const geometry::SemanticScaleMap& scale_map) {
    const auto& morphism = scale_map.realization();
    if (&source.complex() != &morphism.source()) throw std::invalid_argument("constraint system does not live on semantic source");
    (void)detail::inverse_cell_map(morphism); // certify injectivity before constructing partial output
    FiniteConstraintSystem<Value> target(morphism.target());
    for (const auto& [cell, domain] : source.domains()) {
        target.set_domain(core::CellId{cell.grade, morphism.image(cell)}, domain);
    }
    for (const auto& chart : source.charts()) {
        auto patch = detail::push_patch(chart.patch, morphism);
        std::vector<core::CellId> support;
        support.reserve(chart.support.size());
        for (const auto cell : chart.support) support.push_back(core::CellId{cell.grade, morphism.image(cell)});
        target.add_chart(chart.name, std::move(patch), std::move(support), chart.allowed);
    }
    return target;
}

template <class Value>
[[nodiscard]] FiniteConstraintSystem<Value> pullback_constraints_bijective(
    const FiniteConstraintSystem<Value>& target,
    const geometry::SemanticScaleMap& scale_map) {
    const auto& morphism = scale_map.realization();
    if (&target.complex() != &morphism.target()) throw std::invalid_argument("constraint system does not live on semantic target");
    const auto inverse = detail::inverse_cell_map(morphism);
    constexpr auto absent = std::numeric_limits<std::uint32_t>::max();
    FiniteConstraintSystem<Value> source(morphism.source());
    for (const auto& [cell, domain] : target.domains()) {
        if (cell.grade.value >= inverse.size() || cell.index >= inverse[cell.grade.value].size() ||
            inverse[cell.grade.value][cell.index] == absent) {
            throw std::invalid_argument("constraint domain lies outside strict semantic image");
        }
        source.set_domain(core::CellId{cell.grade, inverse[cell.grade.value][cell.index]}, domain);
    }
    for (const auto& chart : target.charts()) {
        auto patch = detail::pull_patch_bijective(chart.patch, morphism, inverse);
        std::vector<core::CellId> support;
        support.reserve(chart.support.size());
        for (const auto cell : chart.support) {
            if (cell.grade.value >= inverse.size() || cell.index >= inverse[cell.grade.value].size() ||
                inverse[cell.grade.value][cell.index] == absent) {
                throw std::invalid_argument("constraint chart support lies outside strict semantic image");
            }
            support.push_back(core::CellId{cell.grade, inverse[cell.grade.value][cell.index]});
        }
        source.add_chart(chart.name, std::move(patch), std::move(support), chart.allowed);
    }
    return source;
}

} // namespace rfc::local
