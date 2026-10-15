#pragma once
#include "rfc/core/builder.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rfc::algebra {
enum class PolygonFilling : std::uint8_t { BoundaryOnly, Disk, DoubledDiskSphere };

// Creates declared primary edges and ordered faces. No metric embedding,
// geometric circumference, or phase action is inferred from the vertex count.
inline core::RelationalComplex oriented_polygon(const std::vector<core::IdentityKey>& vertices,
                                               PolygonFilling filling) {
    if (filling != PolygonFilling::BoundaryOnly && filling != PolygonFilling::Disk &&
        filling != PolygonFilling::DoubledDiskSphere)
        throw std::invalid_argument("unknown polygon filling");
    if (vertices.size() < 3) throw std::invalid_argument("polygon requires at least three distinct identities");
    if (vertices.size() >= std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("polygon cell count exceeds retained index domain");
    auto sorted = vertices;
    std::sort(sorted.begin(),sorted.end());
    if (std::adjacent_find(sorted.begin(),sorted.end()) != sorted.end())
        throw std::invalid_argument("polygon repeats a vertex identity");
    core::RelationSignature signature;
    const auto edge = signature.add_type("oriented_polygon_edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face = signature.add_attaching_loop_type("oriented_polygon_face",vertices.size(),vertices.size());
    core::RelationalComplexBuilder builder(std::move(signature));
    for (std::size_t j = 0; j < vertices.size(); ++j)
        builder.add_grade_one(edge,{{vertices[j],true,true,false,1},
                                   {vertices[(j+1u)%vertices.size()],true,false,false,1}});
    builder.declare_grade(core::Grade{2});
    if (filling != PolygonFilling::BoundaryOnly) {
        std::vector<core::HigherIncidenceInput> occurrences;
        occurrences.reserve(vertices.size());
        for (std::size_t j = 0; j < vertices.size(); ++j)
            occurrences.emplace_back(static_cast<std::uint32_t>(j),true,false,false,numeric::Rational{1});
        builder.add_higher(core::Grade{2},face,occurrences);
        if (filling == PolygonFilling::DoubledDiskSphere) {
            occurrences.clear();
            for (std::size_t j = vertices.size(); j != 0; --j)
                occurrences.emplace_back(static_cast<std::uint32_t>(j-1u),true,true,false,numeric::Rational{1});
            builder.add_higher(core::Grade{2},face,occurrences);
        }
    }
    return builder.freeze();
}
} // namespace rfc::algebra
