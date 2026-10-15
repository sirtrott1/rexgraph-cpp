#pragma once

#include "rfc/inference/relational_value_admission.hpp"

namespace rfc::inference {
struct RelationalInitialUseCorrespondence { std::uint64_t original{}, initial{}; };
struct RelationalInitialInputLineageDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<RelationalInitialUseCorrespondence> uses;
};
struct RelationalInitialInputLineageOrigins { std::uint64_t original{}, initial{}, interface{}; };
namespace initial_input_lineage_detail {
using namespace field_interface_detail;
inline constexpr std::array<std::byte,8> magic{
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'K'},std::byte{'L'},std::byte{1},std::byte{0},std::byte{0}};
inline std::vector<std::byte> payload(std::span<const std::byte> original,std::span<const std::byte> initial,
    const RelationalInitialInputLineageDeclaration& d) {
    auto w=field_program_codec_detail::envelope(magic);field_program_codec_detail::blob(w,original);
    field_program_codec_detail::blob(w,initial);w.u64(d.key);w.u64(d.revision);w.u64(d.uses.size());
    for(const auto u:d.uses){w.u64(u.original);w.u64(u.initial);}return field_program_codec_detail::finish(std::move(w));
}
inline void source_copy(const core::RelationalComplex& old,const core::RelationalComplex& next,Guard& guard) {
    require(old.grade_count()==next.grade_count()&&old.semantic_scale()==next.semantic_scale(),"initial source changes copied grade or semantic scale");
    const auto& a=old.signature();const auto& b=next.signature();
    require(a.type_count()<=b.type_count()&&a.aspects().size()<=b.aspects().size(),"initial source omits original signature declarations");
    for(std::size_t i=0;i<a.aspects().size();++i){guard.step();const auto id=static_cast<composite::AspectId>(i);
        const auto& x=a.aspects().at(id);const auto& y=b.aspects().at(id);
        require(x.name==y.name&&x.role==y.role&&x.default_value==y.default_value,"initial source changes original aspect meaning");}
    for(std::size_t i=0;i<a.type_count();++i){guard.step();const auto id=static_cast<core::RelationTypeId>(i);const auto& x=a.type(id);const auto& y=b.type(id);
        require(x.name==y.name&&x.boundary_policy==y.boundary_policy&&x.constructor==y.constructor&&
            y.min_arity>=x.min_arity&&(!x.max_arity||(y.max_arity&&y.max_arity<=x.max_arity))&&
            x.constraints.shape==y.constraints.shape&&x.constraints.distinguished_role==y.constraints.distinguished_role&&
            x.constraints.rules.size()==y.constraints.rules.size(),"initial source changes original type law beyond admitted arity specialization");
        for(std::size_t j=0;j<x.constraints.rules.size();++j){guard.step();const auto& u=x.constraints.rules[j];const auto& v=y.constraints.rules[j];
            require(u.kind==v.kind&&u.aspect==v.aspect&&u.count==v.count&&u.value==v.value,"initial source changes original type rules");}}
    for(std::size_t k=0;k<old.grade_count();++k){guard.step();const auto g=grade(k);const auto& x=old.grade(g);const auto& y=next.grade(g);
        require(x.relation_types==y.relation_types&&x.identities==y.identities&&x.relation_identities==y.relation_identities&&
            x.cell_scales==y.cell_scales&&x.offsets==y.offsets&&x.lower_index==y.lower_index&&x.upper_index==y.upper_index&&
            x.occurrence_identities==y.occurrence_identities&&x.lattice_coefficient==y.lattice_coefficient,
            "initial source changes primary identities ordered flags attachment or exact scales");
        for(std::size_t p=0;p<x.incidence_count();++p)for(std::size_t aspect=0;aspect<b.aspects().size();++aspect){guard.step();const auto id=static_cast<composite::AspectId>(aspect);
            require(y.aspects.get(id,p)==(aspect<a.aspects().size()?x.aspects.get(id,p):false),"initial source changes original flag aspects or activates new roles");}}
}
} // namespace initial_input_lineage_detail

// An explicit dependency for a separately declared initial source. It retains
// both complete histories and copied raw/source data. It neither identifies
// their owners nor supplies a changed-signature morphism or field transport.
class OwnedRelationalInitialInputLineage {
    struct State { OwnedRelationalInputUseAsset original,initial; RelationalInitialInputLineageDeclaration declaration; };
public:
    OwnedRelationalInitialInputLineage(OwnedRelationalInputUseAsset original,OwnedRelationalInputUseAsset initial,
        RelationalInitialInputLineageDeclaration declaration,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        using namespace initial_input_lineage_detail;
        const auto old=original,next=initial;const auto d=declaration;Guard guard(limits,std::move(check));
        const auto a=old.publication(),b=next.publication();
        require(d.key&&d.revision&&!old.same_owner(next)&&a.source_namespace()!=b.source_namespace(),"initial lineage needs distinct actual asset namespaces and identity");
        require(old.declaration().bytes==next.declaration().bytes&&old.declaration().encoding==next.declaration().encoding,"initial lineage changes original byte channel");
        source_values(a.endpoint().source,guard);source_values(b.endpoint().source,guard);source_copy(a.endpoint().source,b.endpoint().source,guard);
        require(d.uses.size()==old.declaration().uses.size()&&d.uses.size()==next.declaration().uses.size(),"initial lineage omits original uses");
        std::set<std::uint64_t> seen;
        for(std::size_t i=0;i<d.uses.size();++i){guard.step();const auto u=d.uses[i];
            require(u.original==old.declaration().uses[i].key&&seen.insert(u.initial).second,"initial lineage use order or bijection differs");
            const auto& x=old.use(u.original);const auto& y=next.use(u.initial);
            require(x.cells==y.cells&&x.occurrences==y.occurrences&&x.spans==y.spans,"initial lineage changes original use scope or ordered spans");}
        state_=std::make_shared<const State>(State{old,next,d});
    }
    OwnedRelationalInputUseAsset original()const{return state_->original;}
    OwnedRelationalInputUseAsset initial()const{return state_->initial;}
    const RelationalInitialInputLineageDeclaration& declaration()const &{return state_->declaration;}
    const RelationalInitialInputLineageDeclaration& declaration()const &&=delete;
private:
    std::shared_ptr<const State> state_;
};
inline std::vector<std::byte> relational_initial_input_lineage_payload(const OwnedRelationalInitialInputLineage& lineage) {
    return initial_input_lineage_detail::payload(encode_relational_input_use_asset(lineage.original()),
        encode_relational_input_use_asset(lineage.initial()),lineage.declaration());
}
class AvailableRelationalInitialInputLineage {
    struct State { OwnedRelationalInitialInputLineage lineage; RelationalInitialInputLineageOrigins keys; std::vector<OwnedRelationalValueOrigin> obligations; };
public:
    AvailableRelationalInitialInputLineage(OwnedRelationalInitialInputLineage lineage,RelationalInitialInputLineageOrigins keys,
        const RelationalAvailabilityContext& context) {
        const auto authority=context;std::vector<OwnedRelationalValueOrigin> origins;
        origins.push_back(authority.require(keys.original,RelationalDependencyRole::InputAsset,encode_relational_input_use_asset(lineage.original())));
        origins.push_back(authority.require(keys.initial,RelationalDependencyRole::InputAsset,encode_relational_input_use_asset(lineage.initial())));
        origins.push_back(authority.require(keys.interface,RelationalDependencyRole::Interface,relational_initial_input_lineage_payload(lineage)));
        state_=std::make_shared<const State>(State{std::move(lineage),keys,std::move(origins)});
    }
    OwnedRelationalInitialInputLineage lineage()const{return state_->lineage;}
    RelationalInitialInputLineageOrigins origins()const{return state_->keys;}
    const std::vector<OwnedRelationalValueOrigin>& obligations()const &{return state_->obligations;}
    const std::vector<OwnedRelationalValueOrigin>& obligations()const &&=delete;
    void admit(const RelationalAvailabilityContext& context,std::function<void()> check={})const {
        const auto pin=state_;const auto authority=context;const auto callback=std::move(check);
        for(const auto& origin:pin->obligations){if(callback)callback();authority.recheck(origin);}
    }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::inference
