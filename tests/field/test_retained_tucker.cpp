#include "rfc/field/exact_metric.hpp"
#include "rfc/field/retained_tucker.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

// Supplied exact frames, not fitted factors. The relation mode is the complete
// gradient/curl/harmonic frame of the retained metric-choice fixture; the other
// modes label Boolean observations. Tests also exercise a changed frame and
// metric, a temporal core update, genuine reduced support, and bounded growth.
namespace {
using namespace rfc;
using Q = numeric::Rational;
using S = numeric::SparseRationalVector;
using T = numeric::SparseRationalTensor;
using M = numeric::ExactMatrix;
using Mode = field::RetainedTuckerMode;
using Tucker = field::RetainedTuckerTensor;
std::size_t checks{};

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class E = std::invalid_argument, class F>
void rejects(F&& fn, const char* message) {
    bool rejected = false;
    try { fn(); } catch (const E&) { rejected = true; }
    check(rejected, message);
}
bool same(const S& a, const S& b) { return a.size() == b.size() && a.entries() == b.entries(); }

Mode hodge_mode(bool weighted = false, field::RetainedTuckerStats* stats = nullptr) {
    M r(3, 3), a(3, 3);
    r.set(2, 0, Q{1});
    r.set(0, 1, Q{1}); r.set(1, 1, Q{1});
    r.set(0, 2, Q{weighted ? 5 : 1}); r.set(1, 2, Q{weighted ? -2 : -1});
    a.set(0, 2, Q{1});
    a.set(1, 0, weighted ? Q{2,7} : Q{1,2});
    a.set(1, 1, weighted ? Q{5,7} : Q{1,2});
    a.set(2, 0, weighted ? Q{1,7} : Q{1,2});
    a.set(2, 1, weighted ? Q{-1,7} : Q{-1,2});
    return Mode(std::move(r), std::move(a), {}, stats);
}

T boolean_tensor() {
    // Four Boolean field observations: (1,0,1), (0,1,1), (1,1,0), (0,0,0).
    // Coordinate 7 is absent; coordinate 3 exists and is zero.
    return T(numeric::IndexDomain({3,2,2}),
        numeric::StructuralSupport::sparse(12, {0,1,2,3,4,5,6,8,9,10,11}),
        S(12, {{0,Q{1}}, {2,Q{1}}, {5,Q{1}}, {6,Q{1}}, {8,Q{1}}, {9,Q{1}}}));
}

void boolean_hodge_and_temporal_frame() {
    field::RetainedTuckerStats preparation, execution;
    const auto frame = hodge_mode(false, &preparation);
    check(preparation.left_inverse_columns == 3, "complete Hodge frame was not certified");
    check(frame.is_complete() && !frame.is_identity(), "declared frame completeness changed");
    check(frame.stored_factor_nonzeros() == 10, "unexpected Hodge frame support");
    const auto source = boolean_tensor();
    const auto representation = Tucker::encode(source,
        {frame, Mode::identity(2), Mode::identity(2)}, {}, &execution);
    const S expected(12, {{0,Q{1}}, {1,Q{1}}, {4,Q{1,2}}, {5,Q{1,2}},
                         {6,Q{1}}, {8,Q{1,2}}, {9,Q{-1,2}}});
    check(same(representation.core().values(), expected), "Boolean tensor acquired incorrect rational Hodge coordinates");
    check(representation.core().support().storage() == numeric::StructuralSupport::Storage::Full,
          "core coordinate-space declaration changed");
    const auto reconstructed = representation.reconstruct({}, &execution);
    check(reconstructed.domain() == source.domain(), "reconstruction changed the original tensor domain");
    check(same(reconstructed.values(), source.values()), "Boolean values were not reconstructed exactly");
    for (std::size_t i = 0; i < source.domain().logical_size(); ++i)
        check(reconstructed.state(i) == source.state(i), "source structural support or values changed");
    check(reconstructed.state(3).has_value() && *reconstructed.state(3) == 0,
          "existing zero became absent");
    check(!reconstructed.state(7).has_value(), "absent coordinate became an existing zero");
    check(representation.reconstruction_residual(source.values()).empty(), "certified state has nonzero residual");
    const T packed(source.domain(), numeric::StructuralSupport::packed(12, {0,1,2,3,4,5,6,8,9,10,11}),
                   source.values());
    const auto packed_restored = Tucker::encode(packed, representation.modes()).reconstruct();
    check(packed_restored.support().storage() == numeric::StructuralSupport::Storage::PackedBits &&
          packed_restored.state(3) == Q{0} && !packed_restored.state(7),
          "packed source support was not preserved independently of numeric values");

    auto changed = source.values().entries();
    changed.front().value = Q{2};
    check(same(representation.reconstruction_residual(S(12, changed)), S::basis(12, 0, Q{-1})),
          "nonzero reconstruction mismatch was not measured exactly");

    const auto weighted_frame = hodge_mode(true);
    const auto weighted = representation.reframe({weighted_frame, Mode::identity(2), Mode::identity(2)});
    check(same(weighted.reconstruct().values(), source.values()), "temporal frame change altered observed values");
    check(*weighted.core().state(4) == Q{2,7} && *weighted.core().state(8) == Q{1,7},
          "changed metric/frame has incorrect exact coordinates");
    check(!same(weighted.core().values(), representation.core().values()),
          "frame change was hidden from retained coordinates");
    const auto metric = field::ExactMetric::diagonal({Q{2}, Q{5}, Q{7}});
    const auto* r = weighted_frame.reconstruction_factor();
    for (std::size_t i = 0; i < 3; ++i) {
        const auto u = r->apply_sparse(S::basis(3, i).view());
        for (std::size_t j = 0; j < 3; ++j) {
            const auto v = r->apply_sparse(S::basis(3, j).view());
            const auto value = metric.inner_sparse(u.view(), v.view());
            check(value == (i != j ? Q{0} : (i == 2 ? Q{70} : Q{7})),
                  "weighted frame does not pull back the declared metric correctly");
        }
    }
    // Update one local pattern in exact core coordinates. The new nonzero at
    // source index 4 was an existing zero; creating it changes state, not support.
    auto evolved_core = expected.entries();
    for (auto& entry : evolved_core) if (entry.index == 8) entry.value = Q{-1,2};
    const auto evolved = representation.with_core(S(12, evolved_core));
    auto evolved_values = source.values().entries();
    evolved_values.erase(evolved_values.begin());
    evolved_values.push_back({4,Q{1}});
    check(same(evolved.reconstruct().values(), S(12, evolved_values)),
          "temporal exact core update failed to reconstruct its Boolean field");
    check(!evolved.reconstruct().state(7), "temporal update fabricated an absent source entry");
    check(same(Tucker::encode(evolved.reconstruct(), evolved.modes()).core().values(), evolved.core().values()),
          "temporal core update lost accession/reconstruction consistency");
    rejects([&] { (void)representation.with_core(S::basis(12, 7)); },
            "core update created a source coordinate outside retained support");
    rejects([&] { (void)representation.with_core(S(11)); }, "wrong core domain was accepted");
    check(same(representation.reconstruct().values(), source.values()), "failed update mutated its original representation");

    std::cout << "Boolean Hodge tensor: source_nonzeros=" << source.stored_nonzeros()
              << " core_nonzeros=" << representation.core().stored_nonzeros()
              << " exact_frame_columns=" << preparation.left_inverse_columns
              << " routed_products=" << execution.routed_products << '\n';
}

void arbitrary_modes_and_negative_controls() {
    M shear(2,2), unshear(2,2), swap(2,2);
    shear.set(0,0,Q{1}); shear.set(0,1,Q{1}); shear.set(1,1,Q{1});
    unshear.set(0,0,Q{1}); unshear.set(0,1,Q{-1}); unshear.set(1,1,Q{1});
    swap.set(0,1,Q{1}); swap.set(1,0,Q{1});
    const auto source = boolean_tensor();
    const auto all_modes = Tucker::encode(source,
        {hodge_mode(), Mode(shear, unshear), Mode(swap, swap)});
    check(same(all_modes.reconstruct().values(), source.values()),
          "middle/last tensor mode routing did not reconstruct exactly");
    check(all_modes.reconstruction_residual(source.values()).empty(), "multi-mode tensor residual is nonzero");

    // Every Boolean assignment on the complete three-direction field.
    for (unsigned bits = 0; bits < 8; ++bits) {
        std::vector<numeric::SparseRationalEntry> values;
        for (std::size_t i = 0; i < 3; ++i) if ((bits >> i) & 1u) values.push_back({i,Q{1}});
        const T field(numeric::IndexDomain({3}), numeric::StructuralSupport::full(3), S(3, values));
        const auto t = Tucker::encode(field, {hodge_mode()});
        check(same(t.reconstruct().values(), field.values()), "a Boolean field assignment was lost");
    }

    M bad_r(2,2), bad_a(2,2);
    bad_r.set(0,0,Q{1}); bad_r.set(1,1,Q{1});
    bad_a.set(0,0,Q{1}); bad_a.set(1,1,Q{2});
    rejects([&] { (void)Mode(bad_r, bad_a); }, "incorrect exact dual factor was accepted");
    rejects([] { (void)Mode(M(1000000000,1000000000), M(1000000000,1000000000)); },
            "missing billion-dimensional frame columns were accepted");
    rejects([&] { (void)Tucker::encode(source, {Mode::identity(12)}); }, "wrong tensor mode count was accepted");
    rejects([&] { (void)Tucker::encode(source, {Mode::identity(4),Mode::identity(2),Mode::identity(2)}); },
            "wrong ambient mode extent was accepted");

    M reduced_r(3,2), reduced_a(2,3);
    reduced_r.set(0,0,Q{1}); reduced_r.set(2,1,Q{1});
    reduced_a.set(0,0,Q{1}); reduced_a.set(1,2,Q{1});
    const Mode reduced(reduced_r, reduced_a);
    check(!reduced.is_complete(), "reduced frame mislabeled as a complete field basis");
    const T represented(numeric::IndexDomain({3}), numeric::StructuralSupport::full(3),
                        S(3, {{0,Q{2,3}}, {2,Q{5,7}}}));
    const auto exact_reduced = Tucker::encode(represented, {reduced});
    check(exact_reduced.core().domain().logical_size() == 2 &&
          same(exact_reduced.reconstruct().values(), represented.values()), "valid reduced frame was not exact");
    const T unrepresented(numeric::IndexDomain({3}), numeric::StructuralSupport::full(3), S::basis(3,1));
    rejects([&] { (void)Tucker::encode(unrepresented, {reduced}); },
            "a reduced frame silently discarded an unrepresented direction");
    bool measured = false;
    try { (void)Tucker::encode(unrepresented, {reduced}); }
    catch (const field::RetainedTuckerReconstructionError& error) {
        measured = same(error.residual(), S::basis(3,1,Q{-1}));
    }
    check(measured, "rejected reduced frame did not retain its exact residual witness");

    // Scalars and empty modes are real domains, not exceptional allocations.
    const T scalar(numeric::IndexDomain{}, numeric::StructuralSupport::full(1), S::basis(1,0,Q{3,7}));
    check(same(Tucker::encode(scalar, {}).reconstruct().values(), scalar.values()), "scalar tensor round trip failed");
    const T empty(numeric::IndexDomain({0,4}), numeric::StructuralSupport::full(0), S(0));
    check(Tucker::encode(empty, {Mode::identity(0),Mode::identity(4)}).reconstruct().values().empty(),
          "zero-extent tensor failed to remain empty");
    const T zero_field = T::zero(numeric::IndexDomain({3}), numeric::StructuralSupport::full(3));
    const auto zero_core = Tucker::encode(zero_field, {Mode(M(3,0),M(0,3))});
    check(zero_core.core().domain().logical_size() == 0 && zero_core.reconstruct().values().empty(),
          "zero-dimensional represented subspace failed its exact certificate");
}

void enormous_logical_domain_and_explicit_growth_limits() {
    constexpr std::size_t n = 1000000000ULL;
    const numeric::IndexDomain domain({n,n});
    const auto high = (n-1)*n+17;
    const T source(domain, numeric::StructuralSupport::sparse(domain.logical_size(), {11,18,high}),
                   S(domain.logical_size(), {{11,Q{1}}, {high,Q{1}}}));
    M r(n,2), a(2,n);
    r.set(0,0,Q{1}); r.set(n-1,1,Q{1});
    a.set(0,0,Q{1}); a.set(1,n-1,Q{1});
    const field::RetainedTuckerLimits tiny{8,512,64};
    field::RetainedTuckerStats preparation, execution;
    const Mode select(r,a,tiny,&preparation);
    const auto identity = Mode::identity(n);
    const auto tensor = Tucker::encode(source, {select,identity}, tiny, &execution);
    const auto restored = tensor.reconstruct(tiny, &execution);
    check(domain.logical_size() == 1000000000000000000ULL, "large logical tensor domain changed");
    check(tensor.core().domain().logical_size() == 2000000000ULL, "large core metadata changed");
    check(identity.stored_factor_nonzeros() == 0, "symbolic identity materialized its diagonal");
    check(select.stored_factor_nonzeros() == 4, "selected frame grew with ambient dimension");
    check(preparation.left_inverse_columns == 2 && preparation.left_inverse_products == 2,
          "left inverse certification traversed more than selected factor support");
    check(execution.peak_intermediate_nonzeros == 2 && execution.routed_products == 6,
          "large-domain action did not follow the two supported routes");
    check(execution.coefficient_visits == 12, "large-domain mode action traversed unexpected coordinates");
    check(restored.support().count() == 3 && same(restored.values(),source.values()),
          "large-domain tensor failed exact support/value recovery");
    check(restored.state(18).has_value() && *restored.state(18) == 0 && !restored.state(19),
          "large-domain existing zero/absence distinction was lost");
    // A fully declared zero domain is also metadata-only. No support enumeration
    // is necessary even though every coordinate exists structurally.
    const T full_zero = T::zero(domain, numeric::StructuralSupport::full(domain.logical_size()));
    const auto zero = Tucker::encode(full_zero, {Mode::identity(n),Mode::identity(n)}, tiny);
    check(zero.reconstruct(tiny).values().empty() && zero.support().count() == domain.logical_size(),
          "full zero support forced a logical-domain expansion");

    // Explicitly supplied fan-out can require more nonzeros. Refuse that action
    // before exceeding its caller limit; keep the valid zero representation.
    M fanout(100,1), read_first(1,100);
    for (std::size_t i=0;i<100;++i) fanout.set(i,0,Q{1});
    read_first.set(0,0,Q{1});
    const Mode branching(fanout,read_first);
    const T initial = T::zero(numeric::IndexDomain({100}), numeric::StructuralSupport::full(100));
    const auto dormant = Tucker::encode(initial, {branching}, tiny);
    rejects<std::length_error>([&] { (void)dormant.with_core(S::basis(1,0),tiny); },
                               "an expanding factor bypassed the supported-entry budget");
    check(dormant.reconstruct(tiny).values().empty(), "bounded rejection mutated a valid tensor");
    const auto active = dormant.with_core(S::basis(1,0), {128,2048,64});
    check(active.reconstruct({128,2048,64}).stored_nonzeros() == 100,
          "declared exact fan-out failed with sufficient support budget");
    rejects<std::length_error>([&] { (void)tensor.reconstruct({8,1,64}); }, "step limit was ignored");
    const T high_bits(numeric::IndexDomain({1}),numeric::StructuralSupport::full(1),S::basis(1,0,Q{257}));
    rejects<std::length_error>([&] { (void)Tucker::encode(high_bits,{Mode::identity(1)},{8,512,8}); },
                               "exact integer-bit limit was ignored");

    std::cout << "Retained sparse tensor: logical_size=" << domain.logical_size()
              << " structural_entries=" << source.support().count()
              << " numeric_entries=" << source.stored_nonzeros()
              << " factor_entries=" << select.stored_factor_nonzeros()
              << " peak_intermediate_nonzeros=" << execution.peak_intermediate_nonzeros
              << " coefficient_visits=" << execution.coefficient_visits
              << " routed_products=" << execution.routed_products << '\n';
}
} // namespace

int main() {
    try {
        boolean_hodge_and_temporal_frame();
        arbitrary_modes_and_negative_controls();
        enormous_logical_domain_and_explicit_growth_limits();
        std::cout << "retained Tucker: " << checks << " exact checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
