#include "native_defect_fixture.hpp"
#include <iostream>
#include <chrono>
#include <random>

using namespace native_defect_fixture;
namespace {
std::size_t checks{};
void check(bool yes, const char* message) { ++checks; if (!yes) throw std::runtime_error(message); }
template<class F> void rejects(F&& f, const char* message) {
    bool failed = false; try { f(); } catch (const std::exception&) { failed = true; }
    check(failed, message);
}
void compare(const NativeDefectView& a, const NativeDefectView& b, const std::vector<NativeDefectQuery>& qs) {
    check(a.route_keys() == b.route_keys(), "incremental route support differs from rebuild");
    for (auto key : a.route_keys()) {
        const auto& x = a.route(key); const auto& y = b.route(key);
        check(x.upper == y.upper && x.middle == y.middle && x.lower == y.lower, "route identity/attachment mismatch");
        check(x.active == y.active && x.contribution == y.contribution, "route map differs from rebuild");
    }
    for (const auto& q : qs) {
        const auto& x = a.certificate(q.key); const auto& y = b.certificate(q.key);
        check(x.query == y.query && x.selected_routes == y.selected_routes, "chart membership differs from rebuild");
        check(x.total_routes == y.total_routes && x.active_routes == y.active_routes && x.nonzero_routes == y.nonzero_routes,
              "chart counters differ from rebuild");
        check(x.sums == y.sums && x.block_routes == y.block_routes && x.status() == y.status(), "chart sum differs from rebuild");
    }
}
void reference(const NativeDefectView& view, bool stalks) {
    const auto e = view.endpoint();
    for (std::size_t k = 2; k < e->source.grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        const auto full = global_boundary(*e, core::Grade{static_cast<std::uint16_t>(k - 1)}, stalks)
                            .multiply(global_boundary(*e, grade, stalks));
        check(indexed_defect(view, grade, stalks) == full, "index disagrees with native full boundary composition");
        check(view.flat(grade) == (full.nonzeros() == 0), "index curvature status disagrees with full native composition");
    }
    for (const auto key : view.route_keys()) {
        const auto& r = view.route(key);
        const auto p = resolve_stable_occurrence(e->source, key.upper).value(), q = resolve_stable_occurrence(e->source, key.lower).value();
        const auto active = e->source.grade(key.upper.upper_grade).aspects.get(e->source.signature().existence_aspect(), p) &&
                            e->source.grade(key.lower.upper_grade).aspects.get(e->source.signature().existence_aspect(), q);
        M expected;
        if (stalks) {
            const auto system = temporal_linear_fiber_system(*e, realization(*e).stalk_carriers);
            const auto a = system.flag_map(key.upper.upper_grade, p).transport, b = system.flag_map(key.lower.upper_grade, q).transport;
            expected = active ? b.multiply(a) : M(b.rows(), a.cols());
        } else {
            expected = M(1, 1); if (active) expected.set(0, 0, e->source.incidence_coefficient(key.upper.upper_grade, p) * e->source.incidence_coefficient(key.lower.upper_grade, q));
        }
        check(r.active == active && r.contribution == expected, "occurrence-level native product mismatch");
    }
}
void basics(const std::filesystem::path& out) {
    auto e = endpoint(); NativeDefectStore index(e, 17, {}, queries()); auto v = index.pin();
    check(v.route_count() == 15 && v.stats().route_products == 12, "retained inactive route population");
    check(v.certificate(1).total_routes == 15 && v.certificate(1).selected_routes.size() == 15, "complete route coverage");
    check(v.certificate(1).local_channels_cancel(), "cancelling route channels erased");
    check(v.certificate(2).status() == NativeDefectStatus::PartialNonzero && !v.certificate(2).curved_in_scope(), "partial chart mislabeled curved");
    check(v.certificate(3).complete() && v.certificate(3).assembled_zero(), "endpoint scope incomplete");
    check(v.certificate(4).status() == NativeDefectStatus::PartialNonzero, "scoped chart coverage");
    check(v.certificate(5).complete() && v.certificate(5).assembled_zero(), "declared empty upper grade");
    check(v.contains(route(1000000000, 1002)) && v.contains(route(1000000000, 1003)), "cancelling repeated occurrences merged");
    check(v.route(route(1000000000, 1002)).contribution.get(0, 0) == 1 && v.route(route(1000000000, 1003)).contribution.get(0, 0) == -1, "signed duplicate route channels");
    check(v.contains(route(1000000000, 1004)) && !v.route(route(1000000000, 1004)).active, "inactive retained route missing");
    check(v.block({cell(0, 19), cell(2, 200)}).has_value(), "retained zero block confused with no routes");
    check(!v.block({cell(0, 999), cell(2, 200)}).has_value(), "invented route block");
    reference(v, false);
    auto explicit_all = complete(9); explicit_all.selection = NativeDefectSelection::Occurrences;
    for (std::size_t k = 1; k <= 2; ++k) {
        const core::Grade g{static_cast<std::uint16_t>(k)};
        for (auto id : e.source.grade(g).occurrence_identities) explicit_all.occurrences.push_back({g, id});
    }
    index.reconfigure({explicit_all}); auto all = index.pin(); check(all.certificate(9).complete(), "validated complete explicit selection");
    explicit_all.occurrences.pop_back(); index.reconfigure({explicit_all});
    const auto subset = index.pin(); check(!subset.certificate(9).complete(), "omitted retained occurrence still complete");
    auto bad = explicit_all; bad.occurrences.push_back(bad.occurrences.front());
    rejects([&] { index.reconfigure({bad}); }, "duplicate occurrence selection accepted");
    bad = explicit_all; bad.selection = static_cast<NativeDefectSelection>(255);
    rejects([&] { index.reconfigure({bad}); }, "unknown query opcode accepted");
    bad = explicit_all; bad.selection = NativeDefectSelection::AllRoutes;
    rejects([&] { index.reconfigure({bad}); }, "caller supplied complete label over partial list");
    bad = complete(); bad.upper_grade = core::Grade{1}; rejects([&] { index.reconfigure({bad}); }, "invalid interface grade");
    bad = complete(); bad.endpoints = NativeDefectBlockKey{cell(1, 100), cell(2, 200)};
    rejects([&] { index.reconfigure({bad}); }, "wrong endpoint scope grades");
    bad = complete(); bad.endpoints = NativeDefectBlockKey{cell(0, 123456), cell(2, 200)};
    rejects([&] { index.reconfigure({bad}); }, "missing endpoint scope");
    rejects([&] { index.reconfigure({complete(), complete()}); }, "duplicate subscriptions accepted");
    rejects([&] { (void)v.flat(core::Grade{1}); }, "invalid Bianchi interface accepted");
    rejects([&] { NativeDefectStore x(e, 0); }, "missing source namespace accepted");
    auto invalid = NativeDefectRealization{}; invalid.kind = static_cast<NativeDefectRealizationKind>(255);
    rejects([&] { NativeDefectStore x(e, 1, invalid); }, "unknown realization accepted");
    auto preserved = [&] { auto owner = std::make_unique<NativeDefectStore>(e, 17, NativeDefectRealization{}, queries()); return owner->pin(); }();
    check(preserved.route_count() == 15 && preserved.certificate(1).complete(), "pin failed after owner destruction");
    e.sequence = 999; check(preserved.sequence() == 0, "caller alias mutated immutable publication");
    save(out / "scalar.rfcx", encode_rfcx(v.endpoint()->source).bytes); save(out / "scalar.rfcxe", encode_temporal_endpoint(*v.endpoint()));
}
void local_charts() {
    auto e = endpoint(2);
    LocalBlockDefinition b; b.block_key = 1; b.kind = LocalBlockKind::StrictSubcomplex;
    for (auto key : {cell(0, 10), cell(0, 11), cell(0, 12), cell(0, 19), cell(1, 100), cell(1, 101), cell(1, 102), cell(2, 200)}) b.cells.push_back(key);
    e.local_blocks.push_back(b);
    b.block_key = 2; b.kind = LocalBlockKind::PartialChart; b.cells = {cell(0, 10), cell(1, 100), cell(2, 200)};
    b.occurrences = {occ(1, 1000), occ(2, 1000000000)}; e.local_blocks.push_back(b);
    auto strict = complete(1); strict.selection = NativeDefectSelection::LocalBlock; strict.block = 1;
    auto partial = strict; partial.key = 2; partial.block = 2;
    auto scoped = strict; scoped.key = 3; scoped.endpoints = NativeDefectBlockKey{cell(0, 10), cell(2, 200)};
    auto qs = std::vector<NativeDefectQuery>{strict, partial, scoped}; NativeDefectStore store(e, 17, {}, qs); auto v = store.pin();
    check(v.certificate(1).selected_routes.size() == 15 && v.certificate(1).total_routes == 30, "strict local selection treated as whole ambient grade");
    check(v.certificate(1).status() == NativeDefectStatus::PartialZero, "strict local zero mislabeled whole grade flat");
    check(v.certificate(3).complete(), "strict endpoint scope not recognized complete");
    check(v.certificate(2).status() == NativeDefectStatus::PartialNonzero, "local block chart lost obstruction");
    auto next = e; ++next.sequence;
    next.local_blocks[1].occurrences.push_back(occ(1, 1002));
    store.advance(make_temporal_segment(e, next)); auto after = store.pin();
    check(after.stats().route_products == 0 && after.stats().query_rebuilds == 1, "chart-only edit recomputed route products");
    check(after.certificate(2).status() == NativeDefectStatus::PartialZero, "chart cancellation not updated");
    NativeDefectStore rebuilt(next, 17, {}, qs); compare(after, rebuilt.pin(), qs);
    auto next2 = next; ++next2.sequence; next2.local_blocks.erase(next2.local_blocks.begin() + 1);
    const auto segment = make_temporal_segment(next, next2);
    rejects([&] { store.advance(segment); }, "missing strict query silently became empty");
    check(store.pin().sequence() == next.sequence, "query failure published partial endpoint");
    store.advance(segment, {}, std::vector<NativeDefectQuery>{strict, scoped});
    check(store.pin().sequence() == next2.sequence, "atomic query removal with block removal");
}
void structural_updates(const std::filesystem::path& out) {
    auto old = endpoint(3); NativeDefectStore store(old, 17, {}, queries()); const auto initial = store.pin();
    auto next = old; next.source = source(3, false, false, true); ++next.sequence;
    auto segment = make_temporal_segment(old, next); store.advance(segment);
    check(store.stats().changed_legs == 0 && store.stats().route_products == 0 && store.stats().reused_routes == 45, "arena reorder recomputed stable routes");
    NativeDefectStore reordered(next, 17, {}, queries()); compare(store.pin(), reordered.pin(), queries());
    old = next; next.source = source(3, true, false, true); ++next.sequence;
    segment = make_temporal_segment(old, next); store.advance(segment); auto changed = store.pin();
    check(changed.stats().candidate_routes == 5 && changed.stats().route_products == 4, "sparse source repair touched unrelated routes");
    check(changed.stats().reused_routes == 40 && changed.flat(core::Grade{2}), "coordinated higher-boundary repair lost flatness");
    NativeDefectStore recomputed(next, 17, {}, queries()); compare(changed, recomputed.pin(), queries()); reference(changed, false);
    const auto word = initial.word_defect_to(changed, route(1000000000, 1000)); check(word.exact_identity(), "source temporal word identity");
    save(out / "repair.rfcxt", encode_temporal_segment(segment)); save(out / "repaired.rfcxe", encode_temporal_endpoint(next));
    old = next; next.source = source(3, true, false, true, true); ++next.sequence; store.advance(make_temporal_segment(old, next));
    check(store.stats().candidate_routes == 2 && store.stats().route_products == 2, "attachment update reverse dependencies");
    NativeDefectStore rebound(next, 17, {}, queries()); compare(store.pin(), rebound.pin(), queries());
    rejects([&] { changed.word_defect_to(store.pin(), route(1000000000, 1002)); }, "reattachment treated as same local word");
    old = next; next.source = source(4, true, false, true, true); ++next.sequence; store.advance(make_temporal_segment(old, next));
    check(store.stats().routes == 60 && store.stats().route_products == 12 && store.stats().reused_routes == 45, "birth route support/update work");
    NativeDefectStore born(next, 17, {}, queries()); compare(store.pin(), born.pin(), queries());
    old = next; next.source = source(2, true, false, true, true); ++next.sequence; store.advance(make_temporal_segment(old, next));
    check(store.stats().routes == 30 && store.stats().route_products == 0 && store.stats().routes_removed == 30, "removal retained stale routes");
    NativeDefectStore removed(next, 17, {}, queries()); compare(store.pin(), removed.pin(), queries());
    check(initial.route_count() == 45 && initial.sequence() == 0, "old pin changed across source updates");
    auto empty = TemporalEndpoint(source(0)); NativeDefectStore zero(empty, 17, {}, {complete(), complete(5, 3)});
    check(zero.pin().route_count() == 0 && zero.pin().flat(core::Grade{2}), "empty retained source index");
}
void maps_and_channels(const std::filesystem::path& out) {
    auto old = endpoint(2, true); const auto r = realization(old);
    NativeDefectStore store(old, 17, r, queries()); const auto initial = store.pin(); reference(initial, true);
    auto next = old; ++next.sequence; set_map(next, occ(1, 1000), M::from_rows({{Q{-1}, 0}, {0, Q{7}}}));
    auto segment = make_temporal_segment(old, next); store.advance(segment); const auto deformed = store.pin();
    check(store.stats().candidate_routes == 1 && store.stats().route_products == 1, "single local map update not local");
    check(deformed.certificate(1).curved_in_scope() && !deformed.certificate(2).curved_in_scope(), "complete/partial deformed status");
    const auto word = initial.word_defect_to(deformed, route(1000000000, 1000)); check(word.exact_identity() && word.direct.nonzeros() == 1, "unused local operator direction was lost");
    NativeDefectStore fresh(next, 17, r, queries()); compare(deformed, fresh.pin(), queries()); reference(deformed, true);
    save(out / "stalk-base.rfcx", encode_rfcx(old.source).bytes); save(out / "stalk-base.rfcxe", encode_temporal_endpoint(old));
    save(out / "deformed.rfcxt", encode_temporal_segment(segment)); save(out / "deformed.rfcxe", encode_temporal_endpoint(next));
    // Independent source boundary realization must not silently consume a local deformation.
    NativeDefectStore scalar(next, 17, {}, queries()); check(scalar.pin().flat(core::Grade{2}), "local tensor deformed primary source");
    old = next; ++next.sequence; set_map(next, occ(1, 1004), M::diagonal({9, 11})); store.advance(make_temporal_segment(old, next));
    check(store.stats().candidate_routes == 1 && store.stats().route_products == 0, "inactive override executed");
    const auto inactive_view = store.pin(); check(inactive_view.route(route(1000000000, 1004)).contribution.nonzeros() == 0, "inactive override leaked into assembly");
    old = next; next.source = source(2, false, false, false, false, true); ++next.sequence; store.advance(make_temporal_segment(old, next));
    const auto activated_view = store.pin(); check(activated_view.route(route(1000000000, 1004)).contribution == M::diagonal({9, 11}), "active realized override was coefficient-scaled twice");
    reference(store.pin(), true);
    old = next; ++next.sequence; set_map(next, occ(1, 1000), M::diagonal({2, 3}), TemporalTensorRole::CoefficientIndependentMap);
    store.advance(make_temporal_segment(old, next));
    const auto coefficient_view = store.pin(); check(coefficient_view.route(route(1000000000, 1000)).contribution == M::diagonal({-2, -3}), "coefficient-independent map not realized exactly once");
    old = next; ++next.sequence;
    TemporalTensor metric; metric.key = 90000000; metric.role = TemporalTensorRole::Metric; metric.input = metric.output = carrier_key(cell(0, 10));
    metric.tensor = temporal_matrix_tensor(M::diagonal({2, 5})); next.tensors.push_back(metric);
    store.advance(make_temporal_segment(old, next)); check(store.stats().route_products == 0 && store.stats().changed_legs == 0, "metric-only birth invalidated incidence products");
    old = next; ++next.sequence; next.tensors.back().tensor = temporal_matrix_tensor(M::diagonal({3, 7}));
    TemporalTensor field; field.key = 90000001; field.role = TemporalTensorRole::Field; field.output = carrier_key(cell(2, 200));
    field.tensor = {numeric::IndexDomain::linear(2), numeric::StructuralSupport::full(2), numeric::SparseRationalVector(2, {{0, Q{99, 7}}})};
    next.tensors.push_back(field); store.advance(make_temporal_segment(old, next));
    check(store.stats().route_products == 0 && store.stats().query_accumulations == 0, "field-only innovation changed defect cache");
    NativeDefectStore last(next, 17, r, queries()); compare(store.pin(), last.pin(), queries());
    // Simultaneous noncommuting edits: the cross term is not discarded.
    auto a = endpoint(1, true); set_map(a, occ(1, 1000), M::from_rows({{1, 2}, {0, 1}})); set_map(a, occ(2, 1000000000), M::from_rows({{2, 0}, {1, 1}}));
    auto b = a; b.sequence = 1; set_map(b, occ(1, 1000), M::from_rows({{2, 1}, {1, 3}})); set_map(b, occ(2, 1000000000), M::from_rows({{1, 1}, {3, 2}}));
    NativeDefectStore pair(a, 17, realization(a), queries()); const auto before = pair.pin(); pair.advance(make_temporal_segment(a, b)); const auto after = pair.pin();
    const auto law = before.word_defect_to(after, route(1000000000, 1000)); check(law.exact_identity(), "finite noncommuting route word");
    check(after.stats().candidate_routes == 5 && after.stats().route_products == 4, "overlapping changed-leg dependencies were counted twice");
    NativeDefectStore double_rebuild(b, 17, realization(b), queries()); compare(after, double_rebuild.pin(), queries()); reference(after, true);
    const auto systems_a = temporal_linear_fiber_system(a, realization(a).stalk_carriers), systems_b = temporal_linear_fiber_system(b, realization(b).stalk_carriers);
    const auto A = systems_a.flag_map(core::Grade{2}, 0).transport, B = systems_a.flag_map(core::Grade{1}, 0).transport;
    const auto Ap = systems_b.flag_map(core::Grade{2}, 0).transport, Bp = systems_b.flag_map(core::Grade{1}, 0).transport;
    const auto dA = Ap.add(A.scale(-1)), dB = Bp.add(B.scale(-1));
    check(law.direct == B.multiply(dA).add(dB.multiply(A)).add(dB.multiply(dA)), "finite interaction term missing");
    check(dB.multiply(dA).nonzeros() != 0, "interaction fixture accidentally first-order");
    std::ofstream oracle(out / "noncommuting.txt"); write_matrix(oracle, A); write_matrix(oracle, B); write_matrix(oracle, Ap); write_matrix(oracle, Bp); write_matrix(oracle, law.direct);
}
void metadata_and_supported_zero() {
    auto a = endpoint(1, true); set_map(a, occ(1, 1000), M::diagonal({2, 3}));
    NativeDefectStore index(a, 17, realization(a), queries()); const auto old = index.pin();
    auto b = a; b.sequence = 1;
    b.tensors.back().tensor = {b.tensors.back().tensor.domain(), numeric::StructuralSupport::sparse(4, {0, 3}), b.tensors.back().tensor.values()};
    index.advance(make_temporal_segment(a, b));
    check(index.stats().changed_legs == 1 && index.stats().route_products == 1, "support-only local tensor declaration was ignored");
    const auto current = index.pin();
    check(current.route(route(1000000000, 1000)).contribution == old.route(route(1000000000, 1000)).contribution, "support-only edit changed exact numerical operator");
    check(current.endpoint()->tensors.back().tensor.support().count() == 2 && old.endpoint()->tensors.back().tensor.support().count() == 4, "retained tensor support collapsed into numerical nonzeros");
    auto c = b; c.sequence = 2;
    for (auto& carrier : c.carriers) if (carrier.key == carrier_key(cell(1, 100))) carrier.coordinate_system += 100000000;
    std::vector<TemporalCorrespondence> maps{{carrier_key(cell(1, 100)), M::from_rows({{1, 1}, {0, 1}})}};
    index.advance(make_temporal_segment(b, c, maps));
    const auto changed = index.pin();
    rejects([&] { current.word_defect_to(changed, route(1000000000, 1000)); }, "equal dimensions silently supplied a changed-basis correspondence");
    const auto law = current.word_defect_to(changed, route(1000000000, 1000), maps);
    check(law.exact_identity(), "changed-basis finite word failed");
    check(index.stats().changed_legs == 6 && index.stats().route_products == 4, "stalk declaration dependencies missed adjacent legs");
    reference(changed, true);
}
void rectangular_and_limits(const std::filesystem::path& out) {
    auto a = endpoint(1, true); TemporalEndpoint b(source(), 1); add_stalks(b, 2, 3, 4);
    const auto cs = temporal_endpoint_detail::carriers(b);
    for (std::uint16_t k = 1; k <= 2; ++k) {
        const auto& arena = b.source.grade(core::Grade{k});
        for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
            const auto upper = stable_cell_key(b.source, {core::Grade{k}, arena.upper_index[p]});
            const auto lower = stable_cell_key(b.source, {core::Grade{static_cast<std::uint16_t>(k - 1)}, arena.lower_index[p]});
            M m(cs.at(carrier_key(lower))->domain.logical_size(), cs.at(carrier_key(upper))->domain.logical_size());
            for (std::size_t i = 0; i < m.rows(); ++i) for (std::size_t j = 0; j < m.cols(); ++j)
                m.set(i, j, Q{static_cast<unsigned long>(i + j + p + 1), static_cast<unsigned long>(p + 1)});
            set_map(b, {core::Grade{k}, arena.occurrence_identities[p]}, m);
        }
    }
    std::vector<TemporalCorrespondence> maps;
    for (const auto& c : b.carriers) {
        M j(c.domain.logical_size(), 2); j.set(0, 0, 1); j.set(1, 1, 1); maps.push_back({c.key, j});
    }
    NativeDefectStore store(a, 17, realization(a), queries()); const auto before = store.pin();
    const auto segment = make_temporal_segment(a, b, maps); store.advance(segment); const auto after = store.pin(); reference(after, true);
    rejects([&] { before.word_defect_to(after, route(1000000000, 1000)); }, "rectangular carrier comparison inferred identity");
    const auto law = before.word_defect_to(after, route(1000000000, 1000), maps);
    check(law.exact_identity() && law.direct.rows() == 2 && law.direct.cols() == 2, "rectangular route word");
    const auto old_sys = temporal_linear_fiber_system(a, realization(a).stalk_carriers), new_sys = temporal_linear_fiber_system(b, realization(b).stalk_carriers);
    const temporal::OperationWord old_word({old_sys.flag_map(core::Grade{2}, 0).transport, old_sys.flag_map(core::Grade{1}, 0).transport});
    const temporal::OperationWord new_word({new_sys.flag_map(core::Grade{2}, 0).transport, new_sys.flag_map(core::Grade{1}, 0).transport});
    // Compare against the canonical word API using maps selected by carrier key,
    // not the incidental vector order in any application.
    const auto get_map = [&](StableCellKey c) { for (const auto& m : maps) if (m.carrier == carrier_key(c)) return m.map; throw std::runtime_error("map missing"); };
    const auto full = temporal::finite_word_defect(old_word, new_word, {get_map(cell(2, 200)), get_map(cell(1, 100)), get_map(cell(0, 10))});
    check(full.direct == law.direct && full.contributions == law.contributions, "bounded specialization differs from paper word API");
    save(out / "rectangular.rfcxt", encode_temporal_segment(segment)); save(out / "rectangular.rfcxe", encode_temporal_endpoint(b));
    std::ofstream oracle(out / "rectangular.txt"); write_matrix(oracle, old_word.op(0)); write_matrix(oracle, old_word.op(1));
    write_matrix(oracle, new_word.op(0)); write_matrix(oracle, new_word.op(1));
    write_matrix(oracle, get_map(cell(2, 200))); write_matrix(oracle, get_map(cell(1, 100))); write_matrix(oracle, get_map(cell(0, 10))); write_matrix(oracle, law.direct);
    auto wrong_ns = NativeDefectStore(b, 99, realization(b), queries()); rejects([&] { before.word_defect_to(wrong_ns.pin(), route(1000000000, 1000), maps); }, "cross-namespace word comparison");
    auto no_maps = b; no_maps.tensors.clear(); rejects([&] { NativeDefectStore x(no_maps, 17, realization(no_maps)); }, "rectangular default transport invented");
    auto missing = realization(a); missing.stalk_carriers.pop_back(); rejects([&] { NativeDefectStore x(a, 17, missing); }, "incomplete selected stalk realization");
    auto duplicated = realization(a); duplicated.stalk_carriers.push_back(duplicated.stalk_carriers.front()); rejects([&] { NativeDefectStore x(a, 17, duplicated); }, "duplicate selected carrier");
    auto zero = a; zero.carriers[0].domain = numeric::IndexDomain::linear(0); rejects([&] { NativeDefectStore x(zero, 17, realization(zero)); }, "zero native linear stalk");
    for (unsigned which = 0; which < 8; ++which) {
        NativeDefectLimits l;
        if (which == 0) l.max_routes = 1;
        if (which == 1) l.max_occurrences = 1;
        if (which == 2) l.max_cells = 1;
        if (which == 3) l.max_dimension = 1;
        if (which == 4) l.max_work = 1;
        if (which == 5) l.max_queries = 1;
        if (which == 6) l.max_query_memberships = 1;
        if (which == 7) l.max_total_nonzeros = 1;
        rejects([&] { NativeDefectStore x(a, 17, realization(a), queries(), l); }, "index resource limit not enforced");
    }
    NativeDefectLimits budget; budget.max_routes = 15; NativeDefectStore bounded(a, 17, realization(a), queries(), budget);
    auto grown = endpoint(2, true); grown.sequence = 1;
    rejects([&] { bounded.advance(make_temporal_segment(a, grown), realization(grown)); }, "route growth budget accepted");
    check(bounded.pin().sequence() == 0 && bounded.pin().route_count() == 15, "budget failure partially published");
}
void higher_interfaces_and_arity() {
    core::RelationSignature sig; const auto type = sig.add_type("generic-exact", core::BoundaryPolicy::DeclaredExact);
    core::RelationalComplexBuilder builder(sig); builder.declare_grade(core::Grade{4});
    for (std::uint64_t i = 0; i < 3; ++i)
        builder.add_grade_one(core::RelationKey{100 + i}, type, {
            {10 + i, true, true, Q{1}, {}, core::OccurrenceKey{1000 + 2 * i}},
            {10 + (i + 1) % 3, true, false, Q{1}, {}, core::OccurrenceKey{1001 + 2 * i}}});
    for (std::uint64_t f = 0; f < 2; ++f)
        builder.add_higher(core::Grade{2}, core::RelationKey{200 + f}, type, {
            {0, true, false, Q{1}, {}, core::OccurrenceKey{2000 + 3 * f}},
            {1, true, false, Q{1}, {}, core::OccurrenceKey{2001 + 3 * f}},
            {2, true, false, Q{1}, {}, core::OccurrenceKey{2002 + 3 * f}}});
    builder.add_higher(core::Grade{3}, core::RelationKey{300}, type, {
        {0, true, false, Q{1}, {}, core::OccurrenceKey{3000}},
        {1, true, true, Q{1}, {}, core::OccurrenceKey{3001}}});
    TemporalEndpoint a(builder.freeze()); add_stalks(a, 1, 1, 1);
    const std::vector<NativeDefectQuery> qs{complete(1, 2), complete(2, 3), complete(3, 4)};
    NativeDefectStore index(a, 17, realization(a), qs); const auto old = index.pin(); reference(old, true);
    check(old.route_count() == 18 && old.flat(core::Grade{2}) && old.flat(core::Grade{3}), "higher route population/flatness");
    auto b = a; b.sequence = 1; set_map(b, occ(2, 2000), M::from_rows({{Q{2}}}));
    index.advance(make_temporal_segment(a, b)); const auto next = index.pin();
    check(next.stats().candidate_routes == 3 && next.stats().route_products == 3, "one leg not invalidated in both adjacent interfaces");
    check(!next.flat(core::Grade{2}) && !next.flat(core::Grade{3}) && next.flat(core::Grade{4}), "higher local curvature interfaces");
    NativeDefectStore rebuilt(b, 17, realization(b), qs); compare(next, rebuilt.pin(), qs); reference(next, true);
    const auto D1 = global_boundary(b, core::Grade{1}, true), D2 = global_boundary(b, core::Grade{2}, true), D3 = global_boundary(b, core::Grade{3}, true);
    const auto F2 = indexed_defect(next, core::Grade{2}, true), F3 = indexed_defect(next, core::Grade{3}, true);
    check(D1.multiply(F3) == F2.multiply(D3), "exact deformed Bianchi associativity failed");
    const auto old2 = global_boundary(a, core::Grade{2}, true), old3 = global_boundary(a, core::Grade{3}, true);
    const auto E2 = D2.add(old2.scale(-1)), E3 = D3.add(old3.scale(-1));
    check(D2.multiply(E3).add(E2.multiply(old3)) == F3.add(indexed_defect(old, core::Grade{3}, true).scale(-1)), "cross-grade finite defect compatibility failed");
    const NativeRouteKey upper_route{occ(3, 3000), occ(2, 2000)};
    check(old.word_defect_to(next, upper_route).exact_identity(), "higher temporal word identity");
    auto local = complete(4, 3); local.selection = NativeDefectSelection::Occurrences; local.occurrences = {occ(3, 3000), occ(2, 2000)};
    index.reconfigure({local}); const auto partial = index.pin();
    check(partial.certificate(4).status() == NativeDefectStatus::PartialNonzero, "grade-three partial mislabeled curvature");
    // 64 by 64 occurrence pairs on ONE endpoint block. Block scratch must be
    // reused rather than copying an ever-growing set for each insertion.
    core::RelationalComplexBuilder high(sig); std::vector<core::BoundaryOccurrenceInput> lower;
    std::vector<core::HigherIncidenceInput> upper;
    for (std::uint64_t i = 0; i < 64; ++i) {
        lower.push_back({10, true, i % 2 != 0, Q{1}, {}, core::OccurrenceKey{1000 + i}});
        upper.push_back({0, true, i % 2 != 0, Q{1}, {}, core::OccurrenceKey{2000 + i}});
    }
    high.add_grade_one(core::RelationKey{100}, type, lower); high.add_higher(core::Grade{2}, core::RelationKey{200}, type, upper);
    TemporalEndpoint wide(high.freeze()); add_stalks(wide, 1, 1, 1);
    NativeDefectStore big(wide, 17, realization(wide), {complete()}); const auto initial = big.pin();
    check(initial.route_count() == 4096 && initial.certificate(1).local_channels_cancel(), "high-arity occurrence products collapsed");
    check(initial.stats().copied_blocks == 0, "initial same-block route insertion copies quadratic state");
    auto wide_next = wide; wide_next.sequence = 1; set_map(wide_next, occ(1, 1000), M::from_rows({{Q{3}}}));
    big.advance(make_temporal_segment(wide, wide_next)); const auto wide_view = big.pin();
    check(wide_view.stats().route_products == 64 && wide_view.stats().copied_blocks == 1 && wide_view.stats().reused_routes == 4032,
          "high-arity update not bounded by affected routes/touched blocks");
    check(wide_view.certificate(1).local_channels_cancel(), "high-arity global cancellation changed");
    NativeDefectStore full(wide_next, 17, realization(wide_next), {complete()}); compare(wide_view, full.pin(), {complete()});
    NativeDefectLimits small; small.max_candidate_routes = 32;
    rejects([&] { NativeDefectStore rejected(wide, 17, realization(wide), {complete()}, small); }, "candidate expansion exceeded preflight bound");
    numeric::Integer numerator{1}; numerator <<= 257; numerator += 1; const Q huge{numerator, numeric::Integer{7}};
    auto large = wide; large.sequence = 1; set_map(large, occ(1, 1000), M::from_rows({{huge}}));
    NativeDefectStore exact(wide, 17, realization(wide), {complete()}); exact.advance(make_temporal_segment(wide, large));
    const auto exact_view = exact.pin();
    check(exact_view.route({occ(2, 2000), occ(1, 1000)}).contribution.get(0, 0) == huge, "large rational route narrowed");
    small = {}; small.max_integer_bytes = 8; NativeDefectStore limited(wide, 17, realization(wide), {complete()}, small);
    rejects([&] { limited.advance(make_temporal_segment(wide, large)); }, "large rational route budget not enforced");
    check(limited.pin().sequence() == 0, "large rational failure partially published");
}

void observations_and_migrations() {
    auto a = endpoint(); RelationalCodeState codes; codes.namespace_key = 100; codes.fields = {1};
    codes.types.push_back({1, RelationalCodeTypeKind::Finite, {0, 1}, {}});
    codes.types.push_back(relational_aspect_code_type(a.source, 2, 1, {{10, a.source.signature().existence_aspect()},
        {11, a.source.signature().orientation_aspect()}, {12, a.source.signature().aspects().id("retained-marker")}}));
    codes.sections.push_back(relational_aspect_code_section(1, 1, 2, occ(1, 1000)));
    codes.sections.push_back(relational_aspect_code_section(2, 1, 2, occ(1, 1010))); a.codes = codes;
    RelationalCodeObservationQuery oq; oq.key = 1;
    NativeDefectStore index(a, 17, {}, queries(), {}, {oq}); const auto observation = index.observation({1, 1});
    auto b = a; b.sequence = 1; b.source = source(1, false, true);
    index.advance(make_temporal_segment(a, b));
    check(index.stats().route_products == 0 && index.stats().changed_legs == 0, "unrelated retained aspect invalidated route products");
    check(index.observation_stats().evaluated == 2, "source-only aspect observations not invalidated");
    check(index.observation({1, 1})->bytes != observation->bytes, "observation source bits stale");
    const auto pinned = index.pin(); const auto old_observation = index.observation({1, 1});
    auto bad = b; ++bad.sequence; bad.source = source(1, false, false);
    auto q = queries(); q[0].endpoints = NativeDefectBlockKey{cell(0, 123456), cell(2, 200)};
    const auto segment = make_temporal_segment(b, bad);
    rejects([&] { index.advance(segment, {}, q); }, "invalid candidate query accepted");
    check(index.pin().sequence() == 1 && index.observation({1, 1})->bytes == old_observation->bytes, "E and defect publications split on failure");
    SourceDeclarationMigration m; m.base_signature = b.source.signature(); m.target_signature = m.base_signature;
    m.target_signature.aspects().add("new-channel");
    for (std::size_t i = 0; i < m.base_signature.aspects().size(); ++i) {
        const auto name = m.base_signature.aspects().at(static_cast<composite::AspectId>(i)).name; m.aspects.push_back({name, name});
    }
    for (std::size_t i = 0; i < m.base_signature.type_count(); ++i) {
        const auto name = m.base_signature.type(static_cast<core::RelationTypeId>(i)).name; m.types.push_back({name, name});
    }
    m.introduced.push_back({"new-channel", true, {}});
    auto migrated = b; ++migrated.sequence; migrated.source = recode_temporal_source(b.source, m);
    index.advance(make_temporal_segment(b, migrated, {}, {}, RelationalCodeEncoding::Automatic, m));
    check(index.stats().route_products == 0 && index.stats().reused_routes == 15, "declaration extension altered unchanged realized routes");
    check(index.pin().endpoint()->source.signature().aspects().size() == 4, "target declaration not pinned");
    NativeDefectStore rebuilt(migrated, 17, {}, queries(), {}, {oq}); compare(index.pin(), rebuilt.pin(), queries());
    check(pinned.endpoint()->source.signature().aspects().size() == 3, "old source pin rebound to new declaration");
}
void random_history(const std::filesystem::path& out) {
    std::mt19937 rng(0x4defec7u); auto e = endpoint(2, true); auto qs = queries(); const auto r = realization(e);
    NativeDefectStore index(e, 17, r, qs);
    std::ofstream oracle(out / "routes.txt"), sums(out / "route-sums.txt");
    std::vector<std::byte> stream;
    for (unsigned iteration = 0; iteration < 80; ++iteration) {
        auto next = e; ++next.sequence;
        const auto grade = static_cast<std::uint16_t>(1 + rng() % 2);
        const auto& arena = next.source.grade(core::Grade{grade}); const auto slot = rng() % arena.incidence_count();
        M m(2, 2);
        for (std::size_t i = 0; i < 2; ++i) for (std::size_t j = 0; j < 2; ++j)
            m.set(i, j, Q{static_cast<long>(rng() % 11) - 5, static_cast<unsigned long>(1 + rng() % 7)});
        set_map(next, {core::Grade{grade}, arena.occurrence_identities[slot]}, m,
                iteration % 3 == 0 ? TemporalTensorRole::CoefficientIndependentMap : TemporalTensorRole::RealizedIncidenceMap);
        const auto segment = make_temporal_segment(e, next); const auto bytes = encode_temporal_segment(segment);
        stream.insert(stream.end(), bytes.begin(), bytes.end());
        const auto before = index.pin(); index.advance(decode_temporal_segment(bytes)); const auto after = index.pin();
        NativeDefectStore rebuild(next, 17, r, qs); compare(after, rebuild.pin(), qs); reference(after, true);
        check(after.stats().candidate_routes <= 5, "random sparse edit touched unrelated routes");
        for (auto key : after.route_keys()) {
            const auto law = before.word_defect_to(after, key); check(law.exact_identity(), "random finite route-word identity");
        }
        const auto system = temporal_linear_fiber_system(next, r.stalk_carriers);
        for (auto key : after.route_keys()) {
            const auto& channel = after.route(key); oracle << iteration << ' ' << channel.active << ' ' << channel.lower.grade.value << ' ' << channel.lower.key << ' ' << channel.upper.grade.value << ' ' << channel.upper.key << '\n';
            write_matrix(oracle, system.flag_map(key.upper.upper_grade, resolve_stable_occurrence(next.source, key.upper).value()).transport);
            write_matrix(oracle, system.flag_map(key.lower.upper_grade, resolve_stable_occurrence(next.source, key.lower).value()).transport);
            write_matrix(oracle, channel.contribution);
        }
        std::set<NativeDefectBlockKey> written;
        for (auto key : after.route_keys()) {
            const auto& channel = after.route(key); const NativeDefectBlockKey bk{channel.lower, channel.upper};
            if (!written.insert(bk).second) continue;
            sums << iteration << ' ' << bk.lower.grade.value << ' ' << bk.lower.key << ' ' << bk.upper.grade.value << ' ' << bk.upper.key << '\n';
            write_matrix(sums, *after.block(bk));
        }
        e = std::move(next);
    }
    save(out / "history.rfcxt", stream); save(out / "history-final.rfcxe", encode_temporal_endpoint(e));
    for (const auto chunk : {std::size_t{1}, std::size_t{7}, std::size_t{64}, std::size_t{4096}}) {
        NativeDefectStore replay(endpoint(2, true), 17, r, qs); TemporalSegmentStreamDecoder decoder;
        std::size_t frames = 0;
        for (std::size_t i = 0; i < stream.size(); i += chunk) {
            decoder.feed(std::span<const std::byte>(stream).subspan(i, std::min(chunk, stream.size() - i)), [&](const TemporalSegment& s, std::span<const std::byte>) { replay.advance(s); ++frames; });
        }
        decoder.finish(); check(frames == 80, "chunked history frame count"); compare(replay.pin(), index.pin(), qs);
    }
}
void work_scaling(const std::filesystem::path& out) {
    std::ofstream log(out / "work.csv"); log << "triangles,routes,changed_legs,candidate_routes,products,reused,scanned\n";
    for (const auto n : {std::size_t{1}, std::size_t{16}, std::size_t{128}}) {
        auto a = endpoint(n, true); NativeDefectStore store(a, 17, realization(a), queries()); auto b = a; ++b.sequence;
        set_map(b, occ(1, 1000), M::from_rows({{Q{-2}, 0}, {0, Q{3}}})); store.advance(make_temporal_segment(a, b)); const auto s = store.stats();
        check(s.changed_legs == 1 && s.candidate_routes == 1 && s.route_products == 1, "affected-route work grew with unrelated source");
        check(s.routes == 15 * n && s.reused_routes == 15 * n - 1, "unaffected route cache not reused");
        log << n << ',' << s.routes << ',' << s.changed_legs << ',' << s.candidate_routes << ',' << s.route_products << ',' << s.reused_routes << ',' << s.scanned_occurrences << '\n';
    }
}
void disk_and_failures(const std::filesystem::path& out) {
    auto a = endpoint(1, true), b = a; ++b.sequence; set_map(b, occ(1, 1000), M::diagonal({Q{-5, 7}, Q{13, 17}}));
    const auto bytes = encode_rfcx(a.source).bytes, sidecar = encode_temporal_endpoint(a), frame = encode_temporal_segment(make_temporal_segment(a, b));
    MappedRfcx mapped{std::span<const std::byte>(bytes)}; auto disk = decode_temporal_endpoint(mapped.materialize(), sidecar);
    NativeDefectStore index(disk, 17, realization(disk), queries()); index.advance(decode_temporal_segment(frame));
    check(encode_temporal_endpoint(*index.pin().endpoint()) == encode_temporal_endpoint(b), "disk-only target reconstruction"); reference(index.pin(), true);
    save(out / "disk-base.rfcx", bytes); save(out / "disk-base.rfcxe", sidecar); save(out / "disk-update.rfcxt", frame);
    std::size_t truncations = 0;
    for (std::size_t n = 0; n < frame.size(); ++n) { rejects([&] { (void)decode_temporal_segment(std::span<const std::byte>(frame).first(n)); }, "truncated segment accepted"); ++truncations; }
    NativeDefectStore base(a, 17, realization(a), queries()); auto wrong = make_temporal_segment(a, b); ++wrong.base_sequence;
    rejects([&] { base.advance(wrong); }, "wrong base accepted"); check(base.pin().sequence() == 0, "failed replay changed publication");
    std::mt19937 rng(0x404040u); std::size_t parsed = 0, replayed = 0;
    for (unsigned i = 0; i < 200; ++i) {
        auto bad = frame; const auto at = 32 + rng() % (bad.size() - 32); bad[at] ^= std::byte{static_cast<unsigned char>(1u << (rng() % 8))};
        const auto hash = rfcx_detail::fnv1a64(std::span<const std::byte>(bad).subspan(32));
        for (std::size_t j = 0; j < 8; ++j) bad[24 + j] = std::byte{static_cast<unsigned char>((hash >> (j * 8)) & 255u)};
        try {
            const auto candidate = decode_temporal_segment(bad); ++parsed;
            NativeDefectStore test(a, 17, realization(a), queries());
            try { test.advance(candidate); ++replayed; reference(test.pin(), true); }
            catch (const std::exception&) { check(test.pin().sequence() == 0, "mutated replay failure partially published"); }
        } catch (const std::exception&) {}
        check(base.pin().sequence() == 0, "mutation changed unrelated base");
    }
    std::ofstream log(out / "malformed.txt"); log << "truncations=" << truncations << " mutations=200 parsed=" << parsed << " replayed=" << replayed << '\n';
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::invalid_argument("usage: rfc_native_defect_tests [NEW_FIXTURE_DIRECTORY]");
        const bool temporary = argc == 1;
        const std::filesystem::path out = temporary ? std::filesystem::temp_directory_path() /
            ("rfc-native-defects-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) : argv[1];
        if (!std::filesystem::create_directory(out)) throw std::runtime_error("use a fresh fixture directory");
        basics(out); local_charts(); structural_updates(out); maps_and_channels(out); rectangular_and_limits(out);
        higher_interfaces_and_arity(); metadata_and_supported_zero(); observations_and_migrations(); random_history(out); work_scaling(out); disk_and_failures(out);
        if (temporary) std::filesystem::remove_all(out);
        std::cout << "native defect index: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1; }
}
