#pragma once
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/core/relational_complex.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace rfc::topology {

// Stable source identity of a retained cell participating in a higher
// attachment. Grade zero keys are derived/shared object identities; positive
// grade keys are asserted primary relation identities. The grade keeps those
// namespaces distinct without flattening either into a graph vertex.
struct RetainedCellIdentity {
    core::Grade grade{};
    std::uint64_t key{};
    constexpr auto operator<=>(const RetainedCellIdentity&) const = default;
};

inline RetainedCellIdentity retained_cell_identity(const core::RelationalComplex& complex,
                                                    core::CellId cell) {
    if (cell.grade.value >= complex.grade_count() ||
        cell.index >= complex.grade(cell.grade).cell_count()) {
        throw std::out_of_range("retained cell identity");
    }
    if (cell.grade.value == 0) {
        return RetainedCellIdentity{cell.grade, complex.grade(cell.grade).identities.at(cell.index)};
    }
    return RetainedCellIdentity{
        cell.grade,
        complex.relation_identity(core::RelationId{cell.grade, cell.index}).key.value};
}

// Native view of one higher primary relation and its retained boundary fiber.
// This is deliberately more general than an attaching *word*: its lower
// participants may themselves be branching or otherwise nonpairwise primary
// relations, and its exact coefficients need not be unit signs.
class HigherAttachingRelationView {
public:
    HigherAttachingRelationView(const core::RelationalComplex& complex,
                                core::Grade grade,
                                std::size_t relation_index)
        : complex_(&complex), fiber_(complex, grade, relation_index) {
        if (grade.value < 2u) {
            throw std::invalid_argument("higher attaching relation requires grade at least two");
        }
    }

    [[nodiscard]] core::RelationId relation() const noexcept {
        return core::RelationId{fiber_.relation().grade, fiber_.relation().index};
    }
    [[nodiscard]] core::RelationIdentity identity() const {
        return complex_->relation_identity(relation());
    }
    [[nodiscard]] core::RelationTypeId relation_type() const { return fiber_.relation_type(); }
    [[nodiscard]] core::Grade grade() const noexcept { return relation().grade; }
    [[nodiscard]] const core::BoundaryOccurrenceFiber& fiber() const noexcept { return fiber_; }

    [[nodiscard]] RetainedCellIdentity participant_identity(std::size_t occurrence) const {
        return retained_cell_identity(*complex_, fiber_.occurrences().at(occurrence).lower);
    }

    [[nodiscard]] bool participant_is_branching(std::size_t occurrence) const {
        const auto lower = fiber_.occurrences().at(occurrence).lower;
        if (lower.grade.value == 0) return false;
        const auto& arena = complex_->grade(lower.grade);
        return complex_->signature().type(arena.relation_types.at(lower.index)).is_branching();
    }

    [[nodiscard]] bool has_branching_participant() const {
        for (std::size_t i = 0; i < fiber_.occurrences().size(); ++i) {
            const auto& occurrence = fiber_.occurrences()[i];
            if (occurrence.exists && participant_is_branching(i)) return true;
        }
        return false;
    }

private:
    const core::RelationalComplex* complex_{};
    core::BoundaryOccurrenceFiber fiber_;
};

inline HigherAttachingRelationView higher_attaching_relation(const core::RelationalComplex& complex,
                                                              core::Grade grade,
                                                              std::size_t relation_index) {
    return HigherAttachingRelationView(complex, grade, relation_index);
}

} // namespace rfc::topology
