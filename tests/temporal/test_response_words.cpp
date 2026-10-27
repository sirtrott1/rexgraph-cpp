#include "covariant_fixtures.hpp"
#include "rfc/temporal/word_derivative.hpp"
#include <iostream>
using namespace covariant_fixtures;
using Field = OwnedDerivativeField;
using Response = PreparedResponseJet;
using Action = DerivativeAction;
using Word = PreparedDerivativeWord;
using FM = covariant_family_detail::Matrix;
static std::size_t checks{};
static void require(bool b, const char *why) {
    ++checks;
    if (!b)
        throw std::runtime_error(why);
}
template <class Error = std::invalid_argument, class Fn> static void rejects(Fn fn, const char *why) {
    bool bad = false;
    try {
        fn();
    } catch (const Error &) {
        bad = true;
    }
    require(bad, why);
}
static void qj(const Q &q) {
    std::cout << '"' << q << '"';
}
static void vj(const V &v) {
    std::cout << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i)
            std::cout << ',';
        qj(v[i]);
    }
    std::cout << ']';
}
static void mj(const M &m) {
    std::cout << '[';
    for (std::size_t i = 0; i < m.rows(); ++i) {
        if (i)
            std::cout << ',';
        V row;
        for (std::size_t j = 0; j < m.cols(); ++j)
            row.push_back(m.get(i, j));
        vj(row);
    }
    std::cout << ']';
}
static void fj(const F &f) {
    std::cout << '[';
    vj(f.numerator());
    std::cout << ',';
    vj(f.denominator());
    std::cout << ']';
}
static void fmj(const FM &m) {
    std::cout << '[';
    for (std::size_t i = 0; i < m.rows; ++i) {
        if (i)
            std::cout << ',';
        std::cout << '[';
        for (std::size_t j = 0; j < m.cols; ++j) {
            if (j)
                std::cout << ',';
            fj(m.at(i, j));
        }
        std::cout << ']';
    }
    std::cout << ']';
}
static FM symbolic(std::size_t rows, std::size_t cols, const std::vector<F> &values, Guard &g) {
    covariant_family_detail::Work w(g, {});
    FM out(rows, cols, w);
    out.values = values;
    return out;
}
static M eval(const FM &f, Q at, Guard &g, bool dot = false) {
    covariant_family_detail::Work w(g, {});
    return covariant_family_detail::evaluate(dot ? covariant_family_detail::derivative(f, w) : f, at, w);
}
static void response_json(const char *name, Q at, const FM &a, const FM &b, const FM &metric,
                          const FM &kernel, const Response &problem, const Response::Result &result) {
    std::cout << "{\"kind\":\"response\",\"name\":\"" << name << "\",\"at\":";
    qj(at);
    std::cout << ",\"A\":";
    fmj(a);
    std::cout << ",\"b\":";
    fmj(b);
    std::cout << ",\"metric\":";
    fmj(metric);
    std::cout << ",\"kernel\":";
    fmj(kernel);
    std::cout << ",\"value\":";
    vj(result.value());
    std::cout << ",\"dot\":";
    vj(result.dot());
    std::cout << ",\"gauge\":";
    mj(problem.gauge());
    std::cout << ",\"gauge_dot\":";
    mj(problem.gauge_dot());
    std::cout << ",\"source_driver\":";
    vj(result.source_driver());
    std::cout << ",\"operator_driver\":";
    vj(result.operator_driver());
    std::cout << ",\"gauge_driver\":";
    vj(result.gauge_driver());
    std::cout << '}';
}
static void word_json(const char *name, Q at, const std::vector<FM> &factors, const FM &input,
                      const Word::Result &result) {
    std::cout << "{\"kind\":\"word\",\"name\":\"" << name << "\",\"at\":";
    qj(at);
    std::cout << ",\"factors\":[";
    for (std::size_t i = 0; i < factors.size(); ++i) {
        if (i)
            std::cout << ',';
        fmj(factors[i]);
    }
    std::cout << "],\"input\":";
    fmj(input);
    std::cout << ",\"value\":";
    vj(result.value());
    std::cout << ",\"dot\":";
    vj(result.dot());
    std::cout << ",\"input_driver\":";
    vj(result.input_driver());
    std::cout << ",\"positions\":[";
    for (std::size_t i = 0; i < result.positions().size(); ++i) {
        if (i)
            std::cout << ',';
        vj(result.positions()[i]);
    }
    std::cout << "]}";
}
static V fsolve(const FM &a, const FM &b, Q at, bool dot) {
    numeric::RationalFunctionMatrix matrix(a.rows, a.cols);
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t j = 0; j < a.cols; ++j)
            matrix.set(i, j, a.at(i, j));
    const auto values = matrix.solve(b.values);
    V out;
    for (const auto &f : values)
        out.push_back((dot ? f.derivative() : f).evaluate(at));
    return out;
}
int main(int argc, char **argv) {
    try {
        const bool dump = argc == 2 && std::string(argv[1]) == "--dump";
        bool first = true;
        if (dump)
            std::cout << '[';
        const auto emit = [&] {
            if (!first)
                std::cout << ',';
            first = false;
        };
        Guard g;
        auto d = domain(-2, 2, g);
        core::OwnedRelationalSource owner(pairs());
        auto input = constants(owner, d, g);
        const F t = F::parameter();
        input.fields[1] = entries(d, {F{Q{1}} + t, F{Q{2}} - t}, g);
        auto family = Family::prepare(owner, d, input, g);
        const auto af = symbolic(2, 2, {t, F{Q{1}}, F{Q{1}}, t}, g),
                   xf = symbolic(2, 2, {F{Q{1}}, t, F{}, F{Q{1}}}, g),
                   yf = symbolic(2, 2, {t + F{Q{2}}, F{}, F{Q{1}}, F{Q{1}}}, g);
        auto eqfamily = CovariantMapFamily::declare(family, 1, family, 1, entries(d, af.values, g), g),
             xmap = CovariantMapFamily::declare(family, 1, family, 1, entries(d, xf.values, g), g),
             ymap = CovariantMapFamily::declare(family, 1, family, 1, entries(d, yf.values, g), g);
        for (const Q &at : V{Q{-3, 2}, Q{-1, 2}, Q{0}, Q{1, 2}, Q{3, 2}}) {
            Guard local;
            const auto p = point(d, at, local);
            const auto context = family.jet(p, local);
            const auto rhs = Field::source(context, 1, local);
            const auto problem = Response::family(eqfamily, context, context, local);
            const auto result = problem.apply(rhs, local);
            require(result.value() == fsolve(af, family.fields()[1], at, false),
                    "native rational-function solve reference");
            require(result.dot() == fsolve(af, family.fields()[1], at, true),
                    "native rational-function solve derivative reference");
            const auto inverse = problem.inverse_action(local);
            const V probe{1, 0};
            const auto trans = inverse.value_action().transpose_apply(probe, local);
            require(eval(af, at, local).transpose().apply(trans) == probe,
                    "saved reduction algebraic transpose");
            const numeric::SparseRationalVector sparse(2, {{0, 1}});
            require(inverse.value_action().apply(sparse, local).dense() == result.value() ||
                        inverse.value_action().apply(sparse, local).dense() ==
                            eval(af, at, local).solve(probe),
                    "sparse original factor action");
            require(inverse.value_action().transpose_apply(sparse, local).dense() == trans,
                    "sparse transpose avoids dense replay");
            auto word = Word::prepare(context, 1,
                                      {Action::family(xmap, context, context, local),
                                       Action::family(ymap, context, context, local), inverse},
                                      local);
            const auto wr = word.execute(rhs, local);
            covariant_family_detail::Work work(local, {});
            const auto bx = covariant_family_detail::product(covariant_family_detail::product(yf, xf, work),
                                                             family.fields()[1], work);
            require(wr.value() == fsolve(af, bx, at, false) && wr.dot() == fsolve(af, bx, at, true),
                    "ordered inverse word matches rational elimination");
            require(wr.positions().size() == 3, "all ordered positions retained");
            const auto id = Word::prepare(context, 1, {}, local).execute(rhs, local);
            require(id.value() == rhs.value() && id.dot() == rhs.dot(),
                    "empty word retains actual declared identity");
            auto pairing = CrossPairingJet::metric(context, 1, local);
            const auto paired = pairing.pair_words(wr, id, local);
            require(paired.total.derivative ==
                        paired.total.left_field + paired.total.pairing + paired.total.right_field,
                    "both field and pairing channels");
            if (dump) {
                emit();
                response_json("pivot-safe", at, af, family.fields()[1], family.metrics()[1],
                              symbolic(2, 0, {}, local), problem, result);
                emit();
                const F den = t * t - F{Q{1}};
                const auto inverse_function =
                    symbolic(2, 2, {t / den, F{Q{-1}} / den, F{Q{-1}} / den, t / den}, local);
                word_json("ordered-inverse", at, {xf, yf, inverse_function}, family.fields()[1], wr);
            }
            if (at == Q{1, 2}) {
                auto opposite = Word::prepare(context, 1,
                                              {Action::family(ymap, context, context, local),
                                               Action::family(xmap, context, context, local), inverse},
                                              local)
                                    .execute(rhs, local);
                require(opposite.value() != wr.value() && opposite.dot() != wr.dot(),
                        "noncommuting order is not pooled");
                auto selector = SourceCellJetSelectors::prepare(context, 1, local);
                const auto pieces = selector.split(rhs, local);
                Q total{}, dot{};
                std::vector<Q> channels, derivatives;
                for (const auto &piece : pieces) {
                    const auto a = word.execute(piece, local).field(local);
                    const auto value = pairing.pair(a, rhs, local);
                    channels.push_back(value.value);
                    derivatives.push_back(value.derivative);
                    total += value.value;
                    dot += value.derivative;
                }
                require(total == paired.total.value && dot == paired.total.derivative,
                        "source channels sum without losing their identities");
                const std::vector<M> frames{M::identity(4), M::from_rows({{1, 1}, {0, 1}})};
                const auto changed = context.reframe(frames, local);
                const auto &s = frames[1];
                const auto transform = [&](const FM &f, bool derivative) {
                    return covariant_detail::solve(s, eval(f, at, local, derivative).multiply(s), local);
                };
                const auto equation = RectangularCovariantJet::declare(
                    changed, 1, changed, 1, transform(af, false), transform(af, true), local);
                const auto problem2 = Response::prepare(equation, local);
                auto word2 =
                    Word::prepare(changed, 1,
                                  {Action::map(RectangularCovariantJet::declare(changed, 1, changed, 1,
                                                                                transform(xf, false),
                                                                                transform(xf, true), local),
                                               local),
                                   Action::map(RectangularCovariantJet::declare(changed, 1, changed, 1,
                                                                                transform(yf, false),
                                                                                transform(yf, true), local),
                                               local),
                                   problem2.inverse_action(local)},
                                  local);
                const auto rhs2 = Field::source(changed, 1, local);
                const auto whole2 = word2.execute(rhs2, local);
                require(s.apply(whole2.value()) == wr.value() && s.apply(whole2.dot()) == wr.dot(),
                        "whole response changes covariantly with its declared frame");
                auto selectors2 = SourceCellJetSelectors::prepare(changed, 1, local);
                auto pairing2 = CrossPairingJet::metric(changed, 1, local);
                const auto pieces2 = selectors2.split(rhs2, local);
                bool wrong = false;
                for (std::size_t i = 0; i < pieces2.size(); ++i) {
                    const auto v = pairing2.pair(word2.execute(pieces2[i], local).field(local), rhs2, local);
                    require(v.value == channels[i] && v.derivative == derivatives[i],
                            "each actual source selector is transported S^-1 Pi S");
                    M diag(2, 2);
                    diag.set(i, i, 1);
                    const auto bad =
                        Field::declare(changed, 1, diag.apply(rhs2.value()), diag.apply(rhs2.dot()), local);
                    wrong =
                        wrong ||
                        pairing2.pair(word2.execute(bad, local).field(local), rhs2, local).value != v.value;
                }
                require(wrong, "untransported selectors can hide behind a correct total");
                if (dump) {
                    emit();
                    std::cout << "{\"kind\":\"selectors\",\"frame\":";
                    mj(s);
                    std::cout << ",\"old\":[";
                    for (std::size_t i = 0; i < selector.selectors().size(); ++i) {
                        if (i)
                            std::cout << ',';
                        mj(selector.selectors()[i]);
                    }
                    std::cout << "],\"next\":[";
                    for (std::size_t i = 0; i < selectors2.selectors().size(); ++i) {
                        if (i)
                            std::cout << ',';
                        mj(selectors2.selectors()[i]);
                    }
                    std::cout << "],\"values\":";
                    vj(channels);
                    std::cout << ",\"derivatives\":";
                    vj(derivatives);
                    std::cout << '}';
                }
            }
        }
        for (const Q &at : V{Q{-1}, Q{1}}) {
            const auto context = family.jet(point(d, at, g), g);
            rejects<std::domain_error>([&] { (void)Response::family(eqfamily, context, context, g); },
                                       "rank exception cannot acquire an inverse");
        }
        auto positive = domain(Q{1, 4}, 3, g);
        auto mainfamily = Family::prepare(owner, positive, main_input(owner, positive, g), g);
        auto lambda = expression(positive, t * t + F{Q{1}}, g);
        for (const Q &at : V{Q{1, 2}, Q{1}, Q{2}}) {
            Guard local;
            const auto context = mainfamily.jet(point(positive, at, local), local);
            const auto rhs = Field::source(context, 1, local);
            const auto response = Response::green_expression(context, 1, lambda, local);
            const auto result = response.apply(rhs, local);
            auto a = symbolic(2, 2,
                              {F{Q{1}} + (t * t + F{Q{1}}) * F({Q{2}}, {0, 0, 1}), F{}, F{},
                               F{Q{1}} + (t * t + F{Q{1}}) * F{Q{3}}},
                              local);
            require(result.value() == fsolve(a, mainfamily.fields()[1], at, false) &&
                        result.dot() == fsolve(a, mainfamily.fields()[1], at, true),
                    "Green includes lambda, metric, Hodge and source derivatives");
            const auto missing =
                Response::green(context, 1, lambda.formal().evaluate(at), Q{0}, local).apply(rhs, local);
            require(missing.value() == result.value() && missing.dot() != result.dot(),
                    "fixed lambda derivative is not varying lambda");
            if (dump) {
                emit();
                response_json("varying-green", at, a, mainfamily.fields()[1], mainfamily.metrics()[1],
                              symbolic(2, 0, {}, local), response, result);
            }
        }
        // A moving complete kernel and metric gauge on a rectangular exact system.
        core::OwnedRelationalSource tri(triangle());
        auto ti = weighted(tri, positive, g);
        std::vector<F> ms{F{Q{1}} + t * t, F{}, F{}, F{}, F{Q{2}} + t * t, F{}, F{}, F{}, F{Q{3}} + t * t};
        ti.metrics[1] = entries(positive, ms, g);
        ti.connections[1] =
            entries(positive,
                    {(F{} - t) / (F{Q{1}} + t * t), F{}, F{}, F{}, (F{} - t) / (F{Q{2}} + t * t), F{}, F{},
                     F{}, (F{} - t) / (F{Q{3}} + t * t)},
                    g);
        const auto triangle_family = Family::prepare(tri, positive, ti, g);
        const auto ga = symbolic(1, 3, {F{Q{1}}, t, t * t}, g),
                   kn = symbolic(3, 2, {F{} - t, F{} - t * t, F{Q{1}}, F{}, F{}, F{Q{1}}}, g);
        const auto gm = CovariantMapFamily::declare(triangle_family, 1, triangle_family, 2,
                                                    entries(positive, ga.values, g), g);
        for (const Q &at : V{Q{1, 2}, Q{1}, Q{2}}) {
            Guard local;
            const auto context = triangle_family.jet(point(positive, at, local), local);
            const auto problem =
                Response::family(gm, context, context, eval(kn, at, local), eval(kn, at, local, true), local);
            const auto rhs = Field::source(context, 2, local);
            const auto result = problem.apply(rhs, local);
            require(problem.rank() == 1 && problem.kernel().cols() == 2 && problem.gauge_dot().nonzeros(),
                    "complete moving metric gauge retained");
            require(problem.equation().value().apply(result.value()) == rhs.value() &&
                        problem.gauge().apply(result.value()) == V(2),
                    "all original potential equations and gauges");
            rejects([&] { (void)problem.inverse_action(local); },
                    "rectangular partial solve is not a total inverse action");
            if (dump) {
                emit();
                response_json("moving-gauge", at, ga, triangle_family.fields()[2],
                              triangle_family.metrics()[1], kn, problem, result);
            }
            const auto boundary_adjoint = RectangularCovariantJet::declare(
                context, 0, context, 1, context.adjoints()[1], context.adjoint_dots()[1], local);
            M harmonic(3, 1);
            for (std::size_t i = 0; i < 3; ++i)
                harmonic.set(i, 0, 1);
            const auto native_potential = Response::gauged(boundary_adjoint, harmonic, M(3, 1), local);
            const auto gradient_value = boundary_adjoint.value().apply(context.base().fields()[0]);
            const auto gradient_dot = boundary_adjoint.dot().apply(context.base().fields()[0]);
            const auto extra_dot = boundary_adjoint.value().apply(context.input().field_derivatives[0]);
            const auto gradient =
                Field::declare(context, 1, gradient_value, numeric::add(gradient_dot, extra_dot), local);
            const auto minimal = native_potential.apply(gradient, local);
            require(minimal.value() == V{-1, 0, 1} && minimal.dot() == V(3),
                    "full native adjoint-kernel minimal-potential gauge");
            auto r = symbolic(2, 2, {t, F{}, F{}, F{Q{1}}}, local);
            covariant_family_detail::Work work(local, {});
            const auto changed = covariant_family_detail::product(kn, r, work);
            const auto p2 = Response::family(gm, context, context, eval(changed, at, local),
                                             eval(changed, at, local, true), local);
            const auto same = p2.apply(rhs, local);
            require(same.value() == result.value() && same.dot() == result.dot(),
                    "kernel coordinate changes do not alter the full metric gauge");
            const auto observation =
                CovariantMapFamily::declare(triangle_family, 1, triangle_family, 2,
                                            entries(positive, {F{Q{1}}, t, F{Q{1}} / t}, local), local);
            const auto lift = CovariantMapFamily::declare(
                triangle_family, 2, triangle_family, 0, entries(positive, {F{Q{1}}, t, t * t}, local), local);
            auto word = Word::prepare(context, 1,
                                      {Action::family(observation, context, context, local),
                                       Action::family(lift, context, context, local)},
                                      local);
            auto wr = word.execute(result.field(local), local);
            auto plain = Word::prepare(context, 2, {}, local).execute(rhs, local);
            const auto form = symbolic(3, 1, {F{Q{1}} + t, F{Q{2}} - t, t * t}, local);
            const auto cross = CrossPairingJet::declare(context, 0, context, 2, eval(form, at, local),
                                                        eval(form, at, local, true), local);
            const auto paired = cross.pair_words(wr, plain, local);
            require(paired.total.pairing != 0 && paired.left_positions.size() == 2,
                    "rectangular cross form retains its derivative and all output axes");
            if (dump) {
                emit();
                std::cout << "{\"kind\":\"cross\",\"at\":";
                qj(at);
                std::cout << ",\"left\":";
                vj(wr.value());
                std::cout << ",\"left_dot\":";
                vj(wr.dot());
                std::cout << ",\"right\":";
                vj(rhs.value());
                std::cout << ",\"right_dot\":";
                vj(rhs.dot());
                std::cout << ",\"operators\":[";
                fmj(symbolic(1, 3, {F{Q{1}}, t, F{Q{1}} / t}, local));
                std::cout << ',';
                fmj(symbolic(3, 1, {F{Q{1}}, t, t * t}, local));
                std::cout << ']';
                std::cout << ",\"form\":";
                fmj(form);
                std::cout << ",\"value\":";
                qj(paired.total.value);
                std::cout << ",\"dot\":";
                qj(paired.total.derivative);
                std::cout << ",\"pairing\":";
                qj(paired.total.pairing);
                std::cout << '}';
            }
            if (at == 1) {
                rejects([&] { (void)Response::prepare(problem.equation(), local); },
                        "missing gauge directions refused");
                auto bad = eval(kn, at, local, true);
                bad.set(0, 0, 0);
                rejects([&] { (void)Response::gauged(problem.equation(), eval(kn, at, local), bad, local); },
                        "wrong kernel tangent refused");
            }
        }
        // Dependent original rows must all survive the reduction.
        const auto p = point(d, Q{0}, g);
        const auto context = family.jet(p, g);
        auto eq = RectangularCovariantJet::declare(context, 1, context, 1, M::from_rows({{1, 1}, {2, 2}}),
                                                   M(2, 2), g);
        auto potential = Response::gauged(eq, M::from_rows({{-1}, {1}}), M(2, 1), g);
        auto rhs = Field::declare(context, 1, {3, 6}, {1, 2}, g);
        const auto solved = potential.apply(rhs, g);
        require(solved.value() == V{Q{3, 2}, Q{3, 2}} && solved.dot() == V{Q{1, 2}, Q{1, 2}},
                "full dependent row/gauge solution");
        rejects([&] { (void)potential.apply(Field::declare(context, 1, {3, 7}, {1, 2}, g), g); },
                "inconsistent original row refused");
        rejects([&] { (void)potential.apply(Field::declare(context, 1, {3, 6}, {1, 3}, g), g); },
                "inconsistent derivative row refused");
        const auto other = family.jet(p, g);
        rejects([&] { (void)potential.apply(Field::declare(other, 1, {3, 6}, {1, 2}, g), g); },
                "equal values do not identify derivative endpoints");
        rejects(
            [&] {
                (void)Word::prepare(
                    context, 1,
                    {Action::map(
                        RectangularCovariantJet::declare(other, 1, other, 1, M::identity(2), M(2, 2), g), g)},
                    g);
            },
            "foreign actual jet connection in word refused");
        auto hole_entries = entries(d, xf.values, g);
        hole_entries[0] = E::declare(d, {0, 1}, {0, 1}, g);
        auto holes = CovariantMapFamily::declare(family, 1, family, 1, hole_entries, g);
        rejects<std::domain_error>([&] { (void)Response::family(holes, context, context, g); },
                                   "cancelled original operator hole retained");
        auto hole_lambda = E::declare(d, {0, 1}, {0, 1}, g);
        rejects<std::domain_error>([&] { (void)Response::green_expression(context, 1, hole_lambda, g); },
                                   "cancelled original scalar hole retained");
        // Pointwise signed Green solves are stationary responses, not minimum claims.
        auto indefinite = Response::green(context, 1, Q{-1}, Q{0}, g);
        const auto signed_result = indefinite.apply(Field::source(context, 1, g), g);
        require(signed_result.value() == V{-1, -2}, "nonsingular signed Green parameter retained");
        rejects([&] { (void)Response::green(context, 1, Q{-1, 2}, Q{0}, g); },
                "singular Green inverse refused");
        auto cloned = family.jet(p, g);
        auto modified = cloned.input();
        modified.field_derivatives[1][0] += 1;
        const auto stale = Jet::prepare(cloned.base(), cloned.direction(), modified, g);
        rejects([&] { (void)Response::family(eqfamily, stale, stale, g); },
                "edited jets cannot borrow exact operator-family authority");
        auto raw_entries = entries(d, {F{Q{1}}, F{}, F{}, F{Q{1}}}, g);
        raw_entries[0] = E::declare(d, {0, Q{123456}}, {0, Q{123456}}, g);
        const auto raw_map = CovariantMapFamily::declare(family, 1, family, 1, raw_entries, g);
        const auto raw_context = family.jet(point(d, Q{1, 2}, g), g);
        const auto raw_problem = Response::family(raw_map, raw_context, raw_context, g);
        const auto raw_rhs = Field::source(raw_context, 1, g);
        auto raw_limits = numeric::ExactActionLimits{};
        raw_limits.integer_bits = 8;
        Guard raw_bits(raw_limits);
        rejects<std::length_error>([&] { (void)raw_problem.apply(raw_rhs, raw_bits); },
                                   "retained original cancelled operator coefficients are revalidated");
        // Result fields retain an original solve and its transitive dependencies.
        auto kept = []() {
            Guard q;
            auto d0 = domain(-2, 2, q);
            core::OwnedRelationalSource o(pairs());
            auto f = Family::prepare(o, d0, constants(o, d0, q), q);
            auto ctx = f.jet(point(d0, 0, q), q);
            return Response::green(ctx, 1, Q{1}, Q{0}, q).apply(Field::source(ctx, 1, q), q).field(q);
        }();
        require(kept.value() == V{Q{1, 3}, Q{2, 3}}, "temporary response lifetime retained");
        auto simple = Response::family(eqfamily, context, context, g);
        auto field = Field::source(context, 1, g);
        Guard measurement;
        const auto measured = simple.apply(field, measurement);
        (void)measured;
        for (const auto cutoff : {std::size_t{1}, measurement.steps() / 2, measurement.steps()}) {
            std::size_t steps{};
            Guard cancel({}, [&] {
                if (++steps == cutoff)
                    throw std::runtime_error("cancel");
            });
            rejects<std::runtime_error>([&] { (void)simple.apply(field, cancel); },
                                        "response early/middle/final cancellation");
        }
        auto word = Word::prepare(context, 1,
                                  {Action::family(xmap, context, context, g), simple.inverse_action(g)}, g);
        Guard wm;
        const auto mr = word.execute(field, wm);
        (void)mr;
        for (const auto cutoff : {std::size_t{1}, wm.steps() / 2, wm.steps()}) {
            std::size_t steps{};
            Guard cancel({}, [&] {
                if (++steps == cutoff)
                    throw std::runtime_error("cancel");
            });
            rejects<std::runtime_error>([&] { (void)word.execute(field, cancel); },
                                        "word early/middle/final cancellation");
        }
        auto limits = numeric::ExactActionLimits{};
        limits.entries = 0;
        Guard low_entries(limits);
        rejects<std::length_error>([&] { (void)simple.apply(field, low_entries); },
                                   "receiving retained closure budget");
        limits = numeric::ExactActionLimits{};
        limits.integer_bits = 1;
        Guard low_bits(limits);
        rejects<std::length_error>([&] { (void)simple.apply(field, low_bits); },
                                   "receiving coefficient bit budget");
        rejects<std::length_error>([&] { (void)Word::prepare(context, 1, {simple.inverse_action(g)}, g, 0); },
                                   "original word position limit");
        core::RelationSignature empty_sig;
        core::RelationalComplexBuilder empty_builder(empty_sig);
        core::OwnedRelationalSource empty_source(empty_builder.freeze());
        const auto ef = Family::prepare(empty_source, d, constants(empty_source, d, g), g);
        const auto ec = ef.jet(p, g);
        const auto er = Response::green(ec, 0, Q{1}, Q{0}, g).apply(Field::source(ec, 0, g), g);
        require(er.value().empty() && er.dot().empty(), "declared zero carrier response");
        auto history = Field::source(ec, 0, g);
        const auto identity_history = Word::prepare(ec, 0, {}, g);
        for (std::size_t i = 0; i < 128; ++i) {
            Guard fresh;
            history = identity_history.execute(history, fresh).field(fresh);
        }
        require(history.dependency_depth() == 128, "derived dependency depth is retained");
        rejects<std::length_error>(
            [&] {
                Guard fresh;
                (void)identity_history.execute(history, fresh).field(fresh);
            },
            "derived dependency depth refusal before recursive validation can grow unbounded");
        if (dump)
            std::cout << "]\n";
        else
            std::cout
                << checks
                << " exact response, complete gauge, ordered-word and source-attribution checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
