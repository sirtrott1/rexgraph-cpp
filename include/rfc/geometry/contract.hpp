#pragma once
#include <concepts>
#include <cstdint>
#include <string_view>

namespace rfc::geometry {

// Geometry is a declared realization/organization of relational structure.
// It is intentionally nonmetric: a metric may later measure a geometry, but
// no geometry contract in this layer requires an inner product or distance.
enum class GeometryKind : std::uint8_t {
    HigherAttachment,
    Projective,
    Coordinate,
    Semantic,
    Embedding,
    ExpansionLift,
    TypedRealization
};

struct GeometryDescriptor {
    GeometryKind kind{GeometryKind::Coordinate};
    std::string_view name{};
    bool exact{true};
    [[nodiscard]] constexpr bool requires_metric() const noexcept { return false; }
};

template <class T>
concept NonmetricGeometry = requires {
    { T::geometry_descriptor() } -> std::same_as<GeometryDescriptor>;
};

} // namespace rfc::geometry
