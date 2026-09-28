#include "rfc/packed_bits.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace rfc;

int main() {
    constexpr std::size_t n = 1u << 20u;
    PackedBits active(n, true), type_a(n), visible(n);
    for (std::size_t i = 0; i < n; ++i) {
        type_a.set(i, (i % 3u) == 0u);
        visible.set(i, (i % 5u) != 0u);
    }
    constexpr std::size_t repeats = 2000;
    std::size_t checksum = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < repeats; ++i) {
        auto q = active & type_a & visible;
        checksum += q.count();
    }
    const auto t1 = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    std::cout << "relations=" << n << " repeats=" << repeats
              << " elapsed_ns=" << ns << " checksum=" << checksum << "\n";
    return 0;
}
