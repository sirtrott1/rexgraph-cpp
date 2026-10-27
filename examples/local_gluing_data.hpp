#pragma once
#include "rfc/inference/native_local_gluing.hpp"
#include "rfc/core/builder.hpp"
#include <array>

namespace local_gluing_example {
using namespace rfc;
using namespace rfc::storage;
using namespace rfc::inference;
using namespace rfc::composite;
using Q=numeric::Rational;using M=numeric::ExactMatrix;using S=numeric::SparseRationalVector;using Guard=numeric::ExactActionGuard;
constexpr core::Grade g0{0},g1{1};
struct Fixture {TemporalEndpoint endpoint;DeclaredLocalFieldContract contract;std::array<std::vector<DeclaredFieldInput>,3> inputs;};
inline core::RelationSignature signature(){core::RelationSignature s;s.aspects().add("gate",AspectRole::RelationRole);s.add_type("pair",core::BoundaryPolicy::DeclaredExact,0,8);return s;}
inline core::RelationalComplex edge(std::uint64_t key,std::uint64_t a,std::uint64_t b,bool extras=false){
    const auto sig=signature();core::RelationalComplexBuilder c(sig);
    std::vector<core::BoundaryOccurrenceInput> os{{a,true,true,1,{},core::OccurrenceKey{key*10+1}},{b,true,false,1,{},core::OccurrenceKey{key*10+2}}};
    if(extras){os.push_back({b,true,false,0,{{sig.aspects().id("gate"),true}},core::OccurrenceKey{key*10+3}});os.push_back({b,false,false,7,{},core::OccurrenceKey{key*10+4}});os.push_back({a,true,false,2,{},core::OccurrenceKey{key*10+5}});os.push_back({a,true,true,2,{},core::OccurrenceKey{key*10+6}});}
    c.add_grade_one(core::RelationKey{key},0,os);return c.freeze();
}
inline Fixture make_source(core::RelationalComplex source,std::size_t extent=1,bool heterogeneous=false){
    Fixture f{TemporalEndpoint(source,7),{},{}};
    TemporalDeclarationState dec;dec.namespace_key=9800;dec.profile=BooleanLayoutProfile::Lsb0OctetsV2;
    std::vector<Declaration> definitions;StableSourceIndex index(f.endpoint.source);
    for(std::size_t k=0;k<source.grade_count();++k){
        const core::Grade g{static_cast<std::uint16_t>(k)};const auto& arena=f.endpoint.source.grade(g);
        for(std::size_t i=0;i<arena.cell_count();++i){
            const auto cell=stable_cell_key(f.endpoint.source,{g,static_cast<std::uint32_t>(i)});
            const auto n=heterogeneous?(k?2u:(cell.key==10?1u:cell.key==20?2u:3u)):extent;
            const auto decl=Declaration::rational_tensor({9800,n,1},{{1,n,{}}});definitions.push_back(decl);
            const auto ck=100+k*10+i;TemporalCarrier c{ck,TemporalCarrierKind::CellStalk,g,cell,n,numeric::IndexDomain::linear(n)};
            f.endpoint.carriers.push_back(c);f.contract.downward.stalks.push_back({c,decl});f.contract.upward.stalks.push_back({c,decl});
            TemporalTensor t;t.key=ck;t.role=TemporalTensorRole::Field;t.output=ck;t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell=cell;
            const Q value=k?Q{2}:cell.key==10?Q{1}:Q{0};
            t.tensor={c.domain,numeric::StructuralSupport::full(n),S::basis(n,0,value)};f.endpoint.tensors.push_back(t);
            TemporalDeclaredValue v;v.key=ck+1000;v.type=decl.id;v.cell=cell;v.value=DeclaredValue::tensor(t.tensor);dec.values.push_back(v);f.inputs[k].push_back({ck,v.key});
        }
    }
    f.contract.downward.allow_constant_identity=!heterogeneous;f.contract.upward.allow_constant_identity=!heterogeneous;
    if(heterogeneous){
        const auto& ar=f.endpoint.source.grade(g1);
        for(std::size_t s=0;s<ar.incidence_count();++s){
            const auto lower=100+ar.lower_index[s],upper=110+ar.upper_index[s];
            const auto n=f.endpoint.carriers[ar.lower_index[s]].domain.logical_size();M map(n,2);
            for(std::size_t j=0;j<std::min(n,std::size_t{2});++j)map.set(j,j,1);
            for(bool upward:{false,true}){
                TemporalTensor t;t.key=200+2*s+unsigned(upward);t.role=upward?TemporalTensorRole::SheafRestriction:TemporalTensorRole::CoefficientIndependentMap;
                t.input=upward?lower:upper;t.output=upward?upper:lower;t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence={g1,ar.occurrence_identities[s]};t.tensor=temporal_matrix_tensor(upward?map.transpose():map);
                f.endpoint.tensors.push_back(t);(upward?f.contract.upward:f.contract.downward).maps.push_back({t.key,t.role});
            }
        }
    }
    dec.registry=DeclarationRegistry(std::move(definitions));f.endpoint.declarations=std::move(dec);validate_temporal_endpoint(f.endpoint);return f;
}
inline Fixture make(std::uint64_t key,std::uint64_t a,std::uint64_t b,std::size_t extent=1,bool heterogeneous=false,bool extras=false){return make_source(edge(key,a,b,extras),extent,heterogeneous);}
inline algebra::RetainedSourceMap identity_map(const core::RelationalComplex& c){
    algebra::RetainedSourceMap m;
    for(std::size_t k=0;k<c.grade_count();++k){const auto& a=c.grade({static_cast<std::uint16_t>(k)});std::vector<std::uint32_t> cs(a.cell_count());std::iota(cs.begin(),cs.end(),0);std::vector<std::size_t> os(a.incidence_count());std::iota(os.begin(),os.end(),0);m.cells.push_back(std::move(cs));m.occurrences.push_back(std::move(os));}return m;
}
inline LocalFieldInterface vertex_interface(const std::vector<Fixture>& fs,std::size_t a,std::size_t b,std::uint64_t key){
    core::RelationalComplexBuilder builder(signature());builder.declare_witness_identity(900);core::OwnedRelationalSource common(builder.freeze());
    const auto map=[&](std::size_t p){StableSourceIndex ix(fs[p].endpoint.source);const auto cell=ix.cell({g0,key});if(!cell)throw std::invalid_argument("fixture shared vertex absent");return algebra::RetainedSourceMap{{{cell->index}},{{}}};};
    return {common,a,b,map(a),map(b)};
}
inline std::vector<NativeLocalFieldPatch> publications(const std::vector<Fixture>& fs){
    std::vector<NativeLocalFieldPatch> out;
    for(std::size_t i=0;i<fs.size();++i){NativeAccessionDirectory d(fs[i].endpoint,500+i);out.push_back({std::string(1,char('A'+i)),d.pin(NativeAccessionRequest::sparse_chain(g0,{},{})),fs[i].contract});}
    return out;
}
inline TemporalEndpoint coordinate_endpoint(const GluedLocalFieldView& view){
    core::RelationalComplexBuilder b(signature());TemporalEndpoint e(b.freeze(),7);
    for(std::size_t k=0;k<view.grade_count();++k)e.carriers.push_back({800+k,TemporalCarrierKind::Explicit,{},{},8800+k,numeric::IndexDomain::linear(view.dimension({static_cast<std::uint16_t>(k)}))});
    return e;
}
inline NativeActionAvailability availability(std::size_t n){NativeActionAvailability out{{999,7}};for(std::size_t i=0;i<n;++i)out.push_back({500+i,7});return out;}
inline PreparedNativeLocalGluing bind(PreparedNativeLocalGluing plan,const TemporalEndpoint& coordinates){
    NativeAccessionDirectory d(coordinates,999);const auto anchor=d.pin(NativeAccessionRequest::sparse_chain(g0,{},{}));
    std::vector<NativeGluedCarrierBinding> bindings;
    for(std::size_t k=0;k<plan.view().grade_count();++k)bindings.push_back({NativeCarrier::bind(anchor,800+k),plan.view().layout({static_cast<std::uint16_t>(k)})});
    return plan.bind_carriers(std::move(bindings));
}
inline std::vector<std::vector<DeclaredFieldInput>> inputs(const std::vector<Fixture>& fs,std::size_t g){std::vector<std::vector<DeclaredFieldInput>> out;for(const auto& f:fs)out.push_back(f.inputs[g]);return out;}
inline PreparedNativeLocalGluing make_plan(const std::vector<Fixture>& fs,std::vector<LocalFieldInterface> interfaces,numeric::ExactActionLimits limits={}){
    auto plan=PreparedNativeLocalGluing::prepare(publications(fs),std::move(interfaces),limits);return bind(plan,coordinate_endpoint(plan.view()));
}
inline std::array<Q,3> moments(const PreparedNativeLocalGluing& plan,const std::vector<Fixture>& fs){
    const auto available=availability(fs.size());auto x=plan.glue_field(g0,inputs(fs,0),available);
    const auto d=plan.boundary(g1),read=d.algebraic_transpose(),lap=read.then(d);
    const auto measure=[&](const NativeFactoredAction& op){return PreparedNativeActionMoments({{"reading",op}},NativeBilinearForm::named_unit(op.output())).execute(x,available).total;};
    return {measure(read),measure(lap),measure(lap.then(read))};
}
inline NativeActionMomentResult third(const PreparedNativeLocalGluing& plan,const std::vector<Fixture>& fs){
    const auto d=plan.boundary(g1),read=d.algebraic_transpose(),lap=read.then(d);std::vector<NativeMomentChannel> channels;
    for(std::size_t p=0;p<fs.size();++p)channels.push_back({std::string(1,char('A'+p)),lap.then(plan.boundary(g1,p).algebraic_transpose())});
    return PreparedNativeActionMoments(std::move(channels),NativeBilinearForm::named_unit(d.input()),lap.then(read)).execute(plan.glue_field(g0,inputs(fs,0),availability(fs.size())),availability(fs.size()));
}
} // namespace local_gluing_example
