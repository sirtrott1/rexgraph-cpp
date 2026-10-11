#pragma once
#include "rfc/numeric/prepared_exact_frames.hpp"
#include <map>
#include <optional>
#include <set>

namespace rfc::learning {
struct TransportParameterBasisRank {
    std::size_t rank{}, workspace_entries{};
};

// Disjoint nonzero column supports have one independent direction per column.
inline TransportParameterBasisRank transport_parameter_basis_rank(const numeric::ExactMatrix& basis,
    numeric::ExactActionGuard& guard) {
    guard.matrix(basis);
    {
        std::set<std::size_t> columns;
        std::optional<std::size_t> previous;
        bool disjoint=true;
        for(const auto& [key,value]:basis.entries()) {
            (void)value;guard.step();
            if(previous && *previous==key.first){disjoint=false;break;}
            previous=key.first;columns.insert(key.second);
            guard.entries(machine::checked_size_add(basis.nonzeros(),columns.size()));
        }
        if(disjoint)return {columns.size(),columns.size()};
    }
    const numeric::PreparedExactFrames frames(basis,guard);
    return {frames.rank(),frames.stored_entries()};
}

// Zero columns do not contribute to the observable parameter image.
inline numeric::ExactMatrix transport_observable_image(const numeric::ExactMatrix& map,
    numeric::ExactActionGuard& guard,std::size_t resident=0) {
    guard.matrix(map);
    std::map<std::size_t,std::size_t> columns;
    const auto add=machine::checked_size_add;
    for(const auto& [key,value]:map.entries()) {
        (void)value;guard.step();columns.try_emplace(key.second,0);
        guard.entries(add(resident,add(map.nonzeros(),columns.size())));
    }
    std::size_t next{};
    for(auto& [column,axis]:columns) {(void)column;guard.step();axis=next++;}
    numeric::ExactMatrix compact(map.rows(),columns.size());
    for(const auto& [key,value]:map.entries()) {
        guard.step();compact.set(key.first,columns.at(key.second),value);
        guard.entries(add(resident,add(map.nonzeros(),add(columns.size(),compact.nonzeros()))));
    }
    const numeric::PreparedExactFrames frames(std::move(compact),guard);
    guard.entries(add(resident,add(map.nonzeros(),add(columns.size(),
        add(frames.stored_entries(),frames.image().nonzeros())))));
    return frames.image();
}
} // namespace rfc::learning
