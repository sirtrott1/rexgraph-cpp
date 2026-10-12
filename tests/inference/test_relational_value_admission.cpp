#include "rfc/inference/relational_value_admission_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
#include <string_view>
using namespace rfc;
using namespace rfc::inference;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using Bytes = std::vector<std::byte>;
namespace {
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool rejected{}; try { action(); } catch (const std::exception&) { rejected = true; } check(rejected, message);
}
template<class F> void rejects_before_action(F action, const char* message) {
    bool rejected{}; try { action(); } catch (const std::invalid_argument&) { rejected = true; } check(rejected, message);
}
numeric::SparseRationalTensor tensor(const V& values) {
    numeric::ExactActionGuard guard; return field_interface_detail::tensor(values, guard);
}
storage::NativeTemporalSeries history(std::uint64_t ns) {
    core::RelationSignature signature;
    const auto type = signature.add_type("relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, type,
        {{10, true, false, Q{1}}, {20, true, true, Q{1}}, {10, true, true, Q{1}},
         {20, false, true, Q{1}}, {10, true, false, Q{0}}});
    storage::TemporalEndpoint endpoint(builder.freeze(), 3);
    storage::NativeTemporalSeries series(endpoint, ns);
    for (std::uint64_t i = 4; i <= 13; ++i) { endpoint.sequence = i; series.append_target(endpoint); }
    return series;
}
OwnedRelationalFieldProgram program(const storage::NativeTemporalSeries& series) {
    RelationalFieldProgramDeclaration d; d.key = 51; d.revision = 1; d.differential = true;
    d.stalks = {{{{0}, 0}, 101, {11, 12}, M::from_rows({{2, 1}, {1, 3}})},
                {{{0}, 1}, 102, {21, 22, 23}, M::identity(3)},
                {{{1}, 0}, 103, {31, 32}, M::from_rows({{3, 1}, {1, 2}})}};
    const auto phi = storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences = {{{1}, 0, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {{1}, 1, phi, M::from_rows({{1, 0}, {0, 1}, {1, 1}})},
                     {{1}, 2, phi, M::from_rows({{2, 1}, {1, 0}})},
                     {{1}, 3, phi, M::from_rows({{7, 3}, {2, 5}, {1, 1}})},
                     {{1}, 4, phi, M::identity(2)}};
    d.nodes = {{1, FieldProgramNodeKind::Input, {1}, {}, {}},
               {2, FieldProgramNodeKind::Boundary, {0}, 1, {}},
               {3, FieldProgramNodeKind::DeclaredMap, {1}, 2, M::from_rows({{1, 0, 0, 0, 1}, {0, 1, 1, 1, 0}})}};
    return {series.at(7), d};
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series) {
    const auto selected = storage::NativeSeriesPrefix::capture(series, 4);
    RelationalInputUseDeclaration d; d.key = 601; d.channel = 602; d.encoding = "UTF-8";
    for (const auto c : std::string("same bytes")) d.bytes.push_back(static_cast<std::byte>(c));
    const auto tail = selected.tail(); const auto source = tail.endpoint().source;
    RelationalSpanUse use; use.key = 111; use.cells = {storage::stable_cell_key(source, {{1}, 0})}; use.spans = {{0, 10}, {0, 0}, {0, 4}};
    for (std::size_t i = 0; i < 5; ++i) use.occurrences.push_back({{1}, source.occurrence_identity({1}, i)});
    d.uses.push_back(use); return {selected.history(), d};
}
RelationalAccessionInput input(const OwnedRelationalFieldProgram& p, const OwnedRelationalInputUseAsset& bytes, const V& values) {
    RelationalInputAccessionDeclaration d; d.key = 701; d.node = 1; d.source_grade = {1}; d.isometric = true;
    d.source_stalks = {p.declaration().stalks.back()}; d.instances.cells = {{0, 1}, {0}};
    d.instances.occurrences = {{}, {0, 1, 2, 3, 4}}; d.accession = M::identity(2); d.reconstruction = M::identity(2);
    return OwnedRelationalInputAccession(bytes, p, d).admit(111, tensor(values));
}
std::vector<RelationalOriginGrant> grants(const OwnedRelationalFieldProgram& p, const RelationalAccessionInput& x,
    const storage::NativeTemporalSeries& series) {
    const auto early = storage::NativeSeriesPrefix::capture(series, 8);
    const auto late = storage::NativeSeriesPrefix::capture(series, 12);
    const auto pp = relational_program_dependency_payload(p); const auto vp = relational_input_value_payload(x);
    const auto ap = relational_input_accession_payload(x); const auto bp = encode_relational_input_use_asset(x.accession().asset());
    const runtime::WorkAvailability admitted{series.source_namespace(), 12};
    using Role = RelationalDependencyRole;
    return {{OwnedRelationalValueOrigin(1, early, Role::Program, pp), admitted},
            {OwnedRelationalValueOrigin(2, early, Role::Maps, pp), admitted},
            {OwnedRelationalValueOrigin(3, early, Role::Metrics, pp), admitted},
            {OwnedRelationalValueOrigin(4, early, Role::InputAsset, bp), admitted},
            {OwnedRelationalValueOrigin(5, late, Role::InputValue, vp), admitted},
            {OwnedRelationalValueOrigin(6, early, Role::InputAccession, ap), admitted},
            {OwnedRelationalValueOrigin(7, early, Role::Reconstruction, ap), admitted},
            {OwnedRelationalValueOrigin(8, late, Role::TargetValue, vp), admitted}};
}
void json(const numeric::SparseRationalTensor& field) {
    std::cout << '[';
    for (std::size_t i = 0; i < field.domain().logical_size(); ++i) { if (i) std::cout << ','; std::cout << '"' << field.state(i).value() << '"'; }
    std::cout << ']';
}
constexpr RelationalProgramOrigins model_origins{1, 2, 3};
constexpr RelationalInputOrigins use_origins{4, 5, 6, 7};
void run(bool emit) {
    const auto source = history(991); const auto publications = history(992);
    const auto p = program(source); const auto bytes = asset(publications);
    const auto selected = storage::NativeSeriesPrefix::capture(publications, 8);
    check(selected.first_sequence() == 3 && selected.cutoff() == 8 && selected.segment_count() == 5, "authentic full initial history and selected prefix");
    rejects([&] { (void)selected.at(9); }, "prefix excludes later publications");
    for (std::size_t trial = 0; trial < 24; ++trial) {
        const V values{Q{static_cast<long>(trial) - 9, 3}, Q{static_cast<long>(trial % 7) - 4, 5}};
        const auto x = input(p, bytes, values); auto original = grants(p, x, publications);
        const RelationalAvailabilityContext context(original);
        const PreparedAvailableRelationalProgram prepared(p, model_origins, context);
        prepared.admit(context);
        const auto actual_accession = x.accession();
        const RelationalInputSubmission submission{bytes, actual_accession.declaration(), x.use(), x.source_field()};
        const auto admitted = prepared.admit_input(submission, use_origins, context);
        const auto result = prepared.execute({admitted}, context);
        result.admit(context); const auto native = result.result(); const auto fields = native.result();
        check(encode_accession_field_result(native) == encode_accession_field_result(execute_relational_inputs(p, {x})), "whole action unchanged by admission");
        check(result.obligations().size() == 7 && fields.occurrence_field(2).support().count() == 12, "full original ordered flag support");
        check(bytes.history().last_sequence() == 4 && result.obligations()[4].publication().cutoff() == 12, "numeric value origin separate from byte asset");
        const auto encoded = encode_available_relational_result(result);
        storage::TemporalReadLimits read;
        const auto saved_program = encode_available_relational_program(prepared);
        const auto cold_program = decode_available_relational_program(saved_program, context, read);
        check(encode_available_relational_program(cold_program) == saved_program, "program cold admission before preparation");
        const auto restored = decode_available_relational_result(encoded, context, read);
        check(encode_available_relational_result(restored) == encoded, "canonical cold result replay against actual authority");
        if (emit) {
            std::cout << "{\"input\":"; json(fields.field(1)); std::cout << ",\"boundary\":"; json(fields.field(2));
            std::cout << ",\"occurrences\":"; json(fields.occurrence_field(2)); std::cout << ",\"later\":"; json(fields.field(3));
            std::cout << ",\"first\":3,\"asset_cut\":4,\"value_cut\":12}\n";
        }
        auto late = original; late[4].availability.max_sequence = 11;
        const RelationalAvailabilityContext unavailable(late);
        auto no_math = numeric::ExactActionLimits{}; no_math.dimension = 0;
        rejects_before_action([&] { (void)prepared.admit_input(submission, use_origins, unavailable, no_math); }, "online value authority checked before any A/R construction");
        rejects([&] { (void)prepared.execute({{x, use_origins}}, unavailable); }, "warm execution rejects late numeric value on early bytes");
        rejects([&] { result.admit(unavailable); }, "warm result rechecks current availability");
        rejects([&] { (void)decode_available_relational_result(encoded, unavailable, read); }, "cold replay rejects late value");
        auto origins = use_origins; origins.value = 8;
        rejects([&] { (void)prepared.execute({{x, origins}}, context); }, "target cannot be renamed to numeric input");
        rejects_before_action([&] { (void)prepared.admit_input(submission, origins, context, no_math); }, "online target cannot be transformed before role check");
        auto absent = original; absent.erase(absent.begin() + 4);
        rejects([&] { (void)decode_available_relational_result(encoded, RelationalAvailabilityContext(absent), read); }, "cold replay missing actual authority");
        auto foreign = original; foreign[4].availability.source_namespace = 993;
        rejects([&] { (void)prepared.execute({{x, use_origins}}, RelationalAvailabilityContext(foreign)); }, "namespace-local availability");
        auto duplicate = original; duplicate.push_back(original[0]);
        rejects([&] { (void)RelationalAvailabilityContext(duplicate); }, "duplicate authority key");
        auto wrong = original; wrong[4].origin = OwnedRelationalValueOrigin(5, selected, RelationalDependencyRole::InputValue, Bytes{std::byte{1}});
        rejects([&] { (void)prepared.execute({{x, use_origins}}, RelationalAvailabilityContext(wrong)); }, "coefficient content cannot substitute actual full origin");
        auto relabeled = original; relabeled[4].origin = OwnedRelationalValueOrigin(5, selected, RelationalDependencyRole::TargetValue, original[4].origin.payload());
        rejects([&] { (void)decode_available_relational_result(encoded, RelationalAvailabilityContext(relabeled), read); }, "cold target to input relabeling rejects");
        auto earlier = original; earlier[4].origin = OwnedRelationalValueOrigin(5, selected, RelationalDependencyRole::InputValue, original[4].origin.payload());
        rejects([&] { (void)decode_available_relational_result(encoded, RelationalAvailabilityContext(earlier), read); }, "archive cannot launder late value to earlier publication");
        auto copied = original; copied[0].origin = OwnedRelationalValueOrigin(1, original[0].origin.publication(), original[0].origin.role(), original[0].origin.payload());
        rejects([&] { (void)prepared.execute({{x, use_origins}}, RelationalAvailabilityContext(copied)); }, "equal live content does not establish origin ownership");
        const auto adopted = decode_available_relational_result(encoded, RelationalAvailabilityContext(copied), read);
        check(encode_available_relational_result(adopted) == encoded, "explicit cold authority adoption after full receipt checks");
        auto independent = original;
        independent.push_back({OwnedRelationalValueOrigin(9, selected, RelationalDependencyRole::InputValue, original[4].origin.payload()), {992, 8}});
        origins = use_origins; origins.value = 9;
        const RelationalAvailabilityContext other_input(independent);
        const auto equal_input = prepared.admit_input(submission, origins, other_input);
        const auto separate = prepared.execute({equal_input}, other_input);
        check(encode_accession_field_result(separate.result()) == encode_accession_field_result(native) &&
            separate.obligations()[4].key() != result.obligations()[4].key(), "independently available equal input keeps distinct origin and cut");
        for (std::size_t dependency = 0; dependency < 7; ++dependency) {
            auto hidden = original; hidden[dependency].availability.max_sequence = hidden[dependency].origin.publication().cutoff() - 1;
            const RelationalAvailabilityContext rejected(hidden);
            if (dependency < 3) {
                rejects([&] { (void)PreparedAvailableRelationalProgram(p, model_origins, rejected); }, "prepare validates program maps and metrics origins");
                rejects([&] { prepared.admit(rejected); }, "cache-hit program admission checks current origin availability");
                rejects_before_action([&] { (void)decode_available_relational_program(saved_program, rejected, read, no_math); }, "program restoration admits roles before preparation");
            }
            rejects([&] { (void)prepared.execute({{x, use_origins}}, rejected); }, "every actual source/map/metric/reconstruction dependency checked");
        }
        if (trial == 0) {
            // Graft legitimate origin receipts onto different, mathematically
            // valid fields. A zero dimension budget detects any premature solve.
            using namespace field_program_codec_detail;
            auto graft = envelope(value_admission_codec_detail::result_magic);
            blob(graft, encode_accession_field_result(execute_relational_inputs(p, {input(p, bytes, {99, 98})})));
            graft.u64(1); graft.u64(2); graft.u64(3); graft.u64(1);
            graft.u64(4); graft.u64(5); graft.u64(6); graft.u64(7); graft.u64(result.obligations().size());
            for (const auto& origin : result.obligations()) blob(graft, origin.receipt());
            const auto false_fields = finish(std::move(graft));
            rejects_before_action([&] { (void)decode_available_relational_result(false_fields, context, read, no_math); }, "passive actual field content checked before native replay");
            auto unrelated = p.declaration(); unrelated.key = 52;
            const OwnedRelationalFieldProgram different(p.publication(), unrelated);
            auto foreign_result = envelope(value_admission_codec_detail::result_magic);
            blob(foreign_result, encode_accession_field_result(execute_relational_inputs(different, {input(different, bytes, values)})));
            foreign_result.u64(1); foreign_result.u64(2); foreign_result.u64(3); foreign_result.u64(1);
            foreign_result.u64(4); foreign_result.u64(5); foreign_result.u64(6); foreign_result.u64(7); foreign_result.u64(result.obligations().size());
            for (const auto& origin : result.obligations()) blob(foreign_result, origin.receipt());
            const auto false_program = finish(std::move(foreign_result));
            rejects_before_action([&] { (void)decode_available_relational_result(false_program, context, read, no_math); }, "passive actual program checked before native preparation");
            auto program_graft = envelope(value_admission_codec_detail::program_magic);
            blob(program_graft, encode_relational_field_program(different)); program_graft.u64(1); program_graft.u64(2); program_graft.u64(3);
            for (const auto& origin : prepared.obligations()) blob(program_graft, origin.receipt());
            const auto false_model = finish(std::move(program_graft));
            rejects_before_action([&] { (void)decode_available_relational_program(false_model, context, read, no_math); }, "valid-envelope model graft rejected before preparation");
            const OwnedRelationalFieldProgram equal_program(p.publication(), p.declaration());
            rejects([&] { (void)prepared.execute({{input(equal_program, bytes, values), use_origins}}, context); }, "equal program bytes do not imply actual carrier owner");
            auto corrupt = encoded; corrupt.back() ^= std::byte{1};
            rejects([&] { (void)decode_available_relational_result(corrupt, context, read); }, "corrupt canonical archive");
            auto tiny = read; tiny.max_file_bytes = 10;
            rejects([&] { (void)decode_available_relational_result(encoded, context, tiny); }, "reader byte budget");
            auto small = numeric::ExactActionLimits{}; small.steps = 0;
            rejects([&] { (void)prepared.execute({{x, use_origins}}, context, small); }, "action resource budget");
            auto resource = RelationalAdmissionLimits{}; resource.origins = 1;
            rejects([&] { (void)RelationalAvailabilityContext(original, resource); }, "origin storage budget");
            resource.payload_bytes = 1;
            rejects([&] { (void)OwnedRelationalValueOrigin(99, selected, RelationalDependencyRole::InputValue, original[4].origin.payload(), resource); }, "payload byte budget");
            std::size_t callbacks{};
            rejects([&] { (void)prepared.execute({{x, use_origins}}, context, {}, [&] { if (++callbacks == 4) throw std::runtime_error("stop"); }); }, "cancelled work publishes no result");
            check(encode_available_relational_result(result) == encoded, "cancel leaves earlier immutable output intact");
            callbacks = 0;
            rejects([&] { (void)decode_available_relational_result(encoded, context, read, {}, [&] { if (++callbacks == 3) throw std::runtime_error("stop"); }); }, "cancel at replay authority boundary");
            auto mutable_bytes = encoded; auto mutable_context = context; bool changed{};
            const auto pinned = decode_available_relational_result(mutable_bytes, mutable_context, read, {}, [&] {
                if (!changed) { changed = true; mutable_bytes.clear(); mutable_context = unavailable; }
            });
            check(encode_available_relational_result(pinned) == encoded, "pin full archive bytes and authority before callback replacement");
            auto mutable_prepared = prepared; mutable_context = context; changed = false;
            const auto pinned_action = mutable_prepared.execute({{x, use_origins}}, mutable_context, {}, [&] {
                if (!changed) { changed = true; mutable_context = unavailable; mutable_prepared = PreparedAvailableRelationalProgram(p, model_origins, context); }
            });
            check(encode_available_relational_result(pinned_action) == encoded, "pin actual prepared owner and authority before callback");
            std::vector<std::future<Bytes>> readers;
            for (std::size_t i = 0; i < 6; ++i) readers.push_back(std::async(std::launch::async, [prepared, x, context] {
                return encode_available_relational_result(prepared.execute({{x, use_origins}}, context));
            }));
            for (auto& reader : readers) check(reader.get() == encoded, "independent concurrent native workspaces");
            const auto escaped = [&] { const PreparedAvailableRelationalProgram local(p, model_origins, context); return local.execute({{x, use_origins}}, context); }();
            check(encode_available_relational_result(escaped) == encoded, "result survives prepared owner lifetime");
        }
    }
}
} // namespace
int main(int argc, char** argv) {
    try { const bool emit = argc > 1 && std::string_view(argv[1]) == "--emit"; run(emit); if (!emit) std::cout << "relational_value_admission_checks=" << checks << '\n'; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
