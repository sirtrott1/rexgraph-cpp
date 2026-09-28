#include "rfc/core/axioms.hpp"
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/core/builder.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <class F>
static void require_throws(F&& fn, const char* message) {
    bool threw = false;
    try { fn(); } catch (const std::exception&) { threw = true; }
    if (!threw) throw std::runtime_error(message);
}

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;

        // A bare relational signature has only universal signed-incidence
        // aspects. Relation roles are introduced only by types that use them.
        core::RelationSignature bare;
        require(!bare.aspects().contains("head"), "head leaked into the universal aspect schema");

        // Arbitrary binary relation roles are signature data, not core code.
        core::RelationSignature role_sig;
        const auto source_role = role_sig.aspects().add(
            "source-role", composite::AspectRole::RelationRole, false);
        core::RelationConstraints source_constraints;
        source_constraints.distinguished_role = source_role;
        source_constraints.rules.push_back(core::RelationRule::aspect_implies_existence(source_role));
        source_constraints.rules.push_back(core::RelationRule::active_aspect_count(source_role, 1u));
        const auto source_typed = role_sig.add_type(
            "source-typed", core::BoundaryPolicy::DeclaredExact, 2, 2, source_constraints);
        core::RelationalComplexBuilder role_builder(role_sig);
        role_builder.add_grade_one(source_typed, {
            {1,true,true,R{1},{{source_role,true}}},
            {2,true,false,R{1}}});
        const auto role_complex = role_builder.freeze();
        const auto& role_arena = role_complex.grade(core::Grade{1});
        require(role_arena.aspects.get(source_role, 0), "custom relation-role aspect was not retained");
        require(!role_arena.aspects.get(source_role, 1), "custom role default was not retained");
        require_throws([&] {
            core::RelationalComplexBuilder bad(role_sig);
            bad.add_grade_one(source_typed, {
                {1,true,true,R{1},{{source_role,true}}},
                {2,true,false,R{1},{{source_role,true}}}});
        }, "signature-owned custom role rule was not enforced");

        // Branching itself is not tied to an aspect literally named "head".
        // The branch helper resolves the type-declared relation-role aspect.
        core::RelationSignature custom_branch_sig;
        const auto origin_role = custom_branch_sig.aspects().add(
            "origin", composite::AspectRole::RelationRole, false);
        const auto custom_branch = custom_branch_sig.add_branch_type_using_role(
            "weighted-origin-branch", origin_role, 4, 4);
        core::RelationalComplexBuilder custom_branch_builder(custom_branch_sig);
        custom_branch_builder.add_weighted_branch_grade_one(
            custom_branch, {10,11,12,13}, 0, {R{1,4},R{1,2},R{1,4}});
        const auto custom_branch_complex = custom_branch_builder.freeze();
        const core::BranchingRelationView custom_branch_view(custom_branch_complex, core::Grade{1}, 0);
        require(custom_branch_view.head_aspect() == origin_role,
                "custom branch role was not used by the branch view");
        require(custom_branch_view.tail_shares() == numeric::RationalVector({R{1,4},R{1,2},R{1,4}}),
                "weighted branch helper changed unequal exact shares");
        require(!custom_branch_sig.aspects().contains("head"),
                "custom branch role unexpectedly introduced a universal head aspect");

        core::RelationSignature sig;
        const auto branch = sig.add_branch_type("branch", 2, 0);
        const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        require(sig.aspects().contains("head"), "branch type did not declare its head role aspect");

        core::RelationalComplexBuilder builder(sig);
        builder.add_grade_one(branch, {
            {10,true,true,true,R{1}},
            {11,true,false,false,R{1,4}},
            {12,true,false,false,R{1,2}},
            {13,true,false,false,R{1,4}}});
        const auto complex = builder.freeze();
        core::BranchingRelationView view(complex, core::Grade{1}, 0);
        require(view.head_aspect() == sig.aspects().id("head"), "branch view did not use the type-declared head aspect");
        require(view.head().lower.index == 0u, "branch head occurrence mismatch");
        require(view.tails().size() == 3u, "branch tail count mismatch");
        require(view.tail_shares() == numeric::RationalVector({R{1,4},R{1,2},R{1,4}}), "declared branch shares not retained");
        require(view.boundary_column() == numeric::RationalVector({R{-1},R{1,4},R{1,2},R{1,4}}), "branch boundary column mismatch");

        // Global orientation reversal remains one valid branch rather than a
        // different relation type.
        core::RelationalComplexBuilder reverse_builder(sig);
        reverse_builder.add_grade_one(branch, {
            {10,true,false,true,R{1}},
            {11,true,true,false,R{1,2}},
            {12,true,true,false,R{1,2}}});
        const auto reversed = reverse_builder.freeze();
        require(core::BranchingRelationView(reversed, core::Grade{1}, 0).reversed(), "reversed branch orientation not recognized");

        require_throws([&] {
            core::RelationalComplexBuilder bad(sig);
            bad.add_grade_one(branch, {
                {1,true,true,true,R{1}},
                {2,true,false,true,R{1,2}},
                {3,true,false,false,R{1,2}}});
        }, "multiple-head branch was accepted");
        require_throws([&] {
            core::RelationalComplexBuilder bad(sig);
            bad.add_grade_one(branch, {
                {1,true,true,true,R{1}},
                {2,true,false,false,R{1,3}},
                {3,true,false,false,R{1,3}}});
        }, "non-normalized branch shares were accepted");
        require_throws([&] {
            core::RelationalComplexBuilder bad(sig);
            bad.add_grade_one(branch, {
                {1,true,true,true,R{1}},
                {2,true,true,false,R{1,2}},
                {3,true,false,false,R{1,2}}});
        }, "tail with head orientation was accepted");

        // Recursive branching through the tower: b0 is a grade-one branch,
        // e1/e2/e3 are its pair legs, and the grade-two branch closes them.
        core::RelationalComplexBuilder nested(sig);
        nested.add_grade_one(branch, {
            {100,true,true,true,R{1}},
            {101,true,false,false,R{1,3}},
            {102,true,false,false,R{1,3}},
            {103,true,false,false,R{1,3}}}); // b0
        nested.add_grade_one(edge, {{100,true,true,false,R{1}},{101,true,false,false,R{1}}});
        nested.add_grade_one(edge, {{100,true,true,false,R{1}},{102,true,false,false,R{1}}});
        nested.add_grade_one(edge, {{100,true,true,false,R{1}},{103,true,false,false,R{1}}});
        nested.add_canonical_higher(core::Grade{2}, branch, {0,1,2,3}, 0);
        const auto tower = nested.freeze();
        require(core::BranchingRelationView(tower, core::Grade{2}, 0).tails().size() == 3u, "higher-grade branch was not retained as branching");
        require(core::bianchi_through(tower, core::Grade{2}).satisfied(), "nested branch Bianchi closure failed");

        std::cout << "branching relation core tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
