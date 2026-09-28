#include "rfc/core/builder.hpp"
#include "rfc/local/covering.hpp"
#include "rfc/topology/monodromy.hpp"
#include "rfc/topology/path_groupoid.hpp"
#include "rfc/topology/fundamental_group.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex make_cycle(std::size_t n) {
    rfc::core::RelationSignature sig;
    const auto edge = sig.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder b(sig);
    for (std::size_t i = 0; i < n; ++i) {
        const auto j = (i + 1u) % n;
        b.add_grade_one(edge, {{static_cast<rfc::core::IdentityKey>(i),true,true,false,1},
                               {static_cast<rfc::core::IdentityKey>(j),true,false,false,1}});
    }
    return b.freeze();
}

static std::pair<rfc::core::RelationalComplex,rfc::core::RelationalComplex> make_branch_cover() {
    rfc::core::RelationSignature sig;
    const auto branch = sig.add_branch_type("branch", 3, 3);
    rfc::core::RelationalComplexBuilder target(sig);
    target.add_grade_one(branch, {{10,true,true,true,1},{20,true,false,false,rfc::numeric::Rational{1,2}},{30,true,false,false,rfc::numeric::Rational{1,2}}});
    rfc::core::RelationalComplexBuilder source(sig);
    source.add_grade_one(branch, {{110,true,true,true,1},{120,true,false,false,rfc::numeric::Rational{1,2}},{130,true,false,false,rfc::numeric::Rational{1,2}}});
    source.add_grade_one(branch, {{210,true,true,true,1},{220,true,false,false,rfc::numeric::Rational{1,2}},{230,true,false,false,rfc::numeric::Rational{1,2}}});
    return {source.freeze(), target.freeze()};
}

int main() {
    try {
        using namespace rfc;
        const auto source = make_cycle(6u);
        const auto target = make_cycle(3u);
        std::vector<std::vector<std::uint32_t>> maps(2);
        maps[0].resize(6u);
        maps[1].resize(6u);
        for (std::uint32_t i = 0; i < 6u; ++i) { maps[0][i] = i % 3u; maps[1][i] = i % 3u; }
        local::StrictRelationalCovering cover(source, target, maps);
        require(cover.fiber(core::CellId{core::Grade{0},0u}).size() == 2u, "C6 -> C3 vertex fiber size mismatch");

        topology::PairwisePathGroupoid groupoid(target);
        const auto e0 = groupoid.generator(0u,true);
        const auto e1 = groupoid.generator(1u,true);
        const auto e2 = groupoid.generator(2u,true);
        const auto loop = groupoid.compose(groupoid.compose(e0,e1),e2);
        require(loop.is_loop() && loop.source() == 0u, "base generator loop mismatch");
        const auto identity = groupoid.compose(loop, loop.inverse());
        require(identity.is_loop() && identity.steps().empty(), "path groupoid inverse law failed");

        const auto lifted = topology::lift_path(cover, loop, 0u);
        require(lifted.endpoint == 3u, "one base generator should move to the other sheet");
        const auto action = topology::monodromy(cover, loop);
        require(action.image.size() == 2u && action.image[0] == 1u && action.image[1] == 0u,
                "two-sheet monodromy should be a transposition");
        const auto base_presentation=topology::fundamental_group_presentation(target,0u);
        require(base_presentation.generator_loops.size()==1u,"C3 should have one fundamental generator");
        const auto generator_action=topology::monodromy(cover,base_presentation.generator_loops[0]);
        require(generator_action.image==action.image,"cover monodromy should agree on the compiled fundamental generator");
        const auto twice = topology::PairwisePath::compose(loop, loop);
        const auto action_twice = topology::monodromy(cover, twice);
        require(action_twice.image[0] == 0u && action_twice.image[1] == 1u,
                "two generator loops should return each sheet");

        std::vector<std::vector<std::uint32_t>> deck(2);
        deck[0].resize(6u);
        deck[1].resize(6u);
        for (std::uint32_t i = 0; i < 6u; ++i) { deck[0][i] = (i + 3u) % 6u; deck[1][i] = (i + 3u) % 6u; }
        require(topology::is_deck_transformation(cover, deck), "sheet-swap deck transformation rejected");
        for (std::uint32_t i = 0; i < 6u; ++i) { deck[0][i] = (i + 1u) % 6u; deck[1][i] = (i + 1u) % 6u; }
        require(!topology::is_deck_transformation(cover, deck), "non-fiber-preserving rotation accepted as deck transformation");

        // Strict covering semantics are not restricted to pairwise graphs.
        auto [branch_source, branch_target] = make_branch_cover();
        std::vector<std::vector<std::uint32_t>> branch_maps(2);
        branch_maps[0] = {0u,1u,2u,0u,1u,2u};
        branch_maps[1] = {0u,0u};
        local::StrictRelationalCovering branch_cover(branch_source, branch_target, branch_maps);
        require(branch_cover.fiber(core::CellId{core::Grade{1},0u}).size() == 2u,
                "branch relation covering fiber mismatch");

        std::cout << "covering/groupoid/monodromy tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
