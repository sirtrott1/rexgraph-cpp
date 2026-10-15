#pragma once
#include "rfc/inference/native_covering.hpp"
#include "../native_cylinder/fixture.hpp"

namespace native_covering_example {
using namespace rfc;
using Q=numeric::Rational;using Guard=numeric::ExactActionGuard;
using Plan=inference::PreparedNativeCovering;using Square=inference::PreparedNativeCoveringPullback;
using Values=Plan::Values;using Channel=local::CoveringChannel;
inline core::Grade grade(std::uint16_t k){return core::Grade{k};}
inline constexpr inference::NativeCoveringAvailability available{{940,31},{941,37}},pulled_available{{942,41},{943,43}};
inline constexpr inference::NativeCoveringPullbackAvailability square_available{available,pulled_available};
inline storage::PinnedNativeAccession anchor(const storage::NativeAccessionDirectory& d){return d.pin(storage::NativeAccessionRequest::sparse_chain(grade(0),{},{}));}
inline core::RelationalComplex cycle(std::uint32_t n) {
    core::RelationSignature s;const auto edge=s.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);core::RelationalComplexBuilder b(s);b.declare_grade(grade(3));
    for(std::uint32_t i=0;i<n;++i)b.declare_witness_identity(10+i);
    for(std::uint32_t i=0;i<n;++i)b.add_grade_one(core::RelationKey{100+i},edge,{
        {10+i,true,true,Q{1},{},core::OccurrenceKey{1000+2*i}},
        {10+(i+1)%n,true,false,Q{1},{},core::OccurrenceKey{1001+2*i}}});
    return b.freeze();
}
inline core::RelationalComplex repeated() {
    core::RelationSignature s;s.add_type("loop-flags");core::RelationalComplexBuilder b(s);b.declare_grade(grade(3));
    b.add_grade_one(core::RelationKey{100},0,{{10,true,true,Q{1},{},core::OccurrenceKey{101}},
        {10,true,false,Q{1},{},core::OccurrenceKey{102}},{10,true,true,Q{1},{},core::OccurrenceKey{103}},
        {10,true,false,Q{1},{},core::OccurrenceKey{104}},{10,false,true,Q{0},{},core::OccurrenceKey{105}},
        {10,true,false,Q{0},{},core::OccurrenceKey{106}}});return b.freeze();
}
inline core::RelationalComplex copies(const core::RelationalComplex& c,std::size_t sheets) {
    core::RelationalComplexBuilder b(c.signature(),c.semantic_scale());b.declare_grade(c.max_grade());
    for(std::size_t s=0;s<sheets;++s)for(const auto id:c.grade(grade(0)).identities)b.declare_witness_identity(id+s*10000+1000);
    for(std::uint16_t k=1;k<c.grade_count();++k){const auto& a=c.grade(grade(k));const auto lower_count=c.grade(grade(k-1)).cell_count();
        for(std::size_t s=0;s<sheets;++s)for(std::size_t i=0;i<a.cell_count();++i){
            std::vector<core::BoundaryOccurrenceInput> low;std::vector<core::HigherIncidenceInput> high;const auto [begin,end]=a.incidence_range(i);
            for(auto p=begin;p<end;++p){
                const auto exists=a.aspects.get(c.signature().existence_aspect(),p),negative=a.aspects.get(c.signature().orientation_aspect(),p);
                auto q=c.incidence_coefficient(grade(k),p);if(q<0)q=-q;std::vector<core::AspectValue> aspects;
                for(std::size_t j=0;j<c.signature().aspects().size();++j){const auto id=static_cast<composite::AspectId>(j);if(id!=c.signature().existence_aspect() && id!=c.signature().orientation_aspect())aspects.push_back({id,a.aspects.get(id,p)});}
                const core::OccurrenceKey occurrence{a.occurrence_identities[p].value+s*100000+10000};
                if(k==1)low.push_back({c.grade(grade(0)).identities[a.lower_index[p]]+s*10000+1000,exists,negative,q,aspects,occurrence});
                else high.push_back({static_cast<std::uint32_t>(s*lower_count+a.lower_index[p]),exists,negative,q,aspects,occurrence});
            }
            const core::RelationKey key{a.relation_identities[i].value+s*10000+1000};
            if(k==1)b.add_grade_one(key,a.relation_types[i],low);else b.add_higher(grade(k),key,a.relation_types[i],high);
        }
    }
    return b.freeze();
}
inline algebra::RetainedSourceMap projection(const core::RelationalComplex& source,const core::RelationalComplex& base) {
    algebra::RetainedSourceMap m;
    for(std::uint16_t k=0;k<source.grade_count();++k){m.cells.emplace_back();m.occurrences.emplace_back();
        for(std::size_t i=0;i<source.grade(grade(k)).cell_count();++i)m.cells[k].push_back(static_cast<std::uint32_t>(i%base.grade(grade(k)).cell_count()));
        for(std::size_t p=0;p<source.grade(grade(k)).incidence_count();++p)m.occurrences[k].push_back(p%base.grade(grade(k)).incidence_count());}
    return m;
}
inline inference::NativeGluingSourceMap named_map(const core::RelationalComplex& from,const core::RelationalComplex& to,
    const algebra::RetainedSourceMap& m,std::uint64_t from_key,std::uint64_t to_key,std::uint64_t key=8000) {
    inference::NativeGluingSourceMap out{key,from_key,to_key,{},{}};
    for(std::uint16_t k=0;k<from.grade_count();++k){const auto g=grade(k);const auto& a=from.grade(g);
        for(std::size_t i=0;i<a.cell_count();++i)out.cells.push_back({storage::stable_cell_key(from,{g,static_cast<std::uint32_t>(i)}),storage::stable_cell_key(to,{g,m.cells[k][i]})});
        for(std::size_t p=0;p<a.incidence_count();++p)out.occurrences.push_back({{g,a.occurrence_identities[p]},{g,to.occurrence_identity(g,m.occurrences[k][p])}});}
    return out;
}
inline Plan prepare(core::RelationalComplex source=cycle(6),core::RelationalComplex base=cycle(3),bool pulled=false) {
    const auto a=pulled?pulled_available:available;storage::NativeAccessionDirectory left(storage::TemporalEndpoint(source,a.cover.max_sequence),a.cover.source_namespace);
    storage::NativeAccessionDirectory right(storage::TemporalEndpoint(base,a.base.max_sequence),a.base.source_namespace);
    const auto map=named_map(source,base,projection(source,base),pulled?121u:101u,pulled?122u:102u);
    return Plan::prepare({anchor(left),anchor(right),pulled?1200u:1100u,7,map,128},a);
}
inline storage::PinnedNativeAccession native_chain(const Plan& p,Channel channel,core::Grade g,const Values& values) {
    const auto pin=p.anchor(channel);std::vector<std::uint64_t> support;std::vector<storage::NativeCoefficient> coefficients;
    for(const auto& [i,q]:values){const auto key=storage::stable_cell_key(pin.endpoint()->source,{g,i}).key;support.push_back(key);if(q!=0)coefficients.push_back({key,q});}
    return pin.access(storage::NativeAccessionRequest::sparse_chain(g,std::move(support),std::move(coefficients)));
}
inline inference::NativeCoveringPullbackRequest square_request(const Plan& original,const Plan& pulled) {
    const auto a=original.numeric_plan(),b=pulled.numeric_plan();
    return {9000,3,named_map(b.source(),a.source(),projection(b.source(),a.source()),pulled.declaration().projection.from,original.declaration().projection.from,9001),
        named_map(b.source(Channel::Base),a.source(Channel::Base),projection(b.source(Channel::Base),a.source(Channel::Base)),pulled.declaration().projection.to,original.declaration().projection.to,9002)};
}
inline inference::NativeCoveringCone cone(const Square& square) {
    const auto p=square.pulled(),q=square.original();const auto a=p.numeric_plan(),b=q.numeric_plan();
    const auto numeric=square.numeric_plan();
    return {p.anchor(Channel::Cover),9900,named_map(a.source(),b.source(),numeric.map(),9900,q.declaration().projection.from,9901),
        named_map(a.source(),a.source(Channel::Base),a.projection(),9900,p.declaration().projection.to,9902)};
}
inline inference::NativeCoveringWalkRequest flag_walk(const Plan& p,core::CellId start,std::vector<topology::CoveringFlagStep> steps) {
    const auto pin=p.anchor(Channel::Base);const auto& c=pin.endpoint()->source;inference::NativeCoveringWalkRequest r{storage::stable_cell_key(c,start),{}};
    for(const auto& s:steps)r.steps.push_back({{s.upper_grade,c.occurrence_identity(s.upper_grade,s.occurrence)},s.upward});
    return r;
}
inline std::vector<inference::NativeCoveringEdgeStep> loop_steps(const Plan& p,unsigned repeats=1) {
    const auto pin=p.anchor(Channel::Base);const auto& c=pin.endpoint()->source;std::vector<inference::NativeCoveringEdgeStep> out;
    for(unsigned r=0;r<repeats;++r)for(std::uint32_t i=0;i<c.grade(grade(1)).cell_count();++i)out.push_back({storage::stable_cell_key(c,{grade(1),i}),true});
    return out;
}
inline inference::NativeGluingSourceMap deck_request(const Plan& p,std::size_t shift) {
    const auto n=p.numeric_plan();auto m=projection(n.source(),n.source());
    for(std::size_t k=0;k<m.cells.size();++k)for(std::size_t i=0;i<m.cells[k].size();++i)m.cells[k][i]=static_cast<std::uint32_t>((i+shift)%m.cells[k].size());
    for(std::size_t i=0;i<m.occurrences[1].size();++i)m.occurrences[1][i]=((i/2+shift)%m.cells[1].size())*2+i%2;
    return named_map(n.source(),n.source(),m,p.declaration().projection.from,p.declaration().projection.from,9100);
}
template<class V>inline bool zero(const V& v){for(const auto& [i,q]:v){(void)i;if(q!=0)return false;}return true;}
} // namespace native_covering_example
