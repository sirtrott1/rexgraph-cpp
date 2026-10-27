#pragma once
#include "rfc/inference/native_krylov.hpp"
#include "rfc/inference/native_tower_operators.hpp"
#include <numeric>

namespace rfc::inference {
enum class NativeOrbitKind { Fixed, Periodic, Nonperiodic, UnresolvedModes };
struct NativeOrbitLimits {
    NativeKrylovLimits krylov{};
    std::size_t divisor_trials{100000}, sample_steps{256};
};
namespace cayley_orbit_detail {
using namespace krylov_detail;
inline Q evaluate(const V &p, const Q &x, G &g) {
    Q y = 0;
    for (auto i = p.rbegin(); i != p.rend(); ++i)
        y = g.add(g.mul(y, x), *i);
    return y;
}
inline std::pair<V, Q> divide(const V &p, const Q &root, G &g) {
    if (p.size() < 2)
        throw std::invalid_argument("Cayley factor degree");
    V q(p.size() - 1);
    q.back() = p.back();
    for (std::size_t i = q.size() - 1; i > 0; --i)
        q[i - 1] = g.add(p[i], g.mul(root, q[i]));
    return {q, g.add(p[0], g.mul(root, q[0]))};
}
inline std::vector<numeric::Integer> divisors(numeric::Integer n, std::size_t &trials, std::size_t cap,
                                              G &g) {
    if (n < 0)
        n = -n;
    if (n == 0)
        throw std::logic_error("zero divisor census");
    std::vector<numeric::Integer> out;
    for (numeric::Integer i = 1; i <= n / i; ++i) {
        g.step();
        if (trials == cap)
            throw std::length_error("complete rational-factor divisor work cap");
        ++trials;
        g.value(Q{i});
        if (n % i != 0)
            continue;
        g.entries(mul(8, add(out.size(), 2)));
        out.push_back(i);
        numeric::Integer other = n / i;
        if (other != i)
            out.push_back(std::move(other));
    }
    return out;
}
// Exhaustive rational-root theorem, never a bounded candidate-value search.
// Exhausting the work cap refuses; it does not assert irreducibility/completion.
inline V rational_roots(V p, std::size_t cap, G &g) {
    V roots;
    if (p.size() > 1 && p[0] == 0) {
        roots.push_back(0);
        p.erase(p.begin());
    }
    if (p.size() == 1)
        return roots;
    numeric::Integer scale = 1;
    for (const auto &q : p) {
        g.step();
        mpz_lcm(scale.get_mpz_t(), scale.get_mpz_t(), q.get_den_mpz_t());
        g.value(Q{scale});
    }
    std::vector<numeric::Integer> z;
    numeric::Integer content = 0;
    for (const auto &q : p) {
        const auto v = g.mul(q, Q{scale});
        z.push_back(v.get_num());
        mpz_gcd(content.get_mpz_t(), content.get_mpz_t(), v.get_num_mpz_t());
    }
    for (auto &a : z)
        a /= content;
    std::size_t trials = 0;
    const auto numerators = divisors(z.front(), trials, cap, g),
               denominators = divisors(z.back(), trials, cap, g);
    for (const auto &n : numerators)
        for (const auto &d : denominators)
            for (int sign : {-1, 1}) {
                Q root(n, d);
                root.canonicalize();
                if (sign < 0)
                    root = -root;
                g.value(root);
                if (evaluate(p, root, g) == 0 && std::find(roots.begin(), roots.end(), root) == roots.end()) {
                    g.entries(mul(8, add(roots.size(), 1)));
                    roots.push_back(std::move(root));
                }
            }
    std::sort(roots.begin(), roots.end());
    return roots;
}
inline std::size_t rational_cosine_order(const Q &cosine) {
    if (cosine == 1)
        return 1;
    if (cosine == -1)
        return 2;
    if (cosine == 0)
        return 4;
    if (cosine == Q{1, 2})
        return 6;
    if (cosine == Q{-1, 2})
        return 3;
    return 0;
}
inline bool equal(const NativeComplexTowerField &a, const NativeComplexTowerField &b) {
    if (a.real.grades.size() != b.real.grades.size() ||
        a.imaginary.grades.size() != b.imaginary.grades.size())
        return false;
    for (std::size_t k = 0; k < a.real.grades.size(); ++k)
        if (!krylov_detail::equal(a.real.grades[k], b.real.grades[k]) ||
            !krylov_detail::equal(a.imaginary.grades[k], b.imaginary.grades[k]))
            return false;
    return true;
}
} // namespace cayley_orbit_detail
// Initial admitted orbit profile: a real rational field on one actual source
// grade, full positive rational tower metrics, strict chain, rational Cayley step.
// General algebraic residual modes retain an explicit unresolved result.
class PreparedNativeCayleyOrbit {
    using Q = krylov_detail::Q;
    using V = krylov_detail::V;
    using G = krylov_detail::G;
    using M = krylov_detail::M;

  public:
    struct Mode {
        Q eigenvalue, cosine;
        std::size_t order;
        NativeFieldValue component;
    };

  private:
    struct State {
        PreparedNativeOperatorFamily family;
        PreparedNativeKrylov krylov;
        PreparedNativeTowerOperator cayley;
        NativeComplexTowerField initial;
        core::Grade grade;
        Q step;
        std::vector<Mode> modes;
        V unresolved;
        NativeOrbitKind kind;
        std::optional<std::size_t> period;
        NativeOrbitLimits limits;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeCayleyOrbit(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    static auto dependencies(const PreparedNativeOperatorFamily &f, const NativeFieldValue &x) {
        auto pins = f.publications();
        for (const auto &p : x.publications())
            native_action_detail::append_pin(pins, p);
        return pins;
    }
    static void strict(const PreparedNativeOperatorFamily &family, G &g) {
        const auto &f = family.numeric_family();
        f.validate_source(g);
        const auto source = f.tower().source();
        M previous;
        for (std::size_t k = 0; k < source->grade_count(); ++k) {
            field::hodge_frame_detail::positive(f.tower().metrics()[k].materialize(), g);
            if (!k)
                continue;
            auto b =
                field::hodge_frame_detail::boundary(*source, core::Grade{static_cast<std::uint16_t>(k)}, g);
            if (k > 1 && numeric::checked_multiply(previous, b, g).nonzeros())
                throw std::invalid_argument("Cayley orbit requires the complete strict chain family");
            previous = std::move(b);
        }
    }
    struct HistoryStep {
        std::shared_ptr<const State> plan;
        NativeComplexTowerField input;
        std::size_t position;
    };
    static NativeComplexTowerField iterate(const std::shared_ptr<const State> &s,
                                           NativeComplexTowerField before, std::size_t position,
                                           const NativeActionAvailability &available,
                                           const runtime::ExecutionLease &lease, G &g) {
        auto after = s->cayley.execute_with_guard(before, available, lease, g);
        auto witness = std::make_shared<const HistoryStep>(HistoryStep{s, std::move(before), position});
        for (auto *part : {&after.real, &after.imaginary})
            for (auto &value : part->grades) {
                auto out = *value.state_;
                out.owners = {witness};
                out.profiles = {"native-ordered-cayley-history-v1"};
                value = NativeFieldValue(std::move(out));
                g.step();
            }
        return after;
    }
    static Q quadrance(const State &s, const NativeComplexTowerField &x, G &g) {
        Q value = 0;
        for (const auto *part : {&x.real, &x.imaginary})
            for (std::size_t k = 0; k < part->grades.size(); ++k) {
                const auto &v = part->grades[k].sparse_values();
                auto m =
                    s.family.numeric_family().metric(core::Grade{static_cast<std::uint16_t>(k)}).apply(v, g);
                value = g.add(value, field::factored_action_detail::dot(v, m, g));
            }
        return value;
    }
    static void validate(const State &s, const NativeActionAvailability &a,
                         const runtime::ExecutionLease &lease, G &g) {
        lease.check_cancelled();
        g.entries(s.retained);
        s.krylov.validate_receiving(a, lease, g);
        s.family.numeric_family().validate_source(g);
        g.value(s.step);
        g.vector(s.unresolved);
        for (const auto &m : s.modes) {
            g.value(m.eigenvalue);
            g.value(m.cosine);
            g.sparse_vector(m.component.sparse_values());
        }
        g.step();
    }
    static PreparedNativeCayleyOrbit build(PreparedNativeOperatorFamily family, NativeFieldValue start,
                                           Q step, std::optional<V> declared, bool require_complete,
                                           const NativeActionAvailability &available,
                                           const runtime::ExecutionLease &lease, NativeOrbitLimits limits,
                                           G &g) {
        using namespace cayley_orbit_detail;
        if (limits.sample_steps > 256)
            throw std::length_error("bounded ordered Cayley history supports at most256 steps");
        step = g.canonical_input(V{step})[0];
        if (start.carrier().declaration->kind != storage::TemporalCarrierKind::SourceGrade)
            throw std::invalid_argument("Cayley starting field must name its source grade");
        const auto grade = start.carrier().declaration->grade;
        if (!family.carrier(grade).same(start.carrier()))
            throw std::invalid_argument("Cayley field and family have different actual owners");
        auto action = family.action_with_guard({field::OperatorKind::Hodge, grade, 0}, available, lease, g);
        admit(g, PreparedNativeKrylov::allowance(action, start), start.carrier().size(),
              limits.krylov.max_degree);
        strict(family, g);
        auto krylov = PreparedNativeKrylov::build(action, start, available, lease, limits.krylov, g);
        V roots = declared
                      ? g.canonical_input(*declared)
                      : (step == 0 ? V{} : rational_roots(krylov.polynomial(), limits.divisor_trials, g));
        V remainder = krylov.polynomial();
        std::vector<Q> seen;
        for (const auto &root : roots) {
            if (root < 0 || std::find(seen.begin(), seen.end(), root) != seen.end())
                throw std::invalid_argument("rational Cayley modes must be distinct and nonnegative");
            auto [quotient, residual] = divide(remainder, root, g);
            if (residual != 0)
                throw std::invalid_argument("declared mode is not an active minimal-polynomial root");
            remainder = std::move(quotient);
            seen.push_back(root);
        }
        if (require_complete && remainder != V{Q{1}})
            throw std::invalid_argument(
                "declared rational factors do not cover the complete active minimal polynomial");
        auto cayley = PreparedNativeTowerOperator::prepare(family, NativeTowerOperatorKind::DiracCayley, step,
                                                           available, lease);
        for (std::size_t i = 0; i < cayley.preparation_steps(); ++i)
            g.step();
        NativeComplexTowerField initial{cayley.zero_tower(g), cayley.zero_tower(g)};
        initial.real.grades[grade.value] = start;
        std::vector<Mode> modes;
        auto sum = start.scaled(0, g);
        std::size_t period = 1;
        bool nonperiodic = false;
        const auto lambda = g.mul(step, step);
        const auto metric = family.numeric_family().metric(grade);
        for (const auto &root : roots) {
            auto [weights, residual] = divide(krylov.polynomial(), root, g);
            if (residual != 0)
                throw std::logic_error("active mode division");
            const auto denominator = evaluate(weights, root, g);
            if (denominator == 0)
                throw std::logic_error("Hodge minimal polynomial must have simple active factors");
            auto component = start.scaled(0, g);
            for (std::size_t i = 0; i < weights.size(); ++i)
                component = component.plus(krylov.powers()[i].scaled(g.div(weights[i], denominator), g), g);
            if (component.sparse_values().nonzeros() == 0 ||
                !krylov_detail::equal(action.execute_with_guard(component, available, lease, g),
                                      component.scaled(root, g)))
                throw std::logic_error("active rational mode full-vector residual");
            for (const auto &old : modes)
                if (field::factored_action_detail::dot(
                        component.sparse_values(), metric.apply(old.component.sparse_values(), g), g) != 0)
                    throw std::logic_error("active modes are not metric orthogonal");
            const auto v = g.mul(lambda, root), cosine = g.div(g.sub(1, v), g.add(1, v));
            const auto order = rational_cosine_order(cosine);
            if (!order)
                nonperiodic = true;
            else
                period = mul(period / std::gcd(period, order), order);
            sum = sum.plus(component, g);
            modes.push_back({root, cosine, order, std::move(component)});
        }
        const bool complete = remainder == V{Q{1}};
        if (complete && !krylov_detail::equal(sum, start))
            throw std::logic_error("complete active modes do not reconstruct the starting field");
        NativeOrbitKind kind =
            complete ? (nonperiodic ? NativeOrbitKind::Nonperiodic
                                    : (period == 1 ? NativeOrbitKind::Fixed : NativeOrbitKind::Periodic))
                     : NativeOrbitKind::UnresolvedModes;
        // The declared zero step fixes every field even when spectral factors
        // remain unresolved. Zero/harmonic fields have independent fixed proofs.
        if (step == 0 || krylov.degree() == 0 ||
            (complete && modes.size() == 1 && modes[0].eigenvalue == 0)) {
            kind = NativeOrbitKind::Fixed;
            period = 1;
        }
        std::size_t tower = 0;
        for (const auto &x : initial.real.grades)
            tower = add(tower, add(x.carrier().size(), 1));
        std::size_t factors = 0;
        for (const auto *actions : {&cayley.state_->lowering, &cayley.state_->raising, &cayley.state_->green})
            for (const auto &a : *actions)
                factors = add(factors, mul(16, add(a.retained_entries(), 1)));
        const auto retained = add(add(krylov.retained_allowance(), factors),
                                  add(mul(128, mul(add(tower, 1), add(modes.size(), 4))),
                                      mul(32, add(roots.size(), remainder.size()))));
        g.entries(retained);
        auto result = std::make_shared<const State>(
            State{std::move(family), std::move(krylov), std::move(cayley), std::move(initial), grade, step,
                  std::move(modes), std::move(remainder), kind,
                  (kind == NativeOrbitKind::Fixed || kind == NativeOrbitKind::Periodic)
                      ? std::optional<std::size_t>{period}
                      : std::nullopt,
                  limits, retained});
        validate(*result, available, lease, g);
        if (result->period) {
            // Verify the full complex vector, not its quadrance or observation.
            std::size_t width = 0;
            for (const auto &x : result->initial.real.grades)
                width = add(width, add(x.carrier().size(), 64));
            g.entries(add(retained, mul(128, mul(add(*result->period, 1), width))));
            auto current = result->initial;
            const auto q = quadrance(*result, current, g);
            for (std::size_t i = 1; i <= *result->period; ++i) {
                current = iterate(result, std::move(current), i, available, lease, g);
                if (quadrance(*result, current, g) != q ||
                    equal(current, result->initial) != (i == *result->period))
                    throw std::logic_error("full-vector return/minimal period certificate failed");
            }
        }
        g.step();
        lease.check_cancelled();
        return PreparedNativeCayleyOrbit(std::move(result));
    }

  public:
    class Sample {
        friend class PreparedNativeCayleyOrbit;
        std::shared_ptr<const State> owner_;
        NativeComplexTowerField value_;
        std::size_t step_;
        Q quadrance_;
        Sample(std::shared_ptr<const State> owner, NativeComplexTowerField value, std::size_t step, Q q)
            : owner_(std::move(owner)), value_(std::move(value)), step_(step), quadrance_(std::move(q)) {}

      public:
        const NativeComplexTowerField &value() const & noexcept {
            return value_;
        }
        const NativeComplexTowerField &value() const && = delete;
        const Q &quadrance() const & noexcept {
            return quadrance_;
        }
        const Q &quadrance() const && = delete;
        std::size_t iterate() const noexcept {
            return step_;
        }
    };
    static PreparedNativeCayleyOrbit prepare_with_guard(PreparedNativeOperatorFamily family,
                                                        NativeFieldValue start, Q step,
                                                        const NativeActionAvailability &available,
                                                        const runtime::ExecutionLease &lease, G &g,
                                                        NativeOrbitLimits limits = {}) {
        return build(std::move(family), std::move(start), std::move(step), {}, false, available, lease,
                     limits, g);
    }
    static PreparedNativeCayleyOrbit prepare(PreparedNativeOperatorFamily family, NativeFieldValue start,
                                             Q step, const NativeActionAvailability &available,
                                             const runtime::ExecutionLease &lease = runtime::ExecutionLease{},
                                             NativeOrbitLimits limits = {}) {
        G g(native_action_detail::limits(lease, dependencies(family, start), family.dimension_limit()),
            [&lease] { lease.check_cancelled(); });
        return build(std::move(family), std::move(start), std::move(step), {}, false, available, lease,
                     limits, g);
    }
    static PreparedNativeCayleyOrbit
    complete(PreparedNativeOperatorFamily family, NativeFieldValue start, Q step, V roots,
             const NativeActionAvailability &available,
             const runtime::ExecutionLease &lease = runtime::ExecutionLease{},
             NativeOrbitLimits limits = {}) {
        G g(native_action_detail::limits(lease, dependencies(family, start), family.dimension_limit()),
            [&lease] { lease.check_cancelled(); });
        return build(std::move(family), std::move(start), std::move(step), std::move(roots), true, available,
                     lease, limits, g);
    }
    static PreparedNativeCayleyOrbit partial(PreparedNativeOperatorFamily family, NativeFieldValue start,
                                             Q step, V roots, const NativeActionAvailability &available,
                                             const runtime::ExecutionLease &lease = runtime::ExecutionLease{},
                                             NativeOrbitLimits limits = {}) {
        G g(native_action_detail::limits(lease, dependencies(family, start), family.dimension_limit()),
            [&lease] { lease.check_cancelled(); });
        return build(std::move(family), std::move(start), std::move(step), std::move(roots), false, available,
                     lease, limits, g);
    }
    template <std::floating_point T>
    static PreparedNativeCayleyOrbit
    prepare_with_guard(PreparedNativeOperatorFamily, NativeFieldValue, T, const NativeActionAvailability &,
                       const runtime::ExecutionLease &, G &, NativeOrbitLimits = {}) = delete;
    template <std::floating_point T>
    static PreparedNativeCayleyOrbit
    prepare(PreparedNativeOperatorFamily, NativeFieldValue, T, const NativeActionAvailability &,
            const runtime::ExecutionLease & = runtime::ExecutionLease{}, NativeOrbitLimits = {}) = delete;
    template <std::floating_point T>
    static PreparedNativeCayleyOrbit
    complete(PreparedNativeOperatorFamily, NativeFieldValue, T, V, const NativeActionAvailability &,
             const runtime::ExecutionLease & = runtime::ExecutionLease{}, NativeOrbitLimits = {}) = delete;
    template <std::floating_point T>
    static PreparedNativeCayleyOrbit
    partial(PreparedNativeOperatorFamily, NativeFieldValue, T, V, const NativeActionAvailability &,
            const runtime::ExecutionLease & = runtime::ExecutionLease{}, NativeOrbitLimits = {}) = delete;
    const PreparedNativeKrylov &krylov() const & noexcept {
        return state_->krylov;
    }
    const PreparedNativeKrylov &krylov() const && = delete;
    const Q &step() const & noexcept {
        return state_->step;
    }
    const Q &step() const && = delete;
    const std::vector<Mode> &modes() const & noexcept {
        return state_->modes;
    }
    const std::vector<Mode> &modes() const && = delete;
    const V &unresolved_polynomial() const & noexcept {
        return state_->unresolved;
    }
    const V &unresolved_polynomial() const && = delete;
    NativeOrbitKind kind() const noexcept {
        return state_->kind;
    }
    std::optional<std::size_t> period() const noexcept {
        return state_->period;
    }
    Sample sample(std::size_t count, const NativeActionAvailability &available,
                  const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) const {
        const auto s = state_;
        G g(native_action_detail::limits(lease, dependencies(s->family, s->krylov.start()),
                                         s->family.dimension_limit()),
            [&lease] { lease.check_cancelled(); });
        if (count > s->limits.sample_steps)
            throw std::length_error("Cayley explicit sample step budget");
        validate(*s, available, lease, g);
        std::size_t width = 0;
        for (const auto &x : s->initial.real.grades)
            width = krylov_detail::add(width, krylov_detail::add(x.carrier().size(), 64));
        g.entries(krylov_detail::add(
            s->retained, krylov_detail::mul(128, krylov_detail::mul(krylov_detail::add(count, 1), width))));
        auto value = s->initial;
        const auto q = quadrance(*s, value, g);
        for (std::size_t i = 0; i < count; ++i) {
            value = iterate(s, std::move(value), i + 1, available, lease, g);
            if (quadrance(*s, value, g) != q)
                throw std::logic_error("Cayley quadrance residual");
        }
        g.step();
        return Sample(s, std::move(value), count, q);
    }
    template <std::floating_point T>
    Sample sample(T, const NativeActionAvailability &,
                  const runtime::ExecutionLease & = runtime::ExecutionLease{}) const = delete;
};
} // namespace rfc::inference
