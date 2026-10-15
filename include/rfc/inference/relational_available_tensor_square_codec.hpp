#pragma once

#include "rfc/inference/relational_available_tensor_square.hpp"
#include "rfc/inference/relational_known_tensor_square_codec.hpp"

namespace rfc::inference {
namespace available_tensor_square_codec_detail {
using namespace field_interface_codec_detail;
inline constexpr std::array<std::byte, 8> input_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'A'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> result_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'R'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline void origins(Writer& w, RelationalTensorSquareOrigins o) {
    w.u64(o.parent_program.program); w.u64(o.parent_program.maps); w.u64(o.parent_program.metrics);
    w.u64(o.parent_input.asset); w.u64(o.parent_input.value); w.u64(o.parent_input.accession); w.u64(o.parent_input.reconstruction);
    w.u64(o.declaration);
    w.u64(o.program.program); w.u64(o.program.maps); w.u64(o.program.metrics);
    w.u64(o.input.asset); w.u64(o.input.value); w.u64(o.input.accession); w.u64(o.input.reconstruction);
}
inline RelationalTensorSquareOrigins origins(Reader& r) {
    RelationalTensorSquareOrigins o;
    o.parent_program = {r.u64(), r.u64(), r.u64()}; o.parent_input = {r.u64(), r.u64(), r.u64(), r.u64()};
    o.declaration = r.u64(); o.program = {r.u64(), r.u64(), r.u64()}; o.input = {r.u64(), r.u64(), r.u64(), r.u64()};
    return o;
}
inline auto roles(RelationalTensorSquareOrigins o) {
    using R = RelationalDependencyRole;
    return std::array<std::pair<std::uint64_t, R>, 15>{{
        {o.parent_program.program, R::Program}, {o.parent_program.maps, R::Maps}, {o.parent_program.metrics, R::Metrics},
        {o.parent_input.asset, R::InputAsset}, {o.parent_input.value, R::InputValue},
        {o.parent_input.accession, R::InputAccession}, {o.parent_input.reconstruction, R::Reconstruction},
        {o.declaration, R::Interface},
        {o.program.program, R::Program}, {o.program.maps, R::Maps}, {o.program.metrics, R::Metrics},
        {o.input.asset, R::InputAsset}, {o.input.value, R::InputValue}, {o.input.accession, R::InputAccession},
        {o.input.reconstruction, R::Reconstruction}}};
}
struct PassiveInput {
    storage::ByteRange native;
    RelationalTensorSquareOrigins origins;
    std::vector<OwnedRelationalValueOrigin> obligations;
    RelationalInputAccessionDeclaration parent_declaration;
    std::optional<numeric::SparseRationalTensor> parent_field;
    std::span<const std::byte> parent_asset;
    std::span<const std::byte> parent_program;
};
inline std::span<const std::byte> require_program(const storage::ByteRange& bytes, RelationalProgramOrigins keys,
    const RelationalAvailabilityContext& context) {
    const auto program=context.require_bytes(keys.program, RelationalDependencyRole::Program, bytes);
    (void)context.require_bytes(keys.maps, RelationalDependencyRole::Maps, bytes);
    (void)context.require_bytes(keys.metrics, RelationalDependencyRole::Metrics, bytes);
    return program.payload_bytes();
}
// Only passive envelope/coordinate parsing occurs here. No native source,
// program, accession or product is constructed before all fifteen obligations
// and their exact links to BOTH original and derived content are admitted.
inline PassiveInput preflight_image(storage::ByteRange bytes, const RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy, const std::function<void()>& check = {}) {
    Reader r(bytes, policy); header(r, bytes, input_magic); const auto native = r.view_blob(); const auto keys = origins(r);
    PassiveInput out{native, keys, {}, {}, {}, {}, {}};
    for (const auto& [key, role] : roles(keys)) {
        if (check) check();
        const auto receipt = r.view_blob(); Reader saved(receipt, policy); header(saved, receipt, value_admission_detail::origin_magic);
        const auto identity = saved.u64(); const auto kind = saved.u8(); (void)saved.view_blob(); const auto payload = saved.view_blob();
        value_admission_detail::require(!saved.remaining() && identity == key && kind == static_cast<unsigned>(role),
            "known tensor saved original obligation role or key differs");
        const auto actual = context.require_bytes(key, role, payload);
        canonical(receipt, actual.receipt()); out.obligations.push_back(actual);
    }
    value_admission_detail::require(!r.remaining(), "trailing available known tensor input bytes");
    Reader joined(native, policy); header(joined, native, known_tensor_square_codec_detail::input_magic);
    const auto square_bytes = joined.view_blob(); const auto target_program = joined.view_blob();
    auto target_declaration = accession(joined); auto target_field = storage::temporal_codec_detail::tensor(joined);
    (void)storage::temporal_codec_detail::tensor(joined);
    value_admission_detail::require(!joined.remaining(), "trailing native known tensor input bytes");
    Reader square(square_bytes, policy); header(square, square_bytes, known_tensor_square_codec_detail::square_magic);
    const auto parent_program = square.view_blob(); const auto asset_bytes = square.blob();
    auto parent_declaration = accession(square); const auto use = square.u64();
    auto parent_field = storage::temporal_codec_detail::tensor(square);
    const auto declaration = known_tensor_square_codec_detail::declaration(square); (void)square.view_blob();
    value_admission_detail::require(!square.remaining(), "trailing native known tensor square bytes");
    const auto parent_bytes=require_program(parent_program, keys.parent_program, context);
    const auto target_bytes=require_program(target_program, keys.program, context);
    const std::vector<std::byte> asset(asset_bytes.begin(), asset_bytes.end());
    std::vector<OwnedRelationalValueOrigin> checked;
    value_admission_detail::input_origins(checked, keys.parent_input, asset,
        relational_input_value_payload(asset, parent_declaration, use, parent_field),
        relational_input_accession_payload(parent_bytes, asset, parent_declaration, use, parent_field), context);
    (void)context.require(keys.declaration, RelationalDependencyRole::Interface,
        relational_tensor_square_dependency_payload(parent_bytes, declaration));
    value_admission_detail::input_origins(checked, keys.input, asset,
        relational_input_value_payload(asset, target_declaration, use, target_field),
        relational_input_accession_payload(target_bytes, asset, target_declaration, use, target_field), context);
    out.parent_declaration = std::move(parent_declaration); out.parent_field = std::move(parent_field); out.parent_asset=asset_bytes;out.parent_program=parent_bytes;
    return out;
}
inline PassiveInput preflight(std::span<const std::byte> bytes,const RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,const std::function<void()>& check={}) {
    return preflight_image(storage::ByteRange(bytes),context,policy,check);
}
} // namespace available_tensor_square_codec_detail
namespace available_tensor_square_codec_detail {
inline storage::PreparedBytePacket prepare_input(const AvailableRelationalTensorSquareInput& input,field_interface_codec_detail::PacketContext& context){
    using namespace available_tensor_square_codec_detail;
    const auto pin=input;storage::BytePacketBuilder w(input_magic,context.max_bytes,context.byte_guard.get());
    w.blob(known_tensor_square_codec_detail::prepare_input(pin.native_input(),context));w.write([&](auto& raw){origins(raw,pin.origins());});
    field_program_detail::require(pin.obligations().size() == 15, "complete known tensor availability ancestry required");
    for(const auto& origin:pin.obligations()){w.receipt(origin);}
    return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_result(const AvailableRelationalTensorSquareResult& result,field_interface_codec_detail::PacketContext& context){
    const auto pin=result;storage::BytePacketBuilder w(available_tensor_square_codec_detail::result_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_input(pin.input(),context));w.blob(field_interface_codec_detail::prepare_accession(pin.result(),context));return std::move(w).finish(context.check);
}
} // namespace available_tensor_square_codec_detail
inline std::vector<std::byte> encode_available_relational_tensor_square_input(const AvailableRelationalTensorSquareInput& input){
    field_interface_codec_detail::PacketContext context;return available_tensor_square_codec_detail::prepare_input(input,context).bytes();
}
namespace available_tensor_square_codec_detail {
inline AvailableRelationalTensorSquareInput decode_image_input(storage::ByteRange frozen,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {},
    std::optional<OwnedRelationalFieldProgram> declared_target = {},
    const std::function<void(const PassiveInput&)>& image_check = {}) {
    value_admission_detail::bound(frozen.size(),policy.max_file_bytes);
    const auto authority = context; const auto callback = std::move(check); if (callback) callback();
    const auto parsed = available_tensor_square_codec_detail::preflight_image(frozen, authority, policy, callback);
    if(image_check)image_check(parsed);
    available_tensor_square_detail::recheck(parsed.obligations, authority, callback);
    auto native = known_tensor_square_codec_detail::decode_input_image(parsed.native, policy, limits, callback, std::move(declared_target),parsed.parent_program);
    auto out = admit_available_relational_tensor_square_input(std::move(native), parsed.origins, authority, callback);
    field_interface_codec_detail::PacketContext packets(policy.max_file_bytes,callback);
    field_program_codec_detail::canonical(frozen,prepare_input(out,packets),packets.check);return out;
}
inline AvailableRelationalTensorSquareInput decode_frozen_input(std::span<const std::byte> frozen,
    const RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={},std::optional<OwnedRelationalFieldProgram> declared_target={},
    const std::function<void(const PassiveInput&)>& image_check={}) {
    return decode_image_input(storage::ByteRange(frozen),context,policy,limits,std::move(check),std::move(declared_target),image_check);
}
} // namespace available_tensor_square_codec_detail
inline AvailableRelationalTensorSquareInput decode_available_relational_tensor_square_input(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    std::optional<OwnedRelationalFieldProgram> target={}) {
    const auto policy=caller_policy;
    const auto frozen=known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;
    return available_tensor_square_codec_detail::decode_frozen_input(frozen,authority,policy,limits,std::move(check),std::move(target));
}
inline std::vector<std::byte> encode_available_relational_tensor_square_result(const AvailableRelationalTensorSquareResult& result) {
    field_interface_codec_detail::PacketContext context;return available_tensor_square_codec_detail::prepare_result(result,context).bytes();
}
namespace available_tensor_square_codec_detail {
inline AvailableRelationalTensorSquareResult decode_image_result(storage::ByteRange frozen,
    const RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {},
    std::optional<OwnedRelationalFieldProgram> declared_target = {},
    const std::function<void(const PassiveInput&)>& image_check = {}) {
    using namespace available_tensor_square_codec_detail;
    value_admission_detail::bound(frozen.size(),policy.max_file_bytes);
    const auto authority = context; const auto callback = std::move(check); if (callback) callback();
    Reader r(frozen, policy); header(r, frozen, available_tensor_square_codec_detail::result_magic);
    const auto input = r.view_blob(); const auto expected = r.view_blob();
    value_admission_detail::require(!r.remaining(), "trailing available known tensor result bytes");
    const auto owned = decode_image_input(input, authority, policy, limits, callback, std::move(declared_target),image_check);
    auto out = owned.execute(authority, limits, callback);field_interface_codec_detail::PacketContext packets(policy.max_file_bytes,callback);
    canonical(expected,field_interface_codec_detail::prepare_accession(out.result(),packets),packets.check);
    canonical(frozen,prepare_result(out,packets),packets.check);return out;
}
inline AvailableRelationalTensorSquareResult decode_frozen_result(std::span<const std::byte> frozen,
    const RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={},std::optional<OwnedRelationalFieldProgram> declared_target={},
    const std::function<void(const PassiveInput&)>& image_check={}) {
    return decode_image_result(storage::ByteRange(frozen),context,policy,limits,std::move(check),std::move(declared_target),image_check);
}
} // namespace available_tensor_square_codec_detail
inline AvailableRelationalTensorSquareResult decode_available_relational_tensor_square_result(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    std::optional<OwnedRelationalFieldProgram> target={}) {
    const auto policy=caller_policy;
    const auto frozen=known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;
    return available_tensor_square_codec_detail::decode_frozen_result(frozen,authority,policy,limits,std::move(check),std::move(target));
}
} // namespace rfc::inference
