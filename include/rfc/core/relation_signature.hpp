#pragma once
#include "rfc/composite/aspect.hpp"
#include "rfc/core/identity.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::core {

enum class BoundaryPolicy : std::uint8_t {
    DeclaredExact,
    CanonicalComposite,
    Witness,
    Algebraic
};

// RelationShape is semantic classification metadata. Validation is performed
// by the signature-owned rule contract below, not by switching on this enum.
enum class RelationShape : std::uint8_t {
    Generic,
    Branching,
    AttachingLoop,
    Witness,
    Algebraic
};

enum class CompositeConstructorKind : std::uint8_t {
    None,
    NormalizedOneHead
};

enum class RelationRuleKind : std::uint8_t {
    AspectImpliesExistence,
    ActiveAspectCountEquals,
    AspectOrientationOpposesActiveComplement,
    AspectMagnitudeEquals,
    ActiveComplementMagnitudePositive,
    ActiveComplementMagnitudeSumEquals
};

struct RelationRule {
    RelationRuleKind kind{RelationRuleKind::AspectImpliesExistence};
    composite::AspectId aspect{};
    std::size_t count{};
    numeric::Rational value{0};

    [[nodiscard]] static RelationRule aspect_implies_existence(composite::AspectId aspect) {
        return RelationRule{RelationRuleKind::AspectImpliesExistence, aspect, 0u, 0};
    }
    [[nodiscard]] static RelationRule active_aspect_count(composite::AspectId aspect, std::size_t count) {
        return RelationRule{RelationRuleKind::ActiveAspectCountEquals, aspect, count, 0};
    }
    [[nodiscard]] static RelationRule role_opposes_complement(composite::AspectId aspect) {
        return RelationRule{RelationRuleKind::AspectOrientationOpposesActiveComplement, aspect, 0u, 0};
    }
    [[nodiscard]] static RelationRule role_magnitude(composite::AspectId aspect, numeric::Rational value) {
        value.canonicalize();
        return RelationRule{RelationRuleKind::AspectMagnitudeEquals, aspect, 0u, std::move(value)};
    }
    [[nodiscard]] static RelationRule complement_positive(composite::AspectId aspect) {
        return RelationRule{RelationRuleKind::ActiveComplementMagnitudePositive, aspect, 0u, 0};
    }
    [[nodiscard]] static RelationRule complement_magnitude_sum(composite::AspectId aspect,
                                                                numeric::Rational value) {
        value.canonicalize();
        return RelationRule{RelationRuleKind::ActiveComplementMagnitudeSumEquals, aspect, 0u, std::move(value)};
    }
};

struct RelationConstraints {
    RelationShape shape{RelationShape::Generic};
    std::optional<composite::AspectId> distinguished_role;
    std::vector<RelationRule> rules;

    [[nodiscard]] static RelationConstraints branching_normalized(composite::AspectId role_aspect) {
        RelationConstraints constraints;
        constraints.shape = RelationShape::Branching;
        constraints.distinguished_role = role_aspect;
        constraints.rules = {
            RelationRule::aspect_implies_existence(role_aspect),
            RelationRule::active_aspect_count(role_aspect, 1u),
            RelationRule::role_opposes_complement(role_aspect),
            RelationRule::role_magnitude(role_aspect, numeric::Rational{1}),
            RelationRule::complement_positive(role_aspect),
            RelationRule::complement_magnitude_sum(role_aspect, numeric::Rational{1})
        };
        return constraints;
    }
};

struct RelationType {
    RelationTypeId id{};
    std::string name;
    BoundaryPolicy boundary_policy{BoundaryPolicy::DeclaredExact};
    std::size_t min_arity{0};
    std::size_t max_arity{0}; // zero means unbounded
    RelationConstraints constraints{};
    CompositeConstructorKind constructor{CompositeConstructorKind::None};

    [[nodiscard]] bool is_branching() const noexcept {
        return constraints.shape == RelationShape::Branching;
    }
    [[nodiscard]] bool is_attaching_loop() const noexcept {
        return constraints.shape == RelationShape::AttachingLoop;
    }
    [[nodiscard]] std::optional<composite::AspectId> distinguished_role_aspect() const noexcept {
        return constraints.distinguished_role;
    }
};

class RelationSignature {
public:
    RelationSignature() : aspects_(composite::AspectSchema::canonical()) {}
    explicit RelationSignature(composite::AspectSchema aspects) : aspects_(std::move(aspects)) {
        require_signed_incidence_aspects();
    }

    [[nodiscard]] composite::AspectSchema& aspects() noexcept { return aspects_; }
    [[nodiscard]] const composite::AspectSchema& aspects() const noexcept { return aspects_; }
    [[nodiscard]] composite::AspectId existence_aspect() const {
        const auto id = aspects_.unique_aspect_with_role(composite::AspectRole::Existence);
        if (!id) throw std::logic_error("relation signature has no existence aspect");
        return *id;
    }
    [[nodiscard]] composite::AspectId orientation_aspect() const {
        const auto id = aspects_.unique_aspect_with_role(composite::AspectRole::Orientation);
        if (!id) throw std::logic_error("relation signature has no orientation aspect");
        return *id;
    }

    RelationTypeId add_type(std::string name,
                            BoundaryPolicy policy = BoundaryPolicy::DeclaredExact,
                            std::size_t min_arity = 0,
                            std::size_t max_arity = 0,
                            RelationConstraints constraints = {},
                            CompositeConstructorKind constructor = CompositeConstructorKind::None) {
        if (name.empty()) throw std::invalid_argument("relation type name must not be empty");
        if (by_name_.contains(name)) throw std::invalid_argument("duplicate relation type name");
        if (max_arity != 0 && min_arity > max_arity) throw std::invalid_argument("invalid arity bounds");
        switch (policy) {
            case BoundaryPolicy::DeclaredExact: case BoundaryPolicy::CanonicalComposite:
            case BoundaryPolicy::Witness: case BoundaryPolicy::Algebraic: break;
            default: throw std::invalid_argument("unknown boundary policy");
        }
        switch (constructor) {
            case CompositeConstructorKind::None: case CompositeConstructorKind::NormalizedOneHead: break;
            default: throw std::invalid_argument("unknown composite constructor");
        }
        if (types_.size() >= std::numeric_limits<RelationTypeId>::max())
            throw std::overflow_error("too many relation types");
        validate_constraints(constraints);
        const auto id = static_cast<RelationTypeId>(types_.size());
        types_.push_back(RelationType{id, std::move(name), policy, min_arity, max_arity,
                                      std::move(constraints), constructor});
        by_name_.emplace(types_.back().name, id);
        return id;
    }

    RelationTypeId add_branch_type(std::string name,
                                   std::size_t min_arity = 2,
                                   std::size_t max_arity = 0,
                                   BoundaryPolicy policy = BoundaryPolicy::CanonicalComposite) {
        const auto role = aspects_.ensure("head", composite::AspectRole::RelationRole, false);
        return add_branch_type_using_role(std::move(name), role, min_arity, max_arity, policy);
    }

    RelationTypeId add_branch_type_using_role(std::string name,
                                              composite::AspectId role_aspect,
                                              std::size_t min_arity = 2,
                                              std::size_t max_arity = 0,
                                              BoundaryPolicy policy = BoundaryPolicy::CanonicalComposite) {
        const auto& definition = aspects_.at(role_aspect);
        if (definition.role != composite::AspectRole::RelationRole) {
            throw std::invalid_argument("branch distinguished aspect must be a relation-role aspect");
        }
        return add_type(std::move(name), policy, min_arity, max_arity,
                        RelationConstraints::branching_normalized(role_aspect),
                        CompositeConstructorKind::NormalizedOneHead);
    }

    RelationTypeId add_attaching_loop_type(std::string name,
                                           std::size_t min_arity = 1,
                                           std::size_t max_arity = 0) {
        RelationConstraints constraints;
        constraints.shape = RelationShape::AttachingLoop;
        return add_type(std::move(name), BoundaryPolicy::DeclaredExact, min_arity, max_arity,
                        std::move(constraints));
    }

    [[nodiscard]] const RelationType& type(RelationTypeId id) const { return types_.at(id); }
    [[nodiscard]] RelationTypeId type_id(std::string_view name) const {
        const auto it = by_name_.find(std::string(name));
        if (it == by_name_.end()) throw std::out_of_range("unknown relation type");
        return it->second;
    }
    [[nodiscard]] std::size_t type_count() const noexcept { return types_.size(); }

private:
    void require_signed_incidence_aspects() const {
        try {
            const auto exists = aspects_.unique_aspect_with_role(composite::AspectRole::Existence);
            const auto orientation = aspects_.unique_aspect_with_role(composite::AspectRole::Orientation);
            if (!exists) throw std::invalid_argument("relation signature requires declared existence aspect");
            if (!orientation) throw std::invalid_argument("relation signature requires declared orientation aspect");
        } catch (const std::logic_error&) {
            throw std::invalid_argument("existence/orientation roles must be unique in relation signature");
        }
    }

    void validate_constraints(const RelationConstraints& constraints) const {
        switch (constraints.shape) {
            case RelationShape::Generic: case RelationShape::Branching: case RelationShape::AttachingLoop:
            case RelationShape::Witness: case RelationShape::Algebraic: break;
            default: throw std::invalid_argument("unknown relation shape");
        }
        if (constraints.distinguished_role) {
            const auto& definition = aspects_.at(*constraints.distinguished_role);
            if (definition.role != composite::AspectRole::RelationRole) {
                throw std::invalid_argument("distinguished relation role must reference a relation-role aspect");
            }
        }
        for (const auto& rule : constraints.rules) {
            switch (rule.kind) {
                case RelationRuleKind::AspectImpliesExistence: case RelationRuleKind::ActiveAspectCountEquals:
                case RelationRuleKind::AspectOrientationOpposesActiveComplement: case RelationRuleKind::AspectMagnitudeEquals:
                case RelationRuleKind::ActiveComplementMagnitudePositive: case RelationRuleKind::ActiveComplementMagnitudeSumEquals: break;
                default: throw std::invalid_argument("unknown executable relation rule");
            }
            (void)aspects_.at(rule.aspect);
            if ((rule.kind == RelationRuleKind::AspectMagnitudeEquals ||
                 rule.kind == RelationRuleKind::ActiveComplementMagnitudeSumEquals) && rule.value < 0) {
                throw std::invalid_argument("relation magnitude rule requires nonnegative exact value");
            }
        }
    }

    composite::AspectSchema aspects_;
    std::vector<RelationType> types_;
    std::unordered_map<std::string, RelationTypeId> by_name_;
};

} // namespace rfc::core
