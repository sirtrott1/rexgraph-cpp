#pragma once
#include "rfc/inference/native_gluing.hpp"
#include "rfc/core/builder.hpp"

namespace native_gluing_example {
using namespace rfc;
using Q=numeric::Rational;
using Plan=inference::PreparedNativeGluing;
using Map=inference::PreparedNativeGluingMorphism;
using Guard=numeric::ExactActionGuard;
using Values=Plan::Values;
inline core::Grade grade(std::uint16_t g) { return core::Grade{g}; }
inline storage::StableCellKey cell(std::uint16_t g,std::uint64_t key) { return {grade(g),key}; }
inline storage::StableOccurrenceKey occurrence(std::uint16_t g,std::uint64_t key) { return {grade(g),core::OccurrenceKey{key}}; }
inline core::RelationSignature signature() {
    core::RelationSignature s;
    s.aspects().add("retained-tag",composite::AspectRole::Provenance,false);
    s.add_type("edge"); s.add_type("face"); s.add_type("higher"); s.add_type("unused-type");
    return s;
}
inline core::RelationalComplex triangle(Q edge_scale=1,Q face_scale=1,bool common=false,bool empty=false,std::uint16_t maximum=3) {
    const auto s=signature(); const auto tag=s.aspects().id("retained-tag"); core::RelationalComplexBuilder b(s);
    b.declare_grade(grade(maximum)); if (empty) return b.freeze();
    const auto edge=[&](std::uint64_t key,std::uint64_t tail,std::uint64_t head) {
        b.add_grade_one(core::RelationKey{key},0,{
            {tail,true,true,edge_scale,{{tag,true}},core::OccurrenceKey{key*10+1}},
            {head,true,false,edge_scale,{{tag,false}},core::OccurrenceKey{key*10+2}},
            {tail,true,false,Q{2},{{tag,true}},core::OccurrenceKey{key*10+3}},
            {tail,true,true,Q{2},{{tag,false}},core::OccurrenceKey{key*10+4}},
            {head,false,true,Q{0},{{tag,true}},core::OccurrenceKey{key*10+5}},
            {head,true,false,Q{0},{{tag,false}},core::OccurrenceKey{key*10+6}}});
    };
    edge(100,10,20); if (common) return b.freeze();
    edge(200,20,30); edge(300,10,30);
    b.add_higher(grade(2),core::RelationKey{100},1,{
        {0,true,false,face_scale,{{tag,true}},core::OccurrenceKey{201}},
        {1,true,false,face_scale,{{tag,false}},core::OccurrenceKey{202}},
        {2,true,true,face_scale,{{tag,true}},core::OccurrenceKey{203}},
        {0,true,false,Q{2},{{tag,false}},core::OccurrenceKey{204}},
        {0,true,true,Q{2},{{tag,true}},core::OccurrenceKey{205}},
        {2,false,true,Q{0},{{tag,false}},core::OccurrenceKey{206}},
        {1,true,false,Q{0},{{tag,true}},core::OccurrenceKey{207}}});
    b.add_higher(grade(3),core::RelationKey{100},2,{
        {0,true,false,Q{1},{{tag,true}},core::OccurrenceKey{301}},
        {0,true,true,Q{1},{{tag,false}},core::OccurrenceKey{302}},
        {0,false,true,Q{0},{{tag,true}},core::OccurrenceKey{303}},
        {0,true,false,Q{0},{{tag,false}},core::OccurrenceKey{304}}});
    return b.freeze();
}
inline storage::NativeAtlasManifest manifest(bool empty=false) {
    storage::NativeAtlasManifest m; m.key=700; m.revision=3;
    for (std::uint64_t i=0;i<2;++i) {
        storage::NativeSourceEmbedding e; e.key=i+1; e.from=1; e.to=i+2;
        if (!empty) {
            e.cells={{cell(0,10),cell(0,10)},{cell(0,20),cell(0,20)},{cell(1,100),cell(1,100)}};
            for (std::uint64_t p=1;p<=6;++p) e.occurrences.push_back({occurrence(1,1000+p),occurrence(1,1000+p)});
        }
        m.embeddings.push_back(std::move(e));
    }
    m.pushouts={{500,1,2,{},{}}}; return m;
}
inline storage::NativeAtlas atlas(Q e=1,Q f=1,bool empty=false) {
    std::vector<storage::NativeAtlasSource> sources{
        {1,900,storage::TemporalEndpoint(triangle(e,f,true,empty),7)},
        {2,901,storage::TemporalEndpoint(triangle(e,f,false,empty),8)},
        {3,902,storage::TemporalEndpoint(triangle(e,f,false,empty),9)}};
    return {sources,manifest(empty)};
}
inline constexpr inference::NativeGluingAvailability available{{900,7},{901,8},{902,9}};
inline constexpr inference::NativeGluingMapAvailability map_available{available,{903,10}};
inline Plan prepare(Q e=1,Q f=1,bool empty=false) { return Plan::prepare({atlas(e,f,empty),500},available); }
inline storage::PinnedNativeAccession local_chain(const Plan& plan,bool right,core::Grade g,const Values& values,bool complete=false) {
    const auto anchor=plan.source_anchor(right?algebra::GluingSource::Right:algebra::GluingSource::Left);
    const auto& source=anchor.endpoint()->source;
    std::vector<std::uint64_t> cells; std::vector<storage::NativeCoefficient> coefficients;
    for (const auto& [i,q]:values) { const auto key=storage::stable_cell_key(source,{g,i}).key; cells.push_back(key); if(q!=0) coefficients.push_back({key,q}); }
    if (complete) {
        cells.clear(); for (std::size_t i=0;i<source.grade(g).cell_count();++i) cells.push_back(storage::stable_cell_key(source,{g,static_cast<std::uint32_t>(i)}).key);
    }
    return anchor.access(storage::NativeAccessionRequest::sparse_chain(g,std::move(cells),std::move(coefficients)));
}
inline inference::NativeGluingSourceMap target_map(std::uint64_t from,const core::RelationalComplex& source) {
    inference::NativeGluingSourceMap m; m.key=from; m.from=from; m.to=4;
    for (std::size_t k=0;k<source.grade_count();++k) {
        const auto g=grade(static_cast<std::uint16_t>(k)); const auto& a=source.grade(g);
        for (std::size_t i=0;i<a.cell_count();++i) { const auto key=storage::stable_cell_key(source,{g,static_cast<std::uint32_t>(i)}); m.cells.push_back({key,key}); }
        for (const auto key:a.occurrence_identities) m.occurrences.push_back({{g,key},{g,key}});
    }
    return m;
}
inline inference::NativeGluingMorphismRequest map_request(const Plan& plan,const storage::NativeAccessionDirectory& target) {
    const auto pin=target.pin(storage::NativeAccessionRequest::sparse_chain(grade(0),{},{}));
    return {pin,4,target_map(2,plan.source_anchor(algebra::GluingSource::Left).endpoint()->source),
        target_map(3,plan.source_anchor(algebra::GluingSource::Right).endpoint()->source)};
}
} // namespace native_gluing_example
