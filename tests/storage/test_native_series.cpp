#include "native_series_fixture.hpp"
#include "native_accession_fixture.hpp"
#include "native_defect_fixture.hpp"
#include "declaration_temporal_fixture.hpp"
#include <iostream>
#include <random>

namespace {
using namespace native_series_fixture;
std::size_t checks = 0;
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F&& f, const char* message) {
    bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } check(failed, message);
}
void equal(const TemporalEndpoint& a, const TemporalEndpoint& b) {
    check(encode_rfcx(a.source).bytes == encode_rfcx(b.source).bytes, "source differs");
    check(encode_temporal_endpoint(a) == encode_temporal_endpoint(b), "endpoint channels differ");
}
std::vector<std::byte> stream(const PaperHistory& h) {
    auto bytes = encode_temporal_segment(h.first); auto second = encode_temporal_segment(h.second);
    bytes.insert(bytes.end(), second.begin(), second.end()); return bytes;
}
void basics(const std::filesystem::path& out) {
    PaperHistory h; NativeTemporalSeries s(h.a, 700);
    check(s.first_sequence() == 40 && s.last_sequence() == 40 && s.segment_count() == 0, "nonzero origin sequence");
    auto pin = s.tail(); h.a.tensors[0] = field_tensor(10, 1, {90, 91});
    check(pin.endpoint().tensors[0].tensor.state(0) == Q{1}, "caller alias reached series");
    h.a = endpoint(40, {1, 2});
    const auto initial_bytes = encode_native_series(s);
    std::vector<std::byte> streamed;bool changed{};auto reentrant=s;
    stream_native_series(reentrant,[&](auto chunk) {
        check(chunk.size()<=65536,"stream emitted an unbounded archive chunk");
        if(!changed){changed=true;reentrant.append(h.first);}
        streamed.insert(streamed.end(),chunk.begin(),chunk.end());
    });
    check(streamed==initial_bytes && reentrant.last_sequence()==41,"stream followed reentrant publication change");
    const auto detached=[] {
        PaperHistory paper;NativeTemporalSeries local(paper.a,700);
        std::optional<NativeSeriesComponent> component;
        local.visit_components([&](auto part){if(part.kind==NativeSeriesComponentKind::BaseSource)component=std::move(part);});
        return *component;
    }();
    check(detached.sequence==40 && *detached.bytes==encode_rfcx(h.a.source).bytes,"native component lost its immutable owner");
    equal(*decode_native_series(initial_bytes).tail().pin(), h.a);
    auto mutable_input=initial_bytes;
    const auto independently_owned=decode_native_series(mutable_input);
    std::fill(mutable_input.begin(),mutable_input.end(),std::byte{0});
    check(encode_native_series(independently_owned)==initial_bytes,
          "reader retained a mutable caller byte alias");
    save(out / "empty.rfcxs", initial_bytes);
    s.append(h.first); s.append_encoded(encode_temporal_segment(h.second));
    auto range = s.difference(40, 42); auto before = range.before(); auto after = range.after();
    equal(before.endpoint(), h.a); equal(after.endpoint(), h.c);
    check(range.segment_count() == 2, "difference collapsed ordered interval");
    check(range.correspondence(1) == h.J1.multiply(h.J0), "ordered carrier composition");
    auto evo = range.field_evolution(10);
    check(evo.accumulation.transported_initial == Vec({4, -1}), "paper transported initial");
    check(evo.accumulation.propagated_innovations == std::vector<Vec>({{5, -4}, {4, -2}}), "paper propagated innovations");
    check(evo.accumulation.terminal == Vec({13, -7}), "paper terminal field");
    auto moments = range.moments({{10, {}}}, 20);
    check(moments.gram.get(1, 1) == 41 && moments.gram.get(1, 2) == 28 && moments.gram.get(2, 2) == 20,
          "paper innovation cross terms lost");
    Q innovation_sum = 0, total = 0;
    for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 3; ++j) {
        total += moments.gram.get(i, j); if (i && j) innovation_sum += moments.gram.get(i, j);
    }
    check(innovation_sum == 117 && total == 218, "moment contraction differs from quadrance");
    check(moments.provenance[0].kind == NativeSeriesChannelKind::InitialField && moments.provenance[1].kind == NativeSeriesChannelKind::Innovation && moments.provenance[1].sequence == 41,
          "channel source time lost");
    const auto samples = range.sample_moments({{40, 10, {}}, {42, 10, {}}}, 20);
    check(samples.gram == M::from_rows({{17, 59}, {59, 218}}), "paper cross-time sampled moments");
    check(samples.provenance[0].kind == NativeSeriesChannelKind::SampledField, "sample mislabeled innovation");
    rejects([&] { (void)range.sample_moments({{39, 10, {}}}, 20); }, "sample outside interval accepted");
    rejects([&] { (void)range.sample_moments({{40, 10, {}}, {40, 10, {}}}, 20); }, "duplicate source sample");
    rejects([&] { (void)range.sample_moments({{40, 20, {}}}, 20); }, "metric sampled as field");
    auto patch = range.source_patch(); check(encode_rfcx(replay_temporal_source(h.a.source, patch)).bytes == encode_rfcx(h.c.source).bytes,
                                          "direct source endpoint difference failed");
    const auto one = s.difference(41, 41);
    check(one.correspondence(1) == M::identity(3) && one.field_evolution(10).accumulation.propagated_innovations.empty(),
          "empty interval typed identity");
    check(one.moments({{10, {}}}, 20).gram.get(0, 0) == 46, "empty interval moment");
    auto old_range = range; auto old_pin = s.at(41); auto old_copy = s;
    s.checkpoint(41);
    auto accelerated = s.at(42), full = s.at(42, true);
    check(accelerated.stats().used_checkpoint && accelerated.stats().replayed_segments == 1 && full.stats().replayed_segments == 2,
          "checkpoint replay schedule");
    equal(accelerated.endpoint(), full.endpoint());
    const auto ck = s.at(41); check(ck.stats().replayed_segments == 0, "checkpoint itself replayed");
    s.checkpoint(42); s.repack_checkpoints({42});
    check(s.checkpoint_sequences() == std::vector<std::uint64_t>{42}, "checkpoint repack selection");
    check(s.at(41).stats().replayed_segments == 1, "removed checkpoint used");
    check(s.at(42).stats().replayed_segments == 0, "terminal checkpoint unused");
    equal(old_pin.endpoint(), h.b); equal(*old_copy.at(42).pin(), h.c);
    check(old_copy.checkpoint_sequences().empty(), "fork mutation escaped");
    check(s.same_history(old_copy), "checkpoint schedule changed temporal identity");
    check(old_range.correspondence(1) == range.correspondence(1), "compaction altered old history view");
    check(std::ranges::equal(s.segment_bytes(0), encode_temporal_segment(h.first)) &&
          std::ranges::equal(s.segment_bytes(1), encode_temporal_segment(h.second)), "compaction rewrote frames");
    auto bytes = encode_native_series(s); check(bytes.size() == s.archive_bytes(), "series byte accounting");
    auto decoded = decode_native_series(bytes); check(encode_native_series(decoded) == bytes, "canonical archive round trip");
    for (std::uint64_t seq = 40; seq <= 42; ++seq) equal(*decoded.at(seq).pin(), *s.at(seq).pin());
    s.repack_checkpoints({}); check(s.checkpoint_sequences().empty() && s.segment_count() == 2, "compaction erased history");
    const auto compact = encode_native_series(s); check(compact.size() < bytes.size(), "redundant checkpoint not removed");
    save(out / "paper.rfcxs", bytes); save(out / "compact.rfcxs", compact);
    save(out / "base.rfcx", encode_rfcx(h.a.source).bytes); save(out / "base.rfcxe", encode_temporal_endpoint(h.a));
    save(out / "history.rfcxt", stream(h));
    equal(*read_native_series(out / "paper.rfcxs").tail().pin(), h.c);
    std::ofstream oracle(out / "paper.txt"); matrix(oracle, h.J0); matrix(oracle, h.J1);
    vector(oracle, {1, 2}); vector(oracle, {2, -1, 3}); vector(oracle, {4, -2});
    vector(oracle, evo.accumulation.transported_initial);
    for (const auto& v : evo.accumulation.propagated_innovations) vector(oracle, v);
    vector(oracle, evo.accumulation.terminal); matrix(oracle, moments.gram);
    auto surviving = [&] { NativeTemporalSeries tmp(h.a, 88); tmp.append(h.first); return tmp.at(41); }();
    equal(surviving.endpoint(), h.b); check(surviving.source_namespace() == 88, "pin namespace lost");
    auto moved = std::move(decoded); rejects([&] { (void)decoded.segment_count(); }, "moved-from series usable");
    check(moved.segment_count() == 2, "move lost history");
    auto p = moved.tail(); auto moved_pin = std::move(p);
    rejects([&] { (void)p.endpoint(); }, "moved-from endpoint pin dereferenced");
    check(moved_pin.sequence() == 42, "moved pin lost endpoint");
    auto d = moved.difference(40, 42); auto moved_range = std::move(d);
    rejects([&] { (void)d.correspondence(1); }, "moved interval dereferenced");
    check(moved_range.segment_count() == 2, "moved interval lost history");
    rejects([&] { (void)s.at(39); }, "pre-base lookup accepted"); rejects([&] { (void)s.at(43); }, "future lookup accepted");
    rejects([&] { (void)s.difference(42, 40); }, "implicit reverse history");
    rejects([&] { (void)range.correspondence(999); }, "missing carrier accepted");
    rejects([&] { (void)range.field_evolution(20); }, "metric treated as field");
    rejects([&] { (void)range.moments({}, 20); }, "empty moment selection");
    rejects([&] { (void)range.moments({{10, {}}, {10, {}}}, 20); }, "duplicate moment channels");
    rejects([&] { (void)range.moments({{10, {}}}, 10); }, "field used as metric");
    rejects([&] { (void)range.moments({{10, 20}}, 20); }, "metric used as accession");
    const auto saved = encode_native_series(s);
    rejects([&] { s.checkpoint(40); }, "base checkpoint duplicated");
    rejects([&] { s.repack_checkpoints({41, 41}); }, "duplicate checkpoint accepted");
    rejects([&] { s.repack_checkpoints({41, 43}); }, "partial repack committed");
    check(encode_native_series(s) == saved, "failed repack changed state");
    auto bad = h.second; bad.base_sequence = 40;
    rejects([&] { s.append(bad); }, "wrong predecessor accepted");
    check(encode_native_series(s) == saved, "failed append changed state");
    NativeTemporalSeries foreign(h.a, 701); foreign.append(h.first); foreign.append(h.second);
    check(!s.same_history(foreign), "history equality ignored source namespace");
    auto maximum = endpoint(UINT64_MAX, {1}); NativeTemporalSeries max_series(maximum, 1);
    auto wrapped = make_temporal_segment(endpoint(0, {1}), endpoint(1, {2}));
    wrapped.base_sequence = UINT64_MAX; wrapped.target_sequence = 0;
    rejects([&] { max_series.append(wrapped); }, "wrapped sequence accepted");
}
void chronology() {
    auto a = endpoint(0, {1, 0}), b = endpoint(1, {1, 0});
    M I = M::identity(2), changed = I; changed.set(1, 1, Q{9});
    NativeTemporalSeries first(a, 1), second(a, 1);
    first.append(make_temporal_segment(a, b, {{1, I}})); second.append(make_temporal_segment(a, b, {{1, changed}}));
    first.checkpoint(1); second.checkpoint(1);
    equal(*first.tail().pin(), *second.tail().pin());
    check(encode_native_series(first) != encode_native_series(second), "equal endpoints erased different operations");
    check(!first.same_history(second), "history equality ignored complete operation definitions");
    check(first.difference(0, 1).correspondence(1) != second.difference(0, 1).correspondence(1), "unused direction lost");
    auto z0 = endpoint(0, {0, 0}), z1 = endpoint(1, {0, 0}), z2 = endpoint(2, {0, 0});
    M A = M::from_rows({{1, 1}, {0, 1}}), B = M::from_rows({{1, 0}, {1, 1}});
    NativeTemporalSeries ab(z0, 2), ba(z0, 2);
    ab.append(make_temporal_segment(z0, z1, {{1, A}})); ab.append(make_temporal_segment(z1, z2, {{1, B}}));
    ba.append(make_temporal_segment(z0, z1, {{1, B}})); ba.append(make_temporal_segment(z1, z2, {{1, A}}));
    check(ab.difference(0, 2).correspondence(1) == B.multiply(A), "noncommutative order reversed");
    check(ba.difference(0, 2).correspondence(1) == A.multiply(B), "noncommutative order erased");
    check(ab.difference(0, 2).correspondence(1) != ba.difference(0, 2).correspondence(1), "different words identified");
    auto removed = b; removed.tensors.clear(); removed.carriers.clear();
    NativeTemporalSeries death(a, 7); death.append(make_temporal_segment(a, removed));
    auto reborn = a; reborn.sequence = 2; death.append(make_temporal_segment(removed, reborn));
    equal(*death.at(2).pin(), reborn);
    rejects([&] { (void)death.difference(0, 2).field_evolution(10); }, "field removal silently bridged");
    rejects([&] { (void)death.difference(0, 2).correspondence(1); }, "carrier removal silently bridged");
}
void target_appends() {
    const auto before_checks = checks;
    PaperHistory h;
    NativeTemporalSeries reference(h.a, 700), direct(h.a, 700);
    const auto fork = direct;
    const auto pin = direct.tail();
    reference.append(h.first); direct.append_target(h.b, {{1, h.J0}});
    reference.checkpoint(41); direct.checkpoint(41);
    reference.append(h.second); direct.append_target(h.c, {{1, h.J1}});
    check(encode_native_series(direct) == encode_native_series(reference), "target append changed complete bytes");
    equal(*direct.at(41).pin(), h.b); equal(*direct.tail().pin(), h.c);
    equal(pin.endpoint(), h.a);
    check(fork.segment_count() == 0 && fork.last_sequence() == 40, "target append changed a fork");
    check(direct.difference(40,42).correspondence(1) == h.J1.multiply(h.J0), "target append changed map order");
    h.c.tensors[0] = field_tensor(10,1,{0,0});
    check(direct.tail().pin()->tensors[0].tensor.state(0) == Q{13}, "target append retained a caller alias");
    const auto saved = encode_native_series(direct);
    auto invalid = *direct.tail().pin();
    rejects([&] { direct.append_target(invalid); }, "target append accepted a repeated sequence");
    invalid.sequence = 43;
    rejects([&] { direct.append_target(invalid, {{1,M::identity(3)}}); }, "target append accepted a wrong map shape");
    rejects([&] { direct.append_target(invalid, {{1,M::identity(2)},{1,M::identity(2)}}); }, "target append accepted duplicate maps");
    invalid.tensors[0].output = 999;
    rejects([&] { direct.append_target(invalid); }, "target append accepted an undeclared carrier");
    check(encode_native_series(direct) == saved, "failed target append changed history");

    auto zero = endpoint(0,{0,0});
    NativeTemporalSeries ab(zero,701), ba(zero,701);
    auto next = zero; next.sequence = 1;
    const auto A = M::from_rows({{1,1},{0,1}}), B = M::from_rows({{1,0},{1,1}});
    ab.append_target(next,{{1,A}}); ba.append_target(next,{{1,B}});
    next.sequence = 2; ab.append_target(next,{{1,B}}); ba.append_target(next,{{1,A}});
    check(!ab.same_history(ba), "target append erased equal-output operation history");
    check(ab.difference(0,2).correspondence(1) == B.multiply(A), "target append reversed the operation word");
    check(ba.difference(0,2).correspondence(1) == A.multiply(B), "target append merged noncommuting words");
    next.sequence = 3;
    next.tensors[0].tensor = {numeric::IndexDomain::linear(2), numeric::StructuralSupport::sparse(2,{0}),
                             numeric::SparseRationalVector(2)};
    ab.append_target(next);
    check(ab.tail().pin()->tensors[0].tensor.state(0) == Q{0}, "target append removed supported zero");
    check(!ab.tail().pin()->tensors[0].tensor.support().contains(1), "target append filled absent support");
    const auto roundtrip = decode_native_series(encode_native_series(ab));
    equal(*roundtrip.tail().pin(), next);
    auto death = next; death.sequence = 4; death.tensors.clear(); death.carriers.clear(); ab.append_target(death);
    next.sequence = 5; ab.append_target(next);
    rejects([&] { (void)ab.difference(3,5).field_evolution(10); }, "target append bridged field death and rebirth");

    for (unsigned mode = 0; mode < 4; ++mode) {
        NativeSeriesLimits limits;
        if (mode == 0) limits.max_segments = 0;
        if (mode == 1) limits.max_archive_bytes = fork.archive_bytes();
        if (mode == 2) limits.temporal.max_dimension = 4;
        NativeTemporalSeries limited(h.a,700,limits);
        if (mode == 3) limited = NativeTemporalSeries(endpoint(UINT64_MAX,{1,2}),700,limits);
        const auto original = encode_native_series(limited);
        rejects([&] { limited.append_target(h.b,{{1,h.J0}}); }, "target append ignored a native history limit");
        check(encode_native_series(limited) == original, "limited target append published a prefix");
    }
    auto moved = std::move(direct);
    rejects([&] { direct.append_target(next); }, "moved-from series accepted target append");
    check(encode_native_series(moved) == saved, "target append move changed history");
    std::cout << "target_append_checks=" << checks-before_checks << '\n';
}
void endpoint_visits() {
    PaperHistory h; NativeTemporalSeries series(h.a, 700);
    series.append(h.first); series.append(h.second); series.checkpoint(41); series.checkpoint(42);
    const auto saved = encode_native_series(series);
    std::vector<NativeSeriesEndpoint> pins;
    series.visit_endpoints([&](const auto& endpoint) { pins.push_back(endpoint); });
    check(pins.size() == 3, "endpoint visit omitted a retained endpoint");
    std::size_t replayed = 0;
    for (std::size_t i = 0; i < pins.size(); ++i) {
        check(pins[i].sequence() == 40+i && pins[i].source_namespace() == 700, "endpoint visit order or owner differs");
        const auto reference = series.at(40+i);
        equal(pins[i].endpoint(), reference.endpoint());
        const auto stats = pins[i].stats(); replayed += stats.replayed_segments;
        check(!stats.used_checkpoint && stats.replayed_segments == (i ? 1u : 0u), "endpoint visit replayed a prefix");
        check(stats.starting_sequence == (i ? 39+i : 40), "endpoint visit starting sequence differs");
    }
    check(replayed == 2 && encode_native_series(series) == saved, "endpoint visit changed history or repeated frames");
    auto reentrant = series;
    std::vector<std::uint64_t> visited;
    reentrant.visit_endpoints([&](const auto& endpoint) {
        visited.push_back(endpoint.sequence());
        if (endpoint.sequence() == 40) {
            reentrant.repack_checkpoints({});
            auto next = h.c; next.sequence = 43; reentrant.append_target(next);
        }
    });
    check(visited == std::vector<std::uint64_t>{40,41,42} && reentrant.last_sequence() == 43,
          "endpoint visit followed a callback publication change");
    std::size_t calls = 0;
    rejects([&] { series.visit_endpoints([&](const auto&) {
        if (++calls == 2) throw std::runtime_error("stop endpoint visit");
    }); }, "endpoint visit swallowed a callback exception");
    check(calls == 2 && encode_native_series(series) == saved, "failed endpoint visit changed history");
    const auto detached = [&] {
        NativeTemporalSeries local(h.a, 700); local.append(h.first); local.append(h.second);
        std::vector<NativeSeriesEndpoint> owned;
        local.visit_endpoints([&](const auto& endpoint) { owned.push_back(endpoint); });
        return owned;
    }();
    equal(detached[0].endpoint(), h.a); equal(detached[1].endpoint(), h.b); equal(detached[2].endpoint(), h.c);
    NativeTemporalSeries empty(h.a, 700); std::size_t count = 0;
    empty.visit_endpoints([&](const auto& endpoint) { ++count; equal(endpoint.endpoint(), h.a); });
    check(count == 1, "empty history did not visit its base");
    auto moved = std::move(empty);
    rejects([&] { empty.visit_endpoints([](const auto&) {}); }, "moved-from history accepted an endpoint visit");
    check(moved.first_sequence() == 40, "endpoint visit move changed its owner");
}
void limits_and_streams() {
    PaperHistory h; auto bytes = stream(h);
    for (const auto chunk : {std::size_t{1}, std::size_t{2}, std::size_t{7}, std::size_t{31}, std::size_t{32},
                             std::size_t{63}, std::size_t{64}, std::size_t{129}, bytes.size() + 1}) {
        NativeTemporalSeries s(h.a, 1); s.append_stream(bytes, chunk); equal(*s.tail().pin(), h.c);
    }
    NativeTemporalSeries s(h.a, 1); const auto before = encode_native_series(s);
    auto truncated = bytes; truncated.pop_back(); rejects([&] { s.append_stream(truncated, 7); }, "truncated stream committed prefix");
    check(encode_native_series(s) == before, "prefix published before stream failure");
    auto out_of_order = encode_temporal_segment(h.second); auto x = encode_temporal_segment(h.first);
    out_of_order.insert(out_of_order.end(), x.begin(), x.end());
    rejects([&] { s.append_stream(out_of_order); }, "reordered stream accepted");
    rejects([&] { s.append_stream(bytes, 0); }, "zero stream chunk accepted");
    s.append_stream({}); check(encode_native_series(s) == before, "empty stream changed history");
    NativeSeriesLimits l; l.max_segments = 1; NativeTemporalSeries limited(h.a, 1, l);
    rejects([&] { limited.append_stream(bytes); }, "aggregate stream segment budget ignored"); check(limited.segment_count() == 0, "budget prefix published");
    l = {}; l.max_archive_bytes = before.size(); NativeTemporalSeries exact(h.a, 1, l);
    rejects([&] { exact.append(h.first); }, "append aggregate byte budget ignored");
    --l.max_archive_bytes; rejects([&] { NativeTemporalSeries bad(h.a, 1, l); }, "base byte budget ignored");
    l = {}; l.max_checkpoints = 0; NativeTemporalSeries none(h.a, 1, l); none.append(h.first);
    rejects([&] { none.checkpoint(41); }, "checkpoint count budget ignored");
    l = {}; l.max_analysis_steps = 1; NativeTemporalSeries analysis(h.a, 1, l); analysis.append_stream(bytes);
    rejects([&] { (void)analysis.difference(40, 42).field_evolution(10); }, "field analysis step budget ignored");
    rejects([&] { (void)analysis.difference(40, 42).correspondence(1); }, "map analysis step budget ignored");
    l = {}; l.max_analysis_scalars = 3; NativeTemporalSeries small(h.a, 1, l); small.append_stream(bytes);
    rejects([&] { (void)small.difference(40, 42).moments({{10, {}}}, 20); }, "moment allocation budget ignored");
    l = {}; l.max_analysis_work = 0; NativeTemporalSeries work(h.a, 1, l); work.append_stream(bytes);
    rejects([&] { (void)work.difference(40, 42).correspondence(1); }, "analysis work budget ignored");
    l = {}; l.max_analysis_dimension = 1; NativeTemporalSeries dimension(h.a, 1, l); dimension.append_stream(bytes);
    rejects([&] { (void)dimension.difference(40, 42).field_evolution(10); }, "analysis dimension ignored");
    rejects([&] { NativeTemporalSeries missing(h.a, 0); }, "missing namespace accepted");
}
void migration_and_indexes(const std::filesystem::path& out) {
    auto a = f2_fixture::endpoint(); const auto migration = f2_fixture::source_map(a.source.signature());
    auto b = f2_fixture::evolved(a, migration); auto seg = f2_fixture::segment(a, b, migration);
    NativeTemporalSeries series(a, 111); series.append(seg); series.checkpoint(1);
    NativeTemporalSeries direct(a,111);
    direct.append_target(b,{},{{1000,20}},RelationalCodeEncoding::Automatic,migration,
                        make_temporal_declaration_update(a.declarations,b.declarations,f2_fixture::maps()));
    direct.checkpoint(1);
    check(encode_native_series(direct) == encode_native_series(series), "target append changed declaration migration bytes");
    auto c = b; c.sequence = 2; c.tensors[0] = field_tensor(1, 10, {5, 3});
    auto next = make_temporal_segment(b, c); series.append(next);
    direct.append_target(c);
    check(encode_native_series(direct) == encode_native_series(series), "target append changed migrated continuation");
    auto loaded = decode_native_series(encode_native_series(series));
    equal(*loaded.at(0).pin(), a); equal(*loaded.at(1).pin(), b); equal(*loaded.at(2).pin(), c);
    std::size_t visited = 0;
    loaded.visit_endpoints([&](const auto& endpoint) {
        const auto reference = loaded.at(endpoint.sequence());
        equal(endpoint.endpoint(), reference.endpoint()); ++visited;
    });
    check(visited == 3, "endpoint visit omitted a declaration migration");
    check(loaded.at(1).pin()->source.grade_count() == 4, "empty grades lost at checkpoint");
    check(loaded.at(1).pin()->source.grade(core::Grade{1}).incidence_count() == 5, "inactive occurrence lost");
    check(loaded.difference(0, 2).correspondence(10) == M::identity(2), "declaration recoding correspondence lost");
    check(loaded.difference(0, 2).field_evolution(1).accumulation.propagated_innovations[0] == Vec({0, 0}), "schema recoding invented innovation");
    rejects([&] { (void)loaded.difference(0, 2).source_patch(); }, "implicit signature collapse accepted");
    save(out / "migration.rfcxs", encode_native_series(series));
    auto e = native_accession_fixture::endpoint(8); NativeTemporalSeries accession(e, 300);
    auto changed = e; changed.sequence = 1; changed.tensors[0] = native_accession_fixture::field(101, 1, 8, 4);
    changed.source = native_accession_fixture::source(8, 2, true);
    accession.append(make_temporal_segment(e, changed)); accession.checkpoint(1);
    auto pinned = decode_native_series(encode_native_series(accession)).at(1);
    NativeAccessionDirectory directory(pinned.endpoint(), pinned.source_namespace(), native_accession_fixture::queries());
    NativeAccessionDirectory fresh(changed, 300, native_accession_fixture::queries());
    auto req = NativeAccessionRequest{}; req.kind = NativeAccessionKind::Field; req.subject = 101;
    const auto v = directory.pin(req), expected = fresh.pin(req);
    check(temporal_endpoint_detail::tensor_equal(*v.tensor(), *expected.tensor()), "checkpoint accession differs");
    equal(*directory.pin_snapshot(), changed);
    save(out / "accession.rfcxs", encode_native_series(accession));
    auto def = native_defect_fixture::endpoint(1, true); NativeTemporalSeries defects(def, 61);
    NativeDefectStore index(def, 61, native_defect_fixture::realization(def), native_defect_fixture::queries());
    auto moved = def; moved.sequence = 1;
    native_defect_fixture::set_map(moved, native_defect_fixture::occ(1, 1000), M::from_rows({{2, 1}, {0, 3}}));
    const auto step = make_temporal_segment(def, moved); defects.append(step); index.advance(step); defects.checkpoint(1);
    auto checkpoint = decode_native_series(encode_native_series(defects)).at(1);
    NativeDefectStore restored(checkpoint.endpoint(), 61, native_defect_fixture::realization(moved), native_defect_fixture::queries());
    const auto original = index.pin(), rebuild = restored.pin(); check(original.route_keys() == rebuild.route_keys(), "checkpoint route population differs");
    for (const auto key : original.route_keys()) check(original.route(key).contribution == rebuild.route(key).contribution, "checkpoint route product differs");
    for (const auto& q : native_defect_fixture::queries()) check(original.certificate(q.key).sums == rebuild.certificate(q.key).sums, "checkpoint defect certificate differs");
    save(out / "defects.rfcxs", encode_native_series(defects));
}
void random_history(const std::filesystem::path& out) {
    std::mt19937 random(0x705u); auto a = endpoint(0, {1, 2}); NativeTemporalSeries s(a, 555);
    NativeTemporalSeries direct(a,555);
    std::vector<TemporalEndpoint> endpoints{a};
    std::vector<M> maps; std::vector<Vec> innovations; std::ofstream oracle(out / "random.txt"); vector(oracle, {1, 2});
    for (std::uint64_t step = 0; step < 32; ++step) {
        const std::size_t rows = 2 + random() % 2, cols = a.tensors[0].tensor.domain().logical_size();
        M J(rows, cols);
        for (std::size_t i = 0; i < rows; ++i) for (std::size_t j = 0; j < cols; ++j)
            J.set(i, j, Q{static_cast<long>(random() % 5) - 2, static_cast<unsigned long>(1 + random() % 3)});
        Vec eta(rows); for (auto& q : eta) { q = Q{static_cast<long>(random() % 7) - 3, static_cast<unsigned long>(1 + random() % 5)}; q.canonicalize(); }
        const auto value = numeric::add(J.apply(native_series_detail::dense(a.tensors[0].tensor.values())), eta);
        auto b = endpoint(step + 1, value); const auto segment = make_temporal_segment(a, b, {{1, J}});
        s.append(segment); if ((step + 1) % 8 == 0) s.checkpoint(step + 1);
        direct.append_target(b,{{1,J}}); if ((step + 1) % 8 == 0) direct.checkpoint(step + 1);
        check(encode_native_series(direct) == encode_native_series(s), "random target append changed complete history bytes");
        endpoints.push_back(b); a = b; maps.push_back(J); innovations.push_back(eta);
        matrix(oracle, J); vector(oracle, eta); vector(oracle, value);
    }
    for (std::uint64_t seq = 0; seq <= 32; ++seq) {
        const auto direct_endpoint = s.at(seq, true), accelerated = s.at(seq);
        equal(direct_endpoint.endpoint(), endpoints.at(static_cast<std::size_t>(seq))); equal(direct_endpoint.endpoint(), accelerated.endpoint());
        check(accelerated.stats().replayed_segments <= 7, "checkpoint schedule exceeded interval");
    }
    const auto range = s.difference(0, 32); const auto expected = temporal::accumulate_innovations({1, 2}, maps, innovations);
    const auto actual = range.field_evolution(10);
    check(actual.accumulation.transported_initial == expected.transported_initial &&
          actual.accumulation.propagated_innovations == expected.propagated_innovations, "random multi-time channels differ");
    const auto gram = range.moments({{10, {}}}, 20);
    for (const auto& channel : gram.channels) vector(oracle, channel.target_field);
    matrix(oracle, gram.gram);
    save(out / "random.rfcxs", encode_native_series(s));
    auto imported = decode_native_series(encode_native_series(s));
    equal(*imported.at(17).pin(), endpoints[17]);
    check(imported.difference(0, 32).moments({{10, {}}}, 20).gram == gram.gram, "disk multi-time gram differs");
    s.repack_checkpoints({16, 32});
    check(s.difference(0, 32).moments({{10, {}}}, 20).gram == gram.gram, "compaction changed channel moments");
}
void typed_moments() {
    auto a = endpoint(0, {1, 2});
    a.carriers.push_back({2, TemporalCarrierKind::Explicit, {}, {}, 71, numeric::IndexDomain::linear(1)});
    a.tensors.push_back(field_tensor(30, 2, {4})); a.tensors.push_back(metric_tensor(31, 2, M::from_rows({{2}})));
    TemporalTensor map; map.key = 40; map.role = TemporalTensorRole::Accession; map.input = 1; map.output = 2;
    map.tensor = temporal_matrix_tensor(M::from_rows({{1, -1}})); a.tensors.push_back(map);
    auto b = a; b.sequence = 1; b.tensors[0] = field_tensor(10, 1, {3, 5}); b.tensors[2] = field_tensor(30, 2, {8});
    NativeTemporalSeries s(a, 5); s.append(make_temporal_segment(a, b));
    const auto moments = s.difference(0, 1).moments({{10, 40}, {30, {}}}, 31);
    check(moments.channels.size() == 4 && moments.carrier == 2, "typed common target lost");
    check(moments.gram.get(0, 2) == -8 && moments.gram.get(1, 3) == -8, "typed cross channels lost");
    check(moments.provenance[0].target_map == 40 && moments.provenance[2].target_map == std::nullopt, "map identity missing");
    rejects([&] { (void)s.difference(0, 1).moments({{10, {}}}, 31); }, "equal numeric values authorize wrong carrier");
    auto wide = endpoint(0, {Q(numeric::Integer(1) << 257), Q{0}}); NativeTemporalSeries big(wide, 6);
    check(big.difference(0, 0).field_evolution(10).accumulation.terminal[0] == wide.tensors[0].tensor.state(0).value(), "large rational changed");
    TemporalEndpoint zero(path(0));
    zero.carriers.push_back({1, TemporalCarrierKind::SourceGrade, core::Grade{1}, {}, 0, numeric::IndexDomain::linear(0)});
    zero.tensors.push_back(field_tensor(10, 1, {}));
    NativeTemporalSeries growing(zero, 91); const auto first = endpoint(1, {0});
    growing.append(make_temporal_segment(zero, first));
    check(growing.difference(0, 1).correspondence(1).cols() == 0, "zero-dimensional source transport");
    check(growing.difference(0, 1).field_evolution(10).accumulation.propagated_innovations[0] == Vec{Q{0}},
          "zero-amplitude relation birth invented innovation");
    check(growing.tail().pin()->source.grade(core::Grade{1}).cell_count() == 1, "zero birth not retained");
    auto unsupported = a; unsupported.sequence = 1; unsupported.tensors[0].tensor = numeric::SparseRationalTensor(
        numeric::IndexDomain::linear(2), numeric::StructuralSupport::sparse(2, {0}), numeric::SparseRationalVector(2, {{0, Q{3}}}));
    NativeTemporalSeries support(a, 9); support.append(make_temporal_segment(a, unsupported)); support.checkpoint(1);
    const auto restored = decode_native_series(encode_native_series(support)).tail();
    check(restored.endpoint().tensors[0].tensor.support().count() == 1 && !restored.endpoint().tensors[0].tensor.state(1), "support boundary lost");
}
void malformed(const std::filesystem::path& out) {
    PaperHistory h; NativeTemporalSeries s(h.a, 700); s.append_stream(stream(h)); s.checkpoint(41);
    const auto bytes = encode_native_series(s);
    std::size_t truncations = 0;
    for (std::size_t n = 0; n < bytes.size(); ++n) { rejects([&] { (void)decode_native_series(std::span<const std::byte>(bytes).first(n)); }, "truncated archive accepted"); ++truncations; }
    auto bad = bytes; bad[12] = std::byte{1}; rejects([&] { (void)decode_native_series(bad); }, "unsupported flags accepted");
    bad = bytes; bad[8] = std::byte{2}; rejects([&] { (void)decode_native_series(bad); }, "unsupported version accepted");
    bad = bytes; bad.back() ^= std::byte{1}; rejects([&] { (void)decode_native_series(bad); }, "unhashed corruption accepted");
    bad = bytes; bad.push_back(std::byte{0}); rejects([&] { (void)decode_native_series(bad); }, "trailing archive byte accepted");
    NativeSeriesLimits limits; limits.max_archive_bytes = bytes.size() - 1;
    rejects([&] { (void)decode_native_series(bytes, limits); }, "archive budget ignored");
    // Fully valid but wrong checkpoint endpoint with refreshed inner/outer hashes.
    rfcx_detail::Reader body(bytes, 32); const auto ns = body.u64();
    const auto blob = [&](rfcx_detail::Reader& r) { const auto n = r.u64(); return r.slice(static_cast<std::size_t>(n)); };
    auto base = blob(body), channels = blob(body); const auto count = body.u64(); std::vector<std::vector<std::byte>> frames;
    for (std::uint64_t i = 0; i < count; ++i) { auto f = blob(body); frames.emplace_back(f.begin(), f.end()); }
    check(body.u64() == 1, "malformed fixture checkpoint count"); const auto seq = body.u64(); (void)blob(body); (void)blob(body);
    auto false_checkpoint = h.b; false_checkpoint.tensors[0] = field_tensor(10, 1, {0, 0, 0});
    rfcx_detail::Writer w; w.raw(native_series_codec_detail::magic); w.u32(1); w.u32(0); w.u64(0); w.u64(0); w.u64(ns);
    native_series_codec_detail::blob(w, base); native_series_codec_detail::blob(w, channels); w.u64(frames.size());
    for (const auto& f : frames) native_series_codec_detail::blob(w, f);
    w.u64(1); w.u64(seq); native_series_codec_detail::blob(w, encode_rfcx(false_checkpoint.source).bytes);
    native_series_codec_detail::blob(w, encode_temporal_endpoint(false_checkpoint));
    w.patch_u64(16, w.size()); auto forged = w.bytes(); rehash(forged);
    rejects([&] { (void)decode_native_series(forged); }, "valid changed checkpoint rewrote history");
    save(out / "wrong-checkpoint.rfcxs", forged);
    std::mt19937 random(905); std::size_t accepted = 0, rejected = 0;
    for (std::size_t trial = 0; trial < 300; ++trial) {
        auto mutated = bytes; const auto i = 32 + random() % (bytes.size() - 32); mutated[i] ^= std::byte(1u << (random() % 8)); rehash(mutated);
        std::optional<NativeTemporalSeries> result;
        try { result.emplace(decode_native_series(mutated)); }
        catch (const std::exception&) { ++rejected; }
        if (result) { check(encode_native_series(*result) == mutated, "accepted noncanonical mutation"); ++accepted; }
    }
    check(accepted + rejected == 300, "mutation count");
    std::ofstream report(out / "malformed.txt"); report << "truncations=" << truncations << " mutations=300 accepted=" << accepted << " rejected=" << rejected << '\n';
}
} // namespace
int main(int argc, char** argv) {
    try {
        const auto out = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path() / "rfc-series-tests";
        if (std::filesystem::exists(out)) { if (argc > 1) throw std::invalid_argument("use a fresh output directory"); std::filesystem::remove_all(out); }
        std::filesystem::create_directories(out);
        basics(out); chronology(); target_appends(); endpoint_visits(); limits_and_streams(); migration_and_indexes(out); random_history(out); typed_moments(); malformed(out);
        std::cout << "native_series_checks=" << checks << " passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL after " << checks << ": " << e.what() << '\n'; return 1; }
}
