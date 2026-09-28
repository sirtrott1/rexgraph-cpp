#pragma once
#include "rfc/machine/packed_bits.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace rfc::learning {

// A trainable two-input Boolean unit. The parameter is exactly four bits: one
// output value for each (a,b) state. Forward and fitting are implemented with
// packed machine logic and popcount, with no floating arithmetic.
class BinaryTruthGate2 {
public:
    explicit constexpr BinaryTruthGate2(std::uint8_t truth_table = 0) noexcept
        : truth_table_(static_cast<std::uint8_t>(truth_table & 0x0fu)) {}

    [[nodiscard]] constexpr std::uint8_t truth_table() const noexcept { return truth_table_; }

    [[nodiscard]] machine::PackedBits forward(const machine::PackedBits& a,
                                               const machine::PackedBits& b) const {
        if (a.size() != b.size()) throw std::invalid_argument("binary gate input size mismatch");
        const auto na = ~a;
        const auto nb = ~b;
        machine::PackedBits result(a.size(), false);
        if ((truth_table_ & 0x1u) != 0) result = result | (na & nb); // 00
        if ((truth_table_ & 0x2u) != 0) result = result | (na & b);  // 01
        if ((truth_table_ & 0x4u) != 0) result = result | (a & nb);  // 10
        if ((truth_table_ & 0x8u) != 0) result = result | (a & b);   // 11
        return result;
    }

private:
    std::uint8_t truth_table_{};
};

struct BinaryGateFit {
    BinaryTruthGate2 gate;
    std::size_t hamming_error{};
};

inline BinaryGateFit fit_binary_truth_gate2(const machine::PackedBits& a,
                                             const machine::PackedBits& b,
                                             const machine::PackedBits& target) {
    if (a.size() != b.size() || a.size() != target.size()) throw std::invalid_argument("binary gate training size mismatch");
    std::size_t best_error = std::numeric_limits<std::size_t>::max();
    std::uint8_t best_table = 0;
    for (std::uint8_t table = 0; table < 16u; ++table) {
        BinaryTruthGate2 gate(table);
        const auto prediction = gate.forward(a, b);
        const auto error = (prediction ^ target).count();
        if (error < best_error) {
            best_error = error;
            best_table = table;
        }
    }
    return BinaryGateFit{BinaryTruthGate2(best_table), best_error};
}

} // namespace rfc::learning
