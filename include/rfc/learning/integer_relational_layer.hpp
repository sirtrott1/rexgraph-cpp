#pragma once
#include "rfc/field/lattice_boundary.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

// Matrix-free exact integer propagation on the retained lattice. The three
// gains are trainable integer parameters; topology remains fixed by the
// RelationalComplex. The layer stays at one grade by combining the current
// field with down-up and up-down relational responses.
class MachineIntegerRelationalLayer {
public:
    MachineIntegerRelationalLayer(const core::RelationalComplex& complex,
                                  core::Grade grade,
                                  std::int64_t self_gain = 1,
                                  std::int64_t down_gain = 1,
                                  std::int64_t up_gain = 1,
                                  std::int64_t bias = 0)
        : complex_(&complex), grade_(grade), self_gain_(self_gain), down_gain_(down_gain), up_gain_(up_gain), bias_(bias) {
        if (grade.value >= complex.grade_count()) throw std::out_of_range("integer relational layer grade");
    }

    [[nodiscard]] std::int64_t self_gain() const noexcept { return self_gain_; }
    [[nodiscard]] std::int64_t down_gain() const noexcept { return down_gain_; }
    [[nodiscard]] std::int64_t up_gain() const noexcept { return up_gain_; }
    [[nodiscard]] std::int64_t bias() const noexcept { return bias_; }

    [[nodiscard]] field::MachineLatticeField forward(const field::MachineLatticeField& input) const {
        if (&input.complex() != complex_ || input.grade() != grade_) throw std::invalid_argument("integer relational layer input mismatch");
        std::vector<std::int64_t> result(input.values().size(), bias_);
        add_scaled(result, input.values(), self_gain_);

        if (grade_.value > 0) {
            field::LatticeBoundaryOperator down(*complex_, grade_);
            const auto lower = down.apply(input);
            const auto returned = down.transpose_apply(lower);
            add_scaled(result, returned.values(), down_gain_);
        }
        if (grade_.value < complex_->max_grade().value) {
            field::LatticeBoundaryOperator up(*complex_, core::Grade{static_cast<std::uint16_t>(grade_.value + 1u)});
            const auto higher = up.transpose_apply(input);
            const auto returned = up.apply(higher);
            add_scaled(result, returned.values(), up_gain_);
        }
        return field::MachineLatticeField(*complex_, grade_, std::move(result));
    }

private:
    static void add_scaled(std::vector<std::int64_t>& destination,
                           const std::vector<std::int64_t>& source,
                           std::int64_t gain) {
        if (destination.size() != source.size()) throw std::invalid_argument("integer relational layer channel mismatch");
        if (gain == 0) return;
        for (std::size_t i = 0; i < destination.size(); ++i) {
            destination[i] = numeric::checked_mac_i64(destination[i], gain, source[i]);
        }
    }

    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    std::int64_t self_gain_{1};
    std::int64_t down_gain_{1};
    std::int64_t up_gain_{1};
    std::int64_t bias_{0};
};

struct IntegerRelationalTrainingSample {
    std::vector<std::int64_t> input;
    std::vector<std::int64_t> target;
};

struct IntegerRelationalFit {
    std::int64_t self_gain{};
    std::int64_t down_gain{};
    std::int64_t up_gain{};
    std::int64_t bias{};
    std::uint64_t absolute_error{std::numeric_limits<std::uint64_t>::max()};
};

inline std::uint64_t absolute_i64(std::int64_t value) {
    if (value >= 0) return static_cast<std::uint64_t>(value);
    if (value == std::numeric_limits<std::int64_t>::min()) throw std::overflow_error("absolute int64 overflow");
    return static_cast<std::uint64_t>(-value);
}

// Exhaustive exact reference fitter for the four integer gains of a native
// relational propagation layer. This is intentionally a small-model verifier;
// future optimizers can replace the search without changing the layer contract.
inline IntegerRelationalFit fit_small_integer_relational_layer(
    const core::RelationalComplex& complex,
    core::Grade grade,
    const std::vector<IntegerRelationalTrainingSample>& samples,
    std::int64_t gain_min,
    std::int64_t gain_max,
    std::int64_t bias_min = 0,
    std::int64_t bias_max = 0) {
    if (samples.empty()) throw std::invalid_argument("integer relational fit requires samples");
    if (gain_min > gain_max || bias_min > bias_max) throw std::invalid_argument("invalid integer relational search range");
    const auto width = complex.grade(grade).cell_count();
    for (const auto& sample : samples) {
        if (sample.input.size() != width || sample.target.size() != width) throw std::invalid_argument("integer relational training sample size mismatch");
    }

    IntegerRelationalFit best;
    for (std::int64_t self = gain_min; self <= gain_max; ++self) {
        for (std::int64_t down = gain_min; down <= gain_max; ++down) {
            for (std::int64_t up = gain_min; up <= gain_max; ++up) {
                for (std::int64_t bias = bias_min; bias <= bias_max; ++bias) {
                    MachineIntegerRelationalLayer layer(complex, grade, self, down, up, bias);
                    std::uint64_t error = 0;
                    for (const auto& sample : samples) {
                        field::MachineLatticeField input(complex, grade, sample.input);
                        const auto output = layer.forward(input);
                        for (std::size_t i = 0; i < width; ++i) {
                            const auto diff = numeric::checked_sub_i64(output.values()[i], sample.target[i]);
                            const auto term = absolute_i64(diff);
                            if (error > std::numeric_limits<std::uint64_t>::max() - term) throw std::overflow_error("integer relational fit loss overflow");
                            error += term;
                        }
                    }
                    if (error < best.absolute_error) {
                        best = IntegerRelationalFit{self, down, up, bias, error};
                        if (error == 0) return best;
                    }
                    if (bias == bias_max) break;
                }
                if (up == gain_max) break;
            }
            if (down == gain_max) break;
        }
        if (self == gain_max) break;
    }
    return best;
}

} // namespace rfc::learning
