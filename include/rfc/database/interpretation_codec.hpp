#pragma once

#include "rfc/database/native_interpretation.hpp"

namespace rfc::database::interpretation_codec_detail {
using storage::temporal_codec_detail::Writer;
using storage::temporal_codec_detail::Reader;
using interpretation_detail::require;
inline constexpr std::array<std::byte,8> snapshot_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'I'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte,8> update_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'I'},std::byte{'U'},std::byte{1},std::byte{0}};
inline void origin(Writer& w,const ExternalOrigin& o){w.u64(o.kind);storage::temporal_codec_detail::string(w,o.token);}
inline ExternalOrigin origin(Reader& r){const auto kind=r.u64();auto token=r.string();return {kind,std::move(token)};}
inline void declaration(Writer& w,composite::DeclarationId d){w.u64(d.name_space);w.u64(d.key);w.u64(d.version);}
inline composite::DeclarationId declaration(Reader& r){const auto ns=r.u64(),key=r.u64(),version=r.u64();return {ns,key,version};}
inline void lineage(Writer& w,const InterpretationLineage& l) {
    w.u64(l.subjects.size());for(const auto& x:l.subjects){origin(w,x.origin);w.u64(x.native_key);}
    w.u64(l.assertions.size());for(const auto& x:l.assertions){origin(w,x.origin);w.u64(x.native_key);}
    w.u64(l.occurrences.size());for(const auto& x:l.occurrences){origin(w,x.origin.assertion);storage::temporal_codec_detail::string(w,x.origin.lineage);w.u64(x.native_key);}
}
inline InterpretationLineage lineage(Reader& r){
    InterpretationLineage out;auto n=r.count(24);
    out.subjects.reserve(n);for(std::size_t i=0;i<n;++i){auto o=origin(r);const auto key=r.u64();out.subjects.push_back({std::move(o),key});}
    n=r.count(24);out.assertions.reserve(n);for(std::size_t i=0;i<n;++i){auto o=origin(r);const auto key=r.u64();out.assertions.push_back({std::move(o),key});}
    n=r.count(32);out.occurrences.reserve(n);for(std::size_t i=0;i<n;++i){auto o=origin(r);auto leg=r.string();const auto key=r.u64();out.occurrences.push_back({{std::move(o),std::move(leg)},key});}return out;
}
// Validate the unbound ledger before writer allocation as well as after reading.
// Kind membership and endpoint attachment are checked again when bound/applied.
inline void ledger_shape(const InterpretationLineage& ledger,const InterpretationLimits& limits,
                         std::size_t& records,std::size_t& metadata) {
    using namespace interpretation_detail;
    std::set<ExternalOrigin> owners;
    const auto visit=[&](const auto& entries,bool assertions) {
        add(records,entries.size(),limits.max_records,"wire lineage record budget");
        std::optional<ExternalOrigin> previous;std::set<std::uint64_t> keys;
        for(const auto& e:entries) {
            interpretation_detail::origin(e.origin,metadata,limits);
            require(!previous || *previous<e.origin,"noncanonical lineage order");previous=e.origin;
            require(e.native_key && e.native_key!=std::numeric_limits<std::uint64_t>::max() && keys.insert(e.native_key).second,"invalid/duplicate lineage key");
            if(assertions)owners.insert(e.origin);
        }
    };
    visit(ledger.subjects,false);visit(ledger.assertions,true);
    add(records,ledger.occurrences.size(),limits.max_records,"wire occurrence record budget");
    std::optional<OccurrenceOrigin> previous;std::set<std::uint64_t> keys;
    for(const auto& e:ledger.occurrences) {
        interpretation_detail::origin(e.origin.assertion,metadata,limits);token(e.origin.lineage,metadata,limits);
        require(owners.contains(e.origin.assertion),"unowned occurrence lineage");
        require(!previous || *previous<e.origin,"noncanonical occurrence order");previous=e.origin;
        require(e.native_key && e.native_key!=std::numeric_limits<std::uint64_t>::max() && keys.insert(e.native_key).second,"invalid/duplicate occurrence key");
    }
}
inline void update_shape(const InterpretationUpdate& update,const InterpretationLimits& limits) {
    require(update.source_namespace && update.binding_namespace && update.version,"unbound interpretation update");
    std::size_t records=0,metadata=0;
    interpretation_detail::token(update.base_revision,metadata,limits);interpretation_detail::token(update.target_revision,metadata,limits);
    require(update.base_revision!=update.target_revision,"interpretation update reuses snapshot revision");
    ledger_shape(update.base,limits,records,metadata);ledger_shape(update.target,limits,records,metadata);
    const auto extends=[](const auto& old,const auto& next) {
        for(const auto& e:old) {
            const auto it=std::lower_bound(next.begin(),next.end(),e.origin,[](const auto& x,const auto& o){return x.origin<o;});
            require(it!=next.end() && *it==e,"lineage deletion/reassignment");
        }
    };
    extends(update.base.subjects,update.target.subjects);extends(update.base.assertions,update.target.assertions);extends(update.base.occurrences,update.target.occurrences);
}
inline void schema(Writer& w,const InterpretationSchema& s){
    // The registry is already owned by the endpoint; do not serialize it twice.
    w.u64(s.source_namespace);w.u64(s.binding_namespace);w.u64(s.version);
    w.u64(s.subjects.size());for(const auto& k:s.subjects){w.u64(k.key);storage::temporal_codec_detail::string(w,k.label);storage::temporal_codec_detail::string(w,k.identity_label);storage::temporal_codec_detail::string(w,k.identity_encoding);}
    w.u64(s.assertions.size());for(const auto& k:s.assertions){w.u64(k.key);storage::temporal_codec_detail::string(w,k.label);declaration(w,k.payload);
        w.u64(k.roles.size());for(const auto& role:k.roles){w.u64(role.key);w.u64(role.participant_kind);w.u64(role.minimum);w.u64(role.maximum);w.u8(role.allow_missing);w.u8(role.allow_null);}
        w.u64(k.fields.size());for(const auto& f:k.fields){w.u64(f.factor);storage::temporal_codec_detail::string(w,f.label);}
        storage::temporal_codec_detail::string(w,k.lineage_label);storage::temporal_codec_detail::string(w,k.lineage_encoding);
        w.u64(k.reference_labels.size());for(const auto& f:k.reference_labels){w.u64(f.factor);storage::temporal_codec_detail::string(w,f.label);}}
}
inline InterpretationSchema schema(Reader& r){
    InterpretationSchema s;s.source_namespace=r.u64();s.binding_namespace=r.u64();s.version=r.u64();
    auto n=r.count(32);s.subjects.reserve(n);for(std::size_t i=0;i<n;++i){const auto key=r.u64();auto label=r.string();auto identity=r.string();auto encoding=r.string();s.subjects.push_back({key,std::move(label),std::move(identity),std::move(encoding)});}
    n=r.count(56);s.assertions.reserve(n);for(std::size_t i=0;i<n;++i){ExternalAssertionKind k;k.key=r.u64();k.label=r.string();k.payload=declaration(r);
        auto m=r.count(34);k.roles.reserve(m);for(std::size_t j=0;j<m;++j){ExternalRole x;x.key=r.u64();x.participant_kind=r.u64();x.minimum=machine::checked_size(r.u64());x.maximum=machine::checked_size(r.u64());x.allow_missing=r.flag();x.allow_null=r.flag();k.roles.push_back(x);}
        m=r.count(16);k.fields.reserve(m);for(std::size_t j=0;j<m;++j){const auto key=r.u64();auto label=r.string();k.fields.push_back({key,std::move(label)});}
        k.lineage_label=r.string();k.lineage_encoding=r.string();m=r.count(16);for(std::size_t j=0;j<m;++j){const auto key=r.u64();auto label=r.string();k.reference_labels.push_back({key,std::move(label)});}
        s.assertions.push_back(std::move(k));}return s;
}
inline void blob(Writer& w,std::span<const std::byte> bytes,const InterpretationLimits& limits){
    interpretation_detail::bound(machine::checked_size_add(w.size(),machine::checked_size_add(8,bytes.size())),limits.max_archive_bytes,"interpretation archive byte budget");
    w.u64(bytes.size());w.raw(bytes);
}
inline Writer header(const std::array<std::byte,8>& magic){Writer w;w.raw(magic);w.u32(1);w.u32(0);w.u64(0);w.u64(0);return w;}
inline std::vector<std::byte> finish(Writer& w,const InterpretationLimits& limits){
    interpretation_detail::bound(w.size(),limits.max_archive_bytes,"interpretation archive byte budget");w.patch_u64(16,w.size());
    w.patch_u64(24,storage::rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));return w.bytes();
}
inline std::span<const std::byte> payload(std::span<const std::byte> bytes,const std::array<std::byte,8>& magic,const InterpretationLimits& limits){
    require(bytes.size()>=32 && bytes.size()<=limits.max_archive_bytes,"interpretation archive size");storage::rfcx_detail::Reader r(bytes);
    require(std::equal(magic.begin(),magic.end(),r.slice(8).begin()),"interpretation archive magic");
    require(r.u32()==1 && r.u32()==0,"unsupported interpretation version/features");require(r.u64()==bytes.size(),"interpretation archive length");
    require(r.u64()==storage::rfcx_detail::fnv1a64(bytes.subspan(32)),"interpretation archive integrity");return bytes.subspan(32);
}
inline storage::TemporalReadLimits policy(const InterpretationLimits& limits){
    auto p=limits.atlas.temporal;p.require_canonical=true;p.max_file_bytes=std::min(p.max_file_bytes,limits.max_archive_bytes);
    p.max_records=std::min(p.max_records,limits.max_records);p.declarations=limits.declarations;return p;
}
inline void write(const std::filesystem::path& path,std::span<const std::byte> bytes){
    storage::write_file_exact(path,bytes,storage::FilePublication::CreateNew);
}
} // namespace rfc::database::interpretation_codec_detail

namespace rfc::database {
inline std::vector<std::byte> encode_interpreted_snapshot(const InterpretedSnapshot& snapshot){
    using namespace interpretation_codec_detail;auto w=header(snapshot_magic);
    schema(w,snapshot.schema());storage::temporal_codec_detail::string(w,snapshot.revision());lineage(w,snapshot.lineage());const auto e=snapshot.endpoint();
    {const auto image=storage::encode_rfcx(e->source);blob(w,image.bytes,snapshot.limits());}
    {const auto bytes=storage::encode_temporal_endpoint(*e);blob(w,bytes,snapshot.limits());}
    return finish(w,snapshot.limits());
}
inline InterpretedSnapshot decode_interpreted_snapshot(std::span<const std::byte> bytes,const InterpretationLimits& limits={}){
    using namespace interpretation_codec_detail;const auto p=policy(limits);Reader r(payload(bytes,snapshot_magic,limits),p);
    auto s=schema(r);const auto revision=r.string();auto ledger=lineage(r);const auto source_bytes=r.blob();const auto sidecar=r.blob();require(r.remaining()==0,"trailing interpretation bytes");
    // Check metadata before allocating the retained source. Types are completed
    // below from the single canonical endpoint registry.
    std::size_t metadata=0;for(const auto& x:ledger.subjects)interpretation_detail::origin(x.origin,metadata,limits);
    for(const auto& x:ledger.assertions)interpretation_detail::origin(x.origin,metadata,limits);
    for(const auto& x:ledger.occurrences){interpretation_detail::origin(x.origin.assertion,metadata,limits);interpretation_detail::token(x.origin.lineage,metadata,limits);}
    storage::MappedRfcx image(source_bytes);std::size_t records=0;
    for(std::size_t i=0;i<image.grade_count();++i){const auto g=image.grade(core::Grade{static_cast<std::uint16_t>(i)});
        interpretation_detail::add(records,machine::checked_size_add(g.cell_count(),g.incidence_count()),limits.max_records,"interpretation source budget");}
    auto endpoint=storage::decode_temporal_endpoint(image.materialize(),sidecar,p);require(endpoint.declarations.has_value(),"interpretation lacks owning declarations");
    std::vector<composite::Declaration> definitions;
    for(const auto& d:endpoint.declarations->registry.definitions())if(d.id.name_space!=s.binding_namespace)definitions.push_back(d);
    s.values=composite::DeclarationRegistry(std::move(definitions),limits.declarations);
    auto out=InterpretedSnapshot::restore(s,ledger,endpoint,revision,limits);
    const auto canonical=encode_interpreted_snapshot(out);require(canonical.size()==bytes.size() && std::equal(canonical.begin(),canonical.end(),bytes.begin()),"noncanonical interpretation archive");return out;
}
inline std::vector<std::byte> encode_interpretation_update(const InterpretationUpdate& update,const InterpretationLimits& limits={}){
    using namespace interpretation_codec_detail;update_shape(update,limits);auto w=header(update_magic);
    w.u64(update.source_namespace);w.u64(update.binding_namespace);w.u64(update.version);storage::temporal_codec_detail::string(w,update.base_revision);storage::temporal_codec_detail::string(w,update.target_revision);lineage(w,update.base);lineage(w,update.target);
    const auto segment=storage::encode_temporal_segment(update.segment);blob(w,segment,limits);return finish(w,limits);
}
inline InterpretationUpdate decode_interpretation_update(std::span<const std::byte> bytes,const InterpretationLimits& limits={}){
    using namespace interpretation_codec_detail;const auto p=policy(limits);Reader r(payload(bytes,update_magic,limits),p);InterpretationUpdate out;
    out.source_namespace=r.u64();out.binding_namespace=r.u64();out.version=r.u64();out.base_revision=r.string();out.target_revision=r.string();out.base=lineage(r);out.target=lineage(r);update_shape(out,limits);
    out.segment=storage::decode_temporal_segment(r.blob(),p);require(r.remaining()==0,"trailing interpretation update bytes");
    const auto canonical=encode_interpretation_update(out,limits);require(canonical.size()==bytes.size() && std::equal(canonical.begin(),canonical.end(),bytes.begin()),"noncanonical interpretation update");return out;
}
inline void write_interpreted_snapshot(const InterpretedSnapshot& snapshot,const std::filesystem::path& path){interpretation_codec_detail::write(path,encode_interpreted_snapshot(snapshot));}
inline InterpretedSnapshot read_interpreted_snapshot(const std::filesystem::path& path,const InterpretationLimits& limits={}){
    storage::validate_file_path(path);
    interpretation_detail::bound(std::filesystem::file_size(path),limits.max_archive_bytes,"interpretation input byte budget");
    storage::ReadOnlyMappedFile file(path);
    interpretation_detail::bound(file.size(),limits.max_archive_bytes,"opened interpretation input byte budget");
    return decode_interpreted_snapshot(file.bytes(),limits);
}
} // namespace rfc::database
