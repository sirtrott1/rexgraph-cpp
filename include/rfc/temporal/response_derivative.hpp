#pragma once
#include "rfc/field/factored_action.hpp"
#include "rfc/temporal/covariant_family.hpp"

namespace rfc::temporal {
namespace response_detail {
using namespace covariant_detail;
using Contexts = std::vector<PreparedCovariantJet>;
inline void append(Contexts &out, const PreparedCovariantJet &jet) {
    if (std::none_of(out.begin(), out.end(), [&](const auto &j) { return j.same_owner(jet); }))
        out.push_back(jet);
}
inline void append(Contexts &out, const Contexts &more) {
    for (const auto &j : more)
        append(out, j);
}
inline std::size_t retained(const Contexts &contexts, std::size_t local) {
    for (const auto &j : contexts)
        local = add(local, j.retained_allowance());
    return local;
}
inline void validate(const Contexts &contexts, Guard &g) {
    for (const auto &j : contexts)
        j.validate_receiving(g);
}
inline V scale(V v, const Q &q, Guard &g) {
    g.vector(v);
    g.value(q);
    for (auto &a : v)
        a = g.mul(a, q);
    return v;
}
inline M scale(const M &a, const Q &q, Guard &g) {
    g.matrix(a);
    g.value(q);
    M b(a.rows(), a.cols());
    for (const auto &[ij, v] : a.entries())
        b.set(ij.first, ij.second, g.mul(v, q));
    return b;
}
inline M column(const V &v, Guard &g) {
    g.vector(v);
    M a(v.size(), 1);
    for (std::size_t i = 0; i < v.size(); ++i) {
        g.step();
        a.set(i, 0, v[i]);
    }
    return a;
}
inline V first_column(const M &a, std::size_t n, Guard &g) {
    g.matrix(a);
    if (a.cols() != 1 || n > a.rows())
        throw std::invalid_argument("response column shape");
    V v(n);
    for (std::size_t i = 0; i < n; ++i) {
        g.step();
        v[i] = a.get(i, 0);
    }
    return v;
}
inline V row_replay(const numeric::IntegerUnimodularAction &rows, V x, bool transpose, Guard &g) {
    if (x.size() != rows.dimension())
        throw std::invalid_argument("response row action dimension");
    g.vector(x);
    const auto perform = [&](const auto &op) {
        g.step();
        g.value(Q{op.multiple});
        using Kind = numeric::IntegerUnimodularAction::Kind;
        if (op.kind == Kind::Swap)
            std::swap(x[op.source], x[op.target]);
        else if (op.kind == Kind::Negate)
            x[op.target] = g.sub(Q{0}, x[op.target]);
        else {
            const auto target = transpose ? op.source : op.target, source = transpose ? op.target : op.source;
            x[target] = g.add(x[target], g.mul(Q{op.multiple}, x[source]));
        }
    };
    if (transpose)
        for (auto i = rows.operations().rbegin(); i != rows.operations().rend(); ++i)
            perform(*i);
    else
        for (const auto &op : rows.operations())
            perform(op);
    return x;
}
inline numeric::SparseRationalVector row_replay(const numeric::IntegerUnimodularAction &rows,
                                                const numeric::SparseRationalVector &x, bool transpose,
                                                Guard &g) {
    if (x.size() != rows.dimension())
        throw std::invalid_argument("response sparse row action dimension");
    using namespace numeric::checked_sparse_detail;
    auto values = coordinates(x, g);
    const auto perform = [&](const auto &op) {
        g.step();
        g.value(Q{op.multiple});
        using Kind = numeric::IntegerUnimodularAction::Kind;
        if (op.kind == Kind::Swap) {
            const auto a = get(values, op.source), b = get(values, op.target);
            set(values, op.target, a, g, rows.operations().size());
            set(values, op.source, b, g, rows.operations().size());
        } else if (op.kind == Kind::Negate)
            set(values, op.target, g.sub(Q{0}, get(values, op.target)), g, rows.operations().size());
        else {
            const auto target = transpose ? op.source : op.target, source = transpose ? op.target : op.source;
            set(values, target, g.add(get(values, target), g.mul(Q{op.multiple}, get(values, source))), g,
                rows.operations().size());
        }
    };
    if (transpose)
        for (auto i = rows.operations().rbegin(); i != rows.operations().rend(); ++i)
            perform(*i);
    else
        for (const auto &op : rows.operations())
            perform(op);
    return vector(x.size(), std::move(values), g);
}
inline void endpoint(const PreparedCovariantJet &a, std::size_t i, const PreparedCovariantJet &b,
                     std::size_t j) {
    if (!a.same_owner(b) || i != j)
        throw std::invalid_argument("derivative action actual jet endpoint differs");
}
inline std::size_t family_extra(const CovariantMapFamily &f) {
    std::size_t n = f.rank_scope().work().retained_coefficient_allowance;
    for (const auto &e : f.declarations())
        n = add(n, mul(8, add(e.original_numerator().size(), e.original_denominator().size())));
    return n;
}
inline void validate_family(const CovariantMapFamily &f, const OwnedJetPoint &point, Guard &g) {
    f.source().validate_receiving(g);
    f.target().validate_receiving(g);
    for (const auto &e : f.declarations()) {
        g.vector(e.original_numerator());
        g.vector(e.original_denominator());
    }
    g.vector(f.rank_scope().root_polynomial());
    (void)covariant_family_detail::regular_cell(f.rank_scope(), point, g, f.source().limits());
}
} // namespace response_detail
class PreparedResponseJet;
class PreparedDerivativeWord;
class SourceCellJetSelectors;
// A first-order field with an actual source/basis/direction owner. Derived
// fields retain the original solve/word and all of their receiving dependencies.
class OwnedDerivativeField {
    using V = response_detail::V;
    using Guard = response_detail::Guard;
    struct State {
        PreparedCovariantJet context;
        std::size_t grade;
        V value, dot;
        response_detail::Contexts contexts;
        std::size_t local, depth;
        std::shared_ptr<const void> origin;
        std::function<void(Guard &)> validate_origin;
    };
    friend class PreparedResponseJet;
    friend class PreparedDerivativeWord;
    friend class SourceCellJetSelectors;
    static OwnedDerivativeField derived(PreparedCovariantJet context, std::size_t grade, V value, V dot,
                                        response_detail::Contexts contexts, std::size_t local,
                                        std::shared_ptr<const void> origin,
                                        std::function<void(Guard &)> validate, std::size_t depth, Guard &g) {
        using namespace response_detail;
        if (depth > 128)
            throw std::length_error("derived field dependency depth budget");
        append(contexts, context);
        g.entries(retained(contexts, local));
        g.vector(value);
        g.vector(dot);
        if (value.size() != context.base().metrics().at(grade).rows() || dot.size() != value.size())
            throw std::invalid_argument("owned derivative field carrier");
        g.step();
        return OwnedDerivativeField(std::make_shared<const State>(
            State{std::move(context), grade, std::move(value), std::move(dot), std::move(contexts), local,
                  depth, std::move(origin), std::move(validate)}));
    }

  public:
    static OwnedDerivativeField declare(PreparedCovariantJet context, std::size_t grade, V value, V dot,
                                        Guard &g) {
        context.validate_receiving(g);
        const auto local = response_detail::mul(8, response_detail::add(value.size(), 1));
        return derived(std::move(context), grade, std::move(value), std::move(dot), {}, local, {}, {}, 0, g);
    }
    template <std::floating_point T>
    static OwnedDerivativeField declare(PreparedCovariantJet, T, V, V, Guard &) = delete;
    static OwnedDerivativeField source(PreparedCovariantJet context, std::size_t grade, Guard &g) {
        auto x = context.base().fields().at(grade), d = context.input().field_derivatives.at(grade);
        return declare(std::move(context), grade, std::move(x), std::move(d), g);
    }
    template <std::floating_point T>
    static OwnedDerivativeField source(PreparedCovariantJet, T, Guard &) = delete;
    const PreparedCovariantJet &context() const & noexcept {
        return state_->context;
    }
    const PreparedCovariantJet &context() const && = delete;
    std::size_t grade() const noexcept {
        return state_->grade;
    }
    const V &value() const & noexcept {
        return state_->value;
    }
    const V &value() const && = delete;
    const V &dot() const & noexcept {
        return state_->dot;
    }
    const V &dot() const && = delete;
    const response_detail::Contexts &contexts() const & noexcept {
        return state_->contexts;
    }
    const response_detail::Contexts &contexts() const && = delete;
    std::size_t local_allowance() const noexcept {
        return state_->local;
    }
    std::size_t dependency_depth() const noexcept {
        return state_->depth;
    }
    std::size_t retained_allowance() const {
        return response_detail::retained(contexts(), local_allowance());
    }
    bool same_owner(const OwnedDerivativeField &f) const noexcept {
        return state_ == f.state_;
    }
    void validate_receiving(Guard &g) const {
        g.entries(retained_allowance());
        response_detail::validate(contexts(), g);
        g.vector(value());
        g.vector(dot());
        if (state_->validate_origin)
            state_->validate_origin(g);
    }

  private:
    explicit OwnedDerivativeField(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
// Typed exact action/differential pairs. Native FactoredAction owns the original
// factors and algebraic transposes; no composite matrix or inverse is required.
class DerivativeAction {
    using M = response_detail::M;
    using V = response_detail::V;
    using Guard = response_detail::Guard;
    struct State {
        PreparedCovariantJet input, output;
        std::size_t input_grade, output_grade;
        field::FactoredAction value, dot;
        response_detail::Contexts contexts;
        std::size_t local;
        std::shared_ptr<const void> origin;
        std::function<void(Guard &)> validate_origin;
    };
    friend class PreparedResponseJet;
    static DerivativeAction make(PreparedCovariantJet input, std::size_t ig, PreparedCovariantJet output,
                                 std::size_t og, field::FactoredAction value, field::FactoredAction dot,
                                 std::size_t local, std::shared_ptr<const void> origin,
                                 std::function<void(Guard &)> validate, Guard &g) {
        using namespace response_detail;
        if (!input.base().point().same_owner(output.base().point()) ||
            !input.direction().same_owner(output.direction()))
            throw std::invalid_argument("derivative action point/direction differs");
        Contexts contexts;
        append(contexts, input);
        append(contexts, output);
        g.entries(retained(contexts, local));
        if (value.cols() != input.base().metrics().at(ig).rows() ||
            value.rows() != output.base().metrics().at(og).rows() || dot.cols() != value.cols() ||
            dot.rows() != value.rows())
            throw std::invalid_argument("derivative action declared ports");
        g.step();
        return DerivativeAction(std::make_shared<const State>(
            State{std::move(input), std::move(output), ig, og, std::move(value), std::move(dot),
                  std::move(contexts), local, std::move(origin), std::move(validate)}));
    }

  public:
    static DerivativeAction map(RectangularCovariantJet map, Guard &g) {
        using namespace response_detail;
        auto pin = std::make_shared<const RectangularCovariantJet>(std::move(map));
        pin->source().validate_receiving(g);
        pin->target().validate_receiving(g);
        const auto local = mul(64, add(add(pin->value().nonzeros(), pin->dot().nonzeros()), 1));
        return make(
            pin->source(), pin->source_grade(), pin->target(), pin->target_grade(),
            field::FactoredAction::matrix(pin->value()), field::FactoredAction::matrix(pin->dot()), local,
            pin,
            [pin](Guard &receive) {
                receive.matrix(pin->value());
                receive.matrix(pin->dot());
            },
            g);
    }
    static DerivativeAction family(CovariantMapFamily family, const PreparedCovariantJet &input,
                                   const PreparedCovariantJet &output, Guard &g) {
        auto action = map(family.jet(input, output, g), g);
        auto next = std::make_shared<State>(*action.state_);
        auto pin = std::make_shared<const CovariantMapFamily>(std::move(family));
        next->local = response_detail::add(next->local, response_detail::family_extra(*pin));
        auto old = next->validate_origin;
        next->origin = pin;
        next->validate_origin = [old, pin, point = input.base().point()](Guard &receive) {
            old(receive);
            response_detail::validate_family(*pin, point, receive);
        };
        g.entries(response_detail::retained(next->contexts, next->local));
        return DerivativeAction(std::move(next));
    }
    static DerivativeAction hodge(PreparedCovariantJet context, std::size_t grade, Guard &g) {
        auto a = context.hodge().at(grade), d = context.hodge_dots().at(grade);
        return map(
            RectangularCovariantJet::declare(context, grade, context, grade, std::move(a), std::move(d), g),
            g);
    }
    template <std::floating_point T> static DerivativeAction hodge(PreparedCovariantJet, T, Guard &) = delete;
    const PreparedCovariantJet &input() const & noexcept {
        return state_->input;
    }
    const PreparedCovariantJet &input() const && = delete;
    const PreparedCovariantJet &output() const & noexcept {
        return state_->output;
    }
    const PreparedCovariantJet &output() const && = delete;
    std::size_t input_grade() const noexcept {
        return state_->input_grade;
    }
    std::size_t output_grade() const noexcept {
        return state_->output_grade;
    }
    const field::FactoredAction &value_action() const & noexcept {
        return state_->value;
    }
    const field::FactoredAction &value_action() const && = delete;
    const field::FactoredAction &dot_action() const & noexcept {
        return state_->dot;
    }
    const field::FactoredAction &dot_action() const && = delete;
    const response_detail::Contexts &contexts() const & noexcept {
        return state_->contexts;
    }
    const response_detail::Contexts &contexts() const && = delete;
    std::size_t local_allowance() const noexcept {
        return state_->local;
    }
    bool same_owner(const DerivativeAction &a) const noexcept {
        return state_ == a.state_;
    }
    void validate_receiving(Guard &g) const {
        g.entries(response_detail::retained(contexts(), local_allowance()));
        response_detail::validate(contexts(), g);
        if (state_->validate_origin)
            state_->validate_origin(g);
    }
    V apply(const V &x, Guard &g) const {
        validate_receiving(g);
        return value_action().apply(x, g);
    }
    V apply_dot(const V &x, Guard &g) const {
        validate_receiving(g);
        return dot_action().apply(x, g);
    }
    V apply_covariant(const V &x, Guard &g) const {
        using namespace response_detail;
        validate_receiving(g);
        const auto source = covariant_detail::apply(input().input().connections[input_grade()], x, g);
        auto y = sum(dot_action().apply(x, g), value_action().apply(source, g), g);
        return sum(std::move(y),
                   covariant_detail::apply(output().input().connections[output_grade()],
                                           value_action().apply(x, g), g),
                   g, true);
    }

  private:
    explicit DerivativeAction(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
class PreparedResponseJet {
    using Q = response_detail::Q;
    using M = response_detail::M;
    using V = response_detail::V;
    using Guard = response_detail::Guard;
    struct State {
        RectangularCovariantJet equation;
        M kernel, kernel_dot, gauge, gauge_dot, stack;
        numeric::IntegerUnimodularAction rows;
        numeric::PreparedExactSolve factor;
        response_detail::Contexts contexts;
        std::size_t local, rank;
        std::shared_ptr<const void> declaration;
        std::function<void(Guard &)> validate_declaration;
    };
    struct ResultState {
        std::shared_ptr<const State> owner;
        OwnedDerivativeField rhs;
        V value, dot, source_driver, operator_driver, gauge_driver;
    };
    static void validate(const State &s, Guard &g) {
        using namespace response_detail;
        g.entries(retained(s.contexts, s.local));
        response_detail::validate(s.contexts, g);
        g.matrix(s.equation.value());
        g.matrix(s.equation.dot());
        g.matrix(s.kernel);
        g.matrix(s.kernel_dot);
        g.matrix(s.gauge);
        g.matrix(s.gauge_dot);
        g.matrix(s.stack);
        for (const auto &op : s.rows.operations()) {
            g.step();
            g.value(Q{op.multiple});
        }
        g.matrix(s.factor.matrix());
        if (s.validate_declaration)
            s.validate_declaration(g);
    }
    static V solve(const State &s, const V &b, const V &gauge, Guard &g) {
        using namespace response_detail;
        V all = b;
        all.insert(all.end(), gauge.begin(), gauge.end());
        const auto transformed = s.rows.apply_rational(column(all, g), g);
        return s.factor.apply(first_column(transformed, s.equation.value().cols(), g), g);
    }
    static OwnedDerivativeField field(const std::shared_ptr<const ResultState> &s, Guard &g) {
        using namespace response_detail;
        auto contexts = s->rhs.contexts();
        append(contexts, s->owner->contexts);
        const auto local =
            add(add(s->rhs.local_allowance(), s->owner->local), mul(32, add(s->value.size(), 1)));
        return OwnedDerivativeField::derived(
            s->owner->equation.source(), s->owner->equation.source_grade(), s->value, s->dot,
            std::move(contexts), local, s,
            [s](Guard &receive) {
                validate(*s->owner, receive);
                s->rhs.validate_receiving(receive);
                receive.vector(s->value);
                receive.vector(s->dot);
            },
            response_detail::add(s->rhs.dependency_depth(), 1), g);
    }

  public:
    class Result {
        friend class PreparedResponseJet;
        std::shared_ptr<const ResultState> state_;
        explicit Result(std::shared_ptr<const ResultState> s) : state_(std::move(s)) {}

      public:
        const V &value() const & noexcept {
            return state_->value;
        }
        const V &value() const && = delete;
        const V &dot() const & noexcept {
            return state_->dot;
        }
        const V &dot() const && = delete;
        const V &source_driver() const & noexcept {
            return state_->source_driver;
        }
        const V &source_driver() const && = delete;
        const V &operator_driver() const & noexcept {
            return state_->operator_driver;
        }
        const V &operator_driver() const && = delete;
        const V &gauge_driver() const & noexcept {
            return state_->gauge_driver;
        }
        const V &gauge_driver() const && = delete;
        OwnedDerivativeField field(Guard &g) const {
            return PreparedResponseJet::field(state_, g);
        }
    };
    static PreparedResponseJet prepare(RectangularCovariantJet equation, Guard &g) {
        const auto n = equation.value().cols();
        return gauged(std::move(equation), M(n, 0), M(n, 0), g);
    }
    static PreparedResponseJet gauged(RectangularCovariantJet equation, M kernel, M kernel_dot,
                                      Guard &guard) {
        using namespace response_detail;
        guard.step();
        Contexts contexts;
        append(contexts, equation.source());
        append(contexts, equation.target());
        const auto m = equation.value().rows(), n = equation.value().cols(), q = kernel.cols(),
                   rows = add(m, q);
        guard.dimension(rows);
        const auto local = mul(64, mul(add(add(rows, n), 1), add(add(rows, n), 1)));
        auto g = phase(guard, retained(contexts, local));
        response_detail::validate(contexts, guard);
        shape(kernel, n, q, g);
        shape(kernel_dot, n, q, g);
        g.matrix(equation.value());
        g.matrix(equation.dot());
        const auto r = covariant_detail::rank(equation.value(), g);
        if (covariant_detail::rank(kernel, g) != q || add(r, q) != n ||
            product(equation.value(), kernel, g).nonzeros() ||
            sum(product(equation.dot(), kernel, g), product(equation.value(), kernel_dot, g), g).nonzeros())
            throw std::invalid_argument("response requires the complete original kernel and its tangent");
        const auto &metric = equation.source().base().metrics()[equation.source_grade()];
        const auto &metric_dot = equation.source().input().metric_derivatives[equation.source_grade()];
        auto c = product(kernel.transpose(), metric, g);
        auto cd =
            sum(product(kernel_dot.transpose(), metric, g), product(kernel.transpose(), metric_dot, g), g);
        M stack(rows, n);
        for (const auto &[ij, v] : equation.value().entries())
            stack.set(ij.first, ij.second, v);
        for (const auto &[ij, v] : c.entries())
            stack.set(m + ij.first, ij.second, v);
        numeric::Integer denominator{1};
        for (const auto &[ij, v] : stack.entries()) {
            (void)ij;
            g.step();
            mpz_lcm(denominator.get_mpz_t(), denominator.get_mpz_t(), v.get_den_mpz_t());
            g.value(Q{denominator});
        }
        numeric::IntegerMatrix z(rows, n);
        for (const auto &[ij, v] : stack.entries()) {
            const auto a = g.mul(v, Q{denominator});
            z.set(ij.first, ij.second, a.get_num());
        }
        auto left = [&] {
            auto certificate = numeric::smith_normal_form_actions(z, g);
            if (certificate.rank != n)
                throw std::invalid_argument("response gauge does not determine a unique field");
            return certificate.left;
        }();
        const auto transformed = left.apply_rational(stack, g);
        M reduced(n, n);
        for (const auto &[ij, v] : transformed.entries())
            if (ij.first < n)
                reduced.set(ij.first, ij.second, v);
        auto factor_guard =
            phase(g, add(left.operations().size(), mul(4, add(transformed.nonzeros(), reduced.nonzeros()))));
        numeric::PreparedExactSolve factor(reduced, factor_guard);
        const auto stored = add(local, add(left.operations().size(),
                                           add(factor.stored_entries(), factor.stored_index_entries())));
        guard.entries(retained(contexts, stored));
        guard.step();
        return PreparedResponseJet(std::make_shared<const State>(State{std::move(equation),
                                                                       std::move(kernel),
                                                                       std::move(kernel_dot),
                                                                       std::move(c),
                                                                       std::move(cd),
                                                                       std::move(stack),
                                                                       std::move(left),
                                                                       std::move(factor),
                                                                       std::move(contexts),
                                                                       stored,
                                                                       r,
                                                                       {},
                                                                       {}}));
    }
    static PreparedResponseJet family(CovariantMapFamily family, const PreparedCovariantJet &unknown,
                                      const PreparedCovariantJet &equations, M kernel, M kernel_dot,
                                      Guard &g) {
        auto p = gauged(family.jet(unknown, equations, g), std::move(kernel), std::move(kernel_dot), g);
        auto next = std::make_shared<State>(*p.state_);
        auto pin = std::make_shared<const CovariantMapFamily>(std::move(family));
        next->local = response_detail::add(next->local, response_detail::family_extra(*pin));
        next->declaration = pin;
        next->validate_declaration = [pin, point = unknown.base().point()](Guard &receive) {
            response_detail::validate_family(*pin, point, receive);
        };
        g.entries(response_detail::retained(next->contexts, next->local));
        return PreparedResponseJet(std::move(next));
    }
    static PreparedResponseJet family(CovariantMapFamily family, const PreparedCovariantJet &unknown,
                                      const PreparedCovariantJet &equations, Guard &g) {
        const auto n = family.formal().cols;
        return PreparedResponseJet::family(std::move(family), unknown, equations, M(n, 0), M(n, 0), g);
    }
    static PreparedResponseJet green(PreparedCovariantJet context, std::size_t grade, Q lambda, Q lambda_dot,
                                     Guard &g) {
        using namespace response_detail;
        g.value(lambda);
        g.value(lambda_dot);
        const auto &l = context.hodge().at(grade);
        const auto a = sum(M::identity(l.rows()), scale(l, lambda, g), g);
        const auto dot = sum(scale(l, lambda_dot, g), scale(context.hodge_dots()[grade], lambda, g), g);
        auto p = prepare(RectangularCovariantJet::declare(context, grade, context, grade, a, dot, g), g);
        auto next = std::make_shared<State>(*p.state_);
        next->local = add(next->local, 2);
        next->validate_declaration = [lambda, lambda_dot](Guard &receive) {
            receive.value(lambda);
            receive.value(lambda_dot);
        };
        return PreparedResponseJet(std::move(next));
    }
    template <class A, class B, class D>
        requires(std::floating_point<A> || std::floating_point<B> || std::floating_point<D>)
    static PreparedResponseJet green(PreparedCovariantJet, A, B, D, Guard &) = delete;
    static PreparedResponseJet green_expression(PreparedCovariantJet context, std::size_t grade,
                                                geometry::OwnedRationalExpression lambda, Guard &g) {
        using namespace response_detail;
        if (!context.base().point().domain().same_owner(lambda.domain()) ||
            !context.direction().same_owner(JetDirection::coordinate(lambda.domain().coordinate())))
            throw std::invalid_argument("Green scalar coordinate/direction owner differs");
        const auto at = lambda.domain().at(context.base().point().value(), g);
        const auto value = lambda.evaluate(at, g);
        covariant_family_detail::Work w(g, {});
        const auto dot = w.evaluate(w.derivative(lambda.formal()), at.value());
        auto p = green(context, grade, value, dot, g);
        auto next = std::make_shared<State>(*p.state_);
        auto pin = std::make_shared<const geometry::OwnedRationalExpression>(std::move(lambda));
        auto old = next->validate_declaration;
        next->local = add(next->local,
                          mul(8, add(pin->original_numerator().size(), pin->original_denominator().size())));
        next->declaration = pin;
        next->validate_declaration = [old, pin, point = context.base().point()](Guard &receive) {
            old(receive);
            receive.vector(pin->original_numerator());
            receive.vector(pin->original_denominator());
            (void)pin->evaluate(pin->domain().at(point.value(), receive), receive);
        };
        g.entries(retained(next->contexts, next->local));
        return PreparedResponseJet(std::move(next));
    }
    template <std::floating_point T>
    static PreparedResponseJet green_expression(PreparedCovariantJet, T, geometry::OwnedRationalExpression,
                                                Guard &) = delete;
    const RectangularCovariantJet &equation() const & noexcept {
        return state_->equation;
    }
    const RectangularCovariantJet &equation() const && = delete;
    const M &kernel() const & noexcept {
        return state_->kernel;
    }
    const M &kernel() const && = delete;
    const M &kernel_dot() const & noexcept {
        return state_->kernel_dot;
    }
    const M &kernel_dot() const && = delete;
    const M &gauge() const & noexcept {
        return state_->gauge;
    }
    const M &gauge() const && = delete;
    const M &gauge_dot() const & noexcept {
        return state_->gauge_dot;
    }
    const M &gauge_dot() const && = delete;
    const numeric::PreparedExactSolve &factor() const & noexcept {
        return state_->factor;
    }
    const numeric::PreparedExactSolve &factor() const && = delete;
    const numeric::IntegerUnimodularAction &row_action() const & noexcept {
        return state_->rows;
    }
    const numeric::IntegerUnimodularAction &row_action() const && = delete;
    std::size_t rank() const noexcept {
        return state_->rank;
    }
    std::size_t local_allowance() const noexcept {
        return state_->local;
    }
    const response_detail::Contexts &contexts() const & noexcept {
        return state_->contexts;
    }
    const response_detail::Contexts &contexts() const && = delete;
    void validate_receiving(Guard &g) const {
        validate(*state_, g);
    }
    Result apply(const OwnedDerivativeField &rhs, Guard &guard) const {
        using namespace response_detail;
        endpoint(rhs.context(), rhs.grade(), equation().target(), equation().target_grade());
        auto contexts = state_->contexts;
        append(contexts, rhs.contexts());
        const auto local =
            add(add(state_->local, rhs.local_allowance()), mul(32, add(equation().value().cols(), 1)));
        auto g = phase(guard, retained(contexts, local));
        validate_receiving(guard);
        rhs.validate_receiving(guard);
        auto x = solve(*state_, rhs.value(), V(gauge().rows()), g);
        auto operator_driver = scale(covariant_detail::apply(equation().dot(), x, g), Q{-1}, g);
        auto gauge_driver = scale(covariant_detail::apply(gauge_dot(), x, g), Q{-1}, g);
        auto driver = sum(rhs.dot(), operator_driver, g);
        auto dot = solve(*state_, driver, gauge_driver, g);
        if (covariant_detail::apply(equation().value(), x, g) != rhs.value() ||
            covariant_detail::apply(gauge(), x, g) != V(gauge().rows()) ||
            sum(covariant_detail::apply(equation().value(), dot, g),
                covariant_detail::apply(equation().dot(), x, g), g) != rhs.dot() ||
            sum(covariant_detail::apply(gauge(), dot, g), covariant_detail::apply(gauge_dot(), x, g), g) !=
                V(gauge().rows()))
            throw std::invalid_argument("original response or differentiated gauge residual is nonzero");
        guard.step();
        return Result(std::make_shared<const ResultState>(
            ResultState{state_, rhs, std::move(x), std::move(dot), rhs.dot(), std::move(operator_driver),
                        std::move(gauge_driver)}));
    }
    DerivativeAction inverse_action(Guard &g) const {
        using namespace response_detail;
        validate_receiving(g);
        if (equation().value().rows() != equation().value().cols() || kernel().cols())
            throw std::invalid_argument(
                "a partial gauged/rectangular solve is not a total inverse word action");
        // The row action is an execution change of equations; solve A directly
        // through the same saved reduction, with U applied before its factor.
        auto pin = state_;
        auto forward = field::FactoredAction::owned(
            pin, equation().value().cols(), equation().value().rows(), state_->local,
            [](const auto &s, const auto &x, auto &guard) {
                validate(*s, guard);
                return s->factor.apply(response_detail::row_replay(s->rows, x, false, guard), guard);
            },
            [](const auto &s, const auto &x, auto &guard) {
                validate(*s, guard);
                return response_detail::row_replay(s->rows, s->factor.apply_transpose(x, guard), true, guard);
            });
        auto dot = forward.then(field::FactoredAction::matrix(equation().dot())).then(forward).scaled(Q{-1});
        const auto local = add(state_->local, add(forward.retained_entries(), dot.retained_entries()));
        return DerivativeAction::make(
            equation().target(), equation().target_grade(), equation().source(), equation().source_grade(),
            std::move(forward), std::move(dot), local, pin,
            [pin](Guard &receive) { validate(*pin, receive); }, g);
    }

  private:
    explicit PreparedResponseJet(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::temporal
