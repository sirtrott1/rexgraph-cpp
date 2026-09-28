#include "rfc/learning/binary_gate.hpp"
#include "rfc/machine/packed_bits.hpp"
#include <cstddef>
#include <iostream>

int main() {
    constexpr std::size_t samples = 256;
    rfc::machine::PackedBits a(samples, false), b(samples, false), target(samples, false);
    for (std::size_t i = 0; i < samples; ++i) {
        const bool av = (i & 1u) != 0;
        const bool bv = (i & 2u) != 0;
        a.set(i, av);
        b.set(i, bv);
        target.set(i, av != bv);
    }
    const auto fit = rfc::learning::fit_binary_truth_gate2(a, b, target);
    std::cout << "learned_truth_table=0x" << std::hex << static_cast<unsigned>(fit.gate.truth_table())
              << std::dec << " hamming_error=" << fit.hamming_error << '\n';
}
