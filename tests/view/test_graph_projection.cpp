#include "rfc/core/builder.hpp"
#include "rfc/view/graph_projection.hpp"
#include "rfc/field/exact_hodge.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
try {
    using namespace rfc;
    using numeric::Rational;
    using numeric::RationalVector;

    // Parallel primary relations remain distinct projected edges and lift back
    // to their distinct retained source coordinates.
    core::RelationSignature pair_signature;
    const auto edge_type = pair_signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder parallel_builder(pair_signature);
    parallel_builder.add_grade_one(core::RelationKey{1001}, edge_type,
                                   {{10,true,true,Rational{1}, {}, core::OccurrenceKey{5001}},
                                    {11,true,false,Rational{1}, {}, core::OccurrenceKey{5002}}});
    parallel_builder.add_grade_one(core::RelationKey{1002}, edge_type,
                                   {{10,true,true,Rational{1}, {}, core::OccurrenceKey{5003}},
                                    {11,true,false,Rational{1}, {}, core::OccurrenceKey{5004}}});
    const auto parallel = parallel_builder.freeze();
    const auto graph = view::project_pairwise_directed(parallel, RationalVector{2,5});
    require(graph.edge_count() == 2, "parallel primary relations collapsed in graph projection");
    require(graph.edge(0).relation_identity.key == core::RelationKey{1001} &&
            graph.edge(1).relation_identity.key == core::RelationKey{1002},
            "graph projection lost primary relation identity");
    require(graph.edge(0).source_occurrence == core::OccurrenceKey{5001} &&
            graph.edge(0).target_occurrence == core::OccurrenceKey{5002},
            "graph projection lost occurrence identity");
    require(graph.edge(0).measure == 2 && graph.edge(1).measure == 5,
            "graph projection lost exact edge measure");
    require(graph.contract().participating_grade == core::Grade{1} &&
            graph.contract().pairwise_rule == view::GraphPairwiseRule::CanonicalSignedPair &&
            graph.contract().orientation_rule == view::GraphOrientationRule::NegativeToPositive &&
            graph.contract().multiplicity_rule == view::GraphMultiplicityRule::PreservePrimaryRelations &&
            graph.contract().measure_rule == view::GraphMeasureRule::DeclaredRelationMeasure,
            "graph projection contract did not record its declared projection rules");
    const auto selected = numeric::SparseRationalVector::basis(2, 1, Rational{3});
    const auto lifted = graph.lift_edge_field(selected);
    require(lifted.dense() == RationalVector({0,3}), "graph edge lift did not preserve source relation coordinate");
    require(graph.project_relation_field(lifted).dense() == RationalVector({0,3}),
            "source relation field did not round-trip through selected graph subspace");
    require(graph.contract().graph_visible_structure_complete(),
            "pure pairwise grade-one projection should be complete over graph-visible structure");
    require(!graph.contract().can_reconstruct_relational_source(),
            "graph projection must never advertise lossless reconstruction of the relational source");

    // Higher relations are retained by the source but explicitly omitted by
    // the graph projection.  The grade-one graph can therefore be identical
    // while the relational complex is not.
    core::RelationSignature triangle_signature;
    const auto triangle_edge = triangle_signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face_type = triangle_signature.add_type("face", core::BoundaryPolicy::DeclaredExact, 3, 3);
    auto add_triangle_edges = [&](core::RelationalComplexBuilder& builder) {
        builder.add_grade_one(triangle_edge, {{0,true,true,false,1},{1,true,false,false,1}});
        builder.add_grade_one(triangle_edge, {{1,true,true,false,1},{2,true,false,false,1}});
        builder.add_grade_one(triangle_edge, {{0,true,true,false,1},{2,true,false,false,1}});
    };
    core::RelationalComplexBuilder open_triangle_builder(triangle_signature);
    add_triangle_edges(open_triangle_builder);
    const auto open_triangle = open_triangle_builder.freeze();

    core::RelationalComplexBuilder triangle_builder(triangle_signature);
    add_triangle_edges(triangle_builder);
    triangle_builder.add_higher(core::Grade{2}, face_type,
                                {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    const auto filled_triangle = triangle_builder.freeze();

    const auto open_graph = view::project_pairwise_directed(open_triangle);
    const auto triangle_graph = view::project_pairwise_directed(filled_triangle);
    require(open_graph.edge_count() == triangle_graph.edge_count() && triangle_graph.edge_count() == 3,
            "open and filled triangle graph projections differ in edge count");
    for (std::size_t i = 0; i < open_graph.edge_count(); ++i) {
        const auto& a = open_graph.edge(i);
        const auto& b = triangle_graph.edge(i);
        require(a.source_identity == b.source_identity && a.target_identity == b.target_identity &&
                a.relation_identity == b.relation_identity && a.measure == b.measure,
                "open and filled triangle did not produce the same lower graph projection");
    }
    require(open_graph.contract().omitted_higher_relations == 0 &&
            triangle_graph.contract().omitted_higher_relations == 1,
            "graph projection did not distinguish retained higher-attachment loss");
    require(!triangle_graph.contract().graph_visible_structure_complete(),
            "graph projection incorrectly claimed graph-visible completeness after dropping a face");

    const std::vector<field::ExactMetric> open_metrics{
        field::ExactMetric::identity(3), field::ExactMetric::identity(3)};
    const std::vector<field::ExactMetric> filled_metrics{
        field::ExactMetric::identity(3), field::ExactMetric::identity(3), field::ExactMetric::identity(1)};
    const field::ExactHodgeOperator open_hodge(open_triangle, core::Grade{1}, open_metrics);
    const field::ExactHodgeOperator filled_hodge(filled_triangle, core::Grade{1}, filled_metrics);
    const RationalVector source{1,0,0};
    require(open_hodge.apply(source) != filled_hodge.apply(source),
            "identical graph projection hid no Hodge difference between open and filled triangle");

    // A branching relation cannot silently become graph edges.  Omission must
    // be declared, and the contract records the loss.
    core::RelationSignature mixed_signature;
    const auto mixed_edge = mixed_signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto branch_type = mixed_signature.add_branch_type("branch", 4, 4);
    core::RelationalComplexBuilder mixed_builder(mixed_signature);
    mixed_builder.add_grade_one(mixed_edge, {{20,true,true,false,1},{21,true,false,false,1}});
    mixed_builder.add_weighted_branch_grade_one(branch_type, {20,21,22,23}, 0,
                                                RationalVector{Rational{1,4},Rational{1,2},Rational{1,4}});
    const auto mixed = mixed_builder.freeze();

    bool strict_rejected = false;
    try {
        (void)view::project_pairwise_directed(mixed, RationalVector{7,9});
    } catch (const std::invalid_argument&) {
        strict_rejected = true;
    }
    require(strict_rejected, "graph projection silently flattened a nonpairwise primary relation");

    view::DirectedGraphProjectionPolicy omit_policy;
    omit_policy.nonpairwise = view::NonPairwiseGraphPolicy::Omit;
    const auto mixed_graph = view::project_pairwise_directed(mixed, RationalVector{7,9}, omit_policy);
    require(mixed_graph.edge_count() == 1 && mixed_graph.edge(0).source_relation == 0,
            "declared nonpairwise omission selected the wrong relation");
    require(mixed_graph.contract().omitted_nonpairwise_relations == 1,
            "graph projection did not report omitted branching relation");
    require(mixed_graph.contract().unrepresented_composite_aspects >= 1,
            "graph projection did not report unrepresented relation-role aspects");

    std::cout << "declared graph projection tests passed\n";
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return EXIT_FAILURE;
}
}
