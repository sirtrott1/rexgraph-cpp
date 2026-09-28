#include "rfc/geometry/chain_map.hpp"
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/local/fiber_system.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;

        core::RelationSignature sig;
        const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto branch = sig.add_branch_type("branch", 3, 3);

        core::RelationalComplexBuilder macro_builder(sig, core::SemanticScale{1});
        macro_builder.add_grade_one(branch, {
            {10,true,true,true,R{1}},
            {20,true,false,false,R{1,2}},
            {30,true,false,false,R{1,2}}});
        const auto macro = macro_builder.freeze();

        core::RelationalComplexBuilder micro_builder(sig, core::SemanticScale{0});
        micro_builder.add_grade_one(edge, {
            {10,true,true,false,R{1}},
            {15,true,false,false,R{1}}}); // H -> M
        micro_builder.add_grade_one(branch, {
            {15,true,true,true,R{1}},
            {20,true,false,false,R{1,2}},
            {30,true,false,false,R{1,2}}}); // M -> T1,T2
        const auto micro = micro_builder.freeze();

        std::vector<geometry::LinearGradeMap> maps(2);
        maps[0].resize(3);
        maps[0][0] = {{0,R{1}}}; // H
        maps[0][1] = {{2,R{1}}}; // T1
        maps[0][2] = {{3,R{1}}}; // T2
        maps[1].resize(1);
        maps[1][0] = {{0,R{1}},{1,R{1}}}; // macro branch -> pair + micro branch
        geometry::RelationalChainMap J(macro, micro, std::move(maps));
        require(J.is_chain_map(), "macro branch did not realize as a micro branching chain");
        geometry::SemanticScaleRealization realization(core::SemanticScale{1}, core::SemanticScale{0}, J);

        const numeric::RationalVector macro_amp{R{2}};
        const auto macro_boundary = macro.boundary_apply(core::Grade{1}, macro_amp);
        const auto micro_field = realization.realization().push_field(core::Grade{1}, macro_amp);
        require(micro_field == numeric::RationalVector({R{2},R{2}}), "macro branch did not expand to both micro relations with one retained amplitude");
        const auto micro_boundary = micro.boundary_apply(core::Grade{1}, micro_field);
        const auto mapped_macro_boundary = realization.realization().push_field(core::Grade{0}, macro_boundary);
        require(micro_boundary == mapped_macro_boundary, "semantic branch realization failed boundary naturality");
        require(micro_boundary == numeric::RationalVector({R{-2},R{0},R{1},R{1}}), "micro branch cancellation field mismatch");

        core::BranchingRelationView macro_branch(macro, core::Grade{1}, 0);
        core::BranchingRelationView micro_branch(micro, core::Grade{1}, 1);
        require(macro_branch.tails().size() == micro_branch.tails().size(), "branch arity changed unexpectedly across semantic scale");

        std::cout << "branching semantic-scale tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
