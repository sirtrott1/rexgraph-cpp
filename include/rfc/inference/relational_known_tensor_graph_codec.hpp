#pragma once

#include "rfc/inference/relational_known_tensor_graph.hpp"
#include "rfc/inference/relational_literal_input_codec.hpp"
#include "rfc/inference/relational_known_tensor_square_codec.hpp"

namespace rfc::inference {
namespace known_tensor_graph_codec_detail {
using namespace field_interface_codec_detail;
using PacketContext = field_interface_codec_detail::PacketContext;
inline constexpr std::array<std::byte,8> graph_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'K'},std::byte{'G'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte,8> input_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'K'},std::byte{'J'},std::byte{1},std::byte{0},std::byte{0}};
inline void declaration(Writer& w,const RelationalKnownTensorGraphDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.blocks.size());
    for (const auto& b : d.blocks) { local_cell(w,b.cell); w.u64(b.source_basis); w.u64(b.lifted_basis); storage::temporal_codec_detail::keys(w,b.left); storage::temporal_codec_detail::keys(w,b.right); storage::temporal_codec_detail::keys(w,b.products); }
}
inline RelationalKnownTensorGraphDeclaration declaration(Reader& r) {
    RelationalKnownTensorGraphDeclaration d; d.key=r.u64(); d.revision=r.u64(); const auto n=r.count(46);
    for (std::size_t i=0;i<n;++i) { RelationalKnownTensorGraphBlock b; b.cell=local_cell(r); b.source_basis=r.u64(); b.lifted_basis=r.u64(); b.left=storage::temporal_codec_detail::keys(r); b.right=storage::temporal_codec_detail::keys(r); b.products=storage::temporal_codec_detail::keys(r); d.blocks.push_back(std::move(b)); }
    return d;
}
inline storage::PreparedBytePacket witness(const OwnedRelationalKnownTensorGraph& graph,PacketContext& context) {
    storage::BytePacketBuilder w({},context.max_bytes,context.byte_guard.get());
    w.write([&](auto& raw) {
        storage::temporal_codec_detail::tensor(raw,graph.lifted_field());
        for (const auto* m : {&graph.metric(),&graph.original_metric(),&graph.projection(),&graph.zero_section()}) storage::temporal_codec_detail::matrix(raw,*m);
        raw.u64(graph.declaration().blocks.size());
        for (const auto& b : graph.declaration().blocks) storage::temporal_codec_detail::tensor(raw,graph.product(b.cell));
    }); return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_graph(const OwnedRelationalKnownTensorGraph& graph,PacketContext& context) {
    const auto pin=graph; storage::BytePacketBuilder w(graph_magic,context.max_bytes,context.byte_guard.get()); const auto l=pin.limits();
    w.u64(l.dimension); w.u64(l.entries); w.u64(l.steps); w.u64(l.integer_bits); const auto literal=pin.literal_parent(); w.u8(literal?1:0);
    if (literal) w.blob(prepare_relational_literal_input(*literal,context.max_bytes,context.check));
    else { const auto input=pin.parent(); const auto a=input.accession(); w.blob(context.program(a.program())); w.blob(context.asset(a.asset()));
        w.write([&](auto& raw) { accession(raw,a.declaration()); raw.u64(input.use()); storage::temporal_codec_detail::tensor(raw,input.source_field()); }); }
    w.write([&](auto& raw) { declaration(raw,pin.declaration()); }); w.blob(witness(pin,context)); return std::move(w).finish(context.check);
}
inline storage::PreparedBytePacket prepare_input(const RelationalKnownTensorGraphInput& value,PacketContext& context) {
    const auto pin=value; const auto input=pin.input(); const auto a=input.accession(); storage::BytePacketBuilder w(input_magic,context.max_bytes,context.byte_guard.get());
    w.blob(prepare_graph(pin.graph(),context)); w.blob(context.program(a.program()));
    w.write([&](auto& raw) { accession(raw,a.declaration()); storage::temporal_codec_detail::tensor(raw,input.source_field()); storage::temporal_codec_detail::tensor(raw,input.represented()); });
    return std::move(w).finish(context.check);
}
}
inline storage::PreparedBytePacket prepare_relational_known_tensor_graph(const OwnedRelationalKnownTensorGraph& value,std::size_t maximum,std::function<void()> check={}) {
    field_interface_codec_detail::PacketContext context(maximum,std::move(check)); return known_tensor_graph_codec_detail::prepare_graph(value,context);
}
inline std::vector<std::byte> encode_relational_known_tensor_graph(const OwnedRelationalKnownTensorGraph& value,std::size_t maximum,std::function<void()> check={}) {
    return prepare_relational_known_tensor_graph(value,maximum,std::move(check)).bytes();
}
inline OwnedRelationalKnownTensorGraph decode_relational_known_tensor_graph(std::span<const std::byte> bytes,const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace known_tensor_graph_codec_detail; const auto policy=caller_policy; const auto image=known_tensor_square_codec_detail::freeze(bytes,policy);
    Reader r(image,policy); header(r,image,graph_magic); const numeric::ExactActionLimits recorded{size(r),size(r),size(r),size(r)};
    known_tensor_graph_detail::policy(recorded,limits); const auto kind=r.u8(); field_program_detail::require(kind<2,"unknown graph parent kind");
    const auto literal=kind?std::optional<OwnedRelationalLiteralInput>{decode_relational_literal_input(r.blob(),policy,limits,check)}:std::nullopt;
    const auto input=literal?literal->input():known_tensor_square_codec_detail::parent(r,policy,limits,check);
    auto d=declaration(r); const auto expected=r.view_blob(); field_program_detail::require(!r.remaining(),"trailing known tensor graph bytes");
    const auto out=literal?OwnedRelationalKnownTensorGraph(*literal,std::move(d),limits,check):OwnedRelationalKnownTensorGraph(input,std::move(d),limits,check);
    known_tensor_graph_codec_detail::PacketContext context(policy.max_file_bytes,check); canonical(expected,witness(out,context),check); canonical(storage::ByteRange(image),prepare_graph(out,context),check); return out;
}
inline storage::PreparedBytePacket prepare_relational_known_tensor_graph_input(const RelationalKnownTensorGraphInput& value,std::size_t maximum,std::function<void()> check={}) {
    field_interface_codec_detail::PacketContext context(maximum,std::move(check)); return known_tensor_graph_codec_detail::prepare_input(value,context);
}
inline std::vector<std::byte> encode_relational_known_tensor_graph_input(const RelationalKnownTensorGraphInput& value,std::size_t maximum,std::function<void()> check={}) {
    return prepare_relational_known_tensor_graph_input(value,maximum,std::move(check)).bytes();
}
inline RelationalKnownTensorGraphInput decode_relational_known_tensor_graph_input(std::span<const std::byte> bytes,const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={},std::optional<OwnedRelationalFieldProgram> declared_target={}) {
    using namespace known_tensor_graph_codec_detail; const auto policy=caller_policy; const auto image=known_tensor_square_codec_detail::freeze(bytes,policy);
    Reader r(image,policy); header(r,image,input_magic); const auto graph=decode_relational_known_tensor_graph(r.blob(),policy,limits,check);
    const auto program_bytes=r.view_blob(); if (declared_target) canonical(program_bytes,encode_relational_field_program(*declared_target));
    const auto program=declared_target?*declared_target:decode_relational_field_program(program_bytes.span(),policy,limits); auto d=accession(r);
    const auto source=storage::temporal_codec_detail::tensor(r), represented=storage::temporal_codec_detail::tensor(r);
    field_program_detail::require(!r.remaining(),"trailing known tensor graph input bytes"); const auto original=graph.parent(); const auto a=original.accession();
    const auto out=graph.admit(OwnedRelationalInputAccession(a.asset(),program,std::move(d),limits,check),limits,check); const auto actual=out.input();
    field_program_detail::require(field_interface_detail::equal(source,actual.source_field())&&field_interface_detail::equal(represented,actual.represented()),"graph input witness differs from actual operands");
    known_tensor_graph_codec_detail::PacketContext context(policy.max_file_bytes,check); canonical(storage::ByteRange(image),prepare_input(out,context),check); return out;
}
} // namespace rfc::inference
