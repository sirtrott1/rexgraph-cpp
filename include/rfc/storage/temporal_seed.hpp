#pragma once

#include "rfc/storage/temporal_segments.hpp"

namespace rfc::storage {

namespace temporal_seed_detail {
inline constexpr std::array<std::byte,8> magic{
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'E'},std::byte{1},std::byte{0},std::byte{0}};
}

// Initial endpoint channels are a sidecar bound to the canonical RFCX source.
// It is not a duplicate source snapshot or a Phase-05 checkpoint manager.
inline std::vector<std::byte> encode_temporal_endpoint(const TemporalEndpoint& endpoint) {
    using namespace temporal_codec_detail;
    validate_temporal_endpoint(endpoint);
    Writer w; w.raw(temporal_seed_detail::magic); w.u32(1);
    w.u32((endpoint.finite_systems.empty() ? 0u : finite_systems_flag) | (endpoint.codes ? relational_codes_flag : 0u) | (endpoint.declarations ? declarations_flag : 0u)); w.u64(0); w.u64(0);
    const auto source=encode_rfcx(endpoint.source);
    w.u64(endpoint.sequence); w.u64(source.stats.payload_hash); w.u64(rfcx_detail::fnv1a64(source.bytes));
    w.u64(temporal_endpoint_hash(endpoint));
    const StableSourceIndex local_index(endpoint.source);
    const auto blocks=index(endpoint.local_blocks,[](const auto& b){return b.block_key;});
    w.u64(blocks.size());
    for(const auto& [key,b]:blocks){(void)key;local(w,temporal_endpoint_detail::normalize_local(endpoint.source,local_index,*b));}
    const auto cs=temporal_endpoint_detail::carriers(endpoint);
    w.u64(cs.size());for(const auto& [key,c]:cs){(void)key;carrier(w,*c);}
    const auto ts=temporal_endpoint_detail::tensors(endpoint);
    w.u64(ts.size());for(const auto& [key,t]:ts){(void)key;tensor_record(w,*t);}
    if (!endpoint.finite_systems.empty()) finite_systems(w, endpoint.finite_systems);
    if (endpoint.codes) code_state(w,*endpoint.codes);
    if (endpoint.declarations) declaration_state(w,*endpoint.declarations);
    w.patch_u64(16,w.size());
    w.patch_u64(24,rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));
    return w.bytes();
}
inline TemporalEndpoint decode_temporal_endpoint(core::RelationalComplex source,
                                                 std::span<const std::byte> bytes,
                                                 const TemporalReadLimits& limits={}) {
    using namespace temporal_codec_detail;
    require(bytes.size()>=32 && bytes.size()<=limits.max_file_bytes,"invalid endpoint sidecar size");
    rfcx_detail::Reader header(bytes);
    require(std::equal(temporal_seed_detail::magic.begin(),temporal_seed_detail::magic.end(),header.slice(8).begin()),"invalid endpoint sidecar magic");
    require(header.u32()==1,"unsupported endpoint sidecar version");
    const auto flags=header.u32();
    require((flags & ~(finite_systems_flag | relational_codes_flag | declarations_flag))==0,"unsupported endpoint sidecar flags");
    require(header.u64()==bytes.size(),"endpoint sidecar length mismatch");
    require(header.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"endpoint sidecar integrity mismatch");
    Reader r(bytes.subspan(32),limits);
    TemporalEndpoint endpoint(std::move(source),r.u64());
    const auto image=encode_rfcx(endpoint.source);
    require(r.u64()==image.stats.payload_hash,"endpoint sidecar base RFCX payload mismatch");
    require(r.u64()==rfcx_detail::fnv1a64(image.bytes),"endpoint sidecar source header/layout mismatch");
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
    require(temporal_endpoint_hash(endpoint)==hash,"endpoint sidecar channel hash mismatch");
    if (limits.require_canonical) {
        const auto canonical = encode_temporal_endpoint(endpoint);
        require(canonical.size() == bytes.size() && std::equal(canonical.begin(),canonical.end(),bytes.begin()),
                "noncanonical endpoint sidecar encoding");
    }
    return endpoint;
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
