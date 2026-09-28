#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace rfc::machine {
// Representation arithmetic only; these helpers impose no relational meaning.
[[nodiscard]] inline constexpr std::size_t checked_size_add(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a) throw std::overflow_error("size addition overflow");
    return a + b;
}
[[nodiscard]] inline constexpr std::size_t checked_size_mul(std::size_t a, std::size_t b) {
    if (b != 0 && a > std::numeric_limits<std::size_t>::max() / b) throw std::overflow_error("size product overflow");
    return a * b;
}
[[nodiscard]] inline constexpr std::size_t ceil_div(std::size_t n, std::size_t d) {
    if (d == 0) throw std::invalid_argument("zero size divisor");
    return n / d + (n % d != 0);
}
[[nodiscard]] inline constexpr std::size_t checked_size(std::uint64_t n) {
    if (n > std::numeric_limits<std::size_t>::max()) throw std::overflow_error("size conversion overflow");
    return static_cast<std::size_t>(n);
}
} // namespace rfc::machine
