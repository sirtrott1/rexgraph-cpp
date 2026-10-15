#pragma once
#include "rfc/learning/operator_basis.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

enum class IntegerActivation : std::uint8_t {
    Identity,
    ReLU,
    Sign
};

inline void validate_integer_activation(IntegerActivation activation) {
    switch (activation) {
        case IntegerActivation::Identity: case IntegerActivation::ReLU: case IntegerActivation::Sign: return;
    }
    throw std::invalid_argument("unknown integer activation");
}

inline std::int64_t apply_integer_activation(IntegerActivation activation, std::int64_t value) {
    switch (activation) {
        case IntegerActivation::Identity: return value;
        case IntegerActivation::ReLU: return value > 0 ? value : 0;
        case IntegerActivation::Sign: return value >= 0 ? 1 : -1;
    }
    throw std::invalid_argument("unknown integer activation");
}

class IntegerNetworkWorkspace {
public:
    IntegerNetworkWorkspace(const core::RelationalComplex& complex, core::Grade grade, std::size_t samples)
        : complex_(&complex), grade_(grade), capacity_(samples) {
        if (grade.value >= complex.grade_count() || !samples) throw std::invalid_argument("integer workspace grade or capacity");
        const auto width = complex.grade(grade).cell_count();
        const auto lower = grade.value ? complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)}).cell_count() : 0;
        const auto upper = grade.value < complex.max_grade().value ? complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value + 1u)}).cell_count() : 0;
        current_.resize(slots(width, samples)); next_.resize(slots(width, samples));
        down_.resize(slots(width, samples)); up_.resize(slots(width, samples)); sum_.resize(slots(width, samples));
        lower_.resize(slots(lower, samples)); upper_.resize(slots(upper, samples));
    }
    IntegerNetworkWorkspace(const core::RelationalComplex&&, core::Grade, std::size_t) = delete;
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t retained_slots() const {
        const auto a = slots(current_.size(), 5);
        if (lower_.size() > std::numeric_limits<std::size_t>::max() - a ||
            upper_.size() > std::numeric_limits<std::size_t>::max() - a - lower_.size())
            throw std::overflow_error("integer workspace slot count overflow");
        return a + lower_.size() + upper_.size();
    }
    static std::size_t slots(std::size_t width, std::size_t samples) {
        if (samples && width > std::numeric_limits<std::size_t>::max() / samples)
            throw std::overflow_error("integer batch slot count overflow");
        return width * samples;
    }
private:
    const core::RelationalComplex* complex_;
    core::Grade grade_;
    std::size_t capacity_;
    std::vector<std::int64_t> current_, next_, down_, up_, sum_, lower_, upper_;
    friend class CompiledIntegerRelationalLayer;
    friend class IntegerRelationalNetwork;
};

class CompiledIntegerRelationalLayer {
public:
    CompiledIntegerRelationalLayer(const core::RelationalComplex& complex,
                                   core::Grade grade,
                                   std::vector<OperatorChannelKind> channels,
                                   std::vector<std::int64_t> gains,
                                   std::int64_t bias = 0,
                                   IntegerActivation activation = IntegerActivation::Identity)
        : complex_(&complex), grade_(grade), channels_(std::move(channels)), gains_(std::move(gains)), bias_(bias), activation_(activation) {
        validate_integer_activation(activation_);
        for (const auto channel : channels_) validate_operator_channel(channel);
        if (channels_.size() != gains_.size()) throw std::invalid_argument("compiled relational layer channel/gain mismatch");
        if (grade.value >= complex.grade_count()) throw std::out_of_range("compiled relational layer grade");
    }

    CompiledIntegerRelationalLayer(const core::RelationalComplex&&, core::Grade,
                                   std::vector<OperatorChannelKind>, std::vector<std::int64_t>,
                                   std::int64_t = 0, IntegerActivation = IntegerActivation::Identity) = delete;
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
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
    void forward_batch_into(IntegerNetworkWorkspace& work, std::span<const std::int64_t> input,
                            std::span<std::int64_t> output, std::size_t samples,
                            const std::function<void()>& check) const {
        std::fill(output.begin(), output.end(), bias_);
        bool have_down = false, have_up = false, have_sum = false;
        const auto n = input.size();
        auto down = std::span(work.down_).first(n);
        auto up = std::span(work.up_).first(n);
        auto sum = std::span(work.sum_).first(n);
        auto make_down = [&] {
            if (have_down) return;
            std::fill(down.begin(), down.end(), 0);
            if (grade_.value) {
                field::LatticeBoundaryOperator boundary(*complex_, grade_);
                const auto lower_size = complex_->grade(boundary.lower_grade()).cell_count() * samples;
                auto lower = std::span(work.lower_).first(lower_size);
                boundary.apply_batch_into(input, lower, samples);
                boundary.transpose_batch_into(lower, down, samples);
            }
            have_down = true;
        };
        auto make_up = [&] {
            if (have_up) return;
            std::fill(up.begin(), up.end(), 0);
            if (grade_.value < complex_->max_grade().value) {
                field::LatticeBoundaryOperator boundary(*complex_, core::Grade{static_cast<std::uint16_t>(grade_.value + 1u)});
                const auto upper_size = complex_->grade(boundary.upper_grade()).cell_count() * samples;
                auto upper = std::span(work.upper_).first(upper_size);
                boundary.transpose_batch_into(input, upper, samples);
                boundary.apply_batch_into(upper, up, samples);
            }
            have_up = true;
        };
        for (std::size_t c = 0; c < channels_.size(); ++c) {
            if (check) check();
            if (!gains_[c]) continue;
            auto response = input;
            if (channels_[c] == OperatorChannelKind::HodgeDown) { make_down(); response = down; }
            else if (channels_[c] == OperatorChannelKind::HodgeUp) { make_up(); response = up; }
            else if (channels_[c] == OperatorChannelKind::Hodge) {
                if (!have_sum) {
                    make_down(); make_up();
                    for (std::size_t i = 0; i < n; ++i) sum[i] = numeric::checked_add_i64(down[i], up[i]);
                    have_sum = true;
                }
                response = sum;
            }
            for (std::size_t i = 0; i < n; ++i) output[i] = numeric::checked_mac_i64(output[i], gains_[c], response[i]);
        }
        for (auto& value : output) value = apply_integer_activation(activation_, value);
    }
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    std::vector<OperatorChannelKind> channels_;
    std::vector<std::int64_t> gains_;
    std::int64_t bias_{};
    IntegerActivation activation_{IntegerActivation::Identity};
    friend class IntegerRelationalNetwork;
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
    validate_integer_activation(activation);
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

    IntegerRelationalNetwork(const core::RelationalComplex&&, core::Grade) = delete;
    void add_layer(CompiledIntegerRelationalLayer layer) {
        if (&layer.complex() != complex_ || layer.grade() != grade_)
            throw std::invalid_argument("network layer source/grade mismatch");
        layers_.push_back(std::move(layer));
    }
    [[nodiscard]] std::size_t size() const noexcept { return layers_.size(); }

    [[nodiscard]] field::MachineLatticeField forward(const field::MachineLatticeField& input) const {
        if (&input.complex() != complex_ || input.grade() != grade_) throw std::invalid_argument("integer relational network input mismatch");
        field::MachineLatticeField current = input;
        for (const auto& layer : layers_) current = layer.forward(current);
        return current;
    }

    // Each sample follows the declared channel and occurrence order independently.
    [[nodiscard]] std::vector<std::int64_t> forward_batch(std::span<const std::int64_t> input,
        std::size_t samples, IntegerNetworkWorkspace& work, const std::function<void()>& check = {}) const {
        if (work.complex_ != complex_ || work.grade_ != grade_ || !samples || samples > work.capacity_ ||
            input.size() != IntegerNetworkWorkspace::slots(complex_->grade(grade_).cell_count(), samples))
            throw std::invalid_argument("integer network batch source, grade or shape mismatch");
        if (check) check();
        auto current = std::span(work.current_).first(input.size());
        auto next = std::span(work.next_).first(input.size());
        std::copy(input.begin(), input.end(), current.begin());
        for (const auto& layer : layers_) {
            if (check) check();
            layer.forward_batch_into(work, current, next, samples, check);
            std::swap(current, next);
        }
        if (check) check();
        return {current.begin(), current.end()};
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
    validate_integer_activation(hidden_activation);
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

inline CompiledIntegerLayerPlan compile_integer_relational_layer(
    const core::RelationalComplex&&, core::Grade, const std::vector<OperatorChannelKind>&,
    const std::vector<std::int64_t>&, std::int64_t = 0,
    IntegerActivation = IntegerActivation::Identity) = delete;

} // namespace rfc::learning
