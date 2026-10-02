#pragma once
#include "rfc_preview/database.hpp"
#include "rfc/core/builder.hpp"
#include <numeric>

namespace rfc_preview::sample {
using namespace rfc;
using namespace rfc::storage;
using namespace rfc::composite;
using Q = numeric::Rational;
inline constexpr std::uint64_t namespace_key=0x524643444231ULL;
inline DeclarationId id(std::uint64_t k){return {namespace_key,k,1};}
// Synthetic, dimensioned *model* input, not an electronic measurement.
// C=1 F, R=1 ohm, dt=1/2 s. Backward Euler solves
// C(v'-v)/dt + G v' = u/R, G=1/R. Rational exactness refers to this
// discretization, not the exponential continuous-time trajectory.
inline Q voltage(std::uint64_t sequence) {
    require(sequence<=64,"sample model supports at most 64 steps");Q v=0;
    for(std::uint64_t t=1;t<=sequence;++t){Q u=t<=5?Q{1}:Q{-1};v=(Q{2}*v+u)/3;}
    return v;
}
struct LogicObservation {bool valid{},low{},high{},uncertain{},negative{},falling{};};
inline LogicObservation observe(Q v,Q previous,Q error,bool valid=true) {
    require(error>=0,"negative voltage uncertainty");
    bool low=valid && v+error<=Q{3,10};bool high=valid && v-error>=Q{7,10};
    return {valid,low,high,valid&&!low&&!high,v<0,v<previous};
}
inline std::vector<std::string> aspect_names(){
    std::vector<std::string> out{"exists","orientation","parent","child","producer","consumer",
                                 "measurement-valid","level-low","level-high","uncertain","negative","falling"};
    while(out.size()<131) { out.push_back("tag-"+std::to_string(out.size())); }
    return out;
}
inline core::RelationSignature signature(){core::RelationSignature s;auto names=aspect_names();
    for(std::size_t i=2;i<names.size();++i)s.aspects().add(names[i],i<6?AspectRole::RelationRole:AspectRole::Custom);
    s.add_type("directory",core::BoundaryPolicy::DeclaredExact,1,1000);
    s.add_type("collective",core::BoundaryPolicy::DeclaredExact,0,1000);
    s.add_type("circuit-wire",core::BoundaryPolicy::DeclaredExact,2,2);return s;
}
inline numeric::SparseRationalTensor tensor(numeric::IndexDomain domain,std::vector<std::size_t> support,
                                           std::vector<numeric::SparseRationalEntry> values){
    auto n=domain.logical_size();return {std::move(domain),numeric::StructuralSupport::sparse(n,std::move(support)),numeric::SparseRationalVector(n,std::move(values))};
}
inline TemporalEndpoint endpoint(std::uint64_t t) {
    auto sig=signature();core::RelationalComplexBuilder b(sig);b.declare_grade(core::Grade{2});
    for(std::uint64_t k:std::array<std::uint64_t,5>{1,2,3,10,20})b.declare_witness_identity(k);
    const Q v=voltage(t),old=voltage(t?t-1:0);const auto obs=observe(v,old,Q{1,100},t!=7);
    std::uint64_t oid=10000;
    auto leg=[&](std::uint64_t target,const char* role,bool active=true,Q magnitude=0,bool orientation=false){
        std::vector<core::AspectValue> bits;
        for(std::size_t i=2;i<sig.aspects().size();++i){const auto& name=sig.aspects().at(static_cast<AspectId>(i)).name;bool set=false;
            if(name==role)set=true;
            if(target==10){if(name=="measurement-valid")set=obs.valid;if(name=="level-low")set=obs.low;if(name=="level-high")set=obs.high;
                if(name=="uncertain") { set=obs.uncertain; }
                if(name=="negative") { set=obs.negative; }
                if(name=="falling") { set=obs.falling; }
            }
            if(name.rfind("tag-",0)==0)set=((oid+i)%7==0);
            bits.push_back({static_cast<AspectId>(i),set});}
        return core::BoundaryOccurrenceInput{target,active,orientation,magnitude,std::move(bits),core::OccurrenceKey{oid++}};};
    b.add_grade_one(core::RelationKey{100},0,{leg(1,"parent"),leg(2,"child"),leg(3,"child")});
    std::vector<core::BoundaryOccurrenceInput> children{leg(2,"parent")};
    for(std::uint64_t i=0;i<130;++i)children.push_back(leg(10+i,"child",i!=129 || t>=4));
    b.add_grade_one(core::RelationKey{101},0,children);
    // Shared target 10 and a repeated inactive occurrence remain separate legs.
    b.add_grade_one(core::RelationKey{102},0,{leg(3,"parent"),leg(10,"child"),leg(10,"child",false),leg(2,"child")});
    b.add_grade_one(core::RelationKey{150},1,{leg(10,"producer"),leg(11,"producer"),leg(10,"consumer"),leg(12,"consumer"),leg(13,"consumer",false)});
    b.add_grade_one(core::RelationKey{151},1,{}); // empty retained assertion
    b.add_grade_one(core::RelationKey{200},2,{leg(20,"",true,1,true),leg(10,"",true,1,false)});
    if(t>=3 && t<=6)b.add_grade_one(core::RelationKey{160},1,{leg(10,"producer"),leg(12,"consumer")}); // zero-field birth/removal
    TemporalEndpoint e(b.freeze(),t);
    const auto n=e.source.grade(core::Grade{0}).cell_count();
    e.carriers={{1,TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(n)}};
    std::vector<numeric::SparseRationalEntry> values;
    const auto& g0=e.source.grade(core::Grade{0});for(std::size_t i=0;i<n;++i)if(g0.identities[i]==10 && v!=0)values.push_back({i,v});
    TemporalTensor field;field.key=10;field.role=TemporalTensorRole::Field;field.output=1;
    field.interpretation="synthetic RC cochain; volt on signal=10, ground=20; declared zero extension elsewhere; BE dt=1/2 s; not measured hardware";
    field.tensor={numeric::IndexDomain::linear(n),numeric::StructuralSupport::full(n),numeric::SparseRationalVector(n,values)};
    e.tensors.push_back(field);
    const auto relations=e.source.grade(core::Grade{1}).cell_count();
    e.carriers.push_back({2,TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(relations)});
    TemporalTensor amplitude;amplitude.key=15;amplitude.role=TemporalTensorRole::Field;amplitude.output=2;
    amplitude.interpretation="application assertion amplitude; deliberately zero, independent of retained existence";
    amplitude.tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(relations),numeric::StructuralSupport::full(relations));e.tensors.push_back(amplitude);
    TemporalTensor uncertainty;uncertainty.key=12;uncertainty.role=TemporalTensorRole::OpaqueExact;
    uncertainty.owner.kind=TemporalOwnerKind::Cell;uncertainty.owner.cell={core::Grade{0},10};
    uncertainty.interpretation="synthetic interval radius in volt; not ADC error certification";
    uncertainty.tensor=tensor(numeric::IndexDomain::linear(1),{0},{{0,Q{1,100}}});e.tensors.push_back(uncertainty);
    TemporalTensor data;data.key=20;data.role=TemporalTensorRole::OpaqueExact;
    data.owner.kind=TemporalOwnerKind::Cell;data.owner.cell={core::Grade{0},1};data.interpretation="application tensor; support is not numerical nonzeros";
    data.tensor=tensor(numeric::IndexDomain({2,3}),t>=2?std::vector<std::size_t>{0,1,2,5}:std::vector<std::size_t>{0,2,5},
                      {{2,Q{7,13}},{5,Q(numeric::Integer("340282366920938463463374607431768211457"),numeric::Integer(17))}});
    e.tensors.push_back(data);
    TemporalDeclarationState d;d.namespace_key=namespace_key;
    std::vector<DeclarationFactor> factors;std::vector<core::SourceAspectFactorBinding> bindings;auto names=aspect_names();
    for(std::size_t i=0;i<names.size();++i){factors.push_back({i+1,id(1),0});bindings.push_back({i+1,names[i]});}
    d.registry=DeclarationRegistry({Declaration::boolean(id(1)),Declaration::finite(id(2),{10,20,30}),
        Declaration::integer(id(3)),Declaration::rational(id(4)),Declaration::optional(id(5),id(4)),
        Declaration::repeated(id(6),id(2),0,100),Declaration::alternative(id(7),{{101,id(3),0},{102,id(4),0}}),
        Declaration::rational_tensor(id(8),{{10,2,{1,2}},{20,3,{10,20,30}}}),
        Declaration::product(id(9),{{1,id(1),0},{2,id(2),0},{3,id(3),0},{4,id(4),0},{5,id(5),0},{6,id(6),0},{7,id(7),0},{8,id(8),0}}),
        Declaration::product(id(10),factors),
        Declaration::product(id(13),{{1,id(4),0},{2,id(4),0},{3,id(4),0},{4,id(4),0},{5,id(4),0},{6,id(4),0},{7,id(4),0}})});
    TemporalDeclaredValue payload;payload.key=100;payload.type=id(9);payload.cell=StableCellKey{core::Grade{1},150};
    payload.value=DeclaredValue::product({DeclaredValue::boolean(false),DeclaredValue::symbol(20),DeclaredValue::integer(numeric::Integer("18446744073709551617")),
        DeclaredValue::rational(Q{0}),t%2?DeclaredValue::present(DeclaredValue::rational(Q{0})):DeclaredValue::absent(),
        DeclaredValue::repeated({DeclaredValue::symbol(10),DeclaredValue::symbol(10),DeclaredValue::symbol(30)}),
        DeclaredValue::alternative(102,DeclaredValue::rational(Q{1,3})),DeclaredValue::tensor(data.tensor)});
    d.values={payload};
    d.branches={{1,"collective",{id(11),id(9),core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
        {{100,"producer",2,2},{200,"consumer",3,3}}}}};d.values[0].branch=1;
    d.branches.push_back({2,"directory",{id(12),id(1),core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
        {{300,"parent",1,1},{400,"child",1,130}}}});
    for(std::uint64_t key=100;key<=102;++key){TemporalDeclaredValue attributes;attributes.key=key+101;attributes.type=id(1);
        attributes.cell=StableCellKey{core::Grade{1},key};attributes.branch=2;attributes.value=DeclaredValue::boolean(key==102);d.values.push_back(attributes);}
    // Profile 13 factors: C[F], R[ohm], dt[s], drive[V], VIL[V], VIH[V], uncertainty[V].
    // Their exact values are retained, not reconstructed from sequence numbers at read time.
    TemporalDeclaredValue parameters;parameters.key=300;parameters.type=id(13);parameters.cell=StableCellKey{core::Grade{0},10};
    parameters.value=DeclaredValue::product({DeclaredValue::rational(1),DeclaredValue::rational(1),DeclaredValue::rational(Q{1,2}),
        DeclaredValue::rational(t<=5?Q{1}:Q{-1}),DeclaredValue::rational(Q{3,10}),DeclaredValue::rational(Q{7,10}),DeclaredValue::rational(Q{1,100})});
    d.values.push_back(parameters);
    d.source_layouts={{1,id(10),bindings}};
    BinaryProgram program(3);program.append({BinaryOpcode::LoadAspect,0,0,0,0,8}); // level-high in source-backed factor order
    program.append({BinaryOpcode::LoadAspect,1,0,0,0,6}); // measurement-valid
    program.append({BinaryOpcode::And,2,0,1,0,0});d.programs={{1,1,program,2}};e.declarations=std::move(d);
    LocalBlockDefinition local;local.block_key=1;local.kind=LocalBlockKind::StrictSubcomplex;
    local.cells.push_back({core::Grade{1},102});for(auto k:{3u,10u,2u})local.cells.push_back({core::Grade{0},k});LocalBlockDefinition other;other.block_key=2;other.kind=LocalBlockKind::StrictSubcomplex;
    other.cells={{core::Grade{1},101},{core::Grade{0},2}};
    for(std::uint64_t k=10;k<140;++k)other.cells.push_back({core::Grade{0},k});
    e.local_blocks={local,other};
    for(auto& block:e.local_blocks)canonicalize_local_block(block);
    validate_temporal_endpoint(e);return e;
}
inline NativeTemporalSeries history(std::uint64_t steps=9){
    require(steps<=64,"sample step budget");auto prev=endpoint(0);NativeTemporalSeries series(prev,namespace_key);
    for(std::uint64_t t=1;t<=steps;++t){auto next=endpoint(t);auto decl=make_temporal_declaration_update(prev.declarations,next.declarations);
        auto segment=make_temporal_segment(prev,next,{}, {},RelationalCodeEncoding::Automatic,std::nullopt,decl);
        series.append(segment);prev=std::move(next);}
    if(steps>=4) { series.checkpoint(4); }
    return series;
}
struct DirectoryLeg {std::uint64_t relation{},occurrence{},target{};bool active{},parent{},child{};};
// Source-bound schema accession. This does not resolve strings/foreign keys.
inline std::vector<DirectoryLeg> directory_legs(const TemporalEndpoint& e){
    // This convenience view is a named application profile, not a guess from a
    // numeric type slot. Reject declaration drift rather than reinterpreting it.
    static const auto expected_signature=signature();
    require(temporal::detail::same_signature_for_temporal_alignment(
                e.source.signature(),expected_signature),
            "catalogue source declaration differs from the sample profile");
    require(e.declarations && e.declarations->namespace_key==namespace_key,
            "catalogue requires its owning application declaration");
    const core::BranchDeclaration expected_branch{id(12),id(1),
        core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
        {{300,"parent",1,1},{400,"child",1,130}}};
    const auto declaration=std::find_if(e.declarations->branches.begin(),
        e.declarations->branches.end(),[](const auto& x){return x.key==2;});
    require(declaration!=e.declarations->branches.end() &&
            declaration->source_type=="directory" &&
            declaration->declaration==expected_branch,
            "catalogue role declaration differs from the sample profile");
    const auto directory_type=e.source.signature().type_id("directory");
    std::vector<DirectoryLeg> out;const auto& g=e.source.grade(core::Grade{1});const auto& low=e.source.grade(core::Grade{0});
    const auto parent=e.source.signature().aspects().id("parent"),child=e.source.signature().aspects().id("child");
    for(std::size_t j=0;j<g.incidence_count();++j)if(g.relation_types[g.upper_index[j]]==directory_type)
        out.push_back({g.relation_identities[g.upper_index[j]].value,g.occurrence_identities[j].value,low.identities[g.lower_index[j]],
            g.aspects.get(e.source.signature().aspects().id("exists"),j),g.aspects.get(parent,j),g.aspects.get(child,j)});
    return out;
}
// Operational one-hot role tensor with an occurrence axis: duplicate target
// identities do not collapse. Inactive support remains; activity is a separate
// source-backed bitplane. Rebuildable view, not a second source in persistence.
inline numeric::SparseRationalTensor directory_tensor(const TemporalEndpoint& e){
    auto legs=directory_legs(e);std::vector<std::size_t> support;std::vector<numeric::SparseRationalEntry> entries;
    for(std::size_t i=0;i<legs.size();++i){if(legs[i].parent){support.push_back(2*i);entries.push_back({2*i,1});}
        if(legs[i].child){support.push_back(2*i+1);entries.push_back({2*i+1,1});}}
    return tensor(numeric::IndexDomain({legs.size(),2}),std::move(support),std::move(entries));
}
} // namespace rfc_preview::sample
