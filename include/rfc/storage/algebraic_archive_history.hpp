#pragma once
#include "rfc/storage/algebraic_archive_codec.hpp"
#include "rfc/storage/native_series.hpp"

namespace rfc::storage {
// Declared physical octets: an ordered Repeated<Integer> of bounded chunks.
// Each magnitude is 0x01 followed by its exact octets, retaining leading zeros.
// Version 2 separates the sequence type from its version 1 Integer chunk type.
struct AlgebraicArchiveChannel {
    std::uint64_t namespace_key,type_key,value_key;
    composite::DeclarationId type() const { return {namespace_key,type_key,2}; }
    composite::DeclarationId chunk_type() const { return {namespace_key,type_key,1}; }
};
namespace algebraic_archive_history_detail {
inline constexpr std::size_t chunk_bytes=4096,max_chunks=4096;
inline composite::Declaration sequence_declaration(AlgebraicArchiveChannel channel) {
    return composite::Declaration::repeated(channel.type(),channel.chunk_type(),1,max_chunks);
}
inline void validate_channel(AlgebraicArchiveChannel channel) {
    algebraic_archive_detail::require(channel.type().valid() && channel.value_key,"archive history channel needs explicit nonzero names");
}
inline numeric::Integer pack(std::span<const std::byte> bytes) {
    numeric::Integer value{1};
    const auto bits=machine::checked_size_mul(bytes.size(),std::size_t{8});
    mpz_mul_2exp(value.get_mpz_t(),value.get_mpz_t(),static_cast<mp_bitcnt_t>(bits));
    numeric::Integer body{0}; if (!bytes.empty()) mpz_import(body.get_mpz_t(),bytes.size(),1,1,1,0,bytes.data());
    value+=body; return value;
}
inline std::vector<std::byte> unpack(const composite::DeclaredValue& value,const AlgebraicArchiveLimits& limits) {
    const auto* integer=std::get_if<numeric::Integer>(&value.data);
    algebraic_archive_detail::require(integer && *integer>0,"archive history payload must be a positive encoded Integer");
    const auto bits=mpz_sizeinbase(integer->get_mpz_t(),2);
    algebraic_archive_detail::require(bits%8==1,"archive history octet sentinel is invalid");
    const auto size=(bits-1)/8; algebraic_archive_detail::bound(size,limits.max_file_bytes,"archive history payload byte budget");
    std::vector<std::byte> bytes(machine::checked_size_add(size,1)); std::size_t written{};
    mpz_export(bytes.data(),&written,1,1,1,0,integer->get_mpz_t());
    algebraic_archive_detail::require(written==bytes.size() && bytes.front()==std::byte{1},"archive history payload prefix differs");
    bytes.erase(bytes.begin()); return bytes;
}
inline composite::DeclaredValue chunks(std::span<const std::byte> bytes) {
    const auto count=machine::ceil_div(bytes.size(),chunk_bytes);
    algebraic_archive_detail::require(count>0 && count<=max_chunks,"archive history chunk count budget");
    std::vector<composite::DeclaredValue> values;values.reserve(count);
    for (std::size_t start=0;start<bytes.size();start+=chunk_bytes)
        values.push_back(composite::DeclaredValue::integer(pack(bytes.subspan(start,std::min(chunk_bytes,bytes.size()-start)))));
    return composite::DeclaredValue::repeated(std::move(values));
}
inline std::vector<std::byte> concatenate(const composite::DeclaredValue& value,const AlgebraicArchiveLimits& limits) {
    const auto* repeated=std::get_if<composite::DeclaredValue::Repeated>(&value.data);
    algebraic_archive_detail::require(repeated && !repeated->elements.empty() && repeated->elements.size()<=max_chunks,"archive history chunk sequence differs");
    std::size_t total{};
    for (std::size_t i=0;i<repeated->elements.size();++i) {
        const auto* integer=std::get_if<numeric::Integer>(&repeated->elements[i].data);
        algebraic_archive_detail::require(integer && *integer>0,"archive history chunk is not a positive Integer");
        const auto bits=mpz_sizeinbase(integer->get_mpz_t(),2);
        algebraic_archive_detail::require(bits%8==1,"archive history chunk sentinel differs");
        const auto n=(bits-1)/8;
        algebraic_archive_detail::require(n>0 && n<=chunk_bytes && (i+1==repeated->elements.size() || n==chunk_bytes),"noncanonical archive chunk boundaries");
        total=machine::checked_size_add(total,n);algebraic_archive_detail::bound(total,limits.max_file_bytes,"archive history aggregate byte budget");
    }
    std::vector<std::byte> out;out.reserve(total);auto chunk_limits=limits;chunk_limits.max_file_bytes=chunk_bytes;
    for (const auto& chunk:repeated->elements) {const auto bytes=unpack(chunk,chunk_limits);out.insert(out.end(),bytes.begin(),bytes.end());}
    return out;
}
} // namespace algebraic_archive_history_detail

// Add once or explicitly replace this named channel. Other endpoint channels
// are retained. Source changes and declaration migrations remain separate work.
inline TemporalEndpoint with_algebraic_archive(const TemporalEndpoint& endpoint,AlgebraicArchiveChannel channel,
    const AlgebraicArchive& archive,const AlgebraicArchiveLimits& limits={}) {
    using algebraic_archive_detail::require;
    algebraic_archive_history_detail::validate_channel(channel); validate_temporal_endpoint(endpoint);
    require(encode_rfcx(endpoint.source).bytes==encode_rfcx(archive.bundle().source()).bytes,
        "archive history channel requires the complete endpoint source");
    auto target=endpoint;
    if (!target.declarations) {
        target.declarations.emplace(); target.declarations->namespace_key=channel.namespace_key;
        target.declarations->registry=composite::DeclarationRegistry({composite::Declaration::integer(channel.chunk_type()),
            algebraic_archive_history_detail::sequence_declaration(channel)});
    }
    auto& state=*target.declarations;
    require(state.namespace_key==channel.namespace_key,"archive history declaration namespace differs");
    const auto& definitions=state.registry.definitions();
    const auto definition=std::find_if(definitions.begin(),definitions.end(),[&](const auto& d){return d.id==channel.type();});
    require(definition!=definitions.end() && *definition==algebraic_archive_history_detail::sequence_declaration(channel) &&
        state.registry.at(channel.chunk_type())==composite::Declaration::integer(channel.chunk_type()),
        "archive history channel type must be declared before adding it to an existing declaration registry");
    const auto bytes=encode_algebraic_archive(archive,limits);
    const auto encoded=algebraic_archive_history_detail::chunks(bytes);
    auto found=std::find_if(state.values.begin(),state.values.end(),[&](const auto& v){return v.key==channel.value_key;});
    if (found==state.values.end()) state.values.push_back({channel.value_key,channel.type(),0,0,{},{},encoded});
    else {
        require(found->type==channel.type() && found->block==0 && found->branch==0 && !found->cell && !found->occurrence,
            "archive history channel conflicts with a different typed value binding");
        found->value=encoded;
    }
    validate_temporal_endpoint(target); return target;
}

// The caller selects the endpoint/sequence. This function cannot inspect a later
// series tail, infer a clock, or use future records to reconstruct a prefix.
inline AlgebraicArchive read_algebraic_archive(const TemporalEndpoint& endpoint,AlgebraicArchiveChannel channel,
    const AlgebraicArchiveLimits& limits={}) {
    using algebraic_archive_detail::require;
    algebraic_archive_history_detail::validate_channel(channel); validate_temporal_endpoint(endpoint);
    require(endpoint.declarations && endpoint.declarations->namespace_key==channel.namespace_key,"archive history declaration namespace absent");
    const auto& state=*endpoint.declarations;
    require(state.registry.at(channel.type())==algebraic_archive_history_detail::sequence_declaration(channel) &&
        state.registry.at(channel.chunk_type())==composite::Declaration::integer(channel.chunk_type()),"archive history channel type differs");
    const auto found=std::find_if(state.values.begin(),state.values.end(),[&](const auto& v){return v.key==channel.value_key;});
    require(found!=state.values.end() && found->type==channel.type() && found->block==0 && found->branch==0 && !found->cell && !found->occurrence,
        "archive history channel value absent or binding differs");
    auto archive=decode_algebraic_archive(algebraic_archive_history_detail::concatenate(found->value,limits),limits);
    require(encode_rfcx(endpoint.source).bytes==encode_rfcx(archive.bundle().source()).bytes,"archive channel source differs from endpoint");
    return archive;
}
} // namespace rfc::storage
