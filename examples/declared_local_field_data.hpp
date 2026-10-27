#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/storage/declared_local_field.hpp"
#include "rfc/storage/temporal_seed.hpp"

namespace declared_field_example {
using namespace rfc;
using namespace rfc::storage;
using namespace rfc::composite;
using Q=numeric::Rational;
using M=numeric::ExactMatrix;
using T=numeric::SparseRationalTensor;
using S=numeric::SparseRationalVector;
constexpr core::Grade g0{0},g1{1};
inline DeclarationId id(std::uint64_t key){return {4102,key,1};}
struct Fixture {
    TemporalEndpoint endpoint;
    DeclaredLocalFieldContract contract;
    std::vector<DeclaredFieldInput> down_inputs, up_inputs;
};
inline core::RelationalComplex source() {
    core::RelationSignature sig;
    const auto gate=sig.aspects().add("gate",AspectRole::RelationRole);
    const auto type=sig.add_type("typed-local",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(core::RelationKey{101},type,{
        {10,true,true,3,{},core::OccurrenceKey{1001}},
        {20,true,false,1,{},core::OccurrenceKey{1002}},
        {30,true,false,0,{{gate,true}},core::OccurrenceKey{1003}},
        {20,false,false,7,{},core::OccurrenceKey{1004}},
        {10,true,false,Q{1,2},{},core::OccurrenceKey{1005}},
        {10,true,true,Q{1,2},{},core::OccurrenceKey{1006}}});
    return b.freeze();
}
inline Fixture make(bool uniform=false, std::size_t extent=7) {
    Fixture f{TemporalEndpoint(source()),{},{},{}};
    TemporalDeclarationState d;d.namespace_key=4102;
    // V1 charges full support in its declared-value work estimate. This explicit
    // test policy admits four million-coordinate records; it does not change V1.
    if(uniform) d.resource_limits.max_value_nodes=machine::checked_size_add(machine::checked_size_mul(extent,8),100);
    std::vector<Declaration> definitions;
    const std::vector<StableCellKey> cells{{g0,10},{g0,20},{g0,30},{g1,101}};
    for(unsigned family=0;family<2;++family) {
        auto& bindings=family?f.contract.upward:f.contract.downward;
        bindings.allow_constant_identity=uniform;
        const std::vector<std::size_t> dims=family?std::vector<std::size_t>{2,1,3,3}:std::vector<std::size_t>{1,2,3,2};
        for(std::size_t i=0;i<4;++i) {
            const auto n=uniform?extent:dims[i];
            const auto key=100+family*10+i;
            const auto typekey=uniform?1u:key;
            std::vector<std::uint64_t> coordinates;
            if(!uniform)for(std::size_t j=0;j<n;++j)coordinates.push_back(typekey*100+j+1);
            const auto decl=Declaration::rational_tensor(id(typekey),{{typekey,n,coordinates}});
            definitions.push_back(decl);
            TemporalCarrier c{key,TemporalCarrierKind::CellStalk,cells[i].grade,cells[i],uniform?1:key,numeric::IndexDomain::linear(n)};
            bindings.stalks.push_back({c,decl});f.endpoint.carriers.push_back(c);
            if((!family && i==3) || (family && i<3)) {
                TemporalTensor t;t.key=key;t.role=TemporalTensorRole::Field;t.output=key;
                t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell=cells[i];
                std::vector<numeric::SparseRationalEntry> entries;
                if(!family || i==0)entries=uniform?std::vector<numeric::SparseRationalEntry>{{0,Q{-2,3}},{n-1,Q{4,7}}}:
                    std::vector<numeric::SparseRationalEntry>{{0,3},{1,4}};
                t.tensor=T(c.domain,numeric::StructuralSupport::full(n),S(n,std::move(entries)));
                TemporalDeclaredValue v;v.key=key+1000;v.type=decl.id;v.cell=cells[i];v.value=DeclaredValue::tensor(t.tensor);
                f.endpoint.tensors.push_back(t);d.values.push_back(v);
                (family?f.up_inputs:f.down_inputs).push_back({t.key,v.key});
            }
        }
    }
    if(!uniform) {
        const std::vector<M> down{M::from_rows({{2,0}}),M::identity(2),M(3,2),M(2,2),M::from_rows({{1,0}}),M::from_rows({{1,0}})};
        const std::vector<M> up{M::from_rows({{2,0},{0,0},{0,5}}),M(3,1),M::identity(3),M(3,1),M(3,2),M(3,2)};
        const std::vector<unsigned> lower{0,1,2,1,0,0};
        for(unsigned family=0;family<2;++family)for(std::size_t i=0;i<6;++i) {
            TemporalTensor t;t.key=200+family*10+i;
            t.role=family?TemporalTensorRole::SheafRestriction:TemporalTensorRole::CoefficientIndependentMap;
            t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence={g1,core::OccurrenceKey{1001+i}};
            t.input=family?110+lower[i]:103;t.output=family?113:100+lower[i];
            t.tensor=temporal_matrix_tensor((family?up:down)[i]);f.endpoint.tensors.push_back(t);
            (family?f.contract.upward:f.contract.downward).maps.push_back({t.key,t.role});
        }
    }
    definitions.push_back(Declaration::boolean(id(900)));
    definitions.push_back(Declaration::product(id(901),{{1,id(900),0},{2,id(900),0}}));
    d.registry=DeclarationRegistry(std::move(definitions));
    d.source_layouts.push_back({901,id(901),{{1,"exists"},{2,"gate"}}});
    BinaryProgram program(3);
    program.append({BinaryOpcode::LoadAspect,0,0,0,0,0});
    program.append({BinaryOpcode::LoadAspect,1,0,0,0,1});
    program.append({BinaryOpcode::And,2,0,1});
    d.programs.push_back({902,901,program,2});
    f.endpoint.declarations=std::move(d);
    validate_temporal_endpoint(f.endpoint);
    return f;
}
} // namespace declared_field_example
