#pragma once
#include "injective_gluing_data.hpp"
#include "rfc/inference/native_finite_cover_gluing.hpp"
namespace finite_cover_example {
using namespace injective_gluing_example;
struct InterfaceData {
    Fixture common;std::vector<LocalStalkPartitionRecords> partitions;
    std::size_t left{},right{};std::array<algebra::RetainedSourceMap,2> maps;
    std::array<std::vector<std::size_t>,2> block_images;
};
struct CoverDataset {std::vector<Fixture> locals;std::vector<LocalStalkPartitionRecords> partitions;std::vector<InterfaceData> interfaces;};
inline StalkCoordinateBlock block(std::uint64_t key,std::size_t n=1){return {"block-"+std::to_string(key),Declaration::rational_tensor({15100,key,1},{{key,n,{}}})};}
inline void declare_blocks(Fixture& f,const std::vector<StalkCoordinateBlock>& bs){auto defs=f.endpoint.declarations->registry.definitions();for(const auto& b:bs)defs.push_back(b.declaration);f.endpoint.declarations->registry=DeclarationRegistry(std::move(defs));}
inline std::vector<LocalStalkPartitionRecords> records(const ChartFixture& c,const std::vector<StalkCoordinateBlock>& bs){std::vector<LocalStalkPartitionRecords> out;for(const auto& ch:c.charts)out.push_back({ch,bs});return out;}
inline CoverDataset make_cover(bool extras=false){
    CoverDataset d;
    const std::array<M,3> maps{M::from_rows({{1,0,1},{0,1,0},{0,0,1}}),M::from_rows({{1,0,0},{0,1,2},{0,0,1}}),M::from_rows({{1,1,1},{0,1,0},{0,0,1}})};
    const std::array<S,3> values{S(3,{{0,2},{1,3},{2,5}}),S(3,{{0,2},{1,3},{2,7}}),S(3,{{0,2},{1,11},{2,13}})};
    for(std::size_t p=0;p<3;++p){
        auto f=transport(make(100,10,20,3,false,extras),maps[p]);set_field(f,g1,values[p]);
        auto ch=changed(f,p,16000+p);const auto bs=std::vector{block(1),block(2),block(3+p)};
        declare_blocks(ch.field,bs);const auto rr=records(ch,bs);d.partitions.insert(d.partitions.end(),rr.begin(),rr.end());d.locals.push_back(ch.field);
    }
    for(std::size_t i=0;i<3;++i){
        const auto n=i==0?2u:1u;auto ch=changed(make(100,10,20,n,false,extras),0,17000+i);
        std::vector<StalkCoordinateBlock> bs{block(1)};if(n==2)bs.push_back(block(2));declare_blocks(ch.field,bs);
        const auto m=identity_map(ch.field.endpoint.source);const std::vector<std::size_t> image=n==2?std::vector<std::size_t>{0,1}:std::vector<std::size_t>{0};
        const std::array<std::size_t,3> left{0,1,0},right{1,2,2};
        d.interfaces.push_back({ch.field,records(ch,bs),left[i],right[i],{m,m},{image,image}});
    }
    return d;
}
inline NativeActionAvailability available(const CoverDataset& d){auto a=availability(d.locals.size());for(std::size_t i=0;i<d.interfaces.size();++i)a.push_back({600+i,7});return a;}
inline PreparedNativeFiniteCoverGluing prepare_cover(const CoverDataset& d,numeric::ExactActionLimits lim={}){
    std::vector<NativeCoverInterfaceRecords> rs;
    for(std::size_t i=0;i<d.interfaces.size();++i){const auto& e=d.interfaces[i];NativeAccessionDirectory dir(e.common.endpoint,600+i);
        NativeCoverInterfaceRecords r{{"interface-"+std::to_string(i),dir.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),e.common.contract},e.left,e.right,e.maps,{}};
        for(const auto& p:e.partitions)r.stalks.push_back({p,e.block_images});
        rs.push_back(std::move(r));
    }
    return PreparedNativeFiniteCoverGluing::prepare(publications(d.locals),d.partitions,rs,lim);
}
inline TemporalEndpoint coordinates_cover(const FiniteCoverLocalFieldView& v){
    core::RelationalComplexBuilder b(signature());TemporalEndpoint e(b.freeze(),7);
    for(std::size_t k=0;k<v.grade_count();++k)e.carriers.push_back({800+k,TemporalCarrierKind::Explicit,{},{},15800+k,numeric::IndexDomain::linear(v.dimension({static_cast<std::uint16_t>(k)}))});
    return e;
}
inline PreparedNativeFiniteCoverGluing bind_cover(const PreparedNativeFiniteCoverGluing& p,const TemporalEndpoint& e){
    NativeAccessionDirectory dir(e,999);const auto anchor=dir.pin(NativeAccessionRequest::sparse_chain(g0,{},{}));std::vector<NativeCoverCarrierBinding> cs;
    for(std::size_t k=0;k<p.view().grade_count();++k)cs.push_back({NativeCarrier::bind(anchor,800+k),p.view().layout({static_cast<std::uint16_t>(k)})});
    return p.bind_carriers(std::move(cs));
}
inline PreparedNativeFiniteCoverGluing plan_cover(const CoverDataset& d){const auto p=prepare_cover(d);return bind_cover(p,coordinates_cover(p.view()));}
inline NativeActionMomentResult cover_result(const PreparedNativeFiniteCoverGluing& p,const CoverDataset& d){
    const auto x=p.glue_field(g1,inputs(d.locals,1),available(d));const auto all=p.boundary(g1);std::vector<NativeMomentChannel> cs;
    for(std::size_t i=0;i<d.locals.size();++i)cs.push_back({"patch-"+std::to_string(i),p.boundary(g1,i)});
    return PreparedNativeActionMoments(std::move(cs),NativeBilinearForm::named_unit(all.output()),all).execute(x,available(d));
}
inline CoverDataset permute(CoverDataset d,const std::array<std::size_t,3>& order){
    const auto old=d.locals;std::array<std::size_t,3> inverse{};for(std::size_t i=0;i<3;++i){d.locals[i]=old[order[i]];inverse[order[i]]=i;}
    for(auto& p:d.partitions)p.chart.patch=inverse[p.chart.patch];
    for(auto& e:d.interfaces){e.left=inverse[e.left];e.right=inverse[e.right];}return d;
}
} // namespace finite_cover_example
