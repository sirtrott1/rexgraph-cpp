#pragma once

#include "rfc/inference/relational_literal_input.hpp"
#include "rfc/inference/relational_field_interface_codec.hpp"

namespace rfc::inference {
namespace literal_input_codec_detail {
inline constexpr std::array<std::byte, 8> magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'L'}, std::byte{'I'},
    std::byte{1}, std::byte{0}, std::byte{0}};
}

inline storage::PreparedBytePacket prepare_relational_literal_input(
    const OwnedRelationalLiteralInput& input, std::size_t max_bytes, std::function<void()> check = {}) {
    using namespace field_interface_codec_detail;
    const auto pin = input; const auto accession_owner = pin.input().accession();
    field_interface_codec_detail::PacketContext context(max_bytes, std::move(check));
    storage::BytePacketBuilder writer(literal_input_codec_detail::magic, max_bytes);
    writer.blob(context.program(accession_owner.program()));
    writer.blob(context.asset(accession_owner.asset()));
    writer.write([&](auto& w) { accession(w, accession_owner.declaration()); });
    const auto& d = pin.declaration(); writer.u64(d.key); writer.u64(d.revision); writer.u64(d.use);
    writer.u64(d.coordinates.size());
    for (const auto& c : d.coordinates) {
        if (context.check) context.check();
        writer.write([&](auto& w) {
            w.u64(c.key); local_cell(w, c.cell); w.u64(c.direction); w.u64(c.use);
            w.u64(c.span_index); w.u64(c.span.begin); w.u64(c.span.end);
            w.u8(static_cast<std::uint8_t>(c.kind)); blob(w, c.operand);
        });
    }
    return std::move(writer).finish(context.check);
}

inline std::vector<std::byte> encode_relational_literal_input(
    const OwnedRelationalLiteralInput& input, std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_relational_literal_input(input, max_bytes, std::move(check)).bytes();
}

namespace literal_input_codec_detail {
struct PassiveInput {
    std::vector<std::byte> program, asset;
    RelationalInputAccessionDeclaration accession;
    RelationalLiteralInputDeclaration declaration;
};
// Parse declared bytes only. Availability adapters can admit the complete
// recipe before constructing its actual program, accession or literal field.
inline PassiveInput passive(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits,
    const std::function<void()>& check) {
    using namespace field_interface_codec_detail;
    Reader reader(bytes, policy); header(reader, bytes, magic);
    const auto program = reader.blob(), asset = reader.blob();
    auto a = accession(reader);
    RelationalLiteralInputDeclaration d; d.key = reader.u64(); d.revision = reader.u64(); d.use = reader.u64();
    const auto count = reader.count(63);
    numeric::ExactActionGuard guard(limits, check); guard.dimension(count);
    auto retained = machine::checked_size_add(machine::checked_size_mul(count, 5), 3);
    guard.entries(retained);
    for (std::size_t i = 0; i < count; ++i) {
        guard.step();
        RelationalLiteralCoordinate c; c.key = reader.u64(); c.cell = local_cell(reader);
        c.direction = reader.u64(); c.use = reader.u64(); c.span_index = size(reader);
        c.span.begin = size(reader); c.span.end = size(reader);
        c.kind = static_cast<RelationalLiteralPredicateKind>(reader.u8());
        const auto operand = reader.blob(); retained = machine::checked_size_add(retained, operand.size());
        guard.entries(retained); c.operand.assign(operand.begin(), operand.end());
        d.coordinates.push_back(std::move(c));
    }
    field_program_detail::require(reader.remaining() == 0, "trailing literal input bytes");
    return {{program.begin(), program.end()}, {asset.begin(), asset.end()}, std::move(a), std::move(d)};
}
} // namespace literal_input_codec_detail

inline OwnedRelationalLiteralInput decode_relational_literal_input(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {},
    std::function<void()> check = {}) {
    using namespace field_interface_codec_detail;
    const auto read_policy = policy;
    if (bytes.size() > read_policy.max_file_bytes) throw std::length_error("literal input file byte admission");
    const std::vector<std::byte> image(bytes.begin(), bytes.end()); bytes = image;
    const auto parsed = literal_input_codec_detail::passive(bytes, read_policy, limits, check);
    auto program = decode_relational_field_program(parsed.program, read_policy, limits);
    auto asset = decode_relational_input_use_asset(parsed.asset, read_policy, limits);
    OwnedRelationalInputAccession owner(std::move(asset), std::move(program), parsed.accession, limits, check);
    OwnedRelationalLiteralInput out(std::move(owner), parsed.declaration, limits, check);
    canonical(bytes, prepare_relational_literal_input(out, read_policy.max_file_bytes, std::move(check)));
    return out;
}

} // namespace rfc::inference
