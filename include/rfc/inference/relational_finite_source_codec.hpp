#pragma once

#include "rfc/inference/relational_finite_source.hpp"
#include "rfc/inference/relational_field_interface_codec.hpp"

namespace rfc::inference {
namespace relational_finite_source_codec_detail {
using namespace field_interface_codec_detail;
inline constexpr std::array<std::byte, 8> source_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Q'}, std::byte{'S'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline std::vector<std::byte> frozen_bytes(std::span<const std::byte> bytes, const storage::TemporalReadLimits& policy) {
    field_program_detail::require(bytes.size() <= policy.max_file_bytes, "finite packet exceeds byte admission");
    return {bytes.begin(), bytes.end()};
}
inline void finite_keys(Writer& w, const std::vector<std::uint64_t>& values) {
    w.u64(values.size()); for (const auto value : values) w.u64(value);
}
inline std::vector<std::uint64_t> finite_keys(Reader& r) {
    const auto n = r.count(8); std::vector<std::uint64_t> out;
    for (std::size_t i = 0; i < n; ++i) out.push_back(r.u64());
    return out;
}
inline void cells(Writer& w, const std::vector<core::CellId>& values) {
    w.u64(values.size()); for (const auto cell : values) local_cell(w, cell);
}
inline std::vector<core::CellId> cells(Reader& r) {
    const auto n = r.count(6); std::vector<core::CellId> out;
    for (std::size_t i = 0; i < n; ++i) out.push_back(local_cell(r));
    return out;
}
inline void declaration(Writer& w, const RelationalFiniteSourceDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.symbols.size());
    for (const auto& s : d.symbols) { w.u64(s.key); blob(w, s.definition); }
    w.u64(d.variables.size()); for (const auto& v : d.variables) { local_cell(w, v.cell); finite_keys(w, v.domain); }
    w.u64(d.charts.size()); for (const auto& c : d.charts) {
        w.u64(c.key); storage::temporal_codec_detail::string(w, c.name); cells(w, c.patch); cells(w, c.support);
        w.u64(c.allowed.size()); for (const auto& row : c.allowed) finite_keys(w, row);
    }
    w.u64(d.bindings.size()); for (const auto& b : d.bindings) { w.u64(b.key); w.u64(b.use); local_cell(w, b.cell); }
}
inline RelationalFiniteSourceDeclaration declaration(Reader& r) {
    RelationalFiniteSourceDeclaration d; d.key = r.u64(); d.revision = r.u64(); const auto symbols = r.count(16);
    for (std::size_t i = 0; i < symbols; ++i) { const auto key = r.u64(); const auto bytes = r.blob(); d.symbols.push_back({key, {bytes.begin(), bytes.end()}}); }
    const auto variables = r.count(14);
    for (std::size_t i = 0; i < variables; ++i) { const auto cell = local_cell(r); d.variables.push_back({cell, finite_keys(r)}); }
    const auto charts = r.count(40);
    for (std::size_t i = 0; i < charts; ++i) {
        RelationalFiniteChart c; c.key = r.u64(); c.name = r.string(); c.patch = cells(r); c.support = cells(r);
        const auto rows = r.count(8); for (std::size_t j = 0; j < rows; ++j) c.allowed.push_back(finite_keys(r)); d.charts.push_back(std::move(c));
    }
    const auto bindings = r.count(22);
    for (std::size_t i = 0; i < bindings; ++i) { const auto key = r.u64(), use = r.u64(); d.bindings.push_back({key, use, local_cell(r)}); }
    return d;
}
} // namespace relational_finite_source_codec_detail
inline std::vector<std::byte> encode_relational_finite_source(const OwnedRelationalFiniteSource& source) {
    using namespace relational_finite_source_codec_detail; const auto pin = source; auto w = envelope(source_magic);
    blob(w, encode_relational_input_use_asset(pin.asset())); declaration(w, pin.declaration()); return finish(std::move(w));
}
inline OwnedRelationalFiniteSource decode_relational_finite_source(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace relational_finite_source_codec_detail; const auto read_policy = policy;
    const auto image = frozen_bytes(bytes, read_policy); bytes = image; Reader r(bytes, read_policy); header(r, bytes, source_magic);
    if (check) check();
    auto asset = decode_relational_input_use_asset(r.blob(), read_policy, limits); auto d = declaration(r);
    field_program_detail::require(!r.remaining(), "trailing finite source bytes");
    OwnedRelationalFiniteSource out(std::move(asset), std::move(d), limits, check);
    canonical(bytes, encode_relational_finite_source(out)); return out;
}
} // namespace rfc::inference
