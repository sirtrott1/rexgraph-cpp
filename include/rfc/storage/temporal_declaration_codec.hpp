#pragma once

#include "rfc/storage/temporal_codec_primitives.hpp"

namespace rfc::storage::temporal_codec_detail {
inline constexpr std::uint32_t declarations_flag = 8u;
inline constexpr std::uint32_t source_migration_flag = 16u;
inline void declaration_id(Writer& w, composite::DeclarationId id) {
    require(id.valid(), "invalid wire declaration identity");
    w.u64(id.name_space); w.u64(id.key); w.u64(id.version);
}
inline composite::DeclarationId declaration_id(Reader& r) {
    const auto ns = r.u64(), key = r.u64(), version = r.u64();
    const composite::DeclarationId id{ns,key,version}; require(id.valid(), "zero wire declaration identity"); return id;
}
inline void declaration_blob(Writer& w, std::span<const std::byte> bytes) { w.u64(bytes.size()); w.raw(bytes); }
inline void declared_payload(Writer& w, const composite::CompiledLayout& layout, const composite::DeclaredValue& v) {
    encode_declared_bits_to(layout,v,[&](std::size_t bits,std::size_t bytes) {
        w.u64(bits); w.u64(bytes); return w.append_zeroed(bytes);
    });
}
inline composite::DeclaredValue declared_payload(Reader& r, const composite::CompiledLayout& layout) {
    const auto count = machine::checked_size(r.u64()); const auto bytes = r.blob();
    return decode_declared_bits(layout,bytes,count);
}
inline void branch_declaration(Writer& w, const TemporalBranchDeclaration& b) {
    w.u64(b.key); string(w,b.source_type); const auto& d = b.declaration;
    declaration_id(w,d.id); declaration_id(w,d.payload); w.u8(static_cast<std::uint8_t>(d.population));
    w.u8(static_cast<std::uint8_t>(d.semantics)); w.u8(d.partition); w.u64(d.roles.size());
    for (const auto& r : d.roles) { w.u64(r.key); string(w,r.source_aspect); w.u64(r.min_count); w.u64(r.max_count); }
}
inline TemporalBranchDeclaration branch_declaration(Reader& r) {
    TemporalBranchDeclaration b; b.key=r.u64(); b.source_type=r.string(); auto& d=b.declaration;
    d.id=declaration_id(r); d.payload=declaration_id(r); d.population=static_cast<core::BranchPopulation>(r.u8());
    d.semantics=static_cast<core::BranchSemantics>(r.u8()); d.partition=r.flag();
    require(d.population<=core::BranchPopulation::Active && d.semantics<=core::BranchSemantics::NormalizedOneHead,"unknown branch declaration opcode");
    const auto n=r.count(32);
    for(std::size_t i=0;i<n;++i) {
        core::BranchRoleDeclaration x; x.key=r.u64(); x.source_aspect=r.string();
        x.min_count=machine::checked_size(r.u64()); x.max_count=machine::checked_size(r.u64()); d.roles.push_back(std::move(x));
    }
    return b;
}
inline void declaration_state(Writer& w, const TemporalDeclarationState& s) {
    temporal_declaration_detail::validate_definitions(s);
    w.u64(s.namespace_key);
    const composite::PreparedLayoutRegistry registry(s.registry,s.resource_limits);
    const composite::CompiledLayout root(registry,s.registry.definitions().front().id,s.profile);
    declaration_blob(w,encode_declarations(root));
    const auto values=temporal_declaration_detail::keyed(s.values); w.u64(values.size());
    std::optional<composite::CompiledLayout> value_layout;
    for(const auto& [key,p]:values) {
        w.u64(key); declaration_id(w,p->type); w.u64(p->block); w.u64(p->branch);
        w.u8(p->cell.has_value()); if(p->cell) cell(w,*p->cell);
        w.u8(p->occurrence.has_value()); if(p->occurrence) occurrence(w,*p->occurrence);
        if(!value_layout || value_layout->root()!=p->type)
            value_layout.emplace(registry,p->type,s.profile);
        declared_payload(w,*value_layout,p->value);
    }
    const auto branches=temporal_declaration_detail::keyed(s.branches); w.u64(branches.size());
    for(const auto& [key,b]:branches) { (void)key; branch_declaration(w,*b); }
    const auto layouts=temporal_declaration_detail::keyed(s.source_layouts); w.u64(layouts.size());
    for(const auto& [key,l]:layouts) {
        w.u64(key); declaration_id(w,l->type);
        auto fs=l->factors; std::sort(fs.begin(),fs.end(),[](const auto& a,const auto& b){return a.factor<b.factor;});
        w.u64(fs.size()); for(const auto& f:fs){w.u64(f.factor);string(w,f.source_aspect);}
    }
    const auto codes=temporal_declaration_detail::keyed(s.code_types); w.u64(codes.size());
    for (const auto& [key,c]:codes) {w.u64(key);declaration_id(w,c->type);}
    const auto programs=temporal_declaration_detail::keyed(s.programs);w.u64(programs.size());
    for(const auto& [key,p]:programs) {
        w.u64(key);w.u64(p->layout);w.u16(p->program.register_count());w.u16(p->output);w.u64(p->program.instruction_count());
        for(const auto& i:p->program.instructions()) {
            w.u8(static_cast<std::uint8_t>(i.opcode));w.u16(i.dst);w.u16(i.a);w.u16(i.b);w.u16(i.c);w.u16(i.aspect);
        }
    }
}
inline TemporalDeclarationState declaration_state(Reader& r) {
    TemporalDeclarationState s; s.resource_limits=r.limits().declarations; s.namespace_key=r.u64();
    const auto layout=decode_declarations(r.blob(),r.limits().declarations);
    s.registry=layout.declarations(); s.profile=layout.profile();
    r.consume_records(s.registry.definitions().size());
    auto n=r.count(58); std::size_t nodes=0,allocated=machine::checked_size_mul(n,sizeof(TemporalDeclaredValue));
    require(allocated<=r.limits().declarations.max_materialized_bytes,"declared root-value decode budget");
    s.values.reserve(n);
    // The registry is needed only for a nonempty value lane. Keep its shared
    // owner in a normal lexical lifetime instead of an optional nested payload.
    if(n != 0) {
        const composite::PreparedLayoutRegistry registry(s.registry,r.limits().declarations);
        std::optional<composite::CompiledLayout> value_layout;
        for(std::size_t i=0;i<n;++i) {
            TemporalDeclaredValue v; v.key=r.u64(); v.type=declaration_id(r); v.block=r.u64(); v.branch=r.u64();
            if(r.flag()) v.cell=cell(r);
            if(r.flag()) v.occurrence=occurrence(r);
            if(!value_layout || value_layout->root()!=v.type) {
                value_layout.emplace(registry,v.type,s.profile);
            }
            // Limit each subsequent allocation by the remaining aggregate allowance.
            auto policy=r.limits().declarations;
            require(nodes<policy.max_value_nodes && allocated<=policy.max_materialized_bytes,"aggregate declaration decode budget");
            policy.max_value_nodes-=nodes; policy.max_materialized_bytes-=allocated;
            const auto bit_count=machine::checked_size(r.u64());const auto bytes=r.blob();
            v.value=decode_declared_bits(*value_layout,bytes,bit_count,policy.max_value_nodes,policy.max_materialized_bytes);
            const auto use=value_layout->allocation(v.value);
            nodes=machine::checked_size_add(nodes,use.value_nodes); allocated=machine::checked_size_add(allocated,use.materialized_bytes);
            require(nodes<=r.limits().declarations.max_value_nodes && allocated<=r.limits().declarations.max_materialized_bytes,"aggregate declared value decode budget");
            s.values.push_back(std::move(v));
        }
    }
    n=r.count(59); for(std::size_t i=0;i<n;++i)s.branches.push_back(branch_declaration(r));
    n=r.count(40); for(std::size_t i=0;i<n;++i){
        TemporalSourceAspectLayout l; l.key=r.u64();l.type=declaration_id(r);const auto count=r.count(16);
        for(std::size_t j=0;j<count;++j){const auto key=r.u64();auto name=r.string();l.factors.push_back({key,std::move(name)});}
        s.source_layouts.push_back(std::move(l));
    }
    n=r.count(32);for(std::size_t i=0;i<n;++i){const auto key=r.u64();s.code_types.push_back({key,declaration_id(r)});}
    n=r.count(28);for(std::size_t j=0;j<n;++j) {
        TemporalSourceLayoutProgram p;p.key=r.u64();p.layout=r.u64();const auto registers=r.u16();p.output=r.u16();
        p.program=composite::BinaryProgram(registers);require(p.output<registers,"invalid declared program output");
        const auto count=r.count(11);
        require(count<=r.limits().declarations.max_value_nodes,"declared program instruction budget");
        for(std::size_t k=0;k<count;++k) {
            composite::BinaryInstruction i;i.opcode=static_cast<composite::BinaryOpcode>(r.u8());i.dst=r.u16();
            i.a=r.u16();i.b=r.u16();i.c=r.u16();i.aspect=r.u16();p.program.append(i);
        }
        s.programs.push_back(std::move(p));
    }
    temporal_declaration_detail::validate_definitions(s,r.limits().declarations); return s;
}
inline bool declaration_state_equal(const std::optional<TemporalDeclarationState>& a,
                                     const std::optional<TemporalDeclarationState>& b) {
    if(a.has_value()!=b.has_value())return false;
    if(!a)return true;
    Writer x,y;declaration_state(x,*a);declaration_state(y,*b);return x.bytes()==y.bytes();
}
inline void declaration_update(Writer& w, const TemporalDeclarationUpdate& update) {
    (void)temporal_declaration_detail::update_usage(update);
    w.u8(update.target.has_value()); if(update.target)declaration_state(w,*update.target);
    auto maps=update.maps;std::sort(maps.begin(),maps.end(),[](const auto& a,const auto& b){return a.from<b.from;});
    w.u64(maps.size());
    for(auto& m:maps) {
        require(m.kind<=composite::DeclarationMapKind::Embedding && update.target,"invalid declaration map lane");
        declaration_id(w,m.from);declaration_id(w,m.to);w.u8(static_cast<std::uint8_t>(m.kind));
        std::sort(m.fills.begin(),m.fills.end(),[](const auto& a,const auto& b){return a.factor<b.factor;});
        w.u64(m.fills.size());const auto& target=update.target->registry.at(m.to);
        for(const auto& f:m.fills){
            w.u64(f.factor);const auto it=std::find_if(target.factors.begin(),target.factors.end(),[&](const auto& x){return x.key==f.factor;});
            require(it!=target.factors.end(),"unknown initialized target factor");
            declared_payload(w,composite::CompiledLayout(update.target->registry,it->type,update.target->profile,update.target->resource_limits),f.value);
        }
    }
    auto replaced=update.replaced_values;std::sort(replaced.begin(),replaced.end());keys(w,replaced);
}
inline TemporalDeclarationUpdate declaration_update(Reader& r) {
    TemporalDeclarationUpdate u;if(r.flag())u.target=declaration_state(r);
    const auto n=r.count(57);
    require(n<=r.limits().declarations.max_definitions,"declaration map population budget");
    auto [used_nodes,used_bytes]=temporal_declaration_detail::update_usage(u,r.limits().declarations);
    for(std::size_t i=0;i<n;++i){
        composite::DeclarationMap m;m.from=declaration_id(r);m.to=declaration_id(r);m.kind=static_cast<composite::DeclarationMapKind>(r.u8());
        require(m.kind<=composite::DeclarationMapKind::Embedding && u.target,"unsupported declaration map");
        const auto count=r.count(24);const auto& d=u.target->registry.at(m.to);
        used_bytes=machine::checked_size_add(used_bytes,machine::checked_size_mul(count,sizeof(composite::DeclarationFactorFill)));
        require(used_bytes<=r.limits().declarations.max_materialized_bytes,"declaration fill root budget");
        for(std::size_t j=0;j<count;++j){
            const auto key=r.u64();const auto it=std::find_if(d.factors.begin(),d.factors.end(),[&](const auto& x){return x.key==key;});
            require(it!=d.factors.end(),"unknown initialized target factor");
            auto policy=r.limits().declarations;
            require(used_nodes<policy.max_value_nodes && used_bytes<=policy.max_materialized_bytes,"aggregate declaration fill budget");
            policy.max_value_nodes-=used_nodes;policy.max_materialized_bytes=std::max<std::size_t>(1,policy.max_materialized_bytes-used_bytes);
            auto value=declared_payload(r,composite::CompiledLayout(u.target->registry,it->type,u.target->profile,policy));
            const auto use=composite::CompiledLayout(u.target->registry,it->type,u.target->profile,r.limits().declarations).allocation(value);
            used_nodes=machine::checked_size_add(used_nodes,use.value_nodes);used_bytes=machine::checked_size_add(used_bytes,use.materialized_bytes);
            require(used_nodes<=r.limits().declarations.max_value_nodes && used_bytes<=r.limits().declarations.max_materialized_bytes,"aggregate initialized-factor decode budget");
            m.fills.push_back({key,std::move(value)});
        }
        u.maps.push_back(std::move(m));
    }
    u.replaced_values=keys(r);
    (void)temporal_declaration_detail::update_usage(u,r.limits().declarations);return u;
}
inline void source_migration(Writer& w, const SourceDeclarationMigration& m) {
    (void)source_declaration_detail::resolve(m);
    declaration_blob(w,source_declaration_detail::signature_bytes(m.base_signature));
    declaration_blob(w,source_declaration_detail::signature_bytes(m.target_signature));
    const auto names=[&](auto maps){std::sort(maps.begin(),maps.end(),[](const auto& a,const auto& b){return a.from<b.from;});
        w.u64(maps.size());for(const auto& p:maps){string(w,p.from);string(w,p.to);}};
    names(m.aspects);names(m.types);
    auto added=m.introduced;std::sort(added.begin(),added.end(),[](const auto& a,const auto& b){return a.target<b.target;});
    w.u64(added.size());
    for(auto& x:added){
        string(w,x.target);w.u8(x.fill.has_value());if(x.fill)w.u8(*x.fill);
        std::sort(x.assignments.begin(),x.assignments.end(),[](const auto& a,const auto& b){return a.occurrence<b.occurrence;});
        w.u64(x.assignments.size());for(const auto& v:x.assignments){occurrence(w,v.occurrence);w.u8(v.value);}
    }
}
inline SourceDeclarationMigration source_migration(Reader& r) {
    SourceDeclarationMigration m;
    m.base_signature=rfcx_detail::read_signature(r.blob(),0);m.target_signature=rfcx_detail::read_signature(r.blob(),0);
    r.consume_records(machine::checked_size_add(m.base_signature.aspects().size(),m.target_signature.aspects().size()));
    r.consume_records(machine::checked_size_add(m.base_signature.type_count(),m.target_signature.type_count()));
    const auto names=[&](auto& maps){const auto n=r.count(16);for(std::size_t i=0;i<n;++i){auto a=r.string();auto b=r.string();maps.push_back({std::move(a),std::move(b)});}};
    names(m.aspects);names(m.types);
    const auto n=r.count(17);for(std::size_t i=0;i<n;++i){
        SourceAspectIntroduction x;x.target=r.string();if(r.flag())x.fill=r.flag();
        const auto count=r.count(11);for(std::size_t j=0;j<count;++j){const auto key=occurrence(r);x.assignments.push_back({key,r.flag()});}
        m.introduced.push_back(std::move(x));
    }
    (void)source_declaration_detail::resolve(m);return m;
}
} // namespace rfc::storage::temporal_codec_detail
