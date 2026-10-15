#pragma once
#include "rfc/inference/native_relational_homotopy.hpp"
#include "../native_cylinder/fixture.hpp"

namespace native_relational_homotopy_example {
using namespace rfc;
using Q = numeric::Rational;
using Guard = numeric::ExactActionGuard;
using Cylinder = inference::PreparedNativeCylinder;
using Plan = inference::PreparedNativeRelationalHomotopy;
using Values = Cylinder::Values;
using Endpoint = topology::CylinderEndpoint;
using native_cylinder_example::grade;
inline constexpr inference::NativeRelationalHomotopyAvailability available{{{1101,11},{1102,13}},{1103,17},{1104,19}};
inline core::RelationSignature signature(bool attaching = false) {
    core::RelationSignature s; s.add_type("exact-relation",core::BoundaryPolicy::DeclaredExact);
    if(attaching)s.add_attaching_loop_type("ordered-face",4,4);else s.add_type("exact-face",core::BoundaryPolicy::DeclaredExact);
    s.add_type("declared-prism",core::BoundaryPolicy::DeclaredExact); return s;
}
inline core::RelationalComplex source(unsigned kind = 0) {
    core::RelationalComplexBuilder b(signature(kind == 2)); b.declare_grade(grade(kind == 2 ? 3 : 2));
    if(kind == 3 || kind == 5) { b.declare_witness_identity(10); b.add_grade_one(core::RelationKey{100},0,{}); if(kind == 3)b.add_grade_one(core::RelationKey{200},0,{}); return b.freeze(); }
    if(kind == 4)return b.freeze();
    if(kind == 2) {
        b.add_grade_one(core::RelationKey{100},0,{{10,true,true,Q{1},{},core::OccurrenceKey{101}},{20,true,false,Q{1},{},core::OccurrenceKey{102}}});
        b.add_higher(grade(2),core::RelationKey{200},1,{{0,true,false,Q{1},{},core::OccurrenceKey{201}},{0,true,true,Q{1},{},core::OccurrenceKey{202}},
            {0,true,false,Q{1},{},core::OccurrenceKey{203}},{0,true,true,Q{1},{},core::OccurrenceKey{204}}}); return b.freeze();
    }
    b.add_grade_one(core::RelationKey{100},0,{{10,true,true,Q{1},{},core::OccurrenceKey{101}}, {20,true,false,Q{1,3},{},core::OccurrenceKey{102}},
        {30,true,false,Q{2,3},{},core::OccurrenceKey{103}}, {20,false,true,Q{0},{},core::OccurrenceKey{104}}, {40,true,false,Q{0},{},core::OccurrenceKey{105}},
        {40,true,false,Q{0},{},core::OccurrenceKey{106}}});
    if(kind == 1) {
        b.add_grade_one(core::RelationKey{200},0,{{10,true,true,Q{1},{},core::OccurrenceKey{201}}, {20,true,false,Q{1,3},{},core::OccurrenceKey{202}},
            {30,true,false,Q{2,3},{},core::OccurrenceKey{203}}, {20,false,true,Q{0},{},core::OccurrenceKey{204}}, {40,true,false,Q{0},{},core::OccurrenceKey{205}}});
        b.add_higher(grade(2),core::RelationKey{300},1,{{0,true,false,Q{2,7},{},core::OccurrenceKey{301}}, {1,true,true,Q{2,7},{},core::OccurrenceKey{302}},
            {0,false,true,Q{0},{},core::OccurrenceKey{303}},{1,true,false,Q{0},{},core::OccurrenceKey{304}}});
    }
    return b.freeze();
}
inline Cylinder cylinder(core::RelationalComplex c) {
    topology::RelationalInterval interval;
    storage::NativeAccessionDirectory a(storage::TemporalEndpoint(std::move(c),11),1101), b(storage::TemporalEndpoint(interval.complex(),13),1102);
    auto request = native_cylinder_example::request(a,b); request.factor_dimension_limit = 128;
    return Cylinder::prepare(std::move(request),available.cylinder);
}
struct SuppliedCarrier { core::RelationalComplex source; topology::CylinderCarrierDeclaration declaration; };
// Example construction only: production certification never creates source IDs.
inline SuppliedCarrier supply(const Cylinder& plan, std::uint64_t identity_base = 5000) {
    const auto cylinder = plan.numeric_plan(); const auto product = cylinder.product(); Guard guard;
    topology::CylinderCarrierDeclaration d{1500,3,{},{},{}};
    std::map<std::uint64_t,std::optional<core::RelationTypeId>> types;
    for(const auto& t:product.declaration().types) {
        const auto output = !t.right ? t.left : std::optional<core::RelationTypeId>{t.left ? 2u : 0u};
        types.emplace(t.key,output); d.types.push_back({t.key,output});
    }
    core::RelationalComplexBuilder b(cylinder.source().signature(),cylinder.source().semantic_scale()); b.declare_grade(product.max_grade());
    std::map<algebra::ProductCell,std::uint32_t> cells;
    topology::cylinder_carrier_detail::each_cell(cylinder,guard,[&](auto id) {
        const auto g = id.grade(); const auto key = identity_base + cells.size();
        std::uint32_t position{};
        if(!g.value)position = b.declare_witness_identity(key);
        else {
            const auto routes = product.retained_boundary(id,guard); std::vector<core::BoundaryOccurrenceInput> low; std::vector<core::HigherIncidenceInput> high;
            for(const auto& r:routes) {
                const bool negative = r.factor_aspects[cylinder.source().signature().orientation_aspect()] != r.koszul_flip;
                const auto magnitude = r.coefficient < 0 ? -r.coefficient : r.coefficient;
                std::vector<core::AspectValue> aspects;
                for(std::size_t a=0;a<r.factor_aspects.size();++a)if(a!=cylinder.source().signature().existence_aspect() && a!=cylinder.source().signature().orientation_aspect())
                    aspects.push_back({static_cast<composite::AspectId>(a),r.factor_aspects[a]});
                const auto occurrence = core::OccurrenceKey{identity_base*10+d.occurrences.size()};
                const auto target_slot = static_cast<std::size_t>(std::count_if(d.occurrences.begin(),d.occurrences.end(),[g](const auto& image){return image.from.upper.grade()==g;}));
                d.occurrences.push_back({{id,r.side,r.factor_occurrence},target_slot});
                if(g.value==1)low.emplace_back(identity_base + static_cast<std::uint64_t>(cells.at(r.lower)),r.exists,negative,magnitude,aspects,occurrence);
                else high.emplace_back(cells.at(r.lower),r.exists,negative,magnitude,aspects,occurrence);
            }
            const auto type = *types.at(product.cell(id).type);
            const auto relation = g.value==1 ? b.add_grade_one(core::RelationKey{key},type,low) : b.add_higher(g,core::RelationKey{key},type,high);
            position = relation.index;
        }
        cells.emplace(id,position); d.cells.push_back({id,position});
    });
    return {b.freeze(),std::move(d)};
}
inline inference::NativeGluingSourceMap source_map(const core::RelationalComplex& a, const core::RelationalComplex& b,
    const algebra::RetainedSourceMap& m, std::uint64_t key, std::uint64_t from, std::uint64_t to) {
    inference::NativeGluingSourceMap out{key,from,to,{},{}};
    for(std::size_t k=0;k<a.grade_count();++k) {
        const auto g=topology::owned_cylinder_detail::grade(k);const auto& x=a.grade(g);const auto& y=b.grade(g);
        for(std::size_t i=0;i<x.cell_count();++i)out.cells.push_back({storage::stable_cell_key(a,{g,static_cast<std::uint32_t>(i)}),storage::stable_cell_key(b,{g,m.cells[k][i]})});
        for(std::size_t p=0;p<x.incidence_count();++p)out.occurrences.push_back({{g,x.occurrence_identities[p]},{g,y.occurrence_identities[m.occurrences[k][p]]}});
    }return out;
}
inline algebra::RetainedSourceMap identity(const core::RelationalComplex& c) {
    algebra::RetainedSourceMap out;
    for(std::size_t k=0;k<c.grade_count();++k) {
        out.cells.emplace_back();out.occurrences.emplace_back();const auto& a=c.grade(topology::owned_cylinder_detail::grade(k));
        for(std::size_t i=0;i<a.cell_count();++i)out.cells[k].push_back(static_cast<std::uint32_t>(i));
        for(std::size_t p=0;p<a.incidence_count();++p)out.occurrences[k].push_back(p);
    }return out;
}
inline inference::NativeRelationalHomotopyRequest request(const Cylinder& plan) {
    auto c=supply(plan);const auto y=supply(plan,9000);Guard guard;
    const auto carrier=topology::OwnedCylinderCarrier::prepare(plan.numeric_plan(),c.source,c.declaration,guard);
    storage::NativeAccessionDirectory ca(storage::TemporalEndpoint(c.source,17),1103),ta(storage::TemporalEndpoint(y.source,19),1104);
    inference::NativeRelationalHomotopyRequest out{1600,4,73,74,native_cylinder_example::anchor(ca),native_cylinder_example::anchor(ta),{1500,3,c.declaration.types,{},{}},{},{},{}};
    const auto product=plan.tensor_product();
    for(const auto& x:c.declaration.cells)out.realization.cells.push_back({product.cell(x.from).source_identity,storage::stable_cell_key(c.source,{x.from.grade(),x.to})});
    for(const auto& x:c.declaration.occurrences)out.realization.occurrences.push_back({product.cell(x.from.upper).source_identity,x.from.side,x.from.occurrence,
        {x.from.upper.grade(),c.source.grade(x.from.upper.grade()).occurrence_identities[x.to]}});
    out.cylinder_target=source_map(c.source,y.source,identity(c.source),1601,73,74);
    out.first=source_map(plan.factor(false).endpoint()->source,y.source,carrier.endpoint(Endpoint::Start),1602,plan.declaration().left_factor,74);
    out.second=source_map(plan.factor(false).endpoint()->source,y.source,carrier.endpoint(Endpoint::Finish),1603,plan.declaration().left_factor,74);
    return out;
}
inline Plan prepare(unsigned kind = 0) { const auto c=cylinder(source(kind));return Plan::prepare(c,request(c),available); }
inline storage::PinnedNativeAccession native_chain(const Cylinder& c,core::Grade g,const Values& v) { return native_cylinder_example::native_chain(c,g,v); }
template<class V> inline bool zero(const V& v) { return native_cylinder_example::zero(v); }
} // namespace native_relational_homotopy_example
