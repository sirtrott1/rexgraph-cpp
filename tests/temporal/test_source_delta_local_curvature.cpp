#include "rfc/core/builder.hpp"
#include "rfc/field/connection.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/temporal/source_delta.hpp"
#include "rfc/temporal/cylinder.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TemporalFixture {
    rfc::core::RelationalComplex old_source;
    rfc::core::RelationalComplex new_source;
};

static TemporalFixture make_head_change_fixture() {
    using namespace rfc;
    using R = numeric::Rational;

    core::RelationSignature sig;
    const auto branch = sig.add_branch_type("branch", 4, 4);
    const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto closure = sig.add_type("closure", core::BoundaryPolicy::DeclaredExact, 4, 4);
    const auto head = sig.aspects().id("head");

    auto build = [&](bool changed) {
        core::RelationalComplexBuilder b(sig);
        if (!changed) {
            b.add_grade_one(core::RelationKey{100}, branch, {
                {10,true,true,R{1},{{head,true}},core::OccurrenceKey{1000}},
                {11,true,false,R{1,3},{},core::OccurrenceKey{1001}},
                {12,true,false,R{1,3},{},core::OccurrenceKey{1002}},
                {13,true,false,R{1,3},{},core::OccurrenceKey{1003}}});
        } else {
            b.add_grade_one(core::RelationKey{100}, branch, {
                {10,true,false,R{1,3},{},core::OccurrenceKey{1000}},
                {11,true,true,R{1},{{head,true}},core::OccurrenceKey{1001}},
                {12,true,false,R{1,3},{},core::OccurrenceKey{1002}},
                {13,true,false,R{1,3},{},core::OccurrenceKey{1003}}});
        }
        b.add_grade_one(core::RelationKey{101}, edge, {
            {10,true,true,R{1},{},core::OccurrenceKey{1100}},
            {11,true,false,R{1},{},core::OccurrenceKey{1101}}});
        b.add_grade_one(core::RelationKey{102}, edge, {
            {10,true,true,R{1},{},core::OccurrenceKey{1102}},
            {12,true,false,R{1},{},core::OccurrenceKey{1103}}});
        b.add_grade_one(core::RelationKey{103}, edge, {
            {10,true,true,R{1},{},core::OccurrenceKey{1104}},
            {13,true,false,R{1},{},core::OccurrenceKey{1105}}});

        if (!changed) {
            b.add_higher(core::Grade{2}, core::RelationKey{200}, closure, {
                {0,true,true,R{1},{},core::OccurrenceKey{2000}},
                {1,true,false,R{1,3},{},core::OccurrenceKey{2001}},
                {2,true,false,R{1,3},{},core::OccurrenceKey{2002}},
                {3,true,false,R{1,3},{},core::OccurrenceKey{2003}}});
        } else {
            b.add_higher(core::Grade{2}, core::RelationKey{200}, closure, {
                {0,true,true,R{1},{},core::OccurrenceKey{2000}},
                {1,true,true,R{1},{},core::OccurrenceKey{2001}},
                {2,true,false,R{1,3},{},core::OccurrenceKey{2002}},
                {3,true,false,R{1,3},{},core::OccurrenceKey{2003}}});
        }
        return b.freeze();
    };

    return {build(false), build(true)};
}

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;
        const auto fixture = make_head_change_fixture();
        const temporal::RelationalSourceDelta delta(fixture.old_source, fixture.new_source);

        require(delta.grades().size() == 3u, "temporal source delta grade count mismatch");
        require(delta.grade(core::Grade{0}).born_cells.empty() && delta.grade(core::Grade{0}).removed_cells.empty(),
                "head change should not create or remove boundary identities");
        require(delta.grade(core::Grade{1}).born_cells.empty() && delta.grade(core::Grade{1}).removed_cells.empty(),
                "head change should retain primary relation identities");
        require(delta.grade(core::Grade{1}).persistent_occurrences.size() == 10u,
                "grade-one occurrence identity alignment mismatch");

        const auto head_aspect = fixture.old_source.signature().aspects().id("head");
        const auto orientation = fixture.old_source.signature().orientation_aspect();
        require(delta.grade(core::Grade{1}).aspect_xor.at(head_aspect).count() == 2u,
                "head-role XOR did not isolate the two changed occurrences");
        require(delta.grade(core::Grade{1}).aspect_xor.at(orientation).count() == 2u,
                "orientation XOR did not isolate the two changed occurrences");

        bool saw_head_old = false;
        bool saw_head_new = false;
        for (const auto& change : delta.grade(core::Grade{1}).occurrence_changes) {
            if (change.occurrence.value == 1000u) {
                saw_head_old = true;
                require(change.old_coefficient == R{-1} && change.new_coefficient == R{1,3},
                        "old-head occurrence coefficient delta mismatch");
                require(change.composite.aggregate == R{4,3}, "old-head composite delta mismatch");
            }
            if (change.occurrence.value == 1001u) {
                saw_head_new = true;
                require(change.old_coefficient == R{1,3} && change.new_coefficient == R{-1},
                        "new-head occurrence coefficient delta mismatch");
                require(change.composite.aggregate == R{-4,3}, "new-head composite delta mismatch");
            }
        }
        require(saw_head_old && saw_head_new, "head-change occurrence witnesses missing");

        const auto J0 = delta.correspondence(core::Grade{0});
        const auto J1 = delta.correspondence(core::Grade{1});
        const auto J2 = delta.correspondence(core::Grade{2});
        require(J0 == numeric::ExactMatrix::identity(4) &&
                J1 == numeric::ExactMatrix::identity(4) &&
                J2 == numeric::ExactMatrix::identity(1),
                "stable-key temporal correspondence should be identity on retained source");

        const auto B1 = field::boundary_matrix(fixture.old_source, core::Grade{1});
        const auto B1p = field::boundary_matrix(fixture.new_source, core::Grade{1});
        const auto B2 = field::boundary_matrix(fixture.old_source, core::Grade{2});
        const auto B2p = field::boundary_matrix(fixture.new_source, core::Grade{2});
        const auto E1 = temporal::temporal_down_defect(B1p, B1, J1, J0);
        const auto E2 = temporal::temporal_down_defect(B2p, B2, J2, J1);
        require(E1.get(0,0) == R{4,3} && E1.get(1,0) == R{-4,3} && E1.nonzeros() == 2u,
                "grade-one temporal boundary defect mismatch");
        require(E2.get(1,0) == R{-4,3} && E2.nonzeros() == 1u,
                "grade-two repair defect mismatch");
        require(B1p.multiply(E2).add(E1.multiply(B2)).nonzeros() == 0u,
                "cross-grade temporal defect compatibility failed");

        const numeric::RationalVector persistent_field{2,3,5,7};
        require(numeric::is_zero(delta.field_innovation(core::Grade{1}, persistent_field, persistent_field)),
                "unchanged primary amplitudes were confused with structural change");

        // Local route channels may be individually nonzero while the complete
        // interface is exactly flat. A partial chart exposing one route is not
        // reclassified as curved merely because its partial sum is nonzero.
        const auto ideal = field::ExactConnectionTower::ideal(fixture.old_source);
        const auto global = ideal.global_route_defect_certificate(core::Grade{2});
        require(global.has_nonzero_channel(), "flat nested branch should retain nonzero route channels");
        require(global.local_channels_cancel(), "flat route channels should cancel only after complete assembly");
        require(!global.curved(), "flat complete interface was mislabeled curved");

        const auto partial = ideal.route_defect_certificate(core::Grade{2}, {0u}, field::RouteCoverage::PartialChart);
        require(partial.partial_nonzero_not_curvature(), "partial nonzero chart should remain a partial contribution, not curvature");
        require(!partial.curved(), "partial chart was mislabeled as global curvature");

        auto multiplier = numeric::ExactMatrix::from_rows({{2},{1},{1},{1}});
        const auto deformed = ideal.entrywise_scaled(core::Grade{2}, multiplier);
        const auto curved = deformed.global_route_defect_certificate(core::Grade{2});
        require(curved.curved(), "complete deformed interface did not expose exact curvature");

        std::cout << "temporal source delta and local/global defect tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
