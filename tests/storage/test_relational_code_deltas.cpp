#include "rfc/storage/temporal_seed.hpp"
#include "rfc/storage/temporal_segments.hpp"
#include "rfc/storage/relational_codes.hpp"
#include <iostream>
#include <random>

#ifndef RFC_TEST_SOURCE_DIR
#define RFC_TEST_SOURCE_DIR "."
#endif
using namespace rfc;
using namespace rfc::storage;
namespace {
using R = numeric::Rational;
using V = RelationalCodeValue;
using TK = RelationalCodeTypeKind;
using OP = RelationalCodeOpcode;
std::size_t checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template <class F> void rejects(F action, const char *message) {
    bool rejected = false;
    try { action(); } catch (const std::exception &) { rejected = true; }
    check(rejected, message);
}
core::RelationalComplex source(bool flip = false) {
    core::RelationSignature sig;
    const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(sig);
    b.declare_witness_identity(10); b.declare_witness_identity(11);
    b.add_grade_one(core::RelationKey{100}, edge,
        {{10, true, !flip, R{1}, {}, core::OccurrenceKey{1000}},
         {11, true, flip, R{1}, {}, core::OccurrenceKey{1001}}});
    return b.freeze();
}
V vector(std::size_t n, const std::vector<std::size_t> &absent = {}) {
    std::vector<std::size_t> support;
    std::vector<numeric::SparseRationalEntry> values;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::find(absent.begin(), absent.end(), i) != absent.end()) continue;
        support.push_back(i);
        if (i != 0 && i != n - 1) values.push_back({i, R(static_cast<unsigned long>(i + 1))});
    }
    return V{numeric::SparseRationalTensor(numeric::IndexDomain({n}),
        numeric::StructuralSupport::sparse(n, support), numeric::SparseRationalVector(n, values))};
}
void value_at(V &value, std::size_t at, const R &q, bool present = true) {
    const auto &old = std::get<0>(value.data);
    std::vector<numeric::SparseRationalEntry> values;
    std::vector<std::size_t> support;
    old.support().for_each_active([&](auto i) { if (i != at) support.push_back(i); });
    for (const auto &e : old.values().entries()) if (e.index != at) values.push_back(e);
    if (present) support.push_back(at);
    if (q != 0) values.push_back({at, q});
    const auto n = old.domain().logical_size();
    value = V{numeric::SparseRationalTensor(old.domain(), numeric::StructuralSupport::sparse(n, support),
                                           numeric::SparseRationalVector(n, values))};
}
RelationalCodeOperation identity(std::uint64_t id, std::uint64_t type) {
    RelationalCodeOperation o;
    o.key = id; o.input = type; o.output = type; o.opcode = OP::Identity;
    return o;
}
RelationalCodeState state(std::size_t n = 48, std::size_t count = 8) {
    RelationalCodeState s;
    s.namespace_key = 9001;
    RelationalCodeType t{1, TK::Rational, {}, {}};
    // Deliberately not numerically sorted, and coordinate identity zero is legal.
    for (std::size_t i = 0; i < n; ++i) t.atoms.push_back(i == 0 ? 0 : 10000 - i);
    s.types.push_back(std::move(t));
    s.types.push_back({2, TK::Finite, {0, 1, 2}, {}});
    s.types.push_back({3, TK::Product, {}, {{700, 2, 1, {}}, {701, 2, 0, {}}}});
    s.types.push_back({4, TK::Product, {}, {{41, 1, 0, {}}, {42, 3, 1, {}}}});
    s.types.push_back({5, TK::Finite, {99}, {}}); // unused declaration for deletion tests
    s.fields = {100};
    s.operations = {identity(10, 1), identity(11, 2)};
    for (std::size_t i = 1; i <= count; ++i) {
        RelationalCodeSection section;
        section.key = i; section.field = 100; section.type = 1;
        section.cell = StableCellKey{core::Grade{0}, 10}; section.value = vector(n);
        s.sections.push_back(std::move(section));
    }
    RelationalCodeSection product;
    product.key = 100000; product.field = 100; product.type = 4;
    product.value = V{V::Product{vector(n), V{V::Product{V{std::uint64_t{0}}, V{std::uint64_t{1}}}}}};
    s.sections.push_back(std::move(product));
    if (count >= 2) s.interfaces = {{1, 1, 2, 10, 10}};
    return s;
}
TemporalEndpoint endpoint(std::uint64_t sequence = 0, std::size_t n = 48, std::size_t count = 8) {
    TemporalEndpoint e(source(), sequence); e.codes = state(n, count); return e;
}
std::vector<std::byte> state_bytes(const RelationalCodeState &s) { return temporal_codec_detail::code_state_bytes(s); }
void write_bytes(const std::filesystem::path &p, const std::vector<std::byte> &bytes) {
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("fixture write failed");
}
std::vector<std::byte> read_bytes(const std::filesystem::path &p) {
    std::ifstream in(p, std::ios::binary);
    std::vector<std::byte> bytes(std::filesystem::file_size(p));
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in) throw std::runtime_error("fixture read failed");
    return bytes;
}
void patch64(std::vector<std::byte> &v, std::size_t off, std::uint64_t n) {
    for (unsigned i = 0; i < 8; ++i) v.at(off + i) = static_cast<std::byte>((n >> (i * 8)) & 255u);
}
void rehash(std::vector<std::byte> &v) { patch64(v, 24, rfcx_detail::fnv1a64(std::span<const std::byte>(v).subspan(32))); }

void support_and_factor_edits(const std::filesystem::path &dir) {
    auto base = endpoint(), next = base; next.sequence = 1;
    // Remove a supported zero; clear a nonzero without deleting its support;
    // introduce an arbitrary-precision rational on coordinate identity zero.
    value_at(next.codes->sections[0].value, 47, R{0}, false);
    value_at(next.codes->sections[0].value, 1, R{0});
    R large(numeric::Integer("123456789012345678901234567890123456789"), numeric::Integer("970000000000000000000003"));
    large.canonicalize(); value_at(next.codes->sections[0].value, 0, large);
    auto &tuple = std::get<2>(next.codes->sections.back().value.data);
    std::get<2>(tuple[1].data)[0] = V{std::uint64_t{2}};
    const auto s = make_temporal_segment(base, next);
    check(s.codes && s.codes->patch && !s.codes->target, "automatic sparse code selection");
    const auto &p = *s.codes->patch;
    check(p.section_edits.size() == 2 && p.sections.empty(), "sparse section leaves selected");
    check(p.types.empty() && p.operations.empty(), "unchanged declarations copied");
    check(p.section_edits[1].leaves[0].factors == std::vector<std::uint64_t>({42, 700}), "stable nested factor path");
    const auto bytes = encode_temporal_segment(s);
    check(std::to_integer<unsigned>(bytes[12]) == 4, "sparse extension bit");
    const auto parsed = decode_temporal_segment(bytes);
    check(encode_temporal_segment(parsed) == bytes, "sparse canonical bytes");
    const auto replayed = replay_temporal_segment(base, parsed);
    check(encode_temporal_endpoint(replayed) == encode_temporal_endpoint(next), "exact sparse endpoint");
    const auto &v = std::get<0>(replayed.codes->sections[0].value.data);
    check(!v.state(47) && v.state(1) == R{0} && v.state(0) == large, "support zero and exact values");
    check(encode_temporal_endpoint(base) == encode_temporal_endpoint(endpoint()), "base endpoint mutated");
    const auto full = make_temporal_segment(base, next, {}, {}, RelationalCodeEncoding::Full);
    check(full.codes && full.codes->target && !full.codes->patch, "forced legacy full update");
    check(std::to_integer<unsigned>(encode_temporal_segment(full)[12]) == 2, "full extension bit unchanged");
    check(encode_temporal_segment(full).size() > bytes.size(), "sparse update did not shrink fixture");
    write_bytes(dir / "sparse.rfcxt", bytes); write_bytes(dir / "full.rfcxt", encode_temporal_segment(full));
    write_bytes(dir / "base.rfcxe", encode_temporal_endpoint(base));
    write_bytes(dir / "target.rfcxe", encode_temporal_endpoint(next));
    // Support births with exactly zero values also require records.
    auto third = next; third.sequence = 2; value_at(third.codes->sections[0].value, 47, R{0}, true);
    const auto add = make_temporal_segment(next, third, {}, {}, RelationalCodeEncoding::Sparse);
    check(add.codes->patch->section_edits[0].leaves[0].values.empty(), "support-only change fabricated amplitude");
    check(encode_temporal_endpoint(replay_temporal_segment(next, add)) == encode_temporal_endpoint(third), "support birth replay");
    write_bytes(dir / "support_birth.rfcxt", encode_temporal_segment(add));
    std::cout << "sparse_bytes=" << bytes.size() << " full_bytes=" << encode_temporal_segment(full).size() << '\n';
}

void declarations_and_new_directions() {
    auto a = endpoint(), b = a; b.sequence = 1;
    b.codes->types.pop_back(); // unused type 5
    b.codes->fields.push_back(777);
    b.codes->types.push_back({88, TK::Finite, {5, 7}, {}});
    RelationalCodeSection born; born.key = 600; born.field = 777; born.type = 88; born.value = V{std::uint64_t{7}};
    b.codes->sections.push_back(born);
    b.codes->sections.erase(b.codes->sections.begin() + 2);
    b.codes->interfaces.clear();
    b.codes->operations.push_back(identity(80, 88));
    const auto p = make_relational_code_patch(*a.codes, *b.codes);
    check(p.removed_types == std::vector<std::uint64_t>{5} && p.types.size() == 1, "type declaration births/removals");
    check(p.removed_sections == std::vector<std::uint64_t>{3} && p.sections[0].birth, "section identity edits");
    check(state_bytes(replay_relational_code_patch(*a.codes, p)) == state_bytes(*b.codes), "complete directory replay");
    auto bad = p; bad.types.push_back(a.codes->types[0]);
    rejects([&] { (void)replay_relational_code_patch(*a.codes, bad); }, "persistent type replaced");
    // Rectangular carrier growth retains a full target section, including a new
    // direction outside the transported old carrier image.
    a = endpoint(); b = a; b.sequence = 1;
    auto new_type = a.codes->types[0]; new_type.key = 99; new_type.atoms.push_back(777777);
    b.codes->types.push_back(new_type);
    RelationalCodeOperation map; map.key = 99; map.input = 1; map.output = 99; map.opcode = OP::Linear;
    std::vector<numeric::SparseRationalEntry> entries; std::vector<std::size_t> support;
    for (std::size_t i = 0; i < 48; ++i) { support.push_back(i * 48 + i); entries.push_back({i * 48 + i, R{1}}); }
    map.kernel = numeric::SparseRationalTensor(numeric::IndexDomain({49, 48}),
        numeric::StructuralSupport::sparse(49 * 48, support), numeric::SparseRationalVector(49 * 48, entries));
    b.codes->operations.push_back(map); b.codes->sections[0].type = 99;
    b.codes->sections[0].value = vector(49); value_at(b.codes->sections[0].value, 48, R{17});
    b.codes->interfaces.clear();
    rejects([&] { (void)make_temporal_segment(a, b); }, "missing rectangular transport accepted");
    const auto segment = make_temporal_segment(a, b, {}, {{1, 99}}, RelationalCodeEncoding::Sparse);
    check(segment.codes->patch->sections[0].target.type == 99 && segment.codes->patch->section_edits.empty(), "carrier growth needs full target");
    const auto result = replay_temporal_segment(a, decode_temporal_segment(encode_temporal_segment(segment)));
    check(std::get<0>(result.codes->sections[0].value.data).state(48) == R{17}, "new direction lost");
    // Operation identity differs even when its action on the current section is unchanged.
    a = endpoint(); b = a; b.sequence = 1;
    RelationalCodeOperation linear; linear.key = 100; linear.input = 1; linear.output = 1; linear.opcode = OP::Linear;
    linear.kernel = numeric::SparseRationalTensor(numeric::IndexDomain({48, 48}),
        numeric::StructuralSupport::sparse(48 * 48, {47}), numeric::SparseRationalVector(48 * 48, {{47, R{1}}}));
    a.codes->operations.push_back(linear); b.codes = a.codes;
    b.codes->operations.back().kernel = numeric::SparseRationalTensor(numeric::IndexDomain({48, 48}),
        numeric::StructuralSupport::sparse(48 * 48, {47}), numeric::SparseRationalVector(48 * 48, {{47, R{19}}}));
    const auto op = make_temporal_segment(a, b, {}, {}, RelationalCodeEncoding::Sparse);
    check(op.codes->patch->operations.size() == 1 && op.codes->patch->sections.empty() && op.codes->patch->section_edits.empty(), "operation-only delta lost");
    check(relational_code_operation_bytes(*replay_temporal_segment(a, op).codes, 100) != relational_code_operation_bytes(*a.codes, 100), "operation history merged");
    b = a; b.sequence = 1;
    const auto t1 = make_temporal_segment(a, b, {}, {{1, 10}}, RelationalCodeEncoding::Sparse);
    const auto t2 = make_temporal_segment(a, b, {}, {{1, 100}}, RelationalCodeEncoding::Sparse);
    check(encode_temporal_segment(t1) != encode_temporal_segment(t2), "transports with same endpoint merged");
    check(encode_temporal_endpoint(replay_temporal_segment(a, t1)) == encode_temporal_endpoint(replay_temporal_segment(a, t2)), "explicit transport changed declared target");
    auto removed = b; removed.sequence = 2; removed.codes.reset();
    const auto drop = make_temporal_segment(b, removed, {}, {}, RelationalCodeEncoding::Sparse);
    check(drop.codes && !drop.codes->patch && !drop.codes->target, "layer removal needs full removal record");
    check(!replay_temporal_segment(b, drop).codes, "code layer removal replay");
    auto reborn = removed; reborn.sequence = 3; reborn.codes = state();
    check(encode_temporal_endpoint(replay_temporal_segment(removed, make_temporal_segment(removed, reborn))) == encode_temporal_endpoint(reborn), "layer birth replay");
}

void malformed_and_canonical(const std::filesystem::path &dir) {
    auto a = endpoint(), b = a; b.sequence = 1;
    value_at(b.codes->sections[0].value, 1, R{0});
    value_at(b.codes->sections[0].value, 47, R{0}, false);
    const auto s = make_temporal_segment(a, b, {}, {}, RelationalCodeEncoding::Sparse);
    const auto good = *s.codes->patch;
    const auto reject_patch = [&](auto mutate) {
        auto p = good; mutate(p);
        rejects([&] { (void)replay_relational_code_patch(*a.codes, p); }, "malformed sparse patch accepted");
    };
    reject_patch([](auto &p) { ++p.namespace_key; });
    reject_patch([](auto &p) { ++p.base_state_hash; });
    reject_patch([](auto &p) { ++p.target_state_hash; });
    reject_patch([](auto &p) { p.section_edits.push_back(p.section_edits.front()); });
    reject_patch([](auto &p) { p.section_edits.front().leaves.push_back(p.section_edits.front().leaves.front()); });
    reject_patch([](auto &p) { p.section_edits.front().type = 2; });
    reject_patch([](auto &p) { p.section_edits.front().section = 9999999; });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().factors = {41}; });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().kind = static_cast<TK>(255); });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().removed_support = {888888}; });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().values.front().coordinate = 888888; });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().values.push_back(p.section_edits.front().leaves.front().values.front()); });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().values.front().target = R{2}; });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().added_support = {0}; });
    reject_patch([](auto &p) { p.section_edits.front().leaves.front().removed_support.push_back(9998); }); // nonzero without clear
    reject_patch([](auto &p) { p.removed_sections.push_back(1); });
    reject_patch([&](auto &p) { p.sections.push_back({false, b.codes->sections[0]}); });
    reject_patch([](auto &p) { p.removed_fields = {100, 100}; });
    reject_patch([](auto &p) { p.section_edits.front().leaves.clear(); });
    auto wrong_base = *a.codes; value_at(wrong_base.sections[1].value, 1, R{3});
    rejects([&] { (void)replay_relational_code_patch(wrong_base, good); }, "wrong base binding accepted");
    auto ambiguous = s; ambiguous.codes->target = b.codes;
    rejects([&] { (void)encode_temporal_segment(ambiguous); }, "ambiguous update encoded");
    rejects([&] { (void)replay_temporal_segment(a, ambiguous); }, "ambiguous update replayed");
    const auto bytes = encode_temporal_segment(s);
    for (std::size_t n = 0; n < bytes.size(); ++n)
        rejects([&] { (void)decode_temporal_segment(std::span<const std::byte>(bytes).first(n)); }, "truncated sparse frame accepted");
    TemporalReadLimits limits; limits.max_records = 1;
    rejects([&] { (void)decode_temporal_segment(bytes, limits); }, "record budget bypass");
    limits = {}; limits.max_integer_bytes = 0;
    rejects([&] { (void)decode_temporal_segment(bytes, limits); }, "integer budget bypass");
    auto conflicting = bytes; conflicting[12] = std::byte{6};
    rejects([&] { (void)decode_temporal_segment(conflicting); }, "conflicting wire extensions accepted");
    TemporalSegmentStreamDecoder decoder;
    rejects([&] { decoder.feed(std::span<const std::byte>(conflicting).first(32), [](const auto &, auto) {}); }, "conflicting stream flags not rejected in header");
    auto shuffled = s;
    auto &leaf = shuffled.codes->patch->section_edits[0].leaves[0];
    std::reverse(leaf.values.begin(), leaf.values.end());
    check(encode_temporal_segment(shuffled) == bytes, "set-like edit order changes canonical bytes");
    // Duplicate path is syntactically serializable, but replay must reject it.
    auto duplicate = s; duplicate.codes->patch->section_edits.push_back(duplicate.codes->patch->section_edits[0]);
    rejects([&] { (void)replay_temporal_segment(a, decode_temporal_segment(encode_temporal_segment(duplicate))); }, "wire duplicate path accepted");
    // Mutations can be valid histories, so compare complete endpoint bytes for
    // any successful replay rather than assuming every mutation must reject.
    std::mt19937_64 rng(0x03d9001);
    std::size_t parsed = 0, replayed = 0;
    for (std::size_t i = 0; i < 400; ++i) {
        auto mutation = bytes; const auto at = 32 + rng() % (bytes.size() - 32);
        mutation[at] ^= static_cast<std::byte>(1u << (rng() % 8)); rehash(mutation);
        std::optional<TemporalSegment> accepted;
        try { accepted = decode_temporal_segment(mutation); } catch (const std::exception &) {}
        if (!accepted) continue;
        ++parsed; check(encode_temporal_segment(*accepted) == mutation, "noncanonical mutation accepted");
        std::optional<TemporalEndpoint> result;
        try { result = replay_temporal_segment(a, *accepted); } catch (const std::exception &) {}
        if (!result) continue;
        ++replayed; check(encode_temporal_endpoint(*result) == encode_temporal_endpoint(b), "mutant replay changed full endpoint");
    }
    std::cout << "truncated_sparse_prefixes=" << bytes.size() << " mutations=400 parsed=" << parsed << " replayed=" << replayed << '\n';
    write_bytes(dir / "malformed_base_valid.rfcxt", bytes);
}

void random_history_and_stream(const std::filesystem::path &dir) {
    std::mt19937_64 gen(90123);
    auto current = endpoint(); const auto initial = current;
    std::vector<std::byte> stream;
    std::vector<TemporalSegment> segments;
    const auto path = dir / "mixed_history.rfcxt";
    std::filesystem::remove(path);
    for (std::size_t step = 0; step < 24; ++step) {
        auto next = current; ++next.sequence;
        for (int j = 0; j < 4; ++j) {
            const auto section = gen() % 8, coord = gen() % 48;
            // Sequence random draws explicitly; argument evaluation order is
            // unspecified and must not change cross-compiler wire fixtures.
            const auto numerator = static_cast<long>(gen() % 31) - 15;
            const auto denominator = static_cast<unsigned long>(gen() % 17 + 1);
            R q(numerator, denominator); q.canonicalize();
            const bool present = gen() % 5 != 0;
            value_at(next.codes->sections[section].value, coord, present ? q : R{0}, present);
        }
        const auto mode = step % 3 == 0 ? RelationalCodeEncoding::Full : RelationalCodeEncoding::Sparse;
        const auto s = make_temporal_segment(current, next, {}, {}, mode);
        const auto bytes = encode_temporal_segment(s);
        check(encode_temporal_endpoint(replay_temporal_segment(current, decode_temporal_segment(bytes))) == encode_temporal_endpoint(next), "random exact replay");
        check(append_temporal_segment(path, current, s) == bytes.size(), "append byte count");
        stream.insert(stream.end(), bytes.begin(), bytes.end()); segments.push_back(s); current = std::move(next);
    }
    check(read_bytes(path) == stream, "append rewrote mixed legacy/sparse frames");
    for (auto chunk : std::vector<std::size_t>{1, 2, 7, 31, 32, 33, 127, 1024, 65536}) {
        TemporalSegmentStreamDecoder d; auto e = initial; std::size_t frames = 0;
        for (std::size_t off = 0; off < stream.size(); off += chunk)
            d.feed(std::span<const std::byte>(stream).subspan(off, std::min(chunk, stream.size() - off)),
                [&](const auto &s, auto raw) {
                    check(encode_temporal_segment(s) == std::vector<std::byte>(raw.begin(), raw.end()), "stream raw bytes changed");
                    e = replay_temporal_segment(e, s); ++frames;
                });
        d.finish(); check(frames == 24, "mixed stream frame count");
        check(encode_temporal_endpoint(e) == encode_temporal_endpoint(current), "mixed stream endpoint");
        std::size_t max_frame = 0; for (const auto &s : segments) max_frame = std::max(max_frame, encode_temporal_segment(s).size());
        check(d.peak_buffered_bytes() <= max_frame, "decoder buffered whole stream");
    }
    const auto before = read_bytes(path);
    auto target = current; ++target.sequence;
    const auto extra = make_temporal_segment(current, target);
    TemporalReadLimits limit; limit.max_records = 24;
    rejects([&] { (void)append_temporal_segment(path, extra, limit); }, "append frame budget bypass");
    check(read_bytes(path) == before, "failed append altered stream");
    auto wrong = extra; ++wrong.base_endpoint_hash;
    rejects([&] { (void)append_temporal_segment(path, wrong); }, "append wrong predecessor");
    check(read_bytes(path) == before, "wrong predecessor modified stream");
    check(encode_temporal_endpoint(initial) == encode_temporal_endpoint(endpoint()), "random replay mutated initial endpoint");
    write_bytes(dir / "history_target.rfcxe", encode_temporal_endpoint(current));
    std::cout << "mixed_frames=24 chunk_sizes=9 stream_bytes=" << stream.size() << '\n';
}

void factorized_large_and_size() {
    auto a = endpoint(), b = a;
    RelationalCodeType product{60, TK::Product, {}, {}}; V::Product values;
    for (std::uint64_t i = 0; i < 131; ++i) {
        product.factors.push_back({1000 + i, 2, static_cast<std::uint16_t>(i % 2), {}});
        values.push_back(V{std::uint64_t{0}});
    }
    a.codes->types.push_back(product);
    RelationalCodeSection s; s.key = 500; s.field = 100; s.type = 60; s.value = V{values};
    a.codes->sections.push_back(s); b = a; b.sequence = 1;
    std::get<2>(b.codes->sections.back().value.data)[130] = V{std::uint64_t{1}};
    const auto segment = make_temporal_segment(a, b);
    check(segment.codes && segment.codes->patch, "large factor patch not selected");
    check(segment.codes->patch->section_edits[0].leaves[0].factors == std::vector<std::uint64_t>{1130}, "131-factor identity lost");
    check(state_bytes(*replay_temporal_segment(a, segment).codes) == state_bytes(*b.codes), "factorized large replay");
    auto small = endpoint(0, 48, 8), small_next = small; small_next.sequence = 1;
    value_at(small_next.codes->sections[0].value, 1, R{9});
    auto big = endpoint(0, 48, 1000), big_next = big; big_next.sequence = 1;
    value_at(big_next.codes->sections[0].value, 1, R{9});
    const auto sb = encode_temporal_segment(make_temporal_segment(small, small_next));
    const auto bb = encode_temporal_segment(make_temporal_segment(big, big_next));
    check(sb.size() == bb.size(), "wire sparse edit size depends on unchanged section count");
    const auto full = encode_temporal_segment(make_temporal_segment(big, big_next, {}, {}, RelationalCodeEncoding::Full));
    check(full.size() > bb.size() * 100, "large fixture sparse storage reduction");
    std::cout << "1000_sections_sparse_bytes=" << bb.size() << " full_bytes=" << full.size() << '\n';
}

void source_views_and_finite(const std::filesystem::path &dir) {
    auto a = endpoint();
    a.codes->types.push_back({200, TK::Finite, {0, 1}, {}});
    a.codes->types.push_back(relational_aspect_code_type(a.source, 201, 200,
        {{300, a.source.signature().existence_aspect()}, {301, a.source.signature().orientation_aspect()}}));
    for (std::uint64_t i = 0; i < 2; ++i)
        a.codes->sections.push_back(relational_aspect_code_section(100001 + i, 100, 201,
            {core::Grade{1}, core::OccurrenceKey{1000 + i}}));
    auto b = a; b.sequence = 1; b.source = source(true);
    // Preserve the shared-interface equality while editing both local sections.
    value_at(b.codes->sections[0].value, 1, R{7});
    value_at(b.codes->sections[1].value, 1, R{7});
    b.finite_systems.push_back({702, TemporalFiniteValueKind::SignedInteger, {}, {}, {}});
    const auto s = make_temporal_segment(a, b, {}, {}, RelationalCodeEncoding::Sparse);
    check(s.codes->patch->sections.empty() && s.codes->patch->section_edits.size() == 2,
          "source views redundantly copied in mixed update");
    const auto bytes = encode_temporal_segment(s);
    check(bytes[12] == std::byte{5}, "finite+sparse-code flags not combined");
    const auto rebuilt = replay_temporal_segment(a, decode_temporal_segment(bytes));
    check(encode_temporal_endpoint(rebuilt) == encode_temporal_endpoint(b), "mixed-source finite code endpoint");
    check(relational_code_section_bytes(a, 100001) != relational_code_section_bytes(rebuilt, 100001),
          "source view observation failed to change with source");
    check(relational_code_glue(rebuilt).front().compatible, "compatible local glue lost in sparse replay");
    auto obstructed = b; obstructed.sequence = 2;
    value_at(obstructed.codes->sections[0].value, 1, R{8});
    auto obstruction = replay_temporal_segment(b, make_temporal_segment(b, obstructed));
    check(!relational_code_glue(obstruction).front().compatible, "sparse replay silently repaired glue obstruction");
    // A sparse-code frame is not a valid endpoint-sidecar extension.
    auto bad_update = s;
    auto copied_view = a.codes->sections.back(); copied_view.value = V{std::uint64_t{1}};
    bad_update.codes->patch->sections.push_back({false, copied_view});
    rejects([&] { (void)encode_temporal_segment(bad_update); }, "sparse writer silently discarded source-view value");
    auto seed = encode_temporal_endpoint(a); seed[12] = std::byte{4};
    rejects([&] { (void)decode_temporal_endpoint(a.source, seed); }, "sidecar accepted patch-only flag");
    const auto source_path = dir / "mixed_source.rfcx", seed_path = dir / "mixed_seed.rfcxe";
    write_rfcx(a.source, source_path); write_temporal_endpoint(a, seed_path);
    MappedRfcx mapped(source_path);
    const auto from_disk = read_temporal_endpoint(mapped, seed_path);
    check(encode_temporal_endpoint(replay_temporal_segment(from_disk, decode_temporal_segment(bytes))) == encode_temporal_endpoint(b), "disk-only mixed replay");
    write_bytes(dir / "finite_code_delta.rfcxt", bytes);
}

void noncanonical_wire() {
    auto a = endpoint(), b = a; b.sequence = 1;
    b.codes->fields.insert(b.codes->fields.end(), {900, 901});
    const auto s = make_temporal_segment(a, b, {}, {}, RelationalCodeEncoding::Sparse);
    const auto canonical = encode_temporal_segment(s);
    auto wire = canonical;
    temporal_codec_detail::Writer pattern;
    temporal_codec_detail::keys(pattern, std::vector<std::uint64_t>{900, 901});
    const auto start = std::search(wire.begin() + 32, wire.end(), pattern.bytes().begin(), pattern.bytes().end());
    check(start != wire.end(), "noncanonical test directory not found");
    const auto off = static_cast<std::size_t>(start - wire.begin()) + 8;
    for (std::size_t i = 0; i < 8; ++i) std::swap(wire[off + i], wire[off + 8 + i]);
    rehash(wire);
    rejects([&] { (void)decode_temporal_segment(wire); }, "unsorted sparse directory accepted as canonical");
    TemporalReadLimits permissive; permissive.require_canonical = false;
    const auto parsed = decode_temporal_segment(wire, permissive);
    check(encode_temporal_segment(parsed) == canonical, "explicit legacy ingestion failed normalization");
    check(encode_temporal_endpoint(replay_temporal_segment(a, parsed)) == encode_temporal_endpoint(b), "normalized sparse directory changed semantics");
    // Unknown operation constructors reject at the wire boundary, even though
    // a sparse patch cannot validate all graph references until replay.
    auto with_op = b; with_op.codes->operations.push_back(identity(99887766, 1));
    const auto op_segment = make_temporal_segment(a, with_op, {}, {}, RelationalCodeEncoding::Sparse);
    auto unknown = encode_temporal_segment(op_segment);
    temporal_codec_detail::Writer op_pattern;
    temporal_codec_detail::code_operation(op_pattern, with_op.codes->operations.back());
    const auto op_at = std::search(unknown.begin() + 32, unknown.end(), op_pattern.bytes().begin(), op_pattern.bytes().end());
    check(op_at != unknown.end(), "operation wire fixture not located");
    unknown[static_cast<std::size_t>(op_at - unknown.begin()) + 24] = std::byte{255}; rehash(unknown);
    rejects([&] { (void)decode_temporal_segment(unknown); }, "unknown sparse opcode decoded");
    auto in_memory = s; std::reverse(in_memory.codes->patch->fields.begin(), in_memory.codes->patch->fields.end());
    check(encode_temporal_segment(in_memory) == canonical, "sparse fields are not canonicalized");
    rejects([&] { (void)make_temporal_segment(a, b, {}, {}, static_cast<RelationalCodeEncoding>(255)); }, "unknown encoding accepted");
}
void frozen_phase03c_compatibility() {
    const auto dir = std::filesystem::path(RFC_TEST_SOURCE_DIR) / "tests/storage/fixtures/phase03c_codes";
    MappedRfcx source_file(dir / "base.rfcx");
    auto base = read_temporal_endpoint(source_file, dir / "base.rfcxe");
    const auto wire = read_bytes(dir / "full.rfcxt");
    const auto parsed = decode_temporal_segment(wire);
    check(parsed.codes && parsed.codes->target && !parsed.codes->patch, "legacy full record reinterpreted");
    check(encode_temporal_segment(parsed) == wire, "frozen legacy code segment bytes changed");
    check(encode_temporal_endpoint(base) == read_bytes(dir / "base.rfcxe"), "frozen legacy sidecar bytes changed");
    auto target = replay_temporal_segment(base, parsed);
    check(encode_temporal_endpoint(target) == read_bytes(dir / "target.rfcxe"), "frozen legacy code replay changed");
    const auto fresh = make_temporal_segment(base, target, {}, {}, RelationalCodeEncoding::Full);
    check(encode_temporal_segment(fresh) == wire, "Full mode not byte compatible with Phase 03C");
}

void observation_maintenance() {
    const auto bytes = [](std::uint64_t n) {
        temporal_codec_detail::Writer w; w.u64(n); return w.bytes();
    };
    // Every key deliberately collides; assertions remain distinct after updates.
    RelationalCodeObservationIndex index([](auto) { return 0; });
    std::map<std::uint64_t, std::uint64_t> expected;
    std::mt19937_64 random(911);
    for (std::uint64_t i = 1; i <= 100; ++i) { index.insert({7, i}, bytes(i % 7)); expected[i] = i % 7; }
    for (std::size_t step = 0; step < 500; ++step) {
        const auto id = random() % 150 + 1, value = random() % 7;
        if (random() % 4 == 0) {
            check(index.erase({7, id}) == (expected.erase(id) != 0), "index erase mismatch");
        } else if (expected.contains(id)) {
            index.replace({7, id}, bytes(value)); expected[id] = value;
        } else { index.insert({7, id}, bytes(value)); expected[id] = value; }
        check(index.size() == expected.size(), "observation index size");
        for (std::uint64_t probe = 0; probe < 7; ++probe) {
            std::vector<RelationalCodeIdentity> want;
            for (const auto &[key, v] : expected) if (v == probe) want.push_back({7, key});
            check(index.find(bytes(probe)) == want, "collision-safe incremental lookup mismatch");
        }
    }
    rejects([&] { index.replace({7, 999999}, bytes(0)); }, "replace missing identity");
    index.insert({8, 1}, bytes(0));
    check(index.contains({8, 1}), "namespace identity merged");
    RelationalCodeObservationIndex moving([](auto b) { return std::to_integer<unsigned>(b[0]); });
    moving.insert({1, 1}, bytes(2)); moving.insert({1, 2}, bytes(3));
    moving.replace({1, 1}, bytes(3));
    check(moving.find(bytes(2)).empty() && moving.find(bytes(3)).size() == 2, "bucket migration failed");
    bool throwing = false;
    RelationalCodeObservationIndex failure([&](auto) -> std::uint64_t {
        if (throwing) throw std::runtime_error("injected selector failure");
        return 0;
    });
    failure.insert({1, 1}, bytes(1)); throwing = true;
    rejects([&] { failure.replace({1, 1}, bytes(2)); }, "throwing selector was ignored");
    rejects([&] { failure.insert({1, 2}, bytes(2)); }, "throwing insert selector ignored");
    throwing = false;
    check(failure.size() == 1 && failure.find(bytes(1)).size() == 1 && failure.find(bytes(2)).empty(), "selector failure changed index");
    // A callback must not invalidate a retained entry while replace is using it.
    RelationalCodeObservationIndex *self = nullptr;
    bool reenter = false;
    RelationalCodeObservationIndex guarded([&](auto) -> std::uint64_t {
        if (reenter) self->erase({1, 1});
        return 0;
    });
    self = &guarded;
    guarded.insert({1, 1}, bytes(1)); reenter = true;
    rejects([&] { guarded.replace({1, 1}, bytes(2)); }, "selector erased in-use observation");
    rejects([&] { guarded.insert({1, 2}, bytes(2)); }, "selector mutated index during insert");
    rejects([&] { (void)guarded.find(bytes(1)); }, "selector mutated index during lookup");
    reenter = false;
    check(guarded.size() == 1 && guarded.find(bytes(1)).size() == 1, "reentry changed observation state");
}
} // namespace
int main(int argc, char **argv) {
    try {
        const auto dir = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path() / "rfc_relational_code_delta_tests";
        std::filesystem::create_directories(dir);
        const auto run = [&](const char *name, auto f) { std::cout << "BEGIN " << name << '\n'; f(); std::cout << "PASS " << name << " checks=" << checks << '\n'; };
        run("support/factor edits", [&] { support_and_factor_edits(dir); });
        run("declarations/new directions", declarations_and_new_directions);
        run("malformed/canonical", [&] { malformed_and_canonical(dir); });
        run("mixed history/stream", [&] { random_history_and_stream(dir); });
        run("factorization/storage", factorized_large_and_size);
        run("source views/finite/glue", [&] { source_views_and_finite(dir); });
        run("noncanonical wire", noncanonical_wire);
        run("frozen Phase 03C compatibility", frozen_phase03c_compatibility);
        run("observation maintenance", observation_maintenance);
        std::cout << "relational_code_delta_checks=" << checks << " passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n'; return 1; }
}
