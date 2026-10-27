#pragma once
#include "sheaf_gluing_data.hpp"
#include "rfc/inference/native_finite_sheaf_sections.hpp"

namespace finite_sections_example {
using namespace sheaf_gluing_example;
using Status=local::FiniteSearchStatus;
struct Data {
    Dataset maps;
    TemporalEndpoint selectors;
    std::vector<NativeFiniteSheafCatalogue> catalogues;
};
inline TemporalEndpoint choice_source(){
    core::RelationalComplexBuilder b(signature());for(std::uint64_t i=0;i<3;++i)b.declare_witness_identity(80000+i);
    return TemporalEndpoint(b.freeze(),7);
}
inline std::vector<DeclaredFieldInput> add_candidate(Fixture& f,std::size_t patch,std::uint64_t key,bool constant=true,bool common_bit=false){
    std::vector<DeclaredFieldInput> result;
    const auto n=f.contract.upward.stalks[0].carrier.domain.logical_size();
    Guard guard;
    for(std::size_t c=0;c<2;++c){
        const auto& original=f.inputs[0][c];
        const auto ts=temporal_endpoint_detail::tensors(f.endpoint);const auto vs=temporal_declaration_detail::keyed(f.endpoint.declarations->values);
        auto t=*ts.at(original.tensor);auto v=*vs.at(original.value);
        S split(n);if(constant || c==0)split=S::basis(n,common_bit?0:1,Q{static_cast<unsigned long>(key)});
        t.key=50000+key*10+c;v.key=60000+key*10+c;
        const auto value=certified_colimit_detail::apply(chart(patch),split,guard,0);
        t.tensor={t.tensor.domain(),numeric::StructuralSupport::full(n),value};v.value=DeclaredValue::tensor(t.tensor);
        result.push_back({t.key,v.key});f.endpoint.tensors.push_back(t);f.endpoint.declarations->values.push_back(v);
    }return result;
}
inline Data data(bool constant=true,bool common_bit=false){
    Data d{dataset(),choice_source(),{}};
    for(std::size_t p=0;p<3;++p){NativeFiniteSheafCatalogue cat{std::string(1,char('x'+p)),{}};
        for(std::uint64_t k=0;k<2;++k)cat.alternatives.push_back({k,add_candidate(d.maps.locals[p],p,k,constant,common_bit)});
        d.catalogues.push_back(std::move(cat));validate_temporal_endpoint(d.maps.locals[p].endpoint);}
    return d;
}
inline std::vector<FiniteSheafCondition> policies(bool conflict=true){
    std::vector<FiniteSheafCondition> c{{"x=y",{0,1},{{0,0},{1,1}}},{"y=z",{1,2},{{0,0},{1,1}}}};
    if(conflict)c.push_back({"z!=x",{2,0},{{0,1},{1,0}}});
    return c;
}
inline NativeActionAvailability availability(){auto out=available();out.push_back({1200,7});return out;}
inline storage::PinnedNativeAccession selector_pin(const Data& d){NativeAccessionDirectory dir(d.selectors,1200);return dir.pin(NativeAccessionRequest::sparse_chain(g0,{},{}));}
inline std::vector<core::CellId> selector_cells(){return {{g0,0},{g0,1},{g0,2}};}
inline PreparedNativeFiniteSheafSections prepare_family(const Data& d,std::vector<FiniteSheafCondition> clauses={},bool sections=true,
    std::vector<NativeFiniteSheafObservation> readings={},FiniteSheafFamilyLimits bounds={}){
    return PreparedNativeFiniteSheafSections::prepare(plan(d.maps),selector_pin(d),selector_cells(),d.catalogues,std::move(readings),std::move(clauses),sections,availability(),bounds);
}
inline NativeFiniteSheafObservation observation(Data& d,std::size_t p,std::size_t coordinate,Q value,std::string label="observed"){
    auto& f=d.maps.locals[p];const auto s=f.contract.upward.stalks[0];const auto n=s.carrier.domain.logical_size();
    const auto ts=temporal_endpoint_detail::tensors(f.endpoint);const auto vs=temporal_declaration_detail::keyed(f.endpoint.declarations->values);
    auto t=*ts.at(f.inputs[0][0].tensor);auto v=*vs.at(f.inputs[0][0].value);t.key=90000;v.key=91000;
    t.tensor={t.tensor.domain(),numeric::StructuralSupport::sparse(n,{coordinate}),S::basis(n,coordinate,value)};v.value=DeclaredValue::tensor(t.tensor);
    f.endpoint.tensors.push_back(t);f.endpoint.declarations->values.push_back(v);return {label,p,s,{t.key,v.key}};
}
inline std::vector<NativeFiniteSheafConditionRecord> store_conditions(Data& d,std::vector<FiniteSheafCondition> cs){
    TemporalDeclarationState ds;ds.namespace_key=97000;ds.profile=BooleanLayoutProfile::Lsb0OctetsV2;
    std::vector<Declaration> defs;std::vector<NativeFiniteSheafConditionRecord> records;
    const auto sym=Declaration::finite({97000,1,1},{0,1});defs.push_back(sym);
    for(std::size_t i=0;i<cs.size();++i){const auto& c=cs[i];std::vector<DeclarationFactor> factors;
        for(const auto p:c.patches)factors.push_back({storage::stable_cell_key(d.selectors.source,{g0,static_cast<std::uint32_t>(p)}).key,sym.id,0});
        const auto tuple=Declaration::product({97000,10+i,1},factors);const auto table=Declaration::repeated({97000,20+i,1},tuple.id,0,4);defs.push_back(tuple);defs.push_back(table);
        std::vector<DeclaredValue> rows;for(const auto& row:c.allowed){std::vector<DeclaredValue> xs;for(const auto key:row)xs.push_back(DeclaredValue::symbol(key));rows.push_back(DeclaredValue::product(std::move(xs)));}
        TemporalDeclaredValue v;v.key=100+i;v.type=table.id;v.value=DeclaredValue::repeated(std::move(rows));ds.values.push_back(v);records.push_back({c.label,c.patches,v.key,table});
    }ds.registry=DeclarationRegistry(defs);d.selectors.declarations=std::move(ds);validate_temporal_endpoint(d.selectors);return records;
}
inline PreparedNativeFiniteSheafSections recorded_family(const Data& d,const std::vector<NativeFiniteSheafConditionRecord>& records,
    bool sections=true,std::vector<NativeFiniteSheafObservation> readings={}){
    const auto clauses=finite_sheaf_conditions_from_records(d.selectors,selector_cells(),d.catalogues,records);
    return prepare_family(d,clauses,sections,std::move(readings));
}
} // namespace finite_sections_example
