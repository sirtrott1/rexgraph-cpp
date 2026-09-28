#pragma once
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/geometry/contract.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::geometry {

enum class ExpansionLegKind : std::uint8_t { Pair, Witness };

struct ExpansionLeg {
    ExpansionLegKind kind{ExpansionLegKind::Pair};
    std::uint32_t primary_relation{};
    std::uint32_t reference_lower{};
    std::optional<std::uint32_t> target_lower{}; // pair target; empty for witness
};

inline numeric::ExactMatrix exact_boundary_matrix(const core::RelationalComplex& complex,
                                                   core::Grade upper_grade) {
    if (upper_grade.value == 0 || upper_grade.value >= complex.grade_count()) {
        throw std::out_of_range("invalid boundary grade for column expansion");
    }
    const auto& upper = complex.grade(upper_grade);
    const auto& lower = complex.grade(core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)});
    numeric::ExactMatrix B(lower.cell_count(), upper.cell_count());
    const auto exists = complex.signature().existence_aspect();
    for (std::size_t j = 0; j < upper.cell_count(); ++j) {
        const auto [begin, end] = upper.incidence_range(j);
        for (std::size_t p = begin; p < end; ++p) {
            if (!upper.aspects.get(exists, p)) continue;
            const auto i = upper.lower_index[p];
            B.set(i, j, B.get(i, j) + complex.incidence_coefficient(upper_grade, p));
        }
    }
    return B;
}

struct RetainedExpandedField {
    numeric::RationalVector primary;
    numeric::RationalVector legs;
};

// Exact sparse pair/witness coordinate expansion B_k = P_k Lambda_k.
// The primary relation field remains retained.  Pair/witness legs are internal
// coordinates and are never promoted to independent primary relations here.
class GradeColumnExpansion {
public:
    [[nodiscard]] static constexpr GeometryDescriptor geometry_descriptor() noexcept {
        return {GeometryKind::ExpansionLift, "exact retained column expansion", true};
    }
    GradeColumnExpansion(const core::RelationalComplex& complex,
                         core::Grade grade,
                         std::vector<std::optional<std::uint32_t>> references = {})
        : complex_(&complex), grade_(grade) {
        if (grade.value == 0 || grade.value >= complex.grade_count()) {
            throw std::out_of_range("column expansion requires positive grade");
        }
        const auto& upper = complex.grade(grade);
        const auto& lower = complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)});
        if (!references.empty() && references.size() != upper.cell_count()) {
            throw std::invalid_argument("one optional expansion reference required per primary relation");
        }
        if (references.empty()) references.resize(upper.cell_count());

        boundary_ = exact_boundary_matrix(complex, grade);
        primary_offsets_.reserve(upper.cell_count() + 1u);
        primary_offsets_.push_back(0u);
        references_.resize(upper.cell_count());

        struct PendingLeg { ExpansionLeg metadata; numeric::Rational lambda; };
        std::vector<PendingLeg> pending;

        for (std::size_t relation = 0; relation < upper.cell_count(); ++relation) {
            numeric::RationalVector b(lower.cell_count(), numeric::Rational{0});
            for (std::size_t i = 0; i < lower.cell_count(); ++i) b[i] = boundary_.get(i, relation);

            std::vector<std::uint32_t> support;
            for (std::size_t i = 0; i < b.size(); ++i) if (b[i] != 0) support.push_back(static_cast<std::uint32_t>(i));
            if (support.empty()) {
                // Zero boundary remains one retained primary relation with an empty internal expansion.
                primary_offsets_.push_back(pending.size());
                continue;
            }

            std::uint32_t reference{};
            if (references[relation]) {
                reference = *references[relation];
                if (reference >= lower.cell_count() || b[reference] == 0) {
                    throw std::invalid_argument("expansion reference must be in aggregated boundary support");
                }
            } else {
                // Prefer a semantic branch head when it remains supported; otherwise use first support.
                const auto& type = complex.signature().type(upper.relation_types[relation]);
                if (type.is_branching()) {
                    core::BranchingRelationView branch(complex, grade, relation);
                    const auto head = branch.head().lower.index;
                    reference = (head < b.size() && b[head] != 0) ? head : support.front();
                } else {
                    reference = support.front();
                }
            }
            references_[relation] = reference;

            for (const auto i : support) {
                if (i == reference) continue;
                pending.push_back(PendingLeg{
                    ExpansionLeg{ExpansionLegKind::Pair, static_cast<std::uint32_t>(relation), reference, i},
                    b[i]});
            }
            numeric::Rational omega = 0;
            for (const auto& value : b) omega += value;
            omega.canonicalize();
            if (omega != 0) {
                pending.push_back(PendingLeg{
                    ExpansionLeg{ExpansionLegKind::Witness, static_cast<std::uint32_t>(relation), reference, std::nullopt},
                    omega});
            }
            primary_offsets_.push_back(pending.size());
        }

        dictionary_ = numeric::ExactMatrix(lower.cell_count(), pending.size());
        lift_ = numeric::ExactMatrix(pending.size(), upper.cell_count());
        legs_.reserve(pending.size());
        for (std::size_t a = 0; a < pending.size(); ++a) {
            const auto& p = pending[a];
            if (p.metadata.kind == ExpansionLegKind::Pair) {
                dictionary_.set(*p.metadata.target_lower, a, numeric::Rational{1});
                dictionary_.set(p.metadata.reference_lower, a, numeric::Rational{-1});
            } else {
                dictionary_.set(p.metadata.reference_lower, a, numeric::Rational{1});
            }
            lift_.set(a, p.metadata.primary_relation, p.lambda);
            legs_.push_back(p.metadata);
        }
        if (!certificate_exact()) throw std::logic_error("internal exact column expansion certificate failed");
    }

    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const numeric::ExactMatrix& boundary() const noexcept { return boundary_; }
    [[nodiscard]] const numeric::ExactMatrix& dictionary() const noexcept { return dictionary_; }
    [[nodiscard]] const numeric::ExactMatrix& lift_matrix() const noexcept { return lift_; }
    [[nodiscard]] const std::vector<ExpansionLeg>& legs() const noexcept { return legs_; }
    [[nodiscard]] const std::vector<std::optional<std::uint32_t>>& references() const noexcept { return references_; }
    [[nodiscard]] std::pair<std::size_t,std::size_t> leg_range(std::size_t primary) const {
        if (primary + 1u >= primary_offsets_.size()) throw std::out_of_range("primary relation index");
        return {primary_offsets_[primary], primary_offsets_[primary + 1u]};
    }

    [[nodiscard]] bool certificate_exact() const { return dictionary_.multiply(lift_) == boundary_; }

    [[nodiscard]] RetainedExpandedField expand(const numeric::RationalVector& primary) const {
        if (primary.size() != boundary_.cols()) throw std::invalid_argument("primary field size mismatch");
        return RetainedExpandedField{primary, lift_.apply(primary)};
    }

    [[nodiscard]] bool consistent(const RetainedExpandedField& field) const {
        return field.primary.size() == boundary_.cols()
            && field.legs.size() == lift_.rows()
            && lift_.apply(field.primary) == field.legs;
    }

    [[nodiscard]] numeric::RationalVector boundary_from_expanded(const RetainedExpandedField& field) const {
        if (!consistent(field)) throw std::invalid_argument("expanded field violates retained primary lift");
        return dictionary_.apply(field.legs);
    }

    [[nodiscard]] bool primary_reconstructible_from_legs() const { return lift_.rank() == lift_.cols(); }

    [[nodiscard]] numeric::ExactMatrix primary_reconstruction() const {
        if (!primary_reconstructible_from_legs()) {
            throw std::runtime_error("leg coordinates do not reconstruct every retained primary amplitude");
        }
        const auto gram = lift_.transpose().multiply(lift_);
        return gram.inverse().multiply(lift_.transpose());
    }

    [[nodiscard]] numeric::ExactMatrix leg_metric(const numeric::ExactMatrix& lower_metric) const {
        if (lower_metric.rows() != boundary_.rows() || lower_metric.cols() != boundary_.rows()) {
            throw std::invalid_argument("lower metric dimension mismatch");
        }
        return dictionary_.transpose().multiply(lower_metric).multiply(dictionary_);
    }

    [[nodiscard]] numeric::ExactMatrix primary_metric_pullback(const numeric::ExactMatrix& lower_metric) const {
        const auto G = leg_metric(lower_metric);
        return lift_.transpose().multiply(G).multiply(lift_);
    }

    [[nodiscard]] numeric::ExactMatrix direct_primary_metric(const numeric::ExactMatrix& lower_metric) const {
        return boundary_.transpose().multiply(lower_metric).multiply(boundary_);
    }

private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    numeric::ExactMatrix boundary_;
    numeric::ExactMatrix dictionary_;
    numeric::ExactMatrix lift_;
    std::vector<ExpansionLeg> legs_;
    std::vector<std::size_t> primary_offsets_;
    std::vector<std::optional<std::uint32_t>> references_;
};

} // namespace rfc::geometry
