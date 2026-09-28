#pragma once
#include "rfc/learning/operator_basis.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

enum class IntegerActivation : std::uint8_t {
    Identity,
    ReLU,
    Sign
};

inline std::int64_t apply_integer_activation(IntegerActivation activation, std::int64_t value) noexcept {
    switch (activation) {
        case IntegerActivation::Identity: return value;
        case IntegerActivation::ReLU: return value > 0 ? value : 0;
        case IntegerActivation::Sign: return value >= 0 ? 1 : -1;
    }
    return value;
}

class CompiledIntegerRelationalLayer {
public:
    CompiledIntegerRelationalLayer(const core::RelationalComplex& complex,
                                   core::Grade grade,
                                   std::vector<OperatorChannelKind> channels,
                                   std::vector<std::int64_t> gains,
                                   std::int64_t bias = 0,
                                   IntegerActivation activation = IntegerActivation::Identity)
        : complex_(&complex), grade_(grade), channels_(std::move(channels)), gains_(std::move(gains)), bias_(bias), activation_(activation) {
        if (channels_.size() != gains_.size()) throw std::invalid_argument("compiled relational layer channel/gain mismatch");
        if (grade.value >= complex.grade_count()) throw std::out_of_range("compiled relational layer grade");
    }

    [[nodiscard]] const std::vector<OperatorChannelKind>& channels() const noexcept { return channels_; }
    [[nodiscard]] const std::vector<std::int64_t>& gains() const noexcept { return gains_; }
    [[nodiscard]] std::int64_t bias() const noexcept { return bias_; }
    [[nodiscard]] IntegerActivation activation() const noexcept { return activation_; }

    [[nodiscard]] field::MachineLatticeField forward(const field::MachineLatticeField& input) const {
        if (&input.complex() != complex_ || input.grade() != grade_) throw std::invalid_argument("compiled relational layer input mismatch");
        std::vector<std::int64_t> result(input.values().size(), bias_);
        for (std::size_t c = 0; c < channels_.size(); ++c) {
            if (gains_[c] == 0) continue;
            const auto response = apply_operator_channel(channels_[c], input);
            for (std::size_t i = 0; i < result.size(); ++i) {
                result[i] = numeric::checked_mac_i64(result[i], gains_[c], response.values()[i]);
            }
        }
        for (auto& value : result) value = apply_integer_activation(activation_, value);
        return field::MachineLatticeField(*complex_, grade_, std::move(result));
    }

private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    std::vector<OperatorChannelKind> channels_;
    std::vector<std::int64_t> gains_;
    std::int64_t bias_{};
    IntegerActivation activation_{IntegerActivation::Identity};
};

struct CompiledIntegerLayerPlan {
    OperatorChannelBasis basis;
    CompiledIntegerRelationalLayer layer;
};

inline CompiledIntegerLayerPlan compile_integer_relational_layer(
    const core::RelationalComplex& complex,
    core::Grade grade,
    const std::vector<OperatorChannelKind>& candidate_channels,
    const std::vector<std::int64_t>& original_gains,
    std::int64_t bias = 0,
    IntegerActivation activation = IntegerActivation::Identity) {
    auto basis = reduce_operator_channels(complex, grade, candidate_channels);
    auto reduced_gains = reduce_machine_channel_gains(basis, original_gains);
    CompiledIntegerRelationalLayer layer(complex, grade, basis.basis_channels(), reduced_gains, bias, activation);
    return CompiledIntegerLayerPlan{std::move(basis), std::move(layer)};
}

class IntegerRelationalNetwork {
public:
    IntegerRelationalNetwork(const core::RelationalComplex& complex, core::Grade grade)
        : complex_(&complex), grade_(grade) {
        if (grade.value >= complex.grade_count()) throw std::out_of_range("integer relational network grade");
    }

    void add_layer(CompiledIntegerRelationalLayer layer) { layers_.push_back(std::move(layer)); }
    [[nodiscard]] std::size_t size() const noexcept { return layers_.size(); }

    [[nodiscard]] field::MachineLatticeField forward(const field::MachineLatticeField& input) const {
        if (&input.complex() != complex_ || input.grade() != grade_) throw std::invalid_argument("integer relational network input mismatch");
        field::MachineLatticeField current = input;
        for (const auto& layer : layers_) current = layer.forward(current);
        return current;
    }

private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    std::vector<CompiledIntegerRelationalLayer> layers_;
};

struct IntegerNetworkTrainingSample {
    std::vector<std::int64_t> input;
    std::vector<std::int64_t> target;
};

struct TwoLayerIntegerFit {
    std::vector<std::int64_t> first_gains;
    std::int64_t first_bias{};
    std::vector<std::int64_t> second_gains;
    std::int64_t second_bias{};
    std::uint64_t absolute_error{std::numeric_limits<std::uint64_t>::max()};
};

inline std::uint64_t network_absolute_i64(std::int64_t value) {
    if (value >= 0) return static_cast<std::uint64_t>(value);
    if (value == std::numeric_limits<std::int64_t>::min()) throw std::overflow_error("absolute int64 overflow");
    return static_cast<std::uint64_t>(-value);
}

inline TwoLayerIntegerFit fit_small_two_layer_network(
    const core::RelationalComplex& complex,
    core::Grade grade,
    const OperatorChannelBasis& basis,
    const std::vector<IntegerNetworkTrainingSample>& samples,
    std::int64_t parameter_min,
    std::int64_t parameter_max,
    IntegerActivation hidden_activation = IntegerActivation::ReLU) {
    if (basis.complex != &complex || basis.grade != grade) throw std::invalid_argument("network fit basis mismatch");
    if (samples.empty()) throw std::invalid_argument("network fit requires samples");
    if (parameter_min > parameter_max) throw std::invalid_argument("invalid network parameter range");
    const std::size_t width = complex.grade(grade).cell_count();
    for (const auto& sample : samples) {
        if (sample.input.size() != width || sample.target.size() != width) throw std::invalid_argument("network training sample size mismatch");
    }
    const std::size_t rank = basis.rank();
    const std::size_t parameter_count = 2u * (rank + 1u);
    std::vector<std::int64_t> params(parameter_count, parameter_min);
    TwoLayerIntegerFit best;
    bool finished = false;
    while (!finished) {
        std::vector<std::int64_t> first(params.begin(), params.begin() + static_cast<std::ptrdiff_t>(rank));
        const std::int64_t first_bias = params[rank];
        const auto second_begin = params.begin() + static_cast<std::ptrdiff_t>(rank + 1u);
        std::vector<std::int64_t> second(second_begin, second_begin + static_cast<std::ptrdiff_t>(rank));
        const std::int64_t second_bias = params.back();
        CompiledIntegerRelationalLayer first_layer(complex, grade, basis.basis_channels(), first, first_bias, hidden_activation);
        CompiledIntegerRelationalLayer second_layer(complex, grade, basis.basis_channels(), second, second_bias, IntegerActivation::Identity);
        std::uint64_t error = 0;
        for (const auto& sample : samples) {
            field::MachineLatticeField input(complex, grade, sample.input);
            const auto hidden = first_layer.forward(input);
            const auto output = second_layer.forward(hidden);
            for (std::size_t i = 0; i < width; ++i) {
                const auto diff = numeric::checked_sub_i64(output.values()[i], sample.target[i]);
                const auto term = network_absolute_i64(diff);
                if (error > std::numeric_limits<std::uint64_t>::max() - term) throw std::overflow_error("network fit loss overflow");
                error += term;
            }
        }
        if (error < best.absolute_error) {
            best = TwoLayerIntegerFit{first, first_bias, second, second_bias, error};
            if (error == 0) return best;
        }

        for (std::size_t i = 0; i < params.size(); ++i) {
            if (params[i] < parameter_max) {
                ++params[i];
                break;
            }
            params[i] = parameter_min;
            if (i + 1u == params.size()) finished = true;
        }
    }
    return best;
}

} // namespace rfc::learning
