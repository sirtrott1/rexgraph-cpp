#pragma once

#include "rfc/storage/native_atlas_codec.hpp"
#include <string>

namespace rfc::database {

// F5 is an interpretation boundary, not a table catalog. External tokens resolve
// once, at ingestion. Native queries below use retained keys and occurrence roles.
struct ExternalOrigin {
    std::uint64_t kind{};
    std::string token; // producer identity, not a content hash or a scan position
    auto operator<=>(const ExternalOrigin&) const = default;
};
struct ExternalRole {
    std::uint64_t key{}, participant_kind{};
    std::size_t minimum{}, maximum{1};
    bool allow_missing{}, allow_null{};
    bool operator==(const ExternalRole&) const = default;
};
struct ExternalSubjectKind {
    std::uint64_t key{};
    std::string label; // external presentation only
    std::string identity_label{}, identity_encoding{};
    bool operator==(const ExternalSubjectKind&) const = default;
};
struct ExternalFieldLabel {
    std::uint64_t factor{};
    std::string label;
    bool operator==(const ExternalFieldLabel&) const = default;
};
struct ExternalAssertionKind {
    std::uint64_t key{};
    std::string label;
    composite::DeclarationId payload;
    std::vector<ExternalRole> roles;
    std::vector<ExternalFieldLabel> fields; // optional presentation of product factors
    std::string lineage_label{}, lineage_encoding{};
    std::vector<ExternalFieldLabel> reference_labels{};
    bool operator==(const ExternalAssertionKind&) const = default;
};
struct InterpretationSchema {
    std::uint64_t source_namespace{}, binding_namespace{}, version{1};
    composite::DeclarationRegistry values{std::vector<composite::Declaration>{}};
    std::vector<ExternalSubjectKind> subjects;
    std::vector<ExternalAssertionKind> assertions;
};
enum class ReferenceState : std::uint8_t { Missing=0, Null=1, Present=2 };
struct ExternalOccurrence {
    std::string lineage; // unique within the assertion, including across roles
    ExternalOrigin participant;
    bool active{true};
    bool operator==(const ExternalOccurrence&) const = default;
};
struct ExternalRoleValue {
    std::uint64_t role{};
    ReferenceState state{ReferenceState::Present};
    std::vector<ExternalOccurrence> occurrences; // meaningful order, duplicates of participants allowed
    bool operator==(const ExternalRoleValue&) const = default;
};
struct ExternalAssertion {
    ExternalOrigin origin;
    composite::DeclaredValue payload;
    std::vector<ExternalRoleValue> roles;
};
struct LogicalSnapshot {
    std::string revision; // producer snapshot/manifest identity
    // Subject/assertion enumeration order is not identity. A declaration's
    // repeated values and each role's occurrence order ARE meaningful.
    std::vector<ExternalOrigin> subjects;
    std::vector<ExternalAssertion> assertions;
};
struct OriginBinding {
    ExternalOrigin origin;
    std::uint64_t native_key{};
    bool operator==(const OriginBinding&) const = default;
};
struct OccurrenceOrigin {
    ExternalOrigin assertion;
    std::string lineage;
    auto operator<=>(const OccurrenceOrigin&) const = default;
};
struct OccurrenceBinding {
    OccurrenceOrigin origin;
    std::uint64_t native_key{};
    bool operator==(const OccurrenceBinding&) const = default;
};
struct InterpretationLineage {
    // Tombstones remain after removal; keys are never recycled for new origins.
    std::vector<OriginBinding> subjects, assertions;
    std::vector<OccurrenceBinding> occurrences;
    bool operator==(const InterpretationLineage&) const = default;
};
struct InterpretationLimits {
    std::size_t max_records{100000}, max_origin_bytes{4096}, max_metadata_bytes{16u<<20};
    std::size_t max_archive_bytes{128u<<20}, max_work{4000000};
    composite::DeclarationLimits declarations{};
    storage::NativeAtlasLimits atlas{};
};

namespace interpretation_detail {
using composite::Declaration;
using composite::DeclarationId;
using composite::DeclaredValue;
using composite::DeclarationKind;
using storage::temporal_source_detail::require;
inline void bound(std::size_t n,std::size_t limit,const char* message) {
    if(n>limit)throw std::length_error(message);
}
inline void add(std::size_t& n,std::size_t amount,std::size_t limit,const char* message) {
    n=machine::checked_size_add(n,amount);bound(n,limit,message);
}
inline std::string source_type(std::uint64_t kind) { return "external/"+std::to_string(kind); }
inline std::string role_aspect(std::uint64_t kind,std::uint64_t role) {
    return "role/"+std::to_string(kind)+"/"+std::to_string(role);
}
inline void token(const std::string& s,std::size_t& total,const InterpretationLimits& limits,bool nonempty=true) {
    require(!nonempty || !s.empty(),"explicit nonempty external lineage required");
    bound(s.size(),limits.max_origin_bytes,"external token/label byte budget");
    add(total,s.size(),limits.max_metadata_bytes,"external aggregate metadata budget");
}
inline void origin(const ExternalOrigin& o,std::size_t& total,const InterpretationLimits& limits) {
    require(o.kind!=0,"zero external origin kind");token(o.token,total,limits);
}
// Preflight physical value buffers before boundary-level copies. The F1 layout
// separately checks semantic validity and canonical encoded/materialized size.
inline void account_value_copy(const DeclaredValue& value,const InterpretationLimits& limits,
                               std::size_t& nodes,std::size_t& bytes,std::size_t depth=0) {
    bound(depth,limits.declarations.max_depth,"external value nesting budget");
    add(nodes,1,limits.declarations.max_value_nodes,"external value node budget");
    add(bytes,sizeof(DeclaredValue),limits.declarations.max_materialized_bytes,"external value object budget");
    const auto sequence=[&](const auto& xs) {
        bound(xs.size(),limits.declarations.max_value_nodes-nodes,"external value child budget");
        for(const auto& x:xs)account_value_copy(x,limits,nodes,bytes,depth+1);
    };
    const auto integer=[&](const numeric::Integer& n) {
        const auto bits=composite::declaration_detail::integer_bits(n);
        bound(bits,limits.declarations.max_integer_bits,"external integer bit budget");
        add(bytes,machine::ceil_div(bits,8),limits.declarations.max_materialized_bytes,"external integer buffer budget");
    };
    switch(value.data.index()) {
        case 0:case 1:break;
        case 2:integer(std::get<2>(value.data));break;
        case 3:{const auto& q=std::get<3>(value.data);integer(q.get_num());integer(q.get_den());break;}
        case 4:sequence(std::get<4>(value.data).elements);break;
        case 5:sequence(std::get<5>(value.data).element);break;
        case 6:sequence(std::get<6>(value.data).element);break;
        case 7:sequence(std::get<7>(value.data).elements);break;
        case 8:{const auto& t=std::get<8>(value.data);
            add(bytes,machine::checked_size_mul(t.domain().rank(),sizeof(std::size_t)),limits.declarations.max_materialized_bytes,"external tensor shape budget");
            if(const auto* p=t.support().packed_bits())add(bytes,machine::checked_size_mul(p->words().size(),sizeof(std::uint64_t)),limits.declarations.max_materialized_bytes,"external packed support buffer budget");
            if(const auto* p=t.support().sparse_indices())add(bytes,machine::checked_size_mul(p->size(),sizeof(std::size_t)),limits.declarations.max_materialized_bytes,"external sparse support buffer budget");
            add(nodes,machine::checked_size_add(t.support().count(),t.values().nonzeros()),limits.declarations.max_value_nodes,"external tensor work budget");
            add(bytes,machine::checked_size_mul(t.values().nonzeros(),sizeof(numeric::SparseRationalEntry)),limits.declarations.max_materialized_bytes,"external tensor entries budget");
            for(const auto& e:t.values().entries()){integer(e.value.get_num());integer(e.value.get_den());}break;}
        default:throw std::invalid_argument("unknown external value constructor");
    }
}
template<class T> inline void sort_keys(std::vector<T>& xs) {
    std::sort(xs.begin(),xs.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    for(std::size_t i=0;i<xs.size();++i)require(xs[i].key && (!i || xs[i-1].key!=xs[i].key),"duplicate/zero external declaration key");
}
struct CompiledInterpretation {
    InterpretationSchema schema;
    composite::DeclarationRegistry registry;
    core::RelationSignature signature;
    std::map<std::uint64_t,DeclarationId> wrappers, role_products, layouts;
    std::map<std::uint64_t,core::RelationTypeId> types;
    std::map<std::pair<std::uint64_t,std::uint64_t>,composite::AspectId> aspects;

    explicit CompiledInterpretation(InterpretationSchema input,const InterpretationLimits& limits={})
        :schema(std::move(input)),registry(std::vector<Declaration>{}) {
        limits.declarations.validate();
        require(schema.source_namespace && schema.binding_namespace && schema.version,"zero interpretation namespace/version");
        bound(schema.subjects.size(),limits.max_records,"subject declaration budget");
        bound(schema.assertions.size(),limits.max_records,"assertion declaration budget");
        require(!schema.assertions.empty(),"interpretation requires an assertion declaration");
        sort_keys(schema.subjects);sort_keys(schema.assertions);
        std::set<std::uint64_t> subject_kinds;
        std::size_t bytes=0,records=schema.subjects.size();
        for(const auto& s:schema.subjects){subject_kinds.insert(s.key);token(s.label,bytes,limits,false);token(s.identity_label,bytes,limits,false);token(s.identity_encoding,bytes,limits,false);}
        auto definitions=schema.values.definitions();
        for(const auto& d:definitions)require(d.id.name_space!=schema.binding_namespace,"payload declaration uses reserved binding namespace");
        const auto id=[&](std::uint64_t key){return DeclarationId{schema.binding_namespace,key,schema.version};};
        definitions.push_back(Declaration::finite(id(1),{0,1,2}));
        definitions.push_back(Declaration::boolean(id(2)));
        // Role order is declared data over retained occurrence identities, never
        // the incidental order of a generic source fiber in its arena.
        definitions.push_back(Declaration::integer(id(3)));
        std::size_t maximum_role=0;
        for(const auto& a:schema.assertions)for(const auto& r:a.roles)maximum_role=std::max(maximum_role,r.maximum);
        bound(maximum_role,limits.max_records,"role maximum budget");
        definitions.push_back(Declaration::repeated(id(4),id(3),0,maximum_role));
        std::uint64_t next=5;
        for(auto& a:schema.assertions) {
            add(records,1,limits.max_records,"interpretation declaration budget");token(a.label,bytes,limits,false);token(a.lineage_label,bytes,limits,false);token(a.lineage_encoding,bytes,limits,false);
            const auto& payload=schema.values.at(a.payload);sort_keys(a.roles);
            add(records,a.roles.size(),limits.max_records,"interpretation role budget");
            std::vector<composite::DeclarationFactor> role_factors, bit_factors, order_factors;
            for(const auto& role:a.roles) {
                require(subject_kinds.contains(role.participant_kind),"unknown participant kind");
                require(role.minimum<=role.maximum,"invalid role cardinality");
                bound(role.maximum,limits.max_records,"role maximum budget");
                role_factors.push_back({role.key,id(1),0});bit_factors.push_back({role.key,id(2),0});order_factors.push_back({role.key,id(4),0});
                aspects.emplace(std::make_pair(a.key,role.key),signature.aspects().add(role_aspect(a.key,role.key),composite::AspectRole::RelationRole));
            }
            std::sort(a.reference_labels.begin(),a.reference_labels.end(),[](const auto& x,const auto& y){return x.factor<y.factor;});
            std::set<std::uint64_t> reference_labels;
            for(const auto& label:a.reference_labels){require(reference_labels.insert(label.factor).second && std::any_of(a.roles.begin(),a.roles.end(),[&](const auto& r){return r.key==label.factor;}),"invalid reference presentation label");token(label.label,bytes,limits,false);}
            std::sort(a.fields.begin(),a.fields.end(),[](const auto& x,const auto& y){return x.factor<y.factor;});
            std::set<std::uint64_t> fields;
            for(const auto& field:a.fields) {
                require(payload.kind==DeclarationKind::Product && fields.insert(field.factor).second,"invalid external product label");
                require(std::any_of(payload.factors.begin(),payload.factors.end(),[&](const auto& f){return f.key==field.factor;}),"label for nonexistent factor");
                token(field.label,bytes,limits,false);
            }
            require(next<std::numeric_limits<std::uint64_t>::max()-3,"generated declaration key overflow");
            const auto roles_id=id(next++),orders_id=id(next++),wrapper_id=id(next++),layout_id=id(next++);
            definitions.push_back(Declaration::product(roles_id,std::move(role_factors)));
            definitions.push_back(Declaration::product(orders_id,std::move(order_factors)));
            definitions.push_back(Declaration::product(wrapper_id,{{1,a.payload,0},{2,roles_id,0},{3,orders_id,0}}));
            definitions.push_back(Declaration::product(layout_id,std::move(bit_factors)));
            role_products.emplace(a.key,roles_id);wrappers.emplace(a.key,wrapper_id);layouts.emplace(a.key,layout_id);
            types.emplace(a.key,signature.add_type(source_type(a.key)));
        }
        registry=composite::DeclarationRegistry(std::move(definitions),limits.declarations);
    }
    const ExternalAssertionKind& assertion(std::uint64_t kind) const {
        const auto it=std::lower_bound(schema.assertions.begin(),schema.assertions.end(),kind,[](const auto& a,auto k){return a.key<k;});
        require(it!=schema.assertions.end() && it->key==kind,"unknown assertion kind");return *it;
    }
};
inline const DeclaredValue::Product& product(const DeclaredValue& value) {
    const auto* p=std::get_if<DeclaredValue::Product>(&value.data);require(p,"expected product value");return *p;
}
inline std::uint64_t symbol(const DeclaredValue& value) {
    const auto* p=std::get_if<DeclaredValue::Symbol>(&value.data);require(p,"expected finite reference state");return p->key;
}
} // namespace interpretation_detail
} // namespace rfc::database
