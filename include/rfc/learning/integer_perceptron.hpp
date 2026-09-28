#pragma once
#include "rfc/numeric/checked_int.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

struct SparseIntFeature {
    std::uint32_t index{};
    std::int64_t value{};
};

struct SparseIntSample {
    std::vector<SparseIntFeature> features;
    std::int8_t label{1}; // -1 or +1
};

class ExactIntegerPerceptron {
public:
    explicit ExactIntegerPerceptron(std::size_t feature_count)
        : weights_(feature_count, 0) {}

    [[nodiscard]] std::int64_t score(const std::vector<SparseIntFeature>& features) const {
        std::int64_t acc = bias_;
        for (const auto& feature : features) {
            if (feature.index >= weights_.size()) throw std::out_of_range("perceptron feature index");
            acc = numeric::checked_mac_i64(acc, weights_[feature.index], feature.value);
        }
        return acc;
    }
    [[nodiscard]] std::int8_t predict(const std::vector<SparseIntFeature>& features) const {
        return score(features) >= 0 ? std::int8_t{1} : std::int8_t{-1};
    }

    // Classical exact perceptron updates. No learning-rate approximation exists:
    // learning_rate is an integer and every update is checked integer arithmetic.
    std::size_t train_epoch(const std::vector<SparseIntSample>& samples, std::int64_t learning_rate = 1) {
        if (learning_rate <= 0) throw std::invalid_argument("integer perceptron learning rate must be positive");
        std::size_t mistakes = 0;
        for (const auto& sample : samples) {
            if (sample.label != 1 && sample.label != -1) throw std::invalid_argument("perceptron label must be -1 or +1");
            const auto signed_margin = numeric::checked_mul_i64(static_cast<std::int64_t>(sample.label), score(sample.features));
            if (signed_margin > 0) continue;
            ++mistakes;
            const auto step = numeric::checked_mul_i64(learning_rate, static_cast<std::int64_t>(sample.label));
            for (const auto& feature : sample.features) {
                if (feature.index >= weights_.size()) throw std::out_of_range("perceptron feature index");
                weights_[feature.index] = numeric::checked_mac_i64(weights_[feature.index], step, feature.value);
            }
            bias_ = numeric::checked_add_i64(bias_, step);
        }
        return mistakes;
    }

    [[nodiscard]] const std::vector<std::int64_t>& weights() const noexcept { return weights_; }
    [[nodiscard]] std::int64_t bias() const noexcept { return bias_; }

private:
    std::vector<std::int64_t> weights_;
    std::int64_t bias_{0};
};

} // namespace rfc::learning
