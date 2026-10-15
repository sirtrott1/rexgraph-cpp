#pragma once

#include "rfc/inference/relational_known_tensor_square.hpp"
#include "rfc/inference/relational_field_interface_codec.hpp"

namespace rfc::inference {
namespace known_tensor_square_codec_detail {
using namespace field_interface_codec_detail;
inline constexpr std::array<std::byte, 8> square_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'S'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> input_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'I'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline void declaration(Writer& w, const RelationalKnownTensorSquareDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.lifted_basis); local_cell(w, d.context);
    w.u64(d.pair_directions.size()); for (const auto key : d.pair_directions) w.u64(key);
}
inline RelationalKnownTensorSquareDeclaration declaration(Reader& r) {
    RelationalKnownTensorSquareDeclaration d;
    d.key = r.u64(); d.revision = r.u64(); d.lifted_basis = r.u64(); d.context = local_cell(r);
    const auto n = r.count(8); for (std::size_t i = 0; i < n; ++i) d.pair_directions.push_back(r.u64());
    return d;
}
inline RelationalAccessionInput parent(Reader& r, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits, const std::function<void()>& check,std::span<const std::byte> checked_program={}) {
    const auto program_image=r.view_blob();
    if(!checked_program.empty())canonical(program_image,checked_program);
    auto program = decode_relational_field_program(checked_program.empty()?program_image.span():checked_program, policy, limits);
    auto asset = decode_relational_input_use_asset(r.blob(), policy, limits); auto d = accession(r);
    const OwnedRelationalInputAccession a(std::move(asset), std::move(program), std::move(d), limits, check);
    const auto use = r.u64(); auto field = storage::temporal_codec_detail::tensor(r);
    return a.admit(use, std::move(field), limits, check);
}
inline std::vector<std::byte> witness(const OwnedRelationalKnownTensorSquare& square) {
    Writer w; storage::temporal_codec_detail::tensor(w, square.context_field());
    storage::temporal_codec_detail::tensor(w, square.tensor_square());
    storage::temporal_codec_detail::tensor(w, square.lifted_field());
    storage::temporal_codec_detail::matrix(w, square.metric()); return std::move(w).take_bytes();
}
inline std::vector<std::byte> freeze(std::span<const std::byte> bytes, const storage::TemporalReadLimits& policy) {
    field_program_detail::require(bytes.size() <= policy.max_file_bytes, "known tensor packet exceeds byte admission");
    return {bytes.begin(), bytes.end()};
}
} // namespace known_tensor_square_codec_detail

namespace known_tensor_square_codec_detail {
inline storage::PreparedBytePacket prepare_square(const OwnedRelationalKnownTensorSquare& square,field_interface_codec_detail::PacketContext& context){
    using namespace known_tensor_square_codec_detail;
    const auto pin=square;const auto input=pin.parent();const auto a=input.accession();storage::BytePacketBuilder w(square_magic,context.max_bytes,context.byte_guard.get());
    w.blob(context.program(a.program()));w.blob(context.asset(a.asset()));
    w.write([&](auto& raw){accession(raw,a.declaration());raw.u64(input.use());storage::temporal_codec_detail::tensor(raw,input.source_field());declaration(raw,pin.declaration());});
    w.blob(witness(pin));return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_input(const RelationalKnownTensorSquareInput& input,field_interface_codec_detail::PacketContext& context){
    const auto pin=input;const auto actual=pin.input();const auto a=actual.accession();storage::BytePacketBuilder w(input_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_square(pin.square(),context));w.blob(context.program(a.program()));
    w.write([&](auto& raw){accession(raw,a.declaration());storage::temporal_codec_detail::tensor(raw,actual.source_field());storage::temporal_codec_detail::tensor(raw,actual.represented());});
    return std::move(w).finish(context.check);
}
} // namespace known_tensor_square_codec_detail
inline std::vector<std::byte> encode_relational_known_tensor_square(const OwnedRelationalKnownTensorSquare& square){
    field_interface_codec_detail::PacketContext context;return known_tensor_square_codec_detail::prepare_square(square,context).bytes();
}
namespace known_tensor_square_codec_detail {
inline OwnedRelationalKnownTensorSquare decode_square_image(storage::ByteRange frozen,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits,std::function<void()> check,
    std::span<const std::byte> checked_program={}) {
    if(check)check();
    Reader r(frozen, policy); header(r, frozen, square_magic); auto input = parent(r, policy, limits, check,checked_program);
    auto d = declaration(r); const auto expected = r.view_blob();
    field_program_detail::require(!r.remaining(), "trailing known tensor square bytes");
    OwnedRelationalKnownTensorSquare out(std::move(input), std::move(d), limits, check);
    canonical(expected, witness(out));field_interface_codec_detail::PacketContext packets(policy.max_file_bytes,check);
    canonical(frozen,prepare_square(out,packets),packets.check);return out;
}
} // namespace known_tensor_square_codec_detail
inline OwnedRelationalKnownTensorSquare decode_relational_known_tensor_square(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& caller_policy, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    const auto policy=caller_policy;const auto frozen=known_tensor_square_codec_detail::freeze(bytes,policy);
    return known_tensor_square_codec_detail::decode_square_image(storage::ByteRange(frozen),policy,limits,std::move(check));
}
inline std::vector<std::byte> encode_relational_known_tensor_square_input(const RelationalKnownTensorSquareInput& input) {
    field_interface_codec_detail::PacketContext context;return known_tensor_square_codec_detail::prepare_input(input,context).bytes();
}
namespace known_tensor_square_codec_detail {
inline RelationalKnownTensorSquareInput decode_input_image(storage::ByteRange frozen,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits,std::function<void()> check,
    std::optional<OwnedRelationalFieldProgram> declared_target={},std::span<const std::byte> checked_parent_program={}) {
    if (check) check();
    Reader r(frozen, policy); header(r, frozen, input_magic);
    const auto square = decode_square_image(r.view_blob(), policy, limits, check,checked_parent_program);
    const auto program_bytes = r.view_blob();
    // Explicit archive adoption into a checked parent, never live coercion.
    if (declared_target) canonical(program_bytes, encode_relational_field_program(*declared_target));
    auto program = declared_target ? *declared_target : decode_relational_field_program(program_bytes.span(), policy, limits);
    auto d = accession(r);
    const auto expected_source = storage::temporal_codec_detail::tensor(r);
    const auto expected_represented = storage::temporal_codec_detail::tensor(r);
    field_program_detail::require(!r.remaining(), "trailing known tensor input bytes");
    const auto original = square.parent(); const auto original_accession = original.accession();
    const OwnedRelationalInputAccession a(original_accession.asset(), std::move(program), std::move(d), limits, check);
    const auto out = square.admit(a, limits, check); const auto actual = out.input();
    field_program_detail::require(field_interface_detail::equal(expected_source, actual.source_field()) &&
        field_interface_detail::equal(expected_represented, actual.represented()), "known tensor input witness differs from original operands");
    field_interface_codec_detail::PacketContext packets(policy.max_file_bytes,check);
    canonical(frozen,prepare_input(out,packets),packets.check);return out;
}
} // namespace known_tensor_square_codec_detail
inline RelationalKnownTensorSquareInput decode_relational_known_tensor_square_input(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    std::optional<OwnedRelationalFieldProgram> declared_target={}) {
    const auto policy=caller_policy;const auto frozen=known_tensor_square_codec_detail::freeze(bytes,policy);
    return known_tensor_square_codec_detail::decode_input_image(storage::ByteRange(frozen),policy,limits,std::move(check),std::move(declared_target));
}
} // namespace rfc::inference
