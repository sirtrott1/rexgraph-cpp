#pragma once

#include "rfc/learning/relational_available_local_solve.hpp"

namespace rfc::learning {
namespace available_local_codec_detail {
using namespace available_local_detail;
using Reader = inference::field_program_codec_detail::Reader;
using inference::field_program_codec_detail::blob;
using inference::field_program_codec_detail::canonical;
inline constexpr std::array<std::byte, 8> magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'L'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline inference::RelationalFieldSolveDeclaration declaration(Reader& r) {
    inference::RelationalFieldSolveDeclaration d;
    d.key = r.u64(); d.revision = r.u64(); d.coordinate_carrier = r.u64(); d.coordinate_basis = r.u64(); d.grade = {r.u16()};
    d.kind = static_cast<inference::RelationalFieldSolveKind>(r.u8()); d.lambda = r.rational();
    if (r.flag()) d.harmonic_frame = storage::temporal_codec_detail::matrix(r);
    const auto n = r.count(8); for (std::size_t i = 0; i < n; ++i) d.interior.push_back(inference::field_interface_codec_detail::size(r));
    return d;
}
inline RelationalSolveReadoutDeclaration readout(Reader& r) {
    RelationalSolveReadoutDeclaration d; const auto grades = r.count(8);
    for (std::size_t k = 0; k < grades; ++k) {
        const auto n = r.count(4); std::vector<std::uint32_t> cells;
        for (std::size_t i = 0; i < n; ++i) cells.push_back(r.u32());
        d.patch.push_back(std::move(cells));
    }
    d.view = inference::field_interface_codec_detail::view(r); return d;
}
inline std::vector<std::byte> witness(const AvailableRelationalLocalResult& value) {
    Writer w; const auto result = value.result(); const auto problem = result.problem(); const auto& op = problem.operators();
    for (const auto* m : {&op.lower, &op.upper, &op.metric, &op.below_metric, &op.above_metric, &op.frame, &op.augmented, &problem.partition().schur()})
        storage::temporal_codec_detail::matrix(w, *m);
    const auto& p = op.layout; for (const auto i : {p.a, p.b, p.c, p.d, p.f, p.g, p.e, p.t, p.x, p.size}) w.u64(i);
    w.u64(problem.partition().interface().size()); for (const auto i : problem.partition().interface()) w.u64(i);
    for (const auto* v : {&result.augmented_rhs(), &result.reconstructed(), &result.interface_response(), &result.interface_rhs(), &result.residual()})
        relational_affine_codec_detail::vector(w, *v);
    storage::temporal_codec_detail::tensor(w, result.field());
    const auto parent = value.parent(); storage::temporal_codec_detail::matrix(w, parent.readout_metric());
    storage::temporal_codec_detail::tensor(w, value.patch_field()); storage::temporal_codec_detail::tensor(w, value.visible());
    storage::temporal_codec_detail::tensor(w, value.reconstructed()); return std::move(w).take_bytes();
}
} // namespace available_local_codec_detail
// Independently sufficient packet: complete global context is intentionally
// retained. No separate model file, implicit authority or clipped global inverse.
inline std::vector<std::byte> encode_available_relational_local_result(const AvailableRelationalLocalResult& result) {
    using namespace available_local_codec_detail;
    const auto pin = result; const auto parent = pin.parent(); auto w = inference::field_program_codec_detail::envelope(magic);
    const auto rhs = parent.rhs(); w.u8(static_cast<std::uint8_t>(rhs.index())); blob(w, encode_rhs(rhs)); w.u64(parent.node());
    available_local_detail::declaration(w, parent.declaration()); available_local_detail::readout(w, parent.readout());
    const auto keys = parent.origin_keys(); for (const auto key : {keys.problem, keys.interface, keys.reconstruction, keys.gauge}) w.u64(key);
    w.u64(parent.obligations().size()); for (const auto& origin : parent.obligations()) blob(w, origin.receipt());
    blob(w, witness(pin)); return inference::field_program_codec_detail::finish(std::move(w));
}
inline AvailableRelationalLocalResult decode_available_relational_local_result(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace available_local_codec_detail;
    inference::value_admission_detail::bound(bytes.size(), policy.max_file_bytes);
    const std::vector<std::byte> frozen(bytes.begin(), bytes.end()); const auto authority = context; const auto callback = std::move(check);
    if (callback) callback();
    Reader r(frozen, policy); inference::field_program_codec_detail::header(r, frozen, magic);
    const auto kind = r.u8(); require(kind <= 2, "local solve RHS kind"); const auto raw_rhs = r.blob(); const auto node = r.u64();
    const auto d = declaration(r); const auto read = readout(r); const RelationalSolveOrigins keys{r.u64(), r.u64(), r.u64(), r.u64()};
    std::vector<std::vector<std::byte>> receipts; const auto n = r.count(8);
    for (std::size_t i = 0; i < n; ++i) { const auto receipt = r.blob(); receipts.emplace_back(receipt.begin(), receipt.end()); }
    const auto expected = r.blob(); require(!r.remaining(), "trailing local solve packet bytes");
    // All nested original obligations and all local declarations precede replay.
    std::optional<inference::value_admission_codec_detail::PassiveAvailableResult> fixed;
    std::optional<available_evidence_codec_detail::PassiveFamily> family;
    numeric::RationalVector free; std::optional<Origin> setting; std::vector<std::byte> choice_witness;
    std::optional<available_extension_codec_detail::PassiveEvaluation> extension;
    if (!kind) fixed = inference::value_admission_codec_detail::preflight_result(raw_rhs, authority, policy, callback);
    else if (kind == 2) extension = available_extension_codec_detail::preflight_evaluation(raw_rhs, authority, policy, limits, callback);
    else {
        Reader q(raw_rhs, policy); inference::field_program_codec_detail::header(q, raw_rhs, available_evidence_codec_detail::choice_magic);
        family = available_evidence_codec_detail::preflight_family(q.blob(), authority, policy, limits, callback);
        free = relational_affine_codec_detail::vector(q); const auto key = q.u64(); const auto receipt = q.blob();
        const auto payload = relational_parameter_setting_payload(family->evidence.native.span(), family->evidence.obligations, free);
        setting = authority.require(key, Role::Prospective, payload); canonical(receipt, setting->receipt());
        const auto native = q.blob(); choice_witness.assign(native.begin(), native.end()); require(!q.remaining(), "trailing local choice bytes");
    }
    const auto payload = problem_payload(raw_rhs, node, d); const auto actual = origins(payload, read, d, keys, authority);
    require(actual.size() == receipts.size(), "local solve incomplete origin closure");
    for (std::size_t i = 0; i < actual.size(); ++i) { if (callback) callback(); canonical(receipts[i], actual[i].receipt()); }
    auto rhs = [&]() -> AvailableRelationalSolveRhs {
        if (fixed) return inference::value_admission_codec_detail::replay_result(*fixed, authority, policy, limits, callback);
        if (extension) return available_extension_codec_detail::replay_evaluation(*extension, authority, policy, limits, callback);
        const auto f = available_evidence_codec_detail::replay_family(*family, authority, policy, limits, callback);
        auto choice = f.evaluate(free, setting->key(), authority, limits, callback);
        canonical(choice_witness, encode_relational_affine_evaluation(choice.result())); return choice;
    }();
    canonical(raw_rhs, encode_rhs(rhs));
    const PreparedAvailableRelationalLocalSolve prepared(std::move(rhs), node, d, read, keys, authority, limits, callback);
    auto out = prepared.execute(authority, limits, callback); canonical(expected, witness(out));
    canonical(frozen, encode_available_relational_local_result(out)); return out;
}
} // namespace rfc::learning
