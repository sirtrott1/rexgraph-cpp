#include "rfc/core/builder.hpp"
#include "rfc/local/carrier.hpp"
#include "rfc/storage/local_blocks.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;

        core::RelationSignature sig;
        const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto face = sig.add_type("face", core::BoundaryPolicy::DeclaredExact, 3, 3);
        core::RelationalComplexBuilder b(sig);
        b.add_grade_one(core::RelationKey{100}, edge, {
            {10,true,true,R{1},{},core::OccurrenceKey{1000}},
            {11,true,false,R{1},{},core::OccurrenceKey{1001}}});
        b.add_grade_one(core::RelationKey{101}, edge, {
            {11,true,true,R{1},{},core::OccurrenceKey{1010}},
            {12,true,false,R{1},{},core::OccurrenceKey{1011}}});
        b.add_grade_one(core::RelationKey{102}, edge, {
            {12,true,true,R{1},{},core::OccurrenceKey{1020}},
            {10,true,false,R{1},{},core::OccurrenceKey{1021}}});
        b.add_higher(core::Grade{2}, core::RelationKey{200}, face, {
            {0,true,false,R{1},{},core::OccurrenceKey{2000}},
            {1,true,false,R{1},{},core::OccurrenceKey{2001}},
            {2,true,false,R{1},{},core::OccurrenceKey{2002}}});
        const auto source = b.freeze();

        local::RelationalLocalCarrier carrier(source);
        const auto left_patch = carrier.cell_patch(core::CellId{core::Grade{1},0u});
        const auto right_patch = carrier.cell_patch(core::CellId{core::Grade{1},1u});

        numeric::IndexDomain payload_domain({2,2});
        auto payload_support = numeric::StructuralSupport::sparse(4,{0,1,3});
        numeric::SparseRationalVector payload_values(4, {{0,R{1,2}}, {3,R{3}}});
        storage::LocalExactTensorPayload payload;
        payload.payload_key = 900;
        payload.binding = storage::LocalPayloadBinding::Cell;
        payload.cell = storage::StableCellKey{core::Grade{0},11};
        payload.tensor = numeric::SparseRationalTensor(std::move(payload_domain), std::move(payload_support), std::move(payload_values));

        auto left = storage::make_strict_local_block(source, 1, left_patch, {payload});
        auto right = storage::make_strict_local_block(source, 2, right_patch);

        // Partial two-step chart: face -> e01 -> v10 only. It intentionally
        // omits the other face routes and is not a boundary-closed subcomplex.
        auto partial = storage::make_partial_local_chart(source, 3,
            {core::CellId{core::Grade{0},0u}, core::CellId{core::Grade{0},1u},
             core::CellId{core::Grade{1},0u}, core::CellId{core::Grade{2},0u}},
            {{core::Grade{1},core::OccurrenceKey{1000}},
             {core::Grade{2},core::OccurrenceKey{2000}}});

        storage::validate_local_block_against_source(source,left);
        storage::validate_local_block_against_source(source,right);
        storage::validate_local_block_against_source(source,partial);
        require(left.kind == storage::LocalBlockKind::StrictSubcomplex, "strict local block kind mismatch");
        require(partial.kind == storage::LocalBlockKind::PartialChart, "partial chart kind mismatch");

        storage::GlueIndex glue({left,right,partial});
        const storage::StableCellKey shared_v1{core::Grade{0},11};
        const auto* shared = glue.find(shared_v1);
        require(shared != nullptr, "shared stable identity missing from glue index");
        require(std::binary_search(shared->block_keys.begin(), shared->block_keys.end(), 1u) &&
                std::binary_search(shared->block_keys.begin(), shared->block_keys.end(), 2u),
                "glue memberships should identify strict blocks sharing v11");
        const auto between = glue.shared_between(1,2);
        require(between.size() == 1u && between.front() == shared_v1,
                "strict block interface should contain one retained identity");

        const std::array<storage::LocalBlockDefinition,2> strict_blocks{left,right};
        const auto glued = storage::materialize_glued_strict_blocks(source, strict_blocks);
        require(glued.complex.grade(core::Grade{0}).cell_count() == 3u,
                "gluing duplicated a shared grade-zero identity");
        require(glued.complex.grade(core::Grade{1}).cell_count() == 2u,
                "strict block gluing relation count mismatch");

        const auto base_path = std::filesystem::temp_directory_path()/"rfc_local_blocks_base.rfcx";
        const auto local_path = std::filesystem::temp_directory_path()/"rfc_local_blocks.rfcxl";
        const auto stats = storage::write_local_blocks(source,storage::write_rfcx(source,base_path).payload_hash,{left,right,partial},local_path);
        require(stats.blocks == 3u, "local block write count mismatch");
        require(stats.shared_cells >= 1u, "local glue index unexpectedly empty");

        storage::MappedRfcx mapped_source(base_path);
        storage::MappedLocalBlocks mapped(local_path);
        mapped.validate_against(mapped_source);

        // Reusing the same stable keys against different source semantics is
        // rejected by the source fingerprint, even when every referenced key
        // still exists. Reverse e01 and repair the face sign so the alternate
        // source remains a valid flat complex.
        core::RelationalComplexBuilder alt_builder(sig);
        alt_builder.add_grade_one(core::RelationKey{100}, edge, {
            {10,true,false,R{1},{},core::OccurrenceKey{1000}},
            {11,true,true,R{1},{},core::OccurrenceKey{1001}}});
        alt_builder.add_grade_one(core::RelationKey{101}, edge, {
            {11,true,true,R{1},{},core::OccurrenceKey{1010}},
            {12,true,false,R{1},{},core::OccurrenceKey{1011}}});
        alt_builder.add_grade_one(core::RelationKey{102}, edge, {
            {12,true,true,R{1},{},core::OccurrenceKey{1020}},
            {10,true,false,R{1},{},core::OccurrenceKey{1021}}});
        alt_builder.add_higher(core::Grade{2}, core::RelationKey{200}, face, {
            {0,true,true,R{1},{},core::OccurrenceKey{2000}},
            {1,true,false,R{1},{},core::OccurrenceKey{2001}},
            {2,true,false,R{1},{},core::OccurrenceKey{2002}}});
        const auto alt_source = alt_builder.freeze();
        const auto alt_path = std::filesystem::temp_directory_path()/"rfc_local_blocks_alt.rfcx";
        storage::write_rfcx(alt_source,alt_path);
        storage::MappedRfcx mapped_alt(alt_path);
        bool fingerprint_rejected = false;
        try { mapped.validate_against(mapped_alt); } catch (const std::runtime_error&) { fingerprint_rejected = true; }
        require(fingerprint_rejected, "local block image accepted a different RFCX source with reused stable keys");
        require(mapped.block_count() == 3u, "mapped local block count mismatch");
        const auto ml = mapped.find_block(1);
        require(ml && ml->kind() == storage::LocalBlockKind::StrictSubcomplex,
                "mapped strict block lookup mismatch");
        require(ml->payload_count() == 1u, "mapped local payload count mismatch");
        const auto roundtrip_payload = ml->payload(0);
        require(roundtrip_payload.payload_key == 900u && roundtrip_payload.cell == payload.cell,
                "local payload binding roundtrip mismatch");
        require(roundtrip_payload.tensor.domain().extents() == std::vector<std::size_t>({2,2}),
                "local payload shape roundtrip mismatch");
        require(roundtrip_payload.tensor.support().count() == 3u,
                "local payload structural support roundtrip mismatch");
        require(roundtrip_payload.tensor.state(0).value() == R{1,2} &&
                roundtrip_payload.tensor.state(1).value() == R{0} &&
                !roundtrip_payload.tensor.state(2).has_value() &&
                roundtrip_payload.tensor.state(3).value() == R{3},
                "local payload sparse exact state roundtrip mismatch");

        const auto shared_index = mapped.find_shared_cell(shared_v1);
        require(shared_index.has_value(), "mapped glue index missing stable interface key");
        const auto mapped_members = mapped.memberships(*shared_index);
        require(std::binary_search(mapped_members.begin(), mapped_members.end(), 1u) &&
                std::binary_search(mapped_members.begin(), mapped_members.end(), 2u),
                "mapped glue memberships mismatch");

        const auto partial_view = mapped.find_block(3);
        require(partial_view && partial_view->kind() == storage::LocalBlockKind::PartialChart,
                "mapped partial chart classification mismatch");
        require(partial_view->occurrence_count() == 2u, "mapped partial chart occurrence count mismatch");
        const auto face_occ = partial_view->occurrence(1);
        const auto face_slot = storage::resolve_stable_occurrence(mapped_source,face_occ);
        require(face_slot.has_value(), "partial chart occurrence did not resolve into source RFCX");
        const auto face_grade = mapped_source.grade(face_occ.upper_grade);
        require(face_grade.incidence_coefficient(*face_slot) == R{1},
                "local block did not expose exact source coefficient through stable occurrence reference");

        // A local block file is a compact source-native index, not a duplicate
        // RFCX object graph. For this fixture it should be far smaller than the base snapshot.
        require(stats.bytes < mapped_source.file_bytes(), "local block index unexpectedly duplicated full source storage");

        std::filesystem::remove(base_path);
        std::filesystem::remove(local_path);
        std::filesystem::remove(alt_path);
        std::cout << "RFCX local block and glue-index tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
