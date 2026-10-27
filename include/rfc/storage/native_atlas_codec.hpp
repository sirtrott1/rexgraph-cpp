#pragma once

#include "rfc/storage/native_atlas.hpp"
#include <fstream>

namespace rfc::storage::native_atlas_codec_detail {
using temporal_codec_detail::Reader;
using temporal_codec_detail::Writer;
using temporal_source_detail::require;
inline constexpr std::array<std::byte,8> magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'A'},std::byte{1},std::byte{0},std::byte{0}};
inline void blob(Writer& w,std::span<const std::byte> value) {w.u64(value.size());w.raw(value);}
inline void queries(Writer& w,const std::vector<RelationalCodeObservationQuery>& qs) {
    w.u64(qs.size());for(const auto& q:qs){w.u64(q.key);w.u8(static_cast<std::uint8_t>(q.kind));w.u64(q.operation);
        temporal_codec_detail::optional_key(w,q.field);temporal_codec_detail::keys(w,q.subjects);w.u64(q.work_budget);}
}
inline std::vector<RelationalCodeObservationQuery> queries(Reader& r) {
    const auto n=r.count(34);std::vector<RelationalCodeObservationQuery> out;
    for(std::size_t i=0;i<n;++i){RelationalCodeObservationQuery q;q.key=r.u64();q.kind=static_cast<RelationalCodeObservationKind>(r.u8());
        q.operation=r.u64();q.field=temporal_codec_detail::optional_key(r);q.subjects=temporal_codec_detail::keys(r);const auto work=r.u64();require(work<=std::numeric_limits<std::size_t>::max(),"query work width");q.work_budget=static_cast<std::size_t>(work);out.push_back(std::move(q));}
    return out;
}
inline void declaration(Writer& w,composite::DeclarationId id){w.u64(id.name_space);w.u64(id.key);w.u64(id.version);}
inline composite::DeclarationId declaration(Reader& r){const auto ns=r.u64(),key=r.u64(),version=r.u64();return {ns,key,version};}
inline void migration(Writer& w,const std::optional<SourceDeclarationMigration>& m){w.u8(m.has_value());if(m)temporal_codec_detail::source_migration(w,*m);}
inline std::optional<SourceDeclarationMigration> migration(Reader& r){if(r.flag())return temporal_codec_detail::source_migration(r);return std::nullopt;}
inline void columns(Writer& w,const std::vector<NativeChainColumn>& cs){
    w.u64(cs.size());for(const auto& c:cs){temporal_codec_detail::cell(w,c.source);w.u64(c.image.size());
        for(const auto& t:c.image){temporal_codec_detail::cell(w,t.target);temporal_codec_detail::rational(w,t.coefficient);}}
}
inline std::vector<NativeChainColumn> columns(Reader& r){
    const auto n=r.count(18);std::vector<NativeChainColumn> out;out.reserve(n);
    for(std::size_t i=0;i<n;++i){NativeChainColumn c;c.source=temporal_codec_detail::cell(r);const auto count=r.count(20);c.image.reserve(count);
        for(std::size_t j=0;j<count;++j){const auto key=temporal_codec_detail::cell(r);auto q=r.rational();c.image.push_back({key,std::move(q)});}out.push_back(std::move(c));}return out;
}
inline void manifest(Writer& w,const NativeAtlasManifest& m){
    w.u64(m.key);w.u64(m.revision);w.u64(m.embeddings.size());
    for(const auto& e:m.embeddings){w.u64(e.key);w.u64(e.from);w.u64(e.to);migration(w,e.declarations);
        w.u64(e.cells.size());for(const auto& p:e.cells){temporal_codec_detail::cell(w,p.from);temporal_codec_detail::cell(w,p.to);}
        w.u64(e.occurrences.size());for(const auto& p:e.occurrences){temporal_codec_detail::occurrence(w,p.from);temporal_codec_detail::occurrence(w,p.to);}}
    w.u64(m.pushouts.size());for(const auto& p:m.pushouts){w.u64(p.key);w.u64(p.left_embedding);w.u64(p.right_embedding);migration(w,p.left_common);migration(w,p.right_common);}
    w.u64(m.products.size());for(const auto& p:m.products){w.u64(p.key);w.u64(p.left_source);w.u64(p.right_source);w.u64(p.left_factor);w.u64(p.right_factor);}
    w.u64(m.scale_maps.size());for(const auto& h:m.scale_maps){w.u64(h.key);w.u64(h.from);w.u64(h.to);columns(w,h.realization);w.u8(h.retraction.has_value());if(h.retraction)columns(w,*h.retraction);}
    w.u64(m.scale_paths.size());for(const auto& p:m.scale_paths){w.u64(p.key);temporal_codec_detail::keys(w,p.left);temporal_codec_detail::keys(w,p.right);}
    w.u64(m.linear_interfaces.size());for(const auto& f:m.linear_interfaces){w.u64(f.key);w.u64(f.field_identity);w.u64(f.common_source);declaration(w,f.common_type);
        w.u64(f.legs.size());for(const auto& l:f.legs){w.u64(l.source);w.u64(l.field);temporal_codec_detail::matrix(w,l.restriction);temporal_codec_detail::optional_key(w,l.operation);}
        w.u8(f.common_operation.has_value());if(f.common_operation)temporal_codec_detail::matrix(w,*f.common_operation);}
    w.u64(m.finite_interfaces.size());for(const auto& f:m.finite_interfaces){w.u64(f.key);w.u64(f.field_identity);w.u64(f.common_source);declaration(w,f.common_type);
        w.u64(f.legs.size());for(const auto& l:f.legs){w.u64(l.source);w.u64(l.value);w.u64(l.restriction.size());for(const auto& [a,b]:l.restriction){w.u64(a);w.u64(b);}}}
}
inline NativeAtlasManifest manifest(Reader& r){
    NativeAtlasManifest m;m.key=r.u64();m.revision=r.u64();
    auto n=r.count(41);for(std::size_t i=0;i<n;++i){NativeSourceEmbedding e;e.key=r.u64();e.from=r.u64();e.to=r.u64();e.declarations=migration(r);
        auto c=r.count(20);e.cells.reserve(c);for(std::size_t j=0;j<c;++j){const auto from=temporal_codec_detail::cell(r),to=temporal_codec_detail::cell(r);e.cells.push_back({from,to});}
        c=r.count(20);e.occurrences.reserve(c);for(std::size_t j=0;j<c;++j){const auto from=temporal_codec_detail::occurrence(r),to=temporal_codec_detail::occurrence(r);e.occurrences.push_back({from,to});}
        m.embeddings.push_back(std::move(e));}
    n=r.count(26);for(std::size_t i=0;i<n;++i){NativePushoutDeclaration p;p.key=r.u64();p.left_embedding=r.u64();p.right_embedding=r.u64();p.left_common=migration(r);p.right_common=migration(r);m.pushouts.push_back(std::move(p));}
    n=r.count(40);for(std::size_t i=0;i<n;++i){NativeProductManifest p;p.key=r.u64();p.left_source=r.u64();p.right_source=r.u64();p.left_factor=r.u64();p.right_factor=r.u64();m.products.push_back(p);}
    n=r.count(33);for(std::size_t i=0;i<n;++i){NativeScaleMap h;h.key=r.u64();h.from=r.u64();h.to=r.u64();h.realization=columns(r);if(r.flag())h.retraction=columns(r);m.scale_maps.push_back(std::move(h));}
    n=r.count(24);for(std::size_t i=0;i<n;++i){NativeScalePathEquality p;p.key=r.u64();p.left=temporal_codec_detail::keys(r);p.right=temporal_codec_detail::keys(r);m.scale_paths.push_back(std::move(p));}
    n=r.count(57);for(std::size_t i=0;i<n;++i){NativeLinearInterface f;f.key=r.u64();f.field_identity=r.u64();f.common_source=r.u64();f.common_type=declaration(r);
        const auto count=r.count(41);for(std::size_t j=0;j<count;++j){NativeLinearInterfaceLeg l;l.source=r.u64();l.field=r.u64();l.restriction=temporal_codec_detail::matrix(r);l.operation=temporal_codec_detail::optional_key(r);f.legs.push_back(std::move(l));}
        if(r.flag())f.common_operation=temporal_codec_detail::matrix(r);
        m.linear_interfaces.push_back(std::move(f));}
    n=r.count(56);for(std::size_t i=0;i<n;++i){NativeFiniteInterface f;f.key=r.u64();f.field_identity=r.u64();f.common_source=r.u64();f.common_type=declaration(r);
        const auto count=r.count(24);for(std::size_t j=0;j<count;++j){NativeFiniteInterfaceLeg l;l.source=r.u64();l.value=r.u64();const auto k=r.count(16);
            for(std::size_t z=0;z<k;++z){const auto a=r.u64(),b=r.u64();l.restriction.emplace_back(a,b);}f.legs.push_back(std::move(l));}m.finite_interfaces.push_back(std::move(f));}
    return m;
}
} // namespace rfc::storage::native_atlas_codec_detail

namespace rfc::storage {
// Standalone complete atlas snapshot, NOT a durable checkpoint or transaction
// service. Each source occurs once as canonical RFCX + its existing RFCXE.
// Only reference/manifold data follows: no product Cartesian array, copied
// local bitplanes or equal-value identity joins.
inline std::vector<std::byte> encode_native_atlas(const NativeAtlas& atlas){
    using namespace native_atlas_codec_detail;Writer w;w.raw(magic);w.u32(1);w.u32(0);w.u64(0);w.u64(0);
    const auto sources=atlas.sources();w.u64(sources.size());const auto limits=atlas.limits();
    for(const auto& s:sources){w.u64(s.key());w.u64(s.name_space());queries(w,s.queries());const auto endpoint=s.endpoint();
        {const auto image=encode_rfcx(endpoint->source);native_atlas_detail::bound(machine::checked_size_add(w.size(),machine::checked_size_add(image.bytes.size(),8)),limits.max_archive_bytes,"atlas archive byte budget");blob(w,image.bytes);}
        {const auto bytes=encode_temporal_endpoint(*endpoint);native_atlas_detail::bound(machine::checked_size_add(w.size(),machine::checked_size_add(bytes.size(),8)),limits.max_archive_bytes,"atlas archive byte budget");blob(w,bytes);}
    }
    manifest(w,atlas.manifest());native_atlas_detail::bound(w.size(),limits.max_archive_bytes,"atlas archive byte budget");
    w.patch_u64(16,w.size());w.patch_u64(24,rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));return w.bytes();
}
inline NativeAtlas decode_native_atlas(std::span<const std::byte> bytes,const NativeAtlasLimits& limits={}){
    using namespace native_atlas_codec_detail;
    require(bytes.size()>=40 && bytes.size()<=limits.max_archive_bytes,"invalid atlas archive size");
    rfcx_detail::Reader header(bytes);require(std::equal(magic.begin(),magic.end(),header.slice(8).begin()),"atlas magic");
    require(header.u32()==1 && header.u32()==0,"unsupported atlas version/features");require(header.u64()==bytes.size(),"atlas archive length");
    require(header.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"atlas integrity mismatch");
    auto policy=limits.temporal;policy.require_canonical=true;policy.max_file_bytes=std::min(policy.max_file_bytes,limits.max_archive_bytes);
    policy.max_records=std::min(policy.max_records,limits.max_records);policy.max_dimension=std::min(policy.max_dimension,limits.max_dimension);
    policy.max_integer_bytes=std::min(policy.max_integer_bytes,limits.max_integer_bytes);policy.source_declarations=limits.source_declarations;
    Reader r(bytes.subspan(32),policy);const auto n=r.count(32);native_atlas_detail::bound(n,limits.max_sources,"atlas source count");
    std::vector<NativeAtlasSource> sources;sources.reserve(n);std::size_t source_records=0;
    for(std::size_t i=0;i<n;++i){const auto key=r.u64(),ns=r.u64();auto qs=queries(r);const auto source_bytes=r.blob();
        MappedRfcx image(source_bytes);for(std::size_t k=0;k<image.grade_count();++k){const auto g=image.grade(core::Grade{static_cast<std::uint16_t>(k)});
            const auto records=machine::checked_size_add(g.cell_count(),g.incidence_count());source_records=machine::checked_size_add(source_records,records);
            native_atlas_detail::bound(source_records,limits.max_records,"atlas aggregate retained-source records");r.consume_records(records);}
        auto source=image.materialize();const auto sidecar=r.blob();auto endpoint=decode_temporal_endpoint(std::move(source),sidecar,policy);
        sources.emplace_back(key,ns,endpoint,limits.accession,std::move(qs),limits.observations);
    }
    auto m=manifest(r);require(r.remaining()==0,"atlas trailing bytes");NativeAtlas out(sources,m,limits);
    // This checks full nested source/sidecar/declaration/map bytes, not only FNV.
    // Checksums are error detection, NOT authentication of malicious input.
    const auto canonical=encode_native_atlas(out);require(canonical.size()==bytes.size() && std::equal(canonical.begin(),canonical.end(),bytes.begin()),"noncanonical atlas archive");return out;
}
// Unbound declaration blueprint: source keys are parameters, NOT endpoint
// witnesses. Construct/advance NativeAtlas to bind and validate these records.
inline std::vector<std::byte> encode_native_atlas_manifest(const NativeAtlasManifest& input,const NativeAtlasLimits& limits={}) {
    using namespace native_atlas_codec_detail;
    auto m=input;native_atlas_detail::normalize(m,limits);
    Writer w;const std::array<std::byte,8> tag{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'A'},std::byte{'M'},std::byte{1},std::byte{0}};
    w.raw(tag);w.u32(1);w.u32(0);w.u64(0);w.u64(0);manifest(w,m);
    native_atlas_detail::bound(w.size(),limits.max_archive_bytes,"atlas manifest byte budget");
    w.patch_u64(16,w.size());w.patch_u64(24,rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));return w.bytes();
}
inline NativeAtlasManifest decode_native_atlas_manifest(std::span<const std::byte> bytes,const NativeAtlasLimits& limits={}) {
    using namespace native_atlas_codec_detail;
    const std::array<std::byte,8> tag{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'A'},std::byte{'M'},std::byte{1},std::byte{0}};
    require(bytes.size()>=32 && bytes.size()<=limits.max_archive_bytes,"atlas manifest size");rfcx_detail::Reader head(bytes);
    require(std::equal(tag.begin(),tag.end(),head.slice(8).begin()),"atlas manifest magic");
    require(head.u32()==1 && head.u32()==0,"unsupported atlas manifest version/features");require(head.u64()==bytes.size(),"atlas manifest length");
    require(head.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"atlas manifest integrity");
    auto policy=limits.temporal;policy.max_records=std::min(policy.max_records,limits.max_records);
    policy.max_dimension=std::min(policy.max_dimension,limits.max_dimension);policy.max_integer_bytes=std::min(policy.max_integer_bytes,limits.max_integer_bytes);
    Reader r(bytes.subspan(32),policy);auto m=manifest(r);require(r.remaining()==0,"atlas manifest trailing bytes");
    const auto canonical=encode_native_atlas_manifest(m,limits);require(canonical.size()==bytes.size() && std::equal(canonical.begin(),canonical.end(),bytes.begin()),"noncanonical atlas declaration manifest");return m;
}
inline void write_native_atlas(const NativeAtlas& atlas,const std::filesystem::path& path){
    const auto bytes=encode_native_atlas(atlas);
    write_file_exact(path, bytes, FilePublication::CreateNew);
}
inline NativeAtlas read_native_atlas(const std::filesystem::path& path,const NativeAtlasLimits& limits={}){
    const auto bytes=read_file_exact(path,limits.max_archive_bytes);
    return decode_native_atlas(bytes,limits);
}
} // namespace rfc::storage
