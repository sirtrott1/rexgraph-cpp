#pragma once
#include "rfc/machine/bitsliced_int.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

// Multi-bit integer affine unit evaluated bit-sliced across batch lanes.
// No dense matrix is required: this is one sparse unit with explicit integer
// feature weights. Arithmetic is exact modulo the chosen machine width; the
// caller chooses Bits large enough to contain the desired mathematical range.
template <std::size_t Bits>
class BitSlicedAffineUnit {
public:
    BitSlicedAffineUnit(std::vector<std::int64_t> weights, std::int64_t bias = 0)
        : weights_(std::move(weights)), bias_(bias) {
        if (bias_ < machine::BitSlicedInt<Bits>::min_value() || bias_ > machine::BitSlicedInt<Bits>::max_value()) {
            throw std::out_of_range("bit-sliced affine bias outside width");
        }
        for (const auto weight : weights_) {
            if (weight < machine::BitSlicedInt<Bits>::min_value() || weight > machine::BitSlicedInt<Bits>::max_value()) {
                throw std::out_of_range("bit-sliced affine weight outside width");
            }
        }
    }

    [[nodiscard]] machine::BitSlicedInt<Bits> forward(
        const std::vector<machine::BitSlicedInt<Bits>>& features) const {
        if (features.size() != weights_.size()) throw std::invalid_argument("bit-sliced affine feature count mismatch");
        if (features.empty()) return {};
        const auto lanes = features.front().size();
        for (const auto& feature : features) if (feature.size() != lanes) throw std::invalid_argument("bit-sliced affine lane mismatch");
        auto result = machine::BitSlicedInt<Bits>::constant(lanes, bias_);
        for (std::size_t i = 0; i < features.size(); ++i) {
            if (weights_[i] == 0) continue;
            result = result.add_mod(features[i].mul_constant_mod(weights_[i]));
        }
        return result;
    }

    [[nodiscard]] machine::PackedBits threshold(
        const std::vector<machine::BitSlicedInt<Bits>>& features,
        std::int64_t value) const {
        return forward(features).greater_equal(value);
    }

private:
    std::vector<std::int64_t> weights_;
    std::int64_t bias_{};
};

} // namespace rfc::learning
