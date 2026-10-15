#pragma once
#include "rfc/inference/native_cylinder.hpp"
#include "../native_relative/fixture.hpp"

namespace native_cylinder_example {
using namespace rfc;
using Q = numeric::Rational;
using Plan = inference::PreparedNativeCylinder;
using Map = inference::PreparedNativeCylinderMap;
using Values = Plan::Values;
using ProductValues = Plan::ProductValues;
using Guard = numeric::ExactActionGuard;
using Endpoint = topology::CylinderEndpoint;
inline core::Grade grade(std::uint16_t k) { return core::Grade{k}; }
inline constexpr inference::NativeCylinderAvailability available{{930,13},{931,17}}, target_available{{932,19},{933,23}};
inline constexpr inference::NativeCylinderMapAvailability map_available{available,target_available};
inline core::RelationalComplex source(Q edge = 1, Q face = 1) { return native_relative_example::source(edge,face); }
inline core::RelationalComplex branch() {
    core::RelationSignature signature; const auto type = signature.add_branch_type("weighted-branch",4,4);
    core::RelationalComplexBuilder b(signature); b.declare_grade(grade(3));
    b.add_weighted_branch_grade_one(type,{10,20,30,40},0,{Q{1,4},Q{1,2},Q{1,4}});
    return b.freeze();
}
inline core::RelationalComplex ordered() {
    core::RelationSignature signature; const auto edge = signature.add_type("edge");
    const auto loop = signature.add_attaching_loop_type("ordered-loop",4,4);
    core::RelationalComplexBuilder b(signature); b.declare_grade(grade(3));
    b.add_grade_one(core::RelationKey{100},edge,{{10,true,true,Q{1},{},core::OccurrenceKey{101}}, {20,true,false,Q{1},{},core::OccurrenceKey{102}}});
    b.add_higher(grade(2),core::RelationKey{100},loop,{{0,true,false,Q{1},{},core::OccurrenceKey{201}}, {0,true,true,Q{1},{},core::OccurrenceKey{202}},
        {0,true,false,Q{1},{},core::OccurrenceKey{203}}, {0,true,true,Q{1},{},core::OccurrenceKey{204}}});
    return b.freeze();
}
inline core::RelationalComplex unit(bool empty = false, std::uint16_t top = 0, bool extra = false) {
    core::RelationalComplexBuilder b{core::RelationSignature{}}; b.declare_grade(grade(top));
    if(!empty)b.declare_witness_identity(10);
    if(extra)b.declare_witness_identity(20);
    return b.freeze();
}
inline core::RelationalComplex coalescing(bool target = false, bool unused = false) {
    core::RelationSignature signature; signature.add_type("empty-assertion"); core::RelationalComplexBuilder b(signature);
    b.declare_grade(grade(2)); b.declare_witness_identity(10);
    b.add_grade_one(core::RelationKey{100},0,{});
    if(!target)b.add_grade_one(core::RelationKey{200},0,{});
    if(unused)b.add_grade_one(core::RelationKey{900},0,{{10,true,true,Q{9},{},core::OccurrenceKey{901}},
        {10,true,false,Q{9},{},core::OccurrenceKey{902}}});
    return b.freeze();
}
inline algebra::RelationalProductDeclaration declaration(const core::RelationalComplex& a, const core::RelationalComplex& b, std::uint64_t key = 700) {
    algebra::RelationalProductDeclaration out{key,5,{}};
    for(std::size_t i=0;i<=a.signature().type_count();++i)for(std::size_t j=0;j<=b.signature().type_count();++j)
        out.types.push_back({i?std::optional{static_cast<core::RelationTypeId>(i-1)}:std::nullopt,
            j?std::optional{static_cast<core::RelationTypeId>(j-1)}:std::nullopt,key+1+out.types.size()});
    return out;
}
inline storage::PinnedNativeAccession anchor(const storage::NativeAccessionDirectory& directory) {
    return directory.pin(storage::NativeAccessionRequest::sparse_chain(grade(0),{},{}));
}
inline inference::NativeTensorProductRequest request(const storage::NativeAccessionDirectory& a, const storage::NativeAccessionDirectory& b, bool target = false) {
    const auto left=anchor(a),right=anchor(b);
    return {left,right,target?81u:71u,target?82u:72u,declaration(left.endpoint()->source,right.endpoint()->source,target?800u:700u),64};
}
inline Plan prepare(core::RelationalComplex c = source(), bool target = false) {
    topology::RelationalInterval interval;
    storage::NativeAccessionDirectory a(storage::TemporalEndpoint(std::move(c),target?19u:13u),target?932u:930u);
    storage::NativeAccessionDirectory b(storage::TemporalEndpoint(interval.complex(),target?23u:17u),target?933u:931u);
    return Plan::prepare(request(a,b,target),target?target_available:available);
}
inline inference::NativeGluingSourceMap factor_map(const Plan& from,const Plan& to,bool interval) {
    inference::NativeGluingSourceMap out;out.key=interval?901u:900u;
    out.from=interval?from.declaration().right_factor:from.declaration().left_factor;
    out.to=interval?to.declaration().right_factor:to.declaration().left_factor;
    const auto pin=from.factor(interval);const auto& c=pin.endpoint()->source;
    for(std::uint16_t k=0;k<c.grade_count();++k){const auto g=grade(k);const auto& a=c.grade(g);
        for(std::size_t i=0;i<a.cell_count();++i){const auto key=storage::stable_cell_key(c,{g,static_cast<std::uint32_t>(i)});out.cells.push_back({key,key});}
        for(const auto key:a.occurrence_identities)out.occurrences.push_back({{g,key},{g,key}});
    }
    return out;
}
inline inference::NativeCylinderMapRequest map_request(const Plan& from,const Plan& to) { return {990,factor_map(from,to,false),factor_map(from,to,true)}; }
inline storage::PinnedNativeAccession native_chain(const Plan& plan,core::Grade g,const Values& input) {
    const auto pin=plan.factor(false);const auto& c=pin.endpoint()->source;std::vector<std::uint64_t> support;std::vector<storage::NativeCoefficient> values;
    for(const auto& [i,q]:input){const auto key=storage::stable_cell_key(c,{g,i}).key;support.push_back(key);if(q!=0)values.push_back({key,q});}
    return pin.access(storage::NativeAccessionRequest::sparse_chain(g,std::move(support),std::move(values)));
}
template<class V>inline bool zero(const V& values){for(const auto& [i,q]:values){(void)i;if(q!=0)return false;}return true;}
} // namespace native_cylinder_example
