#pragma once

#include "rfc/storage/native_series.hpp"

namespace rfc::storage {
namespace native_series_codec_detail {
inline constexpr std::array<std::byte, 8> magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'},
    std::byte{'X'}, std::byte{'S'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline void blob(temporal_codec_detail::Writer& w, std::span<const std::byte> bytes) {
    w.u64(bytes.size()); w.raw(bytes);
}
inline TemporalEndpoint decode_snapshot(std::span<const std::byte> source, std::span<const std::byte> channels,
                                        const NativeSeriesLimits& limits) {
    const auto p = native_series_detail::policy(limits);
    native_series_detail::bound(source.size(), p.max_file_bytes, "series source byte budget");
    native_series_detail::bound(channels.size(), p.max_file_bytes, "series channel byte budget");
    MappedRfcx image(source);
    auto endpoint = decode_temporal_endpoint(image.materialize(), channels, native_series_detail::policy(limits));
    const auto canonical = encode_rfcx(endpoint.source).bytes;
    native_series_detail::require(canonical.size() == source.size() &&
        std::equal(canonical.begin(), canonical.end(), source.begin()), "noncanonical series source snapshot");
    return endpoint;
}
} // namespace native_series_codec_detail

// Owning canonical archive: base source once + owning channels + exact ordered
// RFCXT frames + optional independently validated full checkpoint witnesses.
// No physical checkpoint is substituted for the retained operation history.
inline std::vector<std::byte> encode_native_series(const NativeTemporalSeries& series) {
    using namespace native_series_codec_detail;
    const auto& s = series.state(); temporal_codec_detail::Writer w;
    w.raw(magic); w.u32(1); w.u32(0); w.u64(s.archive_bytes); w.u64(0);
    w.u64(s.name_space); blob(w, s.base->source); blob(w, s.base->channels);
    w.u64(s.frames.size()); for (const auto& f : s.frames) blob(w, f->bytes);
    w.u64(s.checkpoints.size());
    for (const auto& [seq, cp] : s.checkpoints) { w.u64(seq); blob(w, cp->source); blob(w, cp->channels); }
    native_series_detail::require(w.size() == s.archive_bytes, "series archive accounting invariant");
    w.patch_u64(24, rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));
    return w.bytes();
}
inline NativeTemporalSeries decode_native_series(std::span<const std::byte> bytes, const NativeSeriesLimits& limits = {}) {
    using namespace native_series_codec_detail; using native_series_detail::require; using native_series_detail::bound;
    require(bytes.size() >= 32, "truncated series header"); bound(bytes.size(), limits.max_archive_bytes, "series archive byte budget");
    rfcx_detail::Reader header(bytes);
    require(std::equal(magic.begin(), magic.end(), header.slice(8).begin()), "invalid series magic");
    require(header.u32() == 1, "unsupported series version"); require(header.u32() == 0, "unsupported series flags");
    require(header.u64() == bytes.size(), "series length mismatch");
    require(header.u64() == rfcx_detail::fnv1a64(bytes.subspan(32)), "series integrity mismatch");
    auto policy = native_series_detail::policy(limits); policy.max_file_bytes = limits.max_archive_bytes;
    temporal_codec_detail::Reader r(bytes.subspan(32), policy);
    const auto ns = r.u64(); const auto source = r.blob(); const auto channels = r.blob();
    NativeTemporalSeries out(decode_snapshot(source, channels, limits), ns, limits);
    const auto n = r.count(8); bound(n, limits.max_segments, "series frame count budget");
    for (std::size_t i = 0; i < n; ++i) out.append_encoded(r.blob());
    const auto cps = r.count(24); bound(cps, limits.max_checkpoints, "series checkpoint count budget");
    // Every cached witness is verified against history. Validating only hashes
    // or sequence numbers would allow a checkpoint to rewrite the history.
    std::optional<std::uint64_t> previous;
    for (std::size_t i = 0; i < cps; ++i) {
        const auto seq = r.u64(); require(!previous || seq > *previous, "unordered series checkpoints"); previous = seq;
        const auto cp_source = r.blob(); const auto cp_channels = r.blob();
        auto cp = decode_snapshot(cp_source, cp_channels, limits);
        require(cp.sequence == seq, "checkpoint sequence mismatch");
        auto expected = out.at(seq);
        require(encode_rfcx(expected.endpoint().source).bytes == encode_rfcx(cp.source).bytes &&
                encode_temporal_endpoint(expected.endpoint()) == encode_temporal_endpoint(cp),
                "checkpoint differs from complete retained history");
        out.checkpoint(seq);
    }
    require(r.remaining() == 0, "trailing series data");
    const auto canonical = encode_native_series(out);
    require(canonical.size() == bytes.size() && std::equal(canonical.begin(), canonical.end(), bytes.begin()),
            "noncanonical series archive");
    return out;
}
inline NativeTemporalSeries read_native_series(const std::filesystem::path& path, const NativeSeriesLimits& limits = {}) {
    const auto bytes = read_file_exact(path, limits.max_archive_bytes);
    return decode_native_series(bytes, limits);
}
// No filesystem writer is claimed here. Encode first, then let the host choose
// its exclusive-create/atomic-replace/fsync/recovery policy. The local CLI uses
// its existing exclusive-create helper and never overwrites an input file.

} // namespace rfc::storage
