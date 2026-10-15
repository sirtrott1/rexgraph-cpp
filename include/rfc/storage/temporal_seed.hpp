#pragma once

#include "rfc/storage/temporal_segments.hpp"

namespace rfc::storage {

namespace temporal_seed_detail {
inline constexpr std::array<std::byte,8> magic{
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'E'},std::byte{1},std::byte{0},std::byte{0}};

struct Encoding {
    RfcxImage source;
    std::vector<std::byte> channels;
    std::uint64_t source_hash{}, channel_hash{};
};
inline Encoding encode(const TemporalEndpoint& endpoint, const StableSourceIndex& source_index,
                        RfcxImage source, std::size_t capacity = 0) {
    using namespace temporal_codec_detail;
    temporal_endpoint_detail::validate(endpoint, source_index);
    const auto source_hash = rfcx_detail::fnv1a64(source.bytes);
    const auto header=[&](Writer& w) {
        w.raw(magic); w.u32(1);
        w.u32((endpoint.finite_systems.empty() ? 0u : finite_systems_flag) |
              (endpoint.codes ? relational_codes_flag : 0u) | (endpoint.declarations ? declarations_flag : 0u));
        w.u64(0); w.u64(0);
        w.u64(endpoint.sequence); w.u64(source.stats.payload_hash); w.u64(source_hash); w.u64(0);
    };
    if (endpoint.declarations) {
        auto measured=Writer::measure_only();header(measured);
        (void)endpoint_body(measured,endpoint,source_index,source_hash);
        capacity=std::max(capacity,measured.size());
    }
    Writer w(capacity);header(w);
    const auto hash = endpoint_body(w, endpoint, source_index, source_hash);
    w.patch_u64(56, hash);
    w.patch_u64(16, w.size());
    w.patch_u64(24, rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));
    return {std::move(source), std::move(w).take_bytes(), source_hash, hash};
}
struct Decoded {
    TemporalEndpoint endpoint;
    Encoding encoding;
    std::shared_ptr<const std::vector<std::byte>> retained_channels;
};
} // namespace temporal_seed_detail

// Initial endpoint channels are bound to the canonical RFCX source.
inline std::vector<std::byte> encode_temporal_endpoint(const TemporalEndpoint& endpoint) {
    const StableSourceIndex source_index(endpoint.source);
    auto encoded = temporal_seed_detail::encode(endpoint, source_index, encode_rfcx(endpoint.source));
    encoded.channels.shrink_to_fit();
    return std::move(encoded.channels);
}
namespace temporal_seed_detail {
inline Decoded decode_component(core::RelationalComplex source,
                      std::span<const std::byte> bytes,
                      const TemporalReadLimits& limits,
                      std::shared_ptr<const std::vector<std::byte>> encoded_owner) {
    using namespace temporal_codec_detail;
    require(bytes.size()>=32 && bytes.size()<=limits.max_file_bytes,"invalid endpoint sidecar size");
    rfcx_detail::Reader header(bytes);
    require(std::equal(temporal_seed_detail::magic.begin(),temporal_seed_detail::magic.end(),header.slice(8).begin()),"invalid endpoint sidecar magic");
    require(header.u32()==1,"unsupported endpoint sidecar version");
    const auto flags=header.u32();
    require((flags & ~(finite_systems_flag | relational_codes_flag | declarations_flag))==0,"unsupported endpoint sidecar flags");
    require(header.u64()==bytes.size(),"endpoint sidecar length mismatch");
    require(header.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"endpoint sidecar integrity mismatch");
    require(!encoded_owner || (encoded_owner->data() == bytes.data() && encoded_owner->size() == bytes.size()),
            "endpoint sidecar owner differs from its complete component");
    Reader r = encoded_owner ? Reader(std::move(encoded_owner), limits) : Reader(bytes, limits, true);
    // Retain the complete component, including its unchanged envelope.
    (void)r.u64(); (void)r.u32(); (void)r.u32(); (void)r.u64(); (void)r.u64();
    TemporalEndpoint endpoint(std::move(source),r.u64());
    auto image = encode_rfcx(endpoint.source);
    require(r.u64() == image.stats.payload_hash,"endpoint sidecar base RFCX payload mismatch");
    require(r.u64() == rfcx_detail::fnv1a64(image.bytes),"endpoint sidecar source header/layout mismatch");
    const auto hash=r.u64();
    auto n=r.count();for(std::size_t i=0;i<n;++i)endpoint.local_blocks.push_back(local(r));
    n=r.count();for(std::size_t i=0;i<n;++i)endpoint.carriers.push_back(carrier(r));
    n=r.count();for(std::size_t i=0;i<n;++i)endpoint.tensors.push_back(tensor_record(r));
    if ((flags & finite_systems_flag) != 0) {
        endpoint.finite_systems=finite_systems(r);
        require(!endpoint.finite_systems.empty(),"empty finite endpoint extension");
    }
    if ((flags & relational_codes_flag) != 0) endpoint.codes=code_state(r);
    if ((flags & declarations_flag) != 0) endpoint.declarations=declaration_state(r);
    require(r.remaining()==0,"trailing endpoint sidecar data");
    const StableSourceIndex local_index(endpoint.source);
    for(auto& b:endpoint.local_blocks)b=temporal_endpoint_detail::normalize_local(endpoint.source,local_index,std::move(b));
    auto canonical = encode(endpoint, local_index, std::move(image), bytes.size());
    require(hash == canonical.channel_hash,"endpoint sidecar channel hash mismatch");
    if (limits.require_canonical) {
        require(canonical.channels.size() == bytes.size() &&
                std::equal(canonical.channels.begin(),canonical.channels.end(),bytes.begin()),
                "noncanonical endpoint sidecar encoding");
    }
    return {std::move(endpoint), std::move(canonical), r.retained_owner()};
}
inline Decoded decode(core::RelationalComplex source, std::span<const std::byte> bytes,
                      const TemporalReadLimits& limits) {
    return decode_component(std::move(source), bytes, limits, {});
}
inline Decoded decode_owned(core::RelationalComplex source, std::vector<std::byte>&& bytes,
                            const TemporalReadLimits& limits) {
    // This path consumes a freshly encoded component. It does not adopt a
    // mutable endpoint or waive canonical reencoding and complete validation.
    auto owner = std::make_shared<const std::vector<std::byte>>(std::move(bytes));
    const auto view = std::span<const std::byte>(*owner);
    return decode_component(std::move(source), view, limits, std::move(owner));
}
} // namespace temporal_seed_detail

inline TemporalEndpoint decode_temporal_endpoint(core::RelationalComplex source,
                                                 std::span<const std::byte> bytes,
                                                 const TemporalReadLimits& limits = {}) {
    return temporal_seed_detail::decode(std::move(source), bytes, limits).endpoint;
}
inline std::uint64_t write_temporal_endpoint(const TemporalEndpoint& endpoint,const std::filesystem::path& path) {
    const auto bytes=encode_temporal_endpoint(endpoint);
    write_file_exact(path, bytes);
    return bytes.size();
}
inline TemporalEndpoint read_temporal_endpoint(const MappedRfcx& source,const std::filesystem::path& path,
                                               const TemporalReadLimits& limits={}) {
    const auto bytes = read_file_exact(path, limits.max_file_bytes);
    auto materialized=source.materialize();
    temporal_source_detail::require(encode_rfcx(materialized).stats.payload_hash==source.payload_hash(),
                                   "endpoint source is not canonical reconstructible RFCX");
    return decode_temporal_endpoint(std::move(materialized),bytes,limits);
}

} // namespace rfc::storage
