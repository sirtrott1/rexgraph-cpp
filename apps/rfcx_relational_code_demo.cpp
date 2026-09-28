#include "rfc/storage/relational_codes.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include <iostream>

using namespace rfc;
using namespace rfc::storage;

static core::RelationalComplex make_source(bool flipped) {
    core::RelationSignature signature;
    const auto type = signature.add_type("relation", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    builder.declare_witness_identity(10);
    builder.declare_witness_identity(11);
    builder.add_grade_one(core::RelationKey{100}, type,
                          {{10, true, !flipped, numeric::Rational{1}, {}, core::OccurrenceKey{1000}},
                           {11, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{1001}}});
    return builder.freeze();
}

int main(int argc, char **argv) {
    try {
        const std::filesystem::path output = argc > 1 ? argv[1] : "relational-code-demo";
        if (std::filesystem::exists(output))
            throw std::invalid_argument("output directory already exists; use a fresh directory");
        std::filesystem::create_directories(output);
        TemporalEndpoint base(make_source(false));
        RelationalCodeState codes;
        codes.namespace_key = 17;
        codes.types.push_back({1, RelationalCodeTypeKind::Finite, {0, 1}, {}});
        codes.types.push_back(
            relational_aspect_code_type(base.source, 2, 1,
                                        {{101, base.source.signature().existence_aspect()},
                                         {102, base.source.signature().orientation_aspect()}}));
        codes.fields = {1};
        for (std::uint64_t i = 0; i < 2; ++i)
            codes.sections.push_back(
                relational_aspect_code_section(i + 1, 1, 2, {core::Grade{1}, core::OccurrenceKey{1000 + i}}));
        RelationalCodeOperation positive;
        positive.key = 1;
        positive.input = 2;
        positive.output = 1;
        positive.opcode = RelationalCodeOpcode::Binary;
        positive.binary = composite::BinaryProgram(3);
        positive.binary->append({composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, 0});
        positive.binary->append({composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, 1});
        positive.binary->append({composite::BinaryOpcode::AndNot, 2, 0, 1});
        positive.registers = {2};
        codes.operations.push_back(positive);
        base.codes = codes;
        TemporalEndpoint next(make_source(true), 1);
        next.codes = codes;
        const auto segment = make_temporal_segment(base, next);
        write_rfcx(base.source, output / "base.rfcx");
        write_temporal_endpoint(base, output / "base.rfcxe");
        append_temporal_segment(output / "history.rfcxt", base, segment);
        MappedRfcx mapped(output / "base.rfcx");
        auto current = read_temporal_endpoint(mapped, output / "base.rfcxe");
        const auto before = execute_relational_aspect_code(current, 1, core::Grade{1}).front().count();
        visit_temporal_segments(output / "history.rfcxt", [&](const auto &delta, auto) {
            current = replay_temporal_segment(current, delta);
        });
        const auto after = execute_relational_aspect_code(current, 1, core::Grade{1}).front().count();
        const bool exact = encode_temporal_endpoint(current) == encode_temporal_endpoint(next);
        std::cout << "positive_occurrences=" << before << "->" << after
                  << " source_bound_sections=" << current.codes->sections.size()
                  << " copied_code_update=" << std::boolalpha << segment.codes.has_value()
                  << " exact_disk_replay=" << exact << '\n';
        return exact && before == 1 && after == 2 && !segment.codes ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
