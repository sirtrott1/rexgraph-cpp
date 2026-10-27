#pragma once
#include "rfc/temporal/response_derivative.hpp"

namespace rfc::temporal {
class PreparedDerivativeWord {
    using V = response_detail::V;
    using Guard = response_detail::Guard;
    struct State {
        PreparedCovariantJet input, output;
        std::size_t input_grade, output_grade;
        std::vector<DerivativeAction> factors;
        response_detail::Contexts contexts;
        std::size_t local, max_dimension;
    };
    struct ResultState {
        std::shared_ptr<const State> owner;
        OwnedDerivativeField source;
        V value, dot, input_driver, covariant_input;
        std::vector<V> positions, covariant_positions;
    };
    static void validate(const State &s, Guard &g) {
        g.entries(response_detail::retained(s.contexts, s.local));
        response_detail::validate(s.contexts, g);
        for (const auto &factor : s.factors)
            factor.validate_receiving(g);
    }
    static OwnedDerivativeField field(const std::shared_ptr<const ResultState> &s, Guard &g) {
        using namespace response_detail;
        auto contexts = s->source.contexts();
        append(contexts, s->owner->contexts);
        const auto local = add(add(s->source.local_allowance(), s->owner->local),
                               mul(16, mul(add(s->positions.size(), 2), add(s->value.size(), 1))));
        return OwnedDerivativeField::derived(
            s->owner->output, s->owner->output_grade, s->value, s->dot, std::move(contexts), local, s,
            [s](Guard &receive) {
                validate(*s->owner, receive);
                s->source.validate_receiving(receive);
                receive.vector(s->value);
                receive.vector(s->dot);
                for (const auto &v : s->positions)
                    receive.vector(v);
                for (const auto &v : s->covariant_positions)
                    receive.vector(v);
            },
            response_detail::add(s->source.dependency_depth(), 1), g);
    }

  public:
    class Result {
        friend class PreparedDerivativeWord;
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
        const V &input_driver() const & noexcept {
            return state_->input_driver;
        }
        const V &input_driver() const && = delete;
        const V &covariant_input() const & noexcept {
            return state_->covariant_input;
        }
        const V &covariant_input() const && = delete;
        const std::vector<V> &positions() const & noexcept {
            return state_->positions;
        }
        const std::vector<V> &positions() const && = delete;
        const std::vector<V> &covariant_positions() const & noexcept {
            return state_->covariant_positions;
        }
        const std::vector<V> &covariant_positions() const && = delete;
        OwnedDerivativeField field(Guard &g) const {
            return PreparedDerivativeWord::field(state_, g);
        }
    };
    static PreparedDerivativeWord prepare(PreparedCovariantJet input, std::size_t grade,
                                          std::vector<DerivativeAction> factors, Guard &g,
                                          std::size_t max_positions = 128) {
        using namespace response_detail;
        g.step();
        if (factors.size() > max_positions)
            throw std::length_error("derivative word position budget");
        g.dimension(factors.size());
        auto output = input;
        auto og = grade;
        Contexts contexts;
        append(contexts, input);
        std::size_t local = mul(16, add(factors.size(), 1)),
                    dimension = input.base().metrics().at(grade).rows();
        std::vector<DerivativeAction> unique;
        for (const auto &factor : factors) {
            endpoint(output, og, factor.input(), factor.input_grade());
            append(contexts, factor.contexts());
            output = factor.output();
            og = factor.output_grade();
            dimension = std::max(dimension, output.base().metrics().at(og).rows());
            if (std::none_of(unique.begin(), unique.end(),
                             [&](const auto &f) { return f.same_owner(factor); })) {
                unique.push_back(factor);
                local = add(local, factor.local_allowance());
            }
        }
        g.entries(retained(contexts, local));
        response_detail::validate(contexts, g);
        for (const auto &factor : factors)
            factor.validate_receiving(g);
        g.step();
        return PreparedDerivativeWord(
            std::make_shared<const State>(State{std::move(input), std::move(output), grade, og,
                                                std::move(factors), std::move(contexts), local, dimension}));
    }
    template <class A, class B = std::size_t>
        requires(std::floating_point<A> || std::floating_point<B>)
    static PreparedDerivativeWord prepare(PreparedCovariantJet, A, std::vector<DerivativeAction>, Guard &,
                                          B = 128) = delete;
    const std::vector<DerivativeAction> &factors() const & noexcept {
        return state_->factors;
    }
    const std::vector<DerivativeAction> &factors() const && = delete;
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
    void validate_receiving(Guard &g) const {
        validate(*state_, g);
    }
    Result execute(const OwnedDerivativeField &source, Guard &guard) const {
        using namespace response_detail;
        endpoint(input(), input_grade(), source.context(), source.grade());
        auto contexts = state_->contexts;
        append(contexts, source.contexts());
        const auto length = factors().size();
        const auto local = add(add(state_->local, source.local_allowance()),
                               mul(32, mul(add(length, 2), add(state_->max_dimension, 1))));
        auto g = phase(guard, retained(contexts, local));
        validate_receiving(guard);
        source.validate_receiving(guard);
        std::vector<V> prefix{source.value()};
        auto tangent = source.dot();
        for (const auto &f : factors()) {
            auto next = sum(f.apply_dot(prefix.back(), g), f.apply(tangent, g), g);
            prefix.push_back(f.apply(prefix.back(), g));
            tangent = std::move(next);
        }
        auto input_driver = source.dot(),
             cov_input =
                 sum(source.dot(),
                     covariant_detail::apply(input().input().connections[input_grade()], source.value(), g),
                     g, true);
        for (const auto &f : factors()) {
            input_driver = f.apply(input_driver, g);
            cov_input = f.apply(cov_input, g);
        }
        std::vector<V> positions, cov_positions;
        auto dot = input_driver, covdot = cov_input;
        for (std::size_t i = 0; i < length; ++i) {
            auto d = factors()[i].apply_dot(prefix[i], g), c = factors()[i].apply_covariant(prefix[i], g);
            for (std::size_t j = i + 1; j < length; ++j) {
                d = factors()[j].apply(d, g);
                c = factors()[j].apply(c, g);
            }
            dot = sum(std::move(dot), d, g);
            covdot = sum(std::move(covdot), c, g);
            positions.push_back(std::move(d));
            cov_positions.push_back(std::move(c));
        }
        const auto expected_cov =
            sum(dot, covariant_detail::apply(output().input().connections[output_grade()], prefix.back(), g),
                g, true);
        if (dot != tangent || covdot != expected_cov)
            throw std::logic_error("ordered derivative word residual");
        guard.step();
        return Result(std::make_shared<const ResultState>(
            ResultState{state_, source, std::move(prefix.back()), std::move(dot), std::move(input_driver),
                        std::move(cov_input), std::move(positions), std::move(cov_positions)}));
    }

  private:
    explicit PreparedDerivativeWord(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
// A bilinear cross form maps the right output to the left algebraic dual. It
// is not silently treated as a metric adjoint or a map between vector carriers.
class CrossPairingJet {
    using Q = response_detail::Q;
    using M = response_detail::M;
    using V = response_detail::V;
    using Guard = response_detail::Guard;
    struct State {
        PreparedCovariantJet left, right;
        std::size_t left_grade, right_grade;
        M form, dot;
        response_detail::Contexts contexts;
        std::size_t local;
    };

  public:
    struct Result {
        Q value, derivative, left_field, pairing, right_field, covariant_derivative;
    };
    struct WordResult {
        Result total;
        Q left_input, right_input;
        std::vector<Q> left_positions, right_positions;
    };
    static CrossPairingJet declare(PreparedCovariantJet left, std::size_t lg, PreparedCovariantJet right,
                                   std::size_t rg, M form, M dot, Guard &g) {
        using namespace response_detail;
        if (!left.base().point().same_owner(right.base().point()) ||
            !left.direction().same_owner(right.direction()))
            throw std::invalid_argument("cross pairing point/direction owner differs");
        Contexts contexts;
        append(contexts, left);
        append(contexts, right);
        const auto local = mul(16, add(add(form.nonzeros(), dot.nonzeros()), 1));
        g.entries(retained(contexts, local));
        response_detail::validate(contexts, g);
        shape(form, left.base().metrics().at(lg).rows(), right.base().metrics().at(rg).rows(), g);
        shape(dot, form.rows(), form.cols(), g);
        g.step();
        return CrossPairingJet(
            std::make_shared<const State>(State{std::move(left), std::move(right), lg, rg, std::move(form),
                                                std::move(dot), std::move(contexts), local}));
    }
    template <class A, class B>
        requires(std::floating_point<A> || std::floating_point<B>)
    static CrossPairingJet declare(PreparedCovariantJet, A, PreparedCovariantJet, B, M, M, Guard &) = delete;
    static CrossPairingJet metric(PreparedCovariantJet context, std::size_t grade, Guard &g) {
        auto m = context.base().metrics().at(grade), d = context.input().metric_derivatives.at(grade);
        return declare(context, grade, context, grade, std::move(m), std::move(d), g);
    }
    template <std::floating_point T> static CrossPairingJet metric(PreparedCovariantJet, T, Guard &) = delete;
    const M &form() const & noexcept {
        return state_->form;
    }
    const M &form() const && = delete;
    const M &dot() const & noexcept {
        return state_->dot;
    }
    const M &dot() const && = delete;
    Result pair(const OwnedDerivativeField &left, const OwnedDerivativeField &right, Guard &guard) const {
        using namespace response_detail;
        endpoint(left.context(), left.grade(), state_->left, state_->left_grade);
        endpoint(right.context(), right.grade(), state_->right, state_->right_grade);
        auto contexts = left.contexts();
        append(contexts, right.contexts());
        append(contexts, state_->contexts);
        auto g = phase(guard, retained(contexts, add(add(state_->local, left.local_allowance()),
                                                     right.local_allowance())));
        left.validate_receiving(guard);
        right.validate_receiving(guard);
        g.matrix(form());
        g.matrix(dot());
        const auto value = pairing(left.value(), form(), right.value(), g),
                   l = pairing(left.dot(), form(), right.value(), g),
                   m = pairing(left.value(), dot(), right.value(), g),
                   r = pairing(left.value(), form(), right.dot(), g);
        const auto derivative = g.add(g.add(l, m), r);
        const auto &gl = state_->left.input().connections[state_->left_grade];
        const auto &gr = state_->right.input().connections[state_->right_grade];
        const auto lc = sum(left.dot(), covariant_detail::apply(gl, left.value(), g), g, true),
                   rc = sum(right.dot(), covariant_detail::apply(gr, right.value(), g), g, true);
        const auto residual =
            sum(sum(dot(), product(gl.transpose(), form(), g), g), product(form(), gr, g), g);
        const auto cov = g.add(
            g.add(pairing(lc, form(), right.value(), g), pairing(left.value(), residual, right.value(), g)),
            pairing(left.value(), form(), rc, g));
        if (cov != derivative)
            throw std::logic_error("cross-form derivative residual");
        guard.step();
        return {value, derivative, l, m, r, cov};
    }
    WordResult pair_words(const PreparedDerivativeWord::Result &left,
                          const PreparedDerivativeWord::Result &right, Guard &g) const {
        using namespace response_detail;
        const auto a = left.field(g), b = right.field(g);
        const auto total = pair(a, b, g);
        WordResult out{total,
                       pairing(left.input_driver(), form(), right.value(), g),
                       pairing(left.value(), form(), right.input_driver(), g),
                       {},
                       {}};
        Q result = g.add(g.add(out.left_input, out.right_input), total.pairing);
        g.entries(mul(4, add(add(left.positions().size(), right.positions().size()), 1)));
        for (const auto &v : left.positions()) {
            out.left_positions.push_back(pairing(v, form(), right.value(), g));
            result = g.add(result, out.left_positions.back());
        }
        for (const auto &v : right.positions()) {
            out.right_positions.push_back(pairing(left.value(), form(), v, g));
            result = g.add(result, out.right_positions.back());
        }
        if (result != total.derivative)
            throw std::logic_error("complete cross-word position attribution residual");
        g.step();
        return out;
    }

  private:
    explicit CrossPairingJet(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
// Source-cell selectors are transported from the primary source coordinates by
// the retained constant frame. Equal current outputs never merge their labels.
class SourceCellJetSelectors {
    using M = response_detail::M;
    using V = response_detail::V;
    using Guard = response_detail::Guard;
    struct State {
        PreparedCovariantJet context;
        std::size_t grade;
        std::vector<core::CellId> labels;
        std::vector<M> selectors;
        std::size_t local;
    };

  public:
    static SourceCellJetSelectors prepare(PreparedCovariantJet context, std::size_t grade, Guard &guard) {
        using namespace response_detail;
        const auto n = context.base().metrics().at(grade).rows();
        const auto local = mul(32, mul(add(n, 1), mul(add(n, 1), add(n, 1))));
        auto g = phase(guard, add(context.retained_allowance(), local));
        context.validate_receiving(guard);
        const auto &s = context.base().frames()[grade];
        std::vector<M> selectors;
        std::vector<core::CellId> labels;
        M total(n, n);
        for (std::size_t i = 0; i < n; ++i) {
            M projector(n, n);
            projector.set(i, i, 1);
            auto next = covariant_detail::solve(s, product(projector, s, g), g);
            if (product(next, next, g) != next)
                throw std::logic_error("source selector idempotency");
            total = sum(std::move(total), next, g);
            selectors.push_back(std::move(next));
            labels.push_back({{static_cast<std::uint16_t>(grade)}, static_cast<std::uint32_t>(i)});
        }
        if (total != M::identity(n))
            throw std::logic_error("source selector coverage");
        guard.step();
        return SourceCellJetSelectors(std::make_shared<const State>(
            State{std::move(context), grade, std::move(labels), std::move(selectors), local}));
    }
    template <std::floating_point T>
    static SourceCellJetSelectors prepare(PreparedCovariantJet, T, Guard &) = delete;
    const std::vector<core::CellId> &labels() const & noexcept {
        return state_->labels;
    }
    const std::vector<core::CellId> &labels() const && = delete;
    const std::vector<M> &selectors() const & noexcept {
        return state_->selectors;
    }
    const std::vector<M> &selectors() const && = delete;
    std::vector<OwnedDerivativeField> split(const OwnedDerivativeField &source, Guard &guard) const {
        using namespace response_detail;
        endpoint(source.context(), source.grade(), state_->context, state_->grade);
        auto contexts = source.contexts();
        append(contexts, state_->context);
        const auto local =
            add(source.local_allowance(),
                add(state_->local, mul(16, mul(add(selectors().size(), 1), add(source.value().size(), 1)))));
        auto g = phase(guard, retained(contexts, local));
        source.validate_receiving(guard);
        auto pin = std::make_shared<const std::pair<std::shared_ptr<const State>, OwnedDerivativeField>>(
            state_, source);
        std::vector<OwnedDerivativeField> result;
        V value(source.value().size()), dot(source.dot().size());
        for (const auto &p : selectors()) {
            auto x = covariant_detail::apply(p, source.value(), g),
                 d = covariant_detail::apply(p, source.dot(), g);
            value = sum(std::move(value), x, g);
            dot = sum(std::move(dot), d, g);
            result.push_back(OwnedDerivativeField::derived(
                state_->context, state_->grade, std::move(x), std::move(d), contexts, local, pin,
                [pin](Guard &receive) {
                    pin->second.validate_receiving(receive);
                    for (const auto &selector : pin->first->selectors)
                        receive.matrix(selector);
                },
                response_detail::add(source.dependency_depth(), 1), guard));
        }
        if (value != source.value() || dot != source.dot())
            throw std::logic_error("source selector field reconstruction");
        guard.step();
        return result;
    }

  private:
    explicit SourceCellJetSelectors(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::temporal
