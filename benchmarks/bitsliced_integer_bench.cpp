#include "rfc/learning/bitsliced_threshold.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    constexpr std::size_t lanes = 1u << 20u;
    constexpr std::size_t feature_count = 8;
    constexpr std::size_t repeats = 100;
    std::vector<rfc::machine::PackedBits> features;
    features.reserve(feature_count);
    for (std::size_t f = 0; f < feature_count; ++f) {
        rfc::machine::PackedBits bits(lanes);
        for (std::size_t i = f; i < lanes; i += (f + 3u)) bits.set(i);
        features.push_back(std::move(bits));
    }
    rfc::learning::BitSlicedThresholdUnit<16> unit({1,-2,3,1,-1,2,1,-1}, 2);
    std::size_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < repeats; ++r) checksum += unit.forward(features).count();
    const auto stop = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
    std::cout << "lanes=" << lanes << " features=" << feature_count << " repeats=" << repeats
              << " elapsed_ns=" << ns << " checksum=" << checksum << '\n';
    return 0;
}
