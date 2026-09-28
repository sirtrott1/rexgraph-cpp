#include "rfc/storage/native_defects.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include <filesystem>
#include <iostream>

using namespace rfc;
using namespace rfc::storage;
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("usage: rfc_native_defect_demo NEW_DIRECTORY");
        const std::filesystem::path out(argv[1]);
        if (!std::filesystem::create_directory(out)) throw std::runtime_error("use a fresh output directory");
        core::RelationSignature sig;
        const auto edge = sig.add_type("directed-edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto face = sig.add_type("closed-triangle", core::BoundaryPolicy::DeclaredExact, 3, 3);
        core::RelationalComplexBuilder b(sig);
        for (std::uint64_t i = 0; i < 3; ++i)
            b.add_grade_one(core::RelationKey{100 + i}, edge, {
                {10 + i, true, true, numeric::Rational{1}, {}, core::OccurrenceKey{1000 + 2 * i}},
                {10 + (i + 1) % 3, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{1001 + 2 * i}}});
        b.add_higher(core::Grade{2}, core::RelationKey{200}, face, {
            {0, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{2000}},
            {1, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{2001}},
            {2, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{2002}}});
        TemporalEndpoint base(b.freeze()); NativeDefectRealization realization; realization.kind = NativeDefectRealizationKind::LinearStalks;
        for (std::size_t k = 0; k < base.source.grade_count(); ++k) {
            const core::Grade grade{static_cast<std::uint16_t>(k)};
            for (std::size_t i = 0; i < base.source.grade(grade).cell_count(); ++i) {
                const auto key = stable_cell_key(base.source, {grade, static_cast<std::uint32_t>(i)});
                const auto carrier = 10000 * k + key.key;
                base.carriers.push_back({carrier, TemporalCarrierKind::CellStalk, grade, key, carrier + 1, numeric::IndexDomain::linear(1)});
                realization.stalk_carriers.push_back(carrier);
            }
        }
        auto target = base; target.sequence = 1;
        TemporalTensor map; map.key = 1; map.role = TemporalTensorRole::RealizedIncidenceMap;
        map.input = 10100; map.output = 10; map.owner.kind = TemporalOwnerKind::Occurrence;
        map.owner.occurrence = {core::Grade{1}, core::OccurrenceKey{1000}};
        map.tensor = temporal_matrix_tensor(numeric::ExactMatrix::from_rows({{numeric::Rational{-2}}})); target.tensors.push_back(map);
        write_rfcx(base.source, out / "base.rfcx"); write_temporal_endpoint(base, out / "base.rfcxe");
        append_temporal_segment(out / "history.rfcxt", base, make_temporal_segment(base, target));
        NativeDefectQuery all; all.key = 1; all.upper_grade = core::Grade{2};
        auto partial = all; partial.key = 2; partial.selection = NativeDefectSelection::Occurrences;
        partial.occurrences = {{core::Grade{2}, core::OccurrenceKey{2000}}, {core::Grade{1}, core::OccurrenceKey{1000}}};
        MappedRfcx mapped(out / "base.rfcx"); auto seed = read_temporal_endpoint(mapped, out / "base.rfcxe");
        NativeDefectStore index(seed, 71, realization, {all, partial}); const auto old = index.pin();
        const auto segments = read_temporal_segments(out / "history.rfcxt");
        if (segments.size() != 1) throw std::runtime_error("demo history size mismatch");
        index.advance(segments.front()); const auto next = index.pin();
        const bool exact = encode_temporal_endpoint(*next.endpoint()) == encode_temporal_endpoint(target);
        if (!old.flat(core::Grade{2}) || !next.certificate(1).curved_in_scope() || next.certificate(2).curved_in_scope() || !exact)
            throw std::runtime_error("exact route defect demonstration failed");
        std::cout << "retained_routes=" << next.route_count() << " recomputed_products=" << next.stats().route_products
                  << " reused_routes=" << next.stats().reused_routes
                  << " old_flat=true target_curved=true partial_is_not_curvature=true exact_disk_replay=true\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
