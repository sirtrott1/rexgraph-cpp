#pragma once
#include "rfc/local/covering.hpp"
#include "rfc/topology/path_groupoid.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::topology {

struct LiftedPath {
    PairwisePath path;
    std::uint32_t endpoint{};
};

inline LiftedPath lift_path(const local::StrictRelationalCovering& cover,
                            const PairwisePath& base_path,
                            std::uint32_t source_start) {
    if (&base_path.complex() != &cover.target()) throw std::invalid_argument("base path does not belong to covering target");
    const core::CellId start_cell{core::Grade{0}, source_start};
    if (cover.image(start_cell) != base_path.source()) throw std::invalid_argument("lift start is not over base path source");

    std::uint32_t current = source_start;
    std::vector<EdgeStep> lifted_steps;
    lifted_steps.reserve(base_path.steps().size());

    for (const auto& base_step : base_path.steps()) {
        const auto source_relations = cover.fiber(core::CellId{core::Grade{1}, base_step.relation});
        std::size_t matches = 0;
        EdgeStep chosen{};
        std::uint32_t next = current;
        for (const auto source_relation : source_relations) {
            const auto endpoints = pairwise_endpoints(cover.source(), source_relation.index);
            const auto from = base_step.forward ? endpoints.tail : endpoints.head;
            const auto to = base_step.forward ? endpoints.head : endpoints.tail;
            if (from != current) continue;
            ++matches;
            chosen = EdgeStep{source_relation.index, base_step.forward};
            next = to;
        }
        if (matches != 1u) throw std::logic_error("strict covering did not provide a unique path lift");
        lifted_steps.push_back(chosen);
        current = next;
    }

    return LiftedPath{PairwisePath(cover.source(), source_start, std::move(lifted_steps)), current};
}

struct MonodromyPermutation {
    std::vector<core::CellId> fiber;
    std::vector<std::size_t> image;
};

inline MonodromyPermutation monodromy(const local::StrictRelationalCovering& cover,
                                      const PairwisePath& base_loop) {
    if (!base_loop.is_loop()) throw std::invalid_argument("monodromy requires a base loop");
    const auto fiber = cover.fiber(core::CellId{core::Grade{0}, base_loop.source()});
    if (fiber.empty()) throw std::logic_error("covering fiber is empty");
    std::unordered_map<std::uint32_t, std::size_t> position;
    for (std::size_t i = 0; i < fiber.size(); ++i) position.emplace(fiber[i].index, i);

    std::vector<std::size_t> image(fiber.size(), 0u);
    std::vector<bool> hit(fiber.size(), false);
    for (std::size_t i = 0; i < fiber.size(); ++i) {
        const auto lifted = lift_path(cover, base_loop, fiber[i].index);
        const auto it = position.find(lifted.endpoint);
        if (it == position.end()) throw std::logic_error("lifted loop left the covering fiber");
        image[i] = it->second;
        if (hit[it->second]) throw std::logic_error("monodromy lift is not a permutation");
        hit[it->second] = true;
    }
    return MonodromyPermutation{fiber, image};
}

inline bool is_deck_transformation(const local::StrictRelationalCovering& cover,
                                   std::vector<std::vector<std::uint32_t>> cell_maps) {
    const auto& source = cover.source();
    if (cell_maps.size() != source.grade_count()) return false;
    std::vector<std::vector<std::uint32_t>> inverse(cell_maps.size());
    for (std::size_t k = 0; k < cell_maps.size(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        if (cell_maps[k].size() != source.grade(grade).cell_count()) return false;
        inverse[k].assign(cell_maps[k].size(), algebra::RelationalMorphism::absent);
        for (std::size_t i = 0; i < cell_maps[k].size(); ++i) {
            const auto mapped = cell_maps[k][i];
            if (mapped >= cell_maps[k].size() || inverse[k][mapped] != algebra::RelationalMorphism::absent) return false;
            inverse[k][mapped] = static_cast<std::uint32_t>(i);
        }
    }

    algebra::RelationalMorphism deck(source, source, cell_maps);
    algebra::RelationalMorphism deck_inverse(source, source, inverse);
    if (!deck.is_chain_map() || !deck_inverse.is_chain_map()) return false;
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        for (std::size_t i = 0; i < source.grade(grade).cell_count(); ++i) {
            const core::CellId cell{grade, static_cast<std::uint32_t>(i)};
            const core::CellId moved{grade, deck.image(cell)};
            if (cover.image(cell) != cover.image(moved)) return false;
        }
    }
    return true;
}

} // namespace rfc::topology
