#include "rfc/storage/relational_code_observations.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include <iostream>

using namespace rfc;
using namespace rfc::storage;
namespace {
core::RelationalComplex source(bool flip = false) {
    core::RelationSignature signature;
    const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    builder.declare_witness_identity(10);
    builder.declare_witness_identity(11);
    builder.add_grade_one(core::RelationKey{100}, edge,
        {{10, true, !flip, numeric::Rational{1}, {}, core::OccurrenceKey{1000}},
         {11, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{1001}}});
    return builder.freeze();
}
RelationalCodeValue value(long x) {
    return {numeric::SparseRationalTensor(numeric::IndexDomain({2}), numeric::StructuralSupport::full(2),
                                          numeric::SparseRationalVector(2, {{0, numeric::Rational{x}}}))};
}
} // namespace

int main(int argc, char **argv) {
    try {
        const std::filesystem::path output = argc > 1 ? argv[1] : "relational-code-observation-demo";
        if (std::filesystem::exists(output))
            throw std::invalid_argument("output directory already exists; use a fresh directory");
        std::filesystem::create_directories(output);
        TemporalEndpoint base(source());
        RelationalCodeState codes;
        codes.namespace_key = 700;
        codes.types = {{1, RelationalCodeTypeKind::Rational, {10, 11}, {}},
                       {2, RelationalCodeTypeKind::Finite, {0, 1}, {}}};
        codes.types.push_back(relational_aspect_code_type(base.source, 3, 2,
            {{100, base.source.signature().existence_aspect()}, {101, base.source.signature().orientation_aspect()}}));
        codes.fields = {1, 2};
        RelationalCodeOperation op;
        op.key = 1; op.input = op.output = 1; op.opcode = RelationalCodeOpcode::Linear;
        op.kernel = temporal_matrix_tensor(numeric::ExactMatrix::identity(2));
        codes.operations = {op};
        for (std::uint64_t i = 1; i <= 1024; ++i) {
            RelationalCodeSection section;
            section.key = i; section.field = 1; section.type = 1; section.value = value(1);
            codes.sections.push_back(std::move(section));
        }
        codes.sections.push_back(relational_aspect_code_section(2001, 2, 3, {core::Grade{1}, core::OccurrenceKey{1000}}));
        codes.sections.push_back(relational_aspect_code_section(2002, 2, 3, {core::Grade{1}, core::OccurrenceKey{1001}}));
        codes.interfaces = {{1, 1, 2, 1, 1}};
        base.codes = std::move(codes);
        using Kind = RelationalCodeObservationKind;
        // Query declarations are application configuration. They do not stand
        // in for source, field, operation or metric reconstruction.
        const std::vector<RelationalCodeObservationQuery> queries{
            {1, Kind::Section, 0, 1, {}}, {2, Kind::Application, 1, 1, {}},
            {3, Kind::Section, 0, 2, {}}, {4, Kind::Interface, 0, {}, {}},
            {5, Kind::Operation, 0, {}, {}}};
        auto a = base; a.sequence = 1; a.codes->sections[512].value = value(17);
        auto b = a; b.sequence = 2; b.source = source(true);
        auto c = b; c.sequence = 3;
        auto kernel = numeric::ExactMatrix::identity(2); kernel.set(0, 1, numeric::Rational{7});
        c.codes->operations[0].kernel = temporal_matrix_tensor(kernel);
        const auto da = make_temporal_segment(base, a), db = make_temporal_segment(a, b), dc = make_temporal_segment(b, c);
        if (db.codes) throw std::runtime_error("source-only segment unexpectedly carries code changes");
        write_rfcx(base.source, output / "base.rfcx");
        write_temporal_endpoint(base, output / "base.rfcxe");
        append_temporal_segment(output / "history.rfcxt", base, da);
        append_temporal_segment(output / "history.rfcxt", a, db);
        append_temporal_segment(output / "history.rfcxt", b, dc);
        MappedRfcx mapped(output / "base.rfcx");
        // Deliberate collisions: exact bytes, not bucket equality, determine hits.
        RelationalCodeObservationStore store(read_temporal_endpoint(mapped, output / "base.rfcxe"), queries,
                                             [](auto) { return 0; });
        const auto old_operation = store.observation({5, 1}).bytes;
        std::vector<std::size_t> evaluated;
        visit_temporal_segments(output / "history.rfcxt", [&](const auto &segment, auto) {
            evaluated.push_back(store.advance(segment).evaluated);
        });
        RelationalCodeObservationStore rebuilt(c, queries);
        bool identical = store.identities() == rebuilt.identities();
        for (const auto id : store.identities())
            identical = identical && store.observation(id).bytes == rebuilt.observation(id).bytes;
        const bool exact = encode_temporal_endpoint(store.snapshot()) == encode_temporal_endpoint(c);
        const bool collision_safe = store.find(store.observation({1, 513}).bytes) ==
                                   std::vector<RelationalCodeIdentity>{{1, 513}};
        const bool operational = old_operation != store.observation({5, 1}).bytes;
        std::cout << "observations=" << store.size() << " sparse_evaluations=" << evaluated.at(0)
                  << " source_only_evaluations=" << evaluated.at(1) << " operator_evaluations=" << evaluated.at(2)
                  << " exact_rebuild=" << std::boolalpha << identical << " collision_safe=" << collision_safe
                  << " operation_identity_retained=" << operational << " exact_disk_replay=" << exact << '\n';
        return exact && identical && collision_safe && operational && evaluated == std::vector<std::size_t>{2, 1, 1026} ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
