#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace rfc::numeric {

inline std::int64_t checked_add_i64(std::int64_t lhs, std::int64_t rhs) {
#if defined(__GNUC__) || defined(__clang__)
    std::int64_t result{};
    if (__builtin_add_overflow(lhs, rhs, &result)) throw std::overflow_error("int64 addition overflow");
    return result;
#else
    if ((rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs) ||
        (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs)) {
        throw std::overflow_error("int64 addition overflow");
    }
    return lhs + rhs;
#endif
}

inline std::int64_t checked_sub_i64(std::int64_t lhs, std::int64_t rhs) {
#if defined(__GNUC__) || defined(__clang__)
    std::int64_t result{};
    if (__builtin_sub_overflow(lhs, rhs, &result)) throw std::overflow_error("int64 subtraction overflow");
    return result;
#else
    if ((rhs < 0 && lhs > std::numeric_limits<std::int64_t>::max() + rhs) ||
        (rhs > 0 && lhs < std::numeric_limits<std::int64_t>::min() + rhs)) {
        throw std::overflow_error("int64 subtraction overflow");
    }
    return lhs - rhs;
#endif
}

// Portable reference: check signed bounds before multiplication, never after UB.
inline std::int64_t checked_mul_i64_portable(std::int64_t lhs, std::int64_t rhs) {
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    bool overflow = false;
    if (lhs > 0) {
        if (rhs > 0) overflow = lhs > hi / rhs;
        else if (rhs < 0) overflow = rhs < lo / lhs;
    } else if (lhs < 0) {
        if (rhs > 0) overflow = lhs < lo / rhs;
        else if (rhs < 0) overflow = lhs < hi / rhs;
    }
    if (overflow) throw std::overflow_error("int64 multiplication overflow");
    return lhs * rhs;
}

inline std::int64_t checked_mul_i64(std::int64_t lhs, std::int64_t rhs) {
#if defined(__GNUC__) || defined(__clang__)
    std::int64_t result{};
    if (__builtin_mul_overflow(lhs, rhs, &result)) throw std::overflow_error("int64 multiplication overflow");
    return result;
#else
    return checked_mul_i64_portable(lhs, rhs);
#endif
}

inline std::int64_t checked_mac_i64(std::int64_t accumulator, std::int64_t lhs, std::int64_t rhs) {
    return checked_add_i64(accumulator, checked_mul_i64(lhs, rhs));
}

} // namespace rfc::numeric
