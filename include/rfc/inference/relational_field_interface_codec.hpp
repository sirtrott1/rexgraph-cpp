#pragma once

#include "rfc/inference/relational_field_interface.hpp"
#include "rfc/inference/relational_field_program_codec.hpp"
#include "rfc/storage/native_series_codec.hpp"

namespace rfc::inference {
namespace field_interface_codec_detail {
using namespace field_program_codec_detail;
inline constexpr std::array<std::byte, 8> asset_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'U'}, std::byte{'A'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> accession_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'A'}, std::byte{'R'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> section_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'F'}, std::byte{'S'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline std::size_t size(Reader& r) {
    const auto n = r.u64(); field_program_detail::require(n <= std::numeric_limits<std::size_t>::max(), "interface index exceeds size domain");
    return static_cast<std::size_t>(n);
}
inline void local_cell(Writer& w, core::CellId c) { w.u16(c.grade.value); w.u32(c.index); }
inline core::CellId local_cell(Reader& r) { const auto g = r.u16(); return {{g}, r.u32()}; }
inline void stalks(Writer& w, const std::vector<StalkBasisDeclaration>& values) {
    w.u64(values.size());
    for (const auto& s : values) {
        local_cell(w, s.cell); w.u64(s.basis); w.u64(s.directions.size());
        for (const auto key : s.directions) w.u64(key);
        storage::temporal_codec_detail::matrix(w, s.metric);
    }
}
inline std::vector<StalkBasisDeclaration> stalks(Reader& r) {
    std::vector<StalkBasisDeclaration> out; const auto n = r.count(38);
    for (std::size_t i = 0; i < n; ++i) {
        StalkBasisDeclaration s; s.cell = local_cell(r); s.basis = r.u64(); const auto count = r.count(8);
        for (std::size_t j = 0; j < count; ++j) s.directions.push_back(r.u64());
        s.metric = storage::temporal_codec_detail::matrix(r); out.push_back(std::move(s));
    }
    return out;
}
inline void source_map(Writer& w, const algebra::RetainedSourceMap& map) {
    w.u64(map.cells.size()); for (const auto& row : map.cells) { w.u64(row.size()); for (const auto i : row) w.u32(i); }
    w.u64(map.occurrences.size()); for (const auto& row : map.occurrences) { w.u64(row.size()); for (const auto i : row) w.u64(i); }
}
inline algebra::RetainedSourceMap source_map(Reader& r) {
    algebra::RetainedSourceMap out; const auto cells = r.count(8);
    for (std::size_t k = 0; k < cells; ++k) {
        std::vector<std::uint32_t> row; const auto n = r.count(4);
        for (std::size_t i = 0; i < n; ++i) row.push_back(r.u32());
        out.cells.push_back(std::move(row));
    }
    const auto occurrences = r.count(8);
    for (std::size_t k = 0; k < occurrences; ++k) {
        std::vector<std::size_t> row; const auto n = r.count(8);
        for (std::size_t i = 0; i < n; ++i) row.push_back(size(r));
        out.occurrences.push_back(std::move(row));
    }
    return out;
}
inline void accession(Writer& w, const RelationalInputAccessionDeclaration& d) {
    w.u64(d.key); w.u64(d.node); w.u16(d.source_grade.value); w.u8(d.isometric ? 1 : 0);
    stalks(w, d.source_stalks); source_map(w, d.instances);
    storage::temporal_codec_detail::matrix(w, d.accession); storage::temporal_codec_detail::matrix(w, d.reconstruction);
}
inline RelationalInputAccessionDeclaration accession(Reader& r) {
    RelationalInputAccessionDeclaration d; d.key = r.u64(); d.node = r.u64(); d.source_grade = {r.u16()}; d.isometric = r.flag();
    d.source_stalks = stalks(r); d.instances = source_map(r);
    d.accession = storage::temporal_codec_detail::matrix(r); d.reconstruction = storage::temporal_codec_detail::matrix(r); return d;
}
inline void view(Writer& w, const RelationalFieldViewDeclaration& d) {
    w.u64(d.key); w.u64(d.basis); w.u64(d.directions.size()); for (const auto key : d.directions) w.u64(key);
    storage::temporal_codec_detail::matrix(w, d.accession); storage::temporal_codec_detail::matrix(w, d.reconstruction);
    w.u8(d.physical ? 1 : 0); if (d.physical) storage::temporal_codec_detail::matrix(w, *d.physical);
}
inline RelationalFieldViewDeclaration view(Reader& r) {
    RelationalFieldViewDeclaration d; d.key = r.u64(); d.basis = r.u64(); const auto n = r.count(8);
    for (std::size_t i = 0; i < n; ++i) d.directions.push_back(r.u64());
    d.accession = storage::temporal_codec_detail::matrix(r); d.reconstruction = storage::temporal_codec_detail::matrix(r);
    if (r.flag()) d.physical = storage::temporal_codec_detail::matrix(r);
    return d;
}
} // namespace field_interface_codec_detail

inline std::vector<std::byte> encode_relational_input_use_asset(const OwnedRelationalInputUseAsset& asset) {
    using namespace field_interface_codec_detail;
    const auto pin = asset; auto w = envelope(asset_magic); blob(w, storage::encode_native_series(pin.history()));
    const auto& d = pin.declaration(); w.u64(d.key); w.u64(d.channel);
    storage::temporal_codec_detail::string(w, d.encoding); blob(w, d.bytes); w.u64(d.uses.size());
    for (const auto& use : d.uses) {
        w.u64(use.key); w.u64(use.cells.size());
        for (const auto c : use.cells) { w.u16(c.grade.value); w.u64(c.key); }
        w.u64(use.occurrences.size()); for (const auto p : use.occurrences) { w.u16(p.upper_grade.value); w.u64(p.key.value); }
        w.u64(use.spans.size()); for (const auto span : use.spans) { w.u64(span.begin); w.u64(span.end); }
    }
    return finish(std::move(w));
}
inline OwnedRelationalInputUseAsset decode_relational_input_use_asset(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace field_interface_codec_detail;
    Reader r(bytes, policy); header(r, bytes, asset_magic); storage::NativeSeriesLimits history_policy;
    history_policy.temporal = policy; history_policy.max_archive_bytes = policy.max_file_bytes;
    auto history = storage::decode_native_series(r.blob(), history_policy);
    RelationalInputUseDeclaration d; d.key = r.u64(); d.channel = r.u64(); d.encoding = r.string();
    const auto raw = r.blob(); d.bytes.assign(raw.begin(), raw.end()); const auto n = r.count(32);
    for (std::size_t i = 0; i < n; ++i) {
        RelationalSpanUse use; use.key = r.u64(); const auto cells = r.count(10);
        for (std::size_t j = 0; j < cells; ++j) { const auto g = r.u16(); use.cells.push_back({{g}, r.u64()}); }
        const auto occurrences = r.count(10);
        for (std::size_t j = 0; j < occurrences; ++j) { const auto g = r.u16(); use.occurrences.push_back({{g}, {r.u64()}}); }
        const auto spans = r.count(16);
        for (std::size_t j = 0; j < spans; ++j) { const auto begin = size(r); use.spans.push_back({begin, size(r)}); }
        d.uses.push_back(std::move(use));
    }
    field_program_detail::require(r.remaining() == 0, "trailing input asset bytes");
    OwnedRelationalInputUseAsset out(std::move(history), std::move(d), limits); canonical(bytes, encode_relational_input_use_asset(out)); return out;
}
namespace field_interface_codec_detail {
struct PacketContext:field_program_codec_detail::PacketContext {
    using field_program_codec_detail::PacketContext::PacketContext;
    storage::PreparedBytePacket asset(const OwnedRelationalInputUseAsset& owner){
        for(const auto& [actual,packet]:assets){if(actual.same_owner(owner)){return packet;}}
        if(check){check();}const auto encoded=encode_relational_input_use_asset(owner);value_bound(encoded.size());
        auto packet=storage::PreparedBytePacket::freeze(encoded);if(byte_guard){byte_guard->work(encoded.size());}assets.emplace_back(owner,packet);return packet;
    }
    std::vector<std::pair<OwnedRelationalInputUseAsset,storage::PreparedBytePacket>> assets;
    std::vector<std::pair<OwnedAccessionFieldResult,storage::PreparedBytePacket>> accessions;
};
inline storage::PreparedBytePacket prepare_accession(const OwnedAccessionFieldResult& result,PacketContext& context) {
    using namespace field_interface_codec_detail;
    for(const auto& [actual,packet]:context.accessions){if(actual.same_owner(result)){return packet;}}
    const auto pin=result;storage::BytePacketBuilder w(accession_magic,context.max_bytes,context.byte_guard.get());
    w.blob(field_program_codec_detail::prepare_result(pin.result(),context));
    w.u64(pin.inputs().size());
    for (const auto& input : pin.inputs()) {
        const auto a=input.accession();w.blob(context.asset(a.asset()));
        w.write([&](auto& raw){accession(raw,a.declaration());raw.u64(input.use());storage::temporal_codec_detail::tensor(raw,input.source_field());});
    }
    auto packet=std::move(w).finish(context.check);context.accessions.emplace_back(pin,packet);return packet;
}
} // namespace field_interface_codec_detail
inline std::vector<std::byte> encode_accession_field_result(const OwnedAccessionFieldResult& result){
    field_interface_codec_detail::PacketContext context;return field_interface_codec_detail::prepare_accession(result,context).bytes();
}
inline OwnedAccessionFieldResult decode_accession_field_result(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace field_interface_codec_detail;
    Reader r(bytes, policy); header(r, bytes, accession_magic);
    const auto expected = decode_relational_field_result(r.blob(), policy, limits); const auto program = expected.program();
    std::vector<RelationalAccessionInput> inputs; const auto n = r.count(70);
    for (std::size_t i = 0; i < n; ++i) {
        auto asset = decode_relational_input_use_asset(r.blob(), policy, limits); auto d = accession(r);
        const OwnedRelationalInputAccession a(std::move(asset), program, std::move(d), limits);
        const auto use = r.u64(); auto field = storage::temporal_codec_detail::tensor(r); inputs.push_back(a.admit(use, std::move(field), limits));
    }
    field_program_detail::require(r.remaining() == 0, "trailing accession result bytes");
    auto result = execute_relational_inputs(program, std::move(inputs), limits);
    field_program_detail::require(encode_relational_field_result(expected) == encode_relational_field_result(result.result()), "accession source fields disagree with complete native result replay");
    canonical(bytes, encode_accession_field_result(result)); return result;
}
inline std::vector<std::byte> encode_relational_field_section(const OwnedRelationalFieldSection& section) {
    using namespace field_interface_codec_detail;
    const auto frozen = section; const auto interface = frozen.readings().front().interface(); auto w = envelope(section_magic);
    blob(w, encode_accession_field_result(interface.parent())); const auto& d = interface.declaration();
    w.u64(d.key); w.u64(d.branch); w.u64(d.cells.size());
    for (const auto& binding : d.cells) { w.u64(binding.node); local_cell(w, binding.cell); }
    w.u8(static_cast<std::uint8_t>(frozen.kind())); w.u64(frozen.readings().size());
    for (const auto& reading : frozen.readings()) {
        view(w, reading.declaration()); w.u8(static_cast<std::uint8_t>(reading.status()));
        storage::temporal_codec_detail::tensor(w, reading.visible());
    }
    w.u64(frozen.section().values().size());
    for (const auto& [c, values] : frozen.section().values()) {
        local_cell(w, c); w.u64(values.size()); for (const auto& q : values) storage::temporal_codec_detail::rational(w, q);
    }
    return finish(std::move(w));
}
inline OwnedRelationalFieldSection decode_relational_field_section(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace field_interface_codec_detail;
    Reader r(bytes, policy); header(r, bytes, section_magic); auto parent = decode_accession_field_result(r.blob(), policy, limits);
    RelationalFieldInterfaceDeclaration d; d.key = r.u64(); d.branch = r.u64(); const auto n = r.count(14);
    for (std::size_t i = 0; i < n; ++i) { const auto node = r.u64(); d.cells.push_back({node, local_cell(r)}); }
    const RelationalFieldInterface interface(std::move(parent), std::move(d), limits);
    const auto kind = static_cast<FieldSectionKind>(r.u8()); const auto count = r.count(74); std::vector<RelationalFieldViewReading> readings;
    for (std::size_t i = 0; i < count; ++i) {
        auto declaration = view(r); const auto status = static_cast<FieldReadingStatus>(r.u8());
        auto visible = storage::temporal_codec_detail::tensor(r); readings.push_back(interface.admit(std::move(declaration), std::move(visible), status, limits));
    }
    auto section = assemble_relational_readings(std::move(readings), kind, limits); const auto cells = r.count(14);
    field_program_detail::require(cells == section.section().values().size(), "cold field section omits original stalks");
    for (const auto& [expected_cell, expected_values] : section.section().values()) {
        field_program_detail::require(local_cell(r) == expected_cell, "cold field section original cell order");
        const auto dimension = r.count(10);
        field_program_detail::require(dimension == expected_values.size(), "cold field section stalk dimension");
        for (const auto& q : expected_values) field_program_detail::require(r.rational() == q, "cold field section complete reconstruction mismatch");
    }
    field_program_detail::require(r.remaining() == 0, "trailing field section bytes");
    canonical(bytes, encode_relational_field_section(section)); return section;
}
} // namespace rfc::inference
