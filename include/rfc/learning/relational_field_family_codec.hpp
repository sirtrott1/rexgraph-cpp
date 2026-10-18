#pragma once

#include "rfc/learning/relational_field_family.hpp"
#include "rfc/learning/relational_affine_program_codec.hpp"

namespace rfc::learning {
namespace relational_family_codec_detail {
using namespace relational_affine_codec_detail;
inline constexpr std::array<std::byte, 8> family_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'J'}, std::byte{'F'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> joint_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'J'}, std::byte{'R'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> joint_evaluation_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'J'}, std::byte{'E'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline void views(Writer& w, const std::vector<std::vector<TransportContextView>>& all) {
    w.u64(all.size()); for (const auto& row : all) {
        w.u64(row.size()); for (const auto& v : row) {
            w.u64(v.frame); storage::temporal_codec_detail::matrix(w, v.accession);
            storage::temporal_codec_detail::matrix(w, v.reconstruction);
        }
    }
}
inline std::vector<std::vector<TransportContextView>> views(Reader& r) {
    std::vector<std::vector<TransportContextView>> all; const auto grades = r.count(8);
    for (std::size_t k = 0; k < grades; ++k) {
        std::vector<TransportContextView> row; const auto n = r.count(40);
        for (std::size_t i = 0; i < n; ++i) {
            const auto frame = r.u64(); auto a = storage::temporal_codec_detail::matrix(r);
            auto reconstruction = storage::temporal_codec_detail::matrix(r);
            row.push_back({frame, std::move(a), std::move(reconstruction)});
        }
        all.push_back(std::move(row));
    }
    return all;
}
inline void proof(Writer& w, const PreparedTransportFamilyGluing& g) {
    w.u8(g.consistent() ? 1 : 0); w.u64(g.rank()); vector(w, g.coordinate_origin());
    storage::temporal_codec_detail::matrix(w, g.coordinate_kernel()); vector(w, g.particular());
    storage::temporal_codec_detail::matrix(w, g.kernel()); vector(w, g.residual_origin());
    storage::temporal_codec_detail::matrix(w, g.equations()); vector(w, g.contradiction());
}
inline void proof(Reader& r, const PreparedTransportFamilyGluing& g) {
    inference::field_program_detail::require(r.flag() == g.consistent() && size(r) == g.rank(), "cold field family consistency/rank");
    inference::field_program_detail::require(vector(r) == g.coordinate_origin() && storage::temporal_codec_detail::matrix(r) == g.coordinate_kernel() &&
        vector(r) == g.particular() && storage::temporal_codec_detail::matrix(r) == g.kernel() && vector(r) == g.residual_origin() &&
        storage::temporal_codec_detail::matrix(r) == g.equations() && vector(r) == g.contradiction(), "cold field family exact joint proof differs");
}
inline storage::PreparedBytePacket prepare_family(const PreparedRelationalFieldFamily& family, PacketContext& context) {
    const auto pin = family; const auto& d = pin.declaration(); if (context.check) { context.check(); }
    storage::BytePacketBuilder w(family_magic, context.max_bytes, context.byte_guard.get());
    const auto l = pin.limits(); w.u64(l.dimension); w.u64(l.entries); w.u64(l.steps); w.u64(l.integer_bits);
    w.u64(d.key); w.u64(d.revision); w.u64(pin.members().size());
    for (const auto& member : pin.members()) { if (context.check) { context.check(); } w.blob(prepare_evidence(member, context)); }
    w.u64(d.interfaces.size());
    for (const auto& i : d.interfaces) {
        if (context.check) { context.check(); }
        w.u64(i.key); w.u64(i.revision); w.u64(i.left); w.u64(i.right); w.u64(i.common.source_namespace());
        // A declared cutoff snapshot, not an invented publication history.
        const auto source = storage::encode_rfcx(i.common.endpoint().source);
        const auto endpoint = storage::encode_temporal_endpoint(i.common.endpoint());
        w.write([&](auto& raw) { blob(raw, source.bytes); blob(raw, endpoint); source_map(raw, i.common_left); source_map(raw, i.common_right);
            views(raw, i.left_views); views(raw, i.right_views); });
    }
    w.write([&](auto& raw) { proof(raw, pin.gluing()); }); return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_joint(const RelationalJointFieldResult& result, PacketContext& context) {
    const auto pin = result; storage::BytePacketBuilder w(joint_magic, context.max_bytes, context.byte_guard.get());
    w.blob(prepare_family(pin.family(), context)); w.u64(pin.member());
    w.blob(inference::field_interface_codec_detail::prepare_accession(pin.input(), context));
    const auto owner = pin.family(); const auto program = owner.members().at(pin.member()).program(); w.u64(program.declaration().nodes.size());
    for (const auto& n : program.declaration().nodes) {
        if (context.check) { context.check(); }
        w.write([&](auto& raw) { raw.u64(n.key); field(raw, pin.field(n.key));
            if (n.kind == inference::FieldProgramNodeKind::Boundary) field(raw, pin.occurrence_field(n.key)); });
    }
    return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_joint_evaluation(const RelationalJointFieldEvaluation& evaluation, PacketContext& context) {
    const auto pin = evaluation; storage::BytePacketBuilder w(joint_evaluation_magic, context.max_bytes, context.byte_guard.get());
    w.blob(prepare_joint(pin.family(), context)); w.write([&](auto& raw) { vector(raw, pin.free_values()); });
    w.blob(inference::field_interface_codec_detail::prepare_accession(pin.result(), context)); return std::move(w).finish(context.check);
}
} // namespace relational_family_codec_detail

inline storage::PreparedBytePacket prepare_relational_field_family(const PreparedRelationalFieldFamily& family,
    std::size_t max_bytes, std::function<void()> check = {}) {
    relational_family_codec_detail::PacketContext context(max_bytes, std::move(check));
    return relational_family_codec_detail::prepare_family(family, context);
}
inline std::vector<std::byte> encode_relational_field_family(const PreparedRelationalFieldFamily& family,
    std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_relational_field_family(family, max_bytes, std::move(check)).bytes();
}
inline PreparedRelationalFieldFamily decode_relational_field_family(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap = {}, std::function<void()> check = {}) {
    using namespace relational_family_codec_detail;
    if (check) { check(); } Reader r(bytes, policy); header(r, bytes, family_magic);
    const numeric::ExactActionLimits limits{size(r), size(r), size(r), size(r)};
    inference::field_program_detail::require(limits.dimension <= cap.dimension && limits.entries <= cap.entries &&
        limits.steps <= cap.steps && limits.integer_bits <= cap.integer_bits, "archived joint family policy exceeds caller admission");
    RelationalFieldFamilyDeclaration d; d.key = r.u64(); d.revision = r.u64(); const auto n = r.count(8);
    inference::field_program_detail::require(n <= limits.dimension, "field family member dimension admission");
    std::vector<PreparedRelationalFieldEvidence> members;
    for (std::size_t i = 0; i < n; ++i) { if (check) { check(); } members.push_back(decode_relational_field_evidence(r.blob(), policy, limits)); }
    const auto count = r.count(80); inference::field_program_detail::require(count <= limits.dimension, "field family interface dimension admission");
    for (std::size_t j = 0; j < count; ++j) {
        if (check) { check(); } const auto key = r.u64(), revision = r.u64(); const auto left = size(r), right = size(r); const auto ns = r.u64();
        const auto source_bytes = r.blob(); const auto endpoint_bytes = r.blob(); const storage::MappedRfcx source(source_bytes);
        const auto endpoint = storage::decode_temporal_endpoint(source.materialize(), endpoint_bytes, policy);
        storage::NativeSeriesLimits publication_policy; publication_policy.temporal = policy; publication_policy.max_archive_bytes = policy.max_file_bytes;
        const auto common = storage::NativeTemporalSeries(endpoint, ns, publication_policy).tail();
        auto left_map = source_map(r); auto right_map = source_map(r); auto left_views = views(r); auto right_views = views(r);
        d.interfaces.push_back({key, revision, left, right, common, std::move(left_map), std::move(right_map), std::move(left_views), std::move(right_views)});
    }
    PreparedRelationalFieldFamily family(std::move(d), std::move(members), limits, check); proof(r, family.gluing());
    inference::field_program_detail::require(r.remaining() == 0, "trailing field family bytes");
    canonical(bytes, prepare_relational_field_family(family, policy.max_file_bytes, check), check); return family;
}
inline storage::PreparedBytePacket prepare_relational_joint_result(const RelationalJointFieldResult& result,
    std::size_t max_bytes, std::function<void()> check = {}) {
    relational_family_codec_detail::PacketContext context(max_bytes, std::move(check)); return relational_family_codec_detail::prepare_joint(result, context);
}
inline std::vector<std::byte> encode_relational_joint_result(const RelationalJointFieldResult& result,
    std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_relational_joint_result(result, max_bytes, std::move(check)).bytes();
}
inline RelationalJointFieldResult decode_relational_joint_result(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap = {}, std::function<void()> check = {}) {
    using namespace relational_family_codec_detail;
    if (check) { check(); } Reader r(bytes, policy); header(r, bytes, joint_magic);
    const auto family = decode_relational_field_family(r.blob(), policy, cap, check); const auto member = size(r);
    const auto program = family.members().at(member).program(); if (check) { check(); }
    auto original = input(r, program, policy, family.limits()); auto result = family.execute(member, std::move(original), family.limits(), check);
    const auto count = r.count(32); inference::field_program_detail::require(count == program.declaration().nodes.size(), "cold joint complete node count");
    for (const auto& n : program.declaration().nodes) {
        if (check) { check(); } inference::field_program_detail::require(r.u64() == n.key, "cold joint node order"); field(r, result.field(n.key));
        if (n.kind == inference::FieldProgramNodeKind::Boundary) field(r, result.occurrence_field(n.key));
    }
    inference::field_program_detail::require(r.remaining() == 0, "trailing joint result bytes");
    canonical(bytes, prepare_relational_joint_result(result, policy.max_file_bytes, check), check); return result;
}
inline storage::PreparedBytePacket prepare_relational_joint_evaluation(const RelationalJointFieldEvaluation& evaluation,
    std::size_t max_bytes, std::function<void()> check = {}) {
    relational_family_codec_detail::PacketContext context(max_bytes, std::move(check));
    return relational_family_codec_detail::prepare_joint_evaluation(evaluation, context);
}
inline std::vector<std::byte> encode_relational_joint_evaluation(const RelationalJointFieldEvaluation& evaluation,
    std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_relational_joint_evaluation(evaluation, max_bytes, std::move(check)).bytes();
}
inline RelationalJointFieldEvaluation decode_relational_joint_evaluation(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits cap = {}, std::function<void()> check = {}) {
    using namespace relational_family_codec_detail;
    if (check) { check(); } Reader r(bytes, policy); header(r, bytes, joint_evaluation_magic);
    const auto result = decode_relational_joint_result(r.blob(), policy, cap, check); const auto family = result.family();
    const auto free = vector(r); const auto evaluated = result.evaluate(free, family.limits(), check);
    canonical(r.blob(), inference::encode_accession_field_result(evaluated.result()));
    inference::field_program_detail::require(r.remaining() == 0, "trailing joint evaluation bytes");
    canonical(bytes, prepare_relational_joint_evaluation(evaluated, policy.max_file_bytes, check), check); return evaluated;
}
} // namespace rfc::learning
