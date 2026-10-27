#pragma once
#include "rfc/core/owned_source.hpp"
#include "rfc/field/exact_metric.hpp"
#include "rfc/geometry/univariate_family.hpp"
#include "rfc/numeric/smith_normal_form.hpp"
#include <string>

namespace rfc::temporal {
namespace covariant_detail {
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using Guard = numeric::ExactActionGuard;
inline std::size_t add(std::size_t a, std::size_t b) {
    return machine::checked_size_add(a, b);
}
inline std::size_t mul(std::size_t a, std::size_t b) {
    return machine::checked_size_mul(a, b);
}
inline Guard phase(Guard &g, std::size_t retained) {
    g.entries(retained);
    auto l = g.limits();
    l.entries -= retained;
    return Guard(l, [&g] { g.step(); });
}
inline M sum(M a, const M &b, Guard &g, bool subtract = false) {
    if (a.rows() != b.rows() || a.cols() != b.cols())
        throw std::invalid_argument("jet matrix sum carriers");
    g.matrix(a);
    g.matrix(b);
    for (const auto &[ij, q] : b.entries()) {
        a.set(ij.first, ij.second,
              subtract ? g.sub(a.get(ij.first, ij.second), q) : g.add(a.get(ij.first, ij.second), q));
        g.entries(a.nonzeros());
    }
    return a;
}
inline V sum(V a, const V &b, Guard &g, bool subtract = false) {
    if (a.size() != b.size())
        throw std::invalid_argument("jet vector carriers");
    g.vector(a);
    g.vector(b);
    for (std::size_t i = 0; i < a.size(); ++i)
        a[i] = subtract ? g.sub(a[i], b[i]) : g.add(a[i], b[i]);
    return a;
}
inline M product(const M &a, const M &b, Guard &g) {
    return numeric::checked_multiply(a, b, g);
}
inline V apply(const M &a, const V &b, Guard &g) {
    return numeric::checked_apply(a, b, g);
}
inline Q pairing(const V &x, const M &m, const V &y, Guard &g) {
    const auto v = apply(m, y, g);
    if (x.size() != v.size())
        throw std::invalid_argument("jet pairing carrier");
    Q q;
    for (std::size_t i = 0; i < x.size(); ++i)
        q = g.add(q, g.mul(x[i], v[i]));
    return q;
}
inline M solve(const M &a, const M &rhs, Guard &g) {
    g.matrix(a);
    g.matrix(rhs);
    if (a.rows() != a.cols() || a.rows() != rhs.rows())
        throw std::invalid_argument("jet metric solve carrier");
    M out(a.cols(), rhs.cols());
    if (!a.rows())
        return out;
    numeric::PreparedExactSolve factor(a, g);
    for (std::size_t j = 0; j < rhs.cols(); ++j) {
        V b(rhs.rows());
        for (std::size_t i = 0; i < b.size(); ++i) {
            g.step();
            b[i] = rhs.get(i, j);
        }
        const auto x = factor.apply(b, g);
        for (std::size_t i = 0; i < x.size(); ++i) {
            g.step();
            out.set(i, j, x[i]);
        }
        g.entries(out.nonzeros());
    }
    return out;
}
inline std::size_t rank(const M &a, Guard &g) {
    g.matrix(a);
    numeric::Integer d{1};
    for (const auto &[ij, q] : a.entries()) {
        (void)ij;
        g.step();
        mpz_lcm(d.get_mpz_t(), d.get_mpz_t(), q.get_den_mpz_t());
        g.value(Q{d});
    }
    numeric::IntegerMatrix z(a.rows(), a.cols());
    for (const auto &[ij, q] : a.entries()) {
        const auto v = g.mul(q, Q{d});
        z.set(ij.first, ij.second, v.get_num());
    }
    return numeric::smith_normal_form_actions(z, g).rank;
}
inline void shape(const M &a, std::size_t r, std::size_t c, Guard &g) {
    if (a.rows() != r || a.cols() != c)
        throw std::invalid_argument("jet declared matrix shape");
    g.matrix(a);
}
inline M covariant(const M &dot, const M &value, const M &source, const M &target, Guard &g) {
    return sum(sum(dot, product(value, source, g), g), product(target, value, g), g, true);
}
inline std::vector<M> assemble(const core::RelationalComplex &source, const std::vector<V> &coefficients,
                               Guard &g) {
    if (coefficients.size() != source.grade_count())
        throw std::invalid_argument("jet occurrence grade coverage");
    std::vector<M> result;
    result.emplace_back(0, source.grade({0}).cell_count());
    if (!coefficients.front().empty())
        throw std::invalid_argument("grade zero has no boundary occurrences");
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        const auto &a = source.grade({static_cast<std::uint16_t>(k)});
        g.vector(coefficients[k]);
        if (coefficients[k].size() != a.incidence_count())
            throw std::invalid_argument("jet complete occurrence coverage");
        M b(source.grade({static_cast<std::uint16_t>(k - 1)}).cell_count(), a.cell_count());
        for (std::size_t p = 0; p < a.incidence_count(); ++p) {
            g.step();
            if (a.aspects.get(source.signature().existence_aspect(), p)) {
                const auto i = a.lower_index[p], j = a.upper_index[p];
                b.set(i, j, g.add(b.get(i, j), coefficients[k][p]));
            }
        }
        result.push_back(std::move(b));
    }
    return result;
}
inline std::size_t source_allowance(const core::RelationalComplex &s, Guard &g) {
    g.dimension(s.grade_count());
    std::size_t count{};
    for (std::size_t k = 0; k < s.grade_count(); ++k) {
        const auto &a = s.grade({static_cast<std::uint16_t>(k)});
        g.dimension(a.cell_count());
        count = add(count, add(mul(16, a.incidence_count()), mul(8, a.cell_count())));
        for (const auto &q : a.cell_scales) {
            g.step();
            g.value(q);
        }
        for (const auto &z : a.lattice_coefficient) {
            g.step();
            g.value(Q{z});
        }
    }
    g.entries(count);
    return count;
}
} // namespace covariant_detail
class OwnedJetPoint {
    struct State {
        geometry::UnivariateDomain domain;
        numeric::Rational value;
    };

  public:
    static OwnedJetPoint declare(geometry::UnivariateDomain domain,
                                 const geometry::UnivariateDomain::Value &value,
                                 numeric::ExactActionGuard &g) {
        if (!domain.contains(value, g) || value.value() <= domain.lower() || value.value() >= domain.upper())
            throw std::domain_error("two-sided jet point must be in the original interval interior");
        return OwnedJetPoint(std::make_shared<const State>(State{std::move(domain), value.value()}));
    }
    const geometry::UnivariateDomain &domain() const & noexcept {
        return state_->domain;
    }
    const geometry::UnivariateDomain &domain() const && = delete;
    const numeric::Rational &value() const & noexcept {
        return state_->value;
    }
    const numeric::Rational &value() const && = delete;
    bool same_owner(const OwnedJetPoint &p) const noexcept {
        return state_ == p.state_;
    }

  private:
    explicit OwnedJetPoint(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
class JetDirection {
    struct State {
        std::string name;
        std::optional<geometry::UnivariateCoordinate> coordinate;
    };

  public:
    static JetDirection declare(std::string name) {
        if (name.empty())
            throw std::invalid_argument("empty jet direction");
        return JetDirection(std::make_shared<const State>(State{std::move(name), {}}));
    }
    static JetDirection coordinate(geometry::UnivariateCoordinate coordinate) {
        const auto name = coordinate.name();
        return JetDirection(std::make_shared<const State>(State{name, std::move(coordinate)}));
    }
    bool same_owner(const JetDirection &d) const noexcept {
        return state_ == d.state_ || (state_->coordinate && d.state_->coordinate &&
                                      state_->coordinate->same_owner(*d.state_->coordinate));
    }
    const std::string &name() const & noexcept {
        return state_->name;
    }
    const std::string &name() const && = delete;

  private:
    explicit JetDirection(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
class PreparedCovariantFamily;
// An explicit realization at a point, retaining every original source occurrence.
// This is not a claim of flatness or constant rank on a neighbourhood.
class OwnedJetBase {
    using V = covariant_detail::V;
    using M = covariant_detail::M;
    using Guard = covariant_detail::Guard;
    struct State {
        core::OwnedRelationalSource owner;
        OwnedJetPoint point;
        std::vector<V> coefficients, fields;
        std::vector<M> boundaries, metrics, frames;
        std::vector<field::ExactMetric> positive_metrics;
        std::size_t retained;
        std::shared_ptr<const void> family;
        std::function<void(Guard &)> validate_family;
    };
    friend class PreparedCovariantFamily;

  public:
    static OwnedJetBase declare(core::OwnedRelationalSource owner, OwnedJetPoint point,
                                std::vector<V> coefficients, std::vector<M> metrics, std::vector<V> fields,
                                Guard &guard) {
        using namespace covariant_detail;
        guard.step();
        point.domain().validate(guard);
        guard.value(point.value());
        const auto retained = source_allowance(owner.source(), guard);
        const auto count = owner.source().grade_count();
        if (metrics.size() != count || fields.size() != count)
            throw std::invalid_argument("jet whole-grade metric/field coverage");
        std::size_t entries = retained;
        for (std::size_t k = 0; k < count; ++k) {
            const auto n = owner.source().grade({static_cast<std::uint16_t>(k)}).cell_count();
            entries = add(entries, add(mul(32, mul(n, n)), mul(16, n)));
        }
        guard.entries(entries);
        auto g = phase(guard, entries);
        auto boundaries = assemble(owner.source(), coefficients, g);
        std::vector<field::ExactMetric> positive;
        for (std::size_t k = 0; k < count; ++k) {
            const auto n = owner.source().grade({static_cast<std::uint16_t>(k)}).cell_count();
            shape(metrics[k], n, n, g);
            g.vector(fields[k]);
            if (fields[k].size() != n)
                throw std::invalid_argument("jet field source grade");
            positive.emplace_back(metrics[k], g);
        }
        for (std::size_t k = 2; k < count; ++k)
            if (product(boundaries[k - 1], boundaries[k], g).nonzeros())
                throw std::invalid_argument("realized base chain family does not close");
        std::vector<M> frames;
        for (const auto &m : metrics)
            frames.push_back(M::identity(m.rows()));
        guard.entries(entries);
        guard.step();
        return OwnedJetBase(std::make_shared<const State>(State{std::move(owner),
                                                                std::move(point),
                                                                std::move(coefficients),
                                                                std::move(fields),
                                                                std::move(boundaries),
                                                                std::move(metrics),
                                                                std::move(frames),
                                                                std::move(positive),
                                                                entries,
                                                                {},
                                                                {}}));
    }
    bool same_owner(const OwnedJetBase &b) const noexcept {
        return state_ == b.state_;
    }
    bool same_carrier(const OwnedJetBase &b, std::size_t k, std::size_t l) const noexcept {
        return k == l && (same_owner(b) || (state_->family && state_->family == b.state_->family &&
                                            point().same_owner(b.point())));
    }
    bool belongs_to(const std::shared_ptr<const void> &family) const noexcept {
        return state_->family && state_->family == family;
    }
    const core::OwnedRelationalSource &owner() const & noexcept {
        return state_->owner;
    }
    const core::OwnedRelationalSource &owner() const && = delete;
    const OwnedJetPoint &point() const & noexcept {
        return state_->point;
    }
    const OwnedJetPoint &point() const && = delete;
    const std::vector<V> &occurrences() const & noexcept {
        return state_->coefficients;
    }
    const std::vector<V> &occurrences() const && = delete;
    const std::vector<M> &boundaries() const & noexcept {
        return state_->boundaries;
    }
    const std::vector<M> &boundaries() const && = delete;
    const std::vector<M> &metrics() const & noexcept {
        return state_->metrics;
    }
    const std::vector<M> &metrics() const && = delete;
    const std::vector<V> &fields() const & noexcept {
        return state_->fields;
    }
    const std::vector<V> &fields() const && = delete;
    void validate_dependencies(Guard &guard) const {
        if (state_->validate_family)
            state_->validate_family(guard);
    }
    const std::vector<M> &frames() const & noexcept {
        return state_->frames;
    }
    const std::vector<M> &frames() const && = delete;
    std::vector<M> realize_boundaries(const std::vector<V> &coefficients, Guard &guard) const {
        using namespace covariant_detail;
        auto raw = assemble(owner().source(), coefficients, guard);
        for (std::size_t k = 1; k < raw.size(); ++k)
            raw[k] = solve(frames()[k - 1], product(raw[k], frames()[k], guard), guard);
        return raw;
    }
    // Explicit constant graded basis changes; no time-dependent frame derivative
    // or carrier identification is inferred from equal dimensions.
    OwnedJetBase reframe(const std::vector<M> &changes, Guard &guard) const {
        using namespace covariant_detail;
        if (changes.size() != metrics().size())
            throw std::invalid_argument("constant jet frame whole-grade coverage");
        auto g = phase(guard, retained_allowance());
        auto next = std::make_shared<State>(*state_);
        next->family.reset();
        next->positive_metrics.clear();
        for (std::size_t k = 0; k < changes.size(); ++k) {
            const auto n = metrics()[k].rows();
            shape(changes[k], n, n, g);
            next->frames[k] = product(frames()[k], changes[k], g);
            next->metrics[k] = product(product(changes[k].transpose(), metrics()[k], g), changes[k], g);
            next->positive_metrics.emplace_back(next->metrics[k], g);
            M field(n, 1);
            for (std::size_t i = 0; i < n; ++i)
                field.set(i, 0, fields()[k][i]);
            const auto values = solve(changes[k], field, g);
            for (std::size_t i = 0; i < n; ++i)
                next->fields[k][i] = values.get(i, 0);
        }
        auto out = OwnedJetBase(next);
        next->boundaries = out.realize_boundaries(occurrences(), g);
        guard.step();
        return out;
    }
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }

  private:
    explicit OwnedJetBase(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
enum class JetOrder : unsigned char { First = 1 };
enum class JetMetricContract : unsigned char { Compatible, General };
struct CovariantJetInput {
    std::vector<numeric::RationalVector> occurrence_derivatives, field_derivatives;
    std::vector<numeric::ExactMatrix> metric_derivatives, connections;
};
class PreparedCovariantJet {
    using Q = covariant_detail::Q;
    using V = covariant_detail::V;
    using M = covariant_detail::M;
    using Guard = covariant_detail::Guard;

  public:
    struct Moment {
        Q value, derivative, left_field, right_field, pairing_channel, covariant_left, covariant_right,
            metric_residual_channel;
        std::vector<Q> operator_positions, covariant_positions;
    };

  private:
    struct State {
        OwnedJetBase base;
        JetDirection direction;
        CovariantJetInput input;
        std::vector<M> boundary_dots, boundary_covariants, metric_residuals, adjoints, adjoint_dots, hodge,
            hodge_dots, hodge_covariants;
        std::vector<V> field_covariants;
        bool compatible;
        std::size_t retained;
        std::shared_ptr<const void> family_jet;
    };
    friend class PreparedCovariantFamily;

  public:
    static PreparedCovariantJet prepare(OwnedJetBase base, JetDirection direction, CovariantJetInput input,
                                        Guard &guard,
                                        JetMetricContract contract = JetMetricContract::Compatible) {
        using namespace covariant_detail;
        guard.step();
        const auto count = base.metrics().size();
        if (input.metric_derivatives.size() != count || input.connections.size() != count ||
            input.field_derivatives.size() != count)
            throw std::invalid_argument("first jet all-grade derivative coverage");
        const auto retained = add(base.retained_allowance(), mul(8, base.retained_allowance()));
        guard.entries(retained);
        base.validate_dependencies(guard);
        auto g = phase(guard, retained);
        base.point().domain().validate(g);
        g.value(base.point().value());
        auto bd = base.realize_boundaries(input.occurrence_derivatives, g);
        std::vector<M> bc, mr, adj, ad, lap, ld, lc;
        std::vector<V> xc;
        bool compatible = true;
        for (std::size_t k = 0; k < count; ++k) {
            const auto n = base.metrics()[k].rows();
            shape(input.metric_derivatives[k], n, n, g);
            shape(input.connections[k], n, n, g);
            g.vector(input.field_derivatives[k]);
            if (input.field_derivatives[k].size() != n)
                throw std::invalid_argument("jet field derivative carrier");
            if (input.metric_derivatives[k] != input.metric_derivatives[k].transpose())
                throw std::invalid_argument("metric tangent must be symmetric");
            auto residual = sum(sum(input.metric_derivatives[k],
                                    product(input.connections[k].transpose(), base.metrics()[k], g), g),
                                product(base.metrics()[k], input.connections[k], g), g);
            compatible = compatible && !residual.nonzeros();
            mr.push_back(std::move(residual));
            xc.push_back(
                sum(input.field_derivatives[k], apply(input.connections[k], base.fields()[k], g), g, true));
            if (!k) {
                bc.emplace_back(0, n);
                adj.emplace_back(n, 0);
                ad.emplace_back(n, 0);
                continue;
            }
            bc.push_back(
                covariant(bd[k], base.boundaries()[k], input.connections[k], input.connections[k - 1], g));
            auto a = solve(base.metrics()[k],
                           product(base.boundaries()[k].transpose(), base.metrics()[k - 1], g), g);
            auto rhs = sum(product(bd[k].transpose(), base.metrics()[k - 1], g),
                           product(base.boundaries()[k].transpose(), input.metric_derivatives[k - 1], g), g);
            rhs = sum(std::move(rhs), product(input.metric_derivatives[k], a, g), g, true);
            ad.push_back(solve(base.metrics()[k], rhs, g));
            adj.push_back(std::move(a));
        }
        if (contract == JetMetricContract::Compatible && !compatible)
            throw std::invalid_argument("metric-compatible first jet has nonzero residual");
        for (std::size_t k = 2; k < count; ++k)
            if (sum(product(bd[k - 1], base.boundaries()[k], g), product(base.boundaries()[k - 1], bd[k], g),
                    g)
                    .nonzeros())
                throw std::invalid_argument("first-order whole chain family does not close");
        for (std::size_t k = 0; k < count; ++k) {
            const auto n = base.metrics()[k].rows();
            M l(n, n), dot(n, n);
            if (k) {
                l = product(adj[k], base.boundaries()[k], g);
                dot = sum(product(ad[k], base.boundaries()[k], g), product(adj[k], bd[k], g), g);
            }
            if (k + 1 < count) {
                l = sum(std::move(l), product(base.boundaries()[k + 1], adj[k + 1], g), g);
                dot = sum(sum(std::move(dot), product(bd[k + 1], adj[k + 1], g), g),
                          product(base.boundaries()[k + 1], ad[k + 1], g), g);
            }
            if (product(base.metrics()[k], l, g) != product(l.transpose(), base.metrics()[k], g))
                throw std::logic_error("jet Hodge self-adjointness residual");
            lc.push_back(covariant(dot, l, input.connections[k], input.connections[k], g));
            lap.push_back(std::move(l));
            ld.push_back(std::move(dot));
        }
        guard.step();
        return PreparedCovariantJet(std::make_shared<const State>(State{std::move(base),
                                                                        std::move(direction),
                                                                        std::move(input),
                                                                        std::move(bd),
                                                                        std::move(bc),
                                                                        std::move(mr),
                                                                        std::move(adj),
                                                                        std::move(ad),
                                                                        std::move(lap),
                                                                        std::move(ld),
                                                                        std::move(lc),
                                                                        std::move(xc),
                                                                        compatible,
                                                                        retained,
                                                                        {}}));
    }
    static constexpr JetOrder order() noexcept {
        return JetOrder::First;
    }
    bool same_owner(const PreparedCovariantJet &j) const noexcept {
        return state_ == j.state_;
    }
    bool belongs_to_family(const std::shared_ptr<const void> &family) const noexcept {
        return state_->family_jet && state_->family_jet == family && base().belongs_to(family);
    }
    const OwnedJetBase &base() const & noexcept {
        return state_->base;
    }
    const OwnedJetBase &base() const && = delete;
    const JetDirection &direction() const & noexcept {
        return state_->direction;
    }
    const JetDirection &direction() const && = delete;
    const CovariantJetInput &input() const & noexcept {
        return state_->input;
    }
    const CovariantJetInput &input() const && = delete;
    const std::vector<M> &boundary_dots() const & noexcept {
        return state_->boundary_dots;
    }
    const std::vector<M> &boundary_dots() const && = delete;
    const std::vector<M> &boundary_covariants() const & noexcept {
        return state_->boundary_covariants;
    }
    const std::vector<M> &boundary_covariants() const && = delete;
    const std::vector<M> &adjoints() const & noexcept {
        return state_->adjoints;
    }
    const std::vector<M> &adjoints() const && = delete;
    const std::vector<M> &adjoint_dots() const & noexcept {
        return state_->adjoint_dots;
    }
    const std::vector<M> &adjoint_dots() const && = delete;
    const std::vector<M> &hodge() const & noexcept {
        return state_->hodge;
    }
    const std::vector<M> &hodge() const && = delete;
    const std::vector<M> &hodge_dots() const & noexcept {
        return state_->hodge_dots;
    }
    const std::vector<M> &hodge_dots() const && = delete;
    const std::vector<M> &hodge_covariants() const & noexcept {
        return state_->hodge_covariants;
    }
    const std::vector<M> &hodge_covariants() const && = delete;
    const std::vector<M> &metric_residuals() const & noexcept {
        return state_->metric_residuals;
    }
    const std::vector<M> &metric_residuals() const && = delete;
    const std::vector<V> &field_covariants() const & noexcept {
        return state_->field_covariants;
    }
    const std::vector<V> &field_covariants() const && = delete;
    bool metric_compatible() const noexcept {
        return state_->compatible;
    }
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }
    void validate_receiving(Guard &guard) const {
        guard.entries(retained_allowance());
        base().validate_dependencies(guard);
        base().point().domain().validate(guard);
        guard.value(base().point().value());
        (void)covariant_detail::source_allowance(base().owner().source(), guard);
        for (const auto *group : {&base().occurrences(), &base().fields(), &input().occurrence_derivatives,
                                  &input().field_derivatives, &field_covariants()})
            for (const auto &v : *group)
                guard.vector(v);
        for (const auto *group :
             {&base().boundaries(), &base().metrics(), &base().frames(), &input().metric_derivatives,
              &input().connections, &boundary_dots(), &boundary_covariants(), &adjoints(), &adjoint_dots(),
              &hodge(), &hodge_dots(), &hodge_covariants(), &metric_residuals()})
            for (const auto &m : *group)
                guard.matrix(m);
    }
    PreparedCovariantJet reframe(const std::vector<M> &changes, Guard &guard) const {
        using namespace covariant_detail;
        validate_receiving(guard);
        auto next = base().reframe(changes, guard);
        auto g = phase(guard, retained_allowance());
        auto transformed = input();
        for (std::size_t k = 0; k < changes.size(); ++k) {
            const auto &s = changes[k];
            transformed.metric_derivatives[k] =
                product(product(s.transpose(), input().metric_derivatives[k], g), s, g);
            transformed.connections[k] = solve(s, product(input().connections[k], s, g), g);
            M v(s.rows(), 1);
            for (std::size_t i = 0; i < s.rows(); ++i)
                v.set(i, 0, input().field_derivatives[k][i]);
            const auto d = solve(s, v, g);
            for (std::size_t i = 0; i < s.rows(); ++i)
                transformed.field_derivatives[k][i] = d.get(i, 0);
        }
        return prepare(std::move(next), direction(), std::move(transformed), guard,
                       metric_compatible() ? JetMetricContract::Compatible : JetMetricContract::General);
    }
    template <class A, class B, class C = std::size_t>
        requires(std::floating_point<A> || std::floating_point<B> || std::floating_point<C>)
    Moment moment(A, B, Guard &, C = 128) const = delete;
    Moment moment(std::size_t grade, std::size_t power, Guard &guard, std::size_t max_power = 128) const {
        using namespace covariant_detail;
        guard.step();
        validate_receiving(guard);
        const auto &metric = base().metrics().at(grade);
        const auto &x = base().fields().at(grade);
        if (power > max_power)
            throw std::length_error("jet moment power budget");
        const auto retained = add(state_->retained, mul(add(power, 1), add(mul(4, x.size()), 8)));
        auto g = phase(guard, retained);
        g.matrix(metric);
        g.matrix(hodge()[grade]);
        g.matrix(hodge_dots()[grade]);
        g.matrix(hodge_covariants()[grade]);
        g.matrix(input().metric_derivatives[grade]);
        g.matrix(metric_residuals()[grade]);
        g.vector(x);
        g.vector(input().field_derivatives[grade]);
        g.vector(field_covariants()[grade]);
        std::vector<V> powers{x};
        for (std::size_t i = 0; i < power; ++i)
            powers.push_back(apply(hodge()[grade], powers.back(), g));
        const auto &dot = input().field_derivatives[grade];
        const auto &cov = field_covariants()[grade];
        Moment m;
        m.value = pairing(x, metric, powers.back(), g);
        m.left_field = pairing(dot, metric, powers.back(), g);
        m.right_field = pairing(powers.back(), metric, dot, g);
        m.pairing_channel = pairing(x, input().metric_derivatives[grade], powers.back(), g);
        m.covariant_left = pairing(cov, metric, powers.back(), g);
        m.covariant_right = pairing(powers.back(), metric, cov, g);
        m.metric_residual_channel = pairing(x, metric_residuals()[grade], powers.back(), g);
        Q ordinary = g.add(g.add(m.left_field, m.right_field), m.pairing_channel),
          transported = g.add(g.add(m.covariant_left, m.covariant_right), m.metric_residual_channel);
        for (std::size_t j = 0; j < power; ++j) {
            const auto right = power - 1 - j;
            auto v = apply(hodge_dots()[grade], powers[right], g),
                 w = apply(hodge_covariants()[grade], powers[right], g);
            m.operator_positions.push_back(pairing(powers[j], metric, v, g));
            m.covariant_positions.push_back(pairing(powers[j], metric, w, g));
            ordinary = g.add(ordinary, m.operator_positions.back());
            transported = g.add(transported, m.covariant_positions.back());
        }
        if (ordinary != transported)
            throw std::logic_error("jet all-position product rule residual");
        m.derivative = ordinary;
        guard.step();
        return m;
    }
    template <std::floating_point T>
    M connection_commutator(const PreparedCovariantJet &, T, const M &, const M &, Guard &) const = delete;
    M connection_commutator(const PreparedCovariantJet &other, std::size_t grade,
                            const M &derivative_this_other, const M &derivative_other_this,
                            Guard &guard) const {
        using namespace covariant_detail;
        if (!base().same_owner(other.base()))
            throw std::invalid_argument("mixed jet base owner differs");
        if (direction().same_owner(other.direction()))
            throw std::invalid_argument("mixed directions must be explicitly distinct");
        auto g = phase(guard, add(retained_allowance(), other.retained_allowance()));
        const auto n = base().metrics().at(grade).rows();
        shape(derivative_this_other, n, n, g);
        shape(derivative_other_this, n, n, g);
        const auto &a = input().connections.at(grade);
        const auto &b = other.input().connections.at(grade);
        return sum(sum(derivative_other_this, derivative_this_other, g, true),
                   sum(product(a, b, g), product(b, a, g), g, true), g);
    }

  private:
    explicit PreparedCovariantJet(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
class RectangularCovariantJet {
    using M = covariant_detail::M;
    using V = covariant_detail::V;
    using Guard = covariant_detail::Guard;
    struct State {
        PreparedCovariantJet source, target;
        std::size_t source_grade, target_grade;
        M value, dot, covariant;
    };

  public:
    static RectangularCovariantJet declare(PreparedCovariantJet source, std::size_t source_grade,
                                           PreparedCovariantJet target, std::size_t target_grade, M value,
                                           M dot, Guard &guard) {
        using namespace covariant_detail;
        if (!source.base().point().same_owner(target.base().point()) ||
            !source.direction().same_owner(target.direction()))
            throw std::invalid_argument("rectangular jet point/direction owner differs");
        auto g = phase(guard, add(source.retained_allowance(), target.retained_allowance()));
        const auto n = source.base().metrics().at(source_grade).rows(),
                   m = target.base().metrics().at(target_grade).rows();
        shape(value, m, n, g);
        shape(dot, m, n, g);
        source.validate_receiving(guard);
        target.validate_receiving(guard);
        auto cov = covariant_detail::covariant(dot, value, source.input().connections[source_grade],
                                               target.input().connections[target_grade], g);
        guard.step();
        return RectangularCovariantJet(std::make_shared<const State>(
            State{std::move(source), std::move(target), source_grade, target_grade, std::move(value),
                  std::move(dot), std::move(cov)}));
    }
    template <class A, class B>
        requires(std::floating_point<A> || std::floating_point<B>)
    static RectangularCovariantJet declare(PreparedCovariantJet, A, PreparedCovariantJet, B, M, M,
                                           Guard &) = delete;
    const PreparedCovariantJet &source() const & noexcept {
        return state_->source;
    }
    const PreparedCovariantJet &source() const && = delete;
    const PreparedCovariantJet &target() const & noexcept {
        return state_->target;
    }
    const PreparedCovariantJet &target() const && = delete;
    std::size_t source_grade() const noexcept {
        return state_->source_grade;
    }
    std::size_t target_grade() const noexcept {
        return state_->target_grade;
    }
    const M &value() const & noexcept {
        return state_->value;
    }
    const M &value() const && = delete;
    const M &dot() const & noexcept {
        return state_->dot;
    }
    const M &dot() const && = delete;
    const M &covariant() const & noexcept {
        return state_->covariant;
    }
    const M &covariant() const && = delete;
    RectangularCovariantJet adjoint(Guard &guard) const {
        using namespace covariant_detail;
        auto g = phase(guard, add(source().retained_allowance(), target().retained_allowance()));
        source().validate_receiving(guard);
        target().validate_receiving(guard);
        const auto s = source_grade(), t = target_grade();
        const auto &ms = source().base().metrics()[s];
        const auto &mt = target().base().metrics()[t];
        auto a = solve(ms, product(value().transpose(), mt, g), g);
        auto rhs = sum(product(dot().transpose(), mt, g),
                       product(value().transpose(), target().input().metric_derivatives[t], g), g);
        rhs = sum(std::move(rhs), product(source().input().metric_derivatives[s], a, g), g, true);
        auto d = solve(ms, rhs, g);
        return declare(target(), t, source(), s, std::move(a), std::move(d), guard);
    }
    struct FieldImage {
        V value, derivative, covariant;
    };
    FieldImage apply_to_source_field(Guard &guard) const {
        using namespace covariant_detail;
        auto g = phase(guard, add(source().retained_allowance(), target().retained_allowance()));
        source().validate_receiving(guard);
        target().validate_receiving(guard);
        const auto s = source_grade(), t = target_grade();
        const auto &x = source().base().fields()[s];
        auto y = apply(value(), x, g),
             d = sum(apply(dot(), x, g), apply(value(), source().input().field_derivatives[s], g), g);
        auto c = sum(d, apply(target().input().connections[t], y, g), g, true);
        if (c != sum(apply(covariant(), x, g), apply(value(), source().field_covariants()[s], g), g))
            throw std::logic_error("rectangular field product-rule residual");
        guard.step();
        return {std::move(y), std::move(d), std::move(c)};
    }
    FieldImage projector_image(Guard &guard) const {
        using namespace covariant_detail;
        if (!source().base().same_carrier(target().base(), source_grade(), target_grade()))
            throw std::invalid_argument("projector actual carrier differs");
        auto g = phase(guard, add(source().retained_allowance(), target().retained_allowance()));
        if (product(value(), value(), g) != value() ||
            sum(product(dot(), value(), g), product(value(), dot(), g), g) != dot())
            throw std::invalid_argument("projector value/tangent identity fails");
        return apply_to_source_field(guard);
    }

  private:
    explicit RectangularCovariantJet(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
class PreparedLayerJet {
    using V = covariant_detail::V;
    using M = covariant_detail::M;
    using Guard = covariant_detail::Guard;
    struct State {
        RectangularCovariantJet first, second;
        V offset, offset_dot;
        M kernel, kernel_dot;
        std::size_t constraint_rank;
    };

  public:
    static PreparedLayerJet prepare(RectangularCovariantJet first, RectangularCovariantJet second,
                                    V joint_offset, V joint_offset_dot, M joint_kernel, M joint_kernel_dot,
                                    Guard &guard) {
        using namespace covariant_detail;
        if (!first.target().base().same_carrier(second.target().base(), first.target_grade(),
                                                second.target_grade()) ||
            !first.source().base().point().same_owner(second.source().base().point()) ||
            !first.source().direction().same_owner(second.source().direction()))
            throw std::invalid_argument("layer jet actual shared carrier/direction differs");
        auto g =
            phase(guard, add(add(first.source().retained_allowance(), second.source().retained_allowance()),
                             first.target().retained_allowance()));
        const auto a = first.value().cols(), b = second.value().cols(), n = add(a, b),
                   m = first.value().rows();
        g.dimension(n);
        g.entries(mul(16, mul(add(n, 1), add(m, add(joint_kernel.cols(), 1)))));
        M c(m, n), cd(m, n);
        for (std::size_t i = 0; i < m; ++i) {
            for (std::size_t j = 0; j < a; ++j) {
                g.step();
                c.set(i, j, first.value().get(i, j));
                cd.set(i, j, first.dot().get(i, j));
            }
            for (std::size_t j = 0; j < b; ++j) {
                g.step();
                c.set(i, a + j, -second.value().get(i, j));
                cd.set(i, a + j, -second.dot().get(i, j));
            }
        }
        g.vector(joint_offset);
        g.vector(joint_offset_dot);
        if (joint_offset.size() != n || joint_offset_dot.size() != n)
            throw std::invalid_argument("layer offset full joint carrier");
        shape(joint_kernel, n, joint_kernel.cols(), g);
        shape(joint_kernel_dot, n, joint_kernel.cols(), g);
        if (apply(c, joint_offset, g) != V(m) ||
            sum(apply(cd, joint_offset, g), apply(c, joint_offset_dot, g), g) != V(m) ||
            product(c, joint_kernel, g).nonzeros() ||
            sum(product(cd, joint_kernel, g), product(c, joint_kernel_dot, g), g).nonzeros())
            throw std::invalid_argument("layer compatibility or differentiated complete-family residual");
        const auto r = rank(c, g);
        if (rank(joint_kernel, g) != joint_kernel.cols() || add(r, joint_kernel.cols()) != n)
            throw std::invalid_argument("layer generators do not span the complete compatible family");
        guard.step();
        return PreparedLayerJet(std::make_shared<const State>(
            State{std::move(first), std::move(second), std::move(joint_offset), std::move(joint_offset_dot),
                  std::move(joint_kernel), std::move(joint_kernel_dot), r}));
    }
    const M &kernel() const & noexcept {
        return state_->kernel;
    }
    const M &kernel() const && = delete;
    const M &kernel_dot() const & noexcept {
        return state_->kernel_dot;
    }
    const M &kernel_dot() const && = delete;
    const V &offset() const & noexcept {
        return state_->offset;
    }
    const V &offset() const && = delete;
    const V &offset_dot() const & noexcept {
        return state_->offset_dot;
    }
    const V &offset_dot() const && = delete;
    std::size_t constraint_rank() const noexcept {
        return state_->constraint_rank;
    }

  private:
    explicit PreparedLayerJet(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::temporal
