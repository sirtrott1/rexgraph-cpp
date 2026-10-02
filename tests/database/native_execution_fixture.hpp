#pragma once

#include "rfc/database/native_execution.hpp"
#include "rfc/core/builder.hpp"

namespace native_execution_fixture {
using namespace rfc;
using namespace rfc::storage;
using Q = numeric::Rational;

inline TemporalEndpoint endpoint(bool scalar = false, bool realized = false, bool zero = false) {
    core::RelationSignature signature;
    const auto producer = signature.aspects().add("producer",composite::AspectRole::RelationRole);
    const auto consumer = signature.aspects().add("consumer",composite::AspectRole::RelationRole);
    const auto type = signature.add_type("collective",core::BoundaryPolicy::DeclaredExact,0,10);
    core::RelationalComplexBuilder b(signature);
    b.declare_grade(core::Grade{2});
    b.add_grade_one(core::RelationKey{101},type,{
        {10,true,false,Q{2},{{producer,true}},core::OccurrenceKey{1001}},
        {11,true,true,Q{3},{{consumer,true}},core::OccurrenceKey{1002}},
        {10,true,true,Q{2},{{consumer,true}},core::OccurrenceKey{1003}},
        {10,false,false,Q{7},{{producer,true}},core::OccurrenceKey{1004}}});
    b.add_grade_one(core::RelationKey{102},type,{});
    TemporalEndpoint e(b.freeze());
    for (std::size_t k=0;k<e.source.grade_count();++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const auto& arena = e.source.grade(grade);
        for (std::size_t i=0;i<arena.cell_count();++i) {
            const auto key = stable_cell_key(e.source,{grade,static_cast<std::uint32_t>(i)}).key;
            const auto dim = scalar ? std::size_t{1} : (k==0 && key==11 ? std::size_t{3} : std::size_t{2});
            e.carriers.push_back({key,TemporalCarrierKind::CellStalk,grade,StableCellKey{grade,key},scalar?900u:900u+key,numeric::IndexDomain::linear(dim)});
            if (k!=1) continue;
            TemporalTensor field;
            field.key=key+2000; field.role=TemporalTensorRole::Field; field.owner.kind=TemporalOwnerKind::Cell;
            field.owner.cell={grade,key};field.output=key;field.interpretation="declared test upper field";
            std::vector<numeric::SparseRationalEntry> entries;
            if (!zero) { entries.push_back({0,Q{7,11}}); if (dim>1) entries.push_back({1,Q{13,17}}); }
            field.tensor={numeric::IndexDomain::linear(dim),numeric::StructuralSupport::full(dim),numeric::SparseRationalVector(dim,entries)};
            e.tensors.push_back(field);
        }
    }
    if (!scalar) {
        for (std::uint64_t occurrence=1001;occurrence<=1004;++occurrence) {
            const auto rows=occurrence==1002?3u:2u;
            numeric::ExactMatrix m(rows,2);
            if (occurrence==1002) {m.set(0,0,Q{1,3});m.set(1,1,Q{5,7});m.set(2,0,Q{11,13});m.set(2,1,Q{-2,19});}
            else {m.set(0,0,Q{5});m.set(1,0,Q{1});m.set(1,1,Q{2});}
            TemporalTensor t;
            t.key=occurrence+3000;t.role=realized?TemporalTensorRole::RealizedIncidenceMap:TemporalTensorRole::CoefficientIndependentMap;
            t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence={core::Grade{1},core::OccurrenceKey{occurrence}};
            t.input=101;t.output=occurrence==1002?11:10;t.interpretation="declared occurrence transport";
            if (realized) m=m.scale(e.source.incidence_coefficient(core::Grade{1},static_cast<std::size_t>(occurrence-1001)));
            t.tensor=temporal_matrix_tensor(m);e.tensors.push_back(t);
        }
    }
    validate_temporal_endpoint(e);
    return e;
}
inline NativeAccessionRequest field_request(std::uint64_t key) {
    NativeAccessionRequest r;r.kind=NativeAccessionKind::Field;r.subject=key;return r;
}
inline NativeFieldRequest request(const NativeAccessionDirectory& directory, core::Grade grade = core::Grade{1}) {
    NativeFieldRequest r;r.upper_grade=grade;
    const auto e=directory.pin_snapshot();
    for(const auto& c:e->carriers)if(c.kind==TemporalCarrierKind::CellStalk)r.stalk_carriers.push_back(c.key);
    for(const auto& t:e->tensors)if(t.role==TemporalTensorRole::Field && t.owner.kind==TemporalOwnerKind::Cell && t.owner.cell.grade==grade)
        r.fields.push_back(directory.pin(field_request(t.key)));
    return r;
}
inline PinnedNativeAccession anchor(const NativeAccessionDirectory& directory, core::Grade grade=core::Grade{1}) {
    return directory.pin(NativeAccessionRequest::sparse_chain(grade,{},{}));
}
} // namespace native_execution_fixture
