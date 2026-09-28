#include "rfc/storage/relational_codes.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include <iostream>

using namespace rfc;
using namespace rfc::storage;

int main(int argc, char **argv) {
    try {
        const std::filesystem::path output = argc > 1 ? argv[1] : "relational-code-delta-demo";
        if (std::filesystem::exists(output))
            throw std::invalid_argument("output directory already exists; use a fresh directory");
        std::filesystem::create_directories(output);
        core::RelationSignature signature;
        const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder builder(signature);
        builder.declare_witness_identity(10);
        builder.declare_witness_identity(11);
        builder.add_grade_one(core::RelationKey{100}, edge,
            {{10, true, true, numeric::Rational{1}, {}, core::OccurrenceKey{1000}},
             {11, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{1001}}});
        TemporalEndpoint base(builder.freeze());
        RelationalCodeState codes;
        codes.namespace_key = 700;
        RelationalCodeType type{1, RelationalCodeTypeKind::Rational, {}, {}};
        std::vector<numeric::SparseRationalEntry> values;
        for (std::size_t i = 0; i < 32; ++i) {
            type.atoms.push_back(1000 + i);
            values.push_back({i, numeric::Rational(static_cast<unsigned long>(i + 1))});
        }
        codes.types.push_back(type);
        codes.fields = {1};
        RelationalCodeOperation identity;
        identity.key = 1; identity.input = 1; identity.output = 1;
        identity.opcode = RelationalCodeOpcode::Identity;
        codes.operations.push_back(identity);
        const auto value = numeric::SparseRationalTensor(numeric::IndexDomain({32}),
            numeric::StructuralSupport::full(32), numeric::SparseRationalVector(32, values));
        for (std::uint64_t i = 1; i <= 1024; ++i) {
            RelationalCodeSection section;
            section.key = i; section.field = 1; section.type = 1;
            section.cell = StableCellKey{core::Grade{0}, 10};
            section.value = RelationalCodeValue{value};
            codes.sections.push_back(std::move(section));
        }
        codes.interfaces = {{1, 1, 2, 1, 1}};
        base.codes = std::move(codes);
        auto next = base;
        next.sequence = 1;
        values[0].value = 17;
        const auto changed = numeric::SparseRationalTensor(numeric::IndexDomain({32}),
            numeric::StructuralSupport::full(32), numeric::SparseRationalVector(32, values));
        next.codes->sections[0].value = RelationalCodeValue{changed};
        next.codes->sections[1].value = RelationalCodeValue{changed};
        const auto segment = make_temporal_segment(base, next);
        const auto full = make_temporal_segment(base, next, {}, {}, RelationalCodeEncoding::Full);
        // Deliberately collide every bucket; full typed observations still decide matches.
        RelationalCodeObservationIndex index([](auto) { return 0; });
        const auto old_observation = relational_code_section_bytes(*base.codes, 1);
        index.insert({700, 1}, old_observation); index.insert({700, 2}, old_observation);
        write_rfcx(base.source, output / "base.rfcx");
        write_temporal_endpoint(base, output / "base.rfcxe");
        const auto written = append_temporal_segment(output / "history.rfcxt", base, segment);
        MappedRfcx mapped(output / "base.rfcx");
        auto current = read_temporal_endpoint(mapped, output / "base.rfcxe");
        visit_temporal_segments(output / "history.rfcxt", [&](const auto &delta, auto) {
            current = replay_temporal_segment(current, delta);
        });
        const auto observation = relational_code_section_bytes(*current.codes, 1);
        index.replace({700, 1}, observation); index.replace({700, 2}, observation);
        const bool exact = encode_temporal_endpoint(current) == encode_temporal_endpoint(next);
        const bool glue = relational_code_glue(*current.codes).front().compatible;
        const bool indexed = index.find(old_observation).empty() && index.find(observation).size() == 2;
        const auto edits = segment.codes && segment.codes->patch ? segment.codes->patch->section_edits.size() : 0;
        std::cout << "sections=1024 edited_sections=" << edits
                  << " sparse_bytes=" << written << " full_bytes=" << encode_temporal_segment(full).size()
                  << " glue=" << std::boolalpha << glue << " collision_safe_index=" << indexed
                  << " exact_disk_replay=" << exact << '\n';
        return exact && glue && indexed && edits == 2 ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
