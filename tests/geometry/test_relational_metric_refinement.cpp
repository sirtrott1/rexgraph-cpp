#include "rfc/core/builder.hpp"
#include "rfc/field/boundary.hpp"
#include "rfc/field/exact_metric.hpp"
#include "rfc/geometry/algebraic_locus.hpp"
#include "rfc/geometry/chain_map.hpp"
#include "rfc/local/section.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace rfc;
using Q = numeric::Rational;
using S = numeric::SparseRationalVector;
using P = geometry::ExactPolynomial;
using Metric = field::ExactMetric;
using Map = geometry::RelationalChainMap;
constexpr core::Grade nodes{0}, edges{1}, faces{2};
std::size_t checks{}, field_cases{}, coefficient_checks{};

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F fn, const char* message) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, message);
}
bool equal(const S& a, const S& b) {
    return a.size() == b.size() && a.entries() == b.entries();
}
Q at(const S& x, std::size_t index) {
    if (index >= x.size()) throw std::out_of_range("test field index");
    for (const auto& entry : x.entries()) if (entry.index == index) return entry.value;
    return Q{0};
}
P sub(const P& a, const P& b) { return a.add(b.scale(Q{-1})); }
P square(const P& a) { return a.multiply(a); }

// A filled relational cycle, with the first relation optionally subdivided.
// Keys, endpoints and retained occurrences define the carrier; no embedding is
// supplied. The filling makes the adjacent chain axiom non-vacuous.
core::RelationalComplex triangle(bool subdivided, bool broken_face = false) {
    core::RelationSignature signature;
    const auto edge = signature.add_type("oriented relation", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("filled relation cycle", core::BoundaryPolicy::DeclaredExact, 3, 4);
    core::RelationalComplexBuilder builder(signature,
        core::SemanticScale{static_cast<std::uint16_t>(subdivided ? 0u : 1u)});
    for (const core::IdentityKey key : {101u, 307u, 911u}) builder.declare_witness_identity(key);
    if (subdivided) builder.declare_witness_identity(503u);
    const auto add_edge = [&](std::uint64_t key, core::IdentityKey from, core::IdentityKey to) {
        return builder.add_grade_one(core::RelationKey{key}, edge,
            {{from, true, true, Q{1}}, {to, true, false, Q{1}}});
    };
    if (subdivided) {
        add_edge(1001, 101, 503);
        add_edge(1002, 503, 307);
    } else add_edge(1000, 101, 307);
    add_edge(2000, 307, 911);
    add_edge(3000, 911, 101);
    std::vector<core::HigherIncidenceInput> filling;
    const std::uint32_t count = subdivided ? 4u : 3u;
    for (std::uint32_t i = 0; i < count; ++i)
        if (!(broken_face && i == 1u)) filling.emplace_back(i, true, false, Q{1});
    builder.add_higher(faces, core::RelationKey{4000}, face, filling);
    return builder.freeze();
}

std::vector<geometry::LinearGradeMap> refinement_images(bool broken = false) {
    std::vector<geometry::LinearGradeMap> maps(3);
    maps[0] = {{{0, Q{1}}}, {{1, Q{1}}}, {{2, Q{1}}}};
    maps[1] = {{{0, Q{1}}, {1, Q{1}}}, {{2, Q{1}}}, {{3, Q{1}}}};
    if (broken) maps[1][0].pop_back();
    maps[2] = {{{0, Q{1}}}};
    return maps;
}
Map coarsening(const core::RelationalComplex& fine, const core::RelationalComplex& coarse,
               const Q& a, const Q& b) {
    const Q sum = a + b;
    const Q alpha = a / sum, beta = b / sum;
    std::vector<geometry::LinearGradeMap> maps(3);
    maps[0] = {{{0, Q{1}}}, {{1, Q{1}}}, {{2, Q{1}}}, {{0, beta}, {1, alpha}}};
    maps[1] = {{{0, alpha}}, {{0, beta}}, {{1, Q{1}}}, {{2, Q{1}}}};
    maps[2] = {{{0, Q{1}}}};
    return Map(fine, coarse, std::move(maps));
}

void symbolic_certificate() {
    const auto a = P::variable(5, 0), b = P::variable(5, 1);
    const auto u = P::variable(5, 2), v = P::variable(5, 3), z = P::variable(5, 4);
    const auto sum = a.add(b);
    const auto numerator = b.multiply(u).add(a.multiply(v));
    const auto deviation = sub(sum.multiply(z), numerator);
    const auto original = b.multiply(sum).multiply(square(sub(z, u)))
        .add(a.multiply(sum).multiply(square(sub(v, z))));
    const auto trace_and_square = a.multiply(b).multiply(square(sub(v, u)))
        .add(square(deviation));
    const auto residual = sub(original, trace_and_square);
    check(!original.is_zero(), "symbolic fixture must be nonzero before cancellation");
    check(residual.is_zero(), "universal rational refinement energy identity");
    check(sub(sub(numerator, sum.multiply(u)), a.multiply(sub(v, u))).is_zero(),
          "universal first voltage-drop/coarsening identity");
    check(sub(sub(sum.multiply(v), numerator), b.multiply(sub(v, u))).is_zero(),
          "universal second voltage-drop/coarsening identity");
    check(!sub(original, trace_and_square.add(square(sub(v, u)))).is_zero(),
          "wrong symbolic law must retain a nonzero residual");
    std::cout << "symbolic_energy_residual_terms=" << residual.terms().size() << '\n'
              << "symbolic_domain=Q[a,b,u,v,z]; energy interpretation requires a>0,b>0\n";
}

void check_map_columns(const Map& map) {
    check(map.is_chain_map(), "native chain-map certificate");
    for (std::size_t k = 1; k < map.source().grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        const core::Grade lower{static_cast<std::uint16_t>(k - 1)};
        for (std::size_t i = 0; i < map.source().grade(grade).cell_count(); ++i) {
            const auto source = S::basis(map.source().grade(grade).cell_count(), i);
            const auto image = map.push_field(grade, source.view());
            const auto lhs = map.target().boundary_apply_sparse(grade, image.view());
            const auto boundary = map.source().boundary_apply_sparse(grade, source.view());
            const auto rhs = map.push_field(lower, boundary.view());
            check(equal(lhs, rhs), "all-column sparse boundary naturality");
            ++coefficient_checks;
        }
    }
}

void refinement_and_energy(const core::RelationalComplex& coarse,
                           const core::RelationalComplex& fine, const Map& refine) {
    const std::array<std::pair<Q, Q>, 5> choices{{
        {Q{1}, Q{1}}, {Q{1}, Q{2}},
        {numeric::rational(2, 3), numeric::rational(7, 5)},
        {numeric::rational(11, 13), numeric::rational(17, 19)},
        {numeric::canonical_rational(Q{"1208925819614629174706177/7"}), numeric::rational(5, 11)}
    }};
    const std::array<Q, 5> values{Q{-2}, numeric::rational(-1, 3), Q{0}, numeric::rational(2, 5), Q{3}};
    const field::BoundaryOperator bc(coarse, edges), bf(fine, edges);
    for (const auto& [a, b] : choices) {
        const Q total = a + b;
        const auto restrict = coarsening(fine, coarse, a, b);
        check_map_columns(restrict);
        const auto roundtrip = geometry::compose_chain_maps(refine, restrict);
        check(geometry::same_chain_map(roundtrip, geometry::identity_chain_map(coarse)),
              "metric-dependent coarsening is a chain retraction");
        const auto coarse_r = Metric::diagonal({total, Q{2}, Q{3}});
        const auto fine_r = Metric::diagonal({a, b, Q{2}, Q{3}});
        const auto coarse_c = Metric::diagonal({Q{1} / total, numeric::rational(1, 2), numeric::rational(1, 3)});
        const auto fine_c = Metric::diagonal({Q{1} / a, Q{1} / b, numeric::rational(1, 2), numeric::rational(1, 3)});
        check(coarse_r.supports_sparse_apply() && fine_r.supports_sparse_solve(), "native sparse metric capabilities");
        for (std::size_t i = 0; i < 3; ++i) {
            const auto x = S::basis(3, i);
            const auto ix = refine.push_field(edges, x.view());
            check(equal(restrict.push_field(edges, ix.view()), x), "sparse current refinement round trip");
            for (std::size_t j = 0; j < 3; ++j) {
                const auto y = S::basis(3, j);
                const auto iy = refine.push_field(edges, y.view());
                check(coarse_r.inner_sparse(x.view(), y.view()) == fine_r.inner_sparse(ix.view(), iy.view()),
                      "all-coefficient current metric pullback identity");
                ++coefficient_checks;
            }
        }
        const S cancelled(4, {{0, b}, {1, -a}});
        check(restrict.push_field(edges, cancelled.view()).empty(), "cancelled sparse coarsening keeps no numeric zero entries");
        check(coarse.grade(edges).cell_count() == 3, "numeric cancellation cannot remove retained relations");

        for (const auto& u : values) for (const auto& v : values) for (const auto& w : values) {
            const Q midpoint = (b * u + a * v) / total;
            const S voltage_c(3, {{0, u}, {1, v}, {2, w}});
            const S voltage_f(4, {{0, u}, {1, v}, {2, w}, {3, midpoint}});
            // Native B has (-1,+1) incidence: B^T V is the potential increment
            // along the arrow. C B^T V is the gradient-oriented branch field;
            // conventional passive current along that arrow is its negative.
            // A common sign reversal preserves KCL and every energy identity.
            const auto drop_c = bc.transpose_apply(voltage_c.view());
            const auto drop_f = bf.transpose_apply(voltage_f.view());
            const auto current_c = coarse_c.apply(drop_c.view());
            const auto current_f = fine_c.apply(drop_f.view());
            check(equal(current_f, refine.push_field(edges, current_c.view())), "harmonic voltage extension preserves oriented branch response");
            check(equal(fine_r.solve(drop_f.view()), current_f), "resistance and conductance are exact dual actions");
            check(coarse_c.inner_sparse(drop_c.view(), drop_c.view()) == fine_c.inner_sparse(drop_f.view(), drop_f.view()),
                  "exact electrical trace energy on retained relations");
            const auto injection_c = bc.apply(current_c.view());
            const auto injection_f = bf.apply(current_f.view());
            check(equal(injection_f, refine.push_field(nodes, injection_c.view())), "old-terminal response and internal KCL preserved");
            check(at(injection_f, 3) == 0, "new internal node has exact zero current residual");
            const Q deviation = numeric::rational(2, 7);
            const S perturbed(4, {{0, u}, {1, v}, {2, w}, {3, Q(midpoint + deviation)}});
            const auto perturbed_drop = bf.transpose_apply(perturbed.view());
            const Q energy_gap = fine_c.inner_sparse(perturbed_drop.view(), perturbed_drop.view()) -
                                 fine_c.inner_sparse(drop_f.view(), drop_f.view());
            check(energy_gap == total * deviation * deviation / (a * b) && energy_gap > 0,
                  "injected field deviation has exact positive energy certificate");
            ++field_cases;
        }
    }
    const S old_voltage(3, {{1, Q{1}}}), new_voltage(4, {{1, Q{1}}, {3, numeric::rational(1, 2)}});
    const auto old_drop = bc.transpose_apply(old_voltage.view());
    const auto new_drop = bf.transpose_apply(new_voltage.view());
    const auto good_c = Metric::diagonal({numeric::rational(1, 2), numeric::rational(1, 2), numeric::rational(1, 3)});
    const auto wrong_c = Metric::diagonal({numeric::rational(1, 2), numeric::rational(1, 2), numeric::rational(1, 2), numeric::rational(1, 3)});
    const Q wrong_gap = wrong_c.inner_sparse(new_drop.view(), new_drop.view()) -
                        good_c.inner_sparse(old_drop.view(), old_drop.view());
    check(wrong_gap == numeric::rational(-1, 4), "wrong refinement metric yields exact energy mismatch");
    check(refine.is_chain_map(), "energy mismatch does not fabricate a chain defect");
    std::cout << "wrong_metric_energy_residual=" << wrong_gap << '\n';
}

void gluing(const core::RelationalComplex& fine) {
    const local::RelationalLocalCarrier carrier(fine);
    const auto left_patch = carrier.cell_patch({edges, 0});
    const auto right_patch = carrier.cell_patch({edges, 1});
    const core::CellId middle{nodes, 3};
    local::BasicSection<S> global;
    global.emplace(core::CellId{nodes, 0}, S(1));
    global.emplace(core::CellId{nodes, 1}, S::basis(1, 0, Q{1}));
    global.emplace(middle, S::basis(1, 0, numeric::rational(1, 3)));
    global.emplace(core::CellId{edges, 0}, S::basis(1, 0, numeric::rational(1, 3)));
    global.emplace(core::CellId{edges, 1}, S::basis(1, 0, numeric::rational(2, 3)));
    const auto left = local::restrict_section(carrier, global, left_patch);
    const auto right = local::restrict_section(carrier, global, right_patch);
    const auto merged = local::glue_sections(carrier, left, left_patch, right, right_patch,
                                           local::OverlapCoverage::RequireComplete, equal);
    check(merged.size() == 5, "glued section retains all five actual cells");
    check(merged.at({nodes, 0}).empty(), "present zero-valued stalk remains an explicit structural key");
    for (const auto& [cell, value] : global) check(equal(merged.at(cell), value), "glued section recovers exact local states");
    for (const auto& [cell, value] : local::restrict_section(carrier, merged, left_patch))
        check(equal(left.at(cell), value), "gluing followed by native restriction recovers left patch");
    auto bad = right;
    bad.at(middle) = S::basis(1, 0, numeric::rational(4, 3));
    rejects([&]{ (void)local::glue_sections(carrier, left, left_patch, bad, right_patch,
                       local::OverlapCoverage::RequireComplete, equal); }, "inconsistent overlap must reject");
    bad = right;
    bad.erase(middle);
    rejects([&]{ (void)local::glue_sections(carrier, left, left_patch, bad, right_patch,
                       local::OverlapCoverage::RequireComplete, equal); }, "missing overlap must reject");

    local::BasicSection<Q> pairings;
    for (const auto& [cell, value] : global) { (void)value; pairings.emplace(cell, Q{2}); }
    const auto ml = local::restrict_section(carrier, pairings, left_patch);
    auto mr = local::restrict_section(carrier, pairings, right_patch);
    const Q scale = numeric::rational(3, 2);
    const Q chart_metric = mr.at(middle) / (scale * scale);
    const S chart_state = S::basis(1, 0, Q(scale * at(right.at(middle), 0)));
    const auto original_metric = Metric::diagonal({mr.at(middle)});
    const auto moved_metric = Metric::diagonal({chart_metric});
    check(original_metric.inner_sparse(right.at(middle).view(), right.at(middle).view()) ==
          moved_metric.inner_sparse(chart_state.view(), chart_state.view()), "exact rational chart pairing invariance");
    mr.at(middle) = chart_metric * scale * scale;
    const auto glued_pairing = local::glue_sections(carrier, ml, left_patch, mr, right_patch,
                                                   local::OverlapCoverage::RequireComplete);
    check(glued_pairing.at(middle) == 2, "local metric descriptions glue after exact pullback");
    mr.at(middle) += 1;
    rejects([&]{ (void)local::glue_sections(carrier, ml, left_patch, mr, right_patch,
                      local::OverlapCoverage::RequireComplete); }, "incompatible metric choices retain an overlap obstruction");
}
} // namespace

int main() {
    try {
        symbolic_certificate();
        const auto coarse = triangle(false), fine = triangle(true);
        const Map refine(coarse, fine, refinement_images());
        check_map_columns(refine);
        for (const auto* complex : {&coarse, &fine}) {
            const auto boundary = complex->boundary_apply_sparse(faces, S::basis(1, 0));
            check(complex->boundary_apply_sparse(edges, boundary.view()).empty(), "non-vacuous adjacent chain axiom");
        }
        const auto zero = field::ExactSparseField::zero(fine, edges);
        check(zero.stored_nonzeros() == 0 && zero.tensor().state(3).has_value() && *zero.tensor().state(3) == 0,
              "sparse zero field retains every primary relation");
        check(refine.push_field(edges, S(3).view()).empty(), "zero refinement does not allocate stored amplitudes");
        rejects([&]{ (void)refine.push_field(edges, S(4).view()); }, "sparse transport rejects a wrong carrier");
        const Map broken(coarse, fine, refinement_images(true));
        check(!broken.is_chain_map(), "missing refinement relation is detected structurally");
        rejects([&]{ (void)triangle(true, true); }, "builder rejects a filling that violates the chain axiom");
        rejects([&]{ (void)Metric::diagonal({Q{0}}); }, "zero positive-metric choice rejects");
        rejects([&]{ (void)Metric::diagonal({Q{-1}}); }, "negative positive-metric choice rejects");
        refinement_and_energy(coarse, fine, refine);
        gluing(fine);
        std::cout << "rational_metric_choices=5\nfield_cases=" << field_cases
                  << "\noperator_or_pairing_coefficients=" << coefficient_checks
                  << "\nexact_relational_metric_refinement_checks=" << checks << "\nPASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
