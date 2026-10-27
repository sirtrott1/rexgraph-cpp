#pragma once
#include "rfc/storage/finite_sheaf_sections.hpp"
#include "rfc/inference/native_sheaf_gluing.hpp"

namespace rfc::inference {
struct NativeFiniteSheafCandidate {
    std::uint64_t key{};
    std::vector<storage::DeclaredFieldInput> fields;
};
struct NativeFiniteSheafCatalogue {
    std::string label;
    std::vector<NativeFiniteSheafCandidate> alternatives;
};
struct NativeFiniteSheafObservation {
    std::string label;
    std::size_t patch{};
    storage::DeclaredStalkBinding stalk;
    storage::DeclaredFieldInput record;
};

// Existing repeated/product/finite-symbol records, with product factor identities
// equal to the retained selector-cell keys. No new wire format or truth-bit cast.
struct NativeFiniteSheafConditionRecord {
    std::string label;
    std::vector<std::size_t> patches;
    std::uint64_t value{};
    composite::Declaration expected_table;
};
inline std::vector<storage::FiniteSheafCondition> finite_sheaf_conditions_from_records(
    const storage::TemporalEndpoint& endpoint,const std::vector<core::CellId>& selectors,
    const std::vector<NativeFiniteSheafCatalogue>& cats,
    const std::vector<NativeFiniteSheafConditionRecord>& records,storage::FiniteSheafFamilyLimits limits={}) {
    const auto require=[](bool b,const char* m){if(!b)throw std::invalid_argument(m);};
    require(endpoint.declarations.has_value(),"finite condition declarations missing");
    if(records.size()>limits.clauses)throw std::length_error("finite stored clause budget");
    const auto& registry=endpoint.declarations->registry;
    const auto values=storage::temporal_declaration_detail::keyed(endpoint.declarations->values);
    std::vector<storage::FiniteSheafCondition> out;std::size_t entries=0;
    for(const auto& r:records){
        require(registry.at(r.expected_table.id)==r.expected_table && r.expected_table.kind==composite::DeclarationKind::Repeated,"finite table definition differs");
        const auto& tuple=registry.at(r.expected_table.element);
        require(tuple.kind==composite::DeclarationKind::Product && tuple.factors.size()==r.patches.size(),"finite condition product arity");
        for(std::size_t i=0;i<r.patches.size();++i){const auto p=r.patches[i];require(p<cats.size() && p<selectors.size(),"finite table patch selector");
            const auto& factor=tuple.factors[i];
            require(factor.key==storage::stable_cell_key(endpoint.source,selectors[p]).key && factor.grade==0,"finite table selector identity/order differs");
            const auto& type=registry.at(factor.type);std::vector<std::uint64_t> keys;for(const auto& c:cats[p].alternatives)keys.push_back(c.key);
            require(type.kind==composite::DeclarationKind::Finite && type.symbols==keys,"finite table factor domain/order differs");
        }
        require(values.contains(r.value),"finite condition value missing");const auto& v=*values.at(r.value);
        require(v.type==r.expected_table.id && !v.cell && !v.occurrence && !v.block && !v.branch,"finite table type/owner differs");
        const auto* rows=std::get_if<composite::DeclaredValue::Repeated>(&v.value.data);require(rows!=nullptr,"finite table is not repeated");
        require(rows->elements.size()>=r.expected_table.min_count && rows->elements.size()<=r.expected_table.max_count,"finite table repetition bounds differ");
        entries=machine::checked_size_add(entries,machine::checked_size_mul(rows->elements.size(),r.patches.size()));
        if(entries>limits.tuple_entries)throw std::length_error("finite stored tuple budget");
        storage::FiniteSheafCondition c{r.label,r.patches,{}};
        for(const auto& row:rows->elements){const auto* product=std::get_if<composite::DeclaredValue::Product>(&row.data);require(product && product->elements.size()==r.patches.size(),"finite tuple arity/type differs");std::vector<std::uint64_t> xs;
            for(const auto& x:product->elements){const auto* symbol=std::get_if<composite::DeclaredValue::Symbol>(&x.data);require(symbol!=nullptr,"finite table needs declared symbols");xs.push_back(symbol->key);}c.allowed.push_back(std::move(xs));}
        out.push_back(std::move(c));
    }return out;
}

// Finite catalogue admission and logical clauses are explicit caller contracts,
// not restored execution authority. Every field/reading comes from the actual
// pinned local endpoint. Selector identities have their own pinned source; they
// are NOT synthetic cells appended to the physical complex.
class PreparedNativeFiniteSheafSections {
    struct State {
        PreparedNativeSheafGluing gluing;
        storage::FiniteSheafSectionFamily family;
        std::vector<storage::PinnedNativeAccession> dependencies, roots;
        numeric::ExactActionLimits action_limits;
    };
    std::shared_ptr<const State> state_;
    static void require(bool b,const char* m){if(!b)throw std::invalid_argument(m);}
    explicit PreparedNativeFiniteSheafSections(std::shared_ptr<const State> s):state_(std::move(s)){}
    static void available(const State& s,const NativeActionAvailability& policy){
        for(const auto& p:s.dependencies)native_action_detail::available(p,policy);
    }
public:
    static PreparedNativeFiniteSheafSections prepare(PreparedNativeSheafGluing gluing,
        storage::PinnedNativeAccession selectors,std::vector<core::CellId> selector_cells,
        std::vector<NativeFiniteSheafCatalogue> catalogues,
        std::vector<NativeFiniteSheafObservation> observations,
        std::vector<storage::FiniteSheafCondition> conditions,bool require_sections,
        const NativeActionAvailability& availability,storage::FiniteSheafFamilyLimits bounds={},
        numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}) {
        const auto pin=gluing.state_;
        require(!pin->carriers.empty(),"finite section plan requires bound global carriers");
        require(selectors.kind()==storage::NativeAccessionKind::Chain && selectors.support_count()==0,"finite choices require a pinned empty source anchor");
        require(catalogues.size()==pin->patches.size(),"one finite catalogue per actual patch");
        if(catalogues.size()>bounds.patches)throw std::length_error("finite sheaf patch budget");
        numeric::ExactActionGuard guard(limits,checkpoint);guard.entries(pin->view.retained_records());
        std::vector<storage::PinnedNativeAccession> dependencies,roots;
        native_action_detail::append_pin(dependencies,selectors);
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        for(const auto& p:pin->common)native_action_detail::append_pin(dependencies,p.anchor);
        native_action_detail::append_pin(dependencies,pin->certificate);
        for(const auto& c:pin->carriers)native_action_detail::append_pin(dependencies,c.carrier.publication);
        for(const auto& p:dependencies)native_action_detail::available(p,availability);
        std::size_t alternatives=0,records=pin->view.retained_records();
        std::vector<storage::FiniteSheafCatalogue> cats;
        for(std::size_t p=0;p<catalogues.size();++p){
            alternatives=machine::checked_size_add(alternatives,catalogues[p].alternatives.size());
            if(alternatives>bounds.alternatives)throw std::length_error("finite sheaf alternative budget");
            storage::FiniteSheafCatalogue cat{catalogues[p].label,{}};
            for(const auto& c:catalogues[p].alternatives){
                guard.step();
                for(const auto& f:c.fields){guard.step();storage::NativeAccessionRequest r;r.kind=storage::NativeAccessionKind::Field;r.subject=f.tensor;
                    const auto root=pin->patches[p].anchor.access(r);native_action_detail::append_pin(roots,root);native_action_detail::append_pin(dependencies,root);
                    records=machine::checked_size_add(records,machine::checked_size_add(1,root.tensor_declaration()->tensor.values().nonzeros()));guard.entries(records);}
                cat.alternatives.push_back({c.key,pin->view.patches()[p].bind_upward({0},c.fields,limits)});
            }cats.push_back(std::move(cat));
        }
        std::vector<storage::FiniteSheafObservation> readings;
        for(const auto& o:observations){
            guard.step();require(o.patch<pin->patches.size(),"observation patch out of range");const auto& b=pin->view.patches()[o.patch];
            const auto cell=storage::sheaf_cover_detail::matched(b,o.stalk);require(cell.grade.value==0,"finite observation requires degree zero");
            const auto& endpoint=b.endpoint();const auto ts=storage::temporal_endpoint_detail::tensors(endpoint);
            const auto vs=storage::temporal_declaration_detail::keyed(endpoint.declarations->values);
            require(ts.contains(o.record.tensor) && vs.contains(o.record.value),"observation tensor or typed value absent");
            const auto& t=*ts.at(o.record.tensor);const auto& v=*vs.at(o.record.value);
            require(t.role==storage::TemporalTensorRole::Field && t.owner.kind==storage::TemporalOwnerKind::Cell && t.owner.cell==*o.stalk.carrier.cell && !t.input && t.output==o.stalk.carrier.key && t.tensor.domain()==o.stalk.carrier.domain,"observation role/owner/coordinates differ");
            require(v.cell==t.owner.cell && !v.occurrence && !v.block && !v.branch && v.type==o.stalk.declaration.id,"observation typed owner/type differs");
            const auto* value=std::get_if<numeric::SparseRationalTensor>(&v.value.data);
            require(value && storage::temporal_endpoint_detail::tensor_equal(*value,t.tensor),"observation data/support differ");
            guard.sparse_vector(t.tensor.values());records=machine::checked_size_add(records,1+t.tensor.values().nonzeros());guard.entries(records);
            readings.push_back({o.label,o.patch,o.stalk,t.tensor});
        }
        auto family=storage::FiniteSheafSectionFamily(pin->view,core::OwnedRelationalSource(selectors.endpoint()->source),std::move(selector_cells),std::move(cats),std::move(readings),std::move(conditions),require_sections,bounds,limits,checkpoint);
        for(const auto& p:dependencies)native_action_detail::available(p,availability);
        return PreparedNativeFiniteSheafSections(std::make_shared<const State>(State{std::move(gluing),std::move(family),std::move(dependencies),std::move(roots),limits}));
    }
    const PreparedNativeSheafGluing& gluing()const &{return state_->gluing;}
    const PreparedNativeSheafGluing& gluing()const &&=delete;
    const storage::FiniteSheafSectionFamily& family()const &{return state_->family;}
    const storage::FiniteSheafSectionFamily& family()const &&=delete;
    storage::FiniteSheafQuery query(const NativeActionAvailability& availability,
        const std::map<std::size_t,std::uint64_t>& fixed={},local::FiniteSearchLimits limits={},std::stop_token stop={})const{
        const auto pin=state_;available(*pin,availability);auto r=pin->family.query(fixed,limits,stop);
        available(*pin,availability);r.family_owner=pin;return r;
    }
    storage::FiniteSheafConflict conflict(const NativeActionAvailability& availability,
        local::FiniteSearchLimits limits={},std::stop_token stop={})const{
        const auto pin=state_;available(*pin,availability);auto r=pin->family.conflict(limits,stop);
        available(*pin,availability);r.family_owner=pin;return r;
    }
    // Materialize ONLY an explicitly selected admissible tuple. A witness is not
    // the complete result family or a uniquely inferred set of field values.
    NativeFieldValue materialize(const storage::FiniteSheafQuery& query,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{})const{
        const auto pin=state_;lease.check_cancelled();available(*pin,availability);
        require(query.family_owner.get()==pin.get(),"finite witness belongs to a foreign plan");
        require(query.status==local::FiniteSearchStatus::Satisfiable && query.witness,"finite materialization requires a satisfiable witness");
        runtime::admit_execution({pin->family.clauses().size(),0,0,0,0},lease.policy());
        for(const auto& [p,key]:query.fixed)require(p<query.witness->size() && query.witness->at(p)==key,"finite witness differs from fixed query assumptions");
        auto limits=native_action_detail::limits(lease,pin->dependencies,pin->action_limits.dimension);
        numeric::ExactActionGuard g(limits,[&lease]{lease.check_cancelled();});
        auto x=pin->family.glue_assignment(*query.witness,g);lease.check_cancelled();available(*pin,availability);
        const auto carrier=pin->gluing.state_->carriers.at(0).carrier;
        runtime::admit_execution({pin->family.clauses().size(),x.nonzeros(),x.nonzeros(),0,0},lease.policy());
        return NativeFieldValue({carrier,std::move(x),pin->roots,pin->dependencies,{pin},{"native-finite-sheaf-section-choice-v1"}});
    }
};
} // namespace rfc::inference
