#pragma once

#include "rfc/learning/relational_literal_finite_field.hpp"
#include "rfc/learning/relational_finite_field_codec.hpp"
#include "rfc/inference/relational_literal_input_codec.hpp"

namespace rfc::learning {
namespace literal_finite_field_codec_detail {
inline constexpr std::array<std::byte, 8> map_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'L'}, std::byte{'M'},
    std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> result_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'L'}, std::byte{'B'},
    std::byte{1}, std::byte{0}, std::byte{0}};
} // namespace literal_finite_field_codec_detail

inline storage::PreparedBytePacket prepare_relational_literal_finite_field_map(
    const PreparedRelationalLiteralFiniteFieldMap& map, std::size_t max_bytes, std::function<void()> check = {}) {
    using namespace relational_finite_field_codec_detail;
    const auto pin = map; storage::BytePacketBuilder writer(literal_finite_field_codec_detail::map_magic, max_bytes);
    writer.blob(inference::encode_relational_finite_source(pin.source()));
    const auto actual = pin.map(); writer.u64(actual.targets().size());
    for (const auto& t : actual.targets()) {
        if (check) check();
        writer.u8(static_cast<std::uint8_t>(t.index())); writer.blob(target(t));
    }
    const auto& d = pin.declaration(); writer.u64(d.key); writer.u64(d.revision); writer.u64(d.rules.size());
    for (const auto& r : d.rules) {
        if (check) check();
        writer.write([&](auto& w) {
            w.u64(r.key); finite_keys(w, r.bindings); finite_keys(w, r.symbols);
            w.u8(r.target ? 1 : 0); if (r.target) w.u64(*r.target);
            storage::temporal_codec_detail::string(w, r.unsupported); w.u64(r.inputs.size());
        });
        for (const auto& input : r.inputs) writer.blob(inference::prepare_relational_literal_input(input, max_bytes, check));
    }
    return std::move(writer).finish(std::move(check));
}
inline std::vector<std::byte> encode_relational_literal_finite_field_map(
    const PreparedRelationalLiteralFiniteFieldMap& map, std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_relational_literal_finite_field_map(map, max_bytes, check).bytes(check);
}
inline PreparedRelationalLiteralFiniteFieldMap decode_relational_literal_finite_field_map(
    std::span<const std::byte> bytes, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace relational_finite_field_codec_detail;
    const auto read_policy = policy; const auto image = frozen_bytes(bytes, read_policy); bytes = image;
    Reader reader(bytes, read_policy); header(reader, bytes, literal_finite_field_codec_detail::map_magic);
    auto source = inference::decode_relational_finite_source(reader.blob(), read_policy, limits, check);
    std::vector<RelationalFiniteFieldTarget> targets; numeric::ExactActionGuard guard(limits, check);
    const auto count = reader.count(9); guard.dimension(count); guard.entries(count);
    for (std::size_t i = 0; i < count; ++i) {
        guard.step(); const auto kind = reader.u8(); const auto data = reader.blob();
        inference::field_program_detail::require(kind < 2, "unknown literal finite target discriminant");
        if (!kind) targets.push_back(inference::decode_relational_field_program(data, read_policy, limits));
        else targets.push_back(decode_relational_field_evidence(data, read_policy, limits));
    }
    RelationalLiteralFiniteFieldDeclaration d; d.key = reader.u64(); d.revision = reader.u64();
    const auto rules = reader.count(41); guard.dimension(rules); std::size_t retained = rules; guard.entries(retained);
    const auto source_bytes = inference::encode_relational_input_use_asset(source.asset());
    for (std::size_t i = 0; i < rules; ++i) {
        guard.step(); RelationalLiteralFiniteFieldRule r;
        r.key = reader.u64(); r.bindings = finite_keys(reader); r.symbols = finite_keys(reader);
        if (reader.flag()) r.target = size(reader);
        r.unsupported = reader.string(); const auto inputs = reader.count(8);
        retained = machine::checked_size_add(retained, machine::checked_size_add(inputs,
            machine::checked_size_add(r.bindings.size(), r.symbols.size()))); guard.entries(retained);
        inference::field_program_detail::require(!r.target || *r.target < targets.size(), "serialized literal finite target index");
        inference::field_program_detail::require(r.target || !inputs, "unsupported literal finite rule has input");
        for (std::size_t j = 0; j < inputs; ++j) {
            guard.step(); const auto input = inference::decode_relational_literal_input(reader.blob(), read_policy, limits, check);
            const auto old = input.input().accession();
            const auto program = relational_finite_field_detail::program(targets[*r.target]);
            inference::field_program_detail::require(source_bytes == inference::encode_relational_input_use_asset(old.asset()),
                "serialized literal finite input has a different complete byte owner");
            inference::field_program_detail::require(inference::encode_relational_field_program(program) ==
                inference::encode_relational_field_program(old.program()), "serialized literal finite input has a different complete program");
            inference::OwnedRelationalInputAccession accession(source.asset(), program, old.declaration(), limits, check);
            r.inputs.emplace_back(std::move(accession), input.declaration(), limits, check);
        }
        d.rules.push_back(std::move(r));
    }
    inference::field_program_detail::require(!reader.remaining(), "trailing literal finite map bytes");
    PreparedRelationalLiteralFiniteFieldMap out(std::move(source), std::move(targets), std::move(d), limits, check);
    canonical(bytes, prepare_relational_literal_finite_field_map(out, read_policy.max_file_bytes, check)); return out;
}

inline storage::PreparedBytePacket prepare_relational_literal_finite_field_result(
    const RelationalLiteralFiniteFieldResult& result, std::size_t max_bytes, std::function<void()> check = {}) {
    const auto pin = result; storage::BytePacketBuilder writer(literal_finite_field_codec_detail::result_magic, max_bytes);
    writer.blob(prepare_relational_literal_finite_field_map(pin.map(), max_bytes, check));
    writer.blob(relational_finite_field_codec_detail::witness(pin.family()));
    return std::move(writer).finish(std::move(check));
}
inline std::vector<std::byte> encode_relational_literal_finite_field_result(
    const RelationalLiteralFiniteFieldResult& result, std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_relational_literal_finite_field_result(result, max_bytes, check).bytes(check);
}
inline RelationalLiteralFiniteFieldResult decode_relational_literal_finite_field_result(
    std::span<const std::byte> bytes, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace relational_finite_field_codec_detail;
    const auto read_policy = policy; const auto image = frozen_bytes(bytes, read_policy); bytes = image;
    Reader reader(bytes, read_policy); header(reader, bytes, literal_finite_field_codec_detail::result_magic);
    const auto map_bytes = reader.blob(); const auto expected = reader.blob();
    inference::field_program_detail::require(!reader.remaining(), "trailing literal finite result bytes");
    const auto map = decode_relational_literal_finite_field_map(map_bytes, read_policy, limits, check);
    auto out = map.execute(limits, check); canonical(expected, witness(out.family()));
    canonical(bytes, prepare_relational_literal_finite_field_result(out, read_policy.max_file_bytes, check)); return out;
}
} // namespace rfc::learning
