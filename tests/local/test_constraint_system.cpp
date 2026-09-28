#include "rfc/core/builder.hpp"
#include "rfc/local/constraint_system.hpp"
#include "rfc/local/constraint_sheaf.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <type_traits>

static_assert(std::is_same_v<rfc::local::FiniteConstraintSheaf, rfc::local::FiniteIntegerConstraintSystem>);

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex triangle() {
    using namespace rfc;
    core::RelationSignature sig;
    const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge, {{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge, {{2,true,true,false,1},{0,true,false,false,1}});
    return b.freeze();
}

static rfc::algebra::SubcomplexMask edge_patch(const rfc::core::RelationalComplex& c, std::uint32_t edge) {
    rfc::algebra::SubcomplexMask patch(c);
    patch.select(rfc::core::CellId{rfc::core::Grade{1}, edge});
    patch.close_boundaries(c);
    return patch;
}

static bool independent_bruteforce(const rfc::local::FiniteConstraintSystem<std::int64_t>& sheaf) {
    using namespace rfc;
    for (std::uint64_t state = 0; state < 8u; ++state) {
        local::FiniteConstraintSystem<std::int64_t>::Assignment a;
        for (std::uint32_t i = 0; i < 3u; ++i) a[core::CellId{core::Grade{0}, i}] = static_cast<std::int64_t>((state >> i) & 1u);
        if (sheaf.globally_valid(a)) return true;
    }
    return false;
}

int main() {
    try {
        using namespace rfc;
        const auto c = triangle();
        const auto p01 = edge_patch(c, 0u);
        const auto p12 = edge_patch(c, 1u);
        const auto p20 = edge_patch(c, 2u);
        const core::CellId x{core::Grade{0},0u}, y{core::Grade{0},1u}, z{core::Grade{0},2u};

        // Constraint-complete cover, each patch satisfiable, no compatible global section:
        // x=y, y=z, z!=x.
        local::FiniteConstraintSystem<std::int64_t> unsat(c);
        for (const auto cell : {x,y,z}) unsat.set_boolean_domain(cell);
        unsat.add_boolean_truth_chart("x=y", p01, {x,y}, 0x9u);
        unsat.add_boolean_truth_chart("y=z", p12, {y,z}, 0x9u);
        unsat.add_boolean_truth_chart("z!=x", p20, {z,x}, 0x6u);
        const auto report = unsat.diagnose_cover({p01,p12,p20});
        require(report.every_patch_locally_satisfiable, "local Boolean charts should each be satisfiable");
        require(report.constraint_complete, "edge cover should expose every constraint");
        require(report.global_section_checked, "constraint-complete cover should run the exact global-section check");
        require(!report.global_section_exists, "inconsistent parity cycle unexpectedly has a global section");
        require(report.obstruction == local::FiniteConstraintSystem<std::int64_t>::ObstructionKind::NoCompatibleGlobalSection,
                "constraint-complete global obstruction was misclassified");
        require(report.certificate.chart_indices.size() == 3u,
                "global obstruction certificate did not retain the active chart set");
        require(report.global_unsat_core.has_value(), "global obstruction did not produce an exact unsat core");
        require(report.global_unsat_core->subset_minimal, "global unsat core is not marked subset-minimal");
        require(report.global_unsat_core->chart_indices == std::vector<std::size_t>({0u,1u,2u}),
                "parity-cycle unsat core changed unexpectedly");

        // A redundant tautological chart must not survive exact core reduction.
        local::FiniteConstraintSystem<std::int64_t> redundant(c);
        for (const auto cell : {x,y,z}) redundant.set_boolean_domain(cell);
        redundant.add_boolean_truth_chart("x=y", p01, {x,y}, 0x9u);
        redundant.add_boolean_truth_chart("y=z", p12, {y,z}, 0x9u);
        redundant.add_boolean_truth_chart("z!=x", p20, {z,x}, 0x6u);
        redundant.add_boolean_truth_chart("x tautology", p01, {x}, 0x3u);
        const auto reduced_core = redundant.exact_global_unsat_core();
        require(reduced_core.has_value(), "redundant inconsistent system unexpectedly has a global section");
        require(reduced_core->subset_minimal, "reduced global core is not subset-minimal");
        require(reduced_core->chart_indices == std::vector<std::size_t>({0u,1u,2u}),
                "redundant tautology was not removed from the exact unsat core");
        require(reduced_core->cells == std::vector<core::CellId>({x,y,z}),
                "global unsat core did not retain the exact involved cells");

        // Local unsatisfiability is distinct from a global gluing obstruction.
        // Each predicate is satisfiable by itself, but their conjunction on the
        // same boundary-closed edge patch is impossible.
        local::FiniteConstraintSystem<std::int64_t> local_conflict(c);
        local_conflict.set_boolean_domain(x);
        local_conflict.set_boolean_domain(y);
        local_conflict.add_boolean_truth_chart("x=y", p01, {x,y}, 0x9u);
        local_conflict.add_boolean_truth_chart("x!=y", p01, {x,y}, 0x6u);
        const auto local_core = local_conflict.exact_patch_unsat_core(p01);
        require(local_core.has_value(), "locally inconsistent patch did not produce an exact unsat core");
        require(local_core->chart_indices == std::vector<std::size_t>({0u,1u}),
                "local exact core did not retain the conflicting predicates");
        require(local_core->cells == std::vector<core::CellId>({x,y}),
                "local exact core did not retain its support cells");
        const auto local_report = local_conflict.diagnose_cover({p01});
        require(local_report.obstruction == local::LocalObstructionKind::LocalPatchUnsatisfiable,
                "local inconsistency was not classified before global gluing");
        require(local_report.local_unsat_cores.size() == 1u,
                "local cover diagnosis did not retain its exact patch core");
        require(local_report.local_unsat_cores[0].patch_index == 0u,
                "local exact core lost its cover-patch identity");
        require(local_report.local_unsat_cores[0].core.chart_indices == std::vector<std::size_t>({0u,1u}),
                "local cover diagnosis changed the exact core");

        // Omitted-constraint cover: the first two charts admit a glued zero section,
        // but the cover does not contain the z!=x chart as a local constraint.
        local::FiniteConstraintSystem<std::int64_t>::Assignment a01{{x,0},{y,0}};
        local::FiniteConstraintSystem<std::int64_t>::Assignment a12{{y,0},{z,0}};
        require(unsat.compatible_on_overlap(a01,p01,a12,p12), "zero local sections should agree on overlap");
        const auto glued = unsat.glue(a01,p01,a12,p12);
        require(!unsat.globally_valid(glued), "hidden union constraint should invalidate the glued assignment");
        const auto incomplete = unsat.diagnose_cover({p01,p12});
        require(!incomplete.constraint_complete, "incomplete cover was classified as constraint-complete");
        require(!incomplete.global_section_checked, "incomplete cover should not launch an unnecessary global solve");
        require(incomplete.obstruction == local::FiniteConstraintSystem<std::int64_t>::ObstructionKind::CoverOmitsConstraint,
                "omitted-constraint obstruction was misclassified");
        require(incomplete.certificate.chart_indices == std::vector<std::size_t>({2u}),
                "omitted-constraint certificate did not identify the hidden chart");
        require(incomplete.certificate.cells == std::vector<core::CellId>({x,z}),
                "omitted-constraint certificate did not retain the hidden chart support");
        require(!incomplete.global_unsat_core.has_value(),
                "omitted-constraint cover was incorrectly given a global unsat core");

        // Satisfiable version x=y, y=z, z=x has two global sections.
        local::FiniteConstraintSystem<std::int64_t> sat(c);
        for (const auto cell : {x,y,z}) sat.set_boolean_domain(cell);
        sat.add_boolean_truth_chart("x=y", p01, {x,y}, 0x9u);
        sat.add_boolean_truth_chart("y=z", p12, {y,z}, 0x9u);
        sat.add_boolean_truth_chart("z=x", p20, {z,x}, 0x9u);
        require(sat.global_section().has_value(), "consistent parity cycle has no global section");
        require(!sat.exact_global_unsat_core().has_value(), "satisfiable system produced an unsat core");
        require(independent_bruteforce(sat), "independent brute-force disagrees with satisfiable solver");

        // Deterministic adversarial audit against an independent eight-state scan.
        std::uint64_t rng = 0x9e3779b97f4a7c15ULL;
        for (int trial = 0; trial < 64; ++trial) {
            auto next = [&]() {
                rng ^= rng << 7u; rng ^= rng >> 9u; rng ^= rng << 8u; return rng;
            };
            local::FiniteConstraintSystem<std::int64_t> random(c);
            for (const auto cell : {x,y,z}) random.set_boolean_domain(cell);
            const auto t0 = next() & 0xFu;
            const auto t1 = next() & 0xFu;
            const auto t2 = next() & 0xFu;
            // Empty local predicates are intentionally skipped because add_chart
            // correctly rejects an impossible chart at declaration time.
            if (t0 == 0u || t1 == 0u || t2 == 0u) { --trial; continue; }
            random.add_boolean_truth_chart("c01", p01, {x,y}, t0);
            random.add_boolean_truth_chart("c12", p12, {y,z}, t1);
            random.add_boolean_truth_chart("c20", p20, {z,x}, t2);
            require(random.global_section().has_value() == independent_bruteforce(random),
                    "constraint-sheaf solver disagrees with independent brute-force audit");
        }

        std::cout << "finite constraint-system tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
