#pragma once
#include "rfc/storage/native_atlas_codec.hpp"

namespace native_atlas_fixture {
using namespace rfc;using namespace rfc::storage;using Q=numeric::Rational;
inline StableCellKey cell(std::uint16_t g,std::uint64_t k){return {core::Grade{g},k};}
inline StableOccurrenceKey occ(std::uint16_t g,std::uint64_t k){return {core::Grade{g},core::OccurrenceKey{k}};}
inline core::RelationSignature signature(bool newer=false){
    core::RelationSignature s;if(newer)s.aspects().add("introduced");s.aspects().add("role");s.aspects().add("spare");
    s.add_type(newer?"renamed-edge":"edge",core::BoundaryPolicy::DeclaredExact,3,3);return s;
}
inline SourceDeclarationMigration migration(const core::RelationSignature& a,const core::RelationSignature& b){
    SourceDeclarationMigration m;m.base_signature=a;m.target_signature=b;
    for(std::size_t i=0;i<a.aspects().size();++i){const auto& aspect=a.aspects().at(static_cast<composite::AspectId>(i));m.aspects.push_back({aspect.name,aspect.name});}
    m.types={{a.type(0).name,b.type(0).name}};
    if(b.aspects().size()>a.aspects().size())m.introduced={{"introduced",false,{}}};
    return m;
}
inline core::RelationalComplex path_source(std::uint64_t base,std::uint64_t edge,std::size_t count,
                                          bool newer=false,std::uint16_t scale=0){
    auto sig=signature(newer);core::RelationalComplexBuilder b(sig,core::SemanticScale{scale});b.declare_grade(core::Grade{2});
    for(std::size_t i=0;i<=count;++i)b.declare_witness_identity(base+i);
    for(std::size_t i=0;i<count;++i){const auto key=edge+i;std::vector<core::BoundaryOccurrenceInput> legs;
        for(std::size_t j=0;j<3;++j){std::vector<core::AspectValue> bits{{sig.aspects().id("role"),j==1},{sig.aspects().id("spare"),j==2}};
            if(newer)bits.push_back({sig.aspects().id("introduced"),false});
            legs.push_back({base+i+(j==1?1u:0u),j!=2,j==0,j==2?Q{0}:Q{1},bits,core::OccurrenceKey{key*10+j}});}
        b.add_grade_one(core::RelationKey{key},0,legs);}
    return b.freeze();
}
inline TemporalTensor make_field(std::uint64_t key,std::uint64_t carrier,const std::vector<Q>& values){
    TemporalTensor t;t.key=key;t.role=TemporalTensorRole::Field;t.output=carrier;
    std::vector<numeric::SparseRationalEntry> entries;for(std::size_t i=0;i<values.size();++i)if(values[i]!=0)entries.push_back({i,values[i]});
    t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(values.size()),numeric::StructuralSupport::full(values.size()),numeric::SparseRationalVector(values.size(),entries));return t;
}
inline composite::DeclarationId common_tensor(){return {900,1,1};}
inline composite::DeclarationId common_finite(){return {900,2,1};}
inline TemporalEndpoint endpoint(std::uint64_t sid){
    const bool newer=sid==3;const auto base=sid==3?20u:10u,edge=sid==3?200u:100u;const std::size_t n=sid==1?1:2;
    TemporalEndpoint e(path_source(base,edge,n,newer));const std::size_t dim=sid==3?3:2;
    e.carriers={{501,TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(n+1)},
        {601,TemporalCarrierKind::Explicit,core::Grade{0},{},sid+100,numeric::IndexDomain::linear(dim)}};
    std::vector<Q> vals(n+1);for(std::size_t i=0;i<vals.size();++i)vals[i]=Q{static_cast<unsigned long>(i+1)};
    if(sid==3)vals.back()=4;
    e.tensors={make_field(5001,501,vals),make_field(6001,601,sid==3?std::vector<Q>{3,7,9}:std::vector<Q>{3,4})};
    TemporalTensor op;op.key=6020;op.role=TemporalTensorRole::Observation;op.input=601;op.output=601;
    auto m=numeric::ExactMatrix::identity(dim);m.set(0,0,Q{2});op.tensor=temporal_matrix_tensor(m);e.tensors.push_back(op);
    TemporalDeclarationState ds;ds.namespace_key=900;
    const composite::DeclarationId own{900+sid,3,1};
    ds.registry=composite::DeclarationRegistry({composite::Declaration::rational_tensor(common_tensor(),{{88,1,{}}}),
        composite::Declaration::finite(common_finite(),{1,2}),composite::Declaration::finite(own,{sid*10,sid*10+1})});
    ds.values={{8001,own,0,0,{},{},composite::DeclaredValue::symbol(sid*10)}};e.declarations=ds;
    validate_temporal_endpoint(e);return e;
}
inline NativeSourceEmbedding embedding(std::uint64_t key,std::uint64_t target){
    NativeSourceEmbedding e;e.key=key;e.from=1;e.to=target;
    const auto shift=target==3?10u:0u,edge=target==3?200u:100u;
    e.cells={{cell(0,10),cell(0,10+shift)},{cell(0,11),cell(0,11+shift)},{cell(1,100),cell(1,edge)}};
    for(std::uint64_t j=0;j<3;++j)e.occurrences.push_back({occ(1,1000+j),occ(1,edge*10+j)});
    if(target==3)e.declarations=migration(signature(),signature(true));
    return e;
}
inline NativeAtlasManifest manifest(){
    NativeAtlasManifest m;m.key=700;m.revision=1;m.embeddings={embedding(1,2),embedding(2,3)};
    NativePushoutDeclaration p;p.key=1;p.left_embedding=1;p.right_embedding=2;p.left_common=migration(signature(),signature(true));m.pushouts={p};
    m.products={{1,2,3,100,200},{2,3,2,200,100}};
    NativeLinearInterface f;f.key=1;f.field_identity=444;f.common_source=1;f.common_type=common_tensor();
    numeric::ExactMatrix a(1,2),b(1,3),op(1,1);a.set(0,0,Q{1});b.set(0,0,Q{1});op.set(0,0,Q{2});
    f.legs={{2,6001,a,6020},{3,6001,b,6020}};f.common_operation=op;m.linear_interfaces={f};
    m.finite_interfaces={{1,445,1,common_finite(),{{2,8001,{{20,1},{21,2}}},{3,8001,{{30,1},{31,2}}}}}};
    return m;
}
inline std::vector<NativeAtlasSource> sources(){return {{1,11,endpoint(1)},{2,22,endpoint(2)},{3,33,endpoint(3)}};}
inline NativeAtlas atlas(){return NativeAtlas(sources(),manifest());}
inline NativeChainColumn col(StableCellKey s,std::vector<NativeChainTerm> terms){return {s,std::move(terms)};}
inline std::vector<NativeAtlasSource> hierarchy_sources(){
    return {{4,44,TemporalEndpoint(path_source(1,400,1,false,2))},
            {5,55,TemporalEndpoint(path_source(10,500,2,false,1))},
            {6,66,TemporalEndpoint(path_source(20,600,2,false,0))}};
}
inline NativeAtlasManifest hierarchy_manifest(){
    NativeAtlasManifest m;m.key=701;m.revision=1;
    NativeScaleMap a;a.key=1;a.from=4;a.to=5;
    a.realization={col(cell(0,1),{{cell(0,10),Q{1}}}),col(cell(0,2),{{cell(0,12),Q{1}}}),
        col(cell(1,400),{{cell(1,500),Q{1}},{cell(1,501),Q{1}}})};
    a.retraction=std::vector<NativeChainColumn>{col(cell(0,10),{{cell(0,1),Q{1}}}),col(cell(0,11),{{cell(0,1),Q{1}}}),
        col(cell(0,12),{{cell(0,2),Q{1}}}),col(cell(1,500),{}),col(cell(1,501),{{cell(1,400),Q{1}}})};
    NativeScaleMap b;b.key=2;b.from=5;b.to=6;
    for(std::uint64_t i=0;i<3;++i)b.realization.push_back(col(cell(0,10+i),{{cell(0,20+i),Q{1}}}));
    for(std::uint64_t i=0;i<2;++i)b.realization.push_back(col(cell(1,500+i),{{cell(1,600+i),Q{1}}}));
    NativeScaleMap c;c.key=3;c.from=4;c.to=6;c.realization=a.realization;
    for(auto& column:c.realization)for(auto& t:column.image)t.target.key+=t.target.grade.value?100:10;
    m.scale_maps={a,b,c};m.scale_paths={{1,{1,2},{3}}};return m;
}
}
