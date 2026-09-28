#include "rfc/algebra/subcomplex.hpp"
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/local/fiber_system.hpp"
#include "rfc/local/occurrence_cover.hpp"
#include "rfc/local/sheaf.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/numeric/exact_matrix.hpp"
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

        core::RelationSignature sig;
        const auto branch = sig.add_branch_type("branch", 3, 3);
        core::RelationalComplexBuilder builder(sig);
        builder.add_grade_one(branch, {
            {10,true,true,true,R{1}},
            {20,true,false,false,R{1,2}},
            {30,true,false,false,R{1,2}}}); // h1 -> shared x, y
        builder.add_grade_one(branch, {
            {40,true,true,true,R{1}},
            {20,true,false,false,R{1,2}},
            {50,true,false,false,R{1,2}}}); // h2 -> shared x, z
        const auto complex = builder.freeze();

        local::StalkBundle stalks(complex, 1);
        local::RelationalFiberSystem fibers(stalks);
        local::RelationalSheaf sheaf(fibers);
        local::OccurrenceCover cover(stalks, core::Grade{1});

        // Each branch remains one local tensor with one source stalk and three
        // coupled boundary legs.
        const auto t0 = fibers.branch_tensor(core::Grade{1}, 0);
        require(t0.source_dimension() == 1u && t0.blocks().size() == 3u, "branch tensor did not retain one source and three legs");

        local::StalkBundle vector_stalks(complex, 2);
        local::RelationalFiberSystem vector_fibers(vector_stalks);
        const auto vector_branch = vector_fibers.branch_tensor(core::Grade{1}, 0);
        const auto vector_out = vector_branch.apply(numeric::RationalVector{R{2},R{4}});
        require(vector_out == numeric::RationalVector({R{-2},R{-4},R{1},R{2},R{1},R{2}}),
                "vector-valued branch stalk did not fan out as one block tensor");

        const auto local0 = fibers.realize_relation(core::Grade{1}, 0, numeric::RationalVector{R{2}});
        const auto local1 = fibers.realize_relation(core::Grade{1}, 1, numeric::RationalVector{R{2}});
        require(local0.boundary_values[0].value == numeric::RationalVector{R{-2}}, "branch head fan-out mismatch");
        require(local0.boundary_values[1].value == numeric::RationalVector{R{1}}, "branch tail fan-out mismatch");
        require(local0.boundary_values[2].value == numeric::RationalVector{R{1}}, "branch tail fan-out mismatch");

        // The local branch tensors assemble to the same global boundary action
        // as the canonical relational complex.
        const auto local_occ = cover.scatter_sections({local0, local1});
        const auto assembled = cover.sum_assemble(local_occ);
        const auto direct = complex.boundary_apply(core::Grade{1}, numeric::RationalVector{R{2},R{2}});
        require(assembled == direct, "local branch tensors did not assemble to the global boundary field");
        require(cover.compatible(local_occ), "equal shared-tail branch sections should be compatible");

        local::GradeTensorField grade_tensor(fibers, core::Grade{1});
        require(grade_tensor.apply(numeric::RationalVector{R{2},R{2}}) == direct,
                "global tensor field did not reproduce relational boundary action");
        require(grade_tensor.matrix().apply(numeric::RationalVector{R{2},R{2}}) == direct,
                "assembled block-tensor coordinate realization disagrees with direct action");

        // Boundary-closed local patches glue through the shared stalk.
        algebra::SubcomplexMask patch0(complex), patch1(complex);
        patch0.select(core::CellId{core::Grade{1},0}); patch0.close_boundaries(complex);
        patch1.select(core::CellId{core::Grade{1},1}); patch1.close_boundaries(complex);
        const auto s0 = sheaf.cell_section(local0);
        const auto s1 = sheaf.cell_section(local1);
        require(sheaf.compatible_on_overlap(s0, patch0, s1, patch1), "branch sections should agree on shared stalk");
        const auto global_section = sheaf.glue(s0, patch0, s1, patch1);
        const auto union_patch = join(patch0, patch1);
        require(sheaf.transport_compatible(global_section, union_patch), "glued branch section does not respect flag transports");

        const auto incompatible = fibers.realize_relation(core::Grade{1}, 1, numeric::RationalVector{R{4}});
        const auto bad_section = sheaf.cell_section(incompatible);
        require(!sheaf.compatible_on_overlap(s0, patch0, bad_section, patch1), "incompatible shared-tail values were accepted");
        require_throws([&] { (void)sheaf.glue(s0, patch0, bad_section, patch1); }, "incompatible local branch sections glued");

        // The section quotient metric makes duplicate-difference directions
        // metric-null, while the assembly metric measures their global sum.
        const auto global_metric = numeric::ExactMatrix::identity(cover.global_dimension());
        const auto section_metric = cover.section_metric_pullback(global_metric);
        const auto& arena = complex.grade(core::Grade{1});
        std::vector<std::size_t> shared_slots;
        for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) if (arena.lower_index[slot] == 1u) shared_slots.push_back(slot);
        require(shared_slots.size() == 2u, "shared-tail occurrence multiplicity mismatch");
        numeric::RationalVector duplicate_difference(cover.occurrence_dimension(), R{0});
        duplicate_difference[cover.occurrence_offset(cover.occurrence_index_for_incidence(shared_slots[0]))] = 1;
        duplicate_difference[cover.occurrence_offset(cover.occurrence_index_for_incidence(shared_slots[1]))] = -1;
        require(numeric::dot(duplicate_difference, section_metric.apply(duplicate_difference)) == 0,
                "duplicate-difference direction should be null after stalk gluing");

        const auto assembly_metric = cover.assembly_metric_pullback(global_metric);
        const auto assembled_q = numeric::dot(local_occ, assembly_metric.apply(local_occ));
        const auto direct_q = numeric::dot(direct, direct);
        require(assembled_q == direct_q, "local block metric did not scale through global assembly");
        const auto relation_metric = grade_tensor.local_metric_from_global(global_metric);
        require(numeric::dot(numeric::RationalVector{R{2},R{2}}, relation_metric.apply(numeric::RationalVector{R{2},R{2}})) == direct_q,
                "induced relation/block metric disagrees with glued global quadrance");

        // Scaling the primary branch amplitudes scales the glued field linearly
        // and its quadrance quadratically.
        const auto scaled0 = fibers.realize_relation(core::Grade{1}, 0, numeric::RationalVector{R{6}});
        const auto scaled1 = fibers.realize_relation(core::Grade{1}, 1, numeric::RationalVector{R{6}});
        const auto scaled_occ = cover.scatter_sections({scaled0, scaled1});
        const auto scaled_global = cover.sum_assemble(scaled_occ);
        for (std::size_t i = 0; i < direct.size(); ++i) require(scaled_global[i] == 3 * direct[i], "branch block scaling failed through gluing");
        require(numeric::dot(scaled_global, scaled_global) == 9 * direct_q, "branch quadrance did not scale quadratically");

        std::cout << "branching stalk/sheaf/gluing tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
