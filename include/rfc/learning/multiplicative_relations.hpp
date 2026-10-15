#pragma once

#include "rfc/local/fiber_system.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <map>
#include <memory>
#include <optional>

namespace rfc::learning {
struct RetainedRatio {
    core::RelationKey relation;
    numeric::Rational value{1};
};
struct MultiplicativeComponent {
    core::CellId root;
    std::vector<core::CellId> cells;
    bool nonzero_solution{true};
};
struct MultiplicativeCycle {
    core::RelationId relation;
    core::OccurrenceKey tail_occurrence,head_occurrence;
    std::size_t component{};
    numeric::Rational holonomy{1};
    bool closes{};
};
struct MultiplicativeWalkStep {core::RelationId relation;bool forward{};};

// Companion Proposition 29.3 over Q: x_head = ratio * x_tail. The source is
// retained in full, including zero-amplitude flags and any higher relations.
// Every grade-one relation must have exactly one -1 and one +1 active flag.
// Higher twisted chain closure is a separate check, never inferred from a
// name such as "local system". Inconsistent connected components admit zero
// only; consistent components each retain one exact free amplitude.
class PreparedMultiplicativeRelations {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct Edge {std::size_t tail{},head{},tail_slot{},head_slot{};Q ratio{1};};
    struct Parent {std::size_t cell{},edge{};bool forward{};};
    struct State {
        std::shared_ptr<const core::RelationalComplex> source;
        local::LinearRelationalFiberSystem fibers;
        std::vector<Edge> edges;
        std::vector<MultiplicativeComponent> components;
        std::vector<MultiplicativeCycle> cycles;
        std::vector<std::optional<Parent>> parents;
        std::vector<std::size_t> component;
        V transport;
        M solution_frame;
        State(std::shared_ptr<const core::RelationalComplex> owner,std::vector<RetainedRatio> ratios,Guard& guard)
            :source(require_source(std::move(owner))),fibers(local::LinearStalkBundle(*source,1)) {
            const auto n=source->grade(core::Grade{0}).cell_count();guard.dimension(n);
            const auto count=source->grade_count()>1?source->grade(core::Grade{1}).cell_count():0;
            guard.dimension(count);
            if(ratios.size()!=count)throw std::invalid_argument("one ratio for every retained grade-one relation required");
            std::map<core::RelationKey,Q> declared;
            for(auto& ratio:ratios){
                guard.step();ratio.value=numeric::canonical_rational(ratio.value);guard.value(ratio.value);
                if(ratio.value==0 || !declared.emplace(ratio.relation,ratio.value).second)
                    throw std::invalid_argument("multiplicative ratios must be nonzero with unique relation identities");
            }
            // Every original flag obtains its declared scalar realization;
            // solving the equations does not rewrite any source incidence.
            for(std::size_t k=1;k<source->grade_count();++k){
                const core::Grade grade{static_cast<std::uint16_t>(k)};
                for(std::size_t p=0;p<source->grade(grade).incidence_count();++p){
                    guard.step();M map(1,1);map.set(0,0,source->incidence_coefficient(grade,p));
                    fibers.set_transport(grade,p,std::move(map));
                }
            }
            if(count){
                const auto& arena=source->grade(core::Grade{1});
                for(std::size_t e=0;e<count;++e){
                    guard.step();const auto ratio=declared.find(arena.relation_identities.at(e));
                    if(ratio==declared.end())throw std::invalid_argument("ratio belongs to another retained relation");
                    std::optional<std::size_t> tail,head;const auto [begin,end]=arena.incidence_range(e);
                    for(auto p=begin;p<end;++p){
                        guard.step();const auto q=source->incidence_coefficient(core::Grade{1},p);
                        if(q==0)continue;
                        if(q==-1 && !tail)tail=p;else if(q==1 && !head)head=p;
                        else throw std::invalid_argument("multiplicative relation requires its declared signed pair flags");
                    }
                    if(!tail || !head)throw std::invalid_argument("multiplicative relation has incomplete signed pair");
                    edges.push_back({arena.lower_index[*tail],arena.lower_index[*head],*tail,*head,ratio->second});
                    M twisted_head(1,1);twisted_head.set(0,0,guard.div(1,ratio->second));
                    fibers.set_transport(core::Grade{1},*head,std::move(twisted_head));
                }
            }
            struct Route{std::size_t neighbor{},edge{};bool forward{};};
            std::vector<std::vector<Route>> adjacent(n);std::vector<bool> tree(count),seen(n);
            for(std::size_t e=0;e<edges.size();++e){
                adjacent[edges[e].tail].push_back({edges[e].head,e,true});
                adjacent[edges[e].head].push_back({edges[e].tail,e,false});
            }
            parents.resize(n);component.resize(n);transport=V(n);
            guard.entries(machine::checked_size_add(machine::checked_size_mul(n,7),machine::checked_size_mul(count,9)));
            for(std::size_t root=0;root<n;++root)if(!seen[root]){
                guard.step();const auto ci=components.size();components.push_back({{core::Grade{0},static_cast<std::uint32_t>(root)},{},true});
                std::vector<std::size_t> queue{root};seen[root]=true;transport[root]=1;component[root]=ci;
                for(std::size_t at=0;at<queue.size();++at){
                    const auto v=queue[at];components.back().cells.push_back({core::Grade{0},static_cast<std::uint32_t>(v)});
                    for(const auto route:adjacent[v]){
                        guard.step();if(seen[route.neighbor])continue;
                        seen[route.neighbor]=true;component[route.neighbor]=ci;tree[route.edge]=true;
                        parents[route.neighbor]=Parent{v,route.edge,route.forward};
                        transport[route.neighbor]=route.forward?guard.mul(transport[v],edges[route.edge].ratio)
                            :guard.div(transport[v],edges[route.edge].ratio);
                        queue.push_back(route.neighbor);
                    }
                }
            }
            for(std::size_t e=0;e<count;++e)if(!tree[e]){
                const auto& edge=edges[e];const auto h=guard.div(guard.mul(transport[edge.tail],edge.ratio),transport[edge.head]);
                const auto& arena=source->grade(core::Grade{1});
                cycles.push_back({{core::Grade{1},static_cast<std::uint32_t>(e)},arena.occurrence_identities.at(edge.tail_slot),
                    arena.occurrence_identities.at(edge.head_slot),component[edge.tail],h,h==1});
                if(h!=1)components[component[edge.tail]].nonzero_solution=false;
            }
            std::size_t rank{};for(const auto& c:components)if(c.nonzero_solution)++rank;
            solution_frame=M(n,rank);std::size_t column{};
            for(const auto& c:components)if(c.nonzero_solution){
                for(const auto cell:c.cells){guard.step();solution_frame.set(cell.index,column,transport[cell.index]);}
                ++column;
            }
            guard.matrix(solution_frame);
            for(const auto& edge:edges){
                if(components[component[edge.tail]].nonzero_solution &&
                   guard.sub(transport[edge.head],guard.mul(edge.ratio,transport[edge.tail]))!=0)
                    throw std::logic_error("multiplicative solution family fails an original relation");
            }
        }
        static std::shared_ptr<const core::RelationalComplex> require_source(std::shared_ptr<const core::RelationalComplex> source){
            if(!source)throw std::invalid_argument("multiplicative equations require a retained source owner");
            return source;
        }
    };
    std::shared_ptr<const State> state_;
    const State& state()const{if(!state_)throw std::logic_error("moved-from multiplicative relation owner");return *state_;}
public:
    PreparedMultiplicativeRelations(std::shared_ptr<const core::RelationalComplex> source,std::vector<RetainedRatio> ratios,Guard& guard)
        :state_(std::make_shared<const State>(std::move(source),std::move(ratios),guard)){}
    std::shared_ptr<const core::RelationalComplex> source()const{return state().source;}
    const local::LinearRelationalFiberSystem& fibers()const &{return state().fibers;}
    const local::LinearRelationalFiberSystem& fibers()const &&=delete;
    const auto& components()const &{return state().components;}
    const auto& cycles()const &{return state().cycles;}
    const M& solution_frame()const &{return state().solution_frame;}
    const auto& components()const &&=delete;
    const auto& cycles()const &&=delete;
    const M& solution_frame()const &&=delete;
    std::size_t free_amplitudes()const{return state().solution_frame.cols();}
    V solve(const V& amplitudes,Guard& guard)const{return numeric::checked_apply(state().solution_frame,amplitudes,guard);}
    V residual(const V& values,Guard& guard)const{
        if(values.size()!=state().transport.size())throw std::invalid_argument("multiplicative input carrier mismatch");
        const auto input=guard.canonical_input(values);V out;out.reserve(state().edges.size());
        for(const auto& edge:state().edges)out.push_back(guard.sub(input[edge.head],guard.mul(edge.ratio,input[edge.tail])));
        return out;
    }
    // The cycle is retained as a closing edge and shared tree. Expand a walk
    // only when explicitly requested; the solver never stores all root paths.
    std::vector<MultiplicativeWalkStep> cycle_walk(std::size_t index,Guard& guard)const{
        const auto& s=state();const auto& cycle=s.cycles.at(index);const auto& edge=s.edges.at(cycle.relation.index);
        const auto path=[&](std::size_t v){std::vector<MultiplicativeWalkStep> result;
            while(s.parents[v]){guard.step();const auto p=*s.parents[v];result.push_back({{core::Grade{1},static_cast<std::uint32_t>(p.edge)},p.forward});v=p.cell;}
            std::reverse(result.begin(),result.end());return result;};
        auto out=path(edge.tail);out.push_back({cycle.relation,true});auto back=path(edge.head);
        for(auto it=back.rbegin();it!=back.rend();++it)out.push_back({it->relation,!it->forward});
        guard.entries(out.size());return out;
    }
};
} // namespace rfc::learning
