#pragma once

#include "rfc/inference/relational_field_program.hpp"
#include "rfc/storage/prepared_byte_packet.hpp"

namespace rfc::inference {
namespace field_program_codec_detail {
using Writer = storage::temporal_codec_detail::Writer;
using Reader = storage::temporal_codec_detail::Reader;
inline constexpr std::array<std::byte, 8> program_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'F'}, std::byte{'P'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> result_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'F'}, std::byte{'R'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline Writer envelope(const std::array<std::byte, 8>& magic) {
    Writer writer; writer.raw(magic); writer.u32(1); writer.u32(0); writer.u64(0); writer.u64(0);
    return writer;
}
inline std::vector<std::byte> finish(Writer writer) {
    writer.patch_u64(16, writer.size());
    writer.patch_u64(24, storage::rfcx_detail::fnv1a64(std::span<const std::byte>(writer.bytes()).subspan(32)));
    return std::move(writer).take_bytes();
}
inline void header(Reader& reader, const storage::ByteRange& bytes, const std::array<std::byte, 8>& magic) {
    field_program_detail::require(bytes.size() >= 32, "truncated field program envelope");
    for (const auto byte : magic) field_program_detail::require(reader.u8() == std::to_integer<std::uint8_t>(byte), "field program envelope magic");
    field_program_detail::require(reader.u32() == 1 && reader.u32() == 0, "unsupported field program envelope version");
    field_program_detail::require(reader.u64() == bytes.size(), "field program envelope length");
    field_program_detail::require(reader.u64() == storage::rfcx_detail::fnv1a64(bytes.subspan(32)), "field program envelope integrity");
}
inline void header(Reader& reader, std::span<const std::byte> bytes, const std::array<std::byte, 8>& magic) {
    header(reader, storage::ByteRange(bytes), magic);
}
inline void blob(Writer& writer, std::span<const std::byte> bytes) { writer.u64(bytes.size()); writer.raw(bytes); }
inline void canonical(std::span<const std::byte> bytes, std::span<const std::byte> encoded) {
    field_program_detail::require(bytes.size() == encoded.size() && std::equal(bytes.begin(), bytes.end(), encoded.begin()),
        "noncanonical field program image");
}
inline bool same_field(const numeric::SparseRationalTensor& a, const numeric::SparseRationalTensor& b) {
    return storage::temporal_endpoint_detail::tensor_equal(a, b);
}
inline void canonical(std::span<const std::byte> bytes,const storage::PreparedBytePacket& packet,std::function<void()> check={}) {
    packet.require_equal(bytes,std::move(check));
}
inline void canonical(const storage::ByteRange& bytes, std::span<const std::byte> encoded) { bytes.require_equal(encoded); }
inline void canonical(std::span<const std::byte> bytes, const storage::ByteRange& encoded) { encoded.require_equal(bytes); }
inline void canonical(const storage::ByteRange& bytes, const storage::ByteRange& encoded) {
    field_program_detail::require(bytes.size() == encoded.size(), "noncanonical field program image");
    std::size_t offset{};
    bytes.for_each_span([&](std::span<const std::byte> part) { encoded.subspan(offset, part.size()).require_equal(part); offset += part.size(); });
}
inline void canonical(const storage::ByteRange& bytes, const storage::PreparedBytePacket& packet, std::function<void()> check = {}) {
    field_program_detail::require(bytes.size() == packet.size(), "noncanonical field program image");
    const auto pin = bytes; std::size_t offset{};
    packet.stream([&](std::span<const std::byte> part) { pin.subspan(offset, part.size()).require_equal(part); offset += part.size(); }, std::move(check));
}
} // namespace field_program_codec_detail

inline std::vector<std::byte> encode_relational_field_program(const OwnedRelationalFieldProgram& program) {
    using namespace field_program_codec_detail;
    auto writer = envelope(program_magic);
    const auto publication = program.publication(); const auto& declaration = program.declaration();
    writer.u64(publication.source_namespace());
    blob(writer, storage::encode_rfcx(publication.endpoint().source).bytes);
    blob(writer, storage::encode_temporal_endpoint(publication.endpoint()));
    writer.u64(declaration.key); writer.u64(declaration.revision); writer.u8(declaration.differential ? 1 : 0);
    writer.u64(declaration.stalks.size());
    for (const auto& stalk : declaration.stalks) {
        writer.u16(stalk.cell.grade.value); writer.u32(stalk.cell.index); writer.u64(stalk.basis);
        writer.u64(stalk.directions.size()); for (const auto direction : stalk.directions) writer.u64(direction);
        storage::temporal_codec_detail::matrix(writer, stalk.metric);
    }
    writer.u64(declaration.occurrences.size());
    for (const auto& occurrence : declaration.occurrences) {
        writer.u16(occurrence.grade.value); writer.u64(occurrence.occurrence); writer.u8(static_cast<std::uint8_t>(occurrence.role));
        storage::temporal_codec_detail::matrix(writer, occurrence.map);
    }
    writer.u64(declaration.nodes.size());
    for (const auto& node : declaration.nodes) {
        writer.u64(node.key); writer.u8(static_cast<std::uint8_t>(node.kind)); writer.u16(node.output_grade.value);
        writer.u8(node.input ? 1 : 0); if (node.input) writer.u64(*node.input);
        storage::temporal_codec_detail::matrix(writer, node.map);
    }
    return finish(std::move(writer));
}

inline OwnedRelationalFieldProgram decode_relational_field_program(std::span<const std::byte> bytes,
        const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace field_program_codec_detail;
    Reader reader(bytes, policy); header(reader, bytes, program_magic);
    const auto name_space = reader.u64(); const auto source_bytes = reader.blob(), endpoint_bytes = reader.blob();
    storage::MappedRfcx source(source_bytes);
    auto endpoint = storage::decode_temporal_endpoint(source.materialize(), endpoint_bytes, policy);
    storage::NativeSeriesLimits publication_policy; publication_policy.temporal = policy;
    publication_policy.max_archive_bytes = policy.max_file_bytes;
    const storage::NativeTemporalSeries publication(endpoint, name_space, publication_policy);
    RelationalFieldProgramDeclaration declaration;
    declaration.key = reader.u64(); declaration.revision = reader.u64(); declaration.differential = reader.flag();
    const auto stalk_count = reader.count(39);
    for (std::size_t i = 0; i < stalk_count; ++i) {
        StalkBasisDeclaration stalk; stalk.cell.grade = {reader.u16()}; stalk.cell.index = reader.u32(); stalk.basis = reader.u64();
        const auto direction_count = reader.count(8);
        for (std::size_t j = 0; j < direction_count; ++j) stalk.directions.push_back(reader.u64());
        stalk.metric = storage::temporal_codec_detail::matrix(reader); declaration.stalks.push_back(std::move(stalk));
    }
    const auto occurrence_count = reader.count(35);
    for (std::size_t i = 0; i < occurrence_count; ++i) {
        OccurrenceMapDeclaration occurrence; occurrence.grade = {reader.u16()};
        const auto slot = reader.u64();
        field_program_detail::require(slot <= std::numeric_limits<std::size_t>::max(), "field program occurrence index overflow");
        occurrence.occurrence = static_cast<std::size_t>(slot); occurrence.role = static_cast<storage::TemporalTensorRole>(reader.u8());
        occurrence.map = storage::temporal_codec_detail::matrix(reader); declaration.occurrences.push_back(std::move(occurrence));
    }
    const auto node_count = reader.count(36);
    for (std::size_t i = 0; i < node_count; ++i) {
        FieldProgramNode node; node.key = reader.u64(); node.kind = static_cast<FieldProgramNodeKind>(reader.u8());
        node.output_grade = {reader.u16()}; if (reader.flag()) node.input = reader.u64();
        node.map = storage::temporal_codec_detail::matrix(reader); declaration.nodes.push_back(std::move(node));
    }
    field_program_detail::require(reader.remaining() == 0, "trailing field program bytes");
    OwnedRelationalFieldProgram program(publication.tail(), std::move(declaration), limits);
    canonical(bytes, encode_relational_field_program(program)); return program;
}

namespace field_program_codec_detail {
// A preparation-local cache pins actual immutable owners. Equal bytes or
// dimensions cannot substitute an owner, and no availability result is cached.
struct PacketContext {
    explicit PacketContext(std::size_t maximum=std::numeric_limits<std::size_t>::max(),std::function<void()> callback={}) :max_bytes(maximum) {
        if(callback){const auto held=std::make_shared<std::function<void()>>(std::move(callback));check=[held]{(*held)();};}
    }
    explicit PacketContext(storage::SharedPacketLimits limits,std::function<void()> callback={}) :PacketContext(limits.max_expanded_bytes,std::move(callback)) {
        byte_guard=std::make_shared<storage::shared_packet_detail::Guard>(limits,check);
    }
    storage::PreparedBytePacket program(const OwnedRelationalFieldProgram& owner){
        for(const auto& [actual,packet]:programs){if(actual.same_owner(owner)){return packet;}}
        if(check){check();}const auto encoded=encode_relational_field_program(owner);
        value_bound(encoded.size());auto packet=storage::PreparedBytePacket::freeze(encoded);if(byte_guard){byte_guard->work(encoded.size());}programs.emplace_back(owner,packet);return packet;
    }
    void value_bound(std::size_t size) const {storage::shared_packet_detail::bound(size,max_bytes,"prepared native packet byte admission");}
    std::size_t max_bytes;
    std::function<void()> check;
    std::shared_ptr<storage::shared_packet_detail::Guard> byte_guard;
    std::vector<std::pair<OwnedRelationalFieldProgram,storage::PreparedBytePacket>> programs;
};
inline storage::PreparedBytePacket prepare_result(const OwnedRelationalFieldResult& result,PacketContext& context){
    using namespace field_program_codec_detail;
    const auto pin=result;const auto program=pin.program();storage::BytePacketBuilder writer(result_magic,context.max_bytes,context.byte_guard.get());
    writer.blob(context.program(program));writer.u64(pin.inputs().size());
    for (const auto& input : pin.inputs()) {
        writer.write([&](auto& w){w.u64(input.node);w.u64(input.use);storage::temporal_codec_detail::tensor(w,input.field);});
    }
    writer.u64(program.declaration().nodes.size());
    for (const auto& node : program.declaration().nodes) {
        writer.write([&](auto& w){w.u64(node.key);storage::temporal_codec_detail::tensor(w,pin.field(node.key));
            if(node.kind==FieldProgramNodeKind::Boundary){storage::temporal_codec_detail::tensor(w,pin.occurrence_field(node.key));}});
    }
    return std::move(writer).finish(context.check);
}
} // namespace field_program_codec_detail
inline std::vector<std::byte> encode_relational_field_result(const OwnedRelationalFieldResult& result){
    field_program_codec_detail::PacketContext context;return field_program_codec_detail::prepare_result(result,context).bytes();
}

inline OwnedRelationalFieldResult decode_relational_field_result(std::span<const std::byte> bytes,
        const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace field_program_codec_detail;
    Reader reader(bytes, policy); header(reader, bytes, result_magic);
    auto program = decode_relational_field_program(reader.blob(), policy, limits);
    std::vector<RelationalFieldInput> inputs;
    const auto input_count = reader.count(33);
    for (std::size_t i = 0; i < input_count; ++i) {
        RelationalFieldInput input; input.node = reader.u64(); input.use = reader.u64();
        input.field = storage::temporal_codec_detail::tensor(reader); inputs.push_back(std::move(input));
    }
    auto result = program.execute(std::move(inputs), limits);
    const auto node_count = reader.count(25);
    field_program_detail::require(node_count == program.declaration().nodes.size(), "field result omits complete intermediates");
    for (const auto& node : program.declaration().nodes) {
        field_program_detail::require(reader.u64() == node.key, "field result changes operation order");
        auto field = storage::temporal_codec_detail::tensor(reader);
        field_program_detail::require(same_field(field, result.field(node.key)), "field result complete replay mismatch");
        if (node.kind == FieldProgramNodeKind::Boundary) {
            auto occurrences = storage::temporal_codec_detail::tensor(reader);
            field_program_detail::require(same_field(occurrences, result.occurrence_field(node.key)), "field result original occurrence replay mismatch");
        }
    }
    field_program_detail::require(reader.remaining() == 0, "trailing field result bytes");
    canonical(bytes, encode_relational_field_result(result)); return result;
}
} // namespace rfc::inference
