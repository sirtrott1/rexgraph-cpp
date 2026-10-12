#pragma once

#include "rfc/learning/relational_field_evidence.hpp"
#include "rfc/inference/relational_field_interface_codec.hpp"

namespace rfc::learning {
namespace relational_affine_codec_detail {
using namespace inference::field_interface_codec_detail;
inline constexpr std::array<std::byte, 8> evidence_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'F'}, std::byte{'E'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> affine_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'A'}, std::byte{'F'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> evaluation_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'A'}, std::byte{'E'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline void vector(Writer& w, const numeric::RationalVector& v) {
    w.u64(v.size()); for (const auto& q : v) storage::temporal_codec_detail::rational(w, q);
}
inline numeric::RationalVector vector(Reader& r) {
    const auto n = r.count(10); inference::field_program_detail::require(n <= r.limits().max_dimension, "affine vector read dimension");
    numeric::RationalVector v; for (std::size_t i = 0; i < n; ++i) v.push_back(r.rational()); return v;
}
inline void parameters(Writer& w, const RelationalFieldParameterDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.node); w.u64(d.variables.size());
    for (const auto& c : d.variables) { w.u64(c.occurrence); w.u64(c.row); w.u64(c.column); }
    storage::temporal_codec_detail::matrix(w, d.basis); w.u64(d.parameters.size()); for (const auto key : d.parameters) w.u64(key);
}
inline RelationalFieldParameterDeclaration parameters(Reader& r) {
    RelationalFieldParameterDeclaration d; d.key = r.u64(); d.revision = r.u64(); d.node = r.u64(); const auto n = r.count(24);
    for (std::size_t i = 0; i < n; ++i) { const auto p = size(r), row = size(r), col = size(r); d.variables.push_back({p, row, col}); }
    d.basis = storage::temporal_codec_detail::matrix(r); const auto count = r.count(8);
    for (std::size_t i = 0; i < count; ++i) d.parameters.push_back(r.u64());
    return d;
}
inline void witness(Writer& w, const PreparedRelationalFieldEvidence& e) {
    vector(w, e.particular()); storage::temporal_codec_detail::matrix(w, e.kernel());
    storage::temporal_codec_detail::matrix(w, e.chain_constraints());
    w.u64(e.chain_coordinates().size());
    for (const auto& c : e.chain_coordinates()) { w.u64(c.grade); w.u64(c.row); w.u64(c.column); }
    const auto& system = e.observation_system(); storage::temporal_codec_detail::matrix(w, system.design);
    storage::temporal_codec_detail::matrix(w, system.augmented); vector(w, system.target); w.u64(system.constraints.size());
    for (const auto& c : system.constraints) { w.u64(c.sample); w.u64(c.cell); w.u64(c.axis); }
    w.u64(e.free_parameters().size()); for (const auto key : e.free_parameters()) w.u64(key);
    w.u8(e.parent_relation() ? 1 : 0);
    if (e.parent_relation()) { vector(w, e.parent_relation()->shift); storage::temporal_codec_detail::matrix(w, e.parent_relation()->map); }
}
inline void witness(Reader& r, const PreparedRelationalFieldEvidence& e) {
    using inference::field_program_detail::require;
    require(vector(r) == e.particular(), "cold whole-field evidence particular differs");
    require(storage::temporal_codec_detail::matrix(r) == e.kernel(), "cold whole-field evidence kernel differs");
    require(storage::temporal_codec_detail::matrix(r) == e.chain_constraints(), "cold complete adjacent constraints differ");
    const auto chain = r.count(24); require(chain == e.chain_coordinates().size(), "cold complete adjacent constraint attribution count");
    for (const auto& c : e.chain_coordinates()) require(size(r) == c.grade && size(r) == c.row && size(r) == c.column, "cold complete adjacent constraint attribution");
    const auto& system = e.observation_system();
    require(storage::temporal_codec_detail::matrix(r) == system.design && storage::temporal_codec_detail::matrix(r) == system.augmented &&
        vector(r) == system.target, "cold complete original observation system differs");
    const auto n = r.count(24); require(n == system.constraints.size(), "cold observation attribution count");
    for (const auto& c : system.constraints) require(size(r) == c.sample && size(r) == c.cell && size(r) == c.axis, "cold original constraint attribution");
    const auto free = r.count(8); require(free == e.free_parameters().size(), "cold common named free axes");
    for (const auto key : e.free_parameters()) require(r.u64() == key, "cold free parameter identity");
    require(r.flag() == e.parent_relation().has_value(), "cold evidence parent relationship absent");
    if (e.parent_relation()) require(vector(r) == e.parent_relation()->shift && storage::temporal_codec_detail::matrix(r) == e.parent_relation()->map,
        "cold child does not preserve actual parent free-basis relationship");
}
// An archive explicitly declares that its inputs reference this serialized parent
// program. Adoption constructs fresh accessions against that one parent owner;
// this is not a general equal-bytes runtime owner coercion.
inline inference::OwnedAccessionFieldResult input(Reader& outer, const inference::OwnedRelationalFieldProgram& program,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits) {
    using inference::field_program_detail::require;
    const auto bytes = outer.blob(); Reader r(bytes, policy); header(r, bytes, accession_magic);
    const auto expected = inference::decode_relational_field_result(r.blob(), policy, limits);
    require(inference::encode_relational_field_program(expected.program()) == inference::encode_relational_field_program(program),
        "source-backed evidence archive references a different serialized parent program");
    const auto n = r.count(70); std::vector<inference::RelationalAccessionInput> inputs;
    for (std::size_t i = 0; i < n; ++i) {
        auto asset = inference::decode_relational_input_use_asset(r.blob(), policy, limits); auto d = accession(r);
        const inference::OwnedRelationalInputAccession a(std::move(asset), program, std::move(d), limits);
        const auto use = r.u64(); auto source = storage::temporal_codec_detail::tensor(r); inputs.push_back(a.admit(use, std::move(source), limits));
    }
    require(r.remaining() == 0, "trailing source-backed evidence input bytes");
    auto result = inference::execute_relational_inputs(program, std::move(inputs), limits);
    require(inference::encode_relational_field_result(expected) == inference::encode_relational_field_result(result.result()),
        "source-backed evidence input complete parent execution differs");
    canonical(bytes, inference::encode_accession_field_result(result)); return result;
}
inline void field(Writer& w, const RelationalAffineField& f) {
    storage::temporal_codec_detail::tensor(w, f.particular); storage::temporal_codec_detail::matrix(w, f.directions);
}
inline void field(Reader& r, const RelationalAffineField& expected) {
    const auto p = storage::temporal_codec_detail::tensor(r); const auto k = storage::temporal_codec_detail::matrix(r);
    inference::field_program_detail::require(inference::field_program_codec_detail::same_field(p, expected.particular) && k == expected.directions,
        "cold complete affine intermediary differs");
}
} // namespace relational_affine_codec_detail

inline std::vector<std::byte> encode_relational_field_evidence(const PreparedRelationalFieldEvidence& evidence) {
    using namespace relational_affine_codec_detail;
    std::vector<PreparedRelationalFieldEvidence> lineage; auto current = evidence;
    while (true) { lineage.push_back(current); const auto parent = current.parent(); if (!parent) break; current = *parent; }
    std::reverse(lineage.begin(), lineage.end()); auto w = envelope(evidence_magic); const auto& root = lineage.front();
    const auto limits = root.limits(); w.u64(limits.dimension); w.u64(limits.entries); w.u64(limits.steps); w.u64(limits.integer_bits);
    blob(w, inference::encode_relational_field_program(root.program())); parameters(w, root.declaration()); witness(w, root);
    w.u64(lineage.size()-1);
    for (std::size_t i = 1; i < lineage.size(); ++i) {
        const auto& state = lineage[i]; const auto first = lineage[i-1].observations().size(); const auto& observations = state.observations();
        w.u64(observations.size()-first);
        for (std::size_t j = first; j < observations.size(); ++j) {
            const auto& o = observations[j]; w.u64(o.key); blob(w, inference::encode_accession_field_result(o.input));
            blob(w, inference::encode_relational_input_use_asset(o.target_asset)); w.u64(o.target_use);
            storage::temporal_codec_detail::tensor(w, o.target); w.u64(o.observed.size());
            for (const auto c : o.observed) local_cell(w, c);
        }
        witness(w, state);
    }
    return finish(std::move(w));
}
inline PreparedRelationalFieldEvidence decode_relational_field_evidence(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap = {}) {
    using namespace relational_affine_codec_detail;
    Reader r(bytes, policy); header(r, bytes, evidence_magic);
    const numeric::ExactActionLimits limits{size(r), size(r), size(r), size(r)};
    inference::field_program_detail::require(limits.dimension <= cap.dimension && limits.entries <= cap.entries &&
        limits.steps <= cap.steps && limits.integer_bits <= cap.integer_bits, "archived whole-field evidence policy exceeds caller admission");
    const auto program = inference::decode_relational_field_program(r.blob(), policy, limits);
    auto d = parameters(r); PreparedRelationalFieldEvidence evidence(program, std::move(d), limits); witness(r, evidence);
    const auto updates = r.count(9);
    for (std::size_t i = 0; i < updates; ++i) {
        const auto count = r.count(50); std::vector<RelationalFieldTrainingObservation> suffix;
        for (std::size_t j = 0; j < count; ++j) {
            const auto key = r.u64(); auto original = input(r, program, policy, limits);
            auto asset = inference::decode_relational_input_use_asset(r.blob(), policy, limits); const auto use = r.u64();
            auto target = storage::temporal_codec_detail::tensor(r); const auto observed_count = r.count(6); std::vector<core::CellId> observed;
            for (std::size_t k = 0; k < observed_count; ++k) observed.push_back(local_cell(r));
            suffix.push_back({key, std::move(original), std::move(asset), use, std::move(target), std::move(observed)});
        }
        evidence = evidence.append(std::move(suffix), limits); witness(r, evidence);
    }
    inference::field_program_detail::require(r.remaining() == 0, "trailing whole-field evidence bytes");
    canonical(bytes, encode_relational_field_evidence(evidence)); return evidence;
}
inline std::vector<std::byte> encode_relational_affine_result(const RelationalAffineProgramResult& result) {
    using namespace relational_affine_codec_detail;
    const auto pin = result; const auto evidence = pin.evidence(); auto w = envelope(affine_magic);
    blob(w, encode_relational_field_evidence(evidence)); blob(w, inference::encode_accession_field_result(pin.input()));
    const auto program = evidence.program(); w.u64(program.declaration().nodes.size());
    for (const auto& n : program.declaration().nodes) {
        w.u64(n.key); field(w, pin.field(n.key));
        if (n.kind == inference::FieldProgramNodeKind::Boundary) field(w, pin.occurrence_field(n.key));
    }
    return finish(std::move(w));
}
inline RelationalAffineProgramResult decode_relational_affine_result(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace relational_affine_codec_detail;
    Reader r(bytes, policy); header(r, bytes, affine_magic); const auto evidence = decode_relational_field_evidence(r.blob(), policy, limits);
    const auto program = evidence.program(); auto original = input(r, program, policy, limits); auto result = evidence.execute(std::move(original), limits);
    const auto count = r.count(32); inference::field_program_detail::require(count == program.declaration().nodes.size(), "cold affine result omits whole intermediary fields");
    for (const auto& n : program.declaration().nodes) {
        inference::field_program_detail::require(r.u64() == n.key, "cold affine program order"); field(r, result.field(n.key));
        if (n.kind == inference::FieldProgramNodeKind::Boundary) field(r, result.occurrence_field(n.key));
    }
    inference::field_program_detail::require(r.remaining() == 0, "trailing affine result bytes");
    canonical(bytes, encode_relational_affine_result(result)); return result;
}
inline std::vector<std::byte> encode_relational_affine_evaluation(const RelationalAffineEvaluation& evaluation) {
    using namespace relational_affine_codec_detail;
    const auto pin = evaluation; auto w = envelope(evaluation_magic); blob(w, encode_relational_affine_result(pin.family()));
    vector(w, pin.free_values()); blob(w, inference::encode_accession_field_result(pin.result())); return finish(std::move(w));
}
inline RelationalAffineEvaluation decode_relational_affine_evaluation(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace relational_affine_codec_detail;
    Reader r(bytes, policy); header(r, bytes, evaluation_magic); const auto family = decode_relational_affine_result(r.blob(), policy, limits);
    const auto free = vector(r); auto evaluation = family.evaluate(free, limits);
    canonical(r.blob(), inference::encode_accession_field_result(evaluation.result()));
    inference::field_program_detail::require(r.remaining() == 0, "trailing affine evaluation bytes");
    canonical(bytes, encode_relational_affine_evaluation(evaluation)); return evaluation;
}
} // namespace rfc::learning
