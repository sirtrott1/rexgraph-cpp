#include "rfc/inference/relational_field_program_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
#include <string_view>

using namespace rfc;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using T = numeric::SparseRationalTensor;
using Program = inference::OwnedRelationalFieldProgram;
using Declaration = inference::RelationalFieldProgramDeclaration;
namespace {
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
T tensor(V values) {
    const auto size = values.size(); std::vector<numeric::SparseRationalEntry> entries;
    for (std::size_t i = 0; i < size; ++i) if (values[i] != 0) entries.push_back({i, values[i]});
    return {numeric::IndexDomain({size}), numeric::StructuralSupport::full(size), numeric::SparseRationalVector(size, std::move(entries))};
}
storage::NativeSeriesEndpoint source() {
    core::RelationSignature signature;
    const auto relation = signature.add_type("relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, relation,
        {{10, true, false, Q{1}}, {20, true, true, Q{1}}, {10, true, true, Q{1}},
         {20, false, false, Q{1}}, {30, true, false, Q{0}}});
    builder.add_grade_one(core::RelationKey{101}, relation,
        {{10, true, false, Q{1}}, {20, true, false, Q{1, 2}}, {30, true, true, Q{1}}});
    storage::TemporalEndpoint endpoint(builder.freeze(), 7);
    return storage::NativeTemporalSeries(endpoint, 991).tail();
}
Declaration declaration() {
    Declaration d; d.key = 51; d.revision = 2; d.differential = true;
    d.stalks = {{{core::Grade{0}, 0}, 1001, {101, 102}, M::from_rows({{2, 1}, {1, 3}})},
                {{core::Grade{0}, 1}, 1002, {201, 202, 203}, M::identity(3)},
                {{core::Grade{0}, 2}, 1001, {101, 102}, M::identity(2)},
                {{core::Grade{1}, 0}, 1003, {301, 302}, M::from_rows({{3, 1}, {1, 2}})},
                {{core::Grade{1}, 1}, 1003, {301, 302}, M::identity(2)}};
    const auto phi = storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences = {{core::Grade{1}, 0, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 1, phi, M::from_rows({{1, 0}, {0, 1}, {1, 1}})},
                     {core::Grade{1}, 2, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 3, phi, M::from_rows({{7, 3}, {2, 5}, {1, 1}})},
                     {core::Grade{1}, 4, phi, M::identity(2)},
                     {core::Grade{1}, 5, phi, M::from_rows({{2, 1}, {1, 3}})},
                     {core::Grade{1}, 6, phi, M::from_rows({{0, 1}, {1, 0}, {2, -1}})},
                     {core::Grade{1}, 7, storage::TemporalTensorRole::RealizedIncidenceMap,
                         M::from_rows({{-1, 1}, {2, -2}})}};
    d.nodes = {{1, inference::FieldProgramNodeKind::Input, core::Grade{1}, {}, {}},
               {2, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 1, {}},
               {3, inference::FieldProgramNodeKind::DeclaredMap, core::Grade{1}, 2,
                   M::from_rows({{1, 0, 1, 0, 0, 0, 0}, {0, 1, 0, 1, 0, 0, 0},
                                 {0, 0, 0, 0, 1, 1, 0}, {1, 0, 0, 0, 0, 0, 1}})},
               {4, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 3, {}}};
    return d;
}
void json(const V& values) {
    std::cout << '['; for (std::size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << '"' << values[i] << '"'; } std::cout << ']';
}
void json(const M& matrix) {
    std::cout << '['; for (std::size_t i = 0; i < matrix.rows(); ++i) {
        if (i) std::cout << ',';
        V row(matrix.cols());
        for (std::size_t j = 0; j < row.size(); ++j) row[j] = matrix.get(i, j);
        json(row);
    } std::cout << ']';
}
void corpus(bool emit) {
    const auto publication = source(); const auto d = declaration(); const Program program(publication, d);
    const auto bytes = inference::encode_relational_field_program(program);
    auto restored = inference::decode_relational_field_program(bytes, {});
    check(inference::encode_relational_field_program(restored) == bytes, "program canonical cold bytes");
    check(program.publication().source_namespace() == 991 && restored.publication().sequence() == 7, "program source namespace and cutoff");
    for (std::size_t trial = 0; trial < 60; ++trial) {
        const V x{Q{static_cast<long>(trial) - 4, 3}, Q{static_cast<long>(trial % 7) - 2, 5},
                  Q{static_cast<long>(trial % 11) - 3, 7}, Q{static_cast<long>(trial % 13) - 6, 2}};
        const auto result = program.execute({{1, 800 + trial, tensor(x)}});
        const auto cold = inference::decode_relational_field_result(inference::encode_relational_field_result(result), {});
        for (const auto key : {1u, 2u, 3u, 4u}) check(result.field(key).values().dense() == cold.field(key).values().dense(), "whole intermediate cold replay");
        check(result.occurrence_field(2).domain().logical_size() == 19, "original inactive and repeated occurrence directions");
        check(result.occurrence_field(2).state(7) == Q{0} && result.occurrence_field(2).state(11) == Q{0}, "inactive and supported zero known state");
        const auto selected = result.select(2, {{core::Grade{0}, 1}, {core::Grade{0}, 0}}, 5);
        check(selected.size() == 2 && selected[0].basis == 1002 && selected[0].field.values().dense() ==
            V({result.field(2).state(2).value(), result.field(2).state(3).value(), result.field(2).state(4).value()}), "bounded named stalk selection");
        check(cold.inputs().at(0).use == 800 + trial, "original use survives cold replay");
        if (emit) {
            std::cout << "{\"input\":"; json(x); std::cout << ",\"maps\":[";
            const auto& arena = publication.endpoint().source.grade(core::Grade{1});
            for (std::size_t p = 0; p < d.occurrences.size(); ++p) {
                if (p) std::cout << ',';
                std::cout << "{\"upper\":" << arena.upper_index[p] << ",\"lower\":" << arena.lower_index[p]
                    << ",\"active\":" << (arena.aspects.get(publication.endpoint().source.signature().existence_aspect(), p) ? "true" : "false")
                    << ",\"coefficient\":\"" << publication.endpoint().source.incidence_coefficient(core::Grade{1}, p)
                    << "\",\"realized\":" << (d.occurrences[p].role == storage::TemporalTensorRole::RealizedIncidenceMap ? "true" : "false") << ",\"matrix\":";
                json(d.occurrences[p].map); std::cout << '}';
            }
            std::cout << "],\"application\":"; json(d.nodes[2].map);
            std::cout << ",\"boundary\":"; json(result.field(2).values().dense());
            std::cout << ",\"occurrences\":"; json(result.occurrence_field(2).values().dense());
            std::cout << ",\"middle\":"; json(result.field(3).values().dense());
            std::cout << ",\"final\":"; json(result.field(4).values().dense());
            std::cout << ",\"final_occurrences\":"; json(result.occurrence_field(4).values().dense()); std::cout << "}\n";
        }
    }
}
void admissions() {
    const auto publication = source(); const auto original = declaration(); const Program program(publication, original);
    auto d = original; d.occurrences.erase(d.occurrences.begin());
    rejects([&] { Program invalid(publication, d); }, "equal dimensional missing map rejected");
    d = original; d.stalks[2].directions = {102, 101};
    rejects([&] { Program invalid(publication, d); }, "repeated basis identity direction order rejected");
    d = original; d.stalks[0].directions = {101, 101};
    rejects([&] { Program invalid(publication, d); }, "duplicate directions rejected");
    d = original; d.stalks[0].metric = M::from_rows({{1, 2}, {2, 1}});
    rejects([&] { Program invalid(publication, d); }, "indefinite metric rejected");
    d = original; d.occurrences[3].role = storage::TemporalTensorRole::RealizedIncidenceMap;
    rejects([&] { Program invalid(publication, d); }, "inactive nonzero realized map rejected");
    d = original; d.occurrences[0].role = storage::TemporalTensorRole::SheafRestriction;
    rejects([&] { Program invalid(publication, d); }, "sheaf restriction cannot stand for incidence map");
    d = original; d.nodes[2].input = 4;
    rejects([&] { Program invalid(publication, d); }, "future program node rejected");
    d = original; d.nodes[2].map = M(4, 4);
    rejects([&] { Program invalid(publication, d); }, "declared map actual carrier shape rejected");
    const auto x = tensor({1, 2, 3, 4});
    rejects([&] { program.execute({}); }, "missing input rejected");
    rejects([&] { program.execute({{1, 0, x}}); }, "unnamed original input use rejected");
    const T partial(numeric::IndexDomain({4}), numeric::StructuralSupport::sparse(4, {0}), numeric::SparseRationalVector(4, {{0, 1}}));
    rejects([&] { program.execute({{1, 1, partial}}); }, "absent directions not invented zeros");
    const auto result = program.execute({{1, 1, x}});
    rejects([&] { result.select(2, {{core::Grade{0}, 1}}, 2); }, "bounded export admission");
    rejects([&] { result.select(2, {{core::Grade{1}, 0}}, 10); }, "selection actual grade admission");
    rejects([&] { result.occurrence_field(3); }, "application map has no invented occurrences");
    auto limits = numeric::ExactActionLimits{}; limits.entries = 1;
    rejects([&] { Program invalid(publication, original, limits); }, "preparation stored entry admission");
    rejects([&] { program.execute({{1, 1, x}}, limits); }, "current runtime owner entry admission");
    limits = {}; limits.integer_bits = 2;
    rejects([&] { program.execute({{1, 1, x}}, limits); }, "exact growth admission");
    rejects([&] { program.execute({{1, 1, x}}, {}, [] { throw std::runtime_error("cancel"); }); }, "owned cancellation");
    const auto image = inference::encode_relational_field_result(result);
    auto damaged = image; damaged.back() ^= std::byte{1};
    rejects([&] { inference::decode_relational_field_result(damaged, {}); }, "result integrity check");
    auto policy = storage::TemporalReadLimits{}; policy.max_records = 2;
    rejects([&] { inference::decode_relational_field_result(image, policy); }, "cold reader aggregate record admission");
    using namespace inference::field_program_codec_detail;
    auto writer = envelope(result_magic); blob(writer, inference::encode_relational_field_program(program)); writer.u64(1);
    writer.u64(1); writer.u64(1); storage::temporal_codec_detail::tensor(writer, x); writer.u64(4);
    for (const auto& node : original.nodes) {
        writer.u64(node.key); const auto wrong = node.key == 2 ? tensor(V(7)) : result.field(node.key);
        storage::temporal_codec_detail::tensor(writer, wrong);
        if (node.kind == inference::FieldProgramNodeKind::Boundary) storage::temporal_codec_detail::tensor(writer, result.occurrence_field(node.key));
    }
    const auto wrong = finish(std::move(writer));
    rejects([&] { inference::decode_relational_field_result(wrong, {}); }, "valid envelope cannot substitute false intermediate field");
    check(inference::encode_relational_field_result(result) == image, "failed work preserves prior complete result");
    const auto other_use = program.execute({{1, 99, x}});
    check(inference::encode_relational_field_result(other_use) != image && other_use.field(2).values().dense() == result.field(2).values().dense(), "equal encodings preserve distinct original uses");
}
void ownership() {
    auto d = declaration(); auto& exposed = d.stalks[0].directions;
    bool mutated{}; const auto publication = source();
    Program held(publication, std::move(d), {}, [&] { if (!mutated) { mutated = true; exposed[0] = 999; } });
    check(held.declaration().stalks[0].directions[0] == 101, "moved caller buffers frozen before callback");
    const auto input = tensor({1, 2, 3, 4}); const auto expected = held.execute({{1, 5, input}});
    std::vector<std::future<std::vector<std::byte>>> jobs;
    for (unsigned i = 0; i < 8; ++i) jobs.push_back(std::async(std::launch::async, [held, input] {
        return inference::encode_relational_field_result(held.execute({{1, 5, input}}));
    }));
    const auto bytes = inference::encode_relational_field_result(expected);
    for (auto& job : jobs) check(job.get() == bytes, "independent concurrent sparse actions");
    auto replacement = declaration(); replacement.key = 999; replacement.occurrences[0].map = M::identity(2);
    const Program other(publication, replacement); bool switched{};
    const auto result = held.execute({{1, 5, input}}, {}, [&] { if (!switched) { switched = true; held = other; } });
    check(inference::encode_relational_field_result(result) == bytes, "runtime owner pinned before callback replacement");
    auto current = result; switched = false;
    const auto alternate = other.execute({{1, 5, input}});
    const auto selected = current.select(2, {{core::Grade{0}, 0}}, 2, {}, [&] { if (!switched) { switched = true; current = alternate; } });
    check(selected[0].field.values().dense() == expected.select(2, {{core::Grade{0}, 0}}, 2)[0].field.values().dense(), "result selection owner pinned before callback");
}
Q quadrance(const Declaration& d, core::Grade grade, const V& values) {
    Q result; std::size_t offset{};
    for (const auto& stalk : d.stalks) if (stalk.cell.grade == grade) {
        V local(stalk.directions.size());
        for (std::size_t j = 0; j < local.size(); ++j) local[j] = values[offset + j];
        const auto covector = stalk.metric.apply(local);
        for (std::size_t j = 0; j < local.size(); ++j) result += local[j] * covector[j];
        offset += local.size();
    }
    check(offset == values.size(), "quadrance complete declared carrier"); return result;
}
void basis_covariance() {
    const auto publication = source(); const auto original = declaration();
    const Program first(publication, original); auto changed = original;
    changed.key = 52; ++changed.revision;
    const auto frame = [](std::size_t size) { auto s = M::identity(size); s.set(0, 1, 1); return s; };
    const M s2 = frame(2), s3 = frame(3);
    M lower(7, 7), upper(4, 4);
    std::size_t lo{}, up{};
    for (auto& stalk : changed.stalks) {
        const auto s = frame(stalk.directions.size()), inverse = s.inverse();
        stalk.metric = inverse.transpose().multiply(stalk.metric).multiply(inverse); stalk.basis += 10000;
        auto& global = stalk.cell.grade.value ? upper : lower;
        auto& offset = stalk.cell.grade.value ? up : lo;
        for (const auto& [ij, q] : s.entries()) global.set(offset + ij.first, offset + ij.second, q);
        offset += s.rows();
    }
    const auto& arena = publication.endpoint().source.grade(core::Grade{1});
    const std::vector<std::size_t> dimensions{2, 3, 2};
    for (std::size_t p = 0; p < changed.occurrences.size(); ++p) {
        const auto s = dimensions[arena.lower_index[p]] == 3 ? s3 : s2;
        changed.occurrences[p].map = s.multiply(changed.occurrences[p].map).multiply(s2.inverse());
    }
    changed.nodes[2].map = upper.multiply(changed.nodes[2].map).multiply(lower.inverse());
    const Program second(publication, changed); const V input{Q{2, 3}, -1, 2, Q{-3, 5}};
    const auto a = first.execute({{1, 11, tensor(input)}});
    const auto b = second.execute({{1, 11, tensor(upper.apply(input))}});
    for (const auto key : {1u, 2u, 3u, 4u}) {
        const auto grade = key == 1 || key == 3 ? core::Grade{1} : core::Grade{0};
        const auto& s = grade.value ? upper : lower;
        check(b.field(key).values().dense() == s.apply(a.field(key).values().dense()), "whole-field basis covariance");
        check(quadrance(original, grade, a.field(key).values().dense()) ==
              quadrance(changed, grade, b.field(key).values().dense()), "declared stalk metric basis covariance");
    }
    const auto cold = inference::decode_relational_field_result(inference::encode_relational_field_result(b), {});
    const auto cold_program = cold.program();
    check(cold_program.declaration().stalks[0].basis == 11001 && cold.field(4).values().dense() == b.field(4).values().dense(), "changed basis and metric cold ownership");
}
void chain_family() {
    core::RelationSignature signature;
    const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", core::BoundaryPolicy::DeclaredExact, 3, 3);
    const auto top = signature.add_type("top", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{10, true, true, Q{1}}, {20, true, false, Q{1}}});
    builder.add_grade_one(edge, {{20, true, true, Q{1}}, {30, true, false, Q{1}}});
    builder.add_grade_one(edge, {{10, true, true, Q{1}}, {30, true, false, Q{1}}});
    builder.add_higher(core::Grade{2}, face, {{0, true, false, Q{1}}, {1, true, false, Q{1}}, {2, true, true, Q{1}}});
    builder.add_higher(core::Grade{3}, top, {{0, true, false, Q{1}}, {0, true, true, Q{1}}});
    const auto publication = storage::NativeTemporalSeries(storage::TemporalEndpoint(builder.freeze()), 51).tail();
    Declaration d; d.key = 52; d.differential = true;
    const auto& c = publication.endpoint().source;
    for (std::size_t k = 0; k < c.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        for (std::size_t i = 0; i < c.grade(grade).cell_count(); ++i)
            d.stalks.push_back({{grade, static_cast<std::uint32_t>(i)}, 1, {1, 2}, M::identity(2)});
        if (k) for (std::size_t p = 0; p < c.grade(grade).incidence_count(); ++p)
            d.occurrences.push_back({grade, p, storage::TemporalTensorRole::CoefficientIndependentMap, M::identity(2)});
    }
    d.nodes = {{1, inference::FieldProgramNodeKind::Input, core::Grade{3}, {}, {}},
               {2, inference::FieldProgramNodeKind::Boundary, core::Grade{2}, 1, {}}};
    const Program valid(publication, d);
    const auto valid_result = valid.execute({{1, 1, tensor({1, 2})}});
    check(valid_result.field(2).values().nonzeros() == 0, "complete strict realized chain family");
    auto broken = d; broken.occurrences[0].map = M::identity(2).scale(Q{2});
    rejects([&] { Program invalid(publication, broken); }, "first adjacent operator law cannot use scalar closure");
    broken = d; broken.occurrences.back().map = M::identity(2).scale(Q{2});
    rejects([&] { Program invalid(publication, broken); }, "last adjacent law must also be checked");
    broken.differential = false; const Program application(publication, broken);
    const auto application_result = application.execute({{1, 1, tensor({1, 2})}});
    check(application_result.field(2).values().nonzeros() != 0, "declared application action does not claim a differential");
}
}
int main(int argc, char** argv) {
    try {
        const bool emit = argc == 2 && std::string_view(argv[1]) == "--emit";
        corpus(emit); admissions(); ownership(); basis_covariance(); chain_family();
        if (!emit) std::cout << "relational_field_program_checks=" << checks << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL after " << checks << ": " << error.what() << '\n'; return 1; }
}
