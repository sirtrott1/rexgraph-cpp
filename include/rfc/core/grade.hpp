#pragma once
#include <cstdint>
#include <compare>

namespace rfc::core {

struct Grade {
    std::uint16_t value{};
    constexpr auto operator<=>(const Grade&) const = default;
};

struct SemanticScale {
    std::uint16_t value{};
    constexpr auto operator<=>(const SemanticScale&) const = default;
};

inline constexpr Grade operator+(Grade grade, std::uint16_t amount) { return Grade{static_cast<std::uint16_t>(grade.value + amount)}; }
inline constexpr Grade operator-(Grade grade, std::uint16_t amount) { return Grade{static_cast<std::uint16_t>(grade.value - amount)}; }

} // namespace rfc::core
