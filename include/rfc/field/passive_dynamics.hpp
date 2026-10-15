#pragma once

#include "rfc/field/exact_metric.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"

#include "rfc/numeric/prepared_positive_semidefinite.hpp"
#include <map>
#include <mutex>
#include <optional>
#include <utility>
#include <concepts>
#include <cstddef>
#include <memory>
#include <string_view>

namespace rfc::field {

using Rational = rfc::numeric::Rational;
using RationalVector = rfc::numeric::RationalVector;
using ExactMatrix = rfc::numeric::ExactMatrix;

enum class PassiveIntegrationMethod { ImplicitMidpoint, BackwardEuler };

struct PassiveDynamicsLimits {
    std::size_t max_state_dimension{128};
    std::size_t max_input_dimension{128};
    rfc::numeric::ExactActionLimits exact{};
};

// The retained finite-time law is a sparse relation, not an expanded response
// operator: left*xnext = previous*xold + input*u. All three maps are original
// RexGraphCPP nonzero maps. No inverse or collection of response columns exists.
struct PassiveStepRelation {
    ExactMatrix left, previous, input;
    bool operator==(const PassiveStepRelation&) const = default;
};

struct PassiveExecutionProfile {
    rfc::field::ExactMetricKind metric_kind{rfc::field::ExactMetricKind::Identity};
    std::size_t state_dimension{}, input_dimension{}, retained_model_entries{};
    std::size_t retained_equation_entries{}, retained_factor_entries{};
    std::size_t preparation_work{}, fill_insertions{}, exact_cancellations{};
    std::size_t state_solves{}, expanded_response_columns{};
};

struct PassiveStepResult {
    PassiveIntegrationMethod method{PassiveIntegrationMethod::ImplicitMidpoint};
    Rational step_size{};
    RationalVector xnext;
    PassiveStepRelation relation;
    PassiveExecutionProfile execution;
    RationalVector quadrature_state;
    Rational energy_before{}, energy_after{};
    Rational source_work{}, resistance_loss{}, step_dissipation{};
    RationalVector equation_residual;
    // M*(xnext-xold) - h*((J-R)*quadrature_state + B*u).
    RationalVector balance_residual;
    // source_work - (energy_after-energy_before) - both loss terms.
    Rational energy_balance_residual{};
    std::size_t checked_work{};
    bool exact{};
};

class PassiveLinearDynamics;

class PreparedPassiveStep {
public:
    [[nodiscard]] PassiveStepResult apply(const RationalVector& state,
                                             const RationalVector& input = {}) const;
    [[nodiscard]] const PassiveStepRelation& relation() const;
    [[nodiscard]] PassiveExecutionProfile profile() const;
private:
    struct State;
    std::shared_ptr<const State> state_;
    explicit PreparedPassiveStep(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    friend class PassiveLinearDynamics;
};

// Passive rational lumped dynamics: M*dx/dt = (J-R)*x + B*u.
// M is a genuine positive ExactMetric, J is skew-symmetric, and R is positive
// semidefinite. All physical state coordinates have positive stored energy;
// affine constants, algebraic rail variables, and unresolved DAEs are excluded.
// A returned step is exact for its declared finite time rule. It is not a claim
// of equality with the continuous-time matrix exponential or measured hardware.
class PassiveLinearDynamics {
public:
    PassiveLinearDynamics(ExactMatrix M, ExactMatrix J, ExactMatrix R,
                             ExactMatrix B, PassiveDynamicsLimits limits = {});

    [[nodiscard]] std::size_t state_dimension() const noexcept;
    [[nodiscard]] std::size_t input_dimension() const noexcept;
    [[nodiscard]] const ExactMatrix& mass_matrix() const;
    [[nodiscard]] const ExactMatrix& structure_matrix() const;
    [[nodiscard]] const ExactMatrix& resistance_matrix() const;
    [[nodiscard]] const ExactMatrix& input_matrix() const;
    [[nodiscard]] const rfc::field::ExactMetric& energy_metric() const;
    [[nodiscard]] Rational energy(const RationalVector& state) const;

    // u is the explicitly held input (or declared midpoint/endpoint sample).
    // The native factor is cached by exact time-rule parameters. An action
    // solves one supplied state and certifies the original law; it never solves
    // all basis vectors to manufacture a transition or inverse tensor.
    [[nodiscard]] PreparedPassiveStep prepare(const Rational& h,
                                                  PassiveIntegrationMethod method) const;
    [[nodiscard]] PassiveStepResult step(const RationalVector& x,
                                            const RationalVector& u,
                                            const Rational& h,
                                            PassiveIntegrationMethod method) const;
    [[nodiscard]] PassiveStepResult midpoint(const RationalVector& x,
                                                const Rational& h,
                                                const RationalVector& u = {}) const;
    [[nodiscard]] PassiveStepResult backwardEuler(const RationalVector& x,
                                                     const Rational& h,
                                                     const RationalVector& u = {}) const;

    template<std::floating_point T>
    PassiveStepResult step(const RationalVector&, const RationalVector&, T,
                              PassiveIntegrationMethod) const = delete;
    template<std::floating_point T>
    PassiveStepResult midpoint(const RationalVector&, T,
                                  const RationalVector& = {}) const = delete;
    template<std::floating_point T>
    PassiveStepResult backwardEuler(const RationalVector&, T,
                                       const RationalVector& = {}) const = delete;

    static constexpr std::string_view scope() noexcept {
        return "exact rational finite time rules for a declared passive lumped model; "
               "continuous-time trajectory and hardware fidelity require separate validation";
    }

private:
    struct State;
    std::shared_ptr<const State> state_;
    [[nodiscard]] const State& require_state() const;
    friend class PreparedPassiveStep;
};


namespace passive_dynamics_detail {

inline ExactMatrix combine(const ExactMatrix& a, const Rational& alpha,
                    const ExactMatrix& b, const Rational& beta, numeric::ExactActionGuard& guard) {
    if (a.rows() != b.rows() || a.cols() != b.cols())
        throw std::invalid_argument("electrical relation carrier mismatch");
    guard.matrix(a); guard.matrix(b); guard.value(alpha); guard.value(beta);
    ExactMatrix out(a.rows(), a.cols());
    for (const auto& [key, value] : a.entries()) {
        out.set(key.first, key.second, guard.mul(alpha, value));
        guard.entries(out.nonzeros());
    }
    for (const auto& [key, value] : b.entries()) {
        out.set(key.first, key.second,
                guard.add(out.get(key.first, key.second), guard.mul(beta, value)));
        guard.entries(out.nonzeros());
    }
    return out;
}
inline ExactMatrix scaled(const ExactMatrix& a, const Rational& alpha, numeric::ExactActionGuard& guard) {
    guard.matrix(a); guard.value(alpha);
    ExactMatrix out(a.rows(), a.cols());
    for (const auto& [key, value] : a.entries()) {
        out.set(key.first, key.second, guard.mul(alpha, value));
        guard.entries(out.nonzeros());
    }
    return out;
}
inline RationalVector combine(const RationalVector& a, const Rational& alpha,
                       const RationalVector& b, const Rational& beta, numeric::ExactActionGuard& guard) {
    if (a.size() != b.size()) throw std::invalid_argument("electrical field carrier mismatch");
    guard.vector(a); guard.vector(b); guard.value(alpha); guard.value(beta);
    RationalVector out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        out[i] = guard.add(guard.mul(alpha, a[i]), guard.mul(beta, b[i]));
    return out;
}
inline Rational inner(const RationalVector& a, const RationalVector& b, numeric::ExactActionGuard& guard) {
    if (a.size() != b.size()) throw std::invalid_argument("electrical pairing carrier mismatch");
    guard.vector(a); guard.vector(b);
    Rational out{0};
    for (std::size_t i = 0; i < a.size(); ++i)
        out = guard.add(out, guard.mul(a[i], b[i]));
    return out;
}
inline ExactMetric native_metric(const ExactMatrix& mass, numeric::ExactActionGuard& guard) {
    guard.matrix(mass);
    bool diagonal = true, identity = mass.nonzeros() == mass.rows();
    for (const auto& [key, value] : mass.entries()) {
        diagonal = diagonal && key.first == key.second;
        identity = identity && key.first == key.second && value == 1;
    }
    if (identity) return ExactMetric::identity(mass.rows());
    if (diagonal) {
        RationalVector values(mass.rows());
        for (const auto& [key, value] : mass.entries()) values[key.first] = value;
        return ExactMetric::diagonal(std::move(values));
    }
    return ExactMetric(mass,guard);
}
inline Rational energy_of(const ExactMatrix& mass, const ExactMetric& metric,
                   const RationalVector& x, numeric::ExactActionGuard& guard) {
    const auto mx = rfc::numeric::checked_apply(mass, x, guard);
    const Rational guarded = inner(x, mx, guard);
    const Rational native = metric.inner(x, x);
    guard.value(native);
    if (native != guarded) throw std::logic_error("native electrical metric pairing mismatch");
    return guard.div(native, Rational{2});
}
struct PreparedKernel {
    PassiveStepRelation law;
    PassiveIntegrationMethod method;
    Rational h, theta;
    std::optional<rfc::numeric::PreparedExactSolve> solve;
    std::size_t work{};
    PreparedKernel(const ExactMatrix& mass, const ExactMatrix& generator, const ExactMatrix& ports,
                   Rational duration, PassiveIntegrationMethod rule, numeric::ExactActionGuard& guard)
        : method(rule), h(std::move(duration)),
          theta(rule == PassiveIntegrationMethod::ImplicitMidpoint ? Rational{1,2} : Rational{1}) {
        const auto start = guard.steps();
        const Rational implicit = guard.mul(h, theta);
        const Rational explicit_part = guard.mul(h, guard.sub(Rational{1}, theta));
        law.left = combine(mass, Rational{1}, generator, -implicit, guard);
        law.previous = combine(mass, Rational{1}, generator, explicit_part, guard);
        law.input = scaled(ports, h, guard);
        solve.emplace(law.left, guard);
        work = guard.steps() - start;
    }
};
} // namespace

struct PassiveLinearDynamics::State {
    ExactMatrix M, J, R, B, generator;
    PassiveDynamicsLimits limits;
    std::optional<ExactMetric> metric;
    std::optional<numeric::PreparedPositiveSemidefinite> resistance_certificate;
    mutable std::mutex cache_mutex;
    mutable std::map<std::pair<int, Rational>, std::shared_ptr<const passive_dynamics_detail::PreparedKernel>> kernels;
    State(ExactMatrix mass, ExactMatrix structure, ExactMatrix resistance,
          ExactMatrix ports, PassiveDynamicsLimits requested)
        : M(std::move(mass)), J(std::move(structure)), R(std::move(resistance)),
          B(std::move(ports)), limits(std::move(requested)) {
        if (limits.max_state_dimension == 0)
            throw std::invalid_argument("electrical state dimension policy must be positive");
        const auto n = M.rows();
        if (n == 0 || M.cols() != n || J.rows() != n || J.cols() != n ||
            R.rows() != n || R.cols() != n || B.rows() != n)
            throw std::invalid_argument("electrical relation state dimensions do not agree");
        if (n > limits.max_state_dimension || B.cols() > limits.max_input_dimension)
            throw std::length_error("electrical declared carrier dimension budget");
        numeric::ExactActionGuard guard(limits.exact);
        guard.matrix(M); guard.matrix(J); guard.matrix(R); guard.matrix(B);
        metric.emplace(passive_dynamics_detail::native_metric(M, guard));
        resistance_certificate.emplace(R, guard);
        for (const auto& [key, value] : J.entries())
            if (guard.add(value, J.get(key.second, key.first)) != 0)
                throw std::invalid_argument("electrical J must be skew-symmetric");
        generator = passive_dynamics_detail::combine(J, Rational{1}, R, Rational{-1}, guard);
    }
};
struct PreparedPassiveStep::State {
    std::shared_ptr<const PassiveLinearDynamics::State> model;
    std::shared_ptr<const passive_dynamics_detail::PreparedKernel> kernel;
    State(std::shared_ptr<const PassiveLinearDynamics::State> m, std::shared_ptr<const passive_dynamics_detail::PreparedKernel> k)
        : model(std::move(m)), kernel(std::move(k)) {}
};
inline PassiveLinearDynamics::PassiveLinearDynamics(ExactMatrix M, ExactMatrix J,
                                                   ExactMatrix R, ExactMatrix B, PassiveDynamicsLimits limits)
    : state_(std::make_shared<const State>(std::move(M), std::move(J), std::move(R),
                                          std::move(B), std::move(limits))) {}
inline const PassiveLinearDynamics::State& PassiveLinearDynamics::require_state() const {
    if (!state_) throw std::logic_error("moved-from electrical dynamics owner");
    return *state_;
}
inline std::size_t PassiveLinearDynamics::state_dimension() const noexcept { return state_ ? state_->M.rows() : 0; }
inline std::size_t PassiveLinearDynamics::input_dimension() const noexcept { return state_ ? state_->B.cols() : 0; }
inline const ExactMatrix& PassiveLinearDynamics::mass_matrix() const { return require_state().M; }
inline const ExactMatrix& PassiveLinearDynamics::structure_matrix() const { return require_state().J; }
inline const ExactMatrix& PassiveLinearDynamics::resistance_matrix() const { return require_state().R; }
inline const ExactMatrix& PassiveLinearDynamics::input_matrix() const { return require_state().B; }
inline const ExactMetric& PassiveLinearDynamics::energy_metric() const { return *require_state().metric; }
inline Rational PassiveLinearDynamics::energy(const RationalVector& state) const {
    const auto& s = require_state();
    if (state.size() != s.M.rows()) throw std::invalid_argument("electrical energy state dimension");
    numeric::ExactActionGuard guard(s.limits.exact);
    return passive_dynamics_detail::energy_of(s.M, *s.metric, guard.canonical_input(state), guard);
}
inline PreparedPassiveStep PassiveLinearDynamics::prepare(const Rational& h, PassiveIntegrationMethod method) const {
    const auto& s = require_state();
    if (method != PassiveIntegrationMethod::ImplicitMidpoint && method != PassiveIntegrationMethod::BackwardEuler)
        throw std::invalid_argument("unknown electrical integration method");
    numeric::ExactActionGuard guard(s.limits.exact); guard.value(h);
    const Rational dt = rfc::numeric::canonical_rational(h);
    if (dt <= 0) throw std::invalid_argument("electrical time step must be positive");
    const std::pair<int, Rational> key{static_cast<int>(method), dt};
    std::lock_guard lock(s.cache_mutex);
    auto found = s.kernels.find(key);
    if (found == s.kernels.end()) {
        auto kernel = std::make_shared<const passive_dynamics_detail::PreparedKernel>(s.M, s.generator, s.B, dt, method, guard);
        found = s.kernels.emplace(key, std::move(kernel)).first;
    }
    return PreparedPassiveStep(std::make_shared<const PreparedPassiveStep::State>(state_, found->second));
}
inline const PassiveStepRelation& PreparedPassiveStep::relation() const {
    if (!state_) throw std::logic_error("moved-from prepared electrical step");
    return state_->kernel->law;
}
inline PassiveExecutionProfile PreparedPassiveStep::profile() const {
    if (!state_) throw std::logic_error("moved-from prepared electrical step");
    const auto& s = *state_->model; const auto& k = *state_->kernel;
    const auto stats = k.solve->preparation_stats();
    return {s.metric->kind(), s.M.rows(), s.B.cols(), s.M.nonzeros()+s.J.nonzeros()+s.R.nonzeros()+s.B.nonzeros(),
        k.law.left.nonzeros()+k.law.previous.nonzeros()+k.law.input.nonzeros(), k.solve->stored_entries(),
        k.work, stats.fill_insertions, stats.exact_cancellations, 0, 0};
}
inline PassiveStepResult PreparedPassiveStep::apply(const RationalVector& x, const RationalVector& u) const {
    if (!state_) throw std::logic_error("moved-from prepared electrical step");
    const auto& s = *state_->model; const auto& k = *state_->kernel;
    if (x.size() != s.M.rows() || u.size() != s.B.cols())
        throw std::invalid_argument("electrical state or input carrier dimension");
    numeric::ExactActionGuard guard(s.limits.exact);
    const auto old = guard.canonical_input(x), input = guard.canonical_input(u);
    const auto rhs = passive_dynamics_detail::combine(rfc::numeric::checked_apply(k.law.previous, old, guard), Rational{1},
                            rfc::numeric::checked_apply(k.law.input, input, guard), Rational{1}, guard);
    PassiveStepResult out;
    out.method = k.method; out.step_size = k.h; out.relation = k.law;
    out.execution = profile(); out.execution.state_solves = 1;
    out.xnext = k.solve->apply(rhs, guard);
    out.quadrature_state = passive_dynamics_detail::combine(old, guard.sub(Rational{1}, k.theta), out.xnext, k.theta, guard);
    const auto delta = passive_dynamics_detail::combine(out.xnext, Rational{1}, old, Rational{-1}, guard);
    const auto port = rfc::numeric::checked_apply(s.B, input, guard);
    const auto dynamic = rfc::numeric::checked_apply(s.generator, out.quadrature_state, guard);
    const auto forcing = passive_dynamics_detail::combine(dynamic, Rational{1}, port, Rational{1}, guard);
    out.balance_residual = passive_dynamics_detail::combine(rfc::numeric::checked_apply(s.M, delta, guard), Rational{1}, forcing, -k.h, guard);
    out.equation_residual = passive_dynamics_detail::combine(rfc::numeric::checked_apply(k.law.left, out.xnext, guard),
                                    Rational{1}, rhs, Rational{-1}, guard);
    out.energy_before = passive_dynamics_detail::energy_of(s.M, *s.metric, old, guard);
    out.energy_after = passive_dynamics_detail::energy_of(s.M, *s.metric, out.xnext, guard);
    out.source_work = guard.mul(k.h, passive_dynamics_detail::inner(out.quadrature_state, port, guard));
    out.resistance_loss = guard.mul(k.h, s.resistance_certificate->quadratic_form(out.quadrature_state, guard));
    out.step_dissipation = k.method == PassiveIntegrationMethod::BackwardEuler
        ? passive_dynamics_detail::energy_of(s.M, *s.metric, delta, guard) : Rational{0};
    out.energy_balance_residual = guard.sub(
        guard.sub(guard.sub(out.source_work, guard.sub(out.energy_after, out.energy_before)),
                  out.resistance_loss), out.step_dissipation);
    out.exact = rfc::numeric::is_zero(out.equation_residual) && rfc::numeric::is_zero(out.balance_residual)
        && out.energy_balance_residual == 0 && out.resistance_loss >= 0 && out.step_dissipation >= 0;
    if (!out.exact) throw std::logic_error("electrical exact step certificate failed");
    out.checked_work = guard.steps();
    return out;
}
inline PassiveStepResult PassiveLinearDynamics::step(const RationalVector& x, const RationalVector& u,
                                                    const Rational& h, PassiveIntegrationMethod method) const {
    return prepare(h, method).apply(x, u);
}
inline PassiveStepResult PassiveLinearDynamics::midpoint(const RationalVector& x, const Rational& h,
                                                        const RationalVector& u) const {
    return step(x, u, h, PassiveIntegrationMethod::ImplicitMidpoint);
}
inline PassiveStepResult PassiveLinearDynamics::backwardEuler(const RationalVector& x, const Rational& h,
                                                             const RationalVector& u) const {
    return step(x, u, h, PassiveIntegrationMethod::BackwardEuler);
}
} // namespace rfc::field
