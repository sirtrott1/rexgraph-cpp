#pragma once
#include "rfc/inference/native_relative.hpp"
#include "rfc/core/builder.hpp"

namespace native_relative_example {
using namespace rfc;
using Q=numeric::Rational;
using Space=algebra::RelativeSpace;
using Plan=inference::PreparedNativeRelativePair;
using Map=inference::PreparedNativeRelativePairMap;
using Guard=numeric::ExactActionGuard;
using Values=Plan::Values;
inline core::Grade grade(std::uint16_t k){return core::Grade{k};}
inline core::RelationalComplex source(Q edge=1,Q face=1) {
    core::RelationSignature s; const auto tag=s.aspects().add("retained-tag",composite::AspectRole::Provenance,false);
    s.add_type("edge");s.add_type("face");s.add_type("higher");s.add_type("unused");
    core::RelationalComplexBuilder b(s); b.declare_grade(grade(4));
    const auto add_edge=[&](std::uint64_t key,std::uint64_t tail,std::uint64_t head){
        b.add_grade_one(core::RelationKey{key},0,{
            {tail,true,true,edge,{{tag,true}},core::OccurrenceKey{key*10+1}},
            {head,true,false,edge,{{tag,false}},core::OccurrenceKey{key*10+2}},
            {tail,true,false,Q{2},{{tag,true}},core::OccurrenceKey{key*10+3}},
            {tail,true,true,Q{2},{{tag,false}},core::OccurrenceKey{key*10+4}},
            {head,false,true,Q{0},{{tag,true}},core::OccurrenceKey{key*10+5}},
            {99,true,false,Q{0},{{tag,false}},core::OccurrenceKey{key*10+6}}});
    };
    add_edge(100,10,20);add_edge(200,20,30);add_edge(300,10,30);
    b.add_grade_one(core::RelationKey{900},0,{
        {99,false,true,Q{7},{{tag,true}},core::OccurrenceKey{9001}},
        {99,true,false,Q{0},{{tag,false}},core::OccurrenceKey{9002}}});
    b.add_higher(grade(2),core::RelationKey{100},1,{
        {0,true,false,face,{{tag,true}},core::OccurrenceKey{201}},
        {1,true,false,face,{{tag,false}},core::OccurrenceKey{202}},
        {2,true,true,face,{{tag,true}},core::OccurrenceKey{203}},
        {0,true,false,Q{2},{{tag,false}},core::OccurrenceKey{204}},
        {0,true,true,Q{2},{{tag,true}},core::OccurrenceKey{205}},
        {3,false,true,Q{7},{{tag,false}},core::OccurrenceKey{206}},
        {1,true,false,Q{0},{{tag,true}},core::OccurrenceKey{207}},
        {2,false,true,Q{0},{{tag,false}},core::OccurrenceKey{208}}});
    b.add_higher(grade(3),core::RelationKey{100},2,{
        {0,true,false,Q{1},{{tag,true}},core::OccurrenceKey{301}},
        {0,true,true,Q{1},{{tag,false}},core::OccurrenceKey{302}},
        {0,false,true,Q{9},{{tag,true}},core::OccurrenceKey{303}},
        {0,true,false,Q{0},{{tag,false}},core::OccurrenceKey{304}}});
    return b.freeze();
}
enum class Pair { Face, Full, Point, Path, Empty, Equal, Boundary };
inline std::pair<algebra::SubcomplexMask,algebra::SubcomplexMask> masks(const core::RelationalComplex& c,Pair mode) {
    algebra::SubcomplexMask whole(c),sub(c);
    if(mode==Pair::Empty)return {whole,sub};
    if(mode==Pair::Face){whole.select({grade(2),0});whole.close_boundaries(c);}
    else if(mode==Pair::Path){whole.select({grade(1),0});whole.select({grade(1),1});whole.close_boundaries(c);}
    else for(std::uint16_t k=0;k<c.grade_count();++k) for(std::size_t i=0;i<c.grade(grade(k)).cell_count();++i)whole.select({grade(k),static_cast<std::uint32_t>(i)});
    if(mode==Pair::Face || mode==Pair::Boundary){for(std::uint32_t i=0;i<4;++i)sub.select({grade(1),i});sub.close_boundaries(c);}
    if(mode==Pair::Point)sub.select({grade(0),3});
    if(mode==Pair::Path)sub.select({grade(0),0});
    if(mode==Pair::Equal)sub=whole;
    return {whole,sub};
}
inline inference::NativeRelativePairRequest request(const storage::NativeAccessionDirectory& directory,Pair mode,std::uint64_t key=500) {
    const auto pin=directory.pin(storage::NativeAccessionRequest::sparse_chain(grade(0),{},{}));
    const auto& c=pin.endpoint()->source; const auto [whole,sub]=masks(c,mode);
    inference::NativeRelativePairRequest r{pin,key,3,{},{},64};
    for(std::uint16_t k=0;k<c.grade_count();++k)for(std::size_t i=0;i<c.grade(grade(k)).cell_count();++i){
        const core::CellId cell{grade(k),static_cast<std::uint32_t>(i)};
        if(whole.contains(cell))r.whole.push_back(storage::stable_cell_key(c,cell));
        if(sub.contains(cell))r.sub.push_back(storage::stable_cell_key(c,cell));
    }
    return r;
}
inline constexpr runtime::WorkAvailability available{920,11},target_available{921,15};
inline constexpr inference::NativeRelativeMapAvailability map_available{available,target_available};
inline Plan prepare(Pair mode=Pair::Face,Q edge=1,Q face=1) {
    storage::NativeAccessionDirectory directory(storage::TemporalEndpoint(source(edge,face),11),920);
    return Plan::prepare(request(directory,mode),available);
}
inline inference::NativeRelativeMapRequest identity_map(const Plan& from,const Plan& to) {
    inference::NativeGluingSourceMap m; m.key=770;m.from=from.declaration().key;m.to=to.declaration().key;
    const auto& c=from.source().endpoint()->source;
    for(std::uint16_t k=0;k<c.grade_count();++k){
        const auto g=grade(k);const auto& a=c.grade(g);
        for(std::size_t i=0;i<a.cell_count();++i){const auto key=storage::stable_cell_key(c,{g,static_cast<std::uint32_t>(i)});m.cells.push_back({key,key});}
        for(const auto key:a.occurrence_identities)m.occurrences.push_back({{g,key},{g,key}});
    }
    return {std::move(m)};
}
inline storage::PinnedNativeAccession native_chain(const Plan& plan,core::Grade g,const Values& ambient_values) {
    const auto anchor=plan.source();const auto& c=anchor.endpoint()->source;
    std::vector<std::uint64_t> support;std::vector<storage::NativeCoefficient> coefficients;
    for(const auto& [i,q]:ambient_values){const auto key=storage::stable_cell_key(c,{g,i}).key;support.push_back(key);if(q!=0)coefficients.push_back({key,q});}
    return anchor.access(storage::NativeAccessionRequest::sparse_chain(g,std::move(support),std::move(coefficients)));
}
inline bool zero(const Values& values){for(const auto& [i,q]:values){(void)i;if(q!=0)return false;}return true;}
}
