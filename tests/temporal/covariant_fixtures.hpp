#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/temporal/covariant_family.hpp"
namespace covariant_fixtures {
using namespace rfc;
using namespace temporal;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using F = numeric::RationalFunction;
using E = geometry::OwnedRationalExpression;
using Guard = numeric::ExactActionGuard;
using Family = PreparedCovariantFamily;
using Jet = PreparedCovariantJet;
inline core::RelationalComplex pairs() {
    core::RelationSignature s;
    auto edge = s.add_type("pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(edge, {{0, true, true, false, 1}, {1, true, false, false, 1}});
    b.add_grade_one(edge, {{2, true, true, false, 1}, {3, true, false, false, 1}});
    return b.freeze();
}
inline core::RelationalComplex triangle() {
    core::RelationSignature s;
    auto e = s.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    auto f = s.add_type("face", core::BoundaryPolicy::DeclaredExact, 3, 3);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(e, {{0, true, true, false, 1}, {1, true, false, false, 1}});
    b.add_grade_one(e, {{1, true, true, false, 1}, {2, true, false, false, 1}});
    b.add_grade_one(e, {{0, true, true, false, 1}, {2, true, false, false, 1}});
    b.add_higher({2}, f, {{0, true, false, false, 1}, {1, true, false, false, 1}, {2, true, true, false, 1}});
    return b.freeze();
}
inline core::RelationalComplex higher() {
    core::RelationSignature s;
    auto e = s.add_type("loop", core::BoundaryPolicy::DeclaredExact, 2, 2);
    auto f = s.add_type("cancelled-face", core::BoundaryPolicy::DeclaredExact, 3, 3);
    auto v = s.add_type("volume", core::BoundaryPolicy::DeclaredExact, 1, 1);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(e, {{0, true, true, false, 1}, {0, true, false, false, 1}});
    b.add_higher({2}, f,
                 {{0, true, false, false, 1}, {0, true, true, false, 1}, {0, false, false, false, 3}});
    b.add_higher({3}, v, {{0, true, false, false, 1}});
    return b.freeze();
}
inline E expression(const geometry::UnivariateDomain &d, F f, Guard &g) {
    return E::declare(d, f.numerator(), f.denominator(), g);
}
inline std::vector<E> entries(const geometry::UnivariateDomain &d, const std::vector<F> &values, Guard &g) {
    std::vector<E> out;
    for (const auto &f : values)
        out.push_back(expression(d, f, g));
    return out;
}
inline std::vector<F> constant(const M &m) {
    std::vector<F> out;
    for (std::size_t i = 0; i < m.rows(); ++i)
        for (std::size_t j = 0; j < m.cols(); ++j)
            out.emplace_back(m.get(i, j));
    return out;
}
inline CovariantFamilyInput constants(const core::OwnedRelationalSource &owner,
                                      const geometry::UnivariateDomain &d, Guard &g) {
    CovariantFamilyInput input;
    for (std::size_t k = 0; k < owner.source().grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const auto &a = owner.source().grade(grade);
        std::vector<F> co;
        for (std::size_t p = 0; p < a.incidence_count(); ++p)
            co.emplace_back(owner.source().incidence_coefficient(grade, p));
        input.occurrence_coefficients.push_back(entries(d, co, g));
        input.metrics.push_back(entries(d, constant(M::identity(a.cell_count())), g));
        input.connections.push_back(entries(d, constant(M(a.cell_count(), a.cell_count())), g));
        std::vector<F> x;
        for (std::size_t i = 0; i < a.cell_count(); ++i)
            x.emplace_back(Q{static_cast<unsigned long>(i + 1)});
        input.fields.push_back(entries(d, x, g));
    }
    return input;
}
inline geometry::UnivariateDomain domain(Q lo, Q hi, Guard &g) {
    return geometry::UnivariateDomain::declare(geometry::UnivariateCoordinate::declare("t", 8), lo, hi, true,
                                               true, {}, g);
}
inline OwnedJetPoint point(const geometry::UnivariateDomain &d, Q q, Guard &g) {
    return OwnedJetPoint::declare(d, d.at(std::move(q), g), g);
}
inline CovariantFamilyInput main_input(const core::OwnedRelationalSource &owner,
                                       const geometry::UnivariateDomain &d, Guard &g,
                                       bool compatible = true) {
    auto input = constants(owner, d, g);
    auto t = F::parameter();
    input.metrics[0] = entries(
        d, constant(M::from_rows({{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, Q{3, 2}, 0}, {0, 0, 0, Q{3, 2}}})), g);
    input.metrics[1] = entries(d, {t * t, F{}, F{}, F{Q{1}}}, g);
    input.fields[0] = entries(d, {F{}, F{}, F{}, F{}}, g);
    input.fields[1] = entries(d, {t * t, F{Q{1}} + t}, g);
    if (compatible)
        input.connections[1] = entries(d, {F{Q{-1}} / t, F{}, F{}, F{}}, g);
    return input;
}
inline CovariantFamilyInput weighted(const core::OwnedRelationalSource &owner,
                                     const geometry::UnivariateDomain &d, Guard &g,
                                     bool tangent_only = false) {
    auto input = constants(owner, d, g);
    auto t = F::parameter();
    const auto &a = owner.source().grade({1});
    for (std::size_t p = 0; p < a.incidence_count(); ++p)
        if (a.upper_index[p] == 0)
            input.occurrence_coefficients[1][p] =
                expression(d, F{owner.source().incidence_coefficient({1}, p)} * t, g);
    input.occurrence_coefficients[2][0] = expression(d, tangent_only ? F{Q{2}} - t : F{Q{1}} / t, g);
    for (std::size_t k = 0; k < input.fields.size(); ++k) {
        std::vector<F> x;
        for (std::size_t i = 0; i < input.fields[k].size(); ++i)
            x.push_back(F{Q{static_cast<unsigned long>(i + 1)}} +
                        t * F{Q{static_cast<unsigned long>(k + 1)}});
        input.fields[k] = entries(d, x, g);
    }
    return input;
}
inline CovariantJetInput derivatives(const CovariantFamilyInput &input, Q at, Guard &g) {
    CovariantJetInput out;
    temporal::covariant_family_detail::Work w(g, {});
    for (std::size_t k = 0; k < input.metrics.size(); ++k) {
        V co, x;
        for (const auto &e : input.occurrence_coefficients[k])
            co.push_back(w.evaluate(w.derivative(e.formal()), at));
        for (const auto &e : input.fields[k])
            x.push_back(w.evaluate(w.derivative(e.formal()), at));
        out.occurrence_derivatives.push_back(std::move(co));
        out.field_derivatives.push_back(std::move(x));
        const auto n = input.fields[k].size();
        auto m = temporal::covariant_family_detail::matrix(
            n, n, input.metrics[k],
            input.metrics[k].empty() ? input.fields[k][0].domain() : input.metrics[k][0].domain(), w);
        auto c = temporal::covariant_family_detail::matrix(n, n, input.connections[k],
                                                           input.metrics[k][0].domain(), w);
        out.metric_derivatives.push_back(temporal::covariant_family_detail::evaluate(
            temporal::covariant_family_detail::derivative(m, w), at, w));
        out.connections.push_back(temporal::covariant_family_detail::evaluate(c, at, w));
    }
    return out;
}
inline OwnedJetBase base(const core::OwnedRelationalSource &owner, const geometry::UnivariateDomain &d,
                         const CovariantFamilyInput &input, const OwnedJetPoint &p, Guard &g) {
    std::vector<V> co, x;
    std::vector<M> metrics;
    for (std::size_t k = 0; k < input.metrics.size(); ++k) {
        V a, b;
        for (const auto &e : input.occurrence_coefficients[k])
            a.push_back(e.evaluate(d.at(p.value(), g), g));
        for (const auto &e : input.fields[k])
            b.push_back(e.evaluate(d.at(p.value(), g), g));
        co.push_back(std::move(a));
        x.push_back(std::move(b));
        const auto n = input.fields[k].size();
        M m(n, n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                m.set(i, j, input.metrics[k][i * n + j].evaluate(d.at(p.value(), g), g));
        metrics.push_back(std::move(m));
    }
    return OwnedJetBase::declare(owner, p, std::move(co), std::move(metrics), std::move(x), g);
}
} // namespace covariant_fixtures
