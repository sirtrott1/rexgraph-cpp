#include "rfc/core/builder.hpp"
#include "rfc/topology/attaching_relation.hpp"
#include "rfc/topology/attaching_word.hpp"
#include "rfc/topology/cylinder.hpp"
#include "rfc/topology/integral_homology.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <class F>
static void require_throws(F&& fn, const char* message) {
    try { fn(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}

struct BranchTower {
    rfc::core::RelationalComplex complex;
    rfc::core::RelationTypeId branch_type{};
};

static BranchTower make_nested_branch(bool fill, bool duplicate_fill = false) {
    using namespace rfc;
    core::RelationSignature signature;
    const auto branch = signature.add_branch_type("branch", 2u, 0u);
    const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2u, 2u);
    core::RelationalComplexBuilder builder(signature);

    builder.add_grade_one(branch, {
        {100u, true, true, true, numeric::Rational{1}},
        {101u, true, false, false, numeric::Rational{1,3}},
        {102u, true, false, false, numeric::Rational{1,3}},
        {103u, true, false, false, numeric::Rational{1,3}}});
    builder.add_grade_one(edge, {{100u,true,true,numeric::Rational{1}}, {101u,true,false,numeric::Rational{1}}});
    builder.add_grade_one(edge, {{100u,true,true,numeric::Rational{1}}, {102u,true,false,numeric::Rational{1}}});
    builder.add_grade_one(edge, {{100u,true,true,numeric::Rational{1}}, {103u,true,false,numeric::Rational{1}}});

    if (fill) {
        builder.add_canonical_higher(core::Grade{2}, branch, {0u,1u,2u,3u}, 0u);
        if (duplicate_fill) builder.add_canonical_higher(core::Grade{2}, branch, {0u,1u,2u,3u}, 0u);
    }
    return BranchTower{builder.freeze(), branch};
}

static rfc::core::RelationalComplex make_weighted_branch() {
    using namespace rfc;
    core::RelationSignature signature;
    const auto branch = signature.add_branch_type("weighted-branch", 4u, 4u);
    core::RelationalComplexBuilder builder(signature);
    builder.add_weighted_branch_grade_one(
        branch, {10u,11u,12u,13u}, 0u,
        {numeric::Rational{1,4}, numeric::Rational{1,2}, numeric::Rational{1,4}});
    return builder.freeze();
}


static rfc::core::RelationalComplex make_branching_attaching_loop_projection_case() {
    using namespace rfc;
    core::RelationSignature signature;
    const auto branch = signature.add_branch_type("branch", 2u, 0u);
    const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2u, 2u);
    const auto attaching = signature.add_attaching_loop_type("declared-loop", 4u, 4u);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(branch, {
        {100u, true, true, true, numeric::Rational{1}},
        {101u, true, false, false, numeric::Rational{1,3}},
        {102u, true, false, false, numeric::Rational{1,3}},
        {103u, true, false, false, numeric::Rational{1,3}}});
    builder.add_grade_one(edge, {{100u,true,true,numeric::Rational{1}}, {101u,true,false,numeric::Rational{1}}});
    builder.add_grade_one(edge, {{100u,true,true,numeric::Rational{1}}, {102u,true,false,numeric::Rational{1}}});
    builder.add_grade_one(edge, {{100u,true,true,numeric::Rational{1}}, {103u,true,false,numeric::Rational{1}}});
    builder.add_higher(core::Grade{2}, attaching, {
        {0u,true,true,numeric::Rational{1}},
        {1u,true,false,numeric::Rational{1,3}},
        {2u,true,false,numeric::Rational{1,3}},
        {3u,true,false,numeric::Rational{1,3}}});
    return builder.freeze();
}

int main() {
    try {
        using namespace rfc;
        using I = numeric::Integer;
        using R = numeric::Rational;

        // Homology is computed on primary relation coordinates, not on a
        // pairwise expansion of the branch.  The open nested branch has one
        // primitive integral grade-one cycle z = -a + a1 + a2 + a3.
        const auto open = make_nested_branch(false);
        const std::vector<I> z{I{-1}, I{1}, I{1}, I{1}};
        const auto open_h1 = topology::integral_homology(open.complex, core::Grade{1});
        require(open_h1.free_rank == 1u && open_h1.torsion_invariants.empty(),
                "open nested branch should have one integral H1 direction");
        const auto open_z = topology::integral_chain_certificate(open.complex, core::Grade{1}, z);
        require(open_z.cycle && !open_z.boundary && open_z.represents_nonzero_homology_class(),
                "nested branching cycle should be nontrivial before higher attachment");

        // Asserting the grade-two branching relation makes exactly that same
        // retained grade-one chain a boundary.  Nothing is pairwise-expanded.
        const auto filled = make_nested_branch(true);
        const auto attaching = topology::higher_attaching_relation(filled.complex, core::Grade{2}, 0u);
        require(attaching.has_branching_participant(),
                "higher branching attachment did not retain its branching lower participant");
        require(attaching.fiber().size() == 4u,
                "higher branching attachment occurrence fiber changed arity");
        require(attaching.participant_identity(0u).grade == core::Grade{1},
                "higher attachment participant did not remain a grade-one relation identity");

        const auto filled_z = topology::integral_chain_certificate(filled.complex, core::Grade{1}, z);
        require(filled_z.cycle && filled_z.boundary && !filled_z.represents_nonzero_homology_class(),
                "higher branch should kill the retained grade-one homology class");
        require(filled_z.boundary_preimage.size() == 1u &&
                filled_z.boundary_preimage[0] == 1,
                "higher branch boundary witness mismatch");
        const auto filled_h1 = topology::integral_homology(filled.complex, core::Grade{1});
        require(filled_h1.free_rank == 0u,
                "filled nested branch should have trivial integral H1");

        // A second distinct primary higher branch with the same boundary does
        // not change H1 again; it creates a multiplicity H2 direction.
        const auto duplicate = make_nested_branch(true, true);
        const auto duplicate_h1 = topology::integral_homology(duplicate.complex, core::Grade{1});
        const auto duplicate_h2 = topology::integral_homology(duplicate.complex, core::Grade{2});
        require(duplicate_h1.free_rank == 0u,
                "duplicate higher branch unexpectedly changed H1");
        require(duplicate_h2.free_rank == 1u && duplicate_h2.torsion_invariants.empty(),
                "duplicate primary higher branches should create one H2 multiplicity direction");
        const std::vector<I> face_difference{I{1}, I{-1}};
        const auto face_class = topology::integral_chain_certificate(
            duplicate.complex, core::Grade{2}, face_difference);
        require(face_class.represents_nonzero_homology_class(),
                "difference of duplicate higher branch identities should survive in H2");

        // Native cylinder of an unequal-share branch.  The branch is still one
        // primary factor cell at each endpoint.  Its prism boundary contains
        // four swept boundary participants plus two endpoint copies:
        //   d(a x I) = da x I + a x v0 - a x v1.
        const auto weighted = make_weighted_branch();
        const core::CellId branch_cell{core::Grade{1}, 0u};
        topology::RelationalCylinder cylinder(weighted);
        const auto start_identity = cylinder.endpoint_identity(branch_cell, topology::CylinderEndpoint::Start);
        const auto finish_identity = cylinder.endpoint_identity(branch_cell, topology::CylinderEndpoint::Finish);
        require(start_identity.left == finish_identity.left,
                "cylinder endpoints lost the primary branching relation identity");
        require(start_identity.right != finish_identity.right,
                "cylinder endpoint objects were not distinguished");

        const auto prism = cylinder.prism_cell(branch_cell);
        require(prism.grade() == core::Grade{2},
                "grade-one branch prism should be grade two");
        const auto terms = cylinder.prism_boundary(branch_cell);
        require(terms.size() == 6u,
                "branch prism should have four side occurrences and two endpoint copies");

        std::vector<R> swept;
        R start_coeff{0}, finish_coeff{0};
        std::size_t endpoint_terms = 0;
        for (const auto& term : terms) {
            if (term.side == algebra::ProductBoundarySide::Left) {
                swept.push_back(term.coefficient);
                require(!term.koszul_flip, "source-boundary side should not carry Koszul flip");
            } else {
                ++endpoint_terms;
                require(term.koszul_flip, "interval-boundary side of odd branch should carry Koszul flip");
                if (term.lower.right.index == cylinder.interval().start().index) start_coeff += term.coefficient;
                if (term.lower.right.index == cylinder.interval().finish().index) finish_coeff += term.coefficient;
                require(term.lower.left == branch_cell,
                        "cylinder endpoint term split the branch into pair legs");
            }
        }
        require(swept == std::vector<R>({R{-1}, R{1,4}, R{1,2}, R{1,4}}),
                "branch cylinder changed exact unequal-share boundary coefficients");
        require(endpoint_terms == 2u && start_coeff == 1 && finish_coeff == -1,
                "branch prism endpoint orientation mismatch");

        // Even if an application labels a higher relation as an attaching
        // loop, a boundary containing a branching primary relation is not a
        // pairwise word. The native higher attachment remains available while
        // the fundamental-group projection rejects the lossy flattening.
        const auto projected_case = make_branching_attaching_loop_projection_case();
        const auto native_attachment = topology::higher_attaching_relation(
            projected_case, core::Grade{2}, 0u);
        require(native_attachment.has_branching_participant(),
                "native higher attachment lost its branching participant");
        require_throws([&] { (void)topology::attaching_word(projected_case, 0u); },
                       "branching higher attachment was silently projected to a pairwise attaching word");

        std::cout << "branching homotopy/homology tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
