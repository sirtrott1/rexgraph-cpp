#pragma once
#include "rfc/storage/temporal_seed.hpp"
#include "rfc/storage/relational_code_observations.hpp"

namespace f2_fixture {
using namespace rfc;
using namespace rfc::storage;
using namespace rfc::composite;
using Q=numeric::Rational;
using V=DeclaredValue;
inline DeclarationId id(std::uint64_t key,std::uint64_t version=1){return {77,key,version};}
inline DeclarationRegistry definitions(bool next=false) {
    const auto v=next?2u:1u;
    std::vector<Declaration> ds={
        Declaration::boolean(id(1,v)),
        Declaration::finite(id(2,v),next?std::vector<std::uint64_t>{60,50,40,30,20,10,70}:std::vector<std::uint64_t>{10,20,30,40,50,60}),
        Declaration::rational(id(3,v)),Declaration::optional(id(4,v),id(3,v)),
        Declaration::repeated(id(5,v),id(1,v),0,next?512:129),
        Declaration::product(id(6,v),next?std::vector<DeclarationFactor>{{20,id(4,v),0},{10,id(2,v),0},{30,id(1,v),0}}:
                                                         std::vector<DeclarationFactor>{{10,id(2,v),0},{20,id(4,v),0}}),
        Declaration::product(id(7,v),next?std::vector<DeclarationFactor>{{200,id(1,v),0},{100,id(1,v),0}}:
                                                         std::vector<DeclarationFactor>{{100,id(1,v),0},{200,id(1,v),0}})
    };
    return DeclarationRegistry(std::move(ds));
}
inline std::vector<DeclarationMap> maps(){
    std::vector<DeclarationMap> out;
    for(std::uint64_t k=1;k<=7;++k)out.push_back({id(k),id(k,2),
        (k==2||k==5||k==6)?DeclarationMapKind::Embedding:DeclarationMapKind::Recode,{}});
    out[5].fills={{30,V::boolean(true)}};return out;
}
inline core::RelationalComplex source() {
    core::RelationSignature sig;
    const auto left=sig.aspects().add("left",AspectRole::RelationRole);
    const auto right=sig.aspects().add("right",AspectRole::RelationRole);
    const auto tag=sig.aspects().add("tag",AspectRole::Provenance);
    const auto type=sig.add_type("collective");const auto empty=sig.add_type("empty");
    core::RelationalComplexBuilder b(sig);b.declare_grade(core::Grade{3});
    std::vector<core::BoundaryOccurrenceInput> os;
    for(std::size_t i=0;i<5;++i)os.push_back({10+i%2,i!=4,false,Q{0},{{left,i<2},{right,i>=2},{tag,i%2!=0}},core::OccurrenceKey{100+i}});
    b.add_grade_one(core::RelationKey{7},type,os);b.add_grade_one(core::RelationKey{8},empty,{});
    return b.freeze();
}
inline SourceDeclarationMigration source_map(const core::RelationSignature& base) {
    AspectSchema aspects;
    aspects.add("consumer",AspectRole::RelationRole);
    aspects.add("sign",AspectRole::Orientation,true);
    aspects.add("introduced",AspectRole::Custom,true);
    aspects.add("present",AspectRole::Existence,false);
    aspects.add("producer",AspectRole::RelationRole);
    aspects.add("origin",AspectRole::Provenance,true);
    core::RelationSignature target(aspects);target.add_type("new-type");target.add_type("empty-v2");target.add_type("collective-v2");
    SourceDeclarationMigration m{base,target,
        {{"exists","present"},{"orientation","sign"},{"left","producer"},{"right","consumer"},{"tag","origin"}},
        {{"collective","collective-v2"},{"empty","empty-v2"}},
        {{"introduced",false,{{{core::Grade{1},core::OccurrenceKey{101}},true}}}}};
    return m;
}
inline TemporalDeclarationState declaration_state() {
    TemporalDeclarationState s;s.namespace_key=111;s.registry=definitions();
    TemporalDeclaredValue v;v.key=1;v.type=id(6);v.branch=1;v.cell=StableCellKey{core::Grade{1},7};
    v.value=V::product({V::symbol(30),V::present(V::rational(Q{0}))});s.values.push_back(v);
    v.key=2;v.type=id(5);v.branch=0;v.cell.reset();v.value=V::repeated({V::boolean(true),V::boolean(false)});s.values.push_back(v);
    s.branches.push_back({1,"collective",{id(90),id(6),core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
                                        {{100,"left",2,2},{200,"right",3,3}}}});
    s.source_layouts.push_back({1,id(7),{{100,"left"},{200,"right"}}});
    s.code_types={{1,id(1)},{2,id(7)}};
    return s;
}
inline RelationalCodeState codes() {
    RelationalCodeState c;c.namespace_key=500;
    c.types={{1,RelationalCodeTypeKind::Finite,{0,1},{}},
             {2,RelationalCodeTypeKind::Product,{},{{100,1,0,"left"},{200,1,0,"right"}}}};
    RelationalCodeOperation identity;identity.key=10;identity.input=1;identity.output=1;c.operations.push_back(identity);
    c.fields={1};
    c.sections.push_back({1000,1,2,0,{},StableOccurrenceKey{core::Grade{1},core::OccurrenceKey{100}},true,RelationalCodeValue{std::uint64_t{0}}});
    return c;
}
inline TemporalEndpoint endpoint() {
    TemporalEndpoint e(source());e.declarations=declaration_state();e.codes=codes();
    e.carriers.push_back({10,TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(2)});
    TemporalTensor t;t.key=1;t.role=TemporalTensorRole::Field;t.output=10;
    t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{5}}}));
    e.tensors.push_back(t);
    LocalBlockDefinition block;block.block_key=20;block.kind=LocalBlockKind::StrictSubcomplex;
    block.cells={{core::Grade{0},10},{core::Grade{0},11},{core::Grade{1},7}};e.local_blocks.push_back(block);
    validate_temporal_endpoint(e);return e;
}
inline TemporalEndpoint evolved(const TemporalEndpoint& base, const SourceDeclarationMigration& migration) {
    auto e=base;e.source=recode_temporal_source(base.source,migration);e.sequence=base.sequence+1;
    auto& s=*e.declarations;s.registry=definitions(true);
    const DeclarationRecoding recoding(base.declarations->registry,s.registry,maps());
    for(auto& v:s.values){v.value=recoding.apply(v.type,v.value);v.type=recoding.target_id(v.type);}
    auto& branch=s.branches[0];branch.source_type="collective-v2";branch.declaration.id=id(90,2);branch.declaration.payload=id(6,2);
    branch.declaration.roles={{100,"producer",2,2},{200,"consumer",3,3}};
    s.source_layouts={{1,id(7,2),{{100,"producer"},{200,"consumer"}}}};
    s.code_types={{1,id(1,2)},{3,id(7,2)}};
    auto& c=*e.codes;c.types.push_back({3,RelationalCodeTypeKind::Product,{},{{200,1,0,"consumer"},{100,1,0,"producer"}}});
    c.sections[0].type=3;
    RelationalCodeOperation transport;transport.key=20;transport.input=2;transport.output=3;transport.opcode=RelationalCodeOpcode::FactorMap;
    transport.legs={{200,10},{100,10}};c.operations.push_back(transport);
    validate_temporal_endpoint(e);return e;
}
inline TemporalSegment segment(const TemporalEndpoint& base, const TemporalEndpoint& next,const SourceDeclarationMigration& migration){
    return make_temporal_segment(base,next,{},{{1000,20}},RelationalCodeEncoding::Automatic,migration,
        make_temporal_declaration_update(base.declarations,next.declarations,maps()));
}
} // namespace f2_fixture
