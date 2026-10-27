#include "rfc/core/builder.hpp"
#include "rfc/inference/relational_finite_family_gluing.hpp"
#include "rfc/inference/relational_finite_source_codec.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using namespace inference;
using Id = std::uint64_t;
using Source = OwnedRelationalFiniteSource;
using Family = OwnedRelationalFiniteFamily;
using Tuple = std::vector<Id>;
using Status = RelationalFiniteGluingStatus;
std::size_t checks{};
void check(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "invalid finite gluing accepted");
}
core::CellId cell(std::uint32_t i) {
    return {{0}, i};
}
core::RelationSignature signature() {
    core::RelationSignature s;
    s.add_type("original pair and repeated flags", core::BoundaryPolicy::DeclaredExact, 0, 8);
    return s;
}
core::RelationalComplex complex(unsigned side) {
    core::RelationalComplexBuilder b(signature());
    b.declare_grade({1});
    if (side == 0)
        b.declare_witness_identity(42);
    if (side == 1 || side == 3) {
        b.declare_witness_identity(11);
        b.declare_witness_identity(42);
        b.add_grade_one(
            core::RelationKey{50}, 0,
            {{11, true, true, 1}, {42, true, false, 1}, {11, false, true, 2}, {42, true, false, 0}});
    }
    if (side == 2) {
        b.declare_witness_identity(42);
        b.declare_witness_identity(99);
    }
    if (side == 2 || side == 3) {
        b.declare_witness_identity(99);
        b.add_grade_one(core::RelationKey{51}, 0, {{42, true, true, 1}, {99, true, false, 1}});
    }
    return b.freeze();
}
Source source(unsigned side, bool extra = false) {
    const auto c = complex(side);
    storage::NativeTemporalSeries h(storage::TemporalEndpoint(c, 1), 27100 + side);
    RelationalInputUseDeclaration a;
    a.key = 1;
    a.channel = 1;
    a.encoding = "UTF-8";
    a.bytes = {std::byte{'x'}};
    RelationalSpanUse u;
    u.key = 1;
    u.spans = {{0, 1}};
    for (std::size_t g = 0; g < c.grade_count(); ++g) {
        const core::Grade grade{static_cast<std::uint16_t>(g)};
        const auto &ar = c.grade(grade);
        for (std::uint32_t i = 0; i < ar.cell_count(); ++i)
            u.cells.push_back(storage::stable_cell_key(c, {grade, i}));
        for (std::size_t i = 0; i < ar.incidence_count(); ++i)
            u.occurrences.push_back({grade, c.occurrence_identity(grade, i)});
    }
    a.uses = {u};
    const Id base = side == 1 ? 101 : (side == 2 ? 201 : 501);
    RelationalFiniteSourceDeclaration d;
    d.key = 27;
    d.revision = 1;
    d.symbols = {{base, {std::byte{'a'}}}, {base + 1, {std::byte{'b'}}}};
    const auto n = c.grade({0}).cell_count();
    for (std::uint32_t i = 0; i < n; ++i) {
        d.variables.push_back({cell(i), {base, base + 1}});
        d.bindings.push_back({i + 1, 1, cell(i)});
    }
    if (side == 1 || side == 3)
        d.charts.push_back({1,
                            "left equality",
                            {cell(0), cell(1)},
                            {cell(0), cell(1)},
                            {{base, base}, {base + 1, base + 1}}});
    if (side == 2)
        d.charts.push_back({2,
                            "right inequality",
                            {cell(0), cell(1)},
                            {cell(0), cell(1)},
                            {{base, base + 1}, {base + 1, base}}});
    if (side == 3) {
        d.charts.push_back({2,
                            "right inequality",
                            {cell(1), cell(2)},
                            {cell(1), cell(2)},
                            {{base, base + 1}, {base + 1, base}}});
        if (extra)
            d.charts.push_back({3, "additional original global observation", {cell(0)}, {cell(0)}, {{base}}});
    }
    return {OwnedRelationalInputUseAsset(h, a), d};
}
algebra::RelationalGluingDeclaration maps() {
    algebra::RelationalGluingDeclaration d;
    d.key = 27;
    d.revision = 1;
    d.common_left.cells = {{1}, {}};
    d.common_left.occurrences = {{}, {}};
    d.common_right.cells = {{0}, {}};
    d.common_right.occurrences = {{}, {}};
    d.left_glued.cells = {{0, 1}, {0}};
    d.left_glued.occurrences = {{}, {0, 1, 2, 3}};
    d.right_glued.cells = {{1, 2}, {1}};
    d.right_glued.occurrences = {{}, {4, 5}};
    return d;
}
RelationalFiniteGluingSymbols symbols() {
    return {27, 1, {{501, 502}, {501, 502}}, {{501, 502}, {501, 502}}};
}
Family family(Source s) {
    RelationalFiniteProjectionDeclaration d;
    d.key = 1;
    d.revision = 1;
    d.cells = {cell(0), cell(1)};
    return Family::projection(std::move(s), d);
}
RelationalFiniteFamilyExtension extension(const Family &f, const Tuple &tuple) {
    const auto r = f.extend(tuple);
    if (!r.extension)
        throw std::runtime_error("fixture has no extension");
    return *r.extension;
}
void run() {
    const auto l = source(1), r = source(2), g = source(3);
    const auto lp = encode_relational_finite_source(l), rp = encode_relational_finite_source(r),
               gp = encode_relational_finite_source(g);
    const auto glue = OwnedRelationalFiniteGluing::prepare(complex(0), l, r, g, maps(), symbols());
    auto moved_symbols = symbols();
    auto *moved_values = moved_symbols.left[0].data();
    bool mutated = false;
    const auto frozen =
        OwnedRelationalFiniteGluing::prepare(complex(0), l, r, g, maps(), std::move(moved_symbols), {}, [&] {
            if (!mutated) {
                moved_values[0] = 999;
                mutated = true;
            }
        });
    check(frozen.symbols().left[0] == Tuple{501, 502}, "moved symbol-map alias changed finite gluing");
    const auto lf = family(l), rf = family(r);
    const auto la = extension(lf, {101, 101}), lb = extension(lf, {102, 102});
    const auto ra = extension(rf, {201, 202}), rb = extension(rf, {202, 201});
    const auto good = glue.glue(la, 0, ra, 0);
    check(good.status == Status::Compatible && good.section.has_value(),
          "compatible whole sections did not glue");
    check(good.section->field().at(cell(0)) == 501 && good.section->field().at(cell(1)) == 501 &&
              good.section->field().at(cell(2)) == 502,
          "shared value averaged or original maps lost");
    check(good.section->gluing().global().same_owner(g) &&
              good.section->left().family().source(0).same_owner(l),
          "glued result lost actual endpoints");
    check(glue.glue(la, 0, rb, 0).status == Status::Incompatible, "incompatible pair accepted");
    check(glue.glue(lb, 0, rb, 0).status == Status::Compatible,
          "incompatible pair was mistaken for global UNSAT");
    const auto stricter =
        OwnedRelationalFiniteGluing::prepare(complex(0), l, r, source(3, true), maps(), symbols());
    const auto refuted = stricter.glue(lb, 0, rb, 0);
    check(refuted.status == Status::RefutedByGlobalSource && !refuted.section,
          "original global predicate was bypassed");
    const auto foreign = family(decode_relational_finite_source(lp, {}));
    const auto fake = extension(foreign, {101, 101});
    rejects([&] { (void)glue.glue(fake, 0, ra, 0); });
    auto bad = symbols();
    auto extra_declaration = g.declaration();
    ++extra_declaration.revision;
    extra_declaration.variables.push_back({{{1}, 0}, {501, 502}});
    extra_declaration.bindings.push_back({4, 1, {{1}, 0}});
    const Source uncovered(g.asset(), extra_declaration);
    rejects(
        [&] { (void)OwnedRelationalFiniteGluing::prepare(complex(0), l, r, uncovered, maps(), symbols()); });
    auto partial_declaration = r.declaration();
    ++partial_declaration.revision;
    partial_declaration.variables.erase(partial_declaration.variables.begin());
    partial_declaration.bindings.erase(partial_declaration.bindings.begin());
    partial_declaration.charts.clear();
    const Source partial(r.asset(), partial_declaration);
    auto partial_symbols = symbols();
    partial_symbols.right.erase(partial_symbols.right.begin());
    rejects([&] {
        (void)OwnedRelationalFiniteGluing::prepare(complex(0), l, partial, g, maps(), partial_symbols);
    });
    bad.left[0] = {501};
    rejects([&] { (void)OwnedRelationalFiniteGluing::prepare(complex(0), l, r, g, maps(), bad); });
    bad = symbols();
    bad.right[0] = {501, 501};
    rejects([&] { (void)OwnedRelationalFiniteGluing::prepare(complex(0), l, r, g, maps(), bad); });
    auto wrong = maps();
    wrong.right_glued.cells[0] = {0, 2};
    rejects([&] { (void)OwnedRelationalFiniteGluing::prepare(complex(0), l, r, g, wrong, symbols()); });
    auto limits = RelationalFiniteFamilyLimits{};
    limits.max_steps = good.work.steps - 1;
    check(glue.glue(la, 0, ra, 0, limits).status == Status::BudgetExceeded,
          "global checking reset work budget");
    limits = {};
    limits.max_entries = good.work.peak_entries - 1;
    check(glue.glue(la, 0, ra, 0, limits).status == Status::BudgetExceeded,
          "gluing memory admission ignored");
    std::stop_source stop;
    stop.request_stop();
    check(glue.glue(la, 0, ra, 0, {}, stop.get_token()).status == Status::Cancelled,
          "gluing cancellation ignored");
    const auto retained = [&] {
        const auto local = OwnedRelationalFiniteGluing::prepare(complex(0), l, r, g, maps(), symbols());
        return local.glue(la, 0, ra, 0);
    }();
    check(retained.section->field() == good.section->field(), "glued section borrows destroyed factory");
    std::vector<std::future<bool>> readers;
    for (unsigned i = 0; i < 8; ++i)
        readers.push_back(std::async(std::launch::async, [glue, la, ra, good] {
            const auto out = glue.glue(la, 0, ra, 0);
            return out.section && out.section->field() == good.section->field();
        }));
    for (auto &reader : readers)
        check(reader.get(), "concurrent gluing shared scratch");
    const auto cl = decode_relational_finite_source(lp, {}), cr = decode_relational_finite_source(rp, {}),
               cg = decode_relational_finite_source(gp, {});
    const auto cold = OwnedRelationalFiniteGluing::prepare(complex(0), cl, cr, cg, maps(), symbols());
    const auto cle = extension(family(cl), {101, 101}), cre = extension(family(cr), {201, 202});
    check(cold.glue(cle, 0, cre, 0).section->field() == good.section->field(),
          "cold full-source recipe changed gluing");
    check(encode_relational_finite_source(l) == lp && encode_relational_finite_source(r) == rp &&
              encode_relational_finite_source(g) == gp,
          "gluing mutated source bytes/repeated/zero/inactive flags");
}
} // namespace
int main() {
    try {
        run();
        std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
