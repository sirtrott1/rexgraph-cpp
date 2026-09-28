#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/exact.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::view {

// A graph is an explicit downstream geometric projection of retained relational
// structure.  This policy controls which grade-one primary relations are
// admitted to the pairwise directed projection.  It never changes the source
// RelationalComplex.
enum class NonPairwiseGraphPolicy : std::uint8_t {
    Reject,
    Omit
};

enum class GraphPairwiseRule : std::uint8_t {
    CanonicalSignedPair
};

enum class GraphOrientationRule : std::uint8_t {
    NegativeToPositive
};

enum class GraphMultiplicityRule : std::uint8_t {
    PreservePrimaryRelations
};

enum class GraphMeasureRule : std::uint8_t {
    Unit,
    DeclaredRelationMeasure
};

struct DirectedGraphProjectionPolicy {
    std::optional<core::RelationTypeId> relation_type;
    NonPairwiseGraphPolicy nonpairwise{NonPairwiseGraphPolicy::Reject};
};

struct GraphProjectionContract {
    core::Grade participating_grade{1};
    std::optional<core::RelationTypeId> selected_relation_type;
    GraphPairwiseRule pairwise_rule{GraphPairwiseRule::CanonicalSignedPair};
    GraphOrientationRule orientation_rule{GraphOrientationRule::NegativeToPositive};
    GraphMultiplicityRule multiplicity_rule{GraphMultiplicityRule::PreservePrimaryRelations};
    GraphMeasureRule measure_rule{GraphMeasureRule::DeclaredRelationMeasure};
    bool preserves_primary_relation_identity{true};
    bool preserves_parallel_relation_identity{true};
    bool preserves_object_identity{true};
    bool preserves_boundary_occurrence_identity{true};
    bool preserves_pair_orientation{true};
    bool preserves_relation_type{true};
    bool preserves_declared_edge_measure{true};
    bool has_exact_relation_field_lift{true};

    std::size_t source_grade_one_relations{};
    std::size_t projected_grade_one_relations{};
    std::size_t omitted_by_type_filter{};
    std::size_t omitted_nonpairwise_relations{};
    std::size_t omitted_higher_relations{};
    std::size_t unrepresented_composite_aspects{};

    [[nodiscard]] bool includes_all_grade_one_relations() const noexcept {
        return omitted_by_type_filter == 0 && omitted_nonpairwise_relations == 0 &&
               projected_grade_one_relations == source_grade_one_relations;
    }

    // Completeness here refers only to the graph-visible structural projection.
    // It never upgrades the graph to the canonical relational source: the
    // source signature, recursive relational algebra, and assertion semantics
    // remain owned by RelationalComplex.
    [[nodiscard]] bool graph_visible_structure_complete() const noexcept {
        return includes_all_grade_one_relations() && omitted_higher_relations == 0 &&
               unrepresented_composite_aspects == 0;
    }

    [[nodiscard]] constexpr bool can_reconstruct_relational_source() const noexcept {
        return false;
    }
};

struct ProjectedDirectedEdge {
    std::uint32_t source_relation{}; // compact source arena address, not identity
    core::RelationIdentity relation_identity{};
    core::RelationTypeId relation_type{};

    std::uint32_t source_object{};
    std::uint32_t target_object{};
    core::IdentityKey source_identity{};
    core::IdentityKey target_identity{};
    core::OccurrenceKey source_occurrence{};
    core::OccurrenceKey target_occurrence{};

    // Exact scalar geometric measure assigned by the projection.  This is a
    // graph-view measure, not a definition of the source relational metric.
    numeric::Rational measure{1};
};

class DirectedGraphProjection;

DirectedGraphProjection project_pairwise_directed_impl(
    const core::RelationalComplex&,
    const numeric::RationalVector&,
    DirectedGraphProjectionPolicy,
    GraphMeasureRule);

class DirectedGraphProjection {
public:
    [[nodiscard]] std::size_t object_count() const noexcept { return identities_.size(); }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }
    [[nodiscard]] std::size_t source_relation_count() const noexcept { return source_relation_count_; }
    [[nodiscard]] core::SemanticScale source_semantic_scale() const noexcept { return source_semantic_scale_; }
    [[nodiscard]] const GraphProjectionContract& contract() const noexcept { return contract_; }
    [[nodiscard]] const std::vector<ProjectedDirectedEdge>& edges() const noexcept { return edges_; }
    [[nodiscard]] const std::vector<core::IdentityKey>& object_identities() const noexcept { return identities_; }

    [[nodiscard]] std::uint32_t object_index(core::IdentityKey identity) const {
        const auto it = identity_to_object_.find(identity);
        if (it == identity_to_object_.end()) throw std::out_of_range("object identity not present in graph projection");
        return it->second;
    }

    [[nodiscard]] const std::vector<std::size_t>& outgoing(std::uint32_t object) const {
        return outgoing_.at(object);
    }

    [[nodiscard]] const ProjectedDirectedEdge& edge(std::size_t projected_edge) const {
        return edges_.at(projected_edge);
    }

    [[nodiscard]] std::optional<std::size_t> projected_edge_for_source_relation(
        std::size_t source_relation) const noexcept {
        if (source_relation >= edge_for_source_relation_.size()) return std::nullopt;
        const auto value = edge_for_source_relation_[source_relation];
        if (value == no_edge) return std::nullopt;
        return static_cast<std::size_t>(value);
    }

    // Exact lift of a graph-edge field back into the retained grade-one source
    // coordinate space.  This is lossless on the selected graph subspace and
    // does not claim to reconstruct omitted source structure.
    [[nodiscard]] numeric::SparseRationalVector lift_edge_field(
        numeric::SparseRationalView edge_field) const {
        if (edge_field.size() != edges_.size()) throw std::invalid_argument("graph edge field size mismatch");
        std::vector<numeric::SparseRationalEntry> lifted;
        lifted.reserve(edge_field.nonzeros());
        for (const auto& entry : edge_field.entries()) {
            lifted.push_back({edges_.at(entry.index).source_relation, entry.value});
        }
        return numeric::SparseRationalVector(source_relation_count_, std::move(lifted));
    }

    [[nodiscard]] numeric::SparseRationalVector lift_edge_field(
        const numeric::SparseRationalVector& edge_field) const {
        return lift_edge_field(edge_field.view());
    }

    // Restrict a retained grade-one field to the graph projection.  Active
    // amplitudes on omitted source relations are intentionally absent from the
    // returned graph field; the contract reports why they were omitted.
    [[nodiscard]] numeric::SparseRationalVector project_relation_field(
        numeric::SparseRationalView relation_field) const {
        if (relation_field.size() != source_relation_count_) throw std::invalid_argument("source relation field size mismatch");
        std::vector<numeric::SparseRationalEntry> projected;
        projected.reserve(relation_field.nonzeros());
        for (const auto& entry : relation_field.entries()) {
            const auto edge_index = projected_edge_for_source_relation(entry.index);
            if (edge_index) projected.push_back({*edge_index, entry.value});
        }
        return numeric::SparseRationalVector(edges_.size(), std::move(projected));
    }

    [[nodiscard]] numeric::SparseRationalVector project_relation_field(
        const numeric::SparseRationalVector& relation_field) const {
        return project_relation_field(relation_field.view());
    }

private:
    static constexpr std::uint32_t no_edge = std::numeric_limits<std::uint32_t>::max();

    friend DirectedGraphProjection project_pairwise_directed_impl(
        const core::RelationalComplex&,
        const numeric::RationalVector&,
        DirectedGraphProjectionPolicy,
        GraphMeasureRule);

    DirectedGraphProjection(core::SemanticScale semantic_scale,
                            std::size_t source_relation_count,
                            GraphProjectionContract contract,
                            std::vector<ProjectedDirectedEdge> edges,
                            std::vector<core::IdentityKey> identities,
                            std::vector<std::vector<std::size_t>> outgoing,
                            std::vector<std::uint32_t> edge_for_source_relation)
        : source_semantic_scale_(semantic_scale),
          source_relation_count_(source_relation_count),
          contract_(std::move(contract)),
          edges_(std::move(edges)),
          identities_(std::move(identities)),
          outgoing_(std::move(outgoing)),
          edge_for_source_relation_(std::move(edge_for_source_relation)) {
        identity_to_object_.reserve(identities_.size());
        for (std::size_t i = 0; i < identities_.size(); ++i) {
            identity_to_object_.emplace(identities_[i], static_cast<std::uint32_t>(i));
        }
    }

    core::SemanticScale source_semantic_scale_{};
    std::size_t source_relation_count_{};
    GraphProjectionContract contract_{};
    std::vector<ProjectedDirectedEdge> edges_;
    std::vector<core::IdentityKey> identities_;
    std::unordered_map<core::IdentityKey, std::uint32_t> identity_to_object_;
    std::vector<std::vector<std::size_t>> outgoing_;
    std::vector<std::uint32_t> edge_for_source_relation_;
};

inline DirectedGraphProjection project_pairwise_directed_impl(
    const core::RelationalComplex& complex,
    const numeric::RationalVector& relation_measure,
    DirectedGraphProjectionPolicy policy,
    GraphMeasureRule measure_rule) {
    if (complex.grade_count() < 2) throw std::invalid_argument("directed graph projection requires grade one");
    const auto grade = core::Grade{1};
    const auto& relations = complex.grade(grade);
    const auto& objects = complex.grade(core::Grade{0});
    if (relation_measure.size() != relations.cell_count()) {
        throw std::invalid_argument("graph relation-measure size mismatch");
    }
    for (const auto& measure : relation_measure) {
        if (measure < 0) throw std::invalid_argument("graph projection requires nonnegative relation measures");
    }

    GraphProjectionContract contract;
    contract.selected_relation_type = policy.relation_type;
    contract.measure_rule = measure_rule;
    contract.source_grade_one_relations = relations.cell_count();
    for (std::size_t k = 2; k < complex.grade_count(); ++k) {
        contract.omitted_higher_relations += complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
    }
    const auto aspect_count = complex.signature().aspects().size();
    contract.unrepresented_composite_aspects = aspect_count > 2u ? aspect_count - 2u : 0u;

    std::vector<ProjectedDirectedEdge> edges;
    edges.reserve(relations.cell_count());
    std::vector<std::vector<std::size_t>> outgoing(objects.cell_count());
    std::vector<std::uint32_t> edge_for_source_relation(relations.cell_count(),
                                                        std::numeric_limits<std::uint32_t>::max());

    for (std::size_t relation = 0; relation < relations.cell_count(); ++relation) {
        const auto relation_type = relations.relation_types.at(relation);
        if (policy.relation_type && relation_type != *policy.relation_type) {
            ++contract.omitted_by_type_filter;
            continue;
        }

        const auto [begin, end] = relations.incidence_range(relation);
        std::optional<std::size_t> source_slot;
        std::optional<std::size_t> target_slot;
        std::size_t active_nonzero = 0;
        bool canonical_pair = true;
        for (std::size_t p = begin; p < end; ++p) {
            const auto coefficient = complex.incidence_coefficient(grade, p);
            if (coefficient == 0) continue;
            ++active_nonzero;
            if (coefficient == -1) {
                if (source_slot) canonical_pair = false;
                else source_slot = p;
            } else if (coefficient == 1) {
                if (target_slot) canonical_pair = false;
                else target_slot = p;
            } else {
                canonical_pair = false;
            }
        }
        canonical_pair = canonical_pair && active_nonzero == 2 && source_slot && target_slot;
        if (!canonical_pair) {
            if (policy.nonpairwise == NonPairwiseGraphPolicy::Reject) {
                throw std::invalid_argument("selected relation is not a canonical directed pair; declare omission explicitly to project it away");
            }
            ++contract.omitted_nonpairwise_relations;
            continue;
        }

        if (edges.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            throw std::overflow_error("too many projected graph edges");
        }
        const auto projected_index = static_cast<std::uint32_t>(edges.size());
        const auto source_object = relations.lower_index.at(*source_slot);
        const auto target_object = relations.lower_index.at(*target_slot);
        edges.push_back(ProjectedDirectedEdge{
            static_cast<std::uint32_t>(relation),
            complex.relation_identity(core::RelationId{grade, static_cast<std::uint32_t>(relation)}),
            relation_type,
            source_object,
            target_object,
            objects.identities.at(source_object),
            objects.identities.at(target_object),
            complex.occurrence_identity(grade, *source_slot),
            complex.occurrence_identity(grade, *target_slot),
            relation_measure.at(relation)
        });
        outgoing.at(source_object).push_back(edges.size() - 1u);
        edge_for_source_relation[relation] = projected_index;
    }

    contract.projected_grade_one_relations = edges.size();
    return DirectedGraphProjection(complex.semantic_scale(), relations.cell_count(), std::move(contract),
                                   std::move(edges), objects.identities, std::move(outgoing),
                                   std::move(edge_for_source_relation));
}

inline DirectedGraphProjection project_pairwise_directed(
    const core::RelationalComplex& complex,
    const numeric::RationalVector& relation_measure,
    DirectedGraphProjectionPolicy policy = {}) {
    return project_pairwise_directed_impl(complex, relation_measure, std::move(policy),
                                          GraphMeasureRule::DeclaredRelationMeasure);
}

inline DirectedGraphProjection project_pairwise_directed(
    const core::RelationalComplex& complex,
    DirectedGraphProjectionPolicy policy = {}) {
    if (complex.grade_count() < 2) throw std::invalid_argument("directed graph projection requires grade one");
    numeric::RationalVector unit_measure(complex.grade(core::Grade{1}).cell_count(), numeric::Rational{1});
    return project_pairwise_directed_impl(complex, unit_measure, std::move(policy), GraphMeasureRule::Unit);
}

} // namespace rfc::view
