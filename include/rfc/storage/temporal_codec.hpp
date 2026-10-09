#pragma once

#include "rfc/storage/relational_code_delta.hpp"
#include "rfc/storage/temporal_declaration_codec.hpp"

namespace rfc::storage {
namespace temporal_codec_detail {
// Serialize the retained channels once. Hash extensions keep their original
// domain separators, while the sidecar keeps its original contiguous records.
inline std::uint64_t endpoint_body(Writer& w, const TemporalEndpoint& endpoint,
                                   const StableSourceIndex& source_index,
                                   std::uint64_t source_hash) {
    Writer prefix;
    prefix.u64(endpoint.sequence); prefix.u64(source_hash);
    auto hash = rfcx_detail::fnv1a64(prefix.bytes());
    const auto begin = w.size();
    const auto blocks = index(endpoint.local_blocks, [](const auto& b) { return b.block_key; });
    w.u64(blocks.size());
    for (const auto& [key, b] : blocks) {
        (void)key; local(w, temporal_endpoint_detail::normalize_local(endpoint.source, source_index, *b));
    }
    const auto cs = temporal_endpoint_detail::carriers(endpoint);
    w.u64(cs.size()); for (const auto& [key, c] : cs) { (void)key; carrier(w, *c); }
    const auto ts = temporal_endpoint_detail::tensors(endpoint);
    w.u64(ts.size()); for (const auto& [key, t] : ts) { (void)key; tensor_record(w, *t); }
    hash = rfcx_detail::fnv1a64_update(hash, std::span<const std::byte>(w.bytes()).subspan(begin));
    const auto extension = [&](std::uint64_t tag, auto&& write) {
        Writer separator; separator.u64(tag);
        hash = rfcx_detail::fnv1a64_update(hash, separator.bytes());
        const auto offset = w.size();
        write();
        hash = rfcx_detail::fnv1a64_update(hash, std::span<const std::byte>(w.bytes()).subspan(offset));
    };
    if (!endpoint.finite_systems.empty())
        extension(0x314554494e494658ULL, [&] { finite_systems(w, endpoint.finite_systems); });
    if (endpoint.codes)
        extension(0x3145444f434c4552ULL, [&] { code_state(w, *endpoint.codes); });
    if (endpoint.declarations)
        extension(0x3153434544584652ULL, [&] { declaration_state(w, *endpoint.declarations); });
    return hash;
}
// The caller validates this endpoint against its source index before hashing.
inline std::uint64_t endpoint_hash(const TemporalEndpoint& endpoint,
                                   const StableSourceIndex& source_index, std::uint64_t source_hash) {
    Writer w;
    return endpoint_body(w, endpoint, source_index, source_hash);
}
} // namespace temporal_codec_detail

// Includes every declared endpoint channel and the sequence, not just the
// source snapshot. Physical RFCX source binding remains independently available.
inline std::uint64_t temporal_endpoint_hash(const TemporalEndpoint& endpoint) {
    const StableSourceIndex source_index(endpoint.source);
    temporal_endpoint_detail::validate(endpoint, source_index);
    const auto source = encode_rfcx(endpoint.source);
    return temporal_codec_detail::endpoint_hash(endpoint, source_index, rfcx_detail::fnv1a64(source.bytes));
}

} // namespace rfc::storage
