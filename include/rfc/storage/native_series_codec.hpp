#pragma once

#include "rfc/storage/native_series.hpp"

namespace rfc::storage {
namespace native_series_codec_detail {
inline constexpr std::array<std::byte, 8> magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'},
    std::byte{'X'}, std::byte{'S'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline void blob(temporal_codec_detail::Writer& w, std::span<const std::byte> bytes) {
    w.u64(bytes.size()); w.raw(bytes);
}
inline temporal_seed_detail::Decoded decode_snapshot(std::span<const std::byte> source, std::span<const std::byte> channels,
                                        const NativeSeriesLimits& limits) {
    const auto p = native_series_detail::policy(limits);
    native_series_detail::bound(source.size(), p.max_file_bytes, "series source byte budget");
    native_series_detail::bound(channels.size(), p.max_file_bytes, "series channel byte budget");
    MappedRfcx image(source);
    auto decoded = temporal_seed_detail::decode(image.materialize(), channels, p);
    const auto& canonical = decoded.encoding.source.bytes;
    native_series_detail::require(canonical.size() == source.size() &&
        std::equal(canonical.begin(), canonical.end(), source.begin()), "noncanonical series source snapshot");
    return decoded;
}
} // namespace native_series_codec_detail

// The callback consumes bounded canonical chunks. A pinned immutable fork keeps
// reentrant callers from changing the publication between the integrity pass and
// emission. No complete archive buffer is needed for streaming hosts.
template<class Function> void stream_native_series(const NativeTemporalSeries& series,Function&& consume) {
    using namespace native_series_codec_detail;
    const auto pinned=series;
    const auto body=[&](auto&& sink) {
        const auto integer=[&](std::uint64_t value) {temporal_codec_detail::Writer w;w.u64(value);sink(std::span<const std::byte>(w.bytes()));};
        integer(pinned.source_namespace());bool frame_count=false,checkpoint_count=false;
        pinned.visit_components([&](const NativeSeriesComponent& part) {
            const bool checkpoint=part.kind==NativeSeriesComponentKind::CheckpointSource || part.kind==NativeSeriesComponentKind::CheckpointChannels;
            if((part.kind==NativeSeriesComponentKind::Segment || checkpoint) && !frame_count) {integer(pinned.segment_count());frame_count=true;}
            if(checkpoint && !checkpoint_count) {integer(pinned.storage_stats().checkpoint_count);checkpoint_count=true;}
            if(part.kind==NativeSeriesComponentKind::CheckpointSource)integer(part.sequence);
            integer(part.bytes->size());
            const auto bytes=std::span<const std::byte>(*part.bytes);
            for(std::size_t pos=0;pos<bytes.size();) {
                const auto length=std::min(std::size_t{65536},bytes.size()-pos);sink(bytes.subspan(pos,length));pos+=length;
            }
        });
        if(!frame_count)integer(pinned.segment_count());
        if(!checkpoint_count)integer(pinned.storage_stats().checkpoint_count);
    };
    std::size_t size=32;std::uint64_t hash=1469598103934665603ull;
    body([&](auto bytes){size=machine::checked_size_add(size,bytes.size());for(const auto b:bytes){hash^=std::to_integer<std::uint8_t>(b);hash*=1099511628211ull;}});
    native_series_detail::require(size==pinned.archive_bytes(),"series archive accounting invariant");
    temporal_codec_detail::Writer header;header.raw(magic);header.u32(1);header.u32(0);header.u64(size);header.u64(hash);
    consume(std::span<const std::byte>(header.bytes()));body(consume);
}
// Base source once, exact ordered frames and validated checkpoint witnesses.
inline std::vector<std::byte> encode_native_series(const NativeTemporalSeries& series) {
    std::vector<std::byte> bytes;bytes.reserve(series.archive_bytes());
    stream_native_series(series,[&](auto chunk){bytes.insert(bytes.end(),chunk.begin(),chunk.end());});return bytes;
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
    require(ns != 0, "series namespace required");
    auto decoded = decode_snapshot(source, channels, limits);
    // decode_snapshot certifies both canonical components and creates fresh
    // endpoint storage. Freeze that actual reader-owned result once. A host
    // endpoint constructor continues to copy/redecode its mutable input.
    auto state = std::make_shared<native_series_detail::State>();
    state->name_space = ns; state->limits = limits;
    state->base = std::make_shared<const native_series_detail::Snapshot>(native_series_detail::Snapshot{
        std::make_shared<const TemporalEndpoint>(std::move(decoded.endpoint)),
        std::move(decoded.encoding.source.bytes), decoded.retained_channels ? std::move(decoded.retained_channels)
            : std::make_shared<const native_series_detail::Bytes>(std::move(decoded.encoding.channels))});
    state->tail = state->base->endpoint;
    state->archive_bytes = native_series_detail::archive_size(*state);
    NativeTemporalSeries out(std::move(state));
    const auto n = r.count(8); bound(n, limits.max_segments, "series frame count budget");
    for (std::size_t i = 0; i < n; ++i) out.append_encoded(r.blob());
    const auto cps = r.count(24); bound(cps, limits.max_checkpoints, "series checkpoint count budget");
    // Every cached witness is verified against history. Validating only hashes
    // or sequence numbers would allow a checkpoint to rewrite the history.
    std::optional<std::uint64_t> previous;
    for (std::size_t i = 0; i < cps; ++i) {
        const auto seq = r.u64(); require(!previous || seq > *previous, "unordered series checkpoints"); previous = seq;
        const auto cp_source = r.blob(); const auto cp_channels = r.blob();
        auto decoded_checkpoint = decode_snapshot(cp_source, cp_channels, limits);
        const auto& cp = decoded_checkpoint.endpoint;
        require(cp.sequence == seq, "checkpoint sequence mismatch");
        auto expected = out.at(seq);
        require(encode_rfcx(expected.endpoint().source).bytes == encode_rfcx(cp.source).bytes &&
                encode_temporal_endpoint(expected.endpoint()) == encode_temporal_endpoint(cp),
                "checkpoint differs from complete retained history");
        out.checkpoint(seq);
    }
    require(r.remaining() == 0, "trailing series data");
    std::size_t compared = 0;
    stream_native_series(out, [&](auto chunk) {
        require(chunk.size() <= bytes.size() - compared &&
                std::equal(chunk.begin(), chunk.end(), bytes.data() + compared),
                "noncanonical series archive");
        compared += chunk.size();
    });
    require(compared == bytes.size(), "noncanonical series archive");
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
