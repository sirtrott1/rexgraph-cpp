#pragma once
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/field/square_tiled_surface.hpp"
#include "rfc/local/owned_fiber_system.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/numeric/rational_phase.hpp"
#include "rfc/temporal/covariant_family.hpp"

namespace rfc::temporal {
namespace surface_holonomy_detail {
using Q = numeric::Rational;
using I = numeric::Integer;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using Z = numeric::IntegerMatrix;
using G = numeric::ExactActionGuard;
using E = geometry::OwnedRationalExpression;
using field::period_construction_detail::add;
using field::period_construction_detail::mul;
using field::period_construction_detail::phase;
inline V column(const M &m, std::size_t j, G &g) {
    g.matrix(m);
    V v(m.rows());
    for (std::size_t i = 0; i < v.size(); ++i) {
        g.step();
        v[i] = m.get(i, j);
    }
    return v;
}
inline Q dot(const V &a, const V &b, G &g) {
    if (a.size() != b.size())
        throw std::invalid_argument("surface pairing dimension");
    Q q = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        q = g.add(q, g.mul(a[i], b[i]));
    return q;
}
inline void validate_source(const field::PreparedSquareTiledSurface &s, G &g) {
    for (std::size_t k = 0; k < 3; ++k) {
        const auto &a = s.source().grade(core::Grade{static_cast<std::uint16_t>(k)});
        g.dimension(a.cell_count());
        for (const auto &q : a.cell_scales) {
            g.step();
            g.value(q);
            if (q != 1)
                throw std::invalid_argument("surface theorem requires unit scales");
        }
        for (const auto &z : a.lattice_coefficient) {
            g.step();
            g.value(Q{z});
            if (z != 1 && z != -1)
                throw std::invalid_argument("surface theorem requires unit signed occurrences");
        }
    }
}
inline V lift(const V &re, const V &im, G &g) {
    if (re.size() != im.size())
        throw std::invalid_argument("surface complex field shape");
    g.vector(re);
    g.vector(im);
    V out(mul(2, re.size()));
    for (std::size_t i = 0; i < re.size(); ++i) {
        out[2 * i] = re[i];
        out[2 * i + 1] = im[i];
    }
    return out;
}
inline V part(const V &x, std::size_t j, G &g) {
    if (x.size() % 2)
        throw std::invalid_argument("surface complex coordinate realization");
    V v(x.size() / 2);
    for (std::size_t i = 0; i < v.size(); ++i) {
        g.step();
        v[i] = x[2 * i + j];
    }
    return v;
}
} // namespace surface_holonomy_detail
struct SurfaceHolonomyLimits {
    std::size_t max_edges{64}, max_abs_exponent{16};
};
// A cochain declaration binds the actual certified surface, not an equal matrix.
// Closedness is checked here; nonzero primitive CLASS is checked by the theorem.
class OwnedSurfaceCocycle {
    using I = surface_holonomy_detail::I;
    using G = surface_holonomy_detail::G;
    struct State {
        field::PreparedSquareTiledSurface surface;
        std::vector<I> values;
    };
    std::shared_ptr<const State> state_;
    explicit OwnedSurfaceCocycle(std::shared_ptr<const State> s) : state_(std::move(s)) {}

  public:
    static OwnedSurfaceCocycle declare(field::PreparedSquareTiledSurface surface, std::vector<I> values,
                                       G &g) {
        using namespace surface_holonomy_detail;
        validate_source(surface, g);
        if (values.size() != surface.work().edges)
            throw std::invalid_argument("surface cocycle complete edge coverage");
        V chi;
        for (const auto &z : values) {
            g.step();
            g.value(Q{z});
            chi.emplace_back(z);
        }
        auto b2 = field::period_detail::rational(
            field::period_construction_detail::boundary(surface.source(), 2, g), g);
        if (!numeric::is_zero(numeric::checked_apply(b2.transpose(), chi, g)))
            throw std::invalid_argument("surface integral cochain is not closed");
        g.step();
        return OwnedSurfaceCocycle(
            std::make_shared<const State>(State{std::move(surface), std::move(values)}));
    }
    // Typed integer vectors are exact declarations. Raw mixed coefficient lists
    // must not reach GMP's implicit truncating numeric conversion.
    static OwnedSurfaceCocycle declare(field::PreparedSquareTiledSurface,
                                       std::initializer_list<surface_holonomy_detail::Q>, G &) = delete;
    template <class T>
        requires((std::integral<T> && !std::same_as<T, bool>) || std::same_as<T, I>)
    static OwnedSurfaceCocycle declare(field::PreparedSquareTiledSurface surface,
                                       std::initializer_list<T> values, G &g) {
        std::vector<I> exact;
        g.entries(values.size());
        for (const auto &value : values) {
            g.step();
            if constexpr (std::same_as<T, I>)
                exact.push_back(value);
            else
                exact.emplace_back(std::to_string(value));
        }
        return declare(std::move(surface), std::move(exact), g);
    }
    template <std::floating_point T>
    static OwnedSurfaceCocycle declare(field::PreparedSquareTiledSurface, std::initializer_list<T>,
                                       G &) = delete;
    const field::PreparedSquareTiledSurface &surface() const & noexcept {
        return state_->surface;
    }
    const field::PreparedSquareTiledSurface &surface() const && = delete;
    const std::vector<I> &values() const & noexcept {
        return state_->values;
    }
    const std::vector<I> &values() const && = delete;
    void validate_receiving(G &g) const {
        surface_holonomy_detail::validate_source(surface(), g);
        for (const auto &z : values()) {
            g.step();
            g.value(surface_holonomy_detail::Q{z});
        }
    }
};
class PreparedSurfaceInheritance;
class PreparedSurfaceHolonomy {
    friend class PreparedSurfaceInheritance;
    using Q = surface_holonomy_detail::Q;
    using I = surface_holonomy_detail::I;
    using V = surface_holonomy_detail::V;
    using M = surface_holonomy_detail::M;
    using Z = surface_holonomy_detail::Z;
    using G = surface_holonomy_detail::G;
    using E = surface_holonomy_detail::E;
    struct State {
        field::PreparedSquareTiledSurface surface;
        OwnedSurfaceCocycle cocycle;
        field::PreparedSquareTiledPeriods periods;
        geometry::UnivariateDomain domain;
        core::OwnedRelationalSource source, lifted;
        std::vector<std::vector<I>> exponents;
        std::map<I, std::pair<E, E>> chart;
        std::vector<I> periods_chi, bezout;
        V psi;
        std::vector<M> metrics;
        SurfaceHolonomyLimits limits;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedSurfaceHolonomy(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static core::OwnedRelationalSource coordinate_lift(const core::RelationalComplex &source, G &g) {
        core::RelationSignature signature;
        const auto type =
            signature.add_type("declared-Qi-coordinate-lift", core::BoundaryPolicy::DeclaredExact, 0, 16);
        core::RelationalComplexBuilder b(signature);
        for (std::size_t i = 0; i < 2 * source.grade({0}).cell_count(); ++i)
            b.declare_witness_identity(static_cast<std::uint64_t>(i));
        for (std::uint16_t k = 1; k < 3; ++k) {
            const auto &a = source.grade({k});
            for (std::size_t cell = 0; cell < a.cell_count(); ++cell)
                for (std::size_t j = 0; j < 2; ++j) {
                    std::vector<core::BoundaryOccurrenceInput> edges;
                    std::vector<core::HigherIncidenceInput> higher;
                    const auto [begin, end] = a.incidence_range(cell);
                    for (std::size_t p = begin; p < end; ++p)
                        for (std::size_t i = 0; i < 2; ++i) {
                            g.step();
                            auto low = static_cast<std::uint32_t>(2 * a.lower_index[p] + i);
                            const bool negative = a.lattice_coefficient[p] < 0;
                            if (k == 1)
                                edges.emplace_back(low, true, negative, Q{1});
                            else
                                higher.emplace_back(low, true, negative, Q{1});
                        }
                    if (k == 1)
                        b.add_grade_one(type, edges);
                    else
                        b.add_higher({k}, type, higher);
                }
        }
        return core::OwnedRelationalSource(b.freeze());
    }
    static std::pair<E, E> expression(const geometry::UnivariateDomain &d, const I &exponent, G &g) {
        geometry::univariate_detail::Work w(g, {});
        const auto negative = exponent < 0;
        const I magnitude = negative ? -exponent : exponent;
        V re{1}, im, den{1}, br{1, 0, -1}, bi{0, negative ? Q{-2} : Q{2}}, bd{1, 0, 1};
        for (unsigned long i = 0; i < magnitude.get_ui(); ++i) {
            auto r = w.sub(w.mul(re, br), w.mul(im, bi));
            V m;
            // sub(a,-b) is the guarded polynomial sum.
            auto right = w.mul(im, br);
            for (auto &q : right)
                q = -q;
            m = w.sub(w.mul(re, bi), right);
            re = std::move(r);
            im = std::move(m);
            den = w.mul(den, bd);
        }
        auto minus_im = w.mul(im, im);
        for (auto &q : minus_im)
            q = -q;
        if (!w.sub(w.sub(w.mul(re, re), minus_im), w.mul(den, den)).empty())
            throw std::logic_error("whole rational phase power is not unitary");
        return {E::declare(d, re, den, g), E::declare(d, im, den, g)};
    }
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        s.cocycle.validate_receiving(g);
        s.domain.validate(g);
        for (const auto &group : s.exponents)
            for (const auto &n : group) {
                g.step();
                g.value(Q{n});
            }
        for (const auto &[n, pair] : s.chart) {
            g.value(Q{n});
            for (const auto *e : {&pair.first, &pair.second}) {
                g.vector(e->original_numerator());
                g.vector(e->original_denominator());
            }
        }
        for (const auto &z : s.periods_chi) {
            g.step();
            g.value(Q{z});
        }
        for (const auto &z : s.bezout) {
            g.step();
            g.value(Q{z});
        }
        g.vector(s.psi);
        g.matrix(s.periods.construction().lattice().dual_cochains());
        g.matrix(s.periods.construction().lattice().gram());
        g.matrix(s.periods.construction().lattice().cup_form());
        for (const auto &[ij, z] : s.periods.construction().lattice().integral_cycles().entries()) {
            (void)ij;
            g.step();
            g.value(Q{z});
        }
    }
    struct SampleState {
        std::shared_ptr<const State> owner;
        numeric::RationalPhase phase;
        std::optional<geometry::UnivariateDomain::Value> point;
        local::OwnedLinearFiberSystem fibers;
        M b1, b2;
        numeric::PreparedExactFrames gradient, curl, harmonic;
        field::hodge_frame_detail::Projection pg, pc, ph;
        std::array<std::size_t, 3> betti;
        std::size_t retained;
    };
    static M phase_block(const State &s, std::size_t k, std::size_t p, const numeric::RationalPhase &phase,
                         const std::optional<geometry::UnivariateDomain::Value> &at, G &g) {
        const auto &power = s.exponents[k][p];
        Q re, im;
        if (at) {
            const auto &pair = s.chart.at(power);
            re = pair.first.evaluate(*at, g);
            im = pair.second.evaluate(*at, g);
        } else {
            re = 1;
            im = 0;
            const I count = power < 0 ? -power : power;
            for (unsigned long i = 0; i < count.get_ui(); ++i) {
                auto r = g.sub(g.mul(re, phase.real()), g.mul(im, phase.imaginary()));
                auto v = g.add(g.mul(re, phase.imaginary()), g.mul(im, phase.real()));
                re = std::move(r);
                im = std::move(v);
            }
        }
        const auto coefficient =
            Q{s.surface.source().grade({static_cast<std::uint16_t>(k)}).lattice_coefficient[p]};
        M out(2, 2);
        out.set(0, 0, g.mul(coefficient, re));
        out.set(1, 1, g.mul(coefficient, re));
        out.set(1, 0, g.mul(coefficient, im));
        out.set(0, 1, g.sub(0, g.mul(coefficient, im)));
        return out;
    }
    static SampleState specialize(std::shared_ptr<const State> s, numeric::RationalPhase h,
                                  std::optional<geometry::UnivariateDomain::Value> at, G &guard) {
        using namespace surface_holonomy_detail;
        validate(*s, guard);
        if (at && !s->domain.contains(*at, guard))
            throw std::domain_error("surface finite chart point outside original declaration");
        const auto n = mul(2, s->surface.work().edges), local = mul(128, mul(add(n, 1), add(n, 1))),
                   retained = add(s->retained, local);
        auto g = phase(guard, retained);
        local::LinearRelationalFiberSystem raw(local::LinearStalkBundle(s->source.source(), 2));
        M b1(2 * s->surface.work().vertices, n), b2(n, 2 * s->surface.work().faces);
        for (std::size_t k = 1; k < 3; ++k) {
            const auto &a = s->surface.source().grade({static_cast<std::uint16_t>(k)});
            auto &b = k == 1 ? b1 : b2;
            for (std::size_t p = 0; p < a.incidence_count(); ++p) {
                auto block = phase_block(*s, k, p, h, at, g);
                raw.set_transport({static_cast<std::uint16_t>(k)}, p, block);
                for (const auto &[ij, q] : block.entries()) {
                    const auto row = 2 * a.lower_index[p] + ij.first, col = 2 * a.upper_index[p] + ij.second;
                    b.set(row, col, g.add(b.get(row, col), q));
                }
            }
        }
        local::OwnedLinearFiberSystem fibers(s->source, raw);
        if (numeric::checked_multiply(b1, b2, g).nonzeros())
            throw std::logic_error("surface twisted full chain residual");
        numeric::PreparedExactFrames gradient(b1.transpose(), g), curl(b2, g),
            harmonic(M::vertical_concat(b1, b2.transpose()), g);
        const auto r1 = gradient.rank(), r2 = curl.rank();
        const std::array<std::size_t, 3> betti{
            {(b1.rows() - r1) / 2, (n - r1 - r2) / 2, (b2.cols() - r2) / 2}};
        const auto genus = s->surface.work().genus;
        const std::array<std::size_t, 3> expected = h == numeric::RationalPhase::identity()
                                                        ? std::array<std::size_t, 3>{{1, 2 * genus, 1}}
                                                        : std::array<std::size_t, 3>{{0, 2 * genus - 2, 0}};
        if ((b1.rows() - r1) % 2 || (n - r1 - r2) % 2 || (b2.cols() - r2) % 2 || betti != expected)
            throw std::logic_error("surface rank/primitive-character theorem residual");
        const auto unit = M::identity(n);
        field::hodge_frame_detail::Projection pg(gradient.image(), unit, g), pc(curl.image(), unit, g),
            ph(harmonic.kernel(), unit, g);
        guard.step();
        return {std::move(s),
                std::move(h),
                std::move(at),
                std::move(fibers),
                std::move(b1),
                std::move(b2),
                std::move(gradient),
                std::move(curl),
                std::move(harmonic),
                std::move(pg),
                std::move(pc),
                std::move(ph),
                betti,
                retained};
    }

  public:
    class Field {
        friend class PreparedSurfaceHolonomy;
        friend class PreparedSurfaceInheritance;
        std::shared_ptr<const State> owner_;
        V value_;
        Field(std::shared_ptr<const State> s, V v) : owner_(std::move(s)), value_(std::move(v)) {}

      public:
        const V &values() const & noexcept {
            return value_;
        }
        const V &values() const && = delete;
    };
    class Sectors {
        friend class PreparedSurfaceHolonomy;
        friend class PreparedSurfaceInheritance;
        std::shared_ptr<const void> owner_;
        Field input_;
        std::array<V, 3> values_;
        std::array<Q, 3> q_;
        Sectors(std::shared_ptr<const void> owner, Field input, std::array<V, 3> values, G &g)
            : owner_(std::move(owner)), input_(std::move(input)), values_(std::move(values)) {
            using namespace surface_holonomy_detail;
            V sum(input_.values().size());
            for (std::size_t k = 0; k < 3; ++k) {
                q_[k] = dot(values_[k], values_[k], g);
                for (std::size_t i = 0; i < sum.size(); ++i)
                    sum[i] = g.add(sum[i], values_[k][i]);
            }
            if (sum != input_.values() || g.add(g.add(q_[0], q_[1]), q_[2]) != dot(sum, sum, g))
                throw std::logic_error("surface complete orthogonal sector reconstruction");
        }

      public:
        const std::array<V, 3> &values() const & noexcept {
            return values_;
        }
        const std::array<V, 3> &values() const && = delete;
        const std::array<Q, 3> &quadrances() const & noexcept {
            return q_;
        }
        const std::array<Q, 3> &quadrances() const && = delete;
    };
    class Sample {
        friend class PreparedSurfaceHolonomy;
        std::shared_ptr<const SampleState> state_;
        explicit Sample(SampleState s) : state_(std::make_shared<const SampleState>(std::move(s))) {}

      public:
        const local::OwnedLinearFiberSystem &fibers() const & noexcept {
            return state_->fibers;
        }
        const local::OwnedLinearFiberSystem &fibers() const && = delete;
        const M &boundary(core::Grade grade) const & {
            if (grade.value == 1)
                return state_->b1;
            if (grade.value == 2)
                return state_->b2;
            throw std::out_of_range("surface boundary grade");
        }
        const M &boundary(core::Grade) const && = delete;
        const std::array<std::size_t, 3> &complex_betti() const & noexcept {
            return state_->betti;
        }
        const std::array<std::size_t, 3> &complex_betti() const && = delete;
        const numeric::RationalPhase &phase() const & noexcept {
            return state_->phase;
        }
        const numeric::RationalPhase &phase() const && = delete;
        Sectors decompose(Field field, G &guard) const {
            using namespace surface_holonomy_detail;
            if (field.owner_ != state_->owner)
                throw std::invalid_argument("surface field has a foreign actual family owner");
            validate(*state_->owner, guard);
            guard.value(state_->phase.real());
            guard.value(state_->phase.imaginary());
            if (state_->point) {
                guard.value(state_->point->value());
                if (!state_->owner->domain.contains(*state_->point, guard))
                    throw std::domain_error("retained sample point excluded");
            }
            guard.matrix(state_->b1);
            guard.matrix(state_->b2);

            auto g =
                surface_holonomy_detail::phase(guard, add(state_->retained, mul(32, field.values().size())));
            g.vector(field.values());
            std::array<V, 3> parts{state_->pg.apply(field.values(), g).first,
                                   state_->pc.apply(field.values(), g).first,
                                   state_->ph.apply(field.values(), g).first};
            guard.step();
            return Sectors(state_, std::move(field), std::move(parts), g);
        }
    };
    class Jet {
        friend class PreparedSurfaceHolonomy;
        std::shared_ptr<const State> owner_;
        PreparedCovariantJet jet_;
        Jet(std::shared_ptr<const State> s, PreparedCovariantJet j)
            : owner_(std::move(s)), jet_(std::move(j)) {}

      public:
        const PreparedCovariantJet &get(G &g) const & {
            g.entries(surface_holonomy_detail::add(owner_->retained, jet_.retained_allowance()));
            validate(*owner_, g);
            jet_.validate_receiving(g);
            return jet_;
        }
        const PreparedCovariantJet &get(G &) const && = delete;
    };
    static PreparedSurfaceHolonomy prepare(field::PreparedSquareTiledSurface surface,
                                           OwnedSurfaceCocycle cocycle, geometry::UnivariateDomain domain,
                                           std::vector<M> metrics, field::SquareTiledOrientation orientation,
                                           G &guard, SurfaceHolonomyLimits limits = {}) {
        using namespace surface_holonomy_detail;
        guard.step();
        cocycle.validate_receiving(guard);
        if (!surface.same_owner(cocycle.surface()) || orientation != surface.orientation())
            throw std::invalid_argument("surface cocycle/orientation actual owner differs");
        if (surface.work().edges > limits.max_edges || limits.max_abs_exponent > 32)
            throw std::length_error("surface admitted cell/exponent profile");
        domain.validate(guard);
        if (domain.lower() >= 0 || domain.upper() <= 0)
            throw std::invalid_argument("surface limit requires a two-sided chart around zero");
        geometry::univariate_detail::Work dw(guard, {});
        for (const auto &p : domain.original_guards())
            if (dw.input(p).empty())
                throw std::domain_error("surface chart is identically excluded");
        if (metrics.size() != 3)
            throw std::invalid_argument("surface theorem complete metric coverage");
        for (std::size_t k = 0; k < 3; ++k) {
            guard.matrix(metrics[k]);
            if (metrics[k] !=
                M::identity(surface.source().grade({static_cast<std::uint16_t>(k)}).cell_count()))
                throw std::invalid_argument("surface inheritance theorem requires identity metrics");
        }
        auto retained = add(surface.work().retained_entry_allowance,
                            mul(256, mul(add(surface.work().edges, 1), add(surface.work().edges, 1))));
        auto topology = phase(guard, retained);
        auto periods = surface.integral_periods(topology);
        retained = add(retained, periods.work().retained_entry_allowance);
        auto g = phase(guard, retained);
        const auto &cycles = periods.construction().lattice().integral_cycles();
        std::vector<I> pc(cycles.cols()), bezout(cycles.cols());
        I gcd = 0;
        for (const auto &[ij, z] : cycles.entries()) {
            auto v = g.add(Q{pc[ij.second]}, g.mul(Q{z}, Q{cocycle.values()[ij.first]}));
            pc[ij.second] = v.get_num();
        }
        for (std::size_t i = 0; i < pc.size(); ++i) {
            I next, u, v;
            mpz_gcdext(next.get_mpz_t(), u.get_mpz_t(), v.get_mpz_t(), gcd.get_mpz_t(), pc[i].get_mpz_t());
            g.value(Q{next});
            g.value(Q{u});
            g.value(Q{v});
            for (auto &b : bezout)
                b = g.mul(Q{b}, Q{u}).get_num();
            bezout[i] = v;
            gcd = std::move(next);
        }
        if (gcd != 1)
            throw std::invalid_argument(
                "surface cocycle CLASS must be nonzero primitive on complete integral H1");
        Q identity = 0;
        for (std::size_t i = 0; i < pc.size(); ++i)
            identity = g.add(identity, g.mul(Q{pc[i]}, Q{bezout[i]}));
        if (identity != 1)
            throw std::logic_error("primitive character Bezout witness");
        std::vector<std::vector<I>> exponents(3);
        const auto &edges = surface.source().grade({1});
        const auto &faces = surface.source().grade({2});
        exponents[1].resize(edges.incidence_count());
        exponents[2].resize(faces.incidence_count());
        V psi(surface.work().edges);
        std::map<I, std::pair<E, E>> chart;
        for (std::size_t p = 0; p < edges.incidence_count(); ++p)
            if (edges.lattice_coefficient[p] == 1)
                exponents[1][p] = cocycle.values()[edges.upper_index[p]];
        // Coefficient-wise Laurent closure, with all original face occurrences kept.
        std::map<std::tuple<std::size_t, std::size_t, I>, Q> defect;
        for (std::size_t f = 0; f < faces.cell_count(); ++f) {
            I prefix = 0;
            const auto [begin, end] = faces.incidence_range(f);
            for (std::size_t p = begin; p < end; ++p) {
                const auto e = faces.lower_index[p];
                const bool negative = faces.lattice_coefficient[p] < 0;
                if (negative)
                    prefix -= cocycle.values()[e];
                exponents[2][p] = prefix;
                if (!negative)
                    prefix += cocycle.values()[e];
                g.value(Q{prefix});
                const auto fundamental = surface.fundamental_cycle().at(f);
                psi[e] =
                    g.add(psi[e], g.mul(Q{fundamental * faces.lattice_coefficient[p]}, Q{exponents[2][p]}));
                const auto [a, b] = edges.incidence_range(e);
                for (std::size_t q = a; q < b; ++q) {
                    auto key = std::tuple{std::size_t(edges.lower_index[q]), f,
                                          I(exponents[2][p] + exponents[1][q])};
                    defect[key] = g.add(
                        defect[key], g.mul(Q{faces.lattice_coefficient[p]}, Q{edges.lattice_coefficient[q]}));
                }
            }
            if (prefix != 0)
                throw std::invalid_argument("surface face holonomy is not identically trivial");
        }
        for (const auto &[key, value] : defect) {
            (void)key;
            if (value != 0)
                throw std::logic_error("whole Laurent family chain identity failed");
        }
        for (const auto &group : exponents)
            for (const auto &n : group) {
                const I magnitude = n < 0 ? -n : n;
                g.value(Q{n});
                if (magnitude > I{static_cast<unsigned long>(limits.max_abs_exponent)})
                    throw std::length_error("surface original transport exponent budget");
                if (!chart.contains(n))
                    chart.emplace(n, expression(domain, n, g));
            }
        for (const auto &[n, pair] : chart) {
            (void)n;
            for (const auto *e : {&pair.first, &pair.second})
                retained = add(
                    retained, mul(16, add(e->original_numerator().size(), e->original_denominator().size())));
        }
        auto original = core::OwnedRelationalSource(surface.source());
        auto lifted = coordinate_lift(surface.source(), g);
        guard.entries(retained);
        guard.step();
        return PreparedSurfaceHolonomy(std::make_shared<const State>(
            State{std::move(surface), std::move(cocycle), std::move(periods), std::move(domain),
                  std::move(original), std::move(lifted), std::move(exponents), std::move(chart),
                  std::move(pc), std::move(bezout), std::move(psi), std::move(metrics), limits, retained}));
    }
    static PreparedSurfaceHolonomy prepare(field::PreparedSquareTiledSurface surface,
                                           OwnedSurfaceCocycle cocycle, geometry::UnivariateDomain domain,
                                           G &g, SurfaceHolonomyLimits limits = {}) {
        std::vector<M> metrics;
        for (std::uint16_t k = 0; k < 3; ++k)
            metrics.push_back(M::identity(surface.source().grade({k}).cell_count()));
        auto orientation = surface.orientation();
        return prepare(std::move(surface), std::move(cocycle), std::move(domain), std::move(metrics),
                       orientation, g, limits);
    }
    const field::PreparedSquareTiledSurface &surface() const & noexcept {
        return state_->surface;
    }
    const field::PreparedSquareTiledSurface &surface() const && = delete;
    const field::PreparedSquareTiledPeriods &periods() const & noexcept {
        return state_->periods;
    }
    const field::PreparedSquareTiledPeriods &periods() const && = delete;
    const OwnedSurfaceCocycle &cocycle() const & noexcept {
        return state_->cocycle;
    }
    const OwnedSurfaceCocycle &cocycle() const && = delete;
    const std::vector<I> &class_periods() const & noexcept {
        return state_->periods_chi;
    }
    const std::vector<I> &class_periods() const && = delete;
    const std::vector<I> &bezout() const & noexcept {
        return state_->bezout;
    }
    const std::vector<I> &bezout() const && = delete;
    const V &psi() const & noexcept {
        return state_->psi;
    }
    const V &psi() const && = delete;
    const geometry::UnivariateDomain &domain() const & noexcept {
        return state_->domain;
    }
    const geometry::UnivariateDomain &domain() const && = delete;
    const std::vector<std::vector<I>> &exponents() const & noexcept {
        return state_->exponents;
    }
    const std::vector<std::vector<I>> &exponents() const && = delete;
    bool same_owner(const PreparedSurfaceHolonomy &x) const noexcept {
        return state_ == x.state_;
    }
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    Field field(V re, V im, G &g) const {
        validate(*state_, g);
        if (re.size() != surface().work().edges)
            throw std::invalid_argument("surface field source carrier");
        g.entries(surface_holonomy_detail::add(state_->retained, surface_holonomy_detail::mul(8, re.size())));
        return Field(state_, surface_holonomy_detail::lift(re, im, g));
    }
    Field real_field(V re, G &g) const {
        g.dimension(re.size());
        g.entries(surface_holonomy_detail::add(state_->retained, surface_holonomy_detail::mul(8, re.size())));
        V im(re.size());
        return field(std::move(re), std::move(im), g);
    }
    Sample sample(const geometry::UnivariateDomain::Value &at, G &g) const {
        if (!domain().contains(at, g))
            throw std::domain_error("surface chart point excluded");
        const auto s = at.value();
        auto d = g.add(1, g.mul(s, s));
        numeric::RationalPhase h(g.div(g.sub(1, g.mul(s, s)), d), g.div(g.mul(2, s), d));
        return Sample(specialize(state_, std::move(h), at, g));
    }
    Sample half_turn(G &g) const {
        return Sample(specialize(state_, numeric::RationalPhase::half_turn(), {}, g));
    }
    Jet jet(const geometry::UnivariateDomain::Value &at, std::vector<V> fields, std::vector<V> field_dots,
            G &guard) const {
        using namespace surface_holonomy_detail;
        validate(*state_, guard);
        auto point = OwnedJetPoint::declare(domain(), at, guard);
        if (fields.size() != 3 || field_dots.size() != 3)
            throw std::invalid_argument("surface jet complete field coverage");
        auto g = phase(guard, state_->retained);
        std::vector<V> values(3), dots(3);
        std::vector<M> metrics;
        CovariantJetInput input;
        covariant_family_detail::Work w(g, {});
        for (std::uint16_t k = 0; k < 3; ++k) {
            const auto n = 2 * surface().source().grade({k}).cell_count();
            metrics.push_back(M::identity(n));
            input.metric_derivatives.emplace_back(n, n);
            input.connections.emplace_back(n, n);
            if (k) {
                const auto &a = surface().source().grade({k});
                for (std::size_t cell = 0; cell < a.cell_count(); ++cell)
                    for (std::size_t j = 0; j < 2; ++j) {
                        const auto [begin, end] = a.incidence_range(cell);
                        for (std::size_t p = begin; p < end; ++p)
                            for (std::size_t i = 0; i < 2; ++i) {
                                const auto &expressions = state_->chart.at(state_->exponents[k][p]);
                                const auto &e = i == j ? expressions.first : expressions.second;
                                auto sign = Q{a.lattice_coefficient[p]};
                                if (i == 0 && j == 1)
                                    sign = -sign;
                                values[k].push_back(g.mul(sign, e.evaluate(at, g)));
                                dots[k].push_back(
                                    g.mul(sign, w.evaluate(w.derivative(e.formal()), at.value())));
                            }
                    }
            }
        }
        input.occurrence_derivatives = std::move(dots);
        input.field_derivatives = std::move(field_dots);
        auto base = OwnedJetBase::declare(state_->lifted, point, std::move(values), std::move(metrics),
                                          std::move(fields), g);
        auto result = PreparedCovariantJet::prepare(
            std::move(base), JetDirection::coordinate(domain().coordinate()), std::move(input), g);
        guard.step();
        return Jet(state_, std::move(result));
    }
};
} // namespace rfc::temporal
