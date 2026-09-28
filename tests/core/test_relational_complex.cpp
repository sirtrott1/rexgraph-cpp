#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace rfc;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    try {
        require(rfc::composite::signed_support(false, false) == 0, "absent positive composite support failed");
        require(rfc::composite::signed_support(false, true) == 0, "absent negative composite support failed");
        require(rfc::composite::signed_support(true, false) == 1, "positive composite support failed");
        require(rfc::composite::signed_support(true, true) == -1, "negative composite support failed");

        // Signed-incidence semantics are role-declared, not tied to literal
        // aspect names in the core implementation.
        composite::AspectSchema renamed_aspects;
        renamed_aspects.add("present", composite::AspectRole::Existence, true);
        renamed_aspects.add("polarity", composite::AspectRole::Orientation, false);
        core::RelationSignature renamed_signature(renamed_aspects);
        const auto renamed_edge = renamed_signature.add_type(
            "edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder renamed_builder(renamed_signature);
        renamed_builder.add_grade_one(renamed_edge, {{1,true,true,false,1},{2,true,false,false,1}});
        const auto renamed = renamed_builder.freeze();
        require(renamed.boundary_apply(core::Grade{1}, {1}) == numeric::RationalVector({-1,1}),
                "role-declared signed incidence depends on canonical aspect names");

        core::RelationSignature signature;
        const auto edge_type = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto face_type = signature.add_type("face", core::BoundaryPolicy::DeclaredExact, 3, 3);

        core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(edge_type, {{10,true,true,false,1},{11,true,false,false,1}}); // 01
        builder.add_grade_one(edge_type, {{11,true,true,false,1},{12,true,false,false,1}}); // 12
        builder.add_grade_one(edge_type, {{10,true,true,false,1},{12,true,false,false,1}}); // 02
        builder.add_higher(core::Grade{2}, face_type,
                           {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
        const auto complex = builder.freeze();

        require(complex.grade(core::Grade{0}).cell_count() == 3u, "grade-zero identities were not derived from occurrences");
        require(complex.grade(core::Grade{1}).cell_count() == 3u, "grade-one relation count mismatch");
        require(core::chain_interface_holds(complex, core::Grade{1}), "triangle chain condition failed");
        const auto report = core::bianchi_through(complex, core::Grade{2});
        require(report.satisfied() && report.interfaces.size() == 1u, "grade-two Bianchi report failed");

        const numeric::SparseRationalVector sparse_edge = numeric::SparseRationalVector::basis(3u, 0u);
        require(complex.boundary_apply_sparse(core::Grade{1}, sparse_edge).dense() ==
                    complex.boundary_apply(core::Grade{1}, {1,0,0}),
                "sparse boundary action disagrees with dense reference");
        const numeric::SparseRationalVector sparse_vertex = numeric::SparseRationalVector::basis(3u, 1u);
        require(complex.transpose_apply_sparse(core::Grade{1}, sparse_vertex).dense() ==
                    complex.transpose_apply(core::Grade{1}, {0,1,0}),
                "sparse transpose action disagrees with dense reference");

        core::RelationSignature branch_signature;
        const auto branch = branch_signature.add_branch_type("branch", 4, 4);
        core::RelationalComplexBuilder branch_builder(branch_signature);
        branch_builder.add_grade_one(branch, {
            {1,true,true,true,1},
            {2,true,false,false,numeric::rational(1,3)},
            {3,true,false,false,numeric::rational(1,3)},
            {4,true,false,false,numeric::rational(1,3)}});
        const auto branching = branch_builder.freeze();
        const auto& g1 = branching.grade(core::Grade{1});
        require(g1.cell_scales.at(0) == 3, "canonical branch scale was not integralized to 3");
        require(g1.lattice_coefficient.size() == 4u, "branch lattice incidence count mismatch");
        require(g1.lattice_coefficient[0] == -3 && g1.lattice_coefficient[1] == 1 &&
                g1.lattice_coefficient[2] == 1 && g1.lattice_coefficient[3] == 1,
                "branch primitive integer column mismatch");
        require(branching.incidence_coefficient(core::Grade{1}, 0) == -1, "branch head coefficient mismatch");
        require(branching.incidence_coefficient(core::Grade{1}, 1) == numeric::rational(1,3), "branch tail coefficient mismatch");

        // Unequal exact shares are first-class.  The common lattice scale is an
        // execution coordinate, not a semantic equal-share requirement.
        core::RelationalComplexBuilder unequal_builder(branch_signature);
        unequal_builder.add_grade_one(branch, {
            {1,true,true,true,1},
            {2,true,false,false,numeric::rational(1,4)},
            {3,true,false,false,numeric::rational(1,2)},
            {4,true,false,false,numeric::rational(1,4)}});
        const auto unequal = unequal_builder.freeze();
        const auto& ug1 = unequal.grade(core::Grade{1});
        require(ug1.cell_scales.at(0) == 4, "unequal branch scale should be 4");
        require(ug1.lattice_coefficient[0] == -4 && ug1.lattice_coefficient[1] == 1 &&
                ug1.lattice_coefficient[2] == 2 && ug1.lattice_coefficient[3] == 1,
                "unequal branch lattice column mismatch");
        require(unequal.incidence_coefficient(core::Grade{1}, 1) == numeric::rational(1,4) &&
                unequal.incidence_coefficient(core::Grade{1}, 2) == numeric::rational(1,2) &&
                unequal.incidence_coefficient(core::Grade{1}, 3) == numeric::rational(1,4),
                "unequal branch rational shares were not preserved exactly");

        core::RelationSignature identity_signature;
        const auto identity_edge = identity_signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder identity_builder(identity_signature);
        identity_builder.add_grade_one(core::RelationKey{1001}, identity_edge,
                                       {{30,true,true,false,1,{},core::OccurrenceKey{5001}},
                                        {31,true,false,false,1,{},core::OccurrenceKey{5002}}});
        identity_builder.add_grade_one(core::RelationKey{1002}, identity_edge,
                                       {{30,true,true,false,1,{},core::OccurrenceKey{5003}},
                                        {31,true,false,false,1,{},core::OccurrenceKey{5004}}});
        const auto identity_complex = identity_builder.freeze();
        const auto& ig1 = identity_complex.grade(core::Grade{1});
        require(ig1.relation_identities.at(0) == core::RelationKey{1001} &&
                ig1.relation_identities.at(1) == core::RelationKey{1002},
                "distinct primary identities with equal boundaries were not retained");
        require(identity_complex.relation_identity({core::Grade{1},0}) ==
                    core::RelationIdentity{core::Grade{1},core::RelationKey{1001}},
                "relation identity accessor mismatch");
        require(ig1.occurrence_identities.at(0) == core::OccurrenceKey{5001} &&
                ig1.occurrence_identities.at(3) == core::OccurrenceKey{5004},
                "boundary occurrence identities were not retained");
        require(identity_complex.occurrence_identity(core::Grade{1},3) == core::OccurrenceKey{5004},
                "occurrence identity accessor mismatch");
        require(numeric::is_zero(identity_complex.boundary_apply(core::Grade{1}, {1,-1})),
                "parallel primary relations should retain a closed difference field");

        // A higher primary relation is admissible only when its declared
        // boundary is already closed.  Reject it before a RelationalComplex is
        // frozen instead of constructing an invalid canonical tower and testing
        // Bianchi compatibility afterward.
        core::RelationSignature invalid_signature;
        const auto invalid_edge = invalid_signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto invalid_face = invalid_signature.add_type("face", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder invalid_builder(invalid_signature);
        invalid_builder.add_grade_one(invalid_edge, {{20,true,true,false,1},{21,true,false,false,1}});
        invalid_builder.add_grade_one(invalid_edge, {{21,true,true,false,1},{22,true,false,false,1}});
        bool rejected_open_boundary = false;
        try {
            invalid_builder.add_higher(core::Grade{2}, invalid_face,
                                       {{0,true,false,false,1},{1,true,false,false,1}});
        } catch (const std::invalid_argument&) {
            rejected_open_boundary = true;
        }
        require(rejected_open_boundary, "non-closed higher boundary was not rejected at assertion time");

        std::cout << "core relational complex tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
