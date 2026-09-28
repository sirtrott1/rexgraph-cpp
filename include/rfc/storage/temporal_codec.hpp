#pragma once

#include "rfc/storage/relational_code_delta.hpp"
#include "rfc/storage/temporal_declaration_codec.hpp"

namespace rfc::storage {

// Includes every declared endpoint channel and the sequence, not just the
// source snapshot. Physical RFCX source binding remains independently available.
inline std::uint64_t temporal_endpoint_hash(const TemporalEndpoint& endpoint) {
    using namespace temporal_codec_detail;
    validate_temporal_endpoint(endpoint);
    Writer w;
    const auto source = encode_rfcx(endpoint.source);
    w.u64(endpoint.sequence); w.u64(rfcx_detail::fnv1a64(source.bytes));
    const StableSourceIndex local_index(endpoint.source);
    const auto blocks = index(endpoint.local_blocks, [](const auto& b) { return b.block_key; });
    w.u64(blocks.size()); for (const auto& [key, b] : blocks) { (void)key; local(w, temporal_endpoint_detail::normalize_local(endpoint.source, local_index, *b)); }
    const auto cs = temporal_endpoint_detail::carriers(endpoint);
    w.u64(cs.size()); for (const auto& [key, c] : cs) { (void)key; carrier(w, *c); }
    const auto ts = temporal_endpoint_detail::tensors(endpoint);
    w.u64(ts.size()); for (const auto& [key, t] : ts) { (void)key; tensor_record(w, *t); }
    if (!endpoint.finite_systems.empty()) {
        // Domain-separated suffix; omitted for byte-compatible legacy hashes.
        w.u64(0x314554494e494658ULL); // "XFINITE1", little endian
        finite_systems(w, endpoint.finite_systems);
    }
    if (endpoint.codes) {
        w.u64(0x3145444f434c4552ULL); // "RELCODE1", little endian
        code_state(w, *endpoint.codes);
    }
    if (endpoint.declarations) {
        w.u64(0x3153434544584652ULL); // domain-separated declaration channel
        declaration_state(w, *endpoint.declarations);
    }
    return rfcx_detail::fnv1a64(w.bytes());
}

} // namespace rfc::storage
