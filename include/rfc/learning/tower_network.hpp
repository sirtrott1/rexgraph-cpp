#pragma once
#include "rfc/learning/integer_network.hpp"
#include "rfc/learning/tower_basis.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

class CompiledIntegerTowerLayer {
public:
    CompiledIntegerTowerLayer(const core::RelationalComplex& complex,
                              std::vector<TowerOperatorChannelKind> channels,
                              std::vector<std::int64_t> gains,
                              std::int64_t bias = 0,
                              IntegerActivation activation = IntegerActivation::Identity)
        : complex_(&complex), channels_(std::move(channels)), gains_(std::move(gains)), bias_(bias), activation_(activation) {
        if (channels_.size() != gains_.size()) throw std::invalid_argument("tower layer channel/gain mismatch");
    }

    [[nodiscard]] const std::vector<TowerOperatorChannelKind>& channels() const noexcept { return channels_; }
    [[nodiscard]] const std::vector<std::int64_t>& gains() const noexcept { return gains_; }

    [[nodiscard]] field::MachineLatticeTowerField forward(const field::MachineLatticeTowerField& input) const {
        if (&input.complex() != complex_) throw std::invalid_argument("tower layer input complex mismatch");
        field::MachineLatticeTowerField result(*complex_);
        for (std::size_t k = 0; k < result.grade_count(); ++k) {
            auto& values = result.grade_values(core::Grade{static_cast<std::uint16_t>(k)});
            for (auto& value : values) value = bias_;
        }
        for (std::size_t c = 0; c < channels_.size(); ++c) {
            if (gains_[c] == 0) continue;
            const auto response = apply_tower_operator_channel(channels_[c], input);
            for (std::size_t k = 0; k < result.grade_count(); ++k) {
                field::add_scaled_tower_grade(result.grade_values(core::Grade{static_cast<std::uint16_t>(k)}),
                                              response.grade_values(core::Grade{static_cast<std::uint16_t>(k)}), gains_[c]);
            }
        }
        for (std::size_t k = 0; k < result.grade_count(); ++k) {
            auto& values = result.grade_values(core::Grade{static_cast<std::uint16_t>(k)});
            for (auto& value : values) value = apply_integer_activation(activation_, value);
        }
        return result;
    }

private:
    const core::RelationalComplex* complex_{};
    std::vector<TowerOperatorChannelKind> channels_;
    std::vector<std::int64_t> gains_;
    std::int64_t bias_{};
    IntegerActivation activation_{IntegerActivation::Identity};
};

struct CompiledIntegerTowerLayerPlan {
    TowerOperatorBasis basis;
    CompiledIntegerTowerLayer layer;
};

inline CompiledIntegerTowerLayerPlan compile_integer_tower_layer(
    const core::RelationalComplex& complex,
    const std::vector<TowerOperatorChannelKind>& candidate_channels,
    const std::vector<std::int64_t>& original_gains,
    std::int64_t bias = 0,
    IntegerActivation activation = IntegerActivation::Identity) {
    auto basis = reduce_tower_operator_channels(complex, candidate_channels);
    auto reduced = reduce_machine_tower_channel_gains(basis, original_gains);
    CompiledIntegerTowerLayer layer(complex, basis.basis_channels(), reduced, bias, activation);
    return CompiledIntegerTowerLayerPlan{std::move(basis), std::move(layer)};
}

class IntegerTowerNetwork {
public:
    explicit IntegerTowerNetwork(const core::RelationalComplex& complex) : complex_(&complex) {}
    void add_layer(CompiledIntegerTowerLayer layer) { layers_.push_back(std::move(layer)); }
    [[nodiscard]] std::size_t size() const noexcept { return layers_.size(); }

    [[nodiscard]] field::MachineLatticeTowerField forward(const field::MachineLatticeTowerField& input) const {
        if (&input.complex() != complex_) throw std::invalid_argument("tower network input mismatch");
        field::MachineLatticeTowerField current = input;
        for (const auto& layer : layers_) current = layer.forward(current);
        return current;
    }

private:
    const core::RelationalComplex* complex_{};
    std::vector<CompiledIntegerTowerLayer> layers_;
};


struct IntegerTowerTrainingSample {
    std::vector<std::vector<std::int64_t>> input;
    std::vector<std::vector<std::int64_t>> target;
};

struct IntegerTowerFit {
    std::vector<std::int64_t> gains;
    std::int64_t bias{};
    std::uint64_t absolute_error{std::numeric_limits<std::uint64_t>::max()};
};

inline IntegerTowerFit fit_small_integer_tower_layer(
    const core::RelationalComplex& complex,
    const TowerOperatorBasis& basis,
    const std::vector<IntegerTowerTrainingSample>& samples,
    std::int64_t parameter_min,
    std::int64_t parameter_max,
    IntegerActivation activation = IntegerActivation::Identity) {
    if (basis.complex != &complex) throw std::invalid_argument("tower fit basis mismatch");
    if (samples.empty()) throw std::invalid_argument("tower fit requires samples");
    if (parameter_min > parameter_max) throw std::invalid_argument("invalid tower parameter range");
    for (const auto& sample : samples) {
        if (sample.input.size() != complex.grade_count() || sample.target.size() != complex.grade_count()) {
            throw std::invalid_argument("tower training sample grade count mismatch");
        }
        for (std::size_t k = 0; k < complex.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto width = complex.grade(grade).cell_count();
            if (sample.input[k].size() != width || sample.target[k].size() != width) {
                throw std::invalid_argument("tower training sample grade width mismatch");
            }
        }
    }
    std::vector<std::int64_t> params(basis.rank() + 1u, parameter_min);
    IntegerTowerFit best;
    bool finished = false;
    while (!finished) {
        std::vector<std::int64_t> gains(params.begin(), params.begin() + static_cast<std::ptrdiff_t>(basis.rank()));
        const auto bias = params.back();
        CompiledIntegerTowerLayer layer(complex, basis.basis_channels(), gains, bias, activation);
        std::uint64_t error = 0;
        for (const auto& sample : samples) {
            field::MachineLatticeTowerField input(complex, sample.input);
            const auto output = layer.forward(input);
            for (std::size_t k = 0; k < complex.grade_count(); ++k) {
                const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
                const auto& values = output.grade_values(grade);
                for (std::size_t i = 0; i < values.size(); ++i) {
                    const auto diff = numeric::checked_sub_i64(values[i], sample.target[k][i]);
                    const auto term = network_absolute_i64(diff);
                    if (error > std::numeric_limits<std::uint64_t>::max() - term) throw std::overflow_error("tower fit loss overflow");
                    error += term;
                }
            }
        }
        if (error < best.absolute_error) {
            best = IntegerTowerFit{gains, bias, error};
            if (error == 0) return best;
        }
        for (std::size_t i = 0; i < params.size(); ++i) {
            if (params[i] < parameter_max) { ++params[i]; break; }
            params[i] = parameter_min;
            if (i + 1u == params.size()) finished = true;
        }
    }
    return best;
}

} // namespace rfc::learning
