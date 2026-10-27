#include "covariant_fixtures.hpp"
#include "rfc/temporal/smooth.hpp"
#include <iostream>
using namespace covariant_fixtures;
static std::size_t checks{};
static void require(bool v, const char *why) {
    ++checks;
    if (!v)
        throw std::runtime_error(why);
}
template <class Error = std::invalid_argument, class Work> static void rejects(Work work, const char *why) {
    bool bad = false;
    try {
        work();
    } catch (const Error &) {
        bad = true;
    }
    require(bad, why);
}
static void qjson(const Q &q) {
    std::cout << '"' << q << '"';
}
static void vjson(const V &v) {
    std::cout << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i)
            std::cout << ',';
        qjson(v[i]);
    }
    std::cout << ']';
}
static void mjson(const M &m) {
    std::cout << '[';
    for (std::size_t i = 0; i < m.rows(); ++i) {
        if (i)
            std::cout << ',';
        V row;
        for (std::size_t j = 0; j < m.cols(); ++j)
            row.push_back(m.get(i, j));
        vjson(row);
    }
    std::cout << ']';
}
static void fjson(const F &f) {
    std::cout << '[';
    vjson(f.numerator());
    std::cout << ',';
    vjson(f.denominator());
    std::cout << ']';
}
static void fmjson(const temporal::covariant_family_detail::Matrix &m) {
    std::cout << '[';
    for (std::size_t i = 0; i < m.rows; ++i) {
        if (i)
            std::cout << ',';
        std::cout << '[';
        for (std::size_t j = 0; j < m.cols; ++j) {
            if (j)
                std::cout << ',';
            fjson(m.at(i, j));
        }
        std::cout << ']';
    }
    std::cout << ']';
}
static void moment_json(const Jet::Moment &m) {
    std::cout << "{\"value\":";
    qjson(m.value);
    std::cout << ",\"derivative\":";
    qjson(m.derivative);
    std::cout << ",\"left\":";
    qjson(m.left_field);
    std::cout << ",\"right\":";
    qjson(m.right_field);
    std::cout << ",\"pairing\":";
    qjson(m.pairing_channel);
    std::cout << ",\"cov_left\":";
    qjson(m.covariant_left);
    std::cout << ",\"cov_right\":";
    qjson(m.covariant_right);
    std::cout << ",\"metric_residual\":";
    qjson(m.metric_residual_channel);
    std::cout << ",\"positions\":";
    vjson(m.operator_positions);
    std::cout << ",\"cov_positions\":";
    vjson(m.covariant_positions);
    std::cout << '}';
}
static void emit(const std::string &name, const Family &family, const Jet &jet, Q at, Guard &g) {
    std::cout << "{\"name\":\"" << name << "\",\"at\":";
    qjson(at);
    std::cout << ",\"compatible\":" << jet.metric_compatible() << ",\"grades\":[";
    for (std::size_t k = 0; k < jet.hodge().size(); ++k) {
        if (k)
            std::cout << ',';
        std::cout << "{\"metric_family\":";
        fmjson(family.metrics()[k]);
        std::cout << ",\"connection_family\":";
        fmjson(family.connections()[k]);
        std::cout << ",\"field_family\":";
        fmjson(family.fields()[k]);
        std::cout << ",\"occurrences\":[";
        if (k) {
            const auto &arena = family.owner().source().grade({static_cast<std::uint16_t>(k)});
            for (std::size_t s = 0; s < arena.incidence_count(); ++s) {
                if (s)
                    std::cout << ',';
                std::cout << '[' << arena.lower_index[s] << ',' << arena.upper_index[s] << ','
                          << arena.aspects.get(family.owner().source().signature().existence_aspect(), s)
                          << ',';
                fjson(family.input().occurrence_coefficients[k][s].formal());
                std::cout << ']';
            }
        }
        std::cout << ']';
        std::cout << ",\"boundary_family\":";
        fmjson(family.boundaries()[k]);
        std::cout << ",\"boundary\":";
        mjson(jet.base().boundaries()[k]);
        std::cout << ",\"boundary_dot\":";
        mjson(jet.boundary_dots()[k]);
        std::cout << ",\"boundary_cov\":";
        mjson(jet.boundary_covariants()[k]);
        std::cout << ",\"metric\":";
        mjson(jet.base().metrics()[k]);
        std::cout << ",\"metric_dot\":";
        mjson(jet.input().metric_derivatives[k]);
        std::cout << ",\"connection\":";
        mjson(jet.input().connections[k]);
        std::cout << ",\"field\":";
        vjson(jet.base().fields()[k]);
        std::cout << ",\"field_dot\":";
        vjson(jet.input().field_derivatives[k]);
        std::cout << ",\"field_cov\":";
        vjson(jet.field_covariants()[k]);
        std::cout << ",\"adjoint\":";
        mjson(jet.adjoints()[k]);
        std::cout << ",\"adjoint_dot\":";
        mjson(jet.adjoint_dots()[k]);
        std::cout << ",\"hodge\":";
        mjson(jet.hodge()[k]);
        std::cout << ",\"hodge_dot\":";
        mjson(jet.hodge_dots()[k]);
        std::cout << ",\"hodge_cov\":";
        mjson(jet.hodge_covariants()[k]);
        std::cout << ",\"moments\":[";
        for (std::size_t n = 0; n <= 4; ++n) {
            if (n)
                std::cout << ',';
            moment_json(jet.moment(k, n, g));
        }
        std::cout << "]}";
    }
    std::cout << "]}";
}
int main(int argc, char **argv) {
    try {
        const bool dump = argc == 2 && std::string(argv[1]) == "--dump";
        if (dump)
            std::cout << '[';
        bool first = true;
        Guard g;
        const auto d = domain(Q{1, 4}, Q{3}, g);
        core::OwnedRelationalSource owner(pairs());
        const auto input = main_input(owner, d, g);
        const auto family = Family::prepare(owner, d, input, g);
        const auto p = point(d, Q{1}, g);
        const auto jet = family.jet(p, g);
        require(family.owner().same_owner(owner) && jet.base().owner().same_owner(owner),
                "retained source owner");
        require(jet.order() == JetOrder::First && jet.metric_compatible(), "first-order metric contract");
        require(jet.field_covariants()[1] == V{3, 1}, "canonical field connection sign");
        require(jet.hodge()[1] == M::from_rows({{2, 0}, {0, 3}}), "canonical Hodge value");
        require(jet.hodge_covariants()[1] == M::from_rows({{-4, 0}, {0, 0}}),
                "canonical Hodge derivative sign");
        for (std::size_t n = 1; n <= 3; ++n)
            require(jet.moment(1, n, g).derivative == Q{n == 1   ? 20
                                                        : n == 2 ? 44
                                                                 : 108},
                    "canonical 20/44/108 derivatives");
        const F t = F::parameter();
        for (std::size_t n = 0; n <= 12; ++n) {
            Guard local;
            const auto symbolic = family.symbolic_moment(1, n, local);
            F two{Q{1}}, three{Q{1}}, tp{Q{1}};
            for (std::size_t i = 0; i < n; ++i) {
                two = two * F{Q{2}};
                three = three * F{Q{3}};
            }
            const auto exponent = 6 - static_cast<int>(2 * n);
            for (int i = 0; i < std::abs(exponent); ++i)
                tp = tp * t;
            const auto expected =
                two * (exponent >= 0 ? tp : F{Q{1}} / tp) + three * (F{Q{1}} + t) * (F{Q{1}} + t);
            require(symbolic.formal() == expected && symbolic.derivative() == expected.derivative(),
                    "symbolic all-power formula");
            require(symbolic.evaluate(p, local) == jet.moment(1, n, local).value,
                    "source-bound symbolic specialization");
        }
        std::vector<Family> families{family};
        std::vector<std::string> names{"main"};
        core::OwnedRelationalSource tri(triangle());
        auto wi = weighted(tri, d, g);
        const auto weighted_family = Family::prepare(tri, d, wi, g);
        families.push_back(weighted_family);
        names.push_back("weighted");
        auto dense = wi;
        dense.metrics[0] = entries(d, constant(M::from_rows({{2, 1, 0}, {1, 3, 1}, {0, 1, 2}})), g);
        dense.metrics[1] = entries(d, constant(M::from_rows({{3, 1, 0}, {1, 2, 1}, {0, 1, 3}})), g);
        dense.metrics[2] = entries(d, {F{Q{2}}}, g);
        families.push_back(Family::prepare(tri, d, dense, g));
        names.push_back("dense_metric");
        auto varying = wi;
        varying.metrics[0] = entries(d, {F{Q{1}} + t * t, F{}, F{}, F{}, F{Q{2}}, F{}, F{}, F{}, F{Q{3}}}, g);
        varying.connections[0] =
            entries(d, {(F{} - t) / (F{Q{1}} + t * t), F{}, F{}, F{}, F{}, F{}, F{}, F{}, F{}}, g);
        families.push_back(Family::prepare(tri, d, varying, g));
        names.push_back("varying_metric");
        const auto general =
            Family::prepare(owner, d, main_input(owner, d, g, false), g, JetMetricContract::General);
        families.push_back(general);
        names.push_back("general_pairing");
        core::OwnedRelationalSource hi(higher());
        auto higher_input = constants(hi, d, g);
        families.push_back(Family::prepare(hi, d, higher_input, g));
        names.push_back("higher_occurrences");
        for (std::size_t i = 0; i < families.size(); ++i)
            for (const Q &at : V{Q{1, 2}, Q{1}, Q{2}}) {
                Guard local;
                const auto actual = families[i].jet(point(d, at, local), local);
                for (std::size_t k = 1; k < actual.hodge().size(); ++k) {
                    require(actual.boundary_covariants()[k] ==
                                first_order_boundary_defect(
                                    actual.boundary_dots()[k], actual.base().boundaries()[k],
                                    actual.input().connections[k], actual.input().connections[k - 1]),
                            "old boundary helper regression");
                }
                require(actual.field_covariants()[1] ==
                            covariant_field_derivative(actual.input().field_derivatives[1],
                                                       actual.input().connections[1],
                                                       actual.base().fields()[1]),
                        "old field helper regression");
                if (dump) {
                    if (!first)
                        std::cout << ',';
                    first = false;
                    emit(names[i], families[i], actual, at, local);
                }
            }
        // Exact family closure cannot be inferred from one value and first derivative.
        const auto bad = weighted(tri, d, g, true);
        rejects([&] { (void)Family::prepare(tri, d, bad, g); }, "tangent-flat nonflat family refused");
        const auto tangent = Jet::prepare(base(tri, d, bad, p, g), JetDirection::coordinate(d.coordinate()),
                                          derivatives(bad, 1, g), g);
        require(tangent.order() == JetOrder::First, "tangent data remain first order");
        rejects([&] { (void)Family::prepare(owner, d, main_input(owner, d, g, false), g); },
                "noncompatible metric family refused");
        const auto ordinary = general.jet(p, g).moment(1, 1, g);
        require(ordinary.pairing_channel != 0 && ordinary.metric_residual_channel != 0 &&
                    ordinary.derivative == 20,
                "general metric includes pairing channel");
        require(ordinary.derivative != ordinary.derivative - ordinary.pairing_channel,
                "omitted pairing changes derivative");
        // An upper-grade tangent obstruction must be checked even when grade2 closes.
        auto high_bad = derivatives(higher_input, 1, g);
        high_bad.occurrence_derivatives[2][0] = 1;
        rejects(
            [&] {
                (void)Jet::prepare(base(hi, d, higher_input, p, g),
                                   JetDirection::declare("upper-obstruction"), high_bad, g);
            },
            "complete grade3 tangent closure");
        require(families.back().owner().source().grade({2}).incidence_count() == 3,
                "repeated/inactive occurrences retained");
        const auto higher_jet = families.back().jet(p, g);
        require(higher_jet.base().occurrences()[2].size() == 3, "realization retains inactive slots");
        // Noncommuting independent first directions on the same owned base point.
        const auto tb = base(tri, d, wi, p, g);
        auto da = derivatives(wi, 1, g), db = da;
        for (auto *v : {&da, &db}) {
            for (auto &row : v->occurrence_derivatives)
                std::fill(row.begin(), row.end(), Q{0});
            for (auto &row : v->field_derivatives)
                std::fill(row.begin(), row.end(), Q{0});
        }
        da.connections[1] = M::from_rows({{0, 1, 0}, {-1, 0, 0}, {0, 0, 0}});
        db.connections[1] = M::from_rows({{0, 0, 0}, {0, 0, 1}, {0, -1, 0}});
        const auto a = Jet::prepare(tb, JetDirection::declare("a"), da, g),
                   b = Jet::prepare(tb, JetDirection::declare("b"), db, g);
        const auto comm = a.connection_commutator(b, 1, M(3, 3), M(3, 3), g);
        require(comm.nonzeros() &&
                    comm == da.connections[1]
                                .multiply(db.connections[1])
                                .add(db.connections[1].multiply(da.connections[1]).scale(Q{-1})),
                "ordered mixed connections do not commute");
        // Explicit coherent constant frames transform both adjacent boundaries/metrics.
        const auto wjet = weighted_family.jet(point(d, 2, g), g);
        const std::vector<M> frames{M::from_rows({{1, 0, 0}, {0, 2, 0}, {0, 0, 3}}),
                                    M::from_rows({{1, 1, 0}, {0, 1, 1}, {0, 0, 1}}), M::from_rows({{2}})};
        const auto framed = wjet.reframe(frames, g);
        require(framed.base().owner().same_owner(wjet.base().owner()) &&
                    !framed.base().same_carrier(wjet.base(), 1, 1),
                "explicit basis map retains source with distinct realized carrier");
        for (std::size_t k = 0; k < 3; ++k)
            for (std::size_t n = 0; n < 5; ++n) {
                const auto old = wjet.moment(k, n, g), next = framed.moment(k, n, g);
                require(old.value == next.value && old.derivative == next.derivative,
                        "graded frame covariance includes both adjacent maps and pairing");
            }
        rejects([&] { (void)families[2].symbolic_moment(1, 2, g); },
                "dense metrics use generic exact jet action, not diagonal symbolic profile");
        // Rectangular source/target connections and full metric-adjoint derivative.
        const auto target = weighted_family.jet(p, g);
        M r = M::from_rows({{1, 2}, {0, 1}, {2, 0}}), rd = M::from_rows({{1, 0}, {0, 2}, {1, -1}});
        const auto rectangular = RectangularCovariantJet::declare(jet, 1, target, 1, r, rd, g);
        const auto adj = rectangular.adjoint(g);
        require(adj.value().rows() == 2 && adj.value().cols() == 3, "rectangular adjoint domain retained");
        (void)rectangular.apply_to_source_field(g);
        const auto foreign = family.jet(point(d, 1, g), g);
        rejects(
            [&] { (void)RectangularCovariantJet::declare(jet, 1, foreign, 1, M::identity(2), M(2, 2), g); },
            "equal-valued foreign point refused");
        // Complete changing-map affine family and differentiated projector.
        const auto map_a =
            CovariantMapFamily::declare(family, 1, family, 1, entries(d, {t, F{}, F{}, F{Q{1}}}, g), g);
        auto edited_input = jet.input();
        edited_input.field_derivatives[1][0] += 1;
        const auto edited_jet = Jet::prepare(jet.base(), jet.direction(), edited_input, g);
        require(!family.owns(edited_jet),
                "base point reuse does not grant an exact-family derivative certificate");
        rejects([&] { (void)map_a.jet(edited_jet, jet, g); },
                "stale derivative cannot bind to a certified map family");
        const auto map_b =
            CovariantMapFamily::declare(family, 1, family, 1, entries(d, {F{Q{1}}, F{}, F{}, F{Q{1}}}, g), g);
        const std::vector<F> correct_kernel{F{Q{1}}, F{}, F{}, F{Q{1}}, t, F{}, F{}, F{Q{1}}};
        const auto layer = PreparedLayerFamily::prepare(map_a, map_b, entries(d, {F{}, F{}, F{}, F{}}, g), 2,
                                                        entries(d, correct_kernel, g), g);
        const auto lj = layer.jet(p, g);
        require(layer.constraint_rank() == 2 && lj.kernel().cols() == 2 && lj.kernel_dot().get(2, 0) == 1,
                "changing-map full family derivative");
        rejects(
            [&] {
                (void)PreparedLayerFamily::prepare(map_a, map_b, entries(d, {F{}, F{}, F{}, F{}}, g), 1,
                                                   entries(d, {F{Q{1}}, F{}, t, F{}}, g), g);
            },
            "one compatible sample is not the complete family");
        auto wrong = correct_kernel;
        wrong[4] = wrong[4] + (t - F{Q{1}}) * (t - F{Q{1}});
        rejects(
            [&] {
                (void)PreparedLayerFamily::prepare(map_a, map_b, entries(d, {F{}, F{}, F{}, F{}}, g), 2,
                                                   entries(d, wrong, g), g);
            },
            "first-order agreement is not whole-family compatibility");
        const auto ja = map_a.jet(jet, jet, g), jb = map_b.jet(jet, jet, g);
        rejects([&] { (void)PreparedLayerJet::prepare(ja, jb, V(4), V(4), lj.kernel(), M(4, 2), g); },
                "omitted changing map derivative refused");
        const auto projector =
            CovariantMapFamily::declare(family, 1, family, 1, entries(d, {F{Q{1}}, t, F{}, F{}}, g), g);
        projector.certify_projector(g);
        const auto pi = projector.jet(jet, jet, g).projector_image(g);
        require(pi.derivative != M::from_rows({{1, 1}, {0, 0}}).apply(jet.input().field_derivatives[1]),
                "projector derivative includes Pdot field");
        // Original partial-function holes and rank exceptions remain hard boundaries.
        const auto hd = domain(-2, 2, g);
        auto hi_input = constants(owner, hd, g);
        for (auto &e : hi_input.occurrence_coefficients[1]) {
            const Q c = e.formal().evaluate(1);
            e = E::declare(hd, {0, c}, {0, 1}, g);
        }
        const auto holes = Family::prepare(owner, hd, hi_input, g);
        rejects<std::domain_error>([&] { (void)holes.jet(point(hd, 0, g), g); },
                                   "cancelled original hole survives differentiation");
        auto rank_input = constants(owner, hd, g);
        for (auto &e : rank_input.occurrence_coefficients[1])
            e = expression(hd, e.formal() * t, g);
        const auto ranks = Family::prepare(owner, hd, rank_input, g);
        rejects<std::domain_error>([&] { (void)ranks.jet(point(hd, 0, g), g); },
                                   "no derivative across a rank exception");
        rejects<std::domain_error>([&] { (void)point(hd, 2, g); }, "two-sided jet excludes endpoints");
        rejects([&] { (void)family.jet(point(domain(Q{1, 4}, 3, g), 1, g), g); },
                "equal coordinate names do not bind a family");
        auto missing = input;
        missing.metrics.pop_back();
        rejects([&] { (void)Family::prepare(owner, d, missing, g); }, "all grades required");
        auto limit = numeric::ExactActionLimits{};
        limit.steps = 0;
        Guard no_work(limit);
        rejects<std::length_error>([&] { (void)family.jet(p, no_work); }, "receiving work bound");
        limit = numeric::ExactActionLimits{};
        limit.entries = 0;
        Guard no_entries(limit);
        rejects<std::length_error>([&] { (void)jet.moment(1, 3, no_entries); }, "receiving retained entries");
        limit = numeric::ExactActionLimits{};
        limit.dimension = 1;
        Guard no_dimensions(limit);
        rejects<std::length_error>([&] { (void)jet.moment(1, 3, no_dimensions); }, "receiving dimensions");
        rejects<std::length_error>([&] { (void)jet.moment(1, 5, g, 4); }, "moment order budget");
        Guard measure;
        const auto measured = Jet::prepare(jet.base(), jet.direction(), jet.input(), measure);
        (void)measured;
        for (const auto cutoff : {std::size_t{1}, measure.steps() / 2, measure.steps()}) {
            std::size_t steps{};
            Guard cancel({}, [&] {
                if (++steps == cutoff)
                    throw std::runtime_error("cancel");
            });
            rejects<std::runtime_error>(
                [&] { (void)Jet::prepare(jet.base(), jet.direction(), jet.input(), cancel); },
                "early/middle/final cancellation");
        }
        auto normal = Family::prepare(owner, hd, constants(owner, hd, g), g);
        auto zero_point = point(hd, 0, g);
        auto normal_zero = normal.jet(zero_point, g);
        auto map_entries = entries(hd, {F{Q{1}}, F{}, F{}, F{Q{1}}}, g);
        map_entries[0] = E::declare(hd, {0, 1}, {0, 1}, g);
        auto map_hole = CovariantMapFamily::declare(normal, 1, normal, 1, map_entries, g);
        rejects<std::domain_error>([&] { (void)map_hole.jet(normal_zero, normal_zero, g); },
                                   "original map hole survives formal cancellation");
        core::RelationSignature empty_signature;
        core::RelationalComplexBuilder empty_builder(empty_signature);
        core::OwnedRelationalSource empty_source(empty_builder.freeze());
        auto empty_family = Family::prepare(empty_source, d, constants(empty_source, d, g), g);
        const auto empty_jet = empty_family.jet(p, g);
        require(empty_jet.moment(0, 3, g).value == 0, "declared empty carrier is preserved");
        auto low_bits = numeric::ExactActionLimits{};
        low_bits.integer_bits = 1;
        Guard tiny_bits(low_bits);
        rejects<std::length_error>([&] { (void)jet.moment(1, 1, tiny_bits); },
                                   "receiving bit limit rechecks retained source closure");
        auto exact_limit = numeric::ExactActionLimits{};
        exact_limit.entries = jet.retained_allowance() - 1;
        Guard receiving_closure(exact_limit);
        rejects<std::length_error>([&] { (void)jet.moment(1, 1, receiving_closure); },
                                   "retained family is included in the jet resource receipt");
        auto large_original = hi_input;
        large_original.occurrence_coefficients[1][0] = E::declare(hd, {0, Q{-123456}}, {0, Q{123456}}, g);
        const auto raw_family = Family::prepare(owner, hd, large_original, g);
        const auto raw_jet = raw_family.jet(point(hd, 1, g), g);
        auto original_limit = numeric::ExactActionLimits{};
        original_limit.integer_bits = 4;
        Guard original_bits(original_limit);
        rejects<std::length_error>([&] { (void)raw_jet.moment(1, 1, original_bits); },
                                   "cached jet revalidates cancelled original family coefficients");
        const auto kept = []() {
            Guard own;
            const auto d0 = domain(Q{1, 4}, 3, own);
            core::OwnedRelationalSource o(pairs());
            const auto f = Family::prepare(o, d0, main_input(o, d0, own), own);
            return f.jet(point(d0, 1, own), own);
        }();
        require(kept.moment(1, 1, g).derivative == 20, "prepared jets retain all source lifetimes");
        if (dump)
            std::cout << "]\n";
        else
            std::cout << checks
                      << " source-owned jet, family, adjoint, moment and changing-layer checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
