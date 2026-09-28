#include "rfc/algebra/materialize.hpp"
#include "rfc/algebra/relative.hpp"
#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static rfc::core::RelationalComplex filled_triangle() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", rfc::core::BoundaryPolicy::DeclaredExact, 3, 3);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{0,true,true,false,1,{}},{1,true,false,false,1,{}}});
    builder.add_grade_one(edge, {{1,true,true,false,1,{}},{2,true,false,false,1,{}}});
    builder.add_grade_one(edge, {{0,true,true,false,1,{}},{2,true,false,false,1,{}}});
    builder.add_higher(rfc::core::Grade{2}, face,
                       {{0,true,false,false,1,{}},{1,true,false,false,1,{}},{2,true,true,false,1,{}}});
    return builder.freeze();
}

int main() {
    try {
        const auto triangle = filled_triangle();

        rfc::algebra::SubcomplexMask left(triangle);
        left.select({rfc::core::Grade{1}, 0});
        left.close_boundaries(triangle);
        rfc::algebra::SubcomplexMask right(triangle);
        right.select({rfc::core::Grade{1}, 1});
        right.close_boundaries(triangle);

        const auto glued = rfc::algebra::pushout_inside(triangle, left, right);
        require(glued.overlap.grade(rfc::core::Grade{0}).count() == 1u, "edge overlap should be the shared vertex");
        require(glued.glued.complex.grade(rfc::core::Grade{0}).cell_count() == 3u, "glued path vertex count mismatch");
        require(glued.glued.complex.grade(rfc::core::Grade{1}).cell_count() == 2u, "glued path edge count mismatch");
        require(rfc::core::chain_interface_holds(glued.glued.complex, rfc::core::Grade{0}), "materialized path chain check failed");

        rfc::algebra::SubcomplexMask whole(triangle);
        whole.select({rfc::core::Grade{2}, 0});
        whole.close_boundaries(triangle);
        rfc::algebra::SubcomplexMask boundary(triangle);
        for (std::uint32_t e = 0; e < 3u; ++e) boundary.select({rfc::core::Grade{1}, e});
        boundary.close_boundaries(triangle);
        rfc::algebra::RelativeChainComplex relative(triangle, whole, boundary);
        require(relative.cell_count(rfc::core::Grade{2}) == 1u, "relative face count mismatch");
        require(relative.cell_count(rfc::core::Grade{1}) == 0u, "relative boundary should vanish in quotient");
        rfc::numeric::RationalVector top{1};
        require(relative.boundary_apply(rfc::core::Grade{2}, top).empty(), "relative face boundary should vanish");

        const auto materialized = rfc::algebra::materialize_subcomplex(triangle, boundary);
        require(materialized.complex.grade(rfc::core::Grade{1}).cell_count() == 3u, "boundary materialization edge count mismatch");
        // A retained subcomplex preserves the ambient grade declaration even when
        // the selected top fiber is empty. Trimming is a different projection.
        require(materialized.complex.grade_count() == triangle.grade_count(),
                "materialized boundary must preserve its declared grade inventory");
        require(materialized.complex.grade(rfc::core::Grade{2}).cell_count() == 0u,
                "retaining a declared empty grade must not invent a face");

        std::cout << "relational gluing and relative quotient tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
