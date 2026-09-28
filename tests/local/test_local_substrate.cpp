#include "rfc/algebra/morphism.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/geometry/semantic_scale.hpp"
#include "rfc/local/carrier.hpp"
#include "rfc/local/constraint_system.hpp"
#include "rfc/local/section.hpp"
#include "rfc/local/semantic_constraints.hpp"
#include "rfc/local/stalk_bundle.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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

static rfc::core::RelationSignature branch_signature() {
    rfc::core::RelationSignature sig;
    (void)sig.add_branch_type("branch", 3, 3);
    return sig;
}

static rfc::core::RelationalComplex branch_complex() {
    using R = rfc::numeric::Rational;
    auto sig = branch_signature();
    rfc::core::RelationalComplexBuilder b(sig);
    const auto branch = sig.type_id("branch");
    b.add_weighted_branch_grade_one(branch, {10,20,30}, 0, {R{1,4}, R{3,4}});
    b.add_weighted_branch_grade_one(branch, {40,50,60}, 0, {R{1,2}, R{1,2}});
    return b.freeze();
}

static rfc::core::RelationalComplex edge_complex(rfc::core::SemanticScale scale) {
    rfc::core::RelationSignature sig;
    const auto edge = sig.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder b(sig, scale);
    b.add_grade_one(edge, {{10,true,true,false,1},{20,true,false,false,1}});
    return b.freeze();
}

int main() {
    try {
        using namespace rfc;

        // The common local carrier has no dimension or matrix requirement.
        const auto branch = branch_complex();
        local::RelationalLocalCarrier carrier(branch);
        const core::CellId relation{core::Grade{1},0u};
        const auto patch = carrier.cell_patch(relation);
        local::BasicSection<std::string> labels{{relation,"branch-state"},
                                                {core::CellId{core::Grade{0},0u},"head"}};
        const auto restricted = local::restrict_section(carrier, labels, patch);
        require(restricted.size() == 2u, "value-neutral local restriction failed");

        // Linear dimensions remain a specialization rather than a common local
        // carrier requirement.  The old StalkBundle spelling is only an alias.
        local::LinearStalkBundle vector_stalks(branch, 2u);
        require(vector_stalks.dimension(relation) == 2u, "linear stalk specialization mismatch");

        // One branch constraint can bind the primary branch state and the whole
        // active occurrence fiber coherently.  It is not decomposed into pair charts.
        local::FiniteConstraintSystem<std::int64_t> branch_constraints(branch);
        const core::CellId v0{core::Grade{0},0u}, v1{core::Grade{0},1u}, v2{core::Grade{0},2u};
        for (const auto cell : {relation,v0,v1,v2}) branch_constraints.set_boolean_domain(cell);
        const auto support = branch_constraints.relation_fiber_support(core::RelationId{core::Grade{1},0u});
        require(support == std::vector<core::CellId>({relation,v0,v1,v2}), "branch fiber support order mismatch");
        branch_constraints.add_branch_fiber_chart("whole branch", core::RelationId{core::Grade{1},0u},
                                                  {{1,0,1,1}});
        local::FiniteConstraintSystem<std::int64_t>::Assignment good{{relation,1},{v0,0},{v1,1},{v2,1}};
        local::FiniteConstraintSystem<std::int64_t>::Assignment bad = good;
        bad[v2] = 0;
        require(branch_constraints.globally_valid(good), "whole-fiber branch constraint rejected valid state");
        require(!branch_constraints.globally_valid(bad), "whole-fiber branch constraint accepted split-tail state");

        // Constraint support can be selected directly from a declared composite
        // role without turning the branch into graph edges.
        const auto role = branch.signature().type(branch.grade(core::Grade{1}).relation_types[0]).distinguished_role_aspect();
        require(role.has_value(), "branch relation lost distinguished role aspect");
        local::FiniteConstraintSystem<std::int64_t> aspect_constraints(branch);
        aspect_constraints.set_boolean_domain(v0);
        aspect_constraints.add_relation_aspect_chart("head must be zero", core::RelationId{core::Grade{1},0u},
                                                     *role, true, {{0}});
        require(aspect_constraints.globally_valid({{v0,0}}), "aspect-selected nonlinear chart rejected head state");
        require(!aspect_constraints.globally_valid({{v0,1}}), "aspect-selected nonlinear chart ignored head role");

        // Finite local domains may themselves contain retained relation identities.
        local::FiniteRelationConstraintSystem relation_values(branch);
        const auto r0 = branch.relation_identity(core::RelationId{core::Grade{1},0u});
        const auto r1 = branch.relation_identity(core::RelationId{core::Grade{1},1u});
        relation_values.set_domain(v0, {r0,r1});
        relation_values.set_domain(v1, {r0,r1});
        auto pair_patch = carrier.closure({v0,v1});
        relation_values.add_chart("same relation token", pair_patch, {v0,v1}, {{r0,r0},{r1,r1}});
        require(relation_values.global_section().has_value(), "relation-valued finite local system has no section");
        require(!relation_values.exact_global_unsat_core().has_value(),
                "satisfiable relation-valued finite local system produced an unsat core");

        // Nonlinear semantic transport is defined for strict structural scale
        // maps.  It does not pretend that a general linear chain realization is
        // automatically a map of arbitrary finite values.
        const auto macro = edge_complex(core::SemanticScale{1});
        const auto micro = edge_complex(core::SemanticScale{0});
        std::vector<std::vector<std::uint32_t>> maps{{0u,1u},{0u}};
        algebra::RelationalMorphism strict(macro, micro, maps);
        geometry::SemanticScaleMap scale(core::SemanticScale{1}, core::SemanticScale{0}, std::move(strict));
        local::FiniteConstraintSystem<std::int64_t> macro_constraints(macro);
        const core::CellId mx{core::Grade{0},0u}, my{core::Grade{0},1u};
        macro_constraints.set_boolean_domain(mx);
        macro_constraints.set_boolean_domain(my);
        macro_constraints.add_boolean_truth_chart("equal", macro_constraints.carrier().closure({mx,my}), {mx,my}, 0x9u);
        const auto micro_constraints = local::pushforward_constraints_bijective(macro_constraints, scale);
        require(micro_constraints.global_section().has_value(), "strict semantic constraint pushforward failed");
        const auto roundtrip = local::pullback_constraints_bijective(micro_constraints, scale);
        require(roundtrip.charts().size() == 1u && roundtrip.domains().size() == 2u,
                "strict semantic constraint roundtrip changed finite system");
        local::FiniteConstraintSystem<std::int64_t>::Assignment macro_assignment{{mx,1},{my,1}};
        const auto micro_assignment = local::push_assignment(macro_assignment, scale);
        const auto macro_again = local::pull_assignment(micro_assignment, scale);
        require(macro_again == macro_assignment, "strict semantic assignment roundtrip failed");

        // A noninjective strict map is valid relationally, but cannot be silently
        // promoted to bijective set-valued constraint transport.
        core::RelationSignature witness_sig_a;
        core::RelationalComplexBuilder wb_a(witness_sig_a, core::SemanticScale{1});
        wb_a.declare_witness_identity(1); wb_a.declare_witness_identity(2);
        const auto macro2 = wb_a.freeze();
        core::RelationSignature witness_sig_b;
        core::RelationalComplexBuilder wb_b(witness_sig_b, core::SemanticScale{0});
        wb_b.declare_witness_identity(10);
        const auto micro2 = wb_b.freeze();
        std::vector<std::vector<std::uint32_t>> collision{{0u,0u}};
        require_throws([&] {
            algebra::RelationalMorphism m(macro2, micro2, collision);
            geometry::SemanticScaleMap s(core::SemanticScale{1}, core::SemanticScale{0}, std::move(m));
            local::FiniteConstraintSystem<std::int64_t> c(macro2);
            c.set_boolean_domain(core::CellId{core::Grade{0},0u});
            c.set_boolean_domain(core::CellId{core::Grade{0},1u});
            (void)local::pushforward_constraints_bijective(c, s);
        }, "noninjective semantic constraint transport was silently accepted");

        std::cout << "value-neutral local substrate tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
