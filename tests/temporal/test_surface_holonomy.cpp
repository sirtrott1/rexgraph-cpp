#include "rfc/temporal/response_derivative.hpp"
#include "rfc/temporal/surface_inheritance.hpp"
#include <iostream>
using namespace rfc;
using namespace temporal;
using Q = numeric::Rational;
using I = numeric::Integer;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using G = numeric::ExactActionGuard;
using Surface = field::PreparedSquareTiledSurface;
using H = PreparedSurfaceHolonomy;
using T = PreparedSurfaceInheritance;
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
static geometry::UnivariateDomain domain(G &g, std::vector<V> holes = {}) {
    return geometry::UnivariateDomain::declare(geometry::UnivariateCoordinate::declare("s"), -32, 32, true,
                                               true, std::move(holes), g);
}
static Surface torus(std::uint32_t n, G &g,
                     field::SquareTiledOrientation o = field::SquareTiledOrientation::Positive) {
    return Surface::prepare({0}, {0}, n, o, 10, 1, g);
}
static Surface genus2(G &g, field::SquareTiledOrientation o = field::SquareTiledOrientation::Positive) {
    return Surface::prepare({1, 0, 2}, {2, 1, 0}, 2, o, 20, 1, g);
}
static std::vector<I> seam(const Surface &s, bool l = false) {
    std::vector<I> c(s.work().edges);
    auto n = s.subdivision();
    auto f = s.work().faces;
    if (l) {
        for (std::size_t square : {0u, 1u})
            for (std::size_t i = 0; i < n; ++i)
                c[f + (square * n + i) * n] = 1;
    } else {
        for (std::size_t j = 0; j < n; ++j)
            c[(n - 1) * n + j] = 1;
    }
    return c;
}
static std::vector<I> shifted(const Surface &s, std::vector<I> chi, long multiplier, G &g) {
    V u(s.work().vertices);
    for (std::size_t i = 0; i < u.size(); ++i)
        u[i] = Q{static_cast<long>(i % 3)};
    auto B = field::hodge_frame_detail::boundary(s.source(), {1}, g);
    auto exact = numeric::checked_transpose_apply(B, u, g);
    for (std::size_t i = 0; i < chi.size(); ++i)
        chi[i] = Q(Q{chi[i]} * multiplier + exact[i]).get_num();
    return chi;
}
static I edge_gcd(const std::vector<I> &chi) {
    I gcd = 0;
    for (const auto &v : chi)
        mpz_gcd(gcd.get_mpz_t(), gcd.get_mpz_t(), v.get_mpz_t());
    return gcd;
}
static V basis(std::size_t n, std::size_t i) {
    V v(n);
    v.at(i) = 1;
    return v;
}
static void vjson(const V &v) {
    std::cout << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i)
            std::cout << ',';
        std::cout << '"' << v[i] << '"';
    }
    std::cout << ']';
}
static void zjson(const std::vector<I> &v) {
    V q;
    for (const auto &z : v)
        q.emplace_back(z);
    vjson(q);
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
static void sectors_json(const H::Sectors &s) {
    std::cout << "{\"q\":[";
    for (std::size_t i = 0; i < 3; ++i) {
        if (i)
            std::cout << ',';
        std::cout << '"' << s.quadrances()[i] << '"';
    }
    std::cout << "],\"values\":[";
    for (std::size_t i = 0; i < 3; ++i) {
        if (i)
            std::cout << ',';
        vjson(s.values()[i]);
    }
    std::cout << "]}";
}
static std::vector<Q> points(bool full) {
    if (!full)
        return {Q{0}, Q{1}, Q{-1}, Q{1, 16}, Q{1, 64}, Q{1, 256}, Q{1, 1024}};
    V pos{Q{1, 1024}, Q{1, 256}, Q{1, 64}, Q{1, 16}, Q{1, 8}, Q{1, 4}, Q{1, 3}, Q{1, 2},
          Q{2, 3},    Q{1},      Q{3, 2},  Q{2},     Q{3},    Q{4},    Q{8},    Q{16}};
    V all{Q{0}};
    for (const auto &q : pos) {
        all.push_back(q);
        all.push_back(-q);
    }
    std::sort(all.begin(), all.end());
    return all;
}
static H::Jet make_jet(const H &h, const geometry::UnivariateDomain::Value &at, const V &x, G &g) {
    std::vector<V> fields, dots;
    for (std::uint16_t k = 0; k < 3; ++k) {
        auto n = 2 * h.surface().source().grade({k}).cell_count();
        fields.emplace_back(n);
        dots.emplace_back(n);
    }
    fields[1] = surface_holonomy_detail::lift(x, V(x.size()), g);
    return h.jet(at, std::move(fields), std::move(dots), g);
}
static H run(const char *name, Surface s, std::vector<I> chi, bool full = false, bool archiveL = false) {
    G g;
    auto h = H::prepare(s, OwnedSurfaceCocycle::declare(s, chi, g), domain(g), g);
    auto theorem = T::prepare(h, g);
    const auto n = s.work().edges;
    check(theorem.dimensions() ==
              std::array<std::size_t, 3>{s.work().vertices, s.work().faces, 2 * s.work().genus - 2},
          "complete inherited dimensions");
    Q identity = 0;
    for (std::size_t i = 0; i < h.class_periods().size(); ++i)
        identity += Q{h.class_periods()[i] * h.bezout()[i]};
    check(identity == 1, "complete period Bezout primitive-character witness");
    check(theorem.closed_cochains().kernel().cols() == n - s.work().faces + 1,
          "complete closed-cochain test family");
    const auto b1 = field::hodge_frame_detail::boundary(s.source(), {1}, g),
               b2 = field::hodge_frame_detail::boundary(s.source(), {2}, g);
    if (archiveL) {
        check(surface_holonomy_detail::dot(theorem.harmonic_chi(), theorem.harmonic_chi(), g) == Q{15, 8},
              "original genus2 harmonic chi quadrance");
        check(surface_holonomy_detail::dot(theorem.harmonic_psi(), theorem.harmonic_psi(), g) == Q{20, 13},
              "original genus2 harmonic psi quadrance");
    }
    std::vector<std::size_t> sources =
        archiveL ? std::vector<std::size_t>{0, 7, 17} : std::vector<std::size_t>{0, n / 2};
    if (dump) {
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << "{\"name\":\"" << name << "\",\"orientation\":"
                  << (s.orientation() == field::SquareTiledOrientation::Positive ? 1 : -1)
                  << ",\"genus\":" << s.work().genus << ",\"b1\":";
        mjson(b1);
        std::cout << ",\"b2\":";
        mjson(b2);
        std::cout << ",\"face_edges\":[";
        for (std::size_t i = 0; i < s.face_edges().size(); ++i) {
            if (i)
                std::cout << ',';
            std::cout << '[';
            for (std::size_t j = 0; j < 4; ++j) {
                if (j)
                    std::cout << ',';
                std::cout << s.face_edges()[i][j];
            }
            std::cout << ']';
        }
        std::cout << "],\"edge_vertices\":[";
        const auto &edges = s.source().grade({1});
        for (std::size_t e = 0; e < edges.cell_count(); ++e) {
            if (e)
                std::cout << ',';
            auto [begin, end] = edges.incidence_range(e);
            (void)end;
            std::cout << '[' << edges.lower_index[begin] << ',' << edges.lower_index[begin + 1] << ']';
        }
        std::cout << "],\"chi\":";
        zjson(chi);
        std::cout << ",\"period_cycles\":";
        mjson(field::period_detail::rational(h.periods().construction().lattice().integral_cycles(), g));
        std::cout << ",\"periods\":";
        zjson(h.class_periods());
        std::cout << ",\"bezout\":";
        zjson(h.bezout());
        std::cout << ",\"psi\":";
        vjson(h.psi());
        std::cout << ",\"harmonic_chi\":";
        vjson(theorem.harmonic_chi());
        std::cout << ",\"harmonic_psi\":";
        vjson(theorem.harmonic_psi());
        std::cout << ",\"fan_cup\":";
        mjson(theorem.fan_cup());
        std::cout << ",\"cellular_cup\":";
        mjson(theorem.cellular_cup());
        std::cout << ",\"closed\":";
        mjson(theorem.closed_cochains().kernel());
        std::cout << ",\"limit_frames\":[";
        for (std::size_t i = 0; i < 3; ++i) {
            if (i)
                std::cout << ',';
            mjson(theorem.frames()[i]);
        }
        std::cout << "],\"fields\":[";
        for (std::size_t k = 0; k < sources.size(); ++k) {
            if (k)
                std::cout << ',';
            auto x = basis(n, sources[k]);
            auto f = h.real_field(x, g);
            auto limit = theorem.decompose(f, g);
            std::cout << "{\"source\":" << sources[k] << ",\"value\":";
            vjson(x);
            std::cout << ",\"limit\":";
            sectors_json(limit);
            std::cout << '}';
        }
        std::cout << "],\"samples\":[";
    }
    auto values = points(full);
    for (std::size_t index = 0; index <= values.size(); ++index) {
        G receive;
        const bool half = index == values.size();
        std::optional<geometry::UnivariateDomain::Value> point;
        if (!half)
            point = h.domain().at(values[index], receive);
        auto sample = half ? h.half_turn(receive) : h.sample(*point, receive);
        const bool trivial = !half && values[index] == 0;
        check(sample.complex_betti() == (trivial ? std::array<std::size_t, 3>{1, 2 * s.work().genus, 1}
                                                 : std::array<std::size_t, 3>{0, 2 * s.work().genus - 2, 0}),
              "exact full twisted Betti numbers");
        check(local::GradeTensorField(sample.fibers().fibers(), {1}).matrix() == sample.boundary({1}) &&
                  local::GradeTensorField(sample.fibers().fibers(), {2}).matrix() == sample.boundary({2}),
              "native occurrence fiber assembly agrees with explicit coordinate realization");
        if (dump) {
            if (index)
                std::cout << ',';
            std::cout << "{\"s\":";
            if (half)
                std::cout << "null";
            else
                std::cout << '"' << values[index] << '"';
            std::cout << ",\"phase\":[\"" << sample.phase().real() << "\",\"" << sample.phase().imaginary()
                      << "\"],\"b1\":";
            mjson(sample.boundary({1}));
            std::cout << ",\"b2\":";
            mjson(sample.boundary({2}));
            std::cout << ",\"betti\":[" << sample.complex_betti()[0] << ',' << sample.complex_betti()[1]
                      << ',' << sample.complex_betti()[2] << "],\"fields\":[";
        }
        for (std::size_t k = 0; k < sources.size(); ++k) {
            auto f = h.real_field(basis(n, sources[k]), receive);
            auto split = sample.decompose(f, receive);
            check(split.quadrances()[0] + split.quadrances()[1] + split.quadrances()[2] == 1,
                  "full orthogonal field reconstruction");
            if (dump) {
                if (k)
                    std::cout << ',';
                sectors_json(split);
            }
        }
        if (dump)
            std::cout << "]}";
    }
    // Canonical first-order bridge: formal d/dh psi and chart d/ds=2i differ.
    auto at = h.domain().at(0, g);
    auto bridge = make_jet(h, at, basis(n, 0), g);
    const auto &jet = bridge.get(g);
    V F(2 * s.work().faces);
    for (std::size_t i = 0; i < s.work().faces; ++i)
        F[2 * i] = Q{s.fundamental_cycle().at(i)};
    auto derivative = numeric::checked_apply(jet.boundary_dots()[2], F, g);
    check(surface_holonomy_detail::part(derivative, 0, g) == V(n) &&
              surface_holonomy_detail::part(derivative, 1, g) == numeric::scale(Q{2}, h.psi()),
          "formal h derivative versus two-i chart factor");
    auto base_sample = h.sample(at, g);
    check(jet.base().boundaries()[1] == base_sample.boundary({1}) &&
              jet.base().boundaries()[2] == base_sample.boundary({2}),
          "covariant-jet point jet retains complete coordinate lift");
    auto response = PreparedResponseJet::green(jet, 1, Q{1}, Q{0}, g);
    auto answer = response.apply(OwnedDerivativeField::source(jet, 1, g), g);
    check(answer.value().size() == 2 * n, "response-derivative guarded Green derivative composes with surface chart jet");
    if (dump) {
        std::cout << "],\"jet_at_zero\":{\"b1_dot\":";
        mjson(jet.boundary_dots()[1]);
        std::cout << ",\"b2_dot\":";
        mjson(jet.boundary_dots()[2]);
        std::cout << ",\"green_value\":";
        vjson(answer.value());
        std::cout << ",\"green_dot\":";
        vjson(answer.dot());
        std::cout << "}}";
    }
    return h;
}
int main(int argc, char **argv) {
    try {
        dump = argc == 2 && std::string_view(argv[1]) == "--dump";
        if (dump)
            std::cout << '[';
        G g;
        auto t3 = torus(3, g);
        auto c3 = seam(t3);
        auto h = run("torus3", t3, c3, true);
        auto L = genus2(g);
        auto lc = seam(L, true);
        auto lh = run("L2", L, lc, false, true);
        auto small = torus(1, g);
        run("torus1-repeated-flags", small, seam(small));
        auto shifted_chi = shifted(t3, c3, 1, g);
        auto shifted_h = run("torus-coboundary-shift", t3, shifted_chi);
        auto neg = c3;
        for (auto &x : neg)
            x = -x;
        run("torus-negative-class", t3, neg);
        auto Lo = genus2(g, field::SquareTiledOrientation::Negative);
        run("L2-negative-orientation", Lo, seam(Lo, true));
        run("L2-coboundary-shift", L, shifted(L, lc, 1, g));
        auto coupled = c3;
        for (std::size_t i = 0; i < 3; ++i)
            coupled[9 + 3 * i + 2] = 1;
        run("torus-coupled-periods", t3, coupled);
        auto t2 = torus(2, g);
        run("torus2", t2, seam(t2));
        auto a = T::prepare(h, g), b = T::prepare(shifted_h, g);
        check(a.harmonic_chi() == b.harmonic_chi() && a.harmonic_psi() == b.harmonic_psi(),
              "coboundary shifts preserve both cohomology harmonic directions");
        auto nonprimitive = shifted(t3, c3, 2, g);
        check(edge_gcd(nonprimitive) == 1, "nonprimitive class can have edge gcd1");
        reject([&] { H::prepare(t3, OwnedSurfaceCocycle::declare(t3, nonprimitive, g), domain(g), g); },
               "period gcd2 refuses primitive theorem");
        auto zero_class = shifted(t3, std::vector<I>(18), 1, g);
        check(edge_gcd(zero_class) == 1, "nonzero exact cochain can have edge gcd1");
        reject([&] { H::prepare(t3, OwnedSurfaceCocycle::declare(t3, zero_class, g), domain(g), g); },
               "zero cohomology class refuses theorem");
        reject([&] { H::prepare(t3, OwnedSurfaceCocycle::declare(t3, std::vector<I>(18), g), domain(g), g); },
               "zero cocycle refuses theorem");
        auto not_closed = c3;
        not_closed[0] += 1;
        reject([&] { OwnedSurfaceCocycle::declare(t3, not_closed, g); }, "nonclosed cochain refuses closure");
        auto foreign = torus(3, g);
        reject([&] { H::prepare(foreign, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g), g); },
               "equal surface key/content is not actual owner");
        auto another = H::prepare(t3, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g), g);
        auto wrong_field = another.real_field(basis(18, 0), g);
        reject([&] { a.decompose(wrong_field, g); }, "limit refuses foreign field owner");
        auto sample = h.half_turn(g);
        reject([&] { sample.decompose(wrong_field, g); }, "sample refuses foreign field owner");
        std::vector<M> unit{M::identity(9), M::identity(18), M::identity(9)};
        auto bad = unit;
        bad[0].set(0, 0, 2);
        reject(
            [&] {
                H::prepare(t3, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g), bad, t3.orientation(), g);
            },
            "nonidentity metric refuses theorem");
        reject(
            [&] {
                H::prepare(t3, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g), unit,
                           field::SquareTiledOrientation::Negative, g);
            },
            "wrong orientation refuses theorem");
        auto foreign_domain = domain(g);
        reject([&] { h.sample(foreign_domain.at(1, g), g); }, "equal parameter domain is not actual owner");
        auto excluded =
            H::prepare(t3, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g, {{0, 1}, {-1, 1}}), g);
        reject([&] { excluded.sample(excluded.domain().at(0, g), g); },
               "original hole at trivial phase is retained");
        reject([&] { excluded.sample(excluded.domain().at(1, g), g); },
               "original nonzero chart hole retained");
        auto excluded_half = excluded.half_turn(g);
        check(excluded_half.complex_betti() == std::array<std::size_t, 3>{0, 0, 0},
              "half-turn is separately defined outside finite chart");
        auto excluded_theorem = T::prepare(excluded, g);
        check(excluded_theorem.dimensions() == a.dimensions(),
              "punctured original domain retains a theorem limit");
        reject([&] { H::prepare(t3, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g, {{0}}), g); },
               "identically excluded chart cannot certify an approach");
        reject([&] { H::prepare(t3, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g), g, {1, 16}); },
               "surface cell cap");
        reject(
            [&] { H::prepare(t3, OwnedSurfaceCocycle::declare(t3, shifted_chi, g), domain(g), g, {64, 0}); },
            "transport exponent cap");
        auto carried = []() {
            G local;
            auto s = torus(1, local);
            auto plan = H::prepare(s, OwnedSurfaceCocycle::declare(s, seam(s), local), domain(local), local);
            auto theory = T::prepare(plan, local);
            return theory.decompose(plan.real_field({1, 2}, local), local);
        }();
        check(carried.quadrances()[0] + carried.quadrances()[1] == 5,
              "owned result survives producer lifetime");
        auto huge_guard = H::prepare(t3, OwnedSurfaceCocycle::declare(t3, c3, g), domain(g, {{123456}}), g);
        G narrow({4096, 16000000, 64000000, 3});
        reject([&] { huge_guard.half_turn(narrow); },
               "receiving bits revalidate original unused guard coefficients");
        G low({4096, 1, 64000000, 262144});
        reject([&] { h.half_turn(low); }, "receiving aggregate retention cap");
        auto complex_field = h.field(basis(18, 0), basis(18, 9), g);
        auto complex_limit = a.decompose(complex_field, g);
        auto real_limit = a.decompose(h.real_field(basis(18, 0), g), g),
             imag_limit = a.decompose(h.real_field(basis(18, 9), g), g);
        for (std::size_t k = 0; k < 3; ++k)
            check(complex_limit.quadrances()[k] == real_limit.quadrances()[k] + imag_limit.quadrances()[k],
                  "complex limit keeps both declared field components");
        auto complex_sample = h.sample(h.domain().at(1, g), g);
        auto sc = complex_sample.decompose(complex_field, g),
             sr = complex_sample.decompose(h.real_field(basis(18, 0), g), g),
             si = complex_sample.decompose(h.real_field(basis(18, 9), g), g);
        for (std::size_t k = 0; k < 3; ++k) {
            V expected = sr.values()[k];
            for (std::size_t i = 0; i < 18; ++i) {
                expected[2 * i] -= si.values()[k][2 * i + 1];
                expected[2 * i + 1] += si.values()[k][2 * i];
            }
            check(sc.values()[k] == expected,
                  "sample retains complex linearity without pooling input components");
        }
        G measured;
        auto m = T::prepare(h, measured);
        for (auto stop : {std::size_t{1}, measured.steps() / 2, measured.steps()}) {
            std::size_t n = 0;
            G cancel({}, [&] {
                if (++n == stop)
                    throw std::runtime_error("cancelled");
            });
            reject([&] { T::prepare(h, cancel); }, "theorem early/middle/final cancellation");
        }
        G measured_sample;
        auto ms = h.half_turn(measured_sample);
        for (auto stop : {std::size_t{1}, measured_sample.steps() / 2, measured_sample.steps()}) {
            std::size_t n = 0;
            G cancel({}, [&] {
                if (++n == stop)
                    throw std::runtime_error("cancelled");
            });
            reject([&] { h.half_turn(cancel); }, "sample early/middle/final cancellation");
        }
        (void)m;
        (void)ms;
        if (dump)
            std::cout << "]\n";
        else
            std::cout << checks
                      << " surface primitive-class, whole-family, duality and inheritance checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
