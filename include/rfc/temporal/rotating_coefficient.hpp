#pragma once
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/temporal/response_derivative.hpp"
#include <span>

namespace rfc::temporal {
namespace rotating_detail {
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using G = numeric::ExactActionGuard;
using covariant_detail::add;
using covariant_detail::mul;
using covariant_detail::phase;
inline Q dot(const V &a, const V &b, G &g) {
    if (a.size() != b.size())
        throw std::invalid_argument("rotating pairing carrier");
    Q out;
    for (std::size_t i = 0; i < a.size(); ++i)
        out = g.add(out, g.mul(a[i], b[i]));
    return out;
}
inline std::size_t source_words(const core::RelationalComplex &s, G &g) {
    auto count = covariant_detail::source_allowance(s, g);
    const auto &signature = s.signature();
    for (std::size_t i = 0; i < signature.type_count(); ++i) {
        g.step();
        const auto &t = signature.type(static_cast<core::RelationTypeId>(i));
        count = add(count, add(t.name.size(), mul(8, add(t.constraints.rules.size(), 1))));
        g.entries(count);
        for (const auto &r : t.constraints.rules) {
            g.step();
            g.value(r.value);
        }
    }
    for (std::size_t i = 0; i < signature.aspects().size(); ++i) {
        g.step();
        count = add(count, add(8, signature.aspects().at(static_cast<composite::AspectId>(i)).name.size()));
        g.entries(count);
    }
    for (std::size_t k = 0; k < s.grade_count(); ++k) {
        g.step();
        const auto &a = s.grade({static_cast<std::uint16_t>(k)});
        count = add(count,
                    mul(a.aspects.aspect_count(), machine::PackedBits::words_for_size(a.incidence_count())));
        g.entries(count);
    }
    return count;
}
template <std::integral T>
    requires(!std::same_as<T, bool>)
inline std::size_t index(T i, std::size_t bound) {
    if (!std::in_range<std::size_t>(i) || static_cast<std::size_t>(i) >= bound)
        throw std::out_of_range("rotating source row/occurrence index");
    return static_cast<std::size_t>(i);
}
inline PreparedCovariantJet constant_context(core::OwnedRelationalSource owner, const std::vector<M> &metrics,
                                             G &g) {
    // This private constant direction prepares original-map response factors.
    // It is separate from the public unit-phase parameter and its domain.
    auto d = geometry::UnivariateDomain::declare(
        geometry::UnivariateCoordinate::declare("constant-system-factor"), Q{-1}, Q{1}, true, true, {}, g);
    auto point = OwnedJetPoint::declare(d, d.at(Q{0}, g), g);
    std::vector<V> coefficients, fields;
    CovariantJetInput input;
    for (std::size_t k = 0; k < owner.source().grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        const auto &a = owner.source().grade(grade);
        V values;
        for (std::size_t p = 0; p < a.incidence_count(); ++p) {
            g.step();
            values.push_back(owner.source().incidence_coefficient(grade, p));
        }
        coefficients.push_back(std::move(values));
        fields.emplace_back(a.cell_count());
        input.occurrence_derivatives.emplace_back(a.incidence_count());
        input.metric_derivatives.emplace_back(a.cell_count(), a.cell_count());
        input.connections.emplace_back(a.cell_count(), a.cell_count());
        input.field_derivatives.emplace_back(a.cell_count());
    }
    auto base = OwnedJetBase::declare(std::move(owner), std::move(point), std::move(coefficients), metrics,
                                      std::move(fields), g);
    return PreparedCovariantJet::prepare(std::move(base), JetDirection::declare("constant-system"),
                                         std::move(input), g);
}
inline V response(const PreparedResponseJet &p, V rhs, G &g) {
    const auto n = rhs.size();
    auto field = OwnedDerivativeField::declare(p.equation().target(), p.equation().target_grade(),
                                               std::move(rhs), V(n), g);
    auto result = p.apply(field, g);
    if (!numeric::is_zero(result.dot()))
        throw std::logic_error("constant-system response tangent");
    return result.value();
}
} // namespace rotating_detail

struct RotatingSystemLimits {
    std::size_t max_unknowns{32}, max_equations{64}, max_occurrences{512};
};
class PreparedRotatingCoefficient;
// Full joint right-hand side on an actual immutable source. The original
// real system is proved consistent and has no potential ambiguity.
class OwnedConsistentLinearSystem {
    using Q = rotating_detail::Q;
    using V = rotating_detail::V;
    using M = rotating_detail::M;
    using G = rotating_detail::G;
    struct State {
        core::OwnedRelationalSource owner;
        V rhs, solution;
        M boundary;
        field::PreparedHodgeFrames frames;
        PreparedCovariantJet context;
        PreparedResponseJet potential, dual;
        RotatingSystemLimits limits;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit OwnedConsistentLinearSystem(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    friend class PreparedRotatingCoefficient;
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        (void)rotating_detail::source_words(s.owner.source(), g);
        g.vector(s.rhs);
        g.vector(s.solution);
        g.matrix(s.boundary);
        s.frames.validate(g);
        s.context.validate_receiving(g);
        s.potential.validate_receiving(g);
        s.dual.validate_receiving(g);
        (void)s.potential.factor().factor_receipt(g);
        (void)s.dual.factor().factor_receipt(g);
    }
    static std::size_t admit(const core::RelationalComplex &s, RotatingSystemLimits limits, G &g) {
        using namespace rotating_detail;
        g.step();
        if (limits.max_unknowns > 64 || limits.max_equations > 128 || limits.max_occurrences > 1024)
            throw std::length_error("rotating system admitted finite profile");
        if (s.grade_count() < 2)
            throw std::invalid_argument("rotating system requires equation cells");
        const auto n = s.grade({0}).cell_count(), m = s.grade({1}).cell_count();
        if (!n || !m)
            throw std::invalid_argument("rotating system empty unknown/equation carrier");
        if (n > limits.max_unknowns || m > limits.max_equations ||
            s.grade({1}).incidence_count() > limits.max_occurrences)
            throw std::length_error("rotating system source limits");
        for (std::size_t k = 2; k < s.grade_count(); ++k) {
            g.step();
            if (s.grade({static_cast<std::uint16_t>(k)}).cell_count())
                throw std::invalid_argument("rotating theorem excludes grade-two or higher cells");
        }
        return source_words(s, g);
    }

  public:
    class Row {
        friend class OwnedConsistentLinearSystem;
        std::shared_ptr<const State> owner_;
        std::size_t index_;
        Row(std::shared_ptr<const State> s, std::size_t k) : owner_(std::move(s)), index_(k) {}

      public:
        std::size_t index() const noexcept {
            return index_;
        }
        core::RelationIdentity identity() const {
            return owner_->owner.source().relation_identity({{1}, static_cast<std::uint32_t>(index_)});
        }
    };
    class Coefficient {
        friend class OwnedConsistentLinearSystem;
        friend class PreparedRotatingCoefficient;
        std::shared_ptr<const State> owner_;
        std::size_t row_, slot_, variable_;
        Q value_;
        Coefficient(std::shared_ptr<const State> s, std::size_t row, std::size_t slot, std::size_t v, Q a)
            : owner_(std::move(s)), row_(row), slot_(slot), variable_(v), value_(std::move(a)) {}

      public:
        std::size_t row() const noexcept {
            return row_;
        }
        std::size_t occurrence_slot() const noexcept {
            return slot_;
        }
        std::size_t variable() const noexcept {
            return variable_;
        }
        core::OccurrenceKey identity() const {
            return owner_->owner.source().occurrence_identity({1}, slot_);
        }
        const Q &value() const & noexcept {
            return value_;
        }
        const Q &value() const && = delete;
    };
    static OwnedConsistentLinearSystem declare(core::OwnedRelationalSource owner, std::span<const Q> rhs,
                                               const std::vector<M> &metrics, G &guard,
                                               RotatingSystemLimits limits = {}) {
        using namespace rotating_detail;
        const auto source = admit(owner.source(), limits, guard);
        const auto n = owner.source().grade({0}).cell_count(), m = owner.source().grade({1}).cell_count();
        if (rhs.size() != m || metrics.size() != owner.source().grade_count())
            throw std::invalid_argument("complete system right-hand side and metric coverage");
        const auto width = add(add(n, m), add(metrics.size(), 1));
        const auto retained = add(source, mul(512, mul(width, width)));
        auto g = phase(guard, retained);
        for (std::size_t k = 0; k < metrics.size(); ++k) {
            const auto d = owner.source().grade({static_cast<std::uint16_t>(k)}).cell_count();
            g.matrix(metrics[k]);
            if (metrics[k] != M::identity(d))
                throw std::invalid_argument("rotating theorem requires identity metrics");
        }
        V c;
        c.reserve(rhs.size());
        for (const auto &q : rhs) {
            g.step();
            g.value(q);
            c.push_back(numeric::canonical_rational(q));
        }
        field::PreparedHodgeFrames frames(owner.shared_source(), metrics, {1}, g);
        if (frames.gradient_reduction().rank() != n)
            throw std::invalid_argument("original system must have full column rank and unique potential");
        auto context = constant_context(owner, metrics, g);
        auto b = context.base().boundaries()[1];
        auto potential = PreparedResponseJet::prepare(
            RectangularCovariantJet::declare(context, 0, context, 1, b.transpose(), M(m, n), g), g);
        auto dual = PreparedResponseJet::gauged(
            RectangularCovariantJet::declare(context, 1, context, 0, b, M(n, m), g), frames.harmonic_frame(),
            M(m, frames.harmonic_frame().cols()), g);
        // Response preparation checks the original equation and the complete kernel gauge.
        auto solution = response(potential, c, g);
        if (numeric::checked_transpose_apply(b, solution, g) != c)
            throw std::invalid_argument("original system is inconsistent");
        const auto actual =
            add(add(context.retained_allowance(), add(potential.local_allowance(), dual.local_allowance())),
                add(source, add(add(frames.stored_entries(), frames.stored_index_entries()),
                                add(b.nonzeros(), add(c.size(), solution.size())))));
        if (actual > retained)
            throw std::length_error("rotating system combined retention allowance");
        guard.step();
        return OwnedConsistentLinearSystem(std::make_shared<const State>(
            State{std::move(owner), std::move(c), std::move(solution), std::move(b), std::move(frames),
                  std::move(context), std::move(potential), std::move(dual), limits, retained}));
    }
    static OwnedConsistentLinearSystem declare(core::OwnedRelationalSource owner, std::span<const Q> rhs,
                                               G &g, RotatingSystemLimits limits = {}) {
        (void)admit(owner.source(), limits, g);
        std::vector<M> metrics;
        for (std::size_t k = 0; k < owner.source().grade_count(); ++k) {
            g.step();
            metrics.push_back(
                M::identity(owner.source().grade({static_cast<std::uint16_t>(k)}).cell_count()));
        }
        return declare(std::move(owner), rhs, metrics, g, limits);
    }
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    Row row(T k, G &g) const {
        validate_receiving(g);
        return Row(state_, rotating_detail::index(k, rhs().size()));
    }
    template <std::floating_point T> Row row(T, G &) const = delete;
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    Coefficient coefficient(const Row &row, T local_occurrence, G &g) const {
        validate_receiving(g);
        if (row.owner_ != state_)
            throw std::invalid_argument("foreign actual system row");
        const auto &a = owner().source().grade({1});
        const auto [begin, end] = a.incidence_range(row.index_);
        const auto p = begin + rotating_detail::index(local_occurrence, end - begin);
        if (!a.aspects.get(owner().source().signature().existence_aspect(), p))
            throw std::invalid_argument("absent occurrence is not a rotating coefficient");
        const auto value = owner().source().incidence_coefficient({1}, p);
        g.value(value);
        return Coefficient(state_, row.index_, p, a.lower_index[p], value);
    }
    template <std::floating_point T> Coefficient coefficient(const Row &, T, G &) const = delete;
    const core::OwnedRelationalSource &owner() const & noexcept {
        return state_->owner;
    }
    const core::OwnedRelationalSource &owner() const && = delete;
    const V &rhs() const & noexcept {
        return state_->rhs;
    }
    const V &rhs() const && = delete;
    const V &solution() const & noexcept {
        return state_->solution;
    }
    const V &solution() const && = delete;
    const M &boundary() const & noexcept {
        return state_->boundary;
    }
    const M &boundary() const && = delete;
    const PreparedResponseJet &potential_response() const & noexcept {
        return state_->potential;
    }
    const PreparedResponseJet &potential_response() const && = delete;
    const PreparedResponseJet &dual_response() const & noexcept {
        return state_->dual;
    }
    const PreparedResponseJet &dual_response() const && = delete;
    const field::PreparedHodgeFrames &frames() const & noexcept {
        return state_->frames;
    }
    const field::PreparedHodgeFrames &frames() const && = delete;
    bool same_owner(const OwnedConsistentLinearSystem &s) const noexcept {
        return state_ == s.state_;
    }
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
};

// Proposition30.5. The full-column-rank proof is valid for every unit phase;
// samples additionally verify every equation residual and normal equation.
class PreparedRotatingCoefficient {
    using Q = rotating_detail::Q;
    using V = rotating_detail::V;
    using G = rotating_detail::G;
    using E = geometry::OwnedRationalExpression;

  public:
    struct Parameters {
        Q a, kappa, m_v, g_vv, x, q, numerator, minimum_denominator;
    };

  private:
    struct State {
        OwnedConsistentLinearSystem system;
        OwnedConsistentLinearSystem::Coefficient coefficient;
        geometry::UnivariateDomain domain;
        E real, imaginary;
        V potential, inverse_column, harmonic;
        Parameters parameters;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedRotatingCoefficient(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        s.system.validate_receiving(g);
        s.domain.validate(g);
        g.vector(s.real.original_numerator());
        g.vector(s.real.original_denominator());
        g.vector(s.imaginary.original_numerator());
        g.vector(s.imaginary.original_denominator());
        g.vector(s.potential);
        g.vector(s.inverse_column);
        g.vector(s.harmonic);
        for (const Q *q :
             {&s.parameters.a, &s.parameters.kappa, &s.parameters.m_v, &s.parameters.g_vv, &s.parameters.x,
              &s.parameters.q, &s.parameters.numerator, &s.parameters.minimum_denominator})
            g.value(*q);
    }
    struct Observation {
        std::shared_ptr<const State> owner;
        std::optional<geometry::UnivariateDomain::Value> point;
        Q real, imaginary, distance, denominator, auxiliary_real, auxiliary_imaginary, obstruction;
        V solution_real, solution_imaginary, residual_real, residual_imaginary;
        std::size_t retained;
    };

  public:
    class Sample {
        friend class PreparedRotatingCoefficient;
        std::shared_ptr<const Observation> state_;
        explicit Sample(std::shared_ptr<const Observation> s) : state_(std::move(s)) {}

      public:
        bool half_turn() const noexcept {
            return !state_->point;
        }
        bool solvable() const noexcept {
            return state_->obstruction == 0;
        }
        const Q &phase_real() const & noexcept {
            return state_->real;
        }
        const Q &phase_imaginary() const & noexcept {
            return state_->imaginary;
        }
        const Q &distance_squared() const & noexcept {
            return state_->distance;
        }
        const Q &denominator() const & noexcept {
            return state_->denominator;
        }
        const Q &auxiliary_real() const & noexcept {
            return state_->auxiliary_real;
        }
        const Q &auxiliary_imaginary() const & noexcept {
            return state_->auxiliary_imaginary;
        }
        const Q &obstruction() const & noexcept {
            return state_->obstruction;
        }
        const V &solution_real() const & noexcept {
            return state_->solution_real;
        }
        const V &solution_imaginary() const & noexcept {
            return state_->solution_imaginary;
        }
        const V &residual_real() const & noexcept {
            return state_->residual_real;
        }
        const V &residual_imaginary() const & noexcept {
            return state_->residual_imaginary;
        }
        const auto &point() const & noexcept {
            return state_->point;
        }
        const Q &phase_real() const && = delete;
        const Q &phase_imaginary() const && = delete;
        const Q &distance_squared() const && = delete;
        const Q &denominator() const && = delete;
        const Q &auxiliary_real() const && = delete;
        const Q &auxiliary_imaginary() const && = delete;
        const Q &obstruction() const && = delete;
        const V &solution_real() const && = delete;
        const V &solution_imaginary() const && = delete;
        const V &residual_real() const && = delete;
        const V &residual_imaginary() const && = delete;
        const auto &point() const && = delete;
        void validate_receiving(G &g) const {
            g.entries(state_->retained);
            validate(*state_->owner, g);
            if (state_->point && !state_->owner->domain.contains(*state_->point, g))
                throw std::domain_error("original rotating observation domain");
            for (const Q *q : {&state_->real, &state_->imaginary, &state_->distance, &state_->denominator,
                               &state_->auxiliary_real, &state_->auxiliary_imaginary, &state_->obstruction})
                g.value(*q);
            for (const auto *v : {&state_->solution_real, &state_->solution_imaginary, &state_->residual_real,
                                  &state_->residual_imaginary})
                g.vector(*v);
        }
    };
    static PreparedRotatingCoefficient prepare(OwnedConsistentLinearSystem system,
                                               OwnedConsistentLinearSystem::Coefficient coefficient,
                                               geometry::UnivariateDomain domain, G &guard) {
        using namespace rotating_detail;
        if (coefficient.owner_ != system.state_)
            throw std::invalid_argument("foreign rotating coefficient/source");
        system.validate_receiving(guard);
        domain.validate(guard);
        const auto n = system.solution().size(), m = system.rhs().size();
        std::size_t local = add(256, mul(64, add(n, m)));
        for (const auto &p : domain.original_guards())
            local = add(local, add(p.size(), 1));
        const auto retained = add(system.retained_allowance(), local);
        auto g = phase(guard, retained);
        V axis(m);
        axis[coefficient.row_] = 1;
        auto parts = system.frames().decompose(axis, g);
        auto minimal = response(system.potential_response(), parts.gradient, g);
        const Q kappa = dot(parts.harmonic, parts.harmonic, g);
        if (kappa <= 0)
            throw std::invalid_argument("rotating theorem requires positive harmonic row share");
        V selected(n);
        selected[coefficient.variable_] = 1;
        // M^T z=e_v with the complete harmonic gauge, followed by M g=z.
        // This selects one Gram-inverse column using original response-derivative factors.
        const auto z = response(system.dual_response(), selected, g);
        auto column = response(system.potential_response(), z, g);
        if (numeric::checked_apply(system.boundary(), z, g) != selected ||
            numeric::checked_transpose_apply(system.boundary(), column, g) != z)
            throw std::logic_error("selected original inverse column residual");
        const auto v = coefficient.variable_;
        const auto a = coefficient.value_, aa = g.mul(a, a), mv = minimal[v], gv = column[v];
        if (gv <= 0)
            throw std::logic_error("selected Gram inverse diagonal must be positive");
        const auto x = g.mul(a, system.solution()[v]);
        const auto q = g.add(g.sub(g.mul(aa, g.mul(mv, mv)), g.mul(a, mv)), g.mul(aa, g.mul(kappa, gv)));
        const auto numerator = g.mul(g.mul(x, x), kappa);
        const Q minimum = q < 0 ? g.add(Q{1}, g.mul(Q{4}, q)) : Q{1};
        if (minimum <= 0)
            throw std::logic_error("complete unit-circle denominator positivity");
        auto re = E::declare(domain, {1, 0, -1}, {1, 0, 1}, g);
        auto im = E::declare(domain, {0, 2}, {1, 0, 1}, g);
        geometry::univariate_detail::Work w(g, {});
        auto identity = w.sub(w.sub(w.mul(re.original_denominator(), re.original_denominator()),
                                    w.mul(re.original_numerator(), re.original_numerator())),
                              w.mul(im.original_numerator(), im.original_numerator()));
        if (!identity.empty())
            throw std::logic_error("whole rational unit-phase chart identity");
        guard.step();
        return PreparedRotatingCoefficient(std::make_shared<const State>(
            State{std::move(system), std::move(coefficient), std::move(domain), std::move(re), std::move(im),
                  std::move(minimal), std::move(column), std::move(parts.harmonic),
                  Parameters{a, kappa, mv, gv, x, q, numerator, minimum}, retained}));
    }
    Sample sample(const geometry::UnivariateDomain::Value &at, G &guard) const {
        validate_receiving(guard);
        auto scratch = rotating_detail::phase(guard, state_->retained);
        auto re = state_->real.evaluate(at, scratch);
        auto im = state_->imaginary.evaluate(at, scratch);
        return observe(std::move(re), std::move(im), at, guard);
    }
    Sample half_turn(G &g) const {
        validate_receiving(g);
        return observe(Q{-1}, Q{0}, {}, g);
    }
    const OwnedConsistentLinearSystem &system() const & noexcept {
        return state_->system;
    }
    const OwnedConsistentLinearSystem &system() const && = delete;
    const OwnedConsistentLinearSystem::Coefficient &coefficient() const & noexcept {
        return state_->coefficient;
    }
    const OwnedConsistentLinearSystem::Coefficient &coefficient() const && = delete;
    const geometry::UnivariateDomain &domain() const & noexcept {
        return state_->domain;
    }
    const geometry::UnivariateDomain &domain() const && = delete;
    const Parameters &parameters() const & noexcept {
        return state_->parameters;
    }
    const Parameters &parameters() const && = delete;
    const V &minimal_potential() const & noexcept {
        return state_->potential;
    }
    const V &minimal_potential() const && = delete;
    const V &inverse_column() const & noexcept {
        return state_->inverse_column;
    }
    const V &inverse_column() const && = delete;
    const V &harmonic_row() const & noexcept {
        return state_->harmonic;
    }
    const V &harmonic_row() const && = delete;
    bool same_owner(const PreparedRotatingCoefficient &p) const noexcept {
        return state_ == p.state_;
    }
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    void validate_sample(const Sample &sample, G &g) const {
        if (sample.state_->owner != state_)
            throw std::invalid_argument("foreign rotating observation owner");
        sample.validate_receiving(g);
    }

  private:
    Sample observe(Q re, Q im, std::optional<geometry::UnivariateDomain::Value> point, G &guard) const {
        using namespace rotating_detail;
        const auto &s = *state_;
        const auto &p = s.parameters;
        const auto n = s.system.solution().size(), m = s.system.rhs().size();
        const auto retained = add(s.retained, mul(64, add(add(n, m), 1)));
        auto g = phase(guard, retained);
        if (g.add(g.mul(re, re), g.mul(im, im)) != 1)
            throw std::invalid_argument("phase is not unitary");
        const auto rm = g.sub(re, Q{1}), d = g.add(g.mul(rm, rm), g.mul(im, im));
        const auto dr = g.mul(p.a, rm), di = g.mul(p.a, im), delta2 = g.add(g.mul(dr, dr), g.mul(di, di));
        const auto tr = g.add(Q{1}, g.mul(dr, p.m_v)), ti = g.mul(di, p.m_v);
        const auto D = g.add(g.add(g.mul(tr, tr), g.mul(ti, ti)), g.mul(delta2, g.mul(p.kappa, p.g_vv)));
        if (D <= 0 || D != g.add(Q{1}, g.mul(p.q, d)))
            throw std::logic_error("rotating unit-phase denominator positivity identity");
        const auto alpha = g.add(g.mul(dr, tr), g.mul(di, ti));
        const auto beta = g.sub(g.mul(di, tr), g.mul(dr, ti));
        const auto scale = g.div(s.system.solution()[s.coefficient.variable_], D);
        V ur(n), ui(n);
        for (std::size_t j = 0; j < n; ++j) {
            ur[j] = g.sub(s.system.solution()[j],
                          g.mul(scale, g.add(g.mul(g.mul(delta2, p.kappa), s.inverse_column[j]),
                                             g.mul(alpha, s.potential[j]))));
            ui[j] = g.sub(Q{0}, g.mul(scale, g.mul(beta, s.potential[j])));
        }
        auto rr = numeric::checked_transpose_apply(s.system.boundary(), ur, g);
        auto ri = numeric::checked_transpose_apply(s.system.boundary(), ui, g);
        for (std::size_t k = 0; k < m; ++k)
            rr[k] = g.sub(rr[k], s.system.rhs()[k]);
        const auto k = s.coefficient.row_, v = s.coefficient.variable_;
        rr[k] = g.add(rr[k], g.sub(g.mul(dr, ur[v]), g.mul(di, ui[v])));
        ri[k] = g.add(ri[k], g.add(g.mul(dr, ui[v]), g.mul(di, ur[v])));
        auto nr = numeric::checked_apply(s.system.boundary(), rr, g);
        auto ni = numeric::checked_apply(s.system.boundary(), ri, g);
        nr[v] = g.add(nr[v], g.add(g.mul(dr, rr[k]), g.mul(di, ri[k])));
        ni[v] = g.add(ni[v], g.sub(g.mul(dr, ri[k]), g.mul(di, rr[k])));
        const auto obstruction = g.div(g.mul(p.numerator, d), D);
        if (!numeric::is_zero(nr) || !numeric::is_zero(ni) ||
            g.add(dot(rr, rr, g), dot(ri, ri, g)) != obstruction)
            throw std::logic_error("complete rotating least-squares/obstruction residual");
        guard.step();
        return Sample(std::make_shared<const Observation>(
            Observation{state_, std::move(point), std::move(re), std::move(im), d, D, tr, ti, obstruction,
                        std::move(ur), std::move(ui), std::move(rr), std::move(ri), retained}));
    }
};
} // namespace rfc::temporal
