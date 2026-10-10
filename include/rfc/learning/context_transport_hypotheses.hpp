#pragma once
#include "rfc/learning/transport_parameter_relations.hpp"
#include "rfc/algebra/materialize.hpp"
#include "rfc/algebra/owned_gluing.hpp"
#include <map>

namespace rfc::learning {
// An explicit permutation from one original stalk to a declared common frame.
// Frame equality licenses a comparison, never identification of source cells.
struct TransportContextFrame {
    std::uint64_t frame{};
    std::vector<std::size_t> axes;
};
struct TransportContextHypothesis {
    std::size_t origin{}, target{};
    std::vector<std::pair<std::size_t,std::size_t>> cells, occurrences;
    std::vector<std::size_t> equations, evidence_samples;
};
struct TransportContextAcquisition {
    std::vector<std::size_t> seed_rows;
    std::vector<TransportContextHypothesis> hypotheses;
    std::vector<TransportParameterRelation> relations;
    std::map<std::string,std::size_t> rejections;
    std::size_t attempts{}, steps{}, retained{};
};

namespace context_transport_detail {
// Index declared target rows without constructing their Cartesian pair space.
// Rejection counts retain the full scope and its original precedence.
class TargetIndex {
    using Key=std::pair<std::size_t,std::uint64_t>;
    std::map<Key,std::vector<std::size_t>> groups_;
    std::map<std::size_t,std::size_t> counts_;
    std::set<std::size_t> rows_;
    std::vector<std::size_t> empty_;
public:
    template<class Atlas>
    TargetIndex(const core::RelationalComplex& source,const std::vector<std::size_t>& targets,
        const Atlas& atlas,numeric::ExactActionGuard& guard,std::size_t retained=0) {
        guard.dimension(targets.size());const auto& arena=source.grade(core::Grade{1});
        for(const auto target:targets) {
            guard.step();const auto type=arena.relation_types[target];
            groups_[{type,atlas[1][target].frame}].push_back(target);
            ++counts_[type];rows_.insert(target);
            guard.entries(algebra::owned_gluing_detail::add(retained,entries()));
        }
    }
    std::size_t entries() const {
        using namespace algebra::owned_gluing_detail;
        return add(mul(rows_.size(),2),add(mul(groups_.size(),3),mul(counts_.size(),2)));
    }
    const std::vector<std::size_t>& targets(const core::RelationalComplex& source,std::size_t origin,
        std::uint64_t frame,TransportContextAcquisition& out,numeric::ExactActionGuard& guard) const {
        using namespace algebra::owned_gluing_detail;guard.step();
        out.attempts=add(out.attempts,rows_.size());
        const auto type=source.grade(core::Grade{1}).relation_types[origin];
        const auto found=groups_.find({type,frame});
        const auto& selected=frame && found!=groups_.end()?found->second:empty_;
        const auto count=counts_.find(type);const auto same=count==counts_.end()?0:count->second;
        const auto original=rows_.contains(origin)?1u:0u;
        const auto absent=rows_.size()-same;
        const auto undeclared=same-selected.size()-(frame?0:original);
        const auto reject=[&](const char* name,std::size_t n){if(n)out.rejections[name]=add(out.rejections[name],n);};
        reject("original-row",original);reject("primary-type",absent);reject("undeclared-upper-frame",undeclared);
        return selected;
    }
};
// Validate the full ordered star on original cells and retained occurrences.
inline std::string star(const core::RelationalComplex& source,TransportContextHypothesis& hypothesis,
    numeric::ExactActionGuard& guard) {
    using namespace algebra::owned_gluing_detail;
    const core::Grade up{1};const auto& arena=source.grade(up);
    if(hypothesis.origin==hypothesis.target) return "original-row";
    if(arena.relation_types[hypothesis.origin]!=arena.relation_types[hypothesis.target]) return "primary-type";
    const auto [ab,ae]=arena.incidence_range(hypothesis.origin);
    const auto [bb,be]=arena.incidence_range(hypothesis.target);
    if(ae-ab!=be-bb) return "complete-occurrence-count";
    std::map<std::size_t,std::size_t> lowers;std::set<std::size_t> images;
    for(auto p=ab;p<ae;++p) {
        guard.step();const auto q=bb+p-ab;const auto x=arena.lower_index[p],y=arena.lower_index[q];
        const auto [it,inserted]=lowers.emplace(x,y);
        if((!inserted && it->second!=y) || (inserted && !images.insert(y).second)) return "shared-identity-pattern";
        hypothesis.occurrences.emplace_back(p,q);
    }
    algebra::SubcomplexMask a(source),b(source);a.select({up,static_cast<std::uint32_t>(hypothesis.origin)});
    b.select({up,static_cast<std::uint32_t>(hypothesis.target)});a.close_boundaries(source);b.close_boundaries(source);
    const auto left=algebra::materialize_subcomplex(source,a),right=algebra::materialize_subcomplex(source,b);
    guard.entries(add(words(source),add(words(left.complex),words(right.complex))));
    algebra::RetainedSourceMap retained;retained.cells.resize(left.complex.grade_count());
    retained.occurrences.resize(left.complex.grade_count());
    for(const auto cell:left.to_ambient[0]) {guard.step();retained.cells[0].push_back(
        right.from_ambient[0].at(static_cast<std::uint32_t>(lowers.at(cell))));}
    retained.cells[1].push_back(0);
    for(std::size_t i=0;i<ae-ab;++i) {guard.step();retained.occurrences[1].push_back(i);}
    try {algebra::owned_gluing_detail::map(left.complex,right.complex,retained,true,guard);}
    catch(const std::invalid_argument&) {return "typed-retained-map";}
    for(const auto& pair:lowers) hypothesis.cells.push_back(pair);
    return {};
}
} // namespace context_transport_detail

// Discover ordered complete star comparisons from actual acquired observations.
// Each admitted block has corresponding fixed and variable support throughout.
// Its equations concern coefficient-independent transports in declared frames.
inline TransportContextAcquisition acquire_context_transport_hypotheses(
    const PreparedTransportObservations& observations,
    const std::vector<TransportObservation>& samples,
    const std::vector<std::size_t>& targets,
    const std::vector<std::vector<TransportContextFrame>>& atlas,
    numeric::ExactActionGuard& guard) {
    using namespace algebra::owned_gluing_detail;
    observations.require_grade_one_profile();
    const auto start=guard.steps();guard.step();
    const auto& fibers=observations.owner().fibers();const auto& source=fibers.complex();
    const core::Grade lo{0},up{1};const auto& arena=source.grade(up);
    const auto offsets=fibers.stalks().offsets(up);
    TransportContextAcquisition out;
    out.retained=add(observations.retained_entries(),words(source));
    const auto admit=[&](std::size_t scratch=0) {guard.entries(add(out.retained,scratch));};
    admit();
    if(atlas.size()!=2 || atlas[0].size()!=source.grade(lo).cell_count() || atlas[1].size()!=arena.cell_count())
        throw std::invalid_argument("context atlas must cover both original grades");
    std::map<std::uint64_t,std::size_t> frame_dimensions;
    for(std::size_t k=0;k<2;++k) for(std::size_t i=0;i<atlas[k].size();++i) {
        guard.step();const auto& chart=atlas[k][i];
        out.retained=add(out.retained,add(2,chart.axes.size()));admit();
        if(!chart.frame) {
            if(!chart.axes.empty()) throw std::invalid_argument("absent context frame has coordinates");
            continue;
        }
        const auto n=fibers.stalks().dimension({grade(k),static_cast<std::uint32_t>(i)});
        if(chart.axes.size()!=n) throw std::invalid_argument("complete original stalk frame required");
        std::set<std::size_t> axes;
        for(const auto axis:chart.axes) {guard.step();if(axis>=n || !axes.insert(axis).second)
            throw std::invalid_argument("context frame must be a complete permutation");}
        const auto [it,inserted]=frame_dimensions.emplace(chart.frame,n);
        if(!inserted && it->second!=n) throw std::invalid_argument("declared common frame has conflicting dimensions");
    }
    std::set<std::size_t> distinct;
    for(const auto target:targets) {guard.step();if(target>=arena.cell_count() || !distinct.insert(target).second)
        throw std::invalid_argument("distinct original target relation rows required");}
    std::vector<std::vector<std::size_t>> evidence(arena.cell_count());
    for(std::size_t s=0;s<samples.size();++s) {
        guard.step();const auto& sample=samples[s];guard.vector(sample.input);guard.vector(sample.target);
        if(sample.input.size()!=offsets.back() || sample.observed.size()!=source.grade(lo).cell_count() ||
           sample.target.size()!=fibers.stalks().total_dimension(lo))
            throw std::invalid_argument("complete contextual observation fields required");
        if(std::any_of(sample.observed.begin(),sample.observed.end(),[](auto bit){return bit>1;}))
            throw std::invalid_argument("context observation mask must be Boolean");
        out.retained=add(out.retained,add(sample.input.size(),add(sample.target.size(),sample.observed.size())));admit();
        for(std::size_t row=0;row<arena.cell_count();++row) {
            bool input=false,observed=false;
            for(auto j=offsets[row];j<offsets[row+1];++j) {guard.step();input=input || sample.input[j]!=0;}
            if(!input) continue;
            const auto [begin,end]=arena.incidence_range(row);
            for(auto p=begin;p<end;++p) {guard.step();observed=observed ||
                (sample.observed[arena.lower_index[p]] && arena.aspects.get(source.signature().existence_aspect(),p) &&
                 coefficient(source,up,p,guard)!=0);}
            if(observed) {evidence[row].push_back(s);out.retained=add(out.retained,1);admit();}
        }
    }
    for(std::size_t row=0;row<evidence.size();++row) if(!evidence[row].empty()) out.seed_rows.push_back(row);
    guard.dimension(out.seed_rows.size());
    const context_transport_detail::TargetIndex index(source,targets,atlas,guard,out.retained);
    out.retained=add(out.retained,index.entries());admit();
    using Key=std::tuple<std::size_t,std::size_t,std::size_t>;
    std::map<Key,std::size_t> variables;std::set<Key> fixed;
    for(std::size_t i=0;i<observations.variable_coordinates().size();++i) {
        guard.step();const auto& c=observations.variable_coordinates()[i];variables.emplace(Key{c.occurrence,c.row,c.column},i);
    }
    for(const auto& c:observations.fixed_coordinates()) {guard.step();fixed.emplace(c.occurrence,c.row,c.column);}
    const auto scratch=add(mul(variables.size(),4),mul(fixed.size(),3));admit(scratch);
    const auto compare_frame=[&](std::size_t k,std::size_t a,std::size_t b) {
        const auto& x=atlas[k][a];const auto& y=atlas[k][b];guard.step();
        return x.frame && x.frame==y.frame;
    };
    for(const auto origin:out.seed_rows) for(const auto target:index.targets(source,origin,atlas[1][origin].frame,out,guard)) {
        if(origin==target)continue;
        guard.step();std::string failure;
        const auto reject=[&](const char* reason){failure=reason;};
        TransportContextHypothesis hypothesis;hypothesis.origin=origin;hypothesis.target=target;
        const auto [ab,ae]=arena.incidence_range(origin);const auto [bb,be]=arena.incidence_range(target);
        if(failure.empty() && ae-ab!=be-bb) reject("complete-occurrence-count");
        std::map<std::size_t,std::size_t> lowers;std::set<std::size_t> images;
        if(failure.empty()) for(auto p=ab;p<ae;++p) {
            guard.step();const auto q=bb+p-ab;const auto x=arena.lower_index[p],y=arena.lower_index[q];
            const auto [it,inserted]=lowers.emplace(x,y);
            if((!inserted && it->second!=y) || (inserted && !images.insert(y).second)) {reject("shared-identity-pattern");break;}
            if(!compare_frame(0,x,y)) {reject("undeclared-lower-frame");break;}
            hypothesis.occurrences.emplace_back(p,q);
        }
        if(failure.empty()) {
            algebra::SubcomplexMask a(source),b(source);a.select({up,static_cast<std::uint32_t>(origin)});
            b.select({up,static_cast<std::uint32_t>(target)});a.close_boundaries(source);b.close_boundaries(source);
            const auto left=algebra::materialize_subcomplex(source,a),right=algebra::materialize_subcomplex(source,b);
            admit(add(scratch,add(words(left.complex),words(right.complex))));
            algebra::RetainedSourceMap map;map.cells.resize(left.complex.grade_count());map.occurrences.resize(left.complex.grade_count());
            for(const auto cell:left.to_ambient[0]) {guard.step();map.cells[0].push_back(right.from_ambient[0].at(static_cast<std::uint32_t>(lowers.at(cell))));}
            map.cells[1].push_back(0);
            for(std::size_t i=0;i<ae-ab;++i) {guard.step();map.occurrences[1].push_back(i);}
            try {algebra::owned_gluing_detail::map(left.complex,right.complex,map,true,guard);}
            catch(const std::invalid_argument&) {reject("typed-retained-map");}
        }
        std::vector<TransportParameterRelation> equations;
        if(failure.empty()) for(const auto& [p,q]:hypothesis.occurrences) {
            const auto x=arena.lower_index[p],y=arena.lower_index[q];
            const auto& xframe=atlas[0][x];const auto& yframe=atlas[0][y];
            const auto& aframe=atlas[1][origin];const auto& bframe=atlas[1][target];
            std::vector<std::size_t> lower_inverse(yframe.axes.size()),upper_inverse(bframe.axes.size());
            for(std::size_t i=0;i<yframe.axes.size();++i) {guard.step();lower_inverse[yframe.axes[i]]=i;}
            for(std::size_t i=0;i<bframe.axes.size();++i) {guard.step();upper_inverse[bframe.axes[i]]=i;}
            for(std::size_t i=0;i<xframe.axes.size() && failure.empty();++i)
                for(std::size_t j=0;j<aframe.axes.size();++j) {
                    guard.step();const Key a{p,i,j},b{q,lower_inverse[xframe.axes[i]],upper_inverse[aframe.axes[j]]};
                    const auto av=variables.find(a),bv=variables.find(b);const bool ax=av!=variables.end(),bx=bv!=variables.end();
                    if(ax!=bx || fixed.contains(a)!=fixed.contains(b)) {reject("complete-map-support");break;}
                    if(ax) {
                        const auto name="context:"+std::to_string(origin)+":"+std::to_string(target)+":"+
                            std::to_string(p)+":"+std::to_string(i)+":"+std::to_string(j);
                        equations.push_back({name,{{av->second,Q{1}},{bv->second,Q{-1}}},Q{0}});
                        guard.dimension(add(out.relations.size(),equations.size()));
                        admit(add(scratch,mul(equations.size(),128)));
                    } else if(fixed.contains(a)) {
                        const auto& amap=*fibers.transport_override(up,p);const auto& bmap=*fibers.transport_override(up,q);
                        if(amap.get(i,j)!=bmap.get(std::get<1>(b),std::get<2>(b))) {reject("declared-fixed-map");break;}
                    }
                }
            if(!failure.empty()) break;
        }
        if(failure.empty() && equations.empty()) reject("no-variable-map");
        if(!failure.empty()) {++out.rejections[failure];continue;}
        for(const auto& pair:lowers) hypothesis.cells.push_back(pair);
        hypothesis.evidence_samples=evidence[origin];
        out.retained=add(out.retained,add(8,add(mul(hypothesis.cells.size(),2),
            add(mul(hypothesis.occurrences.size(),2),hypothesis.evidence_samples.size()))));
        for(auto& equation:equations) {
            guard.step();hypothesis.equations.push_back(out.relations.size());
            out.retained=add(out.retained,add(8,equation.name.size()));out.relations.push_back(std::move(equation));admit(scratch);
        }
        out.hypotheses.push_back(std::move(hypothesis));admit(scratch);
    }
    out.steps=guard.steps()-start;return out;
}
} // namespace rfc::learning
