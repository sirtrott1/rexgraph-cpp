#include "rfc/core/builder.hpp"
#include "rfc/temporal/multitime.hpp"
#include "rfc/temporal/refinement_diagram.hpp"
#include <iostream>

using namespace rfc;
using namespace temporal;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using G = numeric::ExactActionGuard;
using Level = OwnedRefinementLevel;
using Map = PreparedRefinementMap;
using Retraction = PreparedRefinementRetraction;
using Response = PreparedRefinementResponse;
using Diagram = PreparedRefinementDiagram;
static std::size_t checks{};
static void require(bool yes, const char *why) {
    ++checks;
    if (!yes)
        throw std::runtime_error(why);
}
template <class Error = std::invalid_argument, class F> static void rejects(F f, const char *why) {
    bool bad = false;
    try {
        f();
    } catch (const Error &) {
        bad = true;
    }
    require(bad, why);
}
static core::Grade grade(std::size_t k) {
    return {static_cast<std::uint16_t>(k)};
}
static core::RelationalComplex triangle(std::size_t level, bool empty = false) {
    core::RelationSignature signature;
    auto e = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    auto f = signature.add_type("filling", core::BoundaryPolicy::DeclaredExact, 3, 5);
    core::RelationalComplexBuilder b(signature, core::SemanticScale{static_cast<std::uint16_t>(2 - level)});
    for (std::size_t i = 0; i < 3 + level; ++i)
        b.declare_witness_identity(100 + i);
    auto edge = [&](std::size_t a, std::size_t c) {
        b.add_grade_one(e, {{100 + a, true, true, Q{1}}, {100 + c, true, false, Q{1}}});
    };
    if (level == 0)
        edge(0, 1);
    else if (level == 1) {
        edge(0, 3);
        edge(3, 1);
    } else {
        edge(0, 4);
        edge(4, 3);
        edge(3, 1);
    }
    edge(1, 2);
    edge(2, 0);
    std::vector<core::HigherIncidenceInput> face;
    for (std::size_t i = 0; i < 3 + level; ++i)
        face.emplace_back(static_cast<std::uint32_t>(i), true, false, Q{1});
    b.add_higher({2}, f, face);
    if (empty)
        b.declare_grade({3});
    return b.freeze();
}
static std::vector<M> metrics(std::size_t level, Q a, Q b, Q c, bool empty = false) {
    V resistance;
    if (level == 0)
        resistance = {Q{a + b + c}, 2, 3};
    else if (level == 1)
        resistance = {Q{a + b}, c, 2, 3};
    else
        resistance = {a, b, c, 2, 3};
    M edge(resistance.size(), resistance.size());
    for (std::size_t i = 0; i < resistance.size(); ++i)
        edge.set(i, i, resistance[i]);
    std::vector<M> out{M::identity(3 + level), edge, M::identity(1)};
    if (empty)
        out.emplace_back(0, 0);
    return out;
}
static RefinementMapDeclaration forward(std::size_t level, RefinementRevision id, bool terms = false,
                                        bool empty = false) {
    RefinementMapDeclaration d{id, std::vector<geometry::LinearGradeMap>(empty ? 4 : 3)};
    d.grades[0].resize(3 + level);
    for (std::size_t i = 0; i < 3 + level; ++i)
        d.grades[0][i].push_back({static_cast<std::uint32_t>(i), 1});
    d.grades[1].resize(3 + level);
    d.grades[1][0] = {{0, 1}, {1, 1}};
    for (std::size_t i = 1; i < 3 + level; ++i)
        d.grades[1][i].push_back({static_cast<std::uint32_t>(i + 1), 1});
    d.grades[2] = {{{0, 1}}};
    if (terms) {
        d.grades[0][0].push_back({0, 0});
        d.grades[0][0].push_back({1, 2});
        d.grades[0][0].push_back({1, -2});
    }
    return d;
}
static RefinementMapDeclaration reverse(std::size_t level, Q a, Q b, RefinementRevision id,
                                        bool empty = false) {
    const Q sum = a + b, alpha = a / sum, beta = b / sum;
    RefinementMapDeclaration d{id, std::vector<geometry::LinearGradeMap>(empty ? 4 : 3)};
    d.grades[0].resize(4 + level);
    for (std::size_t i = 0; i < 3 + level; ++i)
        d.grades[0][i].push_back({static_cast<std::uint32_t>(i), 1});
    d.grades[0].back() = {{0, beta}, {static_cast<std::uint32_t>(level ? 3 : 1), alpha}};
    d.grades[1].resize(4 + level);
    d.grades[1][0] = {{0, alpha}};
    d.grades[1][1] = {{0, beta}};
    for (std::size_t i = 2; i < 4 + level; ++i)
        d.grades[1][i].push_back({static_cast<std::uint32_t>(i - 1), 1});
    d.grades[2] = {{{0, 1}}};
    return d;
}
static RefinementMapDeclaration identity(const Level &n, RefinementRevision id, Q scale = 1) {
    RefinementMapDeclaration d{id, {}};
    for (std::size_t k = 0; k < n.grades(); ++k) {
        geometry::LinearGradeMap map(n.size(k));
        for (std::size_t i = 0; i < map.size(); ++i)
            map[i].push_back({static_cast<std::uint32_t>(i), scale});
        d.grades.push_back(std::move(map));
    }
    return d;
}
static V values(std::size_t n, Q shift) {
    V v;
    for (std::size_t i = 0; i < n; ++i)
        v.push_back(Q{numeric::Integer(std::to_string(i + 1))} + shift);
    return v;
}
static std::vector<M> mixed(const Level &n) {
    std::vector<M> out;
    for (std::size_t k = 0; k < n.grades(); ++k) {
        auto s = M::identity(n.size(k));
        if (n.size(k) > 1) {
            s.set(0, 1, Q{3, 2});
            s.set(1, 1, 2);
        } else if (n.size(k) == 1)
            s.set(0, 0, 2);
        out.push_back(std::move(s));
    }
    return out;
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
        V v;
        for (std::size_t j = 0; j < m.cols(); ++j)
            v.push_back(m.get(i, j));
        vj(v);
    }
    std::cout << ']';
}
static void msj(const std::vector<M> &m) {
    std::cout << '[';
    for (std::size_t i = 0; i < m.size(); ++i) {
        if (i)
            std::cout << ',';
        std::cout << "{\"rows\":" << m[i].rows() << ",\"cols\":" << m[i].cols() << ",\"values\":";
        mj(m[i]);
        std::cout << '}';
    }
    std::cout << ']';
}
static void level_json(const Level &n) {
    std::cout << "{\"metrics\":";
    msj(n.metrics());
    std::cout << ",\"frames\":";
    msj(n.frames());
    std::cout << ",\"boundary\":";
    msj(n.boundaries());
    std::cout << ",\"adjoint\":";
    msj(n.adjoints());
    std::cout << ",\"hodge\":";
    msj(n.hodge());
    G g;
    std::cout << ",\"selectors\":[";
    bool first = true;
    for (std::size_t k = 0; k < n.grades(); ++k)
        for (std::size_t i = 0; i < n.size(k); ++i) {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << "{\"grade\":" << k << ",\"cell\":" << i << ",\"map\":";
            mj(n.source_selector({grade(k), static_cast<std::uint32_t>(i)}, g));
            std::cout << '}';
        }
    std::cout << "]}";
}
static void map_json(const Map &m, std::size_t source, std::size_t target) {
    std::cout << "{\"source\":" << source << ",\"target\":" << target << ",\"maps\":";
    msj(m.maps());
    std::cout << ",\"down\":";
    msj(m.boundary_defects());
    std::cout << ",\"metric\":";
    msj(m.metric_defects());
    std::cout << ",\"adjoint\":";
    msj(m.adjoint_defects());
    std::cout << ",\"hodge\":";
    msj(m.hodge_defects());
    std::cout << ",\"raw_terms\":[";
    for (std::size_t k = 0; k < m.declaration().grades.size(); ++k) {
        if (k)
            std::cout << ',';
        std::cout << '[';
        for (std::size_t j = 0; j < m.declaration().grades[k].size(); ++j) {
            if (j)
                std::cout << ',';
            std::cout << '[';
            const auto &c = m.declaration().grades[k][j];
            for (std::size_t p = 0; p < c.size(); ++p) {
                if (p)
                    std::cout << ',';
                std::cout << '[' << c[p].target << ',';
                qj(c[p].coefficient);
                std::cout << ']';
            }
            std::cout << ']';
        }
        std::cout << ']';
    }
    std::cout << ']';
    std::cout << ",\"hodge_positions\":[";
    for (std::size_t k = 0; k < m.hodge_positions().size(); ++k) {
        if (k)
            std::cout << ',';
        const auto &a = m.hodge_positions()[k];
        msj(std::vector<M>(a.begin(), a.end()));
    }
    std::cout << ']';
    const std::array<const char *, 3> labels{"composition_metric", "composition_adjoint",
                                             "composition_hodge"};
    const std::array<const std::vector<std::array<M, 2>> *, 3> positions{
        &m.composition_metric(), &m.composition_adjoint(), &m.composition_hodge()};
    for (std::size_t i = 0; i < 3; ++i) {
        std::cout << ",\"" << labels[i] << "\":[";
        for (std::size_t k = 0; k < positions[i]->size(); ++k) {
            if (k)
                std::cout << ',';
            const auto &a = positions[i]->at(k);
            msj(std::vector<M>(a.begin(), a.end()));
        }
        std::cout << ']';
    }
    G g;
    std::cout << ",\"sector_defects\":[";
    for (std::size_t k = 0; k < m.source().grades(); ++k) {
        if (k)
            std::cout << ',';
        std::vector<M> v;
        for (std::size_t s = 0; s < 3; ++s)
            v.push_back(m.sector_defect(s, grade(k), g));
        msj(v);
    }
    std::cout << "]}";
}
static void recipe(const char *name, Q a, Q b, Q c, bool framed, bool empty, bool dump) {
    G g;
    std::vector<Level> base;
    for (std::size_t i = 0; i < 3; ++i)
        base.push_back(Level::prepare(core::OwnedRelationalSource(triangle(i, empty)),
                                      metrics(i, a, b, c, empty), {100 + i, 1}, g));
    auto levels = base;
    if (framed)
        for (std::size_t i = 0; i < 3; ++i)
            levels[i] = base[i].reframe(mixed(base[i]), {100 + i, 2}, g);
    auto j01 = Map::prepare(levels[0], levels[1], forward(0, {10, 1}, true, empty), g);
    auto j12 = Map::prepare(levels[1], levels[2], forward(1, {11, 1}, false, empty), g);
    auto j02 = j01.then(j12, {12, 1}, g);
    auto r10 = Map::prepare(levels[1], levels[0], reverse(0, Q{a + b}, c, {20, 1}, empty), g);
    auto r21 = Map::prepare(levels[2], levels[1], reverse(1, a, b, {21, 1}, empty), g);
    auto r20 = r21.then(r10, {22, 1}, g);
    auto t01 = Retraction::prepare(j01, r10, g), t12 = Retraction::prepare(j12, r21, g),
         t02 = Retraction::prepare(j02, r20, g);
    std::vector<Map> maps{j01, j12, j02, r10, r21, r20};
    const std::array<std::pair<std::size_t, std::size_t>, 6> ends{
        {{0, 1}, {1, 2}, {0, 2}, {1, 0}, {2, 1}, {2, 0}}};
    for (const auto &m : maps) {
        require(m.native().is_chain_map(), "canonical native chain validator");
        m.require(RefinementProperty::Chain, g);
        for (const auto &d : m.boundary_defects())
            require(!d.nonzeros(), "complete all-grade chain residual");
    }
    for (const auto *m : {&j01, &j12, &j02})
        m->require(RefinementProperty::MetricIsometry, g);
    require(!j01.satisfies(RefinementProperty::AdjointIntertwining), "isometry need not intertwine adjoints");
    require(!j01.satisfies(RefinementProperty::HodgeIntertwining), "isometry need not intertwine Hodge");
    require(geometry::same_chain_map(j02.native(), geometry::compose_chain_maps(j01.native(), j12.native())),
            "composed native map agrees without deleting original histories");
    require(j01.declaration().grades[0][0].size() == 4,
            "zero/repeated/cancelling map contributions retained");
    require(j02.declaration().grades[0][0].size() == 4, "complete composed term-pair history retained");
    auto diagram = Diagram::prepare(levels, {j01, j12, j02}, {t01, t12, t02}, {40, 1}, g);
    require(diagram.paths().size() == 7,
            "complete DAG path census includes parallel composite and identities");
    std::size_t path = 0;
    for (std::size_t i = 0; i < diagram.paths().size(); ++i)
        if (diagram.paths()[i].maps.size() == 2)
            path = i;
    auto route = diagram.route(path, g);
    auto limit = diagram.limit_contract(
        "finite-refinement", "Supplied continuum comparison theorem",
        {RefinementProperty::Chain, RefinementProperty::MetricIsometry},
        {"No supplied discrete-to-continuum error bound", "No certified period/candidate identification"},
        {"Finite exact sequence; reference slope is an observation"}, g);
    require(!limit.certifies_continuum_limit() && limit.unresolved_hypotheses().size() == 2,
            "limiting metadata does not grant infinite authority");
    rejects<std::domain_error>([&] { limit.require_continuum_limit(g); },
                               "finite observations cannot certify smooth limit");
    if (dump) {
        std::cout << "{\"name\":\"" << name << "\",\"framed\":" << (framed ? "true" : "false")
                  << ",\"parameters\":[";
        qj(a);
        std::cout << ',';
        qj(b);
        std::cout << ',';
        qj(c);
        std::cout << "],\"levels\":[";
        for (std::size_t i = 0; i < 3; ++i) {
            if (i)
                std::cout << ',';
            level_json(levels[i]);
        }
        std::cout << "],\"maps\":[";
        for (std::size_t i = 0; i < maps.size(); ++i) {
            if (i)
                std::cout << ',';
            map_json(maps[i], ends[i].first, ends[i].second);
        }
        std::cout << "],\"paths\":[";
        for (std::size_t i = 0; i < diagram.paths().size(); ++i) {
            if (i)
                std::cout << ',';
            const auto &p = diagram.paths()[i];
            std::cout << "{\"source\":" << p.source << ",\"target\":" << p.target << ",\"word\":[";
            for (std::size_t j = 0; j < p.maps.size(); ++j) {
                if (j)
                    std::cout << ',';
                std::cout << p.maps[j];
            }
            std::cout << "],\"maps\":";
            msj(p.composite);
            std::cout << '}';
        }
        std::cout << "],\"grades\":[";
    }
    for (std::size_t k = 0; k < levels[0].grades(); ++k) {
        G work;
        auto old = base[0].field(grade(k), values(base[0].size(k), Q{-1, 3}), work);
        auto next = base[2].field(grade(k), values(base[2].size(k), Q{2, 5}), work);
        auto eta1 = base[1].field(grade(k), values(base[1].size(k), Q{1, 7}), work);
        auto eta2 = base[2].field(grade(k), values(base[2].size(k), Q{-2, 9}), work);
        if (framed) {
            old = levels[0].from_parent(old, work);
            next = levels[2].from_parent(next, work);
            eta1 = levels[1].from_parent(eta1, work);
            eta2 = levels[2].from_parent(eta2, work);
        }
        auto energy = j02.energy_change(old, next, work);
        energy.validate_receiving(work);
        auto split = t02.split(next, work);
        split.validate_receiving(work);
        require(split.private_field().values().size() == levels[2].size(k), "full fine carrier preserved");
        require(t02.private_frame(grade(k)).cols() == levels[2].size(k) - levels[0].size(k),
                "complete private kernel frame");
        auto accum = diagram.accumulate(route, old, {eta1, eta2}, work);
        accum.validate_receiving(work);
        require(accum.channels().size() == 3 && accum.prefixes().size() == 3,
                "full original joint innovation channels");
        auto legacy = accumulate_innovations(old.values(), {j01.maps()[k], j12.maps()[k]},
                                             {eta1.values(), eta2.values()});
        require(legacy.terminal == accum.terminal().values(), "existing native multi-time identity");
        if (k == 0) {
            auto fine = base[2].field({0}, V{0, 0, 0, 0, 1}, work);
            if (framed)
                fine = levels[2].from_parent(fine, work);
            auto private_split = t02.split(fine, work);
            require(private_split.gram().get(0, 1) != 0, "reconstruction split need not be orthogonal");
            require(!numeric::is_zero(private_split.private_field().values()),
                    "coarse cancellation cannot erase private fine field");
        }
        if (dump) {
            if (k)
                std::cout << ',';
            std::cout << "{\"grade\":" << k << ",\"old\":";
            vj(old.values());
            std::cout << ",\"next\":";
            vj(next.values());
            std::cout << ",\"energy_terms\":[";
            for (std::size_t j = 0; j < 3; ++j) {
                if (j)
                    std::cout << ',';
                qj(energy.terms()[j]);
            }
            std::cout << "],\"energy_total\":";
            qj(energy.total());
            std::cout << ",\"coarse\":";
            vj(split.coarse().values());
            std::cout << ",\"represented\":";
            vj(split.represented().values());
            std::cout << ",\"private\":";
            vj(split.private_field().values());
            std::cout << ",\"private_frame\":";
            msj({t02.private_frame(grade(k))});
            std::cout << ",\"split_gram\":";
            mj(split.gram());
            std::cout << ",\"innovations\":[";
            vj(eta1.values());
            std::cout << ',';
            vj(eta2.values());
            std::cout << "],\"prefixes\":[";
            for (std::size_t j = 0; j < accum.prefixes().size(); ++j) {
                if (j)
                    std::cout << ',';
                vj(accum.prefixes()[j].values());
            }
            std::cout << "],\"channels\":[";
            for (std::size_t j = 0; j < accum.channels().size(); ++j) {
                if (j)
                    std::cout << ',';
                vj(accum.channels()[j].values());
            }
            std::cout << "],\"gram\":";
            mj(accum.gram());
            std::cout << ",\"responses\":[";
        }
        for (std::size_t shift = 0; shift < 3; ++shift) {
            const Q lambda = shift == 0 ? Q{0} : shift == 1 ? Q{1, 2} : Q{2};
            auto green = Response::prepare(j02, grade(k), lambda, work);
            auto result = green.apply(old, next, work);
            result.validate_receiving(work);
            require(result.data().aggregate == numeric::sub(result.data().new_response,
                                                            j02.maps()[k].apply(result.data().old_response)),
                    "full original Green response identity");
            if (shift == 0) {
                green.require_intertwining(work);
                require(!green.defect().nonzeros(), "lambda0 does not certify Hodge compatibility");
            } else if (k < 2)
                require(green.defect().nonzeros() != 0, "full Green comparison retains operator defect");
            if (dump) {
                if (shift)
                    std::cout << ',';
                std::cout << "{\"lambda\":";
                qj(lambda);
                std::cout << ",\"defect\":";
                msj({green.defect()});
                std::cout << ",\"old\":";
                vj(result.data().old_response);
                std::cout << ",\"next\":";
                vj(result.data().new_response);
                std::cout << ",\"innovation\":";
                vj(result.data().coordinate_innovation);
                std::cout << ",\"innovation_term\":";
                vj(result.data().innovation_term);
                std::cout << ",\"operator_term\":";
                vj(result.data().operator_term);
                std::cout << ",\"delta\":";
                vj(result.data().aggregate);
                std::cout << '}';
            }
        }
        if (dump)
            std::cout << "]}";
    }
    if (dump)
        std::cout << "]}";
}
static core::RelationalComplex higher() {
    core::RelationSignature s;
    auto e = s.add_type("loop", core::BoundaryPolicy::DeclaredExact);
    auto f = s.add_type("cancelled face", core::BoundaryPolicy::DeclaredExact);
    auto v = s.add_type("volume", core::BoundaryPolicy::DeclaredExact);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(
        e, {{0, true, true, Q{1}}, {0, true, false, Q{1}}, {0, true, false, Q{0}}, {0, false, false, Q{3}}});
    b.add_higher({2}, f, {{0, true, false, false, 1}, {0, true, true, false, 1}});
    b.add_higher({3}, v, {{0, true, false, false, 1}});
    return b.freeze();
}
static void higher_checks() {
    G g;
    core::OwnedRelationalSource owner(higher());
    auto a =
        Level::prepare(owner, {M::identity(1), M::identity(1), M::identity(1), M::identity(1)}, {1, 1}, g);
    auto b = Level::prepare(
        owner, {M::from_rows({{2}}), M::from_rows({{3}}), M::from_rows({{5}}), M::from_rows({{7}})}, {1, 1},
        g);
    auto map = Map::prepare(a, b, identity(a, {5, 1}), g);
    require(!a.same_owner(b), "equal keys and source do not identify level declarations");
    require(map.native().is_chain_map(), "higher native chain proof");
    require(map.hodge_defects()[3].nonzeros() != 0, "last grade metric/Hodge defect retained");
    require(a.source().source().grade({1}).incidence_count() == 4, "higher source flags retained");
    auto r = Map::prepare(b, a, identity(b, {6, 1}), g);
    auto retraction = Retraction::prepare(map, r, g);
    for (std::size_t k = 0; k < 4; ++k) {
        require(retraction.private_frame(grade(k)).cols() == 0, "higher complete private frame");
        auto x = a.field(grade(k), V{2}, g), y = b.field(grade(k), V{3}, g);
        auto e = map.energy_change(x, y, g);
        require(e.total() == Q{9} * b.metrics()[k].get(0, 0) - Q{4}, "higher metric energy comparison");
    }
    auto d = Diagram::prepare({a, b}, {map}, {retraction}, {8, 1}, g);
    require(d.paths().size() == 3, "higher diagram path completeness");
}
static core::OwnedRelationalSource witnesses(std::size_t n) {
    core::RelationalComplexBuilder b(core::RelationSignature{});
    for (std::size_t i = 0; i < n; ++i)
        b.declare_witness_identity(100 + i);
    return core::OwnedRelationalSource(b.freeze());
}
static void zero_grade_checks() {
    G g;
    auto a = Level::prepare(witnesses(1), {M::from_rows({{2}})}, {91, 1}, g);
    auto b = Level::prepare(witnesses(2), {M::from_rows({{2, 0}, {0, 3}})}, {92, 1}, g);
    auto j = Map::prepare(a, b, {{93, 1}, {{{{0, 1}}}}}, g);
    auto r = Map::prepare(b, a, {{94, 1}, {{{{0, 1}}, {}}}}, g);
    auto retraction = Retraction::prepare(j, r, g);
    auto x = a.field({0}, V{2}, g), y = b.field({0}, V{2, 5}, g);
    auto energy = j.energy_change(x, y, g);
    require(energy.total() == 75, "grade-zero finite energy");
    auto split = retraction.split(y, g);
    require(split.private_field().values() == V({0, 5}), "grade-zero private direction retained");
    auto response = Response::prepare(j, {0}, Q{2}, g);
    response.require_intertwining(g);
    auto result = response.apply(x, y, g);
    require(result.data().aggregate == V({0, 5}), "zero Hodge still retains full source innovation");
    auto d = Diagram::prepare({a, b}, {j}, {retraction}, {95, 1}, g);
    require(d.paths().size() == 3, "grade-zero path census");
    auto empty = Level::prepare(witnesses(0), {M(0, 0)}, {96, 1}, g);
    auto enter = Map::prepare(empty, a, {{97, 1}, {geometry::LinearGradeMap(0)}}, g);
    auto leave = Map::prepare(a, empty, {{98, 1}, {geometry::LinearGradeMap(1)}}, g);
    auto complete = Retraction::prepare(enter, leave, g);
    require(complete.private_frame({0}) == M::identity(1),
            "empty coarse carrier retains every new fine direction");
    auto original = a.field({0}, V{2}, g);
    auto private_split = complete.split(original, g);
    require(private_split.private_field().values() == V{2} && private_split.coarse().values().empty(),
            "unobserved fine value is not zero");
}
static void frame_history_checks() {
    G g;
    auto root = Level::prepare(core::OwnedRelationalSource(triangle(0)), metrics(0, 1, 2, 3), {80, 1}, g);
    auto first = root.reframe(mixed(root), {80, 2}, g);
    std::vector<M> changes;
    for (std::size_t k = 0; k < first.grades(); ++k) {
        auto m = M::identity(first.size(k));
        if (first.size(k) > 1) {
            m.set(0, 0, 0);
            m.set(1, 1, 0);
            m.set(0, 1, 1);
            m.set(1, 0, 1);
        }
        changes.push_back(std::move(m));
    }
    auto second = first.reframe(changes, {80, 3}, g);
    auto parent = second.frame_parent();
    require(parent && parent->same_owner(first), "original frame parent revision retained");
    require(second.frame_changes() == changes,
            "relative frame declarations retained independently of accumulated frames");
    auto map = Map::prepare(first, second, identity(first, {81, 1}), g);
    map.require(RefinementProperty::MetricIsometry, g);
    map.require(RefinementProperty::AdjointIntertwining, g);
    map.require(RefinementProperty::HodgeIntertwining, g);
    for (std::size_t k = 0; k < first.grades(); ++k) {
        require(second.frames()[k] == first.frames()[k].multiply(changes[k]),
                "noncommuting frame operation order");
        auto field = root.field(grade(k), values(root.size(k), 0), g);
        auto x = first.from_parent(field, g);
        auto y = second.from_parent(x, g);
        auto image = map.push(x, g);
        require(image.values() == y.values(), "complete coordinate map agrees with retained frame history");
        auto response = Response::prepare(map, grade(k), Q{1}, g);
        response.require_intertwining(g);
    }
}
static void rejection_checks() {
    G g;
    auto a = Level::prepare(core::OwnedRelationalSource(triangle(0)), metrics(0, 1, 2, 3), {1, 1}, g);
    auto b = Level::prepare(core::OwnedRelationalSource(triangle(1)), metrics(1, 1, 2, 3), {2, 1}, g);
    auto j = Map::prepare(a, b, forward(0, {3, 1}), g);
    auto r = Map::prepare(b, a, reverse(0, 3, 3, {4, 1}), g);
    auto t = Retraction::prepare(j, r, g);
    auto d = Diagram::prepare({a, b}, {j}, {t}, {5, 1}, g);
    auto f = a.field({0}, V{1, 2, 3}, g);
    auto fine = b.field({0}, V{1, 2, 3, 4}, g);
    rejects(
        [&] {
            auto bad = forward(0, {6, 1});
            bad.grades[1][0].pop_back();
            (void)Map::prepare(a, b, bad, g);
        },
        "broken chain map refused");
    rejects(
        [&] {
            auto bad = forward(0, {6, 1});
            bad.grades.pop_back();
            (void)Map::prepare(a, b, bad, g);
        },
        "missing source grade refused");
    rejects<std::out_of_range>(
        [&] {
            auto bad = forward(0, {6, 1});
            bad.grades[0][0][0].target = 99;
            (void)Map::prepare(a, b, bad, g);
        },
        "out-of-carrier map term refused");
    rejects(
        [&] {
            auto bad = reverse(0, 3, 3, {6, 1});
            bad.grades[2][0][0].coefficient = 2;
            auto wrong = Map::prepare(b, a, bad, g);
            (void)Retraction::prepare(j, wrong, g);
        },
        "broken reconstruction refused");
    auto equal = Level::prepare(a.source(), metrics(0, 1, 2, 3), {1, 1}, g);
    rejects(
        [&] {
            auto foreign = Map::prepare(equal, b, forward(0, {7, 1}), g);
            (void)Retraction::prepare(foreign, r, g);
        },
        "foreign actual retraction endpoint refused");
    rejects([&] { (void)j.then(j, {8, 1}, g); }, "foreign middle composition refused");
    rejects([&] { (void)j.push(equal.field({0}, V{1, 2, 3}, g), g); }, "equal-source foreign field refused");
    rejects([&] { (void)j.energy_change(f, b.field({1}, V{1, 2, 3, 4}, g), g); },
            "field grades cannot be identified");
    rejects([&] { (void)t.split(f, g); }, "foreign split field refused");
    rejects([&] { (void)Diagram::prepare({a, equal}, {j}, {}, {9, 1}, g); },
            "missing actual endpoint refused");
    rejects([&] { (void)Diagram::prepare({a, a}, {}, {}, {9, 1}, g); }, "duplicate actual level refused");
    rejects([&] { (void)Diagram::prepare({a, b}, {j, r}, {}, {9, 1}, g); },
            "forward graph cycle refuses incomplete path census");
    auto id = Map::prepare(a, equal, identity(a, {10, 1}), g);
    auto neg = Map::prepare(a, equal, identity(a, {11, 1}, -1), g);
    require(id.satisfies(RefinementProperty::MetricIsometry) &&
                neg.satisfies(RefinementProperty::MetricIsometry),
            "noncommuting control maps individually isometric and strict");
    rejects([&] { (void)Diagram::prepare({a, equal}, {id, neg}, {}, {12, 1}, g); },
            "whole-matrix noncommuting parallel paths refused");
    auto wrong_metrics = metrics(1, 1, 2, 3);
    wrong_metrics[1].set(0, 0, 4);
    auto wrong = Level::prepare(b.source(), wrong_metrics, {13, 1}, g);
    auto jm = Map::prepare(a, wrong, forward(0, {14, 1}), g);
    require(!jm.satisfies(RefinementProperty::MetricIsometry),
            "broken metric is a distinct comparison defect");
    rejects([&] { jm.require(RefinementProperty::MetricIsometry, g); }, "false metric certificate refused");
    rejects([&] { j.require(RefinementProperty::AdjointIntertwining, g); },
            "isometry cannot certify adjoints");
    rejects([&] { j.require(RefinementProperty::HodgeIntertwining, g); }, "isometry cannot certify Hodge");
    rejects(
        [&] {
            auto form = metrics(0, 1, 2, 3);
            form[0].set(0, 0, -1);
            (void)Level::prepare(a.source(), form, {20, 1}, g);
        },
        "nonpositive metric refused");
    rejects(
        [&] {
            auto form = metrics(0, 1, 2, 3);
            form[0].set(0, 1, 1);
            (void)Level::prepare(a.source(), form, {20, 1}, g);
        },
        "nonsymmetric metric refused");
    rejects([&] { (void)a.reframe({M(3, 3), M::identity(3), M::identity(1)}, {21, 1}, g); },
            "singular coordinate frame refused");
    rejects([&] { (void)a.from_parent(f, g); }, "equal dimensions do not create a frame parent");
    auto framed = a.reframe(mixed(a), {22, 1}, g);
    rejects([&] { (void)framed.from_parent(equal.field({0}, V{1, 2, 3}, g), g); },
            "foreign frame parent refused");
    auto another = Diagram::prepare({a, b}, {j}, {t}, {5, 1}, g);
    auto route = another.route(1, g);
    rejects([&] { (void)d.accumulate(route, f, {}, g); }, "foreign route owner refused");
    std::size_t nonempty = 0;
    for (std::size_t i = 0; i < d.paths().size(); ++i)
        if (!d.paths()[i].maps.empty())
            nonempty = i;
    auto actual = d.route(nonempty, g);
    rejects([&] { (void)d.accumulate(actual, f, {}, g); }, "missing joint innovation refused");
    rejects([&] { (void)d.accumulate(actual, f, {a.field({0}, V{1, 2, 3}, g)}, g); },
            "foreign stage innovation refused");
    rejects(
        [&] {
            (void)d.limit_contract("x", "theorem", {RefinementProperty::HodgeIntertwining}, {"unresolved"},
                                   {}, g);
        },
        "unchecked finite limiting premise refused");
    rejects([&] { (void)d.limit_contract("x", "theorem", {}, {}, {}, g); },
            "empty infinite hypotheses cannot create proof");
    rejects(
        [&] {
            (void)d.limit_contract("x", "theorem", {static_cast<RefinementProperty>(99)}, {"unresolved"}, {},
                                   g);
        },
        "unknown finite premise refused");
    rejects([&] { (void)Response::prepare(j, {0}, Q{-1}, g); },
            "negative shift outside admitted Green profile");
    auto green = Response::prepare(j, {0}, Q{1}, g);
    rejects([&] { green.require_intertwining(g); }, "nonzero whole Green defect cannot be certified zero");
    rejects([&] { (void)green.apply(f, a.field({0}, V{1, 2, 3}, g), g); }, "foreign Green endpoint refused");
    rejects<std::out_of_range>([&] { (void)d.route(-1, g); }, "negative route refused");
    rejects<std::out_of_range>([&] { (void)d.route(100, g); }, "foreign route index refused");
    for (auto limits : {RefinementLimits{0}, RefinementLimits{4, 1}, RefinementLimits{4, 32, 1}})
        rejects<std::length_error>(
            [&] { (void)Level::prepare(a.source(), metrics(0, 1, 2, 3), {30, 1}, g, limits); },
            "original source profile caps enforced");
    RefinementLimits map_cap;
    map_cap.max_terms = 1;
    rejects<std::length_error>([&] { (void)Map::prepare(a, b, forward(0, {31, 1}), g, map_cap); },
                               "complete map term bound enforced");
    RefinementLimits shallow;
    shallow.max_depth = 1;
    auto shallow_level = Level::prepare(a.source(), metrics(0, 1, 2, 3), {36, 1}, g, shallow);
    rejects<std::length_error>([&] { (void)shallow_level.reframe(mixed(shallow_level), {36, 2}, g); },
                               "bounded frame provenance depth");
    auto shallow_map = Map::prepare(a, equal, identity(a, {37, 1}), g, shallow);
    auto back = Map::prepare(equal, a, identity(equal, {38, 1}), g, shallow);
    rejects<std::length_error>([&] { (void)shallow_map.then(back, {39, 1}, g); },
                               "complete primitive map history bound");
    RefinementLimits zero_word;
    zero_word.max_depth = 0;
    rejects<std::length_error>([&] { (void)Map::prepare(a, equal, identity(a, {39, 2}), g, zero_word); },
                               "zero map history allowance refuses a declared operation");
    RefinementLimits paths;
    paths.max_paths = 2;
    rejects<std::length_error>([&] { (void)Diagram::prepare({a, b}, {j}, {t}, {32, 1}, g, paths); },
                               "all paths precharged before construction");
    for (auto limits : {numeric::ExactActionLimits{1, 16000000, 64000000, 1u << 24},
                        {4096, 1, 64000000, 1u << 24},
                        {4096, 16000000, 0, 1u << 24},
                        {4096, 16000000, 64000000, 1}}) {
        rejects<std::length_error>(
            [&] {
                G small(limits);
                (void)Level::prepare(a.source(), metrics(0, 1, 2, 3), {33, 1}, small);
            },
            "level receiving limits enforced");
        rejects<std::length_error>(
            [&] {
                G small(limits);
                d.validate_receiving(small);
            },
            "diagram receiving limits enforced");
        rejects<std::length_error>(
            [&] {
                G small(limits);
                (void)green.apply(f, fine, small);
            },
            "response receiving limits enforced");
    }
    G count;
    auto trial = Map::prepare(a, b, forward(0, {34, 1}), count);
    (void)trial;
    for (auto cutoff : {std::size_t{1}, count.steps() / 2, count.steps()}) {
        std::size_t n{};
        G cancel({}, [&] {
            if (++n == cutoff)
                throw std::runtime_error("cancel");
        });
        rejects<std::runtime_error>([&] { (void)Map::prepare(a, b, forward(0, {34, 1}), cancel); },
                                    "early/middle/final complete-map cancellation");
        require(n == cutoff, "exact cancellation boundary");
    }
    G census;
    auto original = Diagram::prepare({a, b}, {j}, {t}, {35, 1}, census);
    (void)original;
    for (auto cutoff : {std::size_t{1}, census.steps() / 2, census.steps()}) {
        std::size_t n{};
        G cancel({}, [&] {
            if (++n == cutoff)
                throw std::runtime_error("cancel");
        });
        rejects<std::runtime_error>([&] { (void)Diagram::prepare({a, b}, {j}, {t}, {35, 1}, cancel); },
                                    "diagram construction cancellation");
        require(n == cutoff, "exact path census cancellation boundary");
    }
    auto saved = [&] {
        auto tmp = t;
        G local;
        return tmp.split(fine, local);
    }();
    saved.validate_receiving(g);
    require(saved.private_field().values().size() == 4, "result retains temporary preparation owner");
}
int main(int argc, char **argv) {
    try {
        const bool dump = argc > 1 && std::string_view(argv[1]) == "--dump";
        if (dump)
            std::cout << '[';
        recipe("integer-three-level", 1, 2, 3, false, false, dump);
        if (dump)
            std::cout << ',';
        recipe("rational-three-level", Q{2, 3}, Q{7, 5}, Q{11, 13}, false, false, dump);
        if (dump)
            std::cout << ',';
        recipe("mixed-original-cell-frames", 1, 2, 3, true, false, dump);
        if (dump)
            std::cout << ',';
        recipe("retained-empty-upper-grade", 1, 2, 3, false, true, dump);
        if (dump)
            std::cout << "]\n";
        else {
            higher_checks();
            zero_grade_checks();
            frame_history_checks();
            rejection_checks();
            std::cout << checks
                      << " owned refinement, complete diagram and finite comparison checks passed\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
