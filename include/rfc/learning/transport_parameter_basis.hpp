#pragma once
#include "rfc/numeric/prepared_exact_frames.hpp"
#include <map>
#include <optional>

namespace rfc::learning {
struct TransportParameterBasisRank {
    std::size_t rank{}, workspace_entries{};
};

namespace transport_basis_detail {
// One exclusive nonzero row per active column gives a nonsingular diagonal minor.
inline std::optional<TransportParameterBasisRank> exclusive_rows(const numeric::ExactMatrix& basis,
    numeric::ExactActionGuard& guard,std::size_t resident=0) {
    std::map<std::size_t,bool> columns;
    std::optional<std::size_t> row,sole;
    const auto finish=[&](){if(sole)columns.at(*sole)=true;};
    for(const auto& [key,value]:basis.entries()) {
        (void)value;guard.step();
        if(!row || *row!=key.first){finish();row=key.first;sole=key.second;}
        else sole.reset();
        columns.try_emplace(key.second,false);
        guard.entries(machine::checked_size_add(resident,machine::checked_size_add(basis.nonzeros(),
            machine::checked_size_mul(columns.size(),2))));
    }
    finish();
    for(const auto& [column,witness]:columns){
        (void)column;guard.step();if(!witness)return std::nullopt;
    }
    return TransportParameterBasisRank{columns.size(),machine::checked_size_mul(columns.size(),2)};
}
} // namespace transport_basis_detail

// Only certified exclusive rows bypass the complete exact reduction.
inline TransportParameterBasisRank transport_parameter_basis_rank(const numeric::ExactMatrix& basis,
    numeric::ExactActionGuard& guard) {
    guard.matrix(basis);
    if(const auto certificate=transport_basis_detail::exclusive_rows(basis,guard))return *certificate;
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
    if(transport_basis_detail::exclusive_rows(compact,guard,add(resident,add(map.nonzeros(),columns.size()))))
        return compact;
    const numeric::PreparedExactFrames frames(std::move(compact),guard);
    guard.entries(add(resident,add(map.nonzeros(),add(columns.size(),
        add(frames.stored_entries(),frames.image().nonzeros())))));
    return frames.image();
}
} // namespace rfc::learning
