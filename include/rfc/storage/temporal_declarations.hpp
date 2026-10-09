#pragma once

#include "rfc/storage/source_declaration_migration.hpp"
#include "rfc/core/declaration_binding.hpp"
#include "rfc/composite/declaration_evolution.hpp"
#include "rfc/storage/declaration_codec.hpp"

namespace rfc::storage {
struct TemporalDeclaredValue {
    std::uint64_t key{};
    composite::DeclarationId type;
    std::uint64_t block{}, branch{};
    std::optional<StableCellKey> cell;
    std::optional<StableOccurrenceKey> occurrence;
    composite::DeclaredValue value;
};
struct TemporalBranchDeclaration {
    std::uint64_t key{};
    std::string source_type;
    core::BranchDeclaration declaration;
};
struct TemporalSourceAspectLayout {
    std::uint64_t key{};
    composite::DeclarationId type;
    std::vector<core::SourceAspectFactorBinding> factors;
};
struct TemporalSourceLayoutProgram {
    std::uint64_t key{}, layout{};
    composite::BinaryProgram program;
    std::uint16_t output{};
};
struct TemporalCodeDeclarationBinding {
    std::uint64_t key{}; // existing relational code type identity
    composite::DeclarationId type;
};
struct TemporalDeclarationState {
    std::uint64_t namespace_key{};
    composite::DeclarationRegistry registry{std::vector<composite::Declaration>{}};
    composite::BooleanLayoutProfile profile{composite::BooleanLayoutProfile::Lsb0OctetsV1};
    std::vector<TemporalDeclaredValue> values;
    std::vector<TemporalBranchDeclaration> branches;
    std::vector<TemporalSourceAspectLayout> source_layouts;
    std::vector<TemporalCodeDeclarationBinding> code_types;
    std::vector<TemporalSourceLayoutProgram> programs;
};
struct TemporalDeclarationUpdate {
    // A complete owning target, not a map/defect from which new directions must
    // be guessed. Null explicitly removes the channel. Absent update inherits it.
    std::optional<TemporalDeclarationState> target;
    std::vector<composite::DeclarationMap> maps;
    // Persistent values whose complete target is a data/owner change, rather
    // than the declared lossless recoding. Births/removals follow stable keys.
    std::vector<std::uint64_t> replaced_values;
};
namespace temporal_declaration_detail {
using temporal_source_detail::require;
template<class T> inline auto keyed(const std::vector<T>& xs) {
    std::map<std::uint64_t,const T*> out;
    for (const auto& x : xs) require(x.key && out.emplace(x.key,&x).second, "zero/duplicate declaration-channel identity");
    return out;
}
inline bool owner_equal(const TemporalDeclaredValue& a, const TemporalDeclaredValue& b) {
    return a.block == b.block && a.branch == b.branch && a.cell == b.cell && a.occurrence == b.occurrence;
}
inline void validate_definitions(const TemporalDeclarationState& s, const composite::DeclarationLimits& limits = {}) {
    require(s.namespace_key && !s.registry.definitions().empty(), "empty/unnamespaced declaration endpoint");
    const composite::PreparedLayoutRegistry registry(s.registry, limits);
    composite::validate_layout_profile(s.profile);
    (void)keyed(s.values); (void)keyed(s.branches); (void)keyed(s.source_layouts); (void)keyed(s.code_types); (void)keyed(s.programs);
    for (const auto& binding : s.code_types) (void)s.registry.at(binding.type);
    std::size_t nodes = 0, bytes = machine::checked_size_mul(s.values.size(), sizeof(TemporalDeclaredValue));
    composite::declaration_detail::budget(bytes, limits.max_materialized_bytes, "declared root-value buffer budget");
    for (const auto& v : s.values) {
        require(!(v.cell && v.occurrence), "declared value has multiple source owners");
        const composite::CompiledLayout layout(registry, v.type, s.profile);
        const auto allocation = layout.allocation(v.value);
        nodes = machine::checked_size_add(nodes, allocation.value_nodes);
        bytes = machine::checked_size_add(bytes, allocation.materialized_bytes);
        composite::declaration_detail::budget(nodes, limits.max_value_nodes, "aggregate declared value work budget");
        composite::declaration_detail::budget(bytes, limits.max_materialized_bytes, "aggregate declared value buffer budget");
    }
}
inline std::pair<std::size_t,std::size_t> update_usage(const TemporalDeclarationUpdate& u,
                                                     const composite::DeclarationLimits& limits = {}) {
    std::size_t nodes=0,bytes=0;
    std::optional<composite::PreparedLayoutRegistry> registry;
    const auto allocation=[&](composite::DeclarationId type,const composite::DeclaredValue& value) {
        if(!registry)registry.emplace(u.target->registry,limits);
        return composite::CompiledLayout(*registry,type,u.target->profile).allocation(value);
    };
    const auto account=[&](const composite::LayoutAllocation& use){
        nodes=machine::checked_size_add(nodes,use.value_nodes);bytes=machine::checked_size_add(bytes,use.materialized_bytes);
        composite::declaration_detail::budget(nodes,limits.max_value_nodes,"declaration update aggregate work budget");
        composite::declaration_detail::budget(bytes,limits.max_materialized_bytes,"declaration update aggregate buffer budget");
    };
    if (u.target) {
        bytes=machine::checked_size_mul(u.target->values.size(),sizeof(TemporalDeclaredValue));
        composite::declaration_detail::budget(bytes,limits.max_materialized_bytes,"declaration update root buffer budget");
        for(const auto& v:u.target->values)account(allocation(v.type,v.value));
    }
    composite::declaration_detail::budget(u.maps.size(),limits.max_definitions,"declaration map population budget");
    for(const auto& m:u.maps) {
        require(u.target.has_value(),"declaration maps without target");
        const auto& type=u.target->registry.at(m.to);
        bytes=machine::checked_size_add(bytes,machine::checked_size_mul(m.fills.size(),sizeof(composite::DeclarationFactorFill)));
        composite::declaration_detail::budget(bytes,limits.max_materialized_bytes,"declaration fill root buffer budget");
        for(const auto& f:m.fills) {
            const auto it=std::find_if(type.factors.begin(),type.factors.end(),[&](const auto& x){return x.key==f.factor;});
            require(it!=type.factors.end(),"unknown initialized declaration factor");
            account(allocation(it->type,f.value));
        }
    }
    return {nodes,bytes};
}

} // namespace temporal_declaration_detail

namespace temporal_declaration_detail {
inline void validate_against_source(const core::RelationalComplex& source,
                                           const StableSourceIndex& source_index,
                                           const std::vector<LocalBlockDefinition>& blocks,
                                           const TemporalDeclarationState& state,
                                           const composite::DeclarationLimits& limits = {}) {
    using namespace temporal_declaration_detail;
    validate_definitions(state, limits);
    std::map<std::uint64_t,const LocalBlockDefinition*> block_index;
    for (const auto& b : blocks) require(block_index.emplace(b.block_key,&b).second, "duplicate declaration owner block");
    std::map<std::uint64_t, core::BranchDeclarationBinding> branches;
    std::set<composite::DeclarationId> branch_ids;
    for (const auto& b : state.branches) {
        require(branch_ids.insert(b.declaration.id).second, "duplicate branch declaration identity");
        branches.emplace(b.key, core::BranchDeclarationBinding(source.signature(), source.signature().type_id(b.source_type),
            b.declaration, composite::CompiledLayout(state.registry,b.declaration.payload,state.profile,limits)));
    }
    for (const auto& l : state.source_layouts)
        (void)core::SourceAspectLayoutBinding(source.signature(), composite::CompiledLayout(state.registry,l.type,state.profile,limits),l.factors);
    const auto layouts = keyed(state.source_layouts);
    for (const auto& p : state.programs) {
        require(layouts.contains(p.layout) && p.output < p.program.register_count(), "declared program layout/output absent");
        const auto& type = state.registry.at(layouts.at(p.layout)->type);
        composite::declaration_detail::budget(p.program.instruction_count(), limits.max_value_nodes, "declared program instruction budget");
        for (const auto& i : p.program.instructions())
            if (i.opcode == composite::BinaryOpcode::LoadAspect)
                require(i.aspect < type.factors.size(), "declared program factor outside layout");
    }
    for (const auto& v : state.values) {
        if (v.cell) require(source_index.cell(*v.cell).has_value(), "declared value cell owner absent");
        if (v.occurrence) require(source_index.occurrence(*v.occurrence).has_value(), "declared value occurrence owner absent");
        if (v.block) {
            require(block_index.contains(v.block), "declared value block owner absent");
            const auto& b = *block_index.at(v.block);
            if (v.cell) require(std::find(b.cells.begin(),b.cells.end(),*v.cell)!=b.cells.end(), "declared cell outside block");
            if (v.occurrence) {
                if (b.kind == LocalBlockKind::PartialChart)
                    require(std::find(b.occurrences.begin(),b.occurrences.end(),*v.occurrence)!=b.occurrences.end(), "declared occurrence outside partial chart");
                else {
                    const auto slot = source_index.occurrence(*v.occurrence).value();
                    const auto& arena = source.grade(v.occurrence->upper_grade);
                    const StableCellKey owner{v.occurrence->upper_grade, arena.relation_identities.at(arena.upper_index.at(slot)).value};
                    require(std::find(b.cells.begin(),b.cells.end(),owner)!=b.cells.end(), "declared occurrence outside strict block");
                }
            }
        }
        if (v.branch) {
            require(v.cell && v.cell->grade.value && branches.contains(v.branch), "branch instance requires declared relation owner");
            const auto& binding = branches.at(v.branch);
            require(v.type == binding.declaration().payload, "branch payload root mismatch");
            const auto owner = *source_index.cell(*v.cell);
            (void)binding.validate(source,core::RelationId{owner.grade,owner.index},v.value,limits.max_value_nodes);
        }
    }
}

} // namespace temporal_declaration_detail

inline void validate_temporal_declarations(const core::RelationalComplex& source,
                                           const std::vector<LocalBlockDefinition>& blocks,
                                           const TemporalDeclarationState& state,
                                           const composite::DeclarationLimits& limits = {}) {
    const StableSourceIndex source_index(source);
    temporal_declaration_detail::validate_against_source(source, source_index, blocks, state, limits);
}

inline composite::DeclaredValue materialize_declared_source_aspects(const core::RelationalComplex& source,
    const TemporalDeclarationState& state, std::uint64_t binding_key, StableOccurrenceKey occurrence,
    const composite::DeclarationLimits& limits = {}) {
    const auto bindings = temporal_declaration_detail::keyed(state.source_layouts);
    const auto it = bindings.find(binding_key);
    temporal_source_detail::require(it != bindings.end(), "unknown declared source layout");
    const auto slot = resolve_stable_occurrence(source,occurrence);
    temporal_source_detail::require(slot.has_value(), "source layout occurrence absent");
    const auto& b = *it->second;
    return core::SourceAspectLayoutBinding(source.signature(),composite::CompiledLayout(state.registry,b.type,state.profile,limits),b.factors)
        .materialize_occurrence(source,occurrence.upper_grade,*slot);
}

// Retarget source-word operands through stable factor identities. Register flow
// and instruction order are untouched; no opcode is rewritten from observed outputs.
inline TemporalSourceLayoutProgram recode_declared_program(const TemporalDeclarationState& base,
    const TemporalDeclarationState& target, const TemporalSourceLayoutProgram& old,
    std::uint64_t target_layout, const std::vector<composite::DeclarationMap>& maps) {
    using temporal_source_detail::require;
    const auto a=temporal_declaration_detail::keyed(base.source_layouts);
    const auto b=temporal_declaration_detail::keyed(target.source_layouts);
    require(a.contains(old.layout) && b.contains(target_layout), "program recoding layout absent");
    const composite::DeclarationRecoding recoding(base.registry,target.registry,maps);
    require(recoding.target_id(a.at(old.layout)->type)==b.at(target_layout)->type,"program type correspondence mismatch");
    const auto& from=base.registry.at(a.at(old.layout)->type);
    const auto& to=target.registry.at(b.at(target_layout)->type);
    require(from.kind==composite::DeclarationKind::Product && to.kind==from.kind,"program needs product factors");
    TemporalSourceLayoutProgram next{old.key,target_layout,composite::BinaryProgram(old.program.register_count()),old.output};
    require(old.output<old.program.register_count(),"program output register absent");
    for(auto i:old.program.instructions()) {
        if(i.opcode==composite::BinaryOpcode::LoadAspect) {
            require(i.aspect<from.factors.size(),"program references nonexistent old factor");
            const auto key=from.factors[i.aspect].key;
            const auto it=std::find_if(to.factors.begin(),to.factors.end(),[&](const auto& f){return f.key==key;});
            require(it!=to.factors.end() && static_cast<std::size_t>(it-to.factors.begin())<std::numeric_limits<composite::AspectId>::max(),"program target factor absent/oversized");
            i.aspect=static_cast<composite::AspectId>(it-to.factors.begin());
        }
        next.program.append(i);
    }
    return next;
}
inline machine::PackedBits execute_declared_source_program(const core::RelationalComplex& source,
    const TemporalDeclarationState& state,std::uint64_t key,core::Grade grade) {
    const auto programs=temporal_declaration_detail::keyed(state.programs);
    const auto layouts=temporal_declaration_detail::keyed(state.source_layouts);
    temporal_source_detail::require(programs.contains(key),"declared source program absent");
    const auto& p=*programs.at(key);
    temporal_source_detail::require(layouts.contains(p.layout),"declared source program layout absent");
    const auto& l=*layouts.at(p.layout);
    return core::SourceAspectLayoutBinding(source.signature(),composite::CompiledLayout(state.registry,l.type,state.profile),l.factors)
        .execute(source,grade,p.program,p.output);
}

// A full target root can intentionally change meaning. Such a persistent value
// must be in replaced_values; it is never called a pure layout transformation.
inline void validate_temporal_declaration_transition(const std::optional<TemporalDeclarationState>& base,
                                                     const TemporalDeclarationUpdate& update) {
    using namespace temporal_declaration_detail;
    (void)update_usage(update);
    if (!base || !update.target) {
        require(update.maps.empty() && update.replaced_values.empty(), "declaration attach/remove has spurious correspondences");
        return;
    }
    const auto& next = *update.target;
    for (const auto& old : base->branches) for (const auto& b : next.branches)
        if (old.declaration.id == b.declaration.id)
            require(old.declaration == b.declaration, "immutable branch declaration redefined");
    require(base->namespace_key == next.namespace_key, "declaration namespace change requires detach/attach");
    composite::DeclarationRecoding recoding(base->registry,next.registry,update.maps);
    const auto old_values = keyed(base->values), next_values = keyed(next.values);
    std::set<std::uint64_t> replaced;
    for (auto key : update.replaced_values)
        require(replaced.insert(key).second && old_values.contains(key) && next_values.contains(key), "invalid declared value replacement");
    for (const auto& [key, v] : next_values) {
        if (!old_values.contains(key) || replaced.contains(key)) continue;
        const auto& old = *old_values.at(key);
        require(owner_equal(old,*v) && recoding.target_id(old.type) == v->type &&
                composite::declared_value_equal(recoding.apply(old.type,old.value),v->value),
                "unrecorded declared value/owner change");
    }
}
inline TemporalDeclarationUpdate make_temporal_declaration_update(const std::optional<TemporalDeclarationState>& base,
    std::optional<TemporalDeclarationState> target, std::vector<composite::DeclarationMap> maps = {}) {
    using namespace temporal_declaration_detail;
    TemporalDeclarationUpdate out{std::move(target),std::move(maps),{}};
    if (base && out.target) {
        if (out.maps.empty()) {
            require(base->registry.definitions() == out.target->registry.definitions(),
                    "changed declaration DAG requires explicit correspondence");
            out.maps = composite::identity_declaration_maps(base->registry);
        }
        const composite::DeclarationRecoding recoding(base->registry,out.target->registry,out.maps);
        const auto old_values = keyed(base->values);
        for (const auto& v : out.target->values) {
            if (!old_values.contains(v.key)) continue;
            const auto& old = *old_values.at(v.key);
            if (!owner_equal(old,v) || recoding.target_id(old.type) != v.type ||
                !composite::declared_value_equal(recoding.apply(old.type,old.value),v.value)) out.replaced_values.push_back(v.key);
        }
    }
    validate_temporal_declaration_transition(base,out);
    return out;
}
} // namespace rfc::storage
