#include "rfc/core/axioms.hpp"
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/field/factored_action.hpp"
#include "rfc/field/field.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Exact finite certificate: one retained relational source, three permitted
// positive rational grade metrics. Every executed field operation uses sparse
// native ports. The finite certificate retains sparse basis witnesses, without
// assembling a dense operator or full-carrier numerical field. Complete basis
// checks certify identities on this entire finite carrier.
namespace {
using namespace rfc;
using Q = numeric::Rational;
using S = numeric::SparseRationalVector;
using Metric = field::ExactMetric;
using Tower = std::vector<S>;
std::size_t checks{}, adjoint_pairs{}, dirac_columns{}, projector_pairs{};

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F fn, const char* message) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, message);
}
bool same(const S& x, const S& y) {
    return x.size() == y.size() && x.entries() == y.entries();
}
S add(const S& x, const S& y) {
    numeric::ExactActionGuard guard;
    return field::factored_action_detail::add(x, y, guard);
}
S scale(const Q& q, const S& x) {
    numeric::ExactActionGuard guard;
    return field::factored_action_detail::scale(x, q, guard);
}
S subtract(const S& x, const S& y) { return add(x, scale(Q{-1}, y)); }

core::RelationalComplex make_carrier(bool inject_defect = false) {
    core::RelationSignature signature;
    const auto role = signature.aspects().add("logical-role", composite::AspectRole::RelationRole);
    const auto provenance = signature.aspects().add("retained-provenance", composite::AspectRole::Provenance);
    core::RelationConstraints constraints;
    constraints.rules.push_back(core::RelationRule::aspect_implies_existence(role));
    const auto relation = signature.add_type("retained-relation", core::BoundaryPolicy::DeclaredExact, 2, 0, constraints);
    const auto filling = signature.add_type("logical-filling", core::BoundaryPolicy::DeclaredExact, 2, 0, constraints);
    core::RelationalComplexBuilder builder(signature, core::SemanticScale{7});
    // Both loops have zero numeric boundary, but remain different primary
    // relations with distinct occurrence identities and distinct role bits.
    builder.add_grade_one(core::RelationKey{101}, relation, {
        {10, true, true, Q{1}, {{role, true}}, core::OccurrenceKey{1001}},
        {10, true, false, Q{1}, {{provenance, true}}, core::OccurrenceKey{1002}},
        {20, true, false, Q{0}, {{role, true}, {provenance, true}}, core::OccurrenceKey{1003}}
    });
    builder.add_grade_one(core::RelationKey{102}, relation, {
        {10, true, true, Q{1}, {{provenance, true}}, core::OccurrenceKey{1004}},
        {10, true, false, Q{1}, {{role, true}}, core::OccurrenceKey{1005}},
        {20, false, false, Q{0}, {{provenance, true}}, core::OccurrenceKey{1006}}
    });
    builder.add_grade_one(core::RelationKey{103}, relation, {
        {10, true, true, Q{1}, {{role, true}}, core::OccurrenceKey{1007}},
        {20, true, false, Q{1}, {{provenance, true}}, core::OccurrenceKey{1008}}
    });
    builder.add_higher(core::Grade{2}, core::RelationKey{201}, filling, {
        {0, true, false, Q{1}, {{role, true}}, core::OccurrenceKey{2001}},
        {1, true, false, Q{1}, {{provenance, true}}, core::OccurrenceKey{2002}},
        {0, true, false, Q{1}, {{provenance, true}}, core::OccurrenceKey{2003}},
        {0, true, true, Q{1}, {{role, true}}, core::OccurrenceKey{2004}},
        {2, true, false, Q{inject_defect ? 1 : 0}, {{role, true}}, core::OccurrenceKey{2005}}
    });
    return builder.freeze();
}

void retained_source_equal(const core::RelationalComplex& before,
                           const core::RelationalComplex& after) {
    check(before.grade_count() == after.grade_count(), "metric changed the grade tower");
    check(before.semantic_scale() == after.semantic_scale(), "metric changed semantic scale");
    for (std::size_t k = 0; k < before.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const auto& a = before.grade(grade);
        const auto& b = after.grade(grade);
        check(a.identities == b.identities && a.relation_identities == b.relation_identities,
              "metric changed retained primary identities");
        check(a.relation_types == b.relation_types && a.cell_scales == b.cell_scales,
              "metric changed types or exact lattice scales");
        check(a.offsets == b.offsets && a.lower_index == b.lower_index && a.upper_index == b.upper_index,
              "metric changed incidence ownership");
        check(a.occurrence_identities == b.occurrence_identities && a.lattice_coefficient == b.lattice_coefficient,
              "metric changed retained occurrences or coefficients");
        check(a.lower_offsets == b.lower_offsets && a.incidence_by_lower == b.incidence_by_lower,
              "metric changed sparse coface routing");
        check(a.aspects.aspect_count() == b.aspects.aspect_count(), "metric changed aspect schema");
        for (std::size_t p = 0; p < a.aspects.aspect_count(); ++p)
            check(a.aspects.plane(static_cast<composite::AspectId>(p)).words() ==
                  b.aspects.plane(static_cast<composite::AspectId>(p)).words(),
                  "metric changed retained Boolean existence, orientation, roles or provenance");
    }
}

Tower zero_tower(const core::RelationalComplex& source) {
    Tower result;
    for (std::size_t k = 0; k < source.grade_count(); ++k)
        result.emplace_back(source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count());
    return result;
}

// Sparse composition of the native lowering/raising ports. The full-vector
// tower convenience wrappers are intentionally not invoked by this test.
Tower dirac(const core::RelationalComplex& source, const std::vector<Metric>& metrics,
            const Tower& x, bool anti = false) {
    auto result = zero_tower(source);
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        const field::BoundaryOperator boundary(source, core::Grade{static_cast<std::uint16_t>(k)});
        result[k-1] = add(result[k-1], boundary.apply(x[k].view()));
        const auto raised = boundary.adjoint(x[k-1].view(), metrics[k-1], metrics[k]);
        result[k] = add(result[k], anti ? scale(Q{-1}, raised) : raised);
    }
    return result;
}

// This fixture has independently checked one-dimensional gradient and curl
// images. Their exact projections therefore require just native pairings and
// division by their positive quadrances, with no frame reduction or solver.
struct RankOneSectors {
    const Metric& metric;
    S gradient_direction, curl_direction;
    S project(const S& direction, const S& x) const {
        return scale(Q{metric.inner_sparse(direction.view(), x.view()) /
                       metric.inner_sparse(direction.view(), direction.view())}, direction);
    }
    std::array<S, 3> decompose(const S& x) const {
        auto gradient = project(gradient_direction, x);
        auto curl = project(curl_direction, x);
        auto harmonic = subtract(subtract(x, gradient), curl);
        return {std::move(gradient), std::move(curl), std::move(harmonic)};
    }
};

void print_sparse(const S& value) {
    std::cout << '{';
    bool first = true;
    for (const auto& entry : value.entries()) {
        if (!first) std::cout << ',';
        std::cout << entry.index << ':' << entry.value;
        first = false;
    }
    std::cout << '}';
}

void audit_metric(const char* label, const core::RelationalComplex& source,
                  const core::RelationalComplex& retained_snapshot, const std::vector<Metric>& metrics,
                  const S& expected_curl, const S& expected_harmonic, const Q& expected_defect_quadrance) {
    const field::BoundaryOperator b1(source, core::Grade{1}), b2(source, core::Grade{2});
    const auto bianchi = core::bianchi_through(source, source.max_grade());
    check(bianchi.satisfied() && bianchi.interfaces.size() == 1, "chain profile did not certify the actual source");
    check(b1.apply(b2.apply(S::basis(1, 0)).view()).empty(), "boundary of boundary is nonzero");
    for (std::size_t k = 0; k < metrics.size(); ++k) {
        const auto n = source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
        check(metrics[k].size() == n && metrics[k].is_diagonal(), "test left its exact diagonal action profile");
        for (std::size_t j = 0; j < n; ++j) {
            const auto e = S::basis(n, j);
            check(metrics[k].inner_sparse(e.view(), e.view()) > 0, "declared metric not positive on a coordinate direction");
            const auto measured = metrics[k].apply(e.view());
            check(same(metrics[k].solve(measured.view()), e), "sparse metric inverse certificate failed");
        }
    }

    // Every pair of basis inputs: the exact adjoint law is bilinear, so this
    // certifies it for every pair of rational fields on these retained grades.
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        const field::BoundaryOperator boundary(source, core::Grade{static_cast<std::uint16_t>(k)});
        for (std::size_t j = 0; j < boundary.domain_size(); ++j)
            for (std::size_t i = 0; i < boundary.codomain_size(); ++i) {
                const auto x = S::basis(boundary.domain_size(), j), y = S::basis(boundary.codomain_size(), i);
                const auto bx = boundary.apply(x.view());
                const auto adjoint = boundary.adjoint(y.view(), metrics[k-1], metrics[k]);
                check(metrics[k-1].inner_sparse(bx.view(), y.view()) == metrics[k].inner_sparse(x.view(), adjoint.view()),
                      "complete-basis exact adjoint identity failed");
                ++adjoint_pairs;
            }
    }

    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const field::ExactHodgeOperator hodge(source, grade, metrics);
        check(hodge.supports_sparse_action(), "native Hodge sparse profile unavailable");
        for (std::size_t j = 0; j < hodge.size(); ++j) {
            auto input = zero_tower(source);
            input[k] = S::basis(hodge.size(), j);
            const auto d2 = dirac(source, metrics, dirac(source, metrics, input));
            const auto a2 = dirac(source, metrics, dirac(source, metrics, input, true), true);
            const auto da = dirac(source, metrics, dirac(source, metrics, input, true));
            const auto ad = dirac(source, metrics, dirac(source, metrics, input), true);
            const auto laplacian = hodge.apply(input[k].view());
            for (std::size_t r = 0; r < input.size(); ++r) {
                const auto expected = r == k ? laplacian : S(input[r].size());
                check(same(d2[r], expected), "D squared differs from native Hodge action");
                check(same(a2[r], scale(Q{-1}, expected)), "anti-Dirac squared differs from negative Hodge");
                check(add(da[r], ad[r]).empty(), "Dirac/anti-Dirac anticommutator is nonzero");
            }
            ++dirac_columns;
        }
    }

    const field::ExactHodgeOperator hodge(source, core::Grade{1}, metrics);
    const RankOneSectors sectors{metrics[1], b1.adjoint(S::basis(2, 0).view(), metrics[0], metrics[1]), b2.apply(S::basis(1, 0))};
    check(!sectors.gradient_direction.empty() && !sectors.curl_direction.empty(), "fixture lost a required Hodge sector");
    check(metrics[1].inner_sparse(sectors.gradient_direction.view(), sectors.curl_direction.view()) == 0,
          "boundary-generated gradient and curl directions are not orthogonal");
    for (std::size_t i = 0; i < 2; ++i) {
        const auto adjoint = b1.adjoint(S::basis(2, i).view(), metrics[0], metrics[1]);
        check(same(sectors.project(sectors.gradient_direction, adjoint), adjoint), "gradient image has more than the declared one direction");
    }
    for (std::size_t i = 0; i < 1; ++i) {
        const auto boundary = b2.apply(S::basis(1, i));
        check(same(sectors.project(sectors.curl_direction, boundary), boundary), "curl image has more than the declared one direction");
    }
    std::array<std::array<S, 3>, 3> images;
    for (std::size_t j = 0; j < 3; ++j) {
        const auto e = S::basis(3, j);
        images[j] = sectors.decompose(e);
        const auto& [gradient, curl, harmonic] = images[j];
        check(same(add(add(gradient, curl), harmonic), e), "exact Hodge reconstruction failed");
        check(b1.apply(harmonic.view()).empty() && b2.adjoint(harmonic.view(), metrics[1], metrics[2]).empty(),
              "harmonic component violates a native kernel condition");
        check(hodge.apply(harmonic.view()).empty(), "harmonic component has nonzero Hodge response");
        check(hodge.apply_up(gradient.view()).empty() && hodge.apply_down(curl.view()).empty(),
              "gradient/curl channels have cross response");
        const auto energy = metrics[1].inner_sparse(e.view(), hodge.apply(e.view()).view());
        const auto boundary = b1.apply(e.view()), adjoint = b2.adjoint(e.view(), metrics[1], metrics[2]);
        check(energy == metrics[0].inner_sparse(boundary.view(), boundary.view()) +
                        metrics[2].inner_sparse(adjoint.view(), adjoint.view()), "Hodge energy factorization failed");
        for (std::size_t p = 0; p < 3; ++p) {
            const auto again = sectors.decompose(images[j][p]);
            for (std::size_t q = 0; q < 3; ++q)
                check(same(again[q], p == q ? images[j][p] : S(3)), "Hodge projector composition law failed");
        }
    }
    // All cross-sector basis pairs, not only the sectors of one input.
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            for (std::size_t p = 0; p < 3; ++p)
                for (std::size_t q = p + 1; q < 3; ++q) {
                    check(metrics[1].inner_sparse(images[i][p].view(), images[j][q].view()) == 0,
                          "complete-basis Hodge sectors are not orthogonal");
                    ++projector_pairs;
                }

    const S input(3, {{0, Q{1}}, {2, Q{1}}});
    const auto [gradient, curl, harmonic] = sectors.decompose(input);
    check(same(gradient, S::basis(3, 2)), "sample gradient differs from exact expected value");
    check(same(curl, expected_curl) && same(harmonic, expected_harmonic), "metric-dependent Hodge representatives differ from exact expected values");
    // Proposed new higher-boundary column, matching make_carrier(true).
    // Its nonzero boundary is the rejected assertion's chain defect.
    const S injected(3, {{0, Q{1}}, {1, Q{1}}, {2, Q{1}}});
    const auto defect = b1.apply(injected.view());
    check(same(defect, S(2, {{0, Q{-1}}, {1, Q{1}}})), "injected chain defect was hidden by a metric choice");
    const auto defect_quadrance = metrics[0].inner_sparse(defect.view(), defect.view());
    check(defect_quadrance == expected_defect_quadrance, "exact defect measurement differs from chosen metric");
    retained_source_equal(retained_snapshot, source);
    std::cout << label << ": curl=";
    print_sparse(curl);
    std::cout << " harmonic=";
    print_sparse(harmonic);
    std::cout << " defect_quadrance=" << defect_quadrance << '\n';
}
} // namespace

int main() {
    try {
        const auto source = make_carrier();
        const auto retained_snapshot = source;
        check(source.grade_count() == 3 && source.grade(core::Grade{0}).cell_count() == 2 &&
              source.grade(core::Grade{1}).cell_count() == 3 && source.grade(core::Grade{2}).cell_count() == 1,
              "retained carrier dimensions changed");
        check(source.grade(core::Grade{1}).incidence_count() == 8 && source.grade(core::Grade{2}).incidence_count() == 5,
              "source dropped cancelled or zero occurrences");
        check(source.relation_identity({core::Grade{1}, 0}).key == core::RelationKey{101} &&
              source.relation_identity({core::Grade{1}, 1}).key == core::RelationKey{102},
              "numerically equal loop boundaries merged primary relations");
        const field::BoundaryOperator b1(source, core::Grade{1});
        for (std::size_t i = 0; i < 2; ++i)
            check(b1.apply(S::basis(3, i)).empty(), "loop occurrences did not cancel exactly");
        const core::BoundaryOccurrenceFiber loop(source, core::Grade{1}, 0);
        const auto role = source.signature().aspects().id("logical-role");
        check(loop.size() == 3 && loop.occurrences()[2].exists && loop.occurrences()[2].coefficient == 0 &&
              loop.occurrences()[2].aspect(role), "zero boundary coefficient lost an existing logical role");
        check(source.occurrence_identity(core::Grade{1}, 2) == core::OccurrenceKey{1003}, "zero occurrence identity lost");
        const field::ExactSparseField zero = field::ExactSparseField::zero(source, core::Grade{1});
        check(zero.values().empty() && zero.tensor().support().count() == 3, "zero numeric field erased relational support");
        check(zero.tensor().state(0).has_value() && *zero.tensor().state(0) == 0, "retained zero field was confused with absence");
        numeric::WorkCounters work;
        const auto cancellation = source.boundary_apply_sparse_counted(core::Grade{1}, S::basis(3, 0).view(), work);
        check(cancellation.empty() && work.active_coordinates == 1 && work.incidences_touched == 3,
              "boundary work did not follow retained occurrence routes");

        audit_metric("unweighted", source, retained_snapshot,
                     {Metric::identity(2), Metric::identity(3), Metric::identity(1)},
                     S(3, {{0, Q{1,2}}, {1, Q{1,2}}}), S(3, {{0, Q{1,2}}, {1, Q{-1,2}}}), Q{2});
        audit_metric("integer-weighted", source, retained_snapshot,
                     {Metric::diagonal({Q{2},Q{3}}), Metric::diagonal({Q{2},Q{5},Q{7}}), Metric::diagonal({Q{11}})},
                     S(3, {{0, Q{2,7}}, {1, Q{2,7}}}), S(3, {{0, Q{5,7}}, {1, Q{-2,7}}}), Q{5});
        audit_metric("rational-weighted", source, retained_snapshot,
                     {Metric::diagonal({Q{1,2},Q{4,3}}), Metric::diagonal({Q{3,2},Q{7,4},Q{5,3}}), Metric::diagonal({Q{2,5}})},
                     S(3, {{0, Q{6,13}}, {1, Q{6,13}}}), S(3, {{0, Q{7,13}}, {1, Q{-6,13}}}), Q{11,6});

        rejects([] { (void)Metric::diagonal({Q{1}, Q{0}}); }, "zero metric entry was accepted as positive");
        rejects([] { (void)Metric::diagonal({Q{1}, Q{-1,3}}); }, "negative metric entry was accepted as positive");
        rejects([&] {
            const std::vector<Metric> wrong{Metric::identity(2), Metric::identity(4), Metric::identity(1)};
            (void)field::ExactHodgeOperator(source, core::Grade{1}, wrong);
        }, "metric on the wrong retained carrier was accepted");
        rejects([] { (void)make_carrier(true); }, "canonical builder accepted an injected nonclosed higher relation");
        std::cout << "relational metric choice: " << checks << " exact checks passed; "
                  << adjoint_pairs << " adjoint basis pairs, " << dirac_columns
                  << " full-tower basis columns, " << projector_pairs
                  << " cross-sector basis pairs; 13 retained occurrences; 4 negative controls\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
