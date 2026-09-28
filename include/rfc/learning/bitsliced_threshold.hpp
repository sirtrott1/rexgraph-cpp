#pragma once
#include "rfc/machine/bitsliced_int.hpp"
#include "rfc/machine/packed_bits.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

// Integer threshold unit evaluated across all batch lanes by bit-sliced Boolean
// arithmetic. Features are binary PackedBits; weights and threshold are exact
// signed machine integers. Width must be chosen so the full score cannot overflow.
template <std::size_t Bits>
class BitSlicedThresholdUnit {
public:
    BitSlicedThresholdUnit(std::vector<std::int32_t> weights, std::int64_t threshold)
        : weights_(std::move(weights)), threshold_(threshold) {
        if (threshold_ < machine::BitSlicedInt<Bits>::min_value() ||
            threshold_ > machine::BitSlicedInt<Bits>::max_value()) {
            throw std::out_of_range("threshold outside bit-sliced width");
        }
        std::int64_t bound = 0;
        for (const auto weight : weights_) {
            const auto magnitude = weight < 0 ? -static_cast<std::int64_t>(weight) : static_cast<std::int64_t>(weight);
            if (bound > machine::BitSlicedInt<Bits>::max_value() - magnitude) throw std::out_of_range("threshold unit score may overflow bit-sliced width");
            bound += magnitude;
        }
        if (-bound < machine::BitSlicedInt<Bits>::min_value() || bound > machine::BitSlicedInt<Bits>::max_value()) {
            throw std::out_of_range("threshold unit score bound exceeds bit-sliced width");
        }
    }

    [[nodiscard]] const std::vector<std::int32_t>& weights() const noexcept { return weights_; }
    [[nodiscard]] std::int64_t threshold() const noexcept { return threshold_; }

    [[nodiscard]] machine::PackedBits forward(const std::vector<machine::PackedBits>& features) const {
        if (features.size() != weights_.size()) throw std::invalid_argument("threshold feature count mismatch");
        if (features.empty()) return machine::PackedBits{};
        const auto lanes = features.front().size();
        for (const auto& feature : features) if (feature.size() != lanes) throw std::invalid_argument("threshold feature lane mismatch");
        machine::BitSlicedInt<Bits> score(lanes);
        for (std::size_t i = 0; i < features.size(); ++i) {
            if (weights_[i] == 0) continue;
            score = score.add_mod(machine::BitSlicedInt<Bits>::masked_constant(features[i], weights_[i]));
        }
        return score.greater_equal(threshold_);
    }

private:
    std::vector<std::int32_t> weights_;
    std::int64_t threshold_{};
};

struct ThresholdFitResult {
    std::vector<std::int32_t> weights;
    std::int64_t threshold{};
    std::size_t hamming_error{};
};

// Exhaustive exact fit intended for small reference units and verification.
// Inference remains word-parallel even though the tiny parameter search is scalar.
template <std::size_t Bits>
inline ThresholdFitResult fit_small_threshold_unit(const std::vector<machine::PackedBits>& features,
                                                   const machine::PackedBits& target,
                                                   std::int32_t weight_min,
                                                   std::int32_t weight_max,
                                                   std::int64_t threshold_min,
                                                   std::int64_t threshold_max) {
    if (features.empty()) throw std::invalid_argument("threshold fit requires features");
    for (const auto& feature : features) if (feature.size() != target.size()) throw std::invalid_argument("threshold fit lane mismatch");
    if (weight_min > weight_max || threshold_min > threshold_max) throw std::invalid_argument("invalid threshold search range");

    const std::int64_t base = static_cast<std::int64_t>(weight_max) - static_cast<std::int64_t>(weight_min) + 1;
    std::uint64_t combinations = 1;
    for (std::size_t i = 0; i < features.size(); ++i) {
        if (combinations > std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(base)) {
            throw std::overflow_error("threshold search space too large");
        }
        combinations *= static_cast<std::uint64_t>(base);
    }

    ThresholdFitResult best;
    best.hamming_error = std::numeric_limits<std::size_t>::max();
    std::vector<std::int32_t> weights(features.size(), weight_min);
    for (std::uint64_t code = 0; code < combinations; ++code) {
        auto cursor = code;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            weights[i] = static_cast<std::int32_t>(static_cast<std::int64_t>(weight_min) + static_cast<std::int64_t>(cursor % static_cast<std::uint64_t>(base)));
            cursor /= static_cast<std::uint64_t>(base);
        }
        for (std::int64_t threshold = threshold_min; threshold <= threshold_max; ++threshold) {
            BitSlicedThresholdUnit<Bits> unit(weights, threshold);
            const auto prediction = unit.forward(features);
            const auto error = (prediction ^ target).count();
            if (error < best.hamming_error) {
                best = ThresholdFitResult{weights, threshold, error};
                if (error == 0) return best;
            }
            if (threshold == threshold_max) break; // avoid signed overflow at max
        }
    }
    return best;
}

} // namespace rfc::learning
