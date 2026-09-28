#pragma once
#include "rfc/machine/packed_bits.hpp"
#include <stdexcept>
#include <vector>

namespace rfc::runtime {

inline machine::PackedBits intersect_masks(const std::vector<const machine::PackedBits*>& masks) {
    if (masks.empty()) throw std::invalid_argument("at least one query mask is required");
    auto result = *masks.front();
    for (std::size_t i = 1; i < masks.size(); ++i) result = result & *masks[i];
    return result;
}

} // namespace rfc::runtime
