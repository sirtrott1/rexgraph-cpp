#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/topology/lattice.hpp"
#include "rfc/topology/modular.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        rfc::core::RelationSignature signature;
        const auto branch = signature.add_branch_type("branch", 4, 4);
        const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto face = signature.add_branch_type("branch-face", 4, 4);
        rfc::core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(branch, {
            {0,true,true,true,1},
            {1,true,false,false,rfc::numeric::rational(1,3)},
            {2,true,false,false,rfc::numeric::rational(1,3)},
            {3,true,false,false,rfc::numeric::rational(1,3)}});
        builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
        builder.add_grade_one(edge, {{0,true,true,false,1},{2,true,false,false,1}});
        builder.add_grade_one(edge, {{0,true,true,false,1},{3,true,false,false,1}});
        builder.add_canonical_higher(rfc::core::Grade{2}, face, {0,1,2,3}, 0);
        const auto complex = builder.freeze();
        require(rfc::core::chain_interface_holds(complex, rfc::core::Grade{1}), "nested branching rational chain condition failed");

        const auto& g1 = complex.grade(rfc::core::Grade{1});
        const auto& g2 = complex.grade(rfc::core::Grade{2});
        require(g1.cell_scales[0] == 3 && g2.cell_scales[0] == 3, "coordinated grade scales mismatch");
        require(g2.lattice_coefficient[0] == -1 && g2.lattice_coefficient[1] == 1 &&
                g2.lattice_coefficient[2] == 1 && g2.lattice_coefficient[3] == 1,
                "higher integral column mismatch");

        rfc::topology::ModularVector top{1};
        const auto mid = rfc::topology::lattice_boundary_apply(complex, rfc::core::Grade{2}, top, 5);
        const auto bottom = rfc::topology::lattice_boundary_apply(complex, rfc::core::Grade{1}, mid, 5);
        for (const auto value : bottom) require(value == 0, "integral lattice chain condition failed mod 5");

        std::cout << "integral lattice tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
