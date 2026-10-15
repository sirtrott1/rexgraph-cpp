#pragma once

#include "rfc/inference/relational_initial_input_lineage.hpp"

namespace rfc::inference {
namespace initial_input_lineage_codec_detail {
using namespace field_interface_codec_detail;
inline constexpr std::array<std::byte,8> magic{
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'K'},std::byte{'J'},std::byte{1},std::byte{0},std::byte{0}};
struct PassiveLineage {
    std::vector<std::byte> original,initial;
    RelationalInitialInputLineageDeclaration declaration;
    RelationalInitialInputLineageOrigins keys;
    std::vector<OwnedRelationalValueOrigin> obligations;
};
inline PassiveLineage preflight(std::span<const std::byte> bytes,const RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,const std::function<void()>& check={}) {
    Reader r(bytes,policy);header(r,bytes,magic);const auto old=r.blob(),next=r.blob();
    RelationalInitialInputLineageDeclaration d;d.key=r.u64();d.revision=r.u64();const auto n=r.count(16);
    for(std::size_t i=0;i<n;++i)d.uses.push_back({r.u64(),r.u64()});
    const RelationalInitialInputLineageOrigins keys{r.u64(),r.u64(),r.u64()};
    PassiveLineage out{{old.begin(),old.end()},{next.begin(),next.end()},std::move(d),keys,{}};
    const std::array<std::pair<std::uint64_t,RelationalDependencyRole>,3> roles{{
        {keys.original,RelationalDependencyRole::InputAsset},{keys.initial,RelationalDependencyRole::InputAsset},{keys.interface,RelationalDependencyRole::Interface}}};
    const std::array<std::vector<std::byte>,3> payloads{out.original,out.initial,
        initial_input_lineage_detail::payload(out.original,out.initial,out.declaration)};
    for(std::size_t i=0;i<3;++i){if(check)check();const auto receipt=r.blob();const auto actual=context.require(roles[i].first,roles[i].second,payloads[i]);
        canonical(receipt,actual.receipt());out.obligations.push_back(actual);}
    field_program_detail::require(!r.remaining(),"trailing initial lineage bytes");return out;
}
} // namespace initial_input_lineage_codec_detail
inline std::vector<std::byte> encode_available_relational_initial_input_lineage(const AvailableRelationalInitialInputLineage& lineage) {
    using namespace initial_input_lineage_codec_detail;
    const auto pin=lineage;const auto l=pin.lineage();const auto& d=l.declaration();auto w=envelope(magic);
    blob(w,encode_relational_input_use_asset(l.original()));blob(w,encode_relational_input_use_asset(l.initial()));
    w.u64(d.key);w.u64(d.revision);w.u64(d.uses.size());for(const auto u:d.uses){w.u64(u.original);w.u64(u.initial);}
    const auto keys=pin.origins();w.u64(keys.original);w.u64(keys.initial);w.u64(keys.interface);
    for(const auto& origin:pin.obligations())blob(w,origin.receipt());
    return finish(std::move(w));
}
inline AvailableRelationalInitialInputLineage decode_available_relational_initial_input_lineage(std::span<const std::byte> bytes,
    const RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},
    std::function<void()> check={},std::optional<OwnedRelationalInputUseAsset> declared_initial={}) {
    field_program_detail::require(bytes.size()<=policy.max_file_bytes,"initial lineage byte cap");
    const std::vector<std::byte> frozen(bytes.begin(),bytes.end());const auto authority=context;const auto callback=std::move(check);if(callback)callback();
    const auto parsed=initial_input_lineage_codec_detail::preflight(frozen,authority,policy,callback);
    for(const auto& origin:parsed.obligations){if(callback)callback();authority.recheck(origin);}
    auto original=decode_relational_input_use_asset(parsed.original,policy,limits);
    if(declared_initial)field_program_codec_detail::canonical(parsed.initial,encode_relational_input_use_asset(*declared_initial));
    auto initial=declared_initial?*declared_initial:decode_relational_input_use_asset(parsed.initial,policy,limits);
    const OwnedRelationalInitialInputLineage owner(std::move(original),std::move(initial),parsed.declaration,limits,callback);
    AvailableRelationalInitialInputLineage out(owner,parsed.keys,authority);
    field_program_codec_detail::canonical(frozen,encode_available_relational_initial_input_lineage(out));return out;
}
} // namespace rfc::inference
