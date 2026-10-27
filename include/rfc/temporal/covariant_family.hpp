#pragma once
#include "rfc/temporal/covariant_jet.hpp"

namespace rfc::temporal {
namespace covariant_family_detail {
using namespace covariant_detail;
using F = numeric::RationalFunction;
using E = geometry::OwnedRationalExpression;
using P = numeric::RationalVector;
struct Work {
    geometry::univariate_detail::Work polynomial;
    Work(Guard &g, geometry::UnivariateLimits l) : polynomial(g, l) {}
    void admit(const F &f) {
        polynomial.admit(f.numerator());
        polynomial.admit(f.denominator());
    }
    F make(P n, P d) {
        if (d.empty())
            throw std::domain_error("zero formal jet divisor");
        const auto common = polynomial.gcd(n, d);
        n = polynomial.divide(n, common).first;
        d = polynomial.divide(d, common).first;
        F f(std::move(n), std::move(d));
        admit(f);
        return f;
    }
    F plus(const F &a, const F &b, bool negative = false) {
        admit(a);
        admit(b);
        auto left = polynomial.mul(a.numerator(), b.denominator()),
             right = polynomial.mul(b.numerator(), a.denominator());
        if (!negative)
            for (auto &q : right)
                q = -q;
        return make(polynomial.sub(std::move(left), right), polynomial.mul(a.denominator(), b.denominator()));
    }
    F times(const F &a, const F &b) {
        admit(a);
        admit(b);
        return make(polynomial.mul(a.numerator(), b.numerator()),
                    polynomial.mul(a.denominator(), b.denominator()));
    }
    F divide(const F &a, const F &b) {
        admit(a);
        admit(b);
        return make(polynomial.mul(a.numerator(), b.denominator()),
                    polynomial.mul(a.denominator(), b.numerator()));
    }
    F derivative(const F &a) {
        admit(a);
        return make(polynomial.sub(polynomial.mul(polynomial.derivative(a.numerator()), a.denominator()),
                                   polynomial.mul(a.numerator(), polynomial.derivative(a.denominator()))),
                    polynomial.mul(a.denominator(), a.denominator()));
    }
    Q evaluate(const F &a, const Q &q) {
        admit(a);
        polynomial.guard.value(q);
        const auto d = polynomial.evaluate(a.denominator(), q);
        if (d == 0)
            throw std::domain_error("formal jet specialization pole");
        return polynomial.guard.div(polynomial.evaluate(a.numerator(), q), d);
    }
};
// Explicit full coordinate arrays on the bounded profile; never a source complex.
struct Matrix {
    std::size_t rows, cols;
    std::vector<F> values;
    Matrix(std::size_t r, std::size_t c, Work &w) : rows(r), cols(c) {
        w.polynomial.guard.dimension(r);
        w.polynomial.guard.dimension(c);
        w.polynomial.scratch(mul(4, mul(r, c)));
        values.resize(mul(r, c));
    }
    F &at(std::size_t i, std::size_t j) {
        return values.at(i * cols + j);
    }
    const F &at(std::size_t i, std::size_t j) const {
        return values.at(i * cols + j);
    }
    bool zero() const {
        return std::all_of(values.begin(), values.end(), [](const auto &f) { return f.is_zero(); });
    }
};
inline Matrix matrix(std::size_t r, std::size_t c, const std::vector<E> &entries,
                     const geometry::UnivariateDomain &d, Work &w) {
    if (entries.size() != mul(r, c))
        throw std::invalid_argument("rational jet complete matrix entries");
    Matrix out(r, c, w);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!d.same_owner(entries[i].domain()))
            throw std::invalid_argument("rational jet expression domain owner");
        w.admit(entries[i].formal());
        out.values[i] = entries[i].formal();
    }
    return out;
}
inline Matrix transpose(const Matrix &a, Work &w) {
    Matrix b(a.cols, a.rows, w);
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t j = 0; j < a.cols; ++j) {
            w.polynomial.guard.step();
            b.at(j, i) = a.at(i, j);
        }
    return b;
}
inline Matrix sum(const Matrix &a, const Matrix &b, Work &w, bool negative = false) {
    if (a.rows != b.rows || a.cols != b.cols)
        throw std::invalid_argument("rational jet sum carriers");
    Matrix c(a.rows, a.cols, w);
    for (std::size_t i = 0; i < c.values.size(); ++i)
        c.values[i] = w.plus(a.values[i], b.values[i], negative);
    return c;
}
inline Matrix product(const Matrix &a, const Matrix &b, Work &w) {
    if (a.cols != b.rows)
        throw std::invalid_argument("rational jet product carriers");
    Matrix c(a.rows, b.cols, w);
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t j = 0; j < b.cols; ++j)
            for (std::size_t k = 0; k < a.cols; ++k)
                c.at(i, j) = w.plus(c.at(i, j), w.times(a.at(i, k), b.at(k, j)));
    return c;
}
inline Matrix derivative(const Matrix &a, Work &w) {
    Matrix b(a.rows, a.cols, w);
    for (std::size_t i = 0; i < a.values.size(); ++i)
        b.values[i] = w.derivative(a.values[i]);
    return b;
}
inline M evaluate(const Matrix &a, const Q &q, Work &w) {
    M out(a.rows, a.cols);
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t j = 0; j < a.cols; ++j)
            out.set(i, j, w.evaluate(a.at(i, j), q));
    return out;
}
inline V vector(const Matrix &a, const Q &q, Work &w) {
    if (a.cols != 1)
        throw std::invalid_argument("rational jet vector columns");
    V out(a.rows);
    for (std::size_t i = 0; i < a.rows; ++i)
        out[i] = w.evaluate(a.at(i, 0), q);
    return out;
}
inline F pairing(const Matrix &x, const Matrix &m, const Matrix &y, Work &w) {
    return product(product(transpose(x, w), m, w), y, w).at(0, 0);
}
inline geometry::PreparedUnivariateFamily census(const Matrix &a, const geometry::UnivariateDomain &d,
                                                 geometry::UnivariateCarrier rows,
                                                 geometry::UnivariateCarrier cols, Guard &g,
                                                 geometry::UnivariateLimits limits) {
    std::vector<E> entries;
    for (const auto &f : a.values)
        entries.push_back(E::declare(d, f.numerator(), f.denominator(), g, limits));
    return geometry::PreparedUnivariateFamily::prepare(d, std::move(rows), std::move(cols),
                                                       std::move(entries), g, limits);
}
inline geometry::PreparedUnivariateFamily scope(const geometry::UnivariateDomain &parent,
                                                const std::vector<P> &guards, Guard &g,
                                                geometry::UnivariateLimits limits) {
    auto d =
        geometry::UnivariateDomain::declare(parent.coordinate(), parent.lower(), parent.upper(),
                                            parent.lower_closed(), parent.upper_closed(), guards, g, limits);
    auto one = E::declare(d, {Q{1}}, {Q{1}}, g, limits);
    return geometry::PreparedUnivariateFamily::prepare(
        d, geometry::UnivariateCarrier::declare("regular-scope-target", 1),
        geometry::UnivariateCarrier::declare("regular-scope-source", 1), {one}, g, limits);
}
inline std::size_t regular_cell(const geometry::PreparedUnivariateFamily &scope, const OwnedJetPoint &point,
                                Guard &g, geometry::UnivariateLimits limits) {
    geometry::univariate_detail::Work w(g, limits);
    const auto &q = point.value();
    if (!scope.domain().contains(scope.domain().at(q, g), g, limits) || q <= scope.domain().lower() ||
        q >= scope.domain().upper() || w.evaluate(scope.root_polynomial(), q) == 0)
        throw std::domain_error("jet point is outside a certified regular cell");
    std::size_t cell{};
    for (std::size_t i = 1; i + 1 < scope.points().size(); ++i) {
        const auto &x = scope.points()[i].interval;
        if (q >= x.upper) {
            ++cell;
            continue;
        }
        if (q <= x.lower)
            break;
        const auto st = w.sturm(scope.root_polynomial());
        if (w.count(st, x.lower, q))
            ++cell;
        break;
    }
    if (cell >= scope.cells().size())
        throw std::logic_error("regular jet cell census residual");
    return cell;
}
} // namespace covariant_family_detail
struct CovariantFamilyInput {
    std::vector<std::vector<geometry::OwnedRationalExpression>> occurrence_coefficients, metrics, connections,
        fields;
};
class PreparedCovariantFamily {
    using Q = covariant_detail::Q;
    using M = covariant_detail::M;
    using V = covariant_detail::V;
    using Guard = covariant_detail::Guard;
    using E = covariant_family_detail::E;
    using F = covariant_family_detail::F;
    using P = covariant_family_detail::P;
    using FM = covariant_family_detail::Matrix;
    struct State {
        core::OwnedRelationalSource owner;
        geometry::UnivariateDomain domain;
        CovariantFamilyInput input;
        std::vector<geometry::UnivariateCarrier> carriers;
        std::vector<FM> boundaries, metrics, connections, fields;
        std::vector<geometry::PreparedUnivariateFamily> boundary_ranks, metric_ranks;
        geometry::PreparedUnivariateFamily regular_scope;
        geometry::UnivariateLimits limits;
        bool metric_compatible;
        std::size_t retained;
    };

  public:
    static PreparedCovariantFamily prepare(core::OwnedRelationalSource owner,
                                           geometry::UnivariateDomain domain, CovariantFamilyInput input,
                                           Guard &guard,
                                           JetMetricContract contract = JetMetricContract::Compatible,
                                           geometry::UnivariateLimits limits = {}) {
        using namespace covariant_family_detail;
        guard.step();
        domain.validate(guard, limits);
        const auto count = owner.source().grade_count();
        if (input.occurrence_coefficients.size() != count || input.metrics.size() != count ||
            input.connections.size() != count || input.fields.size() != count)
            throw std::invalid_argument("rational jet whole-grade family coverage");
        auto retained = covariant_detail::source_allowance(owner.source(), guard);
        for (std::size_t k = 0; k < count; ++k) {
            const auto n = owner.source().grade({static_cast<std::uint16_t>(k)}).cell_count();
            if (n > limits.max_dimension)
                throw std::length_error("rational jet grade dimension profile");
            const auto lower = k ? owner.source().grade({static_cast<std::uint16_t>(k - 1)}).cell_count() : 0;
            const auto matrices = covariant_detail::add(
                covariant_detail::add(covariant_detail::mul(n, n), covariant_detail::mul(lower, n)),
                covariant_detail::add(n, 1));
            retained = covariant_detail::add(
                retained,
                covariant_detail::mul(
                    64, covariant_detail::mul(covariant_detail::add(limits.max_degree, 1), matrices)));
        }
        for (const auto *group :
             {&input.occurrence_coefficients, &input.metrics, &input.connections, &input.fields})
            for (const auto &row : *group)
                for (const auto &e : row) {
                    if (!domain.same_owner(e.domain()))
                        throw std::invalid_argument("family declaration coordinate/domain owner differs");
                    retained = covariant_detail::add(
                        retained,
                        covariant_detail::mul(8, covariant_detail::add(e.original_numerator().size(),
                                                                       e.original_denominator().size())));
                }
        auto action = covariant_detail::phase(guard, retained);
        Work w(action, limits);
        std::vector<FM> boundaries, metrics, connections, fields;
        std::vector<geometry::UnivariateCarrier> carriers;
        if (!input.occurrence_coefficients[0].empty())
            throw std::invalid_argument("grade-zero family has no boundary occurrences");
        std::vector<P> guards = domain.original_guards();
        for (const auto *group :
             {&input.occurrence_coefficients, &input.metrics, &input.connections, &input.fields})
            for (const auto &row : *group)
                for (const auto &e : row) {
                    w.polynomial.admit(e.original_numerator());
                    w.polynomial.admit(e.original_denominator());
                    guards.push_back(e.original_denominator());
                }
        for (std::size_t k = 0; k < count; ++k) {
            const auto &a = owner.source().grade({static_cast<std::uint16_t>(k)});
            const auto n = a.cell_count();
            carriers.push_back(geometry::UnivariateCarrier::declare("source-grade-" + std::to_string(k), n));
            metrics.push_back(matrix(n, n, input.metrics[k], domain, w));
            connections.push_back(matrix(n, n, input.connections[k], domain, w));
            fields.push_back(matrix(n, 1, input.fields[k], domain, w));
            if (!sum(metrics.back(), transpose(metrics.back(), w), w, true).zero())
                throw std::invalid_argument("rational metric family must be symmetric");
            if (!k) {
                boundaries.emplace_back(0, n, w);
                continue;
            }
            if (input.occurrence_coefficients[k].size() != a.incidence_count())
                throw std::invalid_argument("rational boundary whole occurrence coverage");
            FM b(owner.source().grade({static_cast<std::uint16_t>(k - 1)}).cell_count(), n, w);
            for (std::size_t p = 0; p < a.incidence_count(); ++p) {
                action.step();
                if (a.aspects.get(owner.source().signature().existence_aspect(), p)) {
                    auto &value = b.at(a.lower_index[p], a.upper_index[p]);
                    value = w.plus(value, input.occurrence_coefficients[k][p].formal());
                }
            }
            boundaries.push_back(std::move(b));
        }
        for (std::size_t k = 2; k < count; ++k)
            if (!product(boundaries[k - 1], boundaries[k], w).zero())
                throw std::invalid_argument(
                    "exact rational chain family does not close; a tangent jet is insufficient");
        bool compatible = true;
        for (std::size_t k = 0; k < count; ++k) {
            const auto residual =
                sum(sum(derivative(metrics[k], w), product(transpose(connections[k], w), metrics[k], w), w),
                    product(metrics[k], connections[k], w), w);
            compatible = compatible && residual.zero();
        }
        if (contract == JetMetricContract::Compatible && !compatible)
            throw std::invalid_argument("exact rational metric compatibility family residual");
        std::vector<geometry::PreparedUnivariateFamily> br, mr;
        std::size_t rank_retained{};
        for (std::size_t k = 0; k < count; ++k) {
            {
                auto receive = covariant_detail::phase(action, rank_retained);
                mr.push_back(census(metrics[k], domain, carriers[k], carriers[k], receive, limits));
            }
            rank_retained =
                covariant_detail::add(rank_retained, mr.back().work().retained_coefficient_allowance);
            guards.push_back(mr.back().rank_polynomial());
            if (k) {
                auto receive = covariant_detail::phase(action, rank_retained);
                br.push_back(census(boundaries[k], domain, carriers[k - 1], carriers[k], receive, limits));
                rank_retained =
                    covariant_detail::add(rank_retained, br.back().work().retained_coefficient_allowance);
                guards.push_back(br.back().rank_polynomial());
            }
        }
        auto scope_guard = covariant_detail::phase(action, rank_retained);
        auto regular = scope(domain, guards, scope_guard, limits);
        retained = covariant_detail::add(
            retained, covariant_detail::add(rank_retained, regular.work().retained_coefficient_allowance));
        guard.entries(retained);
        guard.step();
        return PreparedCovariantFamily(std::make_shared<const State>(
            State{std::move(owner), std::move(domain), std::move(input), std::move(carriers),
                  std::move(boundaries), std::move(metrics), std::move(connections), std::move(fields),
                  std::move(br), std::move(mr), std::move(regular), limits, compatible, retained}));
    }
    bool same_owner(const PreparedCovariantFamily &f) const noexcept {
        return state_ == f.state_;
    }
    const core::OwnedRelationalSource &owner() const & noexcept {
        return state_->owner;
    }
    const core::OwnedRelationalSource &owner() const && = delete;
    const geometry::UnivariateDomain &domain() const & noexcept {
        return state_->domain;
    }
    const geometry::UnivariateDomain &domain() const && = delete;
    const CovariantFamilyInput &input() const & noexcept {
        return state_->input;
    }
    const CovariantFamilyInput &input() const && = delete;
    const std::vector<geometry::UnivariateCarrier> &carriers() const & noexcept {
        return state_->carriers;
    }
    const std::vector<geometry::UnivariateCarrier> &carriers() const && = delete;
    const std::vector<FM> &boundaries() const & noexcept {
        return state_->boundaries;
    }
    const std::vector<FM> &boundaries() const && = delete;
    const std::vector<FM> &metrics() const & noexcept {
        return state_->metrics;
    }
    const std::vector<FM> &metrics() const && = delete;
    const std::vector<FM> &fields() const & noexcept {
        return state_->fields;
    }
    const std::vector<FM> &fields() const && = delete;
    const std::vector<FM> &connections() const & noexcept {
        return state_->connections;
    }
    const std::vector<FM> &connections() const && = delete;
    const geometry::PreparedUnivariateFamily &regular_scope() const & noexcept {
        return state_->regular_scope;
    }
    const geometry::PreparedUnivariateFamily &regular_scope() const && = delete;
    geometry::UnivariateLimits limits() const noexcept {
        return state_->limits;
    }
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }
    bool metric_compatible() const noexcept {
        return state_->metric_compatible;
    }
    void validate_receiving(Guard &guard) const {
        guard.entries(retained_allowance());
        domain().validate(guard, limits());
        (void)covariant_detail::source_allowance(owner().source(), guard);
        for (const auto *group :
             {&input().occurrence_coefficients, &input().metrics, &input().connections, &input().fields})
            for (const auto &row : *group)
                for (const auto &e : row) {
                    guard.vector(e.original_numerator());
                    guard.vector(e.original_denominator());
                    guard.vector(e.formal().numerator());
                    guard.vector(e.formal().denominator());
                }
        guard.vector(regular_scope().root_polynomial());
    }
    std::size_t regular_cell(const OwnedJetPoint &point, Guard &guard) const {
        if (!domain().same_owner(point.domain()))
            throw std::invalid_argument("family jet point domain owner differs");
        validate_receiving(guard);
        auto g = covariant_detail::phase(guard, state_->retained);
        return covariant_family_detail::regular_cell(regular_scope(), point, g, limits());
    }
    PreparedCovariantJet jet(const OwnedJetPoint &point, Guard &guard) const {
        using namespace covariant_family_detail;
        (void)regular_cell(point, guard);
        auto g = covariant_detail::phase(guard, state_->retained);
        Work w(g, limits());
        const auto &q = point.value();
        std::vector<V> coefficients;
        CovariantJetInput input;
        std::vector<M> metrics;
        std::vector<V> fields;
        for (std::size_t k = 0; k < state_->metrics.size(); ++k) {
            V values, dots;
            for (const auto &e : state_->input.occurrence_coefficients[k]) {
                values.push_back(w.evaluate(e.formal(), q));
                dots.push_back(w.evaluate(w.derivative(e.formal()), q));
            }
            coefficients.push_back(std::move(values));
            input.occurrence_derivatives.push_back(std::move(dots));
            metrics.push_back(evaluate(state_->metrics[k], q, w));
            fields.push_back(vector(state_->fields[k], q, w));
            input.metric_derivatives.push_back(evaluate(derivative(state_->metrics[k], w), q, w));
            input.connections.push_back(evaluate(state_->connections[k], q, w));
            input.field_derivatives.push_back(vector(derivative(state_->fields[k], w), q, w));
        }
        auto base = OwnedJetBase::declare(owner(), point, std::move(coefficients), std::move(metrics),
                                          std::move(fields), g);
        auto linked = std::make_shared<OwnedJetBase::State>(*base.state_);
        linked->family = state_;
        linked->retained = covariant_detail::add(linked->retained, state_->retained);
        linked->validate_family = [family = state_](Guard &receive) {
            PreparedCovariantFamily(family).validate_receiving(receive);
        };
        base = OwnedJetBase(std::move(linked));
        auto jet = PreparedCovariantJet::prepare(
            std::move(base), JetDirection::coordinate(domain().coordinate()), std::move(input), guard,
            metric_compatible() ? JetMetricContract::Compatible : JetMetricContract::General);
        auto accepted = std::make_shared<PreparedCovariantJet::State>(*jet.state_);
        accepted->family_jet = state_;
        guard.step();
        return PreparedCovariantJet(std::move(accepted));
    }
    bool owns(const PreparedCovariantJet &jet) const noexcept {
        return jet.belongs_to_family(state_);
    }
    class SymbolicMoment {
        friend class PreparedCovariantFamily;
        std::shared_ptr<const State> owner_;
        F value_, derivative_;
        std::vector<F> positions_;
        SymbolicMoment(std::shared_ptr<const State> owner, F value, F derivative, std::vector<F> positions)
            : owner_(std::move(owner)), value_(std::move(value)), derivative_(std::move(derivative)),
              positions_(std::move(positions)) {}

      public:
        const F &formal() const & noexcept {
            return value_;
        }
        const F &formal() const && = delete;
        const F &derivative() const & noexcept {
            return derivative_;
        }
        const F &derivative() const && = delete;
        const std::vector<F> &covariant_positions() const & noexcept {
            return positions_;
        }
        const std::vector<F> &covariant_positions() const && = delete;
        Q evaluate(const OwnedJetPoint &point, Guard &guard) const {
            PreparedCovariantFamily family(owner_);
            (void)family.jet(point, guard);
            covariant_family_detail::Work w(guard, family.limits());
            return w.evaluate(value_, point.value());
        }
    };
    template <class A, class B, class C = std::size_t>
        requires(std::floating_point<A> || std::floating_point<B> || std::floating_point<C>)
    SymbolicMoment symbolic_moment(A, B, Guard &, C = 128) const = delete;
    // Symbolic moment construction currently admits diagonal rational metrics.
    // General positive metrics remain supported by jet/adjoint/moment actions.
    SymbolicMoment symbolic_moment(std::size_t grade, std::size_t power, Guard &guard,
                                   std::size_t max_power = 128) const {
        using namespace covariant_family_detail;
        if (power > max_power)
            throw std::length_error("symbolic moment power budget");
        validate_receiving(guard);
        const auto n_allowance = covariant_detail::add(metrics().at(grade).rows, 1);
        const auto coefficients = covariant_detail::mul(
            16, covariant_detail::mul(covariant_detail::add(power, 1),
                                      covariant_detail::mul(covariant_detail::mul(n_allowance, n_allowance),
                                                            covariant_detail::add(limits().max_degree, 1))));
        auto g = covariant_detail::phase(guard, covariant_detail::add(state_->retained, coefficients));
        Work w(g, limits());
        const auto count = metrics().size();
        for (const auto &m : metrics())
            for (std::size_t i = 0; i < m.rows; ++i)
                for (std::size_t j = 0; j < m.cols; ++j)
                    if (i != j && !m.at(i, j).is_zero())
                        throw std::invalid_argument(
                            "symbolic Hodge moments require declared diagonal metric families");
        const auto n = metrics().at(grade).rows;
        FM l(n, n, w);
        for (std::size_t k = 1; k < count; ++k) {
            const auto &b = boundaries()[k];
            FM a(b.cols, b.rows, w);
            for (std::size_t i = 0; i < a.rows; ++i)
                for (std::size_t j = 0; j < a.cols; ++j)
                    a.at(i, j) =
                        w.divide(w.times(b.at(j, i), metrics()[k - 1].at(j, j)), metrics()[k].at(i, i));
            if (k == grade)
                l = sum(l, product(a, b, w), w);
            if (k == grade + 1)
                l = sum(l, product(b, a, w), w);
        }
        const auto ld = derivative(l, w), lc = sum(sum(ld, product(l, connections()[grade], w), w),
                                                   product(connections()[grade], l, w), w, true);
        const auto xd = derivative(fields()[grade], w),
                   xc = sum(xd, product(connections()[grade], fields()[grade], w), w, true);
        const auto md = derivative(metrics()[grade], w),
                   mr = sum(sum(md, product(transpose(connections()[grade], w), metrics()[grade], w), w),
                            product(metrics()[grade], connections()[grade], w), w);
        std::vector<FM> powers{fields()[grade]};
        for (std::size_t i = 0; i < power; ++i)
            powers.push_back(product(l, powers.back(), w));
        const auto value = pairing(fields()[grade], metrics()[grade], powers.back(), w);
        auto transported = w.plus(w.plus(pairing(xc, metrics()[grade], powers.back(), w),
                                         pairing(powers.back(), metrics()[grade], xc, w)),
                                  pairing(fields()[grade], mr, powers.back(), w));
        std::vector<F> positions;
        for (std::size_t i = 0; i < power; ++i) {
            positions.push_back(
                pairing(powers[i], metrics()[grade], product(lc, powers[power - 1 - i], w), w));
            transported = w.plus(transported, positions.back());
        }
        auto dot = w.derivative(value);
        if (!w.plus(dot, transported, true).is_zero())
            throw std::logic_error("symbolic all-power covariant product rule residual");
        guard.step();
        return SymbolicMoment(state_, value, std::move(dot), std::move(positions));
    }

  private:
    explicit PreparedCovariantFamily(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
class CovariantMapFamily {
    using Guard = covariant_detail::Guard;
    using E = covariant_family_detail::E;
    using FM = covariant_family_detail::Matrix;
    struct State {
        PreparedCovariantFamily source, target;
        std::size_t source_grade, target_grade;
        std::vector<E> declarations;
        FM value;
        geometry::PreparedUnivariateFamily rank;
    };

  public:
    static CovariantMapFamily declare(PreparedCovariantFamily source, std::size_t source_grade,
                                      PreparedCovariantFamily target, std::size_t target_grade,
                                      std::vector<E> entries, Guard &guard) {
        using namespace covariant_family_detail;
        if (!source.domain().same_owner(target.domain()))
            throw std::invalid_argument("map-family parameter domain differs");
        auto g = covariant_detail::phase(
            guard, covariant_detail::add(source.retained_allowance(), target.retained_allowance()));
        Work w(g, source.limits());
        auto value = matrix(target.carriers().at(target_grade).dimension(),
                            source.carriers().at(source_grade).dimension(), entries, source.domain(), w);
        auto rank = geometry::PreparedUnivariateFamily::prepare(
            source.domain(), target.carriers()[target_grade], source.carriers()[source_grade], entries, g,
            source.limits());
        return CovariantMapFamily(std::make_shared<const State>(
            State{std::move(source), std::move(target), source_grade, target_grade, std::move(entries),
                  std::move(value), std::move(rank)}));
    }
    template <class A, class B>
        requires(std::floating_point<A> || std::floating_point<B>)
    static CovariantMapFamily declare(PreparedCovariantFamily, A, PreparedCovariantFamily, B, std::vector<E>,
                                      Guard &) = delete;
    const PreparedCovariantFamily &source() const & noexcept {
        return state_->source;
    }
    const PreparedCovariantFamily &source() const && = delete;
    const PreparedCovariantFamily &target() const & noexcept {
        return state_->target;
    }
    const PreparedCovariantFamily &target() const && = delete;
    std::size_t source_grade() const noexcept {
        return state_->source_grade;
    }
    std::size_t target_grade() const noexcept {
        return state_->target_grade;
    }
    const FM &formal() const & noexcept {
        return state_->value;
    }
    const FM &formal() const && = delete;
    const geometry::PreparedUnivariateFamily &rank_scope() const & noexcept {
        return state_->rank;
    }
    const geometry::PreparedUnivariateFamily &rank_scope() const && = delete;
    const std::vector<E> &declarations() const & noexcept {
        return state_->declarations;
    }
    const std::vector<E> &declarations() const && = delete;
    RectangularCovariantJet jet(const PreparedCovariantJet &source_jet,
                                const PreparedCovariantJet &target_jet, Guard &guard) const {
        using namespace covariant_family_detail;
        if (!source().owns(source_jet) || !target().owns(target_jet))
            throw std::invalid_argument("map-family realized source owners differ");
        (void)regular_cell(rank_scope(), source_jet.base().point(), guard, source().limits());
        Work w(guard, source().limits());
        auto value = evaluate(formal(), source_jet.base().point().value(), w),
             dot = evaluate(derivative(formal(), w), source_jet.base().point().value(), w);
        return RectangularCovariantJet::declare(source_jet, source_grade(), target_jet, target_grade(),
                                                std::move(value), std::move(dot), guard);
    }
    void certify_projector(Guard &guard) const {
        using namespace covariant_family_detail;
        if (!source().same_owner(target()) || source_grade() != target_grade())
            throw std::invalid_argument("projector family actual carrier differs");
        Work w(guard, source().limits());
        if (!sum(product(formal(), formal(), w), formal(), w, true).zero())
            throw std::invalid_argument("exact projector family identity fails");
    }

  private:
    explicit CovariantMapFamily(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
class PreparedLayerFamily {
    using Guard = covariant_detail::Guard;
    using E = covariant_family_detail::E;
    using FM = covariant_family_detail::Matrix;
    using P = covariant_family_detail::P;
    struct State {
        CovariantMapFamily first, second;
        std::vector<E> offset_declarations, kernel_declarations;
        FM offset, kernel;
        geometry::PreparedUnivariateFamily scope;
        std::size_t constraint_rank;
    };

  public:
    static PreparedLayerFamily prepare(CovariantMapFamily first, CovariantMapFamily second,
                                       std::vector<E> offset, std::size_t free_dimension,
                                       std::vector<E> kernel, Guard &guard) {
        using namespace covariant_family_detail;
        if (!first.target().same_owner(second.target()) || first.target_grade() != second.target_grade() ||
            !first.source().domain().same_owner(second.source().domain()))
            throw std::invalid_argument("exact layer actual common target/domain differs");
        auto g = covariant_detail::phase(
            guard, covariant_detail::add(covariant_detail::add(first.source().retained_allowance(),
                                                               second.source().retained_allowance()),
                                         first.target().retained_allowance()));
        const auto limits = first.source().limits();
        Work w(g, limits);
        const auto a = first.formal().cols, b = second.formal().cols, n = covariant_detail::add(a, b),
                   m = first.formal().rows;
        FM c(m, n, w);
        for (std::size_t i = 0; i < m; ++i) {
            for (std::size_t j = 0; j < a; ++j)
                c.at(i, j) = first.formal().at(i, j);
            for (std::size_t j = 0; j < b; ++j)
                c.at(i, a + j) = w.plus(F{}, second.formal().at(i, j), true);
        }
        auto o = matrix(n, 1, offset, first.source().domain(), w),
             k = matrix(n, free_dimension, kernel, first.source().domain(), w);
        if (!product(c, o, w).zero() || !product(c, k, w).zero())
            throw std::invalid_argument("exact layer complete affine family compatibility fails");
        auto joint = geometry::UnivariateCarrier::declare("ordered-layer-joint-carrier", n),
             free = geometry::UnivariateCarrier::declare("joint-free-coordinates", free_dimension);
        auto cr = census(c, first.source().domain(), first.target().carriers()[first.target_grade()], joint,
                         g, limits);
        auto kr = census(k, first.source().domain(), joint, free, g, limits);
        if (kr.generic_rank() != free_dimension ||
            covariant_detail::add(cr.generic_rank(), free_dimension) != n)
            throw std::invalid_argument("exact layer supplied affine frame is incomplete");
        std::vector<P> guards = first.source().domain().original_guards();
        for (const auto *map : {&first, &second}) {
            for (const auto &e : map->declarations())
                guards.push_back(e.original_denominator());
            guards.push_back(map->rank_scope().root_polynomial());
            guards.push_back(map->source().regular_scope().root_polynomial());
            guards.push_back(map->target().regular_scope().root_polynomial());
        }
        for (const auto *declarations : {&offset, &kernel})
            for (const auto &e : *declarations)
                guards.push_back(e.original_denominator());
        guards.push_back(cr.rank_polynomial());
        guards.push_back(kr.rank_polynomial());
        const auto rank = cr.generic_rank();
        auto regular = scope(first.source().domain(), guards, g, limits);
        guard.step();
        return PreparedLayerFamily(std::make_shared<const State>(
            State{std::move(first), std::move(second), std::move(offset), std::move(kernel), std::move(o),
                  std::move(k), std::move(regular), rank}));
    }
    template <std::floating_point T>
    static PreparedLayerFamily prepare(CovariantMapFamily, CovariantMapFamily, std::vector<E>, T,
                                       std::vector<E>, Guard &) = delete;
    PreparedLayerJet jet(const OwnedJetPoint &point, Guard &guard) const {
        using namespace covariant_family_detail;
        if (!state_->first.source().domain().same_owner(point.domain()))
            throw std::invalid_argument("layer point domain owner differs");
        (void)regular_cell(state_->scope, point, guard, state_->first.source().limits());
        const auto x = state_->first.source().jet(point, guard),
                   y = state_->second.source().jet(point, guard),
                   z = state_->first.target().jet(point, guard);
        const auto a = state_->first.jet(x, z, guard), b = state_->second.jet(y, z, guard);
        Work w(guard, state_->first.source().limits());
        return PreparedLayerJet::prepare(a, b, vector(state_->offset, point.value(), w),
                                         vector(derivative(state_->offset, w), point.value(), w),
                                         evaluate(state_->kernel, point.value(), w),
                                         evaluate(derivative(state_->kernel, w), point.value(), w), guard);
    }
    std::size_t constraint_rank() const noexcept {
        return state_->constraint_rank;
    }
    const FM &kernel() const & noexcept {
        return state_->kernel;
    }
    const FM &kernel() const && = delete;
    const FM &offset() const & noexcept {
        return state_->offset;
    }
    const FM &offset() const && = delete;
    const geometry::PreparedUnivariateFamily &regular_scope() const & noexcept {
        return state_->scope;
    }
    const geometry::PreparedUnivariateFamily &regular_scope() const && = delete;

  private:
    explicit PreparedLayerFamily(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::temporal
