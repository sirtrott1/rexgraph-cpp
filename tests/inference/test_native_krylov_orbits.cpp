#include "../../examples/native_frames/fixture.hpp"
#include "rfc/field/square_tiled_surface.hpp"
#include "rfc/inference/native_cayley_orbits.hpp"
#include <iostream>
using namespace rfc;
using namespace inference;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using G = numeric::ExactActionGuard;
static std::size_t checks{};
static bool dump = false, first = true;
static void check(bool yes, const char *why) {
    ++checks;
    if (!yes)
        throw std::runtime_error(why);
}
template <class F> static void reject(F f, const char *why) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught, why);
}
static core::RelationalComplex polygon(bool shell) {
    core::RelationSignature s;
    auto type = s.add_type("orbit-incidence", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(s);
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> edges =
        shell ? std::vector<std::pair<std::uint64_t, std::uint64_t>>{{0, 1}, {0, 2}, {0, 3},
                                                                     {1, 2}, {1, 3}, {2, 3}}
              : std::vector<std::pair<std::uint64_t, std::uint64_t>>{{0, 1}, {1, 2}, {2, 0}};
    for (const auto &[a, c] : edges)
        b.add_grade_one(type, {{a, true, true, false, 1}, {c, true, false, false, 1}});
    if (shell) {
        b.add_higher(core::Grade{2}, type, {{0, true, false, 1}, {3, true, false, 1}, {1, true, true, 1}});
        b.add_higher(core::Grade{2}, type, {{0, true, false, 1}, {4, true, false, 1}, {2, true, true, 1}});
        b.add_higher(core::Grade{2}, type, {{1, true, false, 1}, {5, true, false, 1}, {2, true, true, 1}});
        b.add_higher(core::Grade{2}, type, {{3, true, false, 1}, {5, true, false, 1}, {4, true, true, 1}});
    } else
        b.add_higher(core::Grade{2}, type, {{0, true, false, 1}, {1, true, false, 1}, {2, true, false, 1}});
    return b.freeze();
}
static core::RelationalComplex path() {
    core::RelationSignature s;
    auto type = s.add_type("path", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(s);
    for (std::uint64_t i = 0; i < 3; ++i)
        b.add_grade_one(type, {{i, true, true, false, 1}, {i + 1, true, false, false, 1}});
    return b.freeze();
}
static core::RelationalComplex disconnected() {
    core::RelationSignature s;
    auto type = s.add_type("disconnected", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(type, {{0, true, true, false, 1}, {1, true, false, false, 1}});
    b.add_grade_one(type, {{2, true, true, false, 1}, {3, true, false, false, 1}});
    return b.freeze();
}
struct Fixture {
    storage::NativeAccessionDirectory directory;
    PreparedNativeOperatorFamily family;
    NativeFieldValue start;
    NativeActionAvailability available{{900, 7}};
    static storage::TemporalEndpoint endpoint(core::RelationalComplex cx, core::Grade grade, const V &x,
                                              const std::vector<M> &metrics) {
        auto e = native_frame_example::endpoint(std::move(cx), 0);
        e.tensors.push_back(native_update_example::field(9000, 10 + grade.value, x));
        if (!metrics.empty())
            for (std::size_t k = 0; k < metrics.size(); ++k) {
                for (auto &tensor : e.tensors)
                    if (tensor.key == 50 + k)
                        tensor = native_update_example::map(50 + k, 10 + k, 10 + k, metrics[k],
                                                            storage::TemporalTensorRole::Metric);
            }
        return e;
    }
    static PreparedNativeOperatorFamily prepare(const storage::NativeAccessionDirectory &directory,
                                                bool metrics) {
        const auto pin =
            directory.pin(native_update_example::access(storage::NativeAccessionKind::Field, 9000));
        NativeExactTowerRequest r;
        r.profile = field::PreparedExactProfile::GlobalActions;
        r.metrics = metrics ? NativeMetricSelection::DeclaredPositive : NativeMetricSelection::NamedUnit;
        for (std::size_t k = 0; k < pin.endpoint()->source.grade_count(); ++k) {
            r.grade_carriers.push_back(10 + k);
            if (metrics)
                r.metric_tensors.push_back(directory.pin(
                    native_update_example::access(storage::NativeAccessionKind::Tensor, 50 + k)));
        }
        runtime::ExecutionHostPolicy preparation_policy;
        preparation_policy.limits.integer_bits = 1u << 28;
        return PreparedNativeOperatorFamily::prepare(
            PreparedNativeExactTower::prepare(pin, r, preparation_policy), {{900, 7}});
    }
    Fixture(core::RelationalComplex cx, core::Grade grade, V x, std::vector<M> metrics = {})
        : directory(endpoint(std::move(cx), grade, x, metrics), 900),
          family(prepare(directory, !metrics.empty())),
          start(NativeFieldValue::stored(
              directory.pin(native_update_example::access(storage::NativeAccessionKind::Field, 9000)))) {}
};
static void vjson(const V &v) {
    std::cout << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i)
            std::cout << ',';
        std::cout << '"' << v[i] << '"';
    }
    std::cout << ']';
}
static void mjson(const M &m) {
    std::cout << '[';
    for (std::size_t i = 0; i < m.rows(); ++i) {
        if (i)
            std::cout << ',';
        V row(m.cols());
        for (std::size_t j = 0; j < m.cols(); ++j)
            row[j] = m.get(i, j);
        vjson(row);
    }
    std::cout << ']';
}
static void tjson(const NativeTowerField &x) {
    std::cout << '[';
    for (std::size_t k = 0; k < x.grades.size(); ++k) {
        if (k)
            std::cout << ',';
        vjson(x.grades[k].values());
    }
    std::cout << ']';
}
static void emit(const char *name, const Fixture &f, const PreparedNativeCayleyOrbit &orbit,
                 std::size_t steps) {
    if (!dump)
        return;
    if (!first)
        std::cout << ',';
    first = false;
    std::cout << "{\"kind\":\"orbit\",\"name\":\"" << name << "\",\"step\":\"" << orbit.step()
              << "\",\"grade\":" << f.start.carrier().declaration->grade.value << ",\"boundaries\":[";
    const auto &source = f.family.tower().publication().endpoint()->source;
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        if (k > 1)
            std::cout << ',';
        mjson(field::materialize_boundary_matrix(source, core::Grade{static_cast<std::uint16_t>(k)}));
    }
    std::cout << "],\"metrics\":[";
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        if (k)
            std::cout << ',';
        mjson(f.family.numeric_family().tower().metrics()[k].materialize());
    }
    std::cout << "],\"input\":";
    vjson(f.start.values());
    std::cout << ",\"polynomial\":";
    vjson(orbit.krylov().polynomial());
    std::cout << ",\"classification\":" << static_cast<int>(orbit.kind()) << ",\"period\":";
    if (orbit.period())
        std::cout << *orbit.period();
    else
        std::cout << "null";
    std::cout << ",\"unresolved\":";
    vjson(orbit.unresolved_polynomial());
    std::cout << ",\"modes\":[";
    for (std::size_t i = 0; i < orbit.modes().size(); ++i) {
        if (i)
            std::cout << ',';
        const auto &m = orbit.modes()[i];
        std::cout << "{\"lambda\":\"" << m.eigenvalue << "\",\"cosine\":\"" << m.cosine
                  << "\",\"order\":" << m.order << ",\"component\":";
        vjson(m.component.values());
        std::cout << '}';
    }
    std::cout << "],\"orbit\":[";
    for (std::size_t i = 0; i <= steps; ++i) {
        if (i)
            std::cout << ',';
        auto sample = orbit.sample(i, f.available);
        std::cout << "{\"real\":";
        tjson(sample.value().real);
        std::cout << ",\"imaginary\":";
        tjson(sample.value().imaginary);
        std::cout << ",\"quadrance\":\"" << sample.quadrance() << "\"}";
    }
    std::cout << "]}";
}
static void run_orbit(const char *name, Fixture &f, Q step, V polynomial, NativeOrbitKind kind,
                      std::optional<std::size_t> period, std::size_t steps) {
    auto orbit = PreparedNativeCayleyOrbit::prepare(f.family, f.start, step, f.available);
    check(orbit.krylov().polynomial() == polynomial, "field-specific minimal polynomial");
    check(orbit.kind() == kind && orbit.period() == period, "full orbit classification");
    for (std::size_t i = 0; i <= std::min(steps, std::size_t{6}); ++i) {
        auto sample = orbit.sample(i, f.available);
        auto reference_sample = orbit.sample(0, f.available);
        auto reference = reference_sample.value();
        const auto &source = *f.family.numeric_family().tower().source();
        const auto &metrics = f.family.numeric_family().tower().metrics();
        field::ExactComplexTowerField expected;
        for (const auto &g : reference.real.grades)
            expected.real.grades.push_back(g.values());
        for (const auto &g : reference.imaginary.grades)
            expected.imaginary.grades.push_back(g.values());
        for (std::size_t j = 0; j < i; ++j)
            expected = field::exact_dirac_cayley_apply(source, metrics, step, expected);
        for (std::size_t k = 0; k < expected.real.grades.size(); ++k) {
            check(sample.value().real.grades[k].values() == expected.real.grades[k] &&
                      sample.value().imaginary.grades[k].values() == expected.imaginary.grades[k],
                  "native guarded Cayley agrees with original rational reference");
            const bool same_parity = (k % 2) == (f.start.carrier().declaration->grade.value % 2);
            check((same_parity ? sample.value().imaginary.grades[k] : sample.value().real.grades[k])
                          .sparse_values()
                          .nonzeros() == 0,
                  "declared starting-grade parity retained");
        }
    }
    emit(name, f, orbit, steps);
}
static void generic(const M &A, const V &x, const V &polynomial, const char *name) {
    auto e = native_frame_example::endpoint(native_update_example::source(x.size()), 0);
    e.tensors.push_back(native_update_example::map(800, 10, 10, A));
    e.tensors.push_back(native_update_example::field(9000, 10, x));
    storage::NativeAccessionDirectory d(e, 900), foreign(e, 900);
    auto action = NativeFactoredAction::tensor(
        d.pin(native_update_example::access(storage::NativeAccessionKind::Operator, 800)));
    auto input = NativeFieldValue::stored(
        d.pin(native_update_example::access(storage::NativeAccessionKind::Field, 9000)));
    const NativeActionAvailability available{{900, 7}};
    auto k = PreparedNativeKrylov::prepare(action, input, available);
    check(k.polynomial() == polynomial, "generic action minimal polynomial");
    check(k.independence().rank() == k.degree() && k.reduction().rank() == k.degree(),
          "independent rank witnesses");
    check(PreparedNativeKrylov::verify(action, input, polynomial, available).degree() == k.degree(),
          "claimed minimal recurrence recomputed");
    auto wrong = polynomial;
    wrong[0] += 1;
    reject([&] { PreparedNativeKrylov::verify(action, input, wrong, available); },
           "wrong recurrence rejected");
    auto multiple = V(polynomial.size() + 1);
    for (std::size_t i = 0; i < polynomial.size(); ++i) {
        multiple[i] -= polynomial[i];
        multiple[i + 1] += polynomial[i];
    }
    reject([&] { PreparedNativeKrylov::verify(action, input, multiple, available); },
           "nonminimal annihilator rejected");
    reject(
        [&] {
            PreparedNativeKrylov::prepare(action,
                                          NativeFieldValue::stored(foreign.pin(native_update_example::access(
                                              storage::NativeAccessionKind::Field, 9000))),
                                          available);
        },
        "foreign equal field refused");
    reject([&] { PreparedNativeKrylov::prepare(action, input, {{900, 6}}); }, "stale availability refused");
    if (k.degree())
        reject(
            [&] {
                PreparedNativeKrylov::prepare(action, input, available, runtime::ExecutionLease{},
                                              NativeKrylovLimits{k.degree() - 1});
            },
            "insufficient degree refuses completeness");
    if (dump) {
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << "{\"kind\":\"krylov\",\"name\":\"" << name << "\",\"A\":";
        mjson(A);
        std::cout << ",\"input\":";
        vjson(x);
        std::cout << ",\"polynomial\":";
        vjson(k.polynomial());
        std::cout << '}';
    }
}
int main(int argc, char **argv) {
    try {
        dump = argc == 2 && std::string_view(argv[1]) == "--dump";
        if (dump)
            std::cout << '[';
        Fixture triangle(polygon(false), {1}, {1, 0, 0});
        run_orbit("triangle", triangle, 1, {-3, 1}, NativeOrbitKind::Periodic, 3, 9);
        Fixture shell(polygon(true), {1}, {1, 0, 0, 0, 0, 0});
        run_orbit("shell-quarter", shell, Q{1, 2}, {-4, 1}, NativeOrbitKind::Periodic, 4, 12);
        run_orbit("shell-nonperiodic", shell, 1, {-4, 1}, NativeOrbitKind::Nonperiodic, {}, 12);
        G g;
        auto surface = field::PreparedSquareTiledSurface::prepare(
            {0}, {0}, 3, field::SquareTiledOrientation::Positive, 10, 1, g);
        V source(18);
        source[0] = 1;
        Fixture torus(surface.source(), {1}, source);
        run_orbit("torus", torus, 1, {0, 18, -9, 1}, NativeOrbitKind::Nonperiodic, {}, 24);
        Fixture irrational(path(), {1}, {1, 0, 0});
        run_orbit("algebraic-unresolved", irrational, 1, {-4, 10, -6, 1}, NativeOrbitKind::UnresolvedModes,
                  {}, 4);
        run_orbit("zero-step", irrational, 0, {-4, 10, -6, 1}, NativeOrbitKind::Fixed, 1, 2);
        Fixture zero(polygon(false), {1}, {0, 0, 0});
        run_orbit("zero-field", zero, 1, {1}, NativeOrbitKind::Fixed, 1, 2);
        Fixture harmonic(polygon(false), {0}, {1, 1, 1});
        run_orbit("harmonic", harmonic, 1, {0, 1}, NativeOrbitKind::Fixed, 1, 2);
        Fixture mixed(disconnected(), {1}, {1, 1}, {M::identity(4), M::diagonal({Q{2, 3}, Q{2}})});
        run_orbit("mixed-period12", mixed, 1, {3, -4, 1}, NativeOrbitKind::Periodic, 12, 12);
        Fixture sixth(disconnected(), {1}, {1, 0}, {M::identity(4), M::diagonal({Q{6}, Q{2}})});
        run_orbit("sixth-period", sixth, 1, {Q{-1, 3}, 1}, NativeOrbitKind::Periodic, 6, 6);
        Fixture high(disconnected(), {1}, {1, 0}, {M::identity(4), M::diagonal({Q{1, 65}, Q{2}})});
        run_orbit("root-outside-old-search", high, 1, {-130, 1}, NativeOrbitKind::Nonperiodic, {}, 2);
        Fixture denominator(disconnected(), {1}, {1, 0}, {M::identity(4), M::diagonal({Q{14}, Q{2}})});
        run_orbit("denominator-outside-old-search", denominator, 1, {Q{-1, 7}, 1},
                  NativeOrbitKind::Nonperiodic, {}, 2);
        Fixture positive(disconnected(), {1}, {1, 0},
                         {M::from_rows({{2, 1, 0, 0}, {1, 3, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 2}}),
                          M::from_rows({{3, 1}, {1, 2}})});
        run_orbit("general-positive-metric", positive, 1, {Q{9, 5}, -3, 1}, NativeOrbitKind::UnresolvedModes,
                  {}, 3);
        generic(M(2, 2), {1, 2}, {0, 1}, "zero-action");
        generic(M::identity(2), {1, 2}, {-1, 1}, "identity-action");
        generic(M::from_rows({{2, 1}, {0, 2}}), {0, 1}, {4, -4, 1}, "jordan");
        generic(M::from_rows({{2, 0}, {0, 2}}), {1, 1}, {-2, 1}, "repeated-eigenvalue");
        generic(M::from_rows({{2, 0}, {0, 3}}), {1, 0}, {-2, 1}, "inactive-mode");
        generic(M::from_rows({{0, 1}, {0, 0}}), {0, 1}, {0, 0, 1}, "nilpotent");
        generic(M::identity(2), {0, 0}, {1}, "generic-zero");
        auto p =
            PreparedNativeCayleyOrbit::complete(torus.family, torus.start, 1, {0, 3, 6}, torus.available);
        check(p.kind() == NativeOrbitKind::Nonperiodic, "complete rational factor coverage");
        reject(
            [&] {
                PreparedNativeCayleyOrbit::complete(torus.family, torus.start, 1, {0, 3}, torus.available);
            },
            "incomplete factor coverage refuses complete claim");
        reject(
            [&] {
                PreparedNativeCayleyOrbit::complete(torus.family, torus.start, 1, {0, 3, 3, 6},
                                                    torus.available);
            },
            "repeated factors refused");
        reject(
            [&] {
                PreparedNativeCayleyOrbit::complete(torus.family, torus.start, 1, {0, 3, 7}, torus.available);
            },
            "inactive invented mode refused");
        auto partial =
            PreparedNativeCayleyOrbit::partial(torus.family, torus.start, 1, {0, 3}, torus.available);
        check(partial.kind() == NativeOrbitKind::UnresolvedModes && !partial.period() &&
                  partial.unresolved_polynomial() == V{-6, 1},
              "incomplete declared mode analysis is explicitly unresolved");
        reject([&] { PreparedNativeCayleyOrbit::prepare(shell.family, triangle.start, 1, shell.available); },
               "foreign source carrier rejected");
        auto shell_plan =
            PreparedNativeCayleyOrbit::prepare(shell.family, shell.start, Q{1, 2}, shell.available);
        auto at0 = shell_plan.sample(0, shell.available), at2 = shell_plan.sample(2, shell.available);
        check(at0.quadrance() == at2.quadrance() && !cayley_orbit_detail::equal(at0.value(), at2.value()),
              "energy period does not identify full vector period");
        auto torus24 = p.sample(24, torus.available);
        check(torus24.value().real.grades[1].profiles().size() == 1 &&
                  torus24.value().real.grades[1].retained_action_owners() == 1,
              "iterated native output uses owned ordered history");
        for (const auto &m : p.modes())
            if (m.eigenvalue == 0) {
                auto h = m.component.values();
                check(krylov_detail::dot(h, h, g) == Q{1, 9},
                      "torus full harmonic component is fixed with quadrance1/9");
                check(field::factored_action_detail::dot(m.component.sparse_values(),
                                                         torus24.value().real.grades[1].sparse_values(),
                                                         g) == Q{1, 9},
                      "harmonic projection remains fixed after24 steps");
            }
        NativeOrbitLimits no_divisors;
        no_divisors.divisor_trials = 0;
        reject(
            [&] {
                PreparedNativeCayleyOrbit::prepare(shell.family, shell.start, 1, shell.available,
                                                   runtime::ExecutionLease{}, no_divisors);
            },
            "factor work exhaustion refuses completeness");
        check(PreparedNativeCayleyOrbit::prepare(irrational.family, irrational.start, 0, irrational.available,
                                                 runtime::ExecutionLease{}, no_divisors)
                      .kind() == NativeOrbitKind::Fixed,
              "zero step needs no spectral discovery");
        reject([&] { p.sample(257, torus.available); }, "sample step cap");
        runtime::ExecutionHostPolicy tiny;
        tiny.limits.rational_slots = 16;
        tiny.limits.sparse_entries = 16;
        reject([&] { p.sample(1, torus.available, runtime::ExecutionLease(tiny)); },
               "aggregate receiving retention refusal");
        runtime::ExecutionHostPolicy bits;
        bits.limits.integer_bits = 2;
        reject([&] { p.sample(0, torus.available, runtime::ExecutionLease(bits)); },
               "receiving original coefficient bits rechecked");
        const auto action = triangle.family.action({field::OperatorKind::Hodge, {1}, 0}, triangle.available);
        const auto limits = native_action_detail::limits(
            runtime::ExecutionLease{}, triangle.start.publications(), triangle.family.dimension_limit());
        G count(limits);
        auto measured = PreparedNativeKrylov::prepare_with_guard(action, triangle.start, triangle.available,
                                                                 runtime::ExecutionLease{}, count);
        for (const auto stop : {std::size_t{1}, count.steps() / 2, count.steps()}) {
            std::size_t at = 0;
            G cancel(limits, [&] {
                if (++at == stop)
                    throw runtime::ExecutionCancelled();
            });
            reject(
                [&] {
                    PreparedNativeKrylov::prepare_with_guard(action, triangle.start, triangle.available,
                                                             runtime::ExecutionLease{}, cancel);
                },
                "Krylov early/middle/final cancellation");
        }
        G oc(limits);
        auto om = PreparedNativeCayleyOrbit::prepare_with_guard(
            triangle.family, triangle.start, 1, triangle.available, runtime::ExecutionLease{}, oc);
        for (const auto stop : {std::size_t{1}, oc.steps() / 2, oc.steps()}) {
            std::size_t at = 0;
            G cancel(limits, [&] {
                if (++at == stop)
                    throw runtime::ExecutionCancelled();
            });
            reject(
                [&] {
                    PreparedNativeCayleyOrbit::prepare_with_guard(triangle.family, triangle.start, 1,
                                                                  triangle.available,
                                                                  runtime::ExecutionLease{}, cancel);
                },
                "orbit early/middle/final cancellation");
        }
        auto held = []() {
            Fixture local(polygon(false), {1}, {1, 0, 0});
            auto plan = PreparedNativeCayleyOrbit::prepare(local.family, local.start, 1, local.available);
            return plan.sample(3, local.available);
        }();
        check(held.value().real.grades[1].values() == V{1, 0, 0} && held.iterate() == 3,
              "sample retains exact source and full execution history after producers die");
        auto repeated = action.then(action);
        auto repeated_certificate =
            PreparedNativeKrylov::prepare(repeated, triangle.start, triangle.available);
        check(repeated_certificate.polynomial() == V{-9, 1},
              "repeated native action binds its original word");
        reject(
            [&] {
                core::RelationSignature s;
                auto type = s.add_type("nonchain", core::BoundaryPolicy::DeclaredExact, 0, 8);
                core::RelationalComplexBuilder b(s);
                b.add_grade_one(type, {{0, true, true, false, 1}, {1, true, false, false, 1}});
                b.add_higher(core::Grade{2}, type, {{0, true, false, 1}});
                Fixture bad(b.freeze(), {1}, {1});
                PreparedNativeCayleyOrbit::prepare(bad.family, bad.start, 1, bad.available);
            },
            "full chain failure cannot get an orbit certificate");
        Fixture empty(native_frame_example::empty(1), {1}, {});
        auto empty_plan = PreparedNativeCayleyOrbit::prepare(empty.family, empty.start, 1, empty.available);
        check(empty_plan.kind() == NativeOrbitKind::Fixed && empty_plan.krylov().polynomial() == V{1},
              "declared zero-dimensional source carrier");
        auto cancelled = std::make_shared<std::atomic_bool>(true);
        reject([&] { p.sample(0, torus.available, runtime::ExecutionLease({}, cancelled)); },
               "already cancelled receiving sample");
        reject(
            [&] {
                G weak({4096, 16000000, 1, 1u << 24});
                PreparedNativeKrylov::prepare_with_guard(action, triangle.start, triangle.available,
                                                         runtime::ExecutionLease{}, weak);
            },
            "aggregate work cap");
        reject(
            [&] {
                G weak({4096, 16000000, 64000000, 1u << 24});
                runtime::ExecutionHostPolicy narrower;
                narrower.limits.scalar_work = 2;
                PreparedNativeKrylov::prepare_with_guard(action, triangle.start, triangle.available,
                                                         runtime::ExecutionLease(narrower), weak);
            },
            "shared guard cannot weaken current native host policy");
        (void)measured;
        (void)om;
        if (dump)
            std::cout << "]\n";
        else
            std::cout << checks << " native Krylov, complete-factor and full-vector orbit checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
