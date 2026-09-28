#pragma once
#include "rfc/composite/aspect.hpp"
#include "rfc/core/identity.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace rfc::core {

struct AspectValue {
    composite::AspectId aspect{};
    bool value{};
};

// Reserved builder-time symbolic aspect. It means "this relation type's
// declared distinguished role" and is resolved to the signature-owned aspect
// before an incidence enters the retained source. It preserves an ergonomic
// branch-input spelling without making `head` a universal stored field.
inline constexpr composite::AspectId type_distinguished_role_aspect =
    std::numeric_limits<composite::AspectId>::max();

struct BoundaryOccurrenceInput {
    IdentityKey identity{};
    bool exists{true};
    bool negative{false};
    numeric::Rational magnitude{1};
    std::vector<AspectValue> aspects;
    std::optional<OccurrenceKey> occurrence_key;

    BoundaryOccurrenceInput() = default;
    BoundaryOccurrenceInput(IdentityKey identity_, bool exists_, bool negative_,
                            numeric::Rational magnitude_, std::vector<AspectValue> aspects_ = {},
                            std::optional<OccurrenceKey> occurrence_key_ = std::nullopt)
        : identity(identity_), exists(exists_), negative(negative_), magnitude(std::move(magnitude_)),
          aspects(std::move(aspects_)), occurrence_key(occurrence_key_) {}

    // Ergonomic compatibility constructor: the fourth Boolean denotes the
    // relation type's declared distinguished role, not a universal head field.
    BoundaryOccurrenceInput(IdentityKey identity_, bool exists_, bool negative_, bool distinguished_role,
                            numeric::Rational magnitude_, std::vector<AspectValue> aspects_ = {},
                            std::optional<OccurrenceKey> occurrence_key_ = std::nullopt)
        : identity(identity_), exists(exists_), negative(negative_), magnitude(std::move(magnitude_)),
          aspects(std::move(aspects_)), occurrence_key(occurrence_key_) {
        if (distinguished_role) aspects.insert(aspects.begin(), {type_distinguished_role_aspect, true});
    }
};

struct HigherIncidenceInput {
    std::uint32_t lower_index{};
    bool exists{true};
    bool negative{false};
    numeric::Rational magnitude{1};
    std::vector<AspectValue> aspects;
    std::optional<OccurrenceKey> occurrence_key;

    HigherIncidenceInput() = default;
    HigherIncidenceInput(std::uint32_t lower_index_, bool exists_, bool negative_,
                         numeric::Rational magnitude_, std::vector<AspectValue> aspects_ = {},
                         std::optional<OccurrenceKey> occurrence_key_ = std::nullopt)
        : lower_index(lower_index_), exists(exists_), negative(negative_), magnitude(std::move(magnitude_)),
          aspects(std::move(aspects_)), occurrence_key(occurrence_key_) {}

    HigherIncidenceInput(std::uint32_t lower_index_, bool exists_, bool negative_, bool distinguished_role,
                         numeric::Rational magnitude_, std::vector<AspectValue> aspects_ = {},
                         std::optional<OccurrenceKey> occurrence_key_ = std::nullopt)
        : lower_index(lower_index_), exists(exists_), negative(negative_), magnitude(std::move(magnitude_)),
          aspects(std::move(aspects_)), occurrence_key(occurrence_key_) {
        if (distinguished_role) aspects.insert(aspects.begin(), {type_distinguished_role_aspect, true});
    }
};

struct RationalIncidence {
    std::uint32_t lower_index{};
    numeric::Rational coefficient{0};
    bool exists{true};
    bool negative{false};
    std::vector<AspectValue> aspects;
};

} // namespace rfc::core
