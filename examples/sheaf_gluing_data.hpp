#pragma once
#include "local_gluing_data.hpp"
#include "rfc/inference/native_sheaf_gluing.hpp"

namespace sheaf_gluing_example {
using namespace local_gluing_example;
inline M product(const M& a,const M& b){Guard g;return certified_colimit_detail::product(a,b,g);}
inline M chart(std::size_t p){
    if(p==0)return M::from_rows({{1,1},{0,1}});
    if(p==1)return M::identity(3);
    return M::from_rows({{1,0},{1,1}});
}
inline M inverse_chart(std::size_t p){
    if(p==0)return M::from_rows({{1,-1},{0,1}});
    if(p==1)return M::identity(3);
    return M::from_rows({{1,0},{-1,1}});
}
inline M split_restriction(std::size_t p){
    if(p==0)return M::from_rows({{2,0},{1,3}});
    if(p==1)return M::from_rows({{2,0,0},{-1,4,0},{2,0,5}});
    return M::from_rows({{2,0},{3,-2}});
}
inline ObservationCoordinateSpace space(std::string name,std::size_t n,std::uint64_t id){
    std::vector<std::uint64_t> names;for(std::size_t i=0;i<n;++i)names.push_back(100+i);
    return {std::move(name),40000+id,Declaration::rational_tensor({39000,id,1},{{1,n,std::move(names)}})};
}
inline TemporalCarrier explicit_carrier(std::uint64_t key,std::size_t n,std::uint64_t coordinates){return {key,TemporalCarrierKind::Explicit,{},{},coordinates,numeric::IndexDomain::linear(n)};}
inline ObservationMapRecord store(TemporalEndpoint& e,std::uint64_t key,const TemporalCarrier& in,const TemporalCarrier& out,const M& m,TemporalTensorRole role,TemporalTensorOwner owner={}){
    for(const auto& c:{in,out}){const auto it=std::find_if(e.carriers.begin(),e.carriers.end(),[&](const auto& x){return x.key==c.key;});if(it==e.carriers.end())e.carriers.push_back(c);else if(*it!=c)throw std::logic_error("fixture carrier conflict");}
    TemporalTensor t;t.key=key;t.role=role;t.input=in.key;t.output=out.key;t.owner=owner;t.tensor=temporal_matrix_tensor(m);e.tensors.push_back(t);return {key,in,out,role,owner};
}
inline void replace_upward(Fixture& f,const ObservationCoordinateSpace& sp,const M& u){
    f.contract.upward={};f.inputs={};auto defs=f.endpoint.declarations->registry.definitions();defs.push_back(sp.declaration);
    std::map<StableCellKey,TemporalCarrier> carriers;
    for(std::size_t k=0;k<f.endpoint.source.grade_count();++k){const core::Grade g{static_cast<std::uint16_t>(k)};
        for(std::size_t i=0;i<f.endpoint.source.grade(g).cell_count();++i){const auto cell=stable_cell_key(f.endpoint.source,{g,static_cast<std::uint32_t>(i)});
            const auto n=sp.declaration.axes[0].extent;const auto key=500+20*k+i;
            TemporalCarrier c{key,TemporalCarrierKind::CellStalk,g,cell,sp.coordinates,numeric::IndexDomain::linear(n)};carriers.emplace(cell,c);
            f.endpoint.carriers.push_back(c);f.contract.upward.stalks.push_back({c,sp.declaration});
            TemporalTensor t;t.key=key;t.role=TemporalTensorRole::Field;t.output=c.key;t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell=cell;t.tensor={c.domain,numeric::StructuralSupport::full(n),S(n)};
            f.endpoint.tensors.push_back(t);TemporalDeclaredValue v;v.key=key+1000;v.type=sp.declaration.id;v.cell=cell;v.value=DeclaredValue::tensor(t.tensor);f.endpoint.declarations->values.push_back(v);f.inputs[k].push_back({key,v.key});
        }
    }
    for(std::size_t k=1;k<f.endpoint.source.grade_count();++k){const core::Grade g{static_cast<std::uint16_t>(k)};const auto& a=f.endpoint.source.grade(g);
        for(std::size_t slot=0;slot<a.incidence_count();++slot){const auto l=stable_cell_key(f.endpoint.source,{{static_cast<std::uint16_t>(k-1)},a.lower_index[slot]}),h=stable_cell_key(f.endpoint.source,{g,a.upper_index[slot]});
            TemporalTensor t;t.key=10000+100*k+slot;t.role=TemporalTensorRole::SheafRestriction;t.input=carriers.at(l).key;t.output=carriers.at(h).key;t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence={g,a.occurrence_identities[slot]};t.tensor=temporal_matrix_tensor(u);f.endpoint.tensors.push_back(t);f.contract.upward.maps.push_back({t.key,t.role});
        }
    }
    f.endpoint.declarations->registry=DeclarationRegistry(std::move(defs));
}
inline void set_upward(Fixture& f,core::Grade g,std::uint32_t cell,const S& value){
    const auto sk=stable_cell_key(f.endpoint.source,{g,cell});
    for(const auto& b:f.inputs.at(g.value))for(auto& t:f.endpoint.tensors)if(t.key==b.tensor && t.owner.cell==sk){
        t.tensor={t.tensor.domain(),numeric::StructuralSupport::full(value.size()),value};
        for(auto& v:f.endpoint.declarations->values)if(v.key==b.value)v.value=DeclaredValue::tensor(t.tensor);
    }
}
// The tiny matrices below are supplied example certificates, not a production
// kernel/complement discovery routine. C has AB, AC, and optional redundant BC.
inline StalkObservationWitness witness(bool redundant=true){
    M a(7,5),e(5,7),h(7,redundant?3:2);
    a.set(0,0,1);a.set(1,1,1);a.set(2,0,1);a.set(3,2,1);a.set(4,3,1);a.set(5,0,1);a.set(6,4,1);
    e.set(0,0,Q{1,2});e.set(0,2,Q{1,2});e.set(1,1,1);e.set(2,3,1);e.set(3,4,1);e.set(4,6,1);
    h.set(0,0,Q{1,2});h.set(2,0,Q{-1,2});h.set(5,0,Q{1,2});h.set(5,1,-1);
    M s(7,7),r(7,7);std::size_t off=0;
    for(std::size_t p=0;p<3;++p){const auto c=chart(p),ci=inverse_chart(p);for(const auto& [ij,q]:c.entries())s.set(off+ij.first,off+ij.second,q);for(const auto& [ij,q]:ci.entries())r.set(off+ij.first,off+ij.second,q);off+=c.rows();}
    return {space("compatible",5,5),product(s,a),product(e,r),product(s,h)};
}
struct Dataset {std::vector<Fixture> locals,commons;std::vector<NativeObservationInterfaceRecords> requests;TemporalEndpoint certificates;std::vector<NativeStalkObservationRecords> records;};
inline Dataset dataset(bool extras=false,bool higher=false,bool redundant=true){
    auto src=edge(100,10,20,extras);
    if(higher){core::RelationalComplexBuilder b(signature());b.declare_grade({2});b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{30,true,false,1}});b.add_grade_one(core::RelationKey{30},0,{{10,true,true,1},{30,true,false,1}});b.add_higher({2},core::RelationKey{40},0,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});src=b.freeze();}
    core::RelationalComplexBuilder b(signature());Dataset d{{},{},{},TemporalEndpoint(b.freeze(),7),{}};
    const std::array<S,3> split_values{S(2,{{0,2},{1,3}}),S(3,{{0,2},{1,5},{2,7}}),S(2,{{0,2},{1,11}})};
    for(std::size_t p=0;p<3;++p){auto f=make_source(src);replace_upward(f,space(std::string(1,char('A'+p)),p==1?3:2,10+p),product(chart(p),product(split_restriction(p),inverse_chart(p))));Guard g;set_upward(f,g0,0,certified_colimit_detail::apply(chart(p),split_values[p],g,0));d.locals.push_back(std::move(f));}
    const std::array<std::array<std::size_t,2>,3> pairs{{{0,1},{0,2},{1,2}}};std::uint64_t key=20000;
    for(std::size_t i=0;i<(redundant?3u:2u);++i){auto common=make_source(src);replace_upward(common,space("common",1,1),M::from_rows({{2}}));const auto map=identity_map(src);
        NativeAccessionDirectory cd(common.endpoint,600+i);NativeObservationInterfaceRecords req{{"K"+std::to_string(i),cd.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),common.contract},pairs[i][0],pairs[i][1],{map,map},{}};
        for(std::size_t c=0;c<common.contract.upward.stalks.size();++c){const auto& ks=common.contract.upward.stalks[c];NativeObservationInterfaceStalkRecords st{ks,{}};
            for(std::size_t side=0;side<2;++side){auto& f=d.locals[pairs[i][side]];auto defs=f.endpoint.declarations->registry.definitions();defs.push_back(ks.declaration);f.endpoint.declarations->registry=DeclarationRegistry(std::move(defs));
                const auto& ls=f.contract.upward.stalks[c];const auto kc=explicit_carrier(key++,1,ks.carrier.coordinate_system);TemporalTensorOwner owner;owner.kind=TemporalOwnerKind::Cell;owner.cell=*ls.carrier.cell;
                Guard g;const auto r=certified_colimit_detail::block(inverse_chart(pairs[i][side]),0,1,0,ls.carrier.domain.logical_size(),g);
                st.restrictions[side]=store(f.endpoint,key++,ls.carrier,kc,r,TemporalTensorRole::Observation,owner);
            }req.stalks.push_back(st);
        }d.requests.push_back(std::move(req));d.commons.push_back(std::move(common));
    }
    const auto w=witness(redundant);std::vector<Declaration> defs{w.target.declaration};for(const auto& f:d.locals)for(const auto& c:f.contract.upward.stalks)defs.push_back(c.declaration);
    TemporalDeclarationState ds;ds.namespace_key=39000;ds.profile=BooleanLayoutProfile::Lsb0OctetsV2;ds.registry=DeclarationRegistry(std::move(defs));d.certificates.declarations=std::move(ds);
    for(std::size_t c=0;c<d.locals[0].contract.upward.stalks.size();++c){NativeStalkObservationRecords r;for(std::size_t p=0;p<3;++p)r.origins.push_back({p,d.locals[p].contract.upward.stalks[c]});r.target=w.target;
        const auto all=explicit_carrier(key++,7,50000+c),target=explicit_carrier(key++,5,w.target.coordinates),defect=explicit_carrier(key++,redundant?3:2,51000+c);
        r.accession=store(d.certificates,key++,target,all,w.accession,TemporalTensorRole::Observation);
        r.reconstruction=store(d.certificates,key++,all,target,w.reconstruction,TemporalTensorRole::Reconstruction);
        r.compatibility_lift=store(d.certificates,key++,defect,all,w.compatibility_lift,TemporalTensorRole::Reconstruction);d.records.push_back(std::move(r));
    }
    for(const auto& f:d.locals){validate_temporal_endpoint(f.endpoint);}
    validate_temporal_endpoint(d.certificates);return d;
}
inline NativeActionAvailability available(std::size_t common_count=3){NativeActionAvailability out{{500,7},{501,7},{502,7},{700,7},{999,7}};for(std::size_t i=0;i<common_count;++i)out.push_back({600+i,7});return out;}
inline PreparedNativeSheafGluing prepare(const Dataset& d,numeric::ExactActionLimits limits={}){
    auto rs=d.requests;for(std::size_t i=0;i<rs.size();++i){NativeAccessionDirectory dir(d.commons[i].endpoint,600+i);rs[i].common={"K"+std::to_string(i),dir.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),d.commons[i].contract};}
    NativeAccessionDirectory dir(d.certificates,700);return PreparedNativeSheafGluing::prepare(publications(d.locals),std::move(rs),dir.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),d.records,limits);
}
inline TemporalEndpoint coordinates(const SheafCoverLocalFieldView& view){core::RelationalComplexBuilder b(signature());TemporalEndpoint e(b.freeze(),7);for(std::size_t k=0;k<view.grade_count();++k)e.carriers.push_back(explicit_carrier(800+k,view.dimension({static_cast<std::uint16_t>(k)}),55000+k));return e;}
inline PreparedNativeSheafGluing bind_plan(const PreparedNativeSheafGluing& p,const TemporalEndpoint& e){NativeAccessionDirectory d(e,999);const auto anchor=d.pin(NativeAccessionRequest::sparse_chain(g0,{},{}));std::vector<NativeObservationCarrierBinding> cs;for(std::size_t k=0;k<p.view().grade_count();++k)cs.push_back({NativeCarrier::bind(anchor,800+k),p.view().layout({static_cast<std::uint16_t>(k)})});return p.bind_carriers(std::move(cs));}
inline PreparedNativeSheafGluing plan(const Dataset& d){const auto p=prepare(d);return bind_plan(p,coordinates(p.view()));}
inline NativeActionMomentResult result(const PreparedNativeSheafGluing& p,const Dataset& d){
    const auto x=p.glue_field(g0,inputs(d.locals,0),available(d.commons.size()));const auto op=p.coboundary(g0);std::vector<NativeMomentChannel> cs;
    for(std::size_t i=0;i<3;++i)cs.push_back({"patch-"+std::to_string(i),p.coboundary(g0,i)});
    const std::array<NativeActionInteractionRequest,3> pairs{{{0,1},{0,2},{1,2}}};return PreparedNativeActionMoments(std::move(cs),NativeBilinearForm::named_unit(op.output()),op).execute(x,available(d.commons.size()),pairs);
}
} // namespace sheaf_gluing_example
