#pragma once
#include "chart_gluing_data.hpp"
#include "rfc/inference/native_injective_gluing.hpp"
namespace injective_gluing_example {
using namespace chart_gluing_example;
struct Dataset {
    Fixture common;std::vector<Fixture> locals;
    std::array<algebra::RetainedSourceMap,2> maps;
    std::vector<InjectiveStalkInterfaceRecords> requests;
};
inline void set_field(Fixture& f,core::Grade grade,const S& value){
    for(auto& t:f.endpoint.tensors)if(t.role==TemporalTensorRole::Field && t.owner.cell.grade==grade){
        t.tensor={t.tensor.domain(),numeric::StructuralSupport::full(value.size()),value};
        for(auto& v:f.endpoint.declarations->values)if(v.cell==t.owner.cell)v.value=DeclaredValue::tensor(t.tensor);
    }
}
inline Fixture transport(Fixture f,const M& map){
    const auto& a=f.endpoint.source.grade(g1);f.contract.downward.allow_constant_identity=false;
    for(std::size_t slot=0;slot<a.incidence_count();++slot){
        const auto find=[&](core::CellId c){const auto key=stable_cell_key(f.endpoint.source,c);for(const auto& b:f.contract.downward.stalks)if(b.carrier.cell==key)return b.carrier.key;throw std::logic_error("cell");};
        M m=map;for(const auto& [ij,q]:map.entries())m.set(ij.first,ij.second,f.endpoint.source.incidence_coefficient(g1,slot)*q);
        TemporalTensor t;t.key=400+slot;t.role=TemporalTensorRole::RealizedIncidenceMap;t.input=find({g1,a.upper_index[slot]});t.output=find({g0,a.lower_index[slot]});t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence={g1,a.occurrence_identities[slot]};t.tensor=temporal_matrix_tensor(m);
        f.endpoint.tensors.push_back(t);f.contract.downward.maps.push_back({t.key,t.role});
    }
    return f;
}
inline Dataset assemble(Fixture common,Fixture left,Fixture right,std::array<algebra::RetainedSourceMap,2> maps){
    const auto a=changed(left,0,13100),b=changed(right,1,13200);
    Dataset d{std::move(common),{a.field,b.field},maps,{}};
    for(const auto& k:d.common.contract.downward.stalks){
        StableSourceIndex ix(d.common.endpoint.source);const auto c=*ix.cell(*k.carrier.cell);
        std::array<std::size_t,2> at{};
        for(std::size_t p=0;p<2;++p){
            const auto lc=stable_cell_key(d.locals[p].endpoint.source,{c.grade,maps[p].cells[c.grade.value][c.index]});
            const auto& cs=p?b.charts:a.charts;bool found=false;
            for(std::size_t i=0;i<cs.size();++i)if(cs[i].local.carrier.cell==lc){at[p]=i;found=true;break;}
            if(!found)throw std::logic_error("example interface incomplete");
        }
        d.requests.push_back({k,{a.charts[at[0]],b.charts[at[1]]}});
    }
    return d;
}
inline Dataset make_dataset(bool extras=false){
    auto k=make(100,10,20,1,false,extras),a=make(100,10,20,2,false,extras),b=make(100,10,20,3,false,extras);
    a=transport(a,M::from_rows({{1,1},{0,1}}));b=transport(b,M::from_rows({{1,0,1},{0,1,0},{0,0,1}}));
    set_field(k,g1,S::basis(1,0,2));set_field(a,g1,S(2,{{0,2},{1,3}}));set_field(b,g1,S(3,{{0,2},{1,5},{2,7}}));
    const auto m=identity_map(k.endpoint.source);return assemble(k,a,b,{m,m});
}
inline PreparedNativeInjectiveGluing prepare(const Dataset& d,numeric::ExactActionLimits limits={}){
    NativeAccessionDirectory common(d.common.endpoint,502);
    return PreparedNativeInjectiveGluing::prepare({"K",common.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),d.common.contract},publications(d.locals),d.maps,d.requests,limits);
}
inline TemporalEndpoint coordinates(const InjectiveLocalFieldView& v){
    core::RelationalComplexBuilder b(signature());TemporalEndpoint e(b.freeze(),7);
    for(std::size_t k=0;k<v.grade_count();++k)e.carriers.push_back({800+k,TemporalCarrierKind::Explicit,{},{},13800+k,numeric::IndexDomain::linear(v.dimension({static_cast<std::uint16_t>(k)}))});
    return e;
}
inline PreparedNativeInjectiveGluing bind_plan(const PreparedNativeInjectiveGluing& p,const TemporalEndpoint& e){
    NativeAccessionDirectory d(e,999);const auto anchor=d.pin(NativeAccessionRequest::sparse_chain(g0,{},{}));std::vector<NativeInjectiveCarrierBinding> cs;
    for(std::size_t k=0;k<p.view().grade_count();++k)cs.push_back({NativeCarrier::bind(anchor,800+k),p.view().layout({static_cast<std::uint16_t>(k)})});
    return p.bind_carriers(std::move(cs));
}
inline PreparedNativeInjectiveGluing plan(const Dataset& d){const auto p=prepare(d);return bind_plan(p,coordinates(p.view()));}
inline NativeActionMomentResult result(const PreparedNativeInjectiveGluing& p,const Dataset& d){
    const auto x=p.glue_field(g1,inputs(d.locals,1),availability(3));const auto op=p.boundary(g1);
    return PreparedNativeActionMoments({{"left including shared",p.boundary(g1,0)},{"right complement",p.boundary(g1,1)}},NativeBilinearForm::named_unit(op.output()),op).execute(x,availability(3));
}
} // namespace injective_gluing_example
