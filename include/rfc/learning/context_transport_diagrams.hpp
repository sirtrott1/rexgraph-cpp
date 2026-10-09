#pragma once
#include "rfc/learning/context_transport_views.hpp"

namespace rfc::learning {
struct TransportContextDiagram {
    std::size_t patch{};
    std::vector<std::size_t> blocks, equations;
    std::vector<std::pair<std::size_t,std::size_t>> rows, cells, occurrences, shared_cells;
};
struct TransportContextDiagramAcquisition {
    TransportContextViewAcquisition stars;
    std::vector<TransportContextDiagram> diagrams;
    std::map<std::string,std::size_t> rejections;
    std::size_t attempts{}, steps{}, retained{};
};
namespace context_diagram_detail {
using namespace algebra::owned_gluing_detail;

// Connectivity uses retained lower identities, including inactive occurrences.
inline bool connected(const core::RelationalComplex& source,const std::vector<std::size_t>& rows,Guard& guard) {
    const auto& arena=source.grade(core::Grade{1});
    std::set<std::size_t> reached, cells;
    while(reached.size()!=rows.size()) {
        const auto before=reached.size();
        for(const auto row:rows) {
            guard.step();if(reached.contains(row)) continue;
            const auto [begin,end]=arena.incidence_range(row);bool touches=reached.empty();
            for(auto p=begin;p<end;++p) {guard.step();touches=touches || cells.contains(arena.lower_index[p]);}
            if(!touches) continue;
            reached.insert(row);
            for(auto p=begin;p<end;++p) {guard.step();cells.insert(arena.lower_index[p]);}
        }
        if(reached.size()==before) return false;
    }
    return true;
}

// Validate the complete union map using the canonical retained source morphism.
inline void retained_map(const core::RelationalComplex& source,const TransportContextDiagram& diagram,Guard& guard,
    std::size_t resident) {
    const core::Grade up{1};const auto& arena=source.grade(up);
    algebra::SubcomplexMask a(source),b(source);
    for(const auto& [x,y]:diagram.rows) {guard.step();a.select({up,static_cast<std::uint32_t>(x)});b.select({up,static_cast<std::uint32_t>(y)});}
    a.close_boundaries(source);b.close_boundaries(source);
    const auto left=algebra::materialize_subcomplex(source,a),right=algebra::materialize_subcomplex(source,b);
    guard.entries(add(resident,add(words(left.complex),words(right.complex))));
    std::map<std::size_t,std::size_t> lower(diagram.cells.begin(),diagram.cells.end()),upper(diagram.rows.begin(),diagram.rows.end());
    std::map<std::size_t,std::size_t> flags(diagram.occurrences.begin(),diagram.occurrences.end()),local_flags;
    std::size_t index{};
    for(const auto row:right.to_ambient[1]) {
        const auto [begin,end]=arena.incidence_range(row);
        for(auto p=begin;p<end;++p) {guard.step();local_flags.emplace(p,index++);}
    }
    algebra::RetainedSourceMap map;map.cells.resize(source.grade_count());map.occurrences.resize(source.grade_count());
    for(const auto cell:left.to_ambient[0]) {guard.step();map.cells[0].push_back(right.from_ambient[0].at(lower.at(cell)));}
    for(const auto row:left.to_ambient[1]) {
        guard.step();map.cells[1].push_back(right.from_ambient[1].at(upper.at(row)));
        const auto [begin,end]=arena.incidence_range(row);
        for(auto p=begin;p<end;++p) {guard.step();map.occurrences[1].push_back(local_flags.at(flags.at(p)));}
    }
    guard.entries(add(resident,add(words(left.complex),add(words(right.complex),mul(add(diagram.cells.size(),
        add(diagram.rows.size(),diagram.occurrences.size())),12)))));
    algebra::owned_gluing_detail::map(left.complex,right.complex,map,true,guard);
}
} // namespace context_diagram_detail

// Glue observed star correspondences on explicitly selected connected patches.
// Shared identities agree in both directions; flag contributions remain distinct.
inline TransportContextDiagramAcquisition acquire_context_transport_diagrams(
    const PreparedTransportObservations& observations,const std::vector<TransportObservation>& samples,
    const std::vector<std::vector<std::size_t>>& patches,const std::vector<std::vector<TransportContextView>>& atlas,
    numeric::ExactActionGuard& guard) {
    using namespace context_diagram_detail;
    const auto start=guard.steps();guard.step();
    const auto& source=observations.owner().fibers().complex();const auto& arena=source.grade(core::Grade{1});
    std::vector<std::size_t> targets;std::set<std::vector<std::size_t>> unique;
    std::size_t declaration{};guard.dimension(patches.size());
    for(const auto& patch:patches) {
        guard.step();guard.dimension(patch.size());
        if(patch.size()<2) throw std::invalid_argument("context diagram requires at least two original rows");
        std::set<std::size_t> rows;
        for(const auto row:patch) {
            guard.step();if(row>=arena.cell_count() || !rows.insert(row).second)
                throw std::invalid_argument("distinct original diagram rows required");
            if(std::find(targets.begin(),targets.end(),row)==targets.end()) targets.push_back(row);
        }
        if(!unique.insert(std::vector<std::size_t>(rows.begin(),rows.end())).second)
            throw std::invalid_argument("duplicate original context patch");
        declaration=add(declaration,add(4,mul(patch.size(),4)));guard.entries(add(words(source),declaration));
        if(!connected(source,patch,guard)) throw std::invalid_argument("context patch must be connected through retained boundaries");
    }
    TransportContextDiagramAcquisition out;
    out.stars=acquire_context_transport_views(observations,samples,targets,atlas,guard);
    out.retained=add(out.stars.acquisition.retained,declaration);
    const auto admit=[&](std::size_t scratch=0){guard.entries(add(out.retained,scratch));};admit();
    std::map<std::size_t,std::vector<std::size_t>> by_target;
    const auto& stars=out.stars.acquisition.hypotheses;
    for(std::size_t i=0;i<stars.size();++i) {guard.step();by_target[stars[i].target].push_back(i);out.retained=add(out.retained,2);admit();}
    for(std::size_t patch_index=0;patch_index<patches.size();++patch_index) {
        const auto& patch=patches[patch_index];bool missing=false;
        for(const auto row:patch) {guard.step();missing=missing || !by_target.contains(row);}
        if(missing) {++out.rejections["no-observed-star-cover"];continue;}
        // Iterative depth-first enumeration avoids a recursive stack bound.
        std::vector<std::size_t> chosen,cursors(patch.size());
        std::size_t flag_count{};
        for(const auto row:patch) {const auto [begin,end]=arena.incidence_range(row);flag_count=add(flag_count,end-begin);}
        const auto scratch=add(mul(patch.size(),12),mul(flag_count,24));admit(scratch);
        std::size_t depth{};
        while(true) {
            guard.step();const auto& options=by_target.at(patch[depth]);
            if(cursors[depth]==options.size()) {
                cursors[depth]=0;if(!depth) break;--depth;chosen.pop_back();continue;
            }
            const auto next=options[cursors[depth]++];++out.attempts;
            std::map<std::size_t,std::size_t> lowers,inverse;std::set<std::size_t> origins;
            std::string failure;
            const auto extend=[&](std::size_t block) {
                const auto& h=stars[block];guard.step();
                if(!origins.insert(h.origin).second) {failure="shared-upper-identity";return;}
                for(const auto& [x,y]:h.cells) {
                    guard.step();const auto [l,inserted]=lowers.emplace(x,y);const auto [r,added]=inverse.emplace(y,x);
                    if((!inserted && l->second!=y) || (!added && r->second!=x)) {failure="shared-boundary-identity";return;}
                }
            };
            for(const auto block:chosen) {extend(block);if(!failure.empty()) break;}
            if(failure.empty()) extend(next);
            if(!failure.empty()) {++out.rejections[failure];continue;}
            chosen.push_back(next);
            if(chosen.size()!=patch.size()) {++depth;continue;}
            TransportContextDiagram diagram;diagram.patch=patch_index;diagram.blocks=chosen;
            std::map<std::size_t,std::set<std::size_t>> owners;std::set<std::size_t> equations;
            for(const auto block:chosen) {
                const auto& h=stars[block];diagram.rows.emplace_back(h.origin,h.target);
                for(const auto& pair:h.occurrences) {guard.step();diagram.occurrences.push_back(pair);}
                for(const auto& [x,y]:h.cells) {guard.step();(void)y;owners[x].insert(h.origin);}
                for(const auto equation:h.equations) {guard.step();equations.insert(equation);}
            }
            for(const auto& pair:lowers) {guard.step();diagram.cells.push_back(pair);if(owners.at(pair.first).size()>1) diagram.shared_cells.push_back(pair);}
            diagram.equations.assign(equations.begin(),equations.end());
            std::sort(diagram.rows.begin(),diagram.rows.end());std::sort(diagram.occurrences.begin(),diagram.occurrences.end());
            admit(add(scratch,mul(diagram.equations.size(),4)));
            retained_map(source,diagram,guard,add(out.retained,scratch));
            out.retained=add(out.retained,add(8,add(diagram.blocks.size(),add(diagram.equations.size(),mul(2,
                add(diagram.rows.size(),add(diagram.cells.size(),add(diagram.occurrences.size(),diagram.shared_cells.size()))))))));
            guard.dimension(add(out.diagrams.size(),1));admit(scratch);out.diagrams.push_back(std::move(diagram));chosen.pop_back();
        }
    }
    out.steps=guard.steps()-start;return out;
}
} // namespace rfc::learning
