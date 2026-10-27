#pragma once
#include "injective_gluing_data.hpp"
#include "rfc/inference/native_colimit_gluing.hpp"
namespace colimit_example {
using namespace injective_gluing_example;
inline ColimitCoordinateSpace space(std::string name,std::size_t n){return {std::move(name),17000+n,Declaration::rational_tensor({29000,n,1},{{1,n,{}}})};}
inline std::vector<ColimitCoordinateSpace> spaces(){return {space("A",2),space("B",2),space("C",2),space("D",2)};}
inline std::vector<ColimitLinearLink> links(){
    const auto common=space("line",1);const M e1=M::from_rows({{1},{0}}),e2=M::from_rows({{0},{1}}),sum=M::from_rows({{1},{1}});
    const M r1=M::from_rows({{1,0}}),r2=M::from_rows({{0,1}}),half=M::from_rows({{Q{1,2},Q{1,2}}});
    return {{"AB",0,1,common,{{{e1,r1},{e1,r1}}}}, {"AC",0,2,common,{{{e2,r2},{e1,r1}}}}, {"AD",0,3,common,{{{sum,half},{e1,r1}}}}};
}
// Explicit supplied witness, not computed by a production complement finder.
// Local order: A0,A1,B0,B1,C0,C1,D0,D1. Quotient: A0,A1,B1,C1,D1.
inline StalkColimitWitness witness(){
    M q(5,8),h(8,5),z(3,8);
    const std::array<std::size_t,5> representative{0,1,3,5,7};
    for(std::size_t i=0;i<5;++i){q.set(i,representative[i],1);h.set(representative[i],i,1);}
    q.set(0,2,1);q.set(1,4,1);q.set(0,6,1);q.set(1,6,1);
    z.set(0,2,-1);z.set(1,4,-1);z.set(2,6,-1);
    const std::vector<M> rs{M::from_rows({{1,0,0,0,0},{0,1,0,0,0}}),M::from_rows({{1,0,0,0,0},{0,0,1,0,0}}),M::from_rows({{0,1,0,0,0},{0,0,0,1,0}}),M::from_rows({{Q{1,2},Q{1,2},0,0,0},{0,0,0,0,1}})};
    return {space("quotient",5),q,h,z,rs,h.transpose()};
}
inline ColimitMapRecord store(TemporalEndpoint& e,std::uint64_t key,const TemporalCarrier& in,const TemporalCarrier& out,const M& m,TemporalTensorRole role,TemporalTensorOwner owner={}){
    for(const auto& c:{in,out}){const auto it=std::find_if(e.carriers.begin(),e.carriers.end(),[&](const auto& x){return x.key==c.key;});if(it==e.carriers.end())e.carriers.push_back(c);else if(*it!=c)throw std::logic_error("fixture carrier conflict");}
    TemporalTensor t;t.key=key;t.role=role;t.input=in.key;t.output=out.key;t.owner=owner;t.tensor=temporal_matrix_tensor(m);e.tensors.push_back(t);return {key,in,out,role,owner};
}
inline TemporalCarrier explicit_carrier(std::uint64_t key,std::size_t n,std::uint64_t coords){return {key,TemporalCarrierKind::Explicit,{},{},coords,numeric::IndexDomain::linear(n)};}
struct UnsplitDataset {
    std::vector<Fixture> locals,commons;
    std::vector<NativeColimitInterfaceRecords> requests; // anchors populated in prepare
    TemporalEndpoint certificates;
    std::vector<NativeStalkColimitRecords> records;
};
inline UnsplitDataset dataset(bool extras=false,bool higher=false){
    core::RelationalComplex source=edge(100,10,20,extras);
    if(higher){core::RelationalComplexBuilder b(signature());b.declare_grade({2});
        b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});
        b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{30,true,false,1}});
        b.add_grade_one(core::RelationKey{30},0,{{10,true,true,1},{30,true,false,1}});
        b.add_higher({2},core::RelationKey{40},0,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});source=b.freeze();}
    core::RelationalComplexBuilder empty(signature());UnsplitDataset d{{},{},{},TemporalEndpoint(empty.freeze(),7),{}};
    const std::array<M,4> ts{M::identity(2),M::from_rows({{1,2},{0,1}}),M::from_rows({{1,3},{0,1}}),M::from_rows({{1,-1},{0,1}})};
    const std::array<S,4> xs{S(2,{{0,2},{1,3}}),S(2,{{0,2},{1,5}}),S(2,{{0,3},{1,7}}),S(2,{{0,Q{5,2}},{1,11}})};
    for(std::size_t p=0;p<4;++p){auto f=make_source(source,2);if(!higher)f=transport(std::move(f),ts[p]);set_field(f,g0,S(2));set_field(f,g1,xs[p]);if(higher)set_field(f,{2},xs[p]);d.locals.push_back(std::move(f));}
    const auto ls=links();std::uint64_t key=2000;
    for(std::size_t e=0;e<3;++e){
        auto common=make_source(source,1);const auto structural=identity_map(source);
        NativeAccessionDirectory cd(common.endpoint,600+e);NativeColimitInterfaceRecords r{{"K"+std::to_string(e),cd.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),common.contract},0,e+1,{structural,structural},{}};
        for(std::size_t c=0;c<common.contract.downward.stalks.size();++c){const auto& k=common.contract.downward.stalks[c];NativeColimitInterfaceStalkRecords st{k,{}};
            for(std::size_t side=0;side<2;++side){const auto p=side?e+1:0;auto& f=d.locals[p];auto defs=f.endpoint.declarations->registry.definitions();defs.push_back(k.declaration);f.endpoint.declarations->registry=DeclarationRegistry(std::move(defs));
                const auto& local=f.contract.downward.stalks[c];const auto kc=explicit_carrier(key++,1,k.carrier.coordinate_system);TemporalTensorOwner owner;owner.kind=TemporalOwnerKind::Cell;owner.cell=*local.carrier.cell;
                st.sides[side][0]=store(f.endpoint,key++,kc,local.carrier,ls[e].sides[side].map,TemporalTensorRole::PhysicalRealization,owner);
                st.sides[side][1]=store(f.endpoint,key++,local.carrier,kc,ls[e].sides[side].left_inverse,TemporalTensorRole::Accession,owner);
            }r.stalks.push_back(std::move(st));
        }d.requests.push_back(std::move(r));d.commons.push_back(std::move(common));
    }
    std::vector<Declaration> defs;const auto w=witness();defs.push_back(w.target.declaration);
    for(const auto& f:d.locals)for(const auto& b:f.contract.downward.stalks)defs.push_back(b.declaration);
    TemporalDeclarationState ds;ds.namespace_key=29000;ds.profile=BooleanLayoutProfile::Lsb0OctetsV2;ds.registry=DeclarationRegistry(std::move(defs));d.certificates.declarations=std::move(ds);
    for(std::size_t c=0;c<d.locals[0].contract.downward.stalks.size();++c){
        NativeStalkColimitRecords r;for(std::size_t p=0;p<4;++p)r.origins.push_back({p,d.locals[p].contract.downward.stalks[c]});r.target=w.target;
        const auto all=explicit_carrier(key++,8,18000+c),target=explicit_carrier(key++,5,w.target.coordinates),rel=explicit_carrier(key++,3,19000+c);
        r.quotient=store(d.certificates,key++,all,target,w.quotient,TemporalTensorRole::Accession);
        r.section=store(d.certificates,key++,target,all,w.section,TemporalTensorRole::Reconstruction);
        r.relation_lift=store(d.certificates,key++,all,rel,w.relation_lift,TemporalTensorRole::SourceRealization);
        r.reconstruct_observations=store(d.certificates,key++,all,target,w.reconstruct_observations,TemporalTensorRole::Reconstruction);
        for(std::size_t p=0;p<4;++p){const auto local=explicit_carrier(key++,2,r.origins[p].local.carrier.coordinate_system);r.readouts.push_back(store(d.certificates,key++,target,local,w.readouts[p],TemporalTensorRole::Observation));}d.records.push_back(std::move(r));
    }
    for(const auto& f:d.locals){validate_temporal_endpoint(f.endpoint);}
    validate_temporal_endpoint(d.certificates);return d;
}
inline NativeActionAvailability available(){return {{500,7},{501,7},{502,7},{503,7},{600,7},{601,7},{602,7},{700,7},{999,7}};}
inline PreparedNativeColimitGluing prepare_unsplit(const UnsplitDataset& d,numeric::ExactActionLimits limits={}){
    auto req=d.requests;for(std::size_t e=0;e<req.size();++e){NativeAccessionDirectory dir(d.commons[e].endpoint,600+e);req[e].common={"K"+std::to_string(e),dir.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),d.commons[e].contract};}
    NativeAccessionDirectory dir(d.certificates,700);return PreparedNativeColimitGluing::prepare(publications(d.locals),std::move(req),dir.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),d.records,limits);
}
inline TemporalEndpoint coordinates_unsplit(const ColimitLocalFieldView& v){core::RelationalComplexBuilder b(signature());TemporalEndpoint e(b.freeze(),7);for(std::size_t k=0;k<v.grade_count();++k)e.carriers.push_back(explicit_carrier(800+k,v.dimension({static_cast<std::uint16_t>(k)}),20000+k));return e;}
inline PreparedNativeColimitGluing bind_unsplit(const PreparedNativeColimitGluing& p,const TemporalEndpoint& e){
    NativeAccessionDirectory d(e,999);const auto anchor=d.pin(NativeAccessionRequest::sparse_chain(g0,{},{}));std::vector<NativeColimitCarrierBinding> cs;
    for(std::size_t k=0;k<p.view().grade_count();++k){cs.push_back({NativeCarrier::bind(anchor,800+k),p.view().layout({static_cast<std::uint16_t>(k)})});}
    return p.bind_carriers(std::move(cs));
}
inline PreparedNativeColimitGluing plan_unsplit(const UnsplitDataset& d){const auto p=prepare_unsplit(d);return bind_unsplit(p,coordinates_unsplit(p.view()));}
inline NativeActionMomentResult result_unsplit(const PreparedNativeColimitGluing& p,const UnsplitDataset& d){
    const auto x=p.glue_field(g1,inputs(d.locals,1),available());const auto all=p.boundary(g1);std::vector<NativeMomentChannel> cs;
    for(std::size_t i=0;i<4;++i)cs.push_back({"patch-"+std::to_string(i),p.boundary(g1,i)});
    const std::array<NativeActionInteractionRequest,6> pairs{{{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}}};
    return PreparedNativeActionMoments(std::move(cs),NativeBilinearForm::named_unit(all.output()),all).execute(x,available(),pairs);
}
} // namespace colimit_example
