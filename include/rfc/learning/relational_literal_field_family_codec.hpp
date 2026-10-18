#pragma once

#include "rfc/learning/relational_literal_field_family.hpp"
#include "rfc/learning/relational_field_family_codec.hpp"
#include "rfc/inference/relational_known_tensor_graph_codec.hpp"

namespace rfc::learning {
namespace literal_family_codec_detail {
using namespace relational_family_codec_detail;
using PacketContext = relational_affine_codec_detail::PacketContext;
inline constexpr std::array<std::byte,8> result_magic{
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'L'},std::byte{'J'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte,8> evaluation_magic{
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'L'},std::byte{'E'},std::byte{1},std::byte{0},std::byte{0}};
inline storage::PreparedBytePacket prepare_result(const RelationalLiteralJointResult& result,PacketContext& context) {
    const auto pin=result; storage::BytePacketBuilder w(result_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_family(pin.family(),context)); w.u64(pin.member()); w.u64(pin.inputs().size());
    for (const auto& input : pin.inputs()) {
        if (context.check) context.check();
        std::visit([&](const auto& value) {
            using Value=std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value,inference::OwnedRelationalLiteralInput>) {
                w.u8(0); w.blob(inference::prepare_relational_literal_input(value,context.max_bytes,context.check));
            } else { w.u8(1); w.blob(inference::known_tensor_graph_codec_detail::prepare_input(value,context)); }
        },input);
    }
    const auto family=pin.family(); const auto program=family.members().at(pin.member()).program();
    w.u64(program.declaration().nodes.size());
    for (const auto& n : program.declaration().nodes) {
        if (context.check) context.check();
        w.write([&](auto& raw) { raw.u64(n.key); field(raw,pin.field(n.key));
            if (n.kind==inference::FieldProgramNodeKind::Boundary) field(raw,pin.occurrence_field(n.key)); });
    }
    return std::move(w).finish(context.check);
}
inline inference::OwnedRelationalLiteralInput adopt_literal(std::span<const std::byte> bytes,
    const inference::OwnedRelationalFieldProgram& program,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    // Cold adoption uses the serialized declaration and exact program bytes.
    // Live inputs must already have the actual family member's owner.
    const auto parsed=inference::literal_input_codec_detail::passive(bytes,policy,limits,check);
    canonical(parsed.program,inference::encode_relational_field_program(program));
    const auto asset=inference::decode_relational_input_use_asset(parsed.asset,policy,limits);
    const inference::OwnedRelationalInputAccession a(asset,program,parsed.accession,limits,check);
    const inference::OwnedRelationalLiteralInput out(a,parsed.declaration,limits,check);
    canonical(bytes,inference::prepare_relational_literal_input(out,policy.max_file_bytes,check),check); return out;
}
inline storage::PreparedBytePacket prepare_evaluation(const RelationalLiteralJointEvaluation& value,PacketContext& context) {
    const auto pin=value; storage::BytePacketBuilder w(evaluation_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_result(pin.parent(),context)); w.write([&](auto& raw){vector(raw,pin.free_values());});
    w.blob(inference::field_interface_codec_detail::prepare_accession(pin.result(),context)); return std::move(w).finish(context.check);
}
}
inline storage::PreparedBytePacket prepare_relational_literal_joint_result(const RelationalLiteralJointResult& value,
    std::size_t maximum,std::function<void()> check={}) {
    literal_family_codec_detail::PacketContext context(maximum,std::move(check)); return literal_family_codec_detail::prepare_result(value,context);
}
inline std::vector<std::byte> encode_relational_literal_joint_result(const RelationalLiteralJointResult& value,
    std::size_t maximum,std::function<void()> check={}) {
    return prepare_relational_literal_joint_result(value,maximum,std::move(check)).bytes();
}
inline RelationalLiteralJointResult decode_relational_literal_joint_result(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace literal_family_codec_detail; const auto policy=caller_policy;
    const auto image=inference::known_tensor_square_codec_detail::freeze(bytes,policy); Reader r(image,policy); header(r,image,result_magic);
    const auto family=decode_relational_field_family(r.blob(),policy,limits,check);
    relational_evidence_detail::same_limits(family.limits(),limits); const auto member=size(r); const auto program=family.members().at(member).program();
    const auto count=r.count(9); numeric::ExactActionGuard guard(limits,check); guard.dimension(count);
    std::vector<RelationalLiteralFamilyInput> inputs;
    for (std::size_t i=0;i<count;++i) {
        guard.step(); const auto kind=r.u8(); const auto packet=r.blob();
        inference::field_program_detail::require(kind<2,"unknown literal joint input kind");
        if (kind==0) inputs.emplace_back(adopt_literal(packet,program,policy,limits,[&]{guard.step();}));
        else inputs.emplace_back(inference::decode_relational_known_tensor_graph_input(packet,policy,limits,[&]{guard.step();},program));
    }
    const RelationalLiteralJointResult out(family,member,std::move(inputs),limits,[&]{guard.step();});
    inference::field_program_detail::require(r.count(32)==program.declaration().nodes.size(),"literal joint complete node count");
    for (const auto& n : program.declaration().nodes) {
        guard.step(); inference::field_program_detail::require(r.u64()==n.key,"literal joint node order"); field(r,out.field(n.key));
        if (n.kind==inference::FieldProgramNodeKind::Boundary) field(r,out.occurrence_field(n.key));
    }
    inference::field_program_detail::require(!r.remaining(),"trailing literal joint bytes");
    canonical(storage::ByteRange(image),prepare_relational_literal_joint_result(out,policy.max_file_bytes,check),check); return out;
}
inline storage::PreparedBytePacket prepare_relational_literal_joint_evaluation(const RelationalLiteralJointEvaluation& value,
    std::size_t maximum,std::function<void()> check={}) {
    literal_family_codec_detail::PacketContext context(maximum,std::move(check)); return literal_family_codec_detail::prepare_evaluation(value,context);
}
inline std::vector<std::byte> encode_relational_literal_joint_evaluation(const RelationalLiteralJointEvaluation& value,
    std::size_t maximum,std::function<void()> check={}) {
    return prepare_relational_literal_joint_evaluation(value,maximum,std::move(check)).bytes();
}
inline RelationalLiteralJointEvaluation decode_relational_literal_joint_evaluation(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace literal_family_codec_detail; const auto policy=caller_policy;
    const auto image=inference::known_tensor_square_codec_detail::freeze(bytes,policy); Reader r(image,policy); header(r,image,literal_family_codec_detail::evaluation_magic);
    const auto parent=decode_relational_literal_joint_result(r.blob(),policy,limits,check); const auto free=vector(r);
    const auto out=parent.evaluate(free,limits,check); const auto witness=r.view_blob();
    PacketContext context(policy.max_file_bytes,check); canonical(witness,inference::field_interface_codec_detail::prepare_accession(out.result(),context),check);
    inference::field_program_detail::require(!r.remaining(),"trailing literal joint evaluation bytes");
    canonical(storage::ByteRange(image),prepare_evaluation(out,context),check); return out;
}
} // namespace rfc::learning
