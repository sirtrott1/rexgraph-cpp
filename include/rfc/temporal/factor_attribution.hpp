#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::temporal {

struct OrderedFactorInteraction {
    std::vector<std::size_t> changed_positions;
    numeric::ExactMatrix contribution;
};

enum class FactorOrder : unsigned char { AppliedFirst, WrittenProduct };

inline numeric::ExactMatrix ordered_product(const std::vector<numeric::ExactMatrix>& factors,
                                           FactorOrder order = FactorOrder::AppliedFirst) {
    if(factors.empty()) throw std::invalid_argument("ordered product requires at least one factor");
    if(order != FactorOrder::AppliedFirst && order != FactorOrder::WrittenProduct)
        throw std::invalid_argument("unknown factor order");
    numeric::ExactMatrix result=factors.front();
    for(std::size_t i=1;i<factors.size();++i)
        result=order == FactorOrder::AppliedFirst ? factors[i].multiply(result) : result.multiply(factors[i]);
    return result;
}

// Exact finite product attribution. AppliedFirst puts position 0 on the right,
// matching chronological operation words O_m ... O_1. WrittenProduct retains
// the displayed left-to-right product O_1 ... O_m.
inline std::vector<OrderedFactorInteraction> finite_ordered_factor_attribution(
    const std::vector<numeric::ExactMatrix>& old_factors,
    const std::vector<numeric::ExactMatrix>& new_factors,
    FactorOrder order = FactorOrder::AppliedFirst) {
    if(old_factors.empty() || old_factors.size()!=new_factors.size()) {
        throw std::invalid_argument("finite factor attribution size mismatch");
    }
    const std::size_t m=old_factors.size();
    if(m>=63u) throw std::invalid_argument("finite factor attribution supports fewer than 63 factors");
    for(std::size_t i=0;i<m;++i){
        if(old_factors[i].rows()!=new_factors[i].rows() || old_factors[i].cols()!=new_factors[i].cols()) {
            throw std::invalid_argument("finite factor attribution aligned carrier mismatch");
        }
    }
    std::vector<numeric::ExactMatrix> delta;
    delta.reserve(m);
    for(std::size_t i=0;i<m;++i) delta.push_back(new_factors[i].add(old_factors[i].scale(numeric::Rational{-1})));

    std::vector<OrderedFactorInteraction> out;
    const std::uint64_t limit=(std::uint64_t{1}<<m);
    out.reserve(static_cast<std::size_t>(limit-1u));
    for(std::uint64_t mask=1;mask<limit;++mask){
        std::vector<numeric::ExactMatrix> selected;
        std::vector<std::size_t> positions;
        selected.reserve(m);
        for(std::size_t i=0;i<m;++i){
            if((mask&(std::uint64_t{1}<<i))!=0){ selected.push_back(delta[i]); positions.push_back(i); }
            else selected.push_back(old_factors[i]);
        }
        out.push_back({std::move(positions),ordered_product(selected,order)});
    }
    return out;
}

inline numeric::ExactMatrix sum_factor_interactions(const std::vector<OrderedFactorInteraction>& interactions,
                                                     std::size_t rows,
                                                     std::size_t cols) {
    numeric::ExactMatrix out(rows,cols);
    for(const auto& term:interactions) out=out.add(term.contribution);
    return out;
}

} // namespace rfc::temporal
