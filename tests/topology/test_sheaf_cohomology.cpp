#include "rfc/core/builder.hpp"
#include "rfc/local/cellular_sheaf.hpp"
#include "rfc/topology/local_system_transport.hpp"
#include "rfc/topology/sheaf_cohomology.hpp"
#include "rfc/topology/path_groupoid.hpp"
#include "rfc/topology/fundamental_group.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex make_triangle(bool filled) {
    rfc::core::RelationSignature sig;
    const auto edge = sig.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = sig.add_type("face", rfc::core::BoundaryPolicy::DeclaredExact, 3, 3);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge, {{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge, {{2,true,true,false,1},{0,true,false,false,1}});
    if (filled) b.add_higher(rfc::core::Grade{2}, face,
                             {{0,true,false,false,1},{1,true,false,false,1},{2,true,false,false,1}});
    return b.freeze();
}

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;

        const auto cycle = make_triangle(false);
        local::StalkBundle cycle_stalks(cycle);
        local::LinearCellularSheaf constant_cycle(cycle_stalks);
        require(constant_cycle.is_cochain_complex(), "constant cycle sheaf is not a cochain complex");
        const auto H0 = topology::sheaf_cohomology(constant_cycle, core::Grade{0});
        const auto H1 = topology::sheaf_cohomology(constant_cycle, core::Grade{1});
        require(H0.dimension == 1u && H1.dimension == 1u, "constant circle sheaf cohomology mismatch");
        require(constant_cycle.global_section_frame().cols() == 1u, "constant circle global sections mismatch");

        const auto filled = make_triangle(true);
        local::StalkBundle disk_stalks(filled);
        local::LinearCellularSheaf constant_disk(disk_stalks);
        require(constant_disk.is_cochain_complex(), "constant disk sheaf is not a cochain complex");
        require(constant_disk.is_local_system(), "constant disk should be a coherent local system");
        const auto HD0 = topology::sheaf_cohomology(constant_disk, core::Grade{0});
        const auto HD1 = topology::sheaf_cohomology(constant_disk, core::Grade{1});
        const auto HD2 = topology::sheaf_cohomology(constant_disk, core::Grade{2});
        require(HD0.dimension == 1u && HD1.dimension == 0u && HD2.dimension == 0u,
                "constant filled-triangle sheaf cohomology mismatch");

        local::StalkBundle vector_stalks(cycle, 2u);
        local::LinearCellularSheaf vector_cycle(vector_stalks);
        require(topology::sheaf_cohomology(vector_cycle, core::Grade{0}).dimension == 2u,
                "rank-two constant local system H0 mismatch");
        require(topology::sheaf_cohomology(vector_cycle, core::Grade{1}).dimension == 2u,
                "rank-two constant local system H1 mismatch");

        // Twisted one-dimensional local system around the cycle.  All maps are
        // identity except the head restriction on edge 2, which is multiplication by 2.
        local::StalkBundle twisted_stalks(cycle);
        local::LinearCellularSheaf twisted(twisted_stalks);
        const auto& edge_arena = cycle.grade(core::Grade{1});
        const auto [begin, end] = edge_arena.incidence_range(2u);
        std::size_t head_slot = edge_arena.incidence_count();
        for (std::size_t slot = begin; slot < end; ++slot) {
            if (edge_arena.lower_index[slot] == 0u) head_slot = slot;
        }
        require(head_slot != edge_arena.incidence_count(), "twisted local-system head slot missing");
        twisted.set_restriction(core::Grade{1}, head_slot, numeric::ExactMatrix::from_rows({{R{2}}}));
        require(twisted.is_local_system(), "twisted circle restrictions should be invertible local-system maps");
        require(twisted.is_cochain_complex(), "twisted circle local system should define a cochain complex");
        require(topology::sheaf_cohomology(twisted, core::Grade{0}).dimension == 0u,
                "twisted circle H0 should vanish");
        require(topology::sheaf_cohomology(twisted, core::Grade{1}).dimension == 0u,
                "twisted circle H1 should vanish");

        topology::PairwisePath loop(cycle, 0u, {{0u,true},{1u,true},{2u,true}});
        const auto holonomy = topology::local_system_transport(twisted, loop);
        require(holonomy.rows() == 1u && holonomy.cols() == 1u && holonomy.get(0u,0u) == R{1,2},
                "twisted local-system holonomy mismatch");
        const auto cycle_group = topology::fundamental_group_presentation(cycle,0u);
        require(cycle_group.generator_loops.size()==1u,"circle should have one maximal-tree generator loop");
        const auto generator_holonomy = topology::local_system_transport(twisted,cycle_group.generator_loops[0]);
        require(generator_holonomy.get(0u,0u)==R{1,2},"fundamental generator holonomy mismatch");

        // The same arbitrary twist cannot simply be extended over the filled face
        // with identity face restrictions: the exact delta^2 defect detects the obstruction.
        local::StalkBundle obstructed_stalks(filled);
        local::LinearCellularSheaf obstructed(obstructed_stalks);
        const auto& filled_edges = filled.grade(core::Grade{1});
        const auto [fb, fe] = filled_edges.incidence_range(2u);
        std::size_t filled_head_slot = filled_edges.incidence_count();
        for (std::size_t slot = fb; slot < fe; ++slot) if (filled_edges.lower_index[slot] == 0u) filled_head_slot = slot;
        obstructed.set_restriction(core::Grade{1}, filled_head_slot, numeric::ExactMatrix::from_rows({{R{2}}}));
        require(!obstructed.is_cochain_complex(), "incompatible filled-face local system obstruction was not detected");
        require(obstructed.has_invertible_restrictions(), "the obstruction should not be a singular restriction");
        require(!obstructed.is_local_system(), "invertible incoherent restrictions received a local-system certificate");
        require(obstructed.two_step_defect(core::Grade{0}).nonzeros() != 0u,
                "incompatible local system should have a nonzero exact cochain defect");
        bool rejected_cohomology=false;
        try { (void)topology::sheaf_cohomology(obstructed,core::Grade{1}); }
        catch(const std::invalid_argument&) { rejected_cohomology=true; }
        require(rejected_cohomology,"cohomology accepted incoherent restrictions");

        local::LinearCellularSheaf singular(cycle_stalks);
        singular.set_restriction(core::Grade{1},0u,numeric::ExactMatrix(1u,1u));
        require(singular.is_cochain_complex() && !singular.has_invertible_restrictions() && !singular.is_local_system(),
                "cochain coherence alone should not certify a local system");

        std::cout << "sheaf/local-system cohomology tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
