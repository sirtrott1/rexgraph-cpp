#include "rfc/core/builder.hpp"
#include "rfc/inference/relational_finite_family.hpp"
#include "rfc/inference/relational_finite_source_codec.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using namespace inference;
using Family = OwnedRelationalFiniteFamily;
using Source = OwnedRelationalFiniteSource;
using Id = std::uint64_t;
using Tuple = std::vector<Id>;
using Status = local::FiniteSearchStatus;
std::size_t checks{};
core::CellId cell(std::uint32_t i) {
    return {{0}, i};
}
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
    check(rejected, "invalid declaration accepted");
}
Source source(unsigned mask, Id base = 101) {
    core::RelationSignature signature;
    const auto type =
        signature.add_type("retained tuple relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{50}, type,
                    {{1, true, true, 1}, {2, true, false, 1}, {1, false, true, 2}, {2, true, false, 0}});
    const auto complex = b.freeze();
    storage::NativeTemporalSeries h(storage::TemporalEndpoint(complex, 1), 27001 + base);
    RelationalInputUseDeclaration a;
    a.key = 1;
    a.channel = 1;
    a.encoding = "UTF-8";
    a.bytes = {std::byte{'a'}, std::byte{'a'}};
    RelationalSpanUse u;
    u.key = 1;
    u.spans = {{0, 2}};
    u.cells = {storage::stable_cell_key(complex, cell(0)), storage::stable_cell_key(complex, cell(1)),
               storage::stable_cell_key(complex, {{1}, 0})};
    for (std::size_t i = 0; i < 4; ++i)
        u.occurrences.push_back({{1}, complex.occurrence_identity({1}, i)});
    a.uses = {u};
    RelationalFiniteSourceDeclaration d;
    d.key = 27;
    d.revision = 1;
    d.symbols = {{base, {std::byte{'x'}}}, {base + 1, {std::byte{'x'}}}};
    d.variables = {{cell(0), {base, base + 1}}, {cell(1), {base, base + 1}}};
    d.bindings = {{1, 1, cell(0)}, {2, 1, cell(1)}};
    std::vector<Tuple> rows;
    for (unsigned i = 0; i < 4; ++i)
        if ((mask >> i) & 1u)
            rows.push_back({base + (i / 2), base + (i % 2)});
    d.charts = {{1, "whole tuple", {cell(0), cell(1), {{1}, 0}}, {cell(0), cell(1)}, rows}};
    return {OwnedRelationalInputUseAsset(h, a), d};
}
RelationalFiniteProjectionDeclaration declaration(std::vector<core::CellId> cells) {
    RelationalFiniteProjectionDeclaration d;
    d.key = 1;
    d.revision = 1;
    d.cells = std::move(cells);
    return d;
}
std::set<Tuple> tuples(const RelationalFiniteImageResult &r) {
    check(r.status == RelationalFiniteImageStatus::Complete && r.image.has_value(), "image did not complete");
    std::set<Tuple> result;
    for (std::size_t i = 0; i < r.image->size(); ++i) {
        const auto &row = r.image->row(i);
        check(result.insert(row.tuple()).second, "duplicate image row");
        for (std::size_t leaf = 0; leaf < row.leaf_count(); ++leaf) {
            const auto &s = row.family().source(leaf);
            const auto &values = row.witness(leaf);
            check(values.size() == s.declaration().variables.size(), "partial whole witness");
            for (const auto &chart : s.declaration().charts) {
                Tuple value;
                for (const auto c : chart.support)
                    value.push_back(values.at(c));
                check(std::find(chart.allowed.begin(), chart.allowed.end(), value) != chart.allowed.end(),
                      "original predicate failed");
            }
        }
    }
    return result;
}
void exhaustive() {
    const std::vector<std::vector<core::CellId>> ports{
        {}, {cell(0)}, {cell(1)}, {cell(0), cell(1)}, {cell(1), cell(0)}};
    for (unsigned mask = 1; mask < 16; ++mask)
        for (const auto &cells : ports)
            for (bool fixed : {false, true}) {
                const auto s = source(mask);
                auto d = declaration(cells);
                if (fixed)
                    d.fixed = {{cell(0), 101}};
                const auto f = Family::projection(s, d);
                const auto image = f.materialize();
                std::set<Tuple> expected;
                for (unsigned value = 0; value < 4; ++value)
                    if (((mask >> value) & 1u) && (!fixed || value < 2)) {
                        Tuple tuple;
                        for (const auto c : cells)
                            tuple.push_back(101 + (c.index == 0 ? value / 2 : value % 2));
                        expected.insert(tuple);
                    }
                check(tuples(image) == expected, "whole correlated image differs from exhaustive reference");
                check(image.work.steps >= image.work.native.steps &&
                          image.work.completed_queries == image.queries.size(),
                      "action receipts omit work");
            }
    for (unsigned a = 1; a < 16; ++a)
        for (unsigned b = 1; b < 16; ++b)
            for (bool swap : {false, true}) {
                const auto left = Family::projection(source(a), declaration({cell(0), cell(1)}));
                const auto right = Family::projection(source(b, 201), declaration({cell(0), cell(1)}));
                RelationalFiniteInterfaceBijection map{2,
                                                       1,
                                                       swap ? std::vector<std::size_t>{1, 0}
                                                            : std::vector<std::size_t>{0, 1},
                                                       {{202, 201}, {201, 202}}};
                const auto both = Family::intersect_images(left, right, map);
                std::set<Tuple> expected;
                for (unsigned i = 0; i < 4; ++i)
                    if ((a >> i) & 1u) {
                        const unsigned x = i / 2, y = i % 2;
                        const auto r = swap ? 2 * y + (1 - x) : 2 * (1 - x) + y;
                        if ((b >> r) & 1u)
                            expected.insert({101 + x, 101 + y});
                    }
                check(tuples(both.materialize()) == expected,
                      "explicit carrier/port comparison changed intersection");
                check(both.parent(false).same_owner(left) && both.parent(true).same_owner(right),
                      "intersection discarded actual parent lineage");
            }
}
void controls() {
    const auto s = source(9);
    const auto packet = encode_relational_finite_source(s);
    const auto f = Family::projection(s, declaration({cell(0), cell(1)}));
    auto moved = declaration({cell(0), cell(1)});
    auto *moved_cells = moved.cells.data();
    bool changed = false;
    const auto frozen = Family::projection(s, std::move(moved), {}, [&] {
        if (!changed) {
            moved_cells[0] = cell(9);
            changed = true;
        }
    });
    check(frozen.cells() == std::vector<core::CellId>{cell(0), cell(1)},
          "moved input aliases changed the owned interface during a checkpoint");
    const auto image = f.materialize();
    const auto preparation = s.prepare_global_section();
    const auto cached = Family::projection(*preparation.prepared, declaration({cell(0), cell(1)}));
    const auto cached_image = cached.materialize();
    check(tuples(cached_image) == tuples(image) && cached_image.work.preparations == 0 &&
              cached_image.work.reused_preparations == 1 &&
              cached_image.work.prior_preparation_steps == preparation.work.steps,
          "prepared source was recompiled or prior work hidden");
    auto wrong_order = declaration({cell(0)});
    wrong_order.distinct_relations = {{{1}, 0}};
    rejects([&] { (void)Family::projection(*preparation.prepared, wrong_order); });
    check(tuples(image) == std::set<Tuple>{{101, 101}, {102, 102}},
          "joint equality became Cartesian marginals");
    check(image.image->row(0).family().source(0).same_owner(s), "image lost actual source owner");
    auto d = declaration({cell(0), cell(1)});
    d.window = std::vector<Tuple>{{101}, {102}};
    check(tuples(Family::projection(s, d).materialize()).empty(), "complete empty correlated image rejected");
    d.window = std::vector<Tuple>{{}, {101, 102}};
    const auto empty = Family::projection(s, d).materialize();
    check(tuples(empty).empty() && empty.work.source_queries == 0,
          "explicit empty window became missing coverage");
    d.window = std::vector<Tuple>{{102}, {101, 102}};
    check(tuples(Family::projection(s, d).materialize()) == std::set<Tuple>{{102, 102}},
          "windowed image was not retained");
    RelationalFiniteInterfaceBijection identity{3, 1, {0, 1}, {{101, 102}, {101, 102}}};
    auto moved_comparison = identity;
    auto *moved_symbols = moved_comparison.right_symbols[0].data();
    bool changed_comparison = false;
    const auto frozen_comparison = Family::intersect_images(f, f, std::move(moved_comparison), {}, [&] {
        if (!changed_comparison) {
            moved_symbols[0] = 999;
            changed_comparison = true;
        }
    });
    check(tuples(frozen_comparison.materialize()) == tuples(image),
          "moved symbol-map alias changed the actual declared comparison");
    const auto unequal = Family::projection(source(6), declaration({cell(0), cell(1)}));
    check(tuples(Family::intersect_images(f, unequal, identity).materialize()).empty(),
          "same unary images erased joint contradiction");
    const auto nested = Family::intersect_images(unequal, unequal, identity);
    const auto nested_image = nested.materialize();
    check(nested_image.work.preparations == 1 && nested_image.work.reused_preparations == 1 &&
              nested_image.work.prior_preparation_steps == 0,
          "same actual source/group index was redundantly compiled");
    check(tuples(Family::intersect_images(f, nested, identity).materialize()).empty(),
          "nested parent comparisons lost correlations");
    auto invalid = identity;
    invalid.right_positions = {0, 0};
    rejects([&] { (void)Family::intersect_images(f, unequal, invalid); });
    invalid = identity;
    invalid.right_symbols[0] = {101, 101};
    rejects([&] { (void)Family::intersect_images(f, unequal, invalid); });
    invalid = identity;
    invalid.right_symbols[0] = {101, 999};
    rejects([&] { (void)Family::intersect_images(f, unequal, invalid); });
    d = declaration({cell(0), cell(0)});
    rejects([&] { (void)Family::projection(s, d); });
    d = declaration({cell(9)});
    rejects([&] { (void)Family::projection(s, d); });
    d = declaration({cell(0)});
    d.window = std::vector<Tuple>{{101, 101}};
    rejects([&] { (void)Family::projection(s, d); });
    d = declaration({cell(0)});
    d.fixed = {{cell(1), 999}};
    rejects([&] { (void)Family::projection(s, d); });
    rejects([&] { (void)f.extend(Tuple{101}); });
    rejects([&] { (void)f.extend(Tuple{101, 999}); });
    const auto sat = f.extend(Tuple{102, 102});
    check(sat.status == Status::Satisfiable && sat.extension.has_value(), "bounded tuple extension");
    auto limit = RelationalFiniteFamilyLimits{};
    limit.max_candidates = 3;
    const auto partial = f.materialize(limit);
    check(partial.status == RelationalFiniteImageStatus::BudgetExceeded && !partial.image &&
              partial.work.completed_queries == 3,
          "candidate budget published incomplete image");
    limit = {};
    limit.max_rows = 1;
    check(!f.materialize(limit).image, "row budget published truncated family");
    limit = {};
    limit.max_steps = image.work.steps - 1;
    const auto steps = f.materialize(limit);
    check(steps.status == RelationalFiniteImageStatus::BudgetExceeded && !steps.image &&
              steps.work.steps <= limit.max_steps,
          "work budget reset per tuple");
    limit = {};
    limit.max_entries = image.work.peak_entries - 1;
    check(!f.materialize(limit).image, "entry envelope ignored");
    limit = {};
    limit.max_steps = 0;
    check(f.extend(Tuple{101, 101}, limit).status == Status::BudgetExceeded, "zero work limit ignored");
    std::stop_source stop;
    stop.request_stop();
    const auto cancelled = f.materialize({}, stop.get_token());
    check(cancelled.status == RelationalFiniteImageStatus::Cancelled && !cancelled.image,
          "cancelled family published");
    check(f.extend(Tuple{101, 101}, {}, stop.get_token()).status == Status::Cancelled,
          "cancelled extension published");
    const auto kept = [] {
        return Family::projection(source(9), declaration({cell(0), cell(1)})).materialize();
    }();
    check(tuples(kept) == tuples(image), "image borrows destroyed projection/source");
    std::vector<std::future<std::set<Tuple>>> readers;
    for (unsigned i = 0; i < 8; ++i)
        readers.push_back(std::async(std::launch::async, [f] {
            const auto r = f.materialize();
            if (!r.image)
                throw std::runtime_error("concurrent image incomplete");
            std::set<Tuple> out;
            for (std::size_t j = 0; j < r.image->size(); ++j)
                out.insert(r.image->row(j).tuple());
            return out;
        }));
    // The checks counter is intentionally not touched by concurrent readers.
    for (auto &reader : readers)
        check(reader.get() == std::set<Tuple>{{101, 101}, {102, 102}}, "concurrent action scratch shared");
    auto restored = decode_relational_finite_source(packet, {});
    const auto cold = Family::projection(restored, declaration({cell(0), cell(1)}));
    check(!cold.source(0).same_owner(s) && tuples(cold.materialize()) == tuples(image),
          "cold recipe inferred volatile owner identity");
    check(encode_relational_finite_source(s) == packet, "source flags/occurrences/bytes changed");
}
void reused_roots() {
    for (unsigned mask = 1; mask < 16; ++mask) {
        const auto s = source(mask);
        const auto packet = encode_relational_finite_source(s);
        const auto p = *s.prepare_global_section().prepared;
        for (bool fixed : {false, true}) {
            Source::Assignment common;
            if (fixed)
                common.emplace(cell(0), 101);
            const auto propagation = p.propagate(common);
            for (const auto &ports : std::vector<std::vector<core::CellId>>{
                     {}, {cell(0)}, {cell(1)}, {cell(0), cell(1)}, {cell(1), cell(0)}}) {
                auto d = declaration(ports);
                d.fixed = common;
                const auto baseline = Family::projection(p, d).materialize();
                if (!propagation.snapshot) {
                    check(propagation.status == local::FinitePropagationStatus::Unsatisfiable &&
                              tuples(baseline).empty(),
                          "refuted common request became a checkpoint");
                    continue;
                }
                const auto &root = *propagation.snapshot;
                const auto saved = root.domains();
                const auto f = Family::projection(root, d);
                check(f.source(0).same_owner(s) && f.propagation(0)->same_owner(root),
                      "family lost actual root/source owner");
                const auto result = f.materialize();
                check(tuples(result) == tuples(baseline), "root reuse changed full correlated image");
                for (std::size_t i = 0; i < result.image->size(); ++i)
                    check(result.image->row(i).witness(0) == baseline.image->row(i).witness(0),
                          "reused query changed complete source witness");
                check(result.work.preparations == 0 && result.work.reused_preparations == 1 &&
                          result.work.reused_propagations == 1 &&
                          result.work.prior_preparation_steps == p.preparation_work().steps &&
                          result.work.prior_propagation_steps == root.propagation_work().steps &&
                          result.work.source_queries == result.work.continued_source_queries,
                      "prior or executed work hidden/doublecharged");
                check(root.domains() == saved, "family queries changed retained root field");
            }
        }
        check(encode_relational_finite_source(s) == packet, "reuse changed original source archive");
    }
    const auto s = source(9);
    const auto p = *s.prepare_global_section().prepared;
    const auto root = *p.propagate().snapshot;
    auto d = declaration({cell(0), cell(1)});
    const auto f = Family::projection(root, d);
    const RelationalFiniteInterfaceBijection identity{3, 1, {0, 1}, {{101, 102}, {101, 102}}};
    const auto intersection = Family::intersect_images(f, f, identity);
    const auto joint = intersection.materialize();
    check(tuples(joint) == std::set<Tuple>{{101, 101}, {102, 102}} && joint.work.reused_propagations == 2 &&
              joint.work.prior_propagation_steps == root.propagation_work().steps &&
              joint.work.prior_preparation_steps == p.preparation_work().steps,
          "same actual root was counted twice or correlation lost");
    check(intersection.propagation(0)->same_owner(root) && intersection.propagation(1)->same_owner(root),
          "intersection discarded checkpoint lineage");
    const auto independent = *p.propagate().snapshot;
    const auto separately_rooted = Family::projection(independent, d);
    const auto separate = Family::intersect_images(f, separately_rooted, identity).materialize();
    check(separate.work.prior_propagation_steps == 2 * root.propagation_work().steps,
          "independent equal checkpoints were interned");
    const auto baseline = Family::projection(p, d);
    const auto mixed = Family::intersect_images(f, baseline, identity).materialize();
    check(tuples(mixed) == tuples(joint) && mixed.work.continued_source_queries == mixed.work.source_queries &&
              mixed.work.propagations == 0 && mixed.work.reused_propagations == 2,
          "identical common request failed to reuse the actual incoming root");
    auto changed = d;
    changed.fixed.emplace(cell(0), 101);
    rejects([&] { (void)Family::projection(root, changed); });
    const auto conditioned = *p.propagate(changed.fixed).snapshot;
    rejects([&] { (void)Family::projection(conditioned, d); });
    changed.fixed[cell(0)] = 102;
    rejects([&] { (void)Family::projection(conditioned, changed); });
    changed = d;
    changed.distinct_relations = {{{1}, 0}};
    rejects([&] { (void)Family::projection(root, changed); });
    RelationalFiniteFamilyLimits limit;
    const auto full = f.materialize();
    limit.max_entries = full.work.peak_entries - 1;
    check(!f.materialize(limit).image, "root field omitted from action entry bound");
    limit = {};
    limit.max_steps = full.work.steps - 1;
    const auto bounded = f.materialize(limit);
    check(bounded.status == RelationalFiniteImageStatus::BudgetExceeded && !bounded.image &&
              bounded.work.steps <= limit.max_steps,
          "reused native work escaped total budget");
    limit = {};
    limit.max_candidates = 3;
    check(!f.materialize(limit).image, "partial reused image published as complete");
    std::stop_source stop;
    stop.request_stop();
    check(f.materialize({}, stop.get_token()).status == RelationalFiniteImageStatus::Cancelled,
          "root-backed action ignored cancellation");
    std::vector<std::future<bool>> readers;
    for (unsigned i = 0; i < 8; ++i)
        readers.push_back(std::async(std::launch::async, [f] {
            const auto r = f.materialize();
            return r.image && r.image->size() == 2;
        }));
    for (auto &reader : readers)
        check(reader.get(), "root-backed concurrent image shared scratch");
    const auto retained = [] {
        auto owner = source(9);
        const auto prepared = *owner.prepare_global_section().prepared;
        return Family::projection(*prepared.propagate().snapshot, declaration({cell(0), cell(1)}));
    }();
    check(tuples(retained.materialize()) == tuples(full), "family borrowed destroyed checkpoint/source");
    const auto packet = encode_relational_finite_source(s);
    auto restored = decode_relational_finite_source(packet, {});
    const auto cold_root = *restored.prepare_global_section().prepared->propagate().snapshot;
    const auto cold = Family::projection(cold_root, d).materialize();
    check(tuples(cold) == tuples(full) && !cold_root.source().same_owner(s),
          "cold root recipe changed image or identified owners");
    check(encode_relational_finite_source(s) == packet, "reused/cold action changed original source");
}
Source alias_source(std::uint32_t count, bool observed, bool odd_cycle = false) {
    core::RelationSignature signature;
    const auto type = signature.add_type("ordered field relation", core::BoundaryPolicy::DeclaredExact, 0, 64);
    core::RelationalComplexBuilder builder(signature);
    std::vector<core::BoundaryOccurrenceInput> inputs;
    std::vector<core::CellId> patch;
    for (std::uint32_t i = 0; i < count; ++i) {
        inputs.emplace_back(i + 1, true, false, 1);
        patch.push_back(cell(i));
    }
    inputs.emplace_back(1, false, true, 2);
    inputs.emplace_back(2, true, false, 0);
    builder.add_grade_one(core::RelationKey{70}, type, inputs);
    const auto complex = builder.freeze();
    patch.push_back({{1}, 0});
    storage::NativeTemporalSeries history(storage::TemporalEndpoint(complex, 1), 27070);
    RelationalInputUseDeclaration a;
    a.key = 1;
    a.channel = 1;
    a.encoding = "UTF-8";
    a.bytes = {std::byte{'x'}, std::byte{'x'}};
    RelationalSpanUse use;
    use.key = 1;
    use.spans = {{0, 2}};
    for (auto c : patch)
        use.cells.push_back(storage::stable_cell_key(complex, c));
    for (std::size_t i = 0; i < inputs.size(); ++i)
        use.occurrences.push_back({{1}, complex.occurrence_identity({1}, i)});
    a.uses.push_back(use);
    RelationalFiniteSourceDeclaration d;
    d.key = 70;
    d.revision = 1;
    d.symbols = {{101, {std::byte{'x'}}}, {102, {std::byte{'x'}}}};
    for (std::uint32_t i = 0; i < count; ++i) {
        d.variables.push_back({cell(i), {101, 102}});
        d.bindings.push_back({i + 1, 1, cell(i)});
    }
    const std::vector<Tuple> allowed = odd_cycle ? std::vector<Tuple>{{101, 102}, {102, 101}}
                                                : std::vector<Tuple>{{101, 101}, {102, 102}};
    for (std::uint32_t i = 0; i < count - 1 + (odd_cycle ? 1u : 0u); ++i)
        d.charts.push_back({i + 1, "complete shared field law", patch,
                            {cell(i), cell((i + 1) % count)}, allowed});
    if (observed)
        d.charts.push_back({100, "actual observed field coordinate", {cell(0)}, {cell(0)}, {{101}}});
    return {OwnedRelationalInputUseAsset(history, a), d};
}
void canonical_pruning() {
    const auto source = alias_source(25, true);
    const auto bytes = encode_relational_finite_source(source);
    std::vector<core::CellId> cells;
    for (std::uint32_t i = 0; i < 25; ++i)
        cells.push_back(cell(i));
    const auto family = Family::projection(source, declaration(cells));
    RelationalFiniteFamilyLimits limits;
    limits.max_candidates = 1;
    const auto image = family.materialize(limits);
    check(tuples(image) == std::set<Tuple>{Tuple(25, 101)} && image.work.source_queries == 1 &&
              image.work.candidates == 1 && image.work.propagations == 1 && image.work.pruned_values == 25,
          "singleton field still enumerated the declared Cartesian cube");
    check(image.image->family().same_owner(family) && !family.declaration(0).window &&
              !family.propagation(0) && family.window() == std::vector<Tuple>(25, {101, 102}),
          "internal pruning changed original raw family identity/carriers");
    const auto &evidence = image.image->pruning();
    check(image.pruning.get() == &evidence && evidence.complete && evidence.window == std::vector<Tuple>(25, {101}) &&
              evidence.propagations.size() == 1 && evidence.propagations[0].snapshot &&
              evidence.propagations[0].snapshot->source().same_owner(source) &&
              evidence.propagations[0].status == local::FinitePropagationStatus::FixedPoint &&
              !evidence.propagations[0].reused && !evidence.unsatisfiable_leaf,
          "complete pruning evidence lost its original owner, domains or status");
    check(evidence.propagations[0].work.steps == evidence.propagations[0].snapshot->propagation_work().steps &&
              image.work.native.steps >= evidence.propagations[0].work.steps,
          "executed propagation disappeared from shared work receipt");
    auto bounded = limits;
    bounded.max_steps = image.work.steps - 1;
    const auto incomplete = family.materialize(bounded);
    check(incomplete.status == RelationalFiniteImageStatus::BudgetExceeded && !incomplete.image &&
              incomplete.work.steps <= bounded.max_steps && incomplete.pruning && !incomplete.pruning->complete,
          "shared step exhaustion certified a complete pruned image");
    bounded = limits;
    bounded.max_entries = image.work.peak_entries - 1;
    const auto limited = family.materialize(bounded);
    check(limited.status == RelationalFiniteImageStatus::BudgetExceeded && !limited.image &&
              limited.work.peak_entries <= bounded.max_entries,
          "propagation/root evidence escaped receiving entry admission");
    bounded = limits;
    bounded.max_steps = 0;
    check(!family.materialize(bounded).image, "zero shared budget produced a pruning certificate");
    std::stop_source stop;
    stop.request_stop();
    const auto cancelled = family.materialize(limits, stop.get_token());
    check(cancelled.status == RelationalFiniteImageStatus::Cancelled && !cancelled.image,
          "canonical pruning ignored cancellation");
    auto request = declaration({cell(1), cell(0)});
    request.window = std::vector<Tuple>{{102, 101}, {102, 101}};
    const auto ordered = Family::projection(source, request).materialize();
    check(tuples(ordered) == std::set<Tuple>{{101, 101}} &&
              ordered.image->family().declaration(0).window == request.window &&
              ordered.image->family().cells() == request.cells,
          "pruning rewrote original window/cell order");
    request.window = std::vector<Tuple>{{102}, {102, 101}};
    const auto outside = Family::projection(source, request).materialize();
    check(tuples(outside).empty() && outside.pruning->window[0].empty() && !outside.pruning->unsatisfiable_leaf,
          "excluded window became invented support or source UNSAT");
    request.window = std::vector<Tuple>{{}, {102, 101}};
    const auto empty = Family::projection(source, request).materialize();
    check(tuples(empty).empty() && empty.work.preparations == 0 && empty.pruning->propagations.empty(),
          "explicit empty image ran an unnecessary source solve");
    request.window.reset();
    request.fixed.emplace(cell(0), 102);
    const auto contradiction = Family::projection(source, request).materialize();
    check(tuples(contradiction).empty() && contradiction.pruning->unsatisfiable_leaf == 0 &&
              contradiction.pruning->propagations[0].status == local::FinitePropagationStatus::Unsatisfiable &&
              !contradiction.pruning->propagations[0].snapshot && contradiction.queries.empty() &&
              contradiction.image->family().declaration(0).fixed == request.fixed,
          "canonical contradiction lost original fixed request or invented a checkpoint");
    const auto uncertain = Family::projection(alias_source(4, false), declaration({cell(0), cell(1), cell(2), cell(3)}));
    const auto correlated = uncertain.materialize();
    check(tuples(correlated) == std::set<Tuple>{Tuple(4, 101), Tuple(4, 102)} && correlated.work.pruned_values == 0,
          "propagated marginals were mistaken for complete global witnesses");
    const auto odd = Family::projection(alias_source(3, false, true), declaration({cell(0), cell(1), cell(2)})).materialize();
    check(tuples(odd).empty() && odd.pruning->propagations[0].snapshot && !odd.pruning->unsatisfiable_leaf &&
              odd.work.pruned_values == 0 && odd.work.source_queries != 0,
          "locally supported odd cycle was treated as a satisfiable whole family");
    auto grouped = declaration({cell(0), cell(1)});
    grouped.distinct_relations = {{{1}, 0}};
    check(tuples(Family::projection(::source(6), grouped).materialize()) == std::set<Tuple>{{101, 102}, {102, 101}},
          "canonical propagation omitted original distinct-group request");
    const RelationalFiniteInterfaceBijection identity{9, 1, {}, {}};
    auto shared_map = identity;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        shared_map.right_positions.push_back(i);
        shared_map.right_symbols.push_back({101, 102});
    }
    const auto repeated = Family::intersect_images(family, family, shared_map).materialize(limits);
    check(tuples(repeated) == tuples(image) && repeated.work.propagations == 1 &&
              repeated.pruning->propagations.size() == 2 && repeated.pruning->propagations[1].reused &&
              repeated.pruning->propagations[0].snapshot->same_owner(*repeated.pruning->propagations[1].snapshot),
          "same actual common request did not reuse its canonical root");
    const auto retained = [] {
        auto owner = alias_source(25, true);
        std::vector<core::CellId> ports;
        for (std::uint32_t i = 0; i < 25; ++i) ports.push_back(cell(i));
        return Family::projection(owner, declaration(ports)).materialize();
    }();
    check(tuples(retained) == tuples(image) && retained.pruning->propagations[0].snapshot,
          "pruning recipe borrowed a destroyed source or action");
    auto restored = decode_relational_finite_source(bytes, {});
    const auto cold = Family::projection(restored, declaration(cells)).materialize(limits);
    check(tuples(cold) == tuples(image) && cold.work.steps == image.work.steps &&
              cold.pruning->window == evidence.window && !restored.same_owner(source),
          "cold raw recipe changed rows/evidence/work or identified foreign owners");
    std::vector<std::future<RelationalFiniteImageResult>> readers;
    for (unsigned i = 0; i < 4; ++i)
        readers.push_back(std::async(std::launch::async, [family, limits] { return family.materialize(limits); }));
    for (auto &reader : readers) {
        const auto result = reader.get();
        check(tuples(result) == tuples(image) && result.work.steps == image.work.steps &&
                  !result.pruning->propagations[0].snapshot->same_owner(*evidence.propagations[0].snapshot),
              "concurrent raw actions shared mutable roots or changed complete evidence");
    }
    check(encode_relational_finite_source(source) == bytes, "canonical pruning changed source bytes/flags/uses");
}
} // namespace
int main() {
    try {
        exhaustive();
        controls();
        reused_roots();
        canonical_pruning();
        std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
