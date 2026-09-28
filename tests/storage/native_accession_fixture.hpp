#pragma once
#include "rfc/storage/native_accession.hpp"

namespace native_accession_fixture {
using namespace rfc;
using namespace rfc::storage;
using Q=numeric::Rational;
using D=composite::Declaration;
using V=composite::DeclaredValue;
inline composite::DeclarationId id(std::uint64_t key,std::uint64_t version=1) {return {321,key,version};}
inline core::RelationalComplex source(std::size_t count=8,std::size_t custom=2,bool flip=false,bool reverse=false) {
    core::RelationSignature sig;
    for(std::size_t a=0;a<custom;++a)sig.aspects().add("aspect-"+std::to_string(a));
    const auto type=sig.add_type("branch",core::BoundaryPolicy::DeclaredExact,3,3);
    core::RelationalComplexBuilder b(sig);b.declare_grade(core::Grade{2});b.declare_witness_identity(12);
    for(std::size_t k=0;k<count;++k) {
        const auto i=reverse?count-1-k:k;std::vector<core::BoundaryOccurrenceInput> legs;
        for(std::size_t j=0;j<3;++j) {
            std::vector<core::AspectValue> bits;
            for(std::size_t a=0;a<custom;++a)bits.push_back({static_cast<composite::AspectId>(a+2),(3*i+j+a)%3==0});
            const bool oriented=(j==1)!=(flip && i==0 && j==0);
            legs.push_back({10+j%2,j!=2,oriented,j==2?Q{0}:Q{1},bits,core::OccurrenceKey{10000+3*i+j}});
        }
        b.add_grade_one(core::RelationKey{1000+i},type,legs);
    }
    return b.freeze();
}
inline TemporalTensor field(std::uint64_t key,std::uint64_t carrier,std::size_t count,int offset) {
    TemporalTensor t;t.key=key;t.role=TemporalTensorRole::Field;t.output=carrier;
    std::vector<numeric::SparseRationalEntry> xs;
    for(std::size_t i=0;i<count;++i)if((static_cast<int>(i%5)+offset)!=0)xs.push_back({i,Q{static_cast<int>(i%5)+offset}});
    t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(count),numeric::StructuralSupport::full(count),
                                          numeric::SparseRationalVector(count,std::move(xs)));return t;
}
inline TemporalEndpoint endpoint(std::size_t count=8,std::size_t custom=2) {
    TemporalEndpoint e(source(count,custom));
    e.carriers={{1,TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(count)},
                {2,TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(3)}};
    e.tensors={field(101,1,count,0),field(102,1,count,2),field(103,2,3,3)};
    TemporalTensor t;t.key=200;t.role=TemporalTensorRole::OpaqueExact;t.interpretation="supported-zero-fixture";
    t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain({2,3}),numeric::StructuralSupport::sparse(6,{0,2,5}),
                                         numeric::SparseRationalVector(6,{{0,Q{1,3}},{5,Q{-2,7}}}));e.tensors.push_back(t);
    t.key=201;t.role=TemporalTensorRole::Observation;t.input=1;t.output=2;t.interpretation.clear();
    numeric::ExactMatrix m(3,count);m.set(0,0,Q{1});m.set(2,0,Q{2});if(count>1)m.set(1,1,Q{-1});
    t.tensor=temporal_matrix_tensor(m);e.tensors.push_back(t);
    t.key=202;t.role=TemporalTensorRole::Metric;t.input=2;t.output=2;
    numeric::ExactMatrix metric(3,3);for(std::size_t i=0;i<3;++i)metric.set(i,i,Q{static_cast<unsigned long>(i+1)});
    t.tensor=temporal_matrix_tensor(metric);e.tensors.push_back(t);
    LocalBlockDefinition local;local.block_key=1;local.kind=LocalBlockKind::StrictSubcomplex;
    local.cells={{core::Grade{0},10},{core::Grade{0},11},{core::Grade{1},1000}};
    e.local_blocks.push_back(local);
    LocalBlockDefinition partial;partial.block_key=2;partial.kind=LocalBlockKind::PartialChart;
    partial.cells={{core::Grade{0},10},{core::Grade{1},1000}};
    partial.occurrences={{core::Grade{1},core::OccurrenceKey{10000}}};e.local_blocks.push_back(partial);
    TemporalDeclarationState d;d.namespace_key=321;
    std::vector<D> defs{D::boolean(id(1)),D::finite(id(2),{4,7,9}),D::rational_tensor(id(3),{{40,2,{11,22}}}),
        D::product(id(4),{{500,id(1),0},{501,id(2),0},{502,id(3),1}})};
    std::vector<composite::DeclarationFactor> factors;
    TemporalSourceAspectLayout layout;layout.key=400;layout.type=id(5);
    for(std::size_t a=0;a<e.source.signature().aspects().size();++a) {
        factors.push_back({9000+a,id(1),0});
        layout.factors.push_back({9000+a,e.source.signature().aspects().at(static_cast<composite::AspectId>(a)).name});
    }
    defs.push_back(D::product(id(5),factors));d.registry=composite::DeclarationRegistry(defs);d.source_layouts={layout};
    const auto value=V::product({V::boolean(true),V::symbol(7),V::tensor(numeric::SparseRationalTensor(
        numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{5,11}}})))});
    d.values={{301,id(4),1,0,StableCellKey{core::Grade{1},1000},{},value},
              {302,id(4),0,0,StableCellKey{core::Grade{1},1001},{},value}};
    TemporalSourceLayoutProgram program;program.key=401;program.layout=400;program.program=composite::BinaryProgram(3);program.output=2;
    program.program.append({composite::BinaryOpcode::LoadAspect,0,0,0,0,0});
    program.program.append({composite::BinaryOpcode::LoadAspect,1,0,0,0,1});
    program.program.append({composite::BinaryOpcode::AndNot,2,0,1});d.programs={program};e.declarations=std::move(d);
    RelationalCodeState c;c.namespace_key=777;c.fields={100,200};
    c.types={{1,RelationalCodeTypeKind::Rational,{10,11},{}},{2,RelationalCodeTypeKind::Finite,{0,1},{}}};
    c.types.push_back(relational_aspect_code_type(e.source,3,2,
        {{500,e.source.signature().existence_aspect()},{501,e.source.signature().orientation_aspect()}}));
    const auto cv=RelationalCodeValue{numeric::SparseRationalTensor(numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{2}}}))};
    for(std::uint64_t key=10;key<12;++key){RelationalCodeSection s;s.key=key;s.type=1;s.field=100;s.value=cv;c.sections.push_back(s);}
    c.sections.push_back(relational_aspect_code_section(1001,200,3,{core::Grade{1},core::OccurrenceKey{10000}}));
    c.sections.push_back(relational_aspect_code_section(1002,200,3,{core::Grade{1},core::OccurrenceKey{10001}}));
    RelationalCodeOperation op;op.key=20;op.input=1;op.output=1;op.opcode=RelationalCodeOpcode::Linear;
    op.kernel=temporal_matrix_tensor(numeric::ExactMatrix::identity(2));c.operations.push_back(op);
    c.interfaces={{50,10,11,20,20}};e.codes=std::move(c);
    validate_temporal_endpoint(e);return e;
}
inline std::vector<RelationalCodeObservationQuery> queries() {
    using K=RelationalCodeObservationKind;
    return {{1,K::Section,0,100,{}},{2,K::Application,20,100,{}},{3,K::Operation,0,{}, {20}},
             {4,K::Section,0,200,{}},{5,K::Interface,0,{}, {50}}};
}
inline NativeAccessionRequest request(NativeAccessionKind kind,std::uint64_t key) {
    NativeAccessionRequest r;r.kind=kind;r.subject=key;return r;
}
} // namespace native_accession_fixture
