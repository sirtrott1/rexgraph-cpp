#pragma once
#include "../../examples/native_frames/fixture.hpp"
#include "rfc/inference/c_declaration_archive.hpp"
namespace c_declaration_fixtures {
using namespace rfc;
using namespace inference;
using namespace temporal;
using namespace storage;
using P = CDeclarationPayload;
using K = CDeclarationKind;
using R = CDeclarationRecord;
using G = numeric::ExactActionGuard;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
struct Recipe {
    std::string name;
    std::vector<R> records;
    std::vector<std::size_t> exports;
    std::size_t add(K kind, std::vector<std::size_t> refs = {}, std::vector<std::byte> bytes = {}) {
        auto i = records.size();
        records.push_back({kind, i + 1, 1, std::move(refs), std::move(bytes)});
        return i;
    }
    std::size_t source(const core::RelationalComplex &s) {
        return add(K::Source, {}, P{}.blob(encode_rfcx(s).bytes).take());
    }
    CDeclarationArchive prepare(G &g) const {
        return CDeclarationArchive::prepare({77, 88, 3, 99, 12, "declaration-archive exact cold replay: " + name}, records,
                                            exports, g);
    }
};
inline core::RelationalComplex witnesses(std::size_t n) {
    core::RelationSignature s;
    core::RelationalComplexBuilder b(s);
    for (std::size_t i = 0; i < n; ++i)
        b.declare_witness_identity(100 + i);
    return b.freeze();
}
inline core::RelationalComplex equations() {
    core::RelationSignature s;
    auto t = s.add_type("equation", core::BoundaryPolicy::DeclaredExact);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(t, {{0, true, false, false, 1}});
    b.add_grade_one(t, {{0, true, false, false, 1}});
    return b.freeze();
}
inline std::size_t domain(Recipe &c, bool hole = true) {
    auto x = c.add(K::Coordinate, {}, P{}.string("s").u64(0).take());
    return c.add(K::Domain, {x},
                 P{}.rational(-2)
                     .rational(2)
                     .flag(true)
                     .flag(true)
                     .vectors(hole ? std::vector<V>{{0, 1}} : std::vector<V>{})
                     .take());
}
inline std::vector<std::byte> bounds() {
    return P{}.u64(4).u64(32).u64(512).u64(1024).u64(8).u64(16).u64(256).u64(16).take();
}
inline void limits(P &p) {
    p.u64(4).u64(32).u64(512).u64(1024).u64(8).u64(16).u64(256).u64(16);
}
inline Recipe characters() {
    Recipe c{"characters", {}, {}};
    auto s = c.source(equations());
    auto chain = c.add(K::CyclicChain, {s}, P{}.u64(6).u64(0).u64(2).indices({}).indices({0, 1}).take());
    auto out = c.add(K::Characters, {chain}, P{}.u64(32).u64(256).u64(1000000).u64(4096).take());
    c.source(native_frame_example::higher());
    c.exports = {out, chain, s, s};
    return c;
}
inline std::vector<std::byte> field_archive(bool radical = false) {
    auto f = radical ? numeric::CertifiedAlgebraicField::declare(
                           {Q{-2}, Q{0}, Q{1}}, numeric::EisensteinShift{2, numeric::Integer{0}})
                     : numeric::CertifiedAlgebraicField::declare_cyclotomic(1);
    auto s = witnesses(1);
    auto bundle = local::AlgebraicStalkBundle::declare(s, f, {{1}});
    AlgebraicArchiveContents data;
    data.embedding = geometry::SelectedRealAlgebraicEmbedding::isolate(f, radical ? Q{1} : Q{0}, Q{2});
    return encode_algebraic_archive(AlgebraicArchive::declare(bundle, data));
}
inline Recipe branch() {
    Recipe c{"branch", {}, {}};
    auto b = c.add(
        K::Branch, {},
        P{}.vector(V{Q{1, 2}, Q{1, 2}}).indices({11, 12, 13, 14, 15}).indices({101, 102, 103}).u64(2).take());
    auto f = c.add(K::AlgebraicArchive, {}, P{}.blob(field_archive()).take());
    auto modes = c.add(K::BranchModes, {b, f}, P{}.u64(1).vector(V{1}).take());
    c.exports = {b, modes};
    return c;
}
inline Recipe periods() {
    Recipe c{"periods", {}, {}};
    G g;
    auto surface = field::PreparedSquareTiledSurface::prepare(
        {0}, {0}, 1, field::SquareTiledOrientation::Positive, 1, 1, g);
    auto s = c.source(surface.source());
    P p;
    p.matrix(M::identity(2)).u64(1).u64(0).rational(1).u64(surface.cup_terms().size());
    for (const auto &t : surface.cup_terms())
        p.u64(t.face).u64(t.first_slot).u64(t.second_slot).rational(t.coefficient);
    auto periods = c.add(K::PeriodConstruction, {s}, p.take());
    auto sym = c.add(K::Symplectic, {periods});
    P supplied;
    supplied.matrix(M::identity(2))
        .matrix(M::identity(2))
        .matrix(M::identity(2))
        .u64(1)
        .u64(0)
        .rational(1)
        .u64(surface.cup_terms().size());
    for (const auto &t : surface.cup_terms())
        supplied.u64(t.face).u64(t.first_slot).u64(t.second_slot).rational(t.coefficient);
    c.add(K::PeriodLattice, {s}, supplied.take());
    c.exports = {periods, sym};
    return c;
}
inline Recipe surfaces(bool radical = false) {
    Recipe c{radical ? "radical-normalization" : "surface-factors", {}, {}};
    auto s = c.add(K::Surface, {}, P{}.indices({0}).indices({0}).u64(1).u64(0).take());
    auto metric = radical ? M::from_rows({{Q{1, 2}, 0}, {0, 1}}) : M::identity(2);
    auto periods = c.add(K::SurfacePeriods, {s}, P{}.matrix(metric).take());
    auto sym = c.add(K::SurfaceSymplectic, {periods});
    auto f = c.add(K::AlgebraicArchive, {}, P{}.blob(field_archive(radical)).take());
    auto n = c.add(K::Normalization, {sym, f},
                   radical ? P{}.u64(1).vector(V{2, 0}).vector(V{0, 1}).take()
                           : P{}.u64(1).vector(V{1}).vector(V{1}).take());
    if (radical)
        c.exports = {n, periods};
    else
        c.exports = {c.add(K::EllipticFactors, {n}), n, periods};
    return c;
}
inline Recipe univariate() {
    Recipe c{"univariate", {}, {}};
    auto d = domain(c, false);
    auto e = c.add(K::Expression, {d}, P{}.vector(V{0, 1}).vector(V{0, 1}).take());
    auto row = c.add(K::Carrier, {}, P{}.string("rows").u64(1).u64(0).take());
    auto col = c.add(K::Carrier, {}, P{}.string("columns").u64(1).u64(0).take());
    auto family = c.add(K::UnivariateFamily, {d, row, col, e});
    c.exports = {family, e, d};
    return c;
}
inline Recipe jets() {
    Recipe c{"jets-and-words", {}, {}};
    auto s = c.source(equations());
    auto d = domain(c);
    auto point = c.add(K::JetPoint, {d}, P{}.rational(1).take());
    auto direction = c.add(K::JetDirection, {0 + 1});
    auto base = c.add(K::JetBase, {s, point},
                      P{}.vectors({{}, V{1, 1}})
                          .matrices({M::identity(1), M::identity(2)})
                          .vectors({V{3}, V{0, 0}})
                          .take());
    auto jet = c.add(K::Jet, {base, direction},
                     P{}.vectors({{}, V{0, 0}})
                         .vectors({V{5}, V{0, 0}})
                         .matrices({M(1, 1), M(2, 2)})
                         .matrices({M(1, 1), M(2, 2)})
                         .flag(false)
                         .take());
    auto rect = c.add(K::RectangularJet, {jet, jet},
                      P{}.u64(0).u64(0).matrix(M::from_rows({{2}})).matrix(M::from_rows({{1}})).take());
    auto response = c.add(K::Response, {rect}, P{}.matrix(M(1, 0)).matrix(M(1, 0)).take());
    auto f = c.add(K::DerivativeField, {jet}, P{}.u64(0).vector(V{3}).vector(V{5}).take());
    auto result = c.add(K::ResponseApply, {response, f});
    auto green = c.add(K::GreenResponse, {jet}, P{}.u64(0).rational(1).rational(1).take());
    auto gres = c.add(K::ResponseApply, {green, f});
    auto h = c.add(K::HodgeAction, {jet}, P{}.u64(0).take());
    auto inverse = c.add(K::InverseAction, {green});
    auto word = c.add(K::DerivativeWord, {jet, h, inverse}, P{}.u64(0).take());
    auto wr = c.add(K::WordApply, {word, f});
    c.add(K::MapAction, {rect});
    c.add(K::CrossPairing, {jet, jet}, P{}.u64(0).u64(0).matrix(M::identity(1)).matrix(M(1, 1)).take());
    c.add(K::JetReframe, {base}, P{}.matrices({M::from_rows({{2}}), M::from_rows({{1, 1}, {0, 1}})}).take());
    c.exports = {result, gres, wr, jet};
    return c;
}
inline Recipe rational_family() {
    Recipe c{"rational-family", {}, {}};
    auto s = c.source(equations());
    auto d = domain(c);
    auto one = c.add(K::Expression, {d}, P{}.vector(V{0, 1}).vector(V{0, 1}).take());
    auto zero = c.add(K::Expression, {d}, P{}.vector(V{0}).vector(V{1}).take());
    auto point = c.add(K::JetPoint, {d}, P{}.rational(1).take());
    std::vector<std::size_t> refs{s,    d,    one,  one,  one,  one, zero, zero, one,
                                  zero, zero, zero, zero, zero, one, one,  one};
    P p;
    p.u64(2)
        .u64(0)
        .u64(2)
        .u64(2)
        .u64(1)
        .u64(4)
        .u64(2)
        .u64(1)
        .u64(4)
        .u64(2)
        .u64(1)
        .u64(2)
        .flag(false)
        .u64(8)
        .u64(8)
        .u64(10000)
        .u64(4096);
    auto family = c.add(K::CovariantFamily, refs, p.take());
    auto jet = c.add(K::FamilyJet, {family, point});
    auto map = c.add(K::FamilyMap, {family, family, one}, P{}.u64(0).u64(0).take());
    c.add(K::FamilyMapJet, {map, jet, jet});
    c.add(K::FamilyAction, {map, jet, jet});
    auto response = c.add(K::FamilyResponse, {map, jet, jet}, P{}.matrix(M(1, 0)).matrix(M(1, 0)).take());
    auto layer = c.add(K::LayerFamily, {map, map, zero, zero, one, one}, P{}.u64(2).u64(1).take());
    c.add(K::LayerFamilyJet, {layer, point});
    c.exports = {family, jet, response};
    return c;
}
inline Recipe holonomy() {
    Recipe c{"holonomy", {}, {}};
    auto s = c.add(K::Surface, {}, P{}.indices({0}).indices({0}).u64(1).u64(0).take());
    auto chi = c.add(K::SurfaceCocycle, {s}, P{}.vector(V{1, 0}).take());
    auto d = domain(c, false);
    auto h = c.add(K::SurfaceHolonomy, {s, chi, d});
    auto inherited = c.add(K::SurfaceInheritance, {h});
    c.exports = {inherited, h};
    return c;
}
inline Recipe rotating() {
    Recipe c{"rotating", {}, {}};
    auto s = c.source(equations());
    auto system =
        c.add(K::RotatingSystem, {s}, P{}.vector(V{1, 1}).matrices({M::identity(1), M::identity(2)}).take());
    auto d = domain(c, false);
    auto rot = c.add(K::RotatingCoefficient, {system, d}, P{}.u64(0).u64(0).take());
    c.exports = {rot, system};
    return c;
}
inline Recipe refinement() {
    Recipe c{"refinement", {}, {}};
    auto s = c.source(witnesses(1)), t = c.source(witnesses(2));
    P a;
    limits(a);
    auto bnd = a.take();
    auto level = [&](std::size_t src, const M &metric) {
        P p;
        p.matrices({metric});
        limits(p);
        return c.add(K::RefinementLevel, {src}, p.take());
    };
    auto l = level(s, M::from_rows({{2}})), m = level(t, M::from_rows({{2, 0}, {0, 3}}));
    auto map = [&](bool reverse) {
        P p;
        limits(p);
        p.u64(1).u64(reverse ? 2 : 1).u64(reverse ? 1 : 4).u64(0).rational(1);
        if (!reverse)
            p.u64(1).rational(0).u64(1).rational(2).u64(1).rational(-2);
        if (reverse)
            p.u64(0);
        return c.add(K::RefinementMap,
                     reverse ? std::vector<std::size_t>{m, l} : std::vector<std::size_t>{l, m}, p.take());
    };
    auto j = map(false), r = map(true);
    auto retract = c.add(K::RefinementRetraction, {j, r});
    P dp;
    limits(dp);
    dp.u64(2).u64(1);
    auto diagram = c.add(K::RefinementDiagram, {l, m, j, retract}, dp.take());
    auto x = c.add(K::RefinementField, {l}, P{}.u64(0).vector(V{2}).take());
    auto y = c.add(K::RefinementField, {m}, P{}.u64(0).vector(V{2, 5}).take());
    auto eta = c.add(K::RefinementField, {m}, P{}.u64(0).vector(V{0, 5}).take());
    auto energy = c.add(K::RefinementEnergy, {j, x, y});
    auto split = c.add(K::RefinementSplit, {retract, y});
    auto response = c.add(K::RefinementResponse, {j}, P{}.u64(0).rational(2).take());
    auto apply = c.add(K::RefinementResponseApply, {response, x, y});
    auto accumulation = c.add(K::RefinementAccumulate, {diagram, x, eta}, P{}.u64(1).take());
    auto contract = c.add(K::LimitingContract, {diagram},
                          P{}.string("finite example")
                              .string("external limiting theorem text")
                              .indices({0, 1})
                              .u64(1)
                              .string("uniform infinite approximation hypothesis remains unresolved")
                              .u64(1)
                              .string("finite samples have no rate certificate")
                              .take());
    auto framed = c.add(K::RefinementReframe, {l}, P{}.matrices({M::from_rows({{2}})}).take());
    P ip;
    limits(ip);
    ip.u64(1).u64(1).u64(1).u64(0).rational(1);
    auto ident = c.add(K::RefinementMap, {l, framed}, ip.take());
    c.add(K::RefinementCompose, {ident, c.add(K::RefinementMap, {framed, l}, ip.take())});
    c.exports = {energy, split, apply, accumulation, contract, diagram};
    return c;
}
inline Recipe orbit() {
    Recipe c{"orbit", {}, {}};
    auto cx = native_frame_example::pair();
    auto s = c.source(cx);
    auto endpoint = native_frame_example::endpoint(cx, 0);
    endpoint.tensors.push_back(native_update_example::field(9000, 10, V{1, 0}));
    auto p = c.add(K::NativeEndpoint, {s}, P{}.u64(900).blob(encode_temporal_endpoint(endpoint)).take());
    auto family =
        c.add(K::NativeOperatorFamily, {p}, P{}.u64(9000).u64(0).indices({10, 11}).indices({}).take());
    auto field = c.add(K::NativeField, {p}, P{}.u64(9000).take());
    auto krylov =
        c.add(K::Krylov, {family, field},
              P{}.u64(static_cast<std::uint64_t>(field::OperatorKind::Hodge)).u64(0).rational(0).take());
    auto out = c.add(K::CayleyOrbit, {family, field}, P{}.rational(1).take());
    c.exports = {krylov, out, field};
    return c;
}
inline std::vector<Recipe> recipes() {
    return {characters(), branch(),          periods(),  surfaces(), surfaces(true), univariate(),
            jets(),       rational_family(), holonomy(), rotating(), refinement(),   orbit()};
}
} // namespace c_declaration_fixtures
