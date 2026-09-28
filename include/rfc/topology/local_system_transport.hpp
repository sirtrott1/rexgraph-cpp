#pragma once
#include "rfc/local/cellular_sheaf.hpp"
#include "rfc/topology/path_groupoid.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>

namespace rfc::topology {

inline std::pair<std::size_t, std::size_t> pairwise_incidence_slots(const core::RelationalComplex& complex,
                                                                    std::size_t relation_index) {
    const auto endpoints = pairwise_endpoints(complex, relation_index);
    const auto& arena = complex.grade(core::Grade{1});
    const auto [begin, end] = arena.incidence_range(relation_index);
    std::size_t tail_slot = arena.incidence_count();
    std::size_t head_slot = arena.incidence_count();
    for (std::size_t slot = begin; slot < end; ++slot) {
        if (arena.lower_index[slot] == endpoints.tail) tail_slot = slot;
        if (arena.lower_index[slot] == endpoints.head) head_slot = slot;
    }
    if (tail_slot == arena.incidence_count() || head_slot == arena.incidence_count()) {
        throw std::logic_error("pairwise endpoint incidence slot missing");
    }
    return {tail_slot, head_slot};
}

// Parallel transport induced by an additive local system on the pairwise
// one-skeleton.  Compatibility on an edge requires rho_tail(x_tail) =
// rho_head(x_head), hence forward transport is rho_head^{-1} rho_tail.
inline numeric::ExactMatrix local_system_transport(const local::LinearCellularSheaf& sheaf,
                                                   const PairwisePath& path) {
    if (&path.complex() != &sheaf.complex()) throw std::invalid_argument("local-system path belongs to another complex");
    const core::CellId source{core::Grade{0}, path.source()};
    numeric::ExactMatrix total = numeric::ExactMatrix::identity(sheaf.stalks().dimension(source));
    std::uint32_t current = path.source();

    for (const auto& step : path.steps()) {
        const auto endpoints = pairwise_endpoints(sheaf.complex(), step.relation);
        const auto [tail_slot, head_slot] = pairwise_incidence_slots(sheaf.complex(), step.relation);
        const auto rho_tail = sheaf.restriction(core::Grade{1}, tail_slot);
        const auto rho_head = sheaf.restriction(core::Grade{1}, head_slot);
        numeric::ExactMatrix step_map;
        if (step.forward) {
            if (current != endpoints.tail) throw std::logic_error("local-system path endpoint drift");
            step_map = rho_head.inverse().multiply(rho_tail);
            current = endpoints.head;
        } else {
            if (current != endpoints.head) throw std::logic_error("local-system path endpoint drift");
            step_map = rho_tail.inverse().multiply(rho_head);
            current = endpoints.tail;
        }
        total = step_map.multiply(total);
    }
    return total;
}

} // namespace rfc::topology
