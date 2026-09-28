#pragma once
#include "rfc/storage/native_defects.hpp"
#include "rfc/storage/temporal_diagnostics.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include "rfc/field/connection.hpp"
#include <fstream>
#include <filesystem>
#include <random>
#include <sstream>

namespace native_defect_fixture {
using namespace rfc;
using namespace rfc::storage;
using Q = numeric::Rational;
using M = numeric::ExactMatrix;
inline StableCellKey cell(std::uint16_t k, std::uint64_t id) { return {core::Grade{k}, id}; }
inline StableOccurrenceKey occ(std::uint16_t k, std::uint64_t id) { return {core::Grade{k}, core::OccurrenceKey{id}}; }
inline NativeRouteKey route(std::uint64_t upper, std::uint64_t lower) { return {occ(2, upper), occ(1, lower)}; }
inline std::uint64_t carrier_key(StableCellKey c) { return 1000000 * std::uint64_t(c.grade.value) + c.key; }
inline core::RelationalComplex source(std::size_t triangles = 1, bool twist = false, bool tag = false,
                                      bool reorder = false, bool reattach = false, bool activate_zero = false) {
    core::RelationSignature sig;
    const auto marked = sig.aspects().add("retained-marker");
    const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 5, 5);
    const auto face = sig.add_type("face", core::BoundaryPolicy::DeclaredExact, 3, 3);
    core::RelationalComplexBuilder builder(sig); builder.declare_grade(core::Grade{3});
    for (std::size_t t = 0; t < triangles; ++t) {
        const std::uint64_t v = 10 + t * 10, e = 100 + t * 10, f = 200 + t * 10;
        builder.declare_witness_identity(v + 9);
        std::map<unsigned, std::uint32_t> edge_index;
        for (const unsigned j : reorder ? std::vector<unsigned>{2, 0, 1} : std::vector<unsigned>{0, 1, 2}) {
            const auto a = v + j, b = v + (j + 1) % 3;
            const auto id = 1000 + t * 100 + j * 10;
            const bool changed = twist && t == 0 && j == 0;
            const Q q = changed ? Q{2} : Q{1};
            const auto repeated = reattach && t == 0 && j == 0 ? b : a;
            std::vector<core::BoundaryOccurrenceInput> legs{
                {a, true, !changed, q, {{marked, tag}}, core::OccurrenceKey{id}},
                {b, true, changed, q, {}, core::OccurrenceKey{id + 1}},
                {repeated, true, false, Q{1}, {}, core::OccurrenceKey{id + 2}},
                {repeated, true, true, Q{1}, {}, core::OccurrenceKey{id + 3}},
                {v + 9, activate_zero, false, Q{0}, {{marked, true}}, core::OccurrenceKey{id + 4}}};
            if (reorder) std::reverse(legs.begin(), legs.end());
            edge_index[j] = builder.add_grade_one(core::RelationKey{e + j}, edge, legs).index;
        }
        std::vector<core::HigherIncidenceInput> legs;
        for (unsigned j = 0; j < 3; ++j)
            legs.push_back({edge_index.at(j), true, twist && t == 0 && j == 0,
                            twist && t == 0 && j == 0 ? Q{1, 2} : Q{1}, {}, core::OccurrenceKey{1000000000 + t * 100 + j}});
        if (reorder) std::reverse(legs.begin(), legs.end());
        builder.add_higher(core::Grade{2}, core::RelationKey{f}, face, legs);
    }
    return builder.freeze();
}
inline NativeDefectQuery complete(std::uint64_t key = 1, std::uint16_t grade = 2) {
    NativeDefectQuery q; q.key = key; q.upper_grade = core::Grade{grade}; return q;
}
inline std::vector<NativeDefectQuery> queries() {
    auto partial = complete(2); partial.selection = NativeDefectSelection::Occurrences;
    partial.occurrences = {occ(2, 1000000000), occ(1, 1000)};
    auto between = complete(3); between.endpoints = NativeDefectBlockKey{cell(0, 10), cell(2, 200)};
    auto restricted = partial; restricted.key = 4; restricted.endpoints = between.endpoints;
    return {complete(), partial, between, restricted, complete(5, 3)};
}
inline void add_stalks(TemporalEndpoint& endpoint, std::size_t lower = 2, std::size_t middle = 2, std::size_t upper = 2) {
    for (std::size_t k = 0; k < endpoint.source.grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        for (std::size_t i = 0; i < endpoint.source.grade(grade).cell_count(); ++i) {
            const auto key = stable_cell_key(endpoint.source, {grade, static_cast<std::uint32_t>(i)});
            const auto dim = k == 0 ? lower : k == 1 ? middle : upper;
            endpoint.carriers.push_back({carrier_key(key), TemporalCarrierKind::CellStalk, grade, key, carrier_key(key) + 10000000,
                                         numeric::IndexDomain::linear(dim)});
        }
    }
}
inline NativeDefectRealization realization(const TemporalEndpoint& endpoint) {
    NativeDefectRealization r; r.kind = NativeDefectRealizationKind::LinearStalks;
    for (const auto& c : endpoint.carriers) if (c.kind == TemporalCarrierKind::CellStalk) r.stalk_carriers.push_back(c.key);
    return r;
}
inline void set_map(TemporalEndpoint& endpoint, StableOccurrenceKey key, const M& map,
                    TemporalTensorRole role = TemporalTensorRole::RealizedIncidenceMap) {
    const auto slot = resolve_stable_occurrence(endpoint.source, key).value();
    const auto& arena = endpoint.source.grade(key.upper_grade);
    const auto upper = stable_cell_key(endpoint.source, {key.upper_grade, arena.upper_index[slot]});
    const auto lower = stable_cell_key(endpoint.source, {core::Grade{static_cast<std::uint16_t>(key.upper_grade.value - 1)}, arena.lower_index[slot]});
    TemporalTensor t; t.key = 100000000 + std::uint64_t(key.upper_grade.value) * 1000000 + key.key.value;
    t.role = role; t.input = carrier_key(upper); t.output = carrier_key(lower);
    t.owner.kind = TemporalOwnerKind::Occurrence; t.owner.occurrence = key;
    t.tensor = temporal_matrix_tensor(map);
    std::erase_if(endpoint.tensors, [&](const auto& x) { return x.key == t.key; }); endpoint.tensors.push_back(std::move(t));
}
inline TemporalEndpoint endpoint(std::size_t n = 1, bool stalks = false) {
    TemporalEndpoint e(source(n)); if (stalks) add_stalks(e); return e;
}
inline M global_boundary(const TemporalEndpoint& e, core::Grade grade, bool stalks) {
    if (!stalks) return field::boundary_matrix(e.source, grade);
    const auto system = temporal_linear_fiber_system(e, realization(e).stalk_carriers);
    const auto upper = system.stalks().offsets(grade);
    const auto lower = system.stalks().offsets(core::Grade{static_cast<std::uint16_t>(grade.value - 1)});
    M result(lower.back(), upper.back());
    const auto& arena = e.source.grade(grade);
    for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
        if (!arena.aspects.get(e.source.signature().existence_aspect(), p)) continue;
        const auto flag = system.flag_map(grade, p);
        for (const auto& [ij, q] : flag.transport.entries()) {
            const auto row = lower.at(arena.lower_index[p]) + ij.first, col = upper.at(arena.upper_index[p]) + ij.second;
            result.set(row, col, result.get(row, col) + q);
        }
    }
    return result;
}
inline std::map<StableCellKey, std::size_t> offsets(const TemporalEndpoint& e, core::Grade grade, bool stalks) {
    std::map<StableCellKey, std::size_t> out; std::size_t offset = 0;
    const auto cs = temporal_endpoint_detail::carriers(e);
    for (std::size_t i = 0; i < e.source.grade(grade).cell_count(); ++i) {
        const auto c = stable_cell_key(e.source, {grade, static_cast<std::uint32_t>(i)}); out[c] = offset;
        offset += stalks ? cs.at(carrier_key(c))->domain.logical_size() : 1;
    }
    return out;
}
inline M indexed_defect(const NativeDefectView& v, core::Grade grade, bool stalks) {
    const auto e = v.endpoint(); const auto upper = offsets(*e, grade, stalks);
    const auto lower = offsets(*e, core::Grade{static_cast<std::uint16_t>(grade.value - 2)}, stalks);
    const auto A = global_boundary(*e, grade, stalks), B = global_boundary(*e, core::Grade{static_cast<std::uint16_t>(grade.value - 1)}, stalks);
    M result(B.rows(), A.cols());
    std::set<NativeDefectBlockKey> visited;
    for (auto key : v.route_keys()) {
        const auto& r = v.route(key); if (key.upper.upper_grade != grade) continue;
        const NativeDefectBlockKey bk{r.lower, r.upper}; if (!visited.insert(bk).second) continue;
        const auto block = v.block(bk).value();
        for (const auto& [ij, q] : block.entries()) result.set(lower.at(bk.lower) + ij.first, upper.at(bk.upper) + ij.second, q);
    }
    return result;
}
inline void save(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("defect fixture write failure");
}
inline void write_matrix(std::ostream& out, const M& m) {
    out << m.rows() << ' ' << m.cols();
    for (std::size_t i = 0; i < m.rows(); ++i) for (std::size_t j = 0; j < m.cols(); ++j) out << ' ' << m.get(i, j).get_str();
    out << '\n';
}
} // namespace native_defect_fixture
