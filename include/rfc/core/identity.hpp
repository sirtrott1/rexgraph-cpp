#pragma once
#include "rfc/core/grade.hpp"
#include <cstdint>
#include <compare>
#include <string>

namespace rfc::core {

using IdentityKey = std::uint64_t;
using RelationTypeId = std::uint32_t;

struct RelationKey {
    std::uint64_t value{};
    constexpr auto operator<=>(const RelationKey&) const = default;
};

struct OccurrenceKey {
    std::uint64_t value{};
    constexpr auto operator<=>(const OccurrenceKey&) const = default;
};

struct CellId {
    Grade grade{};
    std::uint32_t index{};
    constexpr auto operator<=>(const CellId&) const = default;
};

struct RelationId {
    Grade grade{};
    std::uint32_t index{};
    constexpr auto operator<=>(const RelationId&) const = default;
    constexpr operator CellId() const noexcept { return CellId{grade, index}; }
};

struct RelationIdentity {
    Grade grade{};
    RelationKey key{};
    constexpr auto operator<=>(const RelationIdentity&) const = default;
};

} // namespace rfc::core
