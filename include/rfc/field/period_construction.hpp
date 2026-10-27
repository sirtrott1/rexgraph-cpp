#pragma once
#include "rfc/field/period_lattice.hpp"
#include <type_traits>

namespace rfc::field {
namespace period_construction_detail {
using namespace period_detail;
inline std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
inline std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
// Container/scalar entry allowance, not bytes or a bound on GMP/RSS. A phase
// receives the remaining entry allowance and shares the caller's step clock.
inline Guard phase(Guard& parent,std::size_t retained){
    parent.entries(retained);auto limits=parent.limits();limits.entries-=retained;
    return Guard(limits,[&parent]{parent.step();});
}
inline Z boundary(const core::RelationalComplex& source,std::size_t k,Guard& g){
    const auto& a=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
    Z out(source.grade(core::Grade{static_cast<std::uint16_t>(k-1)}).cell_count(),a.cell_count());
    const auto exists=source.signature().existence_aspect();
    for(std::size_t s=0;s<a.incidence_count();++s){
        g.step();g.value(Q{a.lattice_coefficient[s]});
        if(!a.aspects.get(exists,s))continue;
        const auto i=a.lower_index[s],j=a.upper_index[s];
        const auto q=g.add(Q{out.get(i,j)},Q{a.lattice_coefficient[s]});out.set(i,j,q.get_num());g.entries(out.nonzeros());
    }
    return out;
}
inline M measure(const ExactMetric& metric,const M& frame,Guard& g){
    g.matrix(frame);
    if(metric.kind()==ExactMetricKind::Identity)return frame;
    if(const auto* d=metric.stored_diagonal()){
        M out(frame.rows(),frame.cols());
        for(const auto& [ij,q]:frame.entries())out.set(ij.first,ij.second,g.mul((*d)[ij.first],q));
        return out;
    }
    return product(*metric.stored_sparse_form(),frame,g);
}
inline M solve_columns(const M& a,const M& rhs,Guard& g){
    if(a.rows()!=rhs.rows())throw std::invalid_argument("period construction solve carrier");
    g.matrix(a);g.matrix(rhs);M out(a.cols(),rhs.cols());
    if(!a.rows())return out;
    const numeric::PreparedExactSolve solve(a,g);
    for(std::size_t j=0;j<rhs.cols();++j){
        numeric::RationalVector v(rhs.rows());for(std::size_t i=0;i<v.size();++i){g.step();v[i]=rhs.get(i,j);}
        const auto x=solve.apply(v,g);
        for(std::size_t i=0;i<x.size();++i){g.step();out.set(i,j,x[i]);}g.entries(out.nonzeros());
    }
    if(product(a,out,g)!=rhs)throw std::logic_error("period construction solve residual");
    return out;
}
inline std::size_t action_entries(const numeric::SmithNormalFormActions& s){
    return add(add(s.diagonal.nonzeros(),s.invariant_factors.size()),mul(4,add(s.left.operations().size(),s.right_inverse.operations().size())));
}
} // namespace period_construction_detail

struct IntegralPeriodConstructionWork {
    std::size_t source_cells{},source_occurrences{},kernel_rank{},boundary_rank{},period_rank{};
    std::size_t retained_entry_allowance{},preparation_steps{};
};
class IntegralPeriodPathComparison;

// Constructs the full primitive torsion-free H1 lattice by integral Smith
// actions, then its metric harmonic duals. Cup/fundamental data are declarations
// validated by PreparedIntegralPeriodLattice, not discovered surface geometry.
// Supported metrics are owned native identity, diagonal and sparse SPD forms.
class PreparedIntegralPeriodConstruction {
    using Q=numeric::Rational;using I=numeric::Integer;using Z=numeric::IntegerMatrix;
    using M=numeric::ExactMatrix;using Guard=numeric::ExactActionGuard;
    struct State {
        PreparedIntegralPeriodLattice lattice;
        numeric::SmithNormalFormActions source_smith,boundary_smith;
        IntegralPeriodConstructionWork work;
    };
public:
    static PreparedIntegralPeriodConstruction prepare(core::RelationalComplex source,ExactMetric metric,
        Z::SparseVector fundamental,std::vector<PeriodCupTerm> cup,PeriodCoordinateBasis coordinates,
        std::uint64_t key,std::uint64_t revision,Guard& guard){
        using namespace period_construction_detail;
        const auto start=guard.steps();guard.step();
        if(coordinates!=PeriodCoordinateBasis::CoordinatedIntegralLattice||!key||!revision||source.grade_count()<3)
            throw std::invalid_argument("period construction requires a retained integral source and identity/revision");
        if(metric.kind()!=ExactMetricKind::Identity&&metric.kind()!=ExactMetricKind::Diagonal&&metric.kind()!=ExactMetricKind::SparseMatrix)
            throw std::invalid_argument("period construction metric profile requires an owned native SPD form");
        const auto e=source.grade(core::Grade{1}).cell_count();
        if(metric.size()!=e)throw std::invalid_argument("period construction metric carrier");
        IntegralPeriodConstructionWork work;
        guard.dimension(source.grade_count());
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto& a=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
            guard.dimension(a.cell_count());guard.dimension(a.incidence_count());
            work.source_cells=add(work.source_cells,a.cell_count());work.source_occurrences=add(work.source_occurrences,a.incidence_count());
            for(const auto& q:a.cell_scales){guard.step();guard.value(q);}
            for(const auto& z:a.lattice_coefficient){guard.step();guard.value(Q{z});}
        }
        // Include retained source tables, declarations and both validator copies.
        std::size_t retained=add(128,add(mul(64,work.source_cells),mul(96,work.source_occurrences)));
        retained=add(retained,add(mul(16,cup.size()),mul(4,fundamental.size())));guard.entries(retained);
        for(const auto& t:cup){guard.step();guard.value(t.coefficient);}
        integral_values(fundamental,source.grade(core::Grade{2}).cell_count(),guard);
        if(const auto* d=metric.stored_diagonal()){guard.vector(*d);retained=add(retained,mul(4,d->size()));}
        if(const auto* m=metric.stored_sparse_form()){guard.matrix(*m);retained=add(retained,mul(4,m->nonzeros()));}
        auto assembly=phase(guard,retained);
        const auto b1=boundary(source,1,assembly),b2=boundary(source,2,assembly);
        if(b1.multiply(b2,assembly).nonzeros())throw std::invalid_argument("period source chain is inconsistent");
        Z previous=b2;
        for(std::size_t k=3;k<source.grade_count();++k){
            auto next=boundary(source,k,assembly);
            if(previous.multiply(next,assembly).nonzeros())throw std::invalid_argument("period source higher chain is inconsistent");
            previous=std::move(next);
        }
        retained=add(retained,mul(4,add(b1.nonzeros(),b2.nonzeros())));
        auto smith_guard=phase(guard,retained);
        auto s1=numeric::smith_normal_form_actions(b1,smith_guard);
        retained=add(retained,action_entries(s1));
        auto quotient_guard=phase(guard,retained);
        const auto k=e-s1.rank;
        const auto coordinates2=s1.right_inverse.apply(b2,quotient_guard);
        for(const auto& [ij,value]:coordinates2.entries())if(ij.first<s1.rank&&value!=0)
            throw std::logic_error("boundary outside integral kernel");
        const auto relations=topology::select_rows(coordinates2,s1.rank,k);
        auto s2=numeric::smith_normal_form_actions(relations,quotient_guard);
        for(const auto& factor:s2.invariant_factors){quotient_guard.step();if(factor!=1)throw std::invalid_argument("period construction requires torsion-free integral H1");}
        const auto h=k-s2.rank;
        if(h%2)throw std::invalid_argument("period construction requires even complete H1 rank");
        retained=add(retained,add(action_entries(s2),mul(4,coordinates2.nonzeros())));
        // Selected frames and small Gram solves; no full V or global inverse.
        retained=add(retained,mul(24,add(add(mul(e,h),mul(e,s2.rank)),add(mul(s2.rank,s2.rank),mul(h,h)))));
        auto build=phase(guard,retained);
        Z selector(k,h);for(std::size_t j=0;j<h;++j){build.step();selector.set(s2.rank+j,j,I{1});}
        const auto free=s2.left.apply_inverse(selector,build);
        Z embedded(e,h);for(const auto& [ij,z]:free.entries()){build.step();embedded.set(s1.rank+ij.first,ij.second,z);}
        auto cycles=s1.right_inverse.apply_inverse(embedded,build);
        const auto a=rational(b2.multiply(s2.right_columns(0,s2.rank,build),build),build);
        const auto z=rational(cycles,build);
        const auto ma=measure(metric,a,build),mz=measure(metric,z,build);
        const auto coefficients=solve_columns(product(a.transpose(),ma,build),product(a.transpose(),mz,build),build);
        const auto correction=product(a,coefficients,build);
        M harmonic=z;
        for(const auto& [ij,q]:correction.entries())harmonic.set(ij.first,ij.second,build.sub(harmonic.get(ij.first,ij.second),q));
        const auto measured=measure(metric,harmonic,build);
        const auto gram=product(harmonic.transpose(),measured,build);
        const auto inverse_columns=solve_columns(gram,M::identity(h),build);
        auto cochains=product(measured,inverse_columns,build);
        auto acceptance=phase(guard,retained);
        auto lattice=PreparedIntegralPeriodLattice::prepare(std::move(source),std::move(cycles),std::move(cochains),std::move(metric),
            std::move(fundamental),std::move(cup),coordinates,key,revision,acceptance);
        work.kernel_rank=k;work.boundary_rank=s2.rank;work.period_rank=h;work.retained_entry_allowance=retained;
        guard.step();work.preparation_steps=guard.steps()-start;
        return PreparedIntegralPeriodConstruction(std::make_shared<const State>(State{std::move(lattice),std::move(s1),std::move(s2),work}));
    }
    const PreparedIntegralPeriodLattice& lattice() const & noexcept{return state_->lattice;}
    const PreparedIntegralPeriodLattice& lattice() const &&=delete;
    const numeric::SmithNormalFormActions& source_smith() const & noexcept{return state_->source_smith;}
    const numeric::SmithNormalFormActions& source_smith() const &&=delete;
    const numeric::SmithNormalFormActions& boundary_smith() const & noexcept{return state_->boundary_smith;}
    const numeric::SmithNormalFormActions& boundary_smith() const &&=delete;
    const IntegralPeriodConstructionWork& work() const & noexcept{return state_->work;}
    const IntegralPeriodConstructionWork& work() const &&=delete;

    // Deterministic bounded BFS over the actual retained unit pairwise edges.
    // Validate the entire requested profile even for base==vertex. Repeated,
    // absent, zero or branch occurrences never become an inferred graph edge.
    IntegralPeriodChain connecting_chain(std::uint32_t base,std::uint32_t vertex,Guard& guard) const {
        using namespace period_construction_detail;
        guard.step();const auto& source=lattice().source();const auto& edges=source.grade(core::Grade{1});
        const auto n=source.grade(core::Grade{0}).cell_count();
        if(base>=n||vertex>=n)throw std::out_of_range("period connecting vertex");
        guard.dimension(n);guard.dimension(edges.cell_count());
        auto action=phase(guard,add(work().retained_entry_allowance,add(mul(8,n),mul(8,edges.cell_count()))));
        struct Arc {std::uint32_t vertex;std::size_t edge;int sign;};
        std::vector<std::vector<Arc>> adjacency(n);const auto exists=source.signature().existence_aspect();
        for(std::size_t e=0;e<edges.cell_count();++e){
            action.step();const auto [begin,end]=edges.incidence_range(e);
            if(end-begin!=2||!edges.aspects.get(exists,begin)||!edges.aspects.get(exists,begin+1))
                throw std::invalid_argument("connecting chains require retained present unit pairwise edges");
            const auto& x=edges.lattice_coefficient[begin];const auto& y=edges.lattice_coefficient[begin+1];
            action.value(Q{x});action.value(Q{y});
            if((x!=1&&x!=-1)||y!=-x)throw std::invalid_argument("connecting chains require opposite unit incidences");
            const auto u=edges.lower_index[begin],v=edges.lower_index[begin+1];
            const int sign=x==-1?1:-1;
            adjacency[u].push_back({v,e,sign});adjacency[v].push_back({u,e,-sign});
        }
        std::vector<std::size_t> parent(n,n),edge(n);std::vector<int> signs(n);std::vector<std::uint32_t> queue;
        queue.reserve(n);queue.push_back(base);parent[base]=base;
        for(std::size_t i=0;i<queue.size()&&parent[vertex]==n;++i)for(const auto& a:adjacency[queue[i]]){
            action.step();if(parent[a.vertex]!=n)continue;
            parent[a.vertex]=queue[i];edge[a.vertex]=a.edge;signs[a.vertex]=a.sign;queue.push_back(a.vertex);
        }
        if(parent[vertex]==n)throw std::invalid_argument("period vertices are disconnected in the admitted edge profile");
        Z::SparseVector values;
        for(std::size_t v=vertex;v!=base;v=parent[v]){action.step();values.emplace(edge[v],I{signs[v]});}
        auto chain=lattice().chain(std::move(values),action);
        (void)lattice().point(base,vertex,chain,action);guard.step();return chain;
    }
    IntegralPeriodPoint point(std::uint32_t base,std::uint32_t vertex,Guard& guard) const {
        const auto chain=connecting_chain(base,vertex,guard);auto action=period_construction_detail::phase(guard,work().retained_entry_allowance);
        auto result=lattice().point(base,vertex,chain,action);guard.step();return result;
    }
    template<class B,class V> requires (std::is_integral_v<B>&&std::is_integral_v<V> &&
        !std::is_same_v<B,bool> && !std::is_same_v<V,bool>)
    IntegralPeriodChain connecting_chain(B base,V vertex,Guard& guard) const {
        if(!std::in_range<std::uint32_t>(base)||!std::in_range<std::uint32_t>(vertex))throw std::out_of_range("period vertex narrowing");
        return connecting_chain(static_cast<std::uint32_t>(base),static_cast<std::uint32_t>(vertex),guard);
    }
    template<class B,class V> requires (std::is_integral_v<B>&&std::is_integral_v<V> &&
        !std::is_same_v<B,bool> && !std::is_same_v<V,bool>)
    IntegralPeriodPoint point(B base,V vertex,Guard& guard) const {
        if(!std::in_range<std::uint32_t>(base)||!std::in_range<std::uint32_t>(vertex))throw std::out_of_range("period vertex narrowing");
        return point(static_cast<std::uint32_t>(base),static_cast<std::uint32_t>(vertex),guard);
    }
    template<class B,class V> requires (std::is_floating_point_v<B>||std::is_floating_point_v<V>)
    IntegralPeriodChain connecting_chain(B,V,Guard&) const=delete;
    template<class B,class V> requires (std::is_floating_point_v<B>||std::is_floating_point_v<V>)
    IntegralPeriodPoint point(B,V,Guard&) const=delete;

    // Integer quotient coordinates of a CLOSED chain; both Smith changes are
    // replayed, then independently checked against the accepted period duals.
    std::vector<I> homology_coordinates(const IntegralPeriodChain& chain,Guard& guard) const {
        if(!lattice().same_owner(chain.owner()))throw std::invalid_argument("homology chain has a foreign period owner");
        auto action=period_construction_detail::phase(guard,work().retained_entry_allowance);
        period_detail::integral_values(chain.values(),lattice().integral_cycles().rows(),action);
        const auto full=source_smith().right_inverse.apply_sparse(chain.values(),action);Z::SparseVector kernel;
        for(const auto& [i,z]:full){action.step();if(i<source_smith().rank&&z!=0)throw std::invalid_argument("homology coordinates require a closed chain");
            if(i>=source_smith().rank)kernel.emplace(i-source_smith().rank,z);}
        const auto quotient=boundary_smith().left.apply_sparse(kernel,action);
        action.entries(work().period_rank);std::vector<I> out(work().period_rank);
        for(const auto& [i,z]:quotient)if(i>=boundary_smith().rank)out[i-boundary_smith().rank]=z;
        const auto periods=lattice().periods(chain,action);
        for(std::size_t j=0;j<out.size();++j){action.step();action.value(Q{out[j]});if(periods[j]!=Q{out[j]})throw std::logic_error("integral homology/period residual");}
        guard.step();return out;
    }
    IntegralPeriodPathComparison compare_paths(IntegralPeriodPoint first,IntegralPeriodPoint second,Guard& guard) const;
private:
    explicit PreparedIntegralPeriodConstruction(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};

// Retains both actual source chains, their closed difference and its integer
// coordinates in the complete period lattice. Fractional points agree exactly.
class IntegralPeriodPathComparison {
public:
    const IntegralPeriodPoint& first() const & noexcept{return first_;}
    const IntegralPeriodPoint& first() const &&=delete;
    const IntegralPeriodPoint& second() const & noexcept{return second_;}
    const IntegralPeriodPoint& second() const &&=delete;
    const IntegralPeriodChain& difference() const & noexcept{return difference_;}
    const IntegralPeriodChain& difference() const &&=delete;
    const std::vector<numeric::Integer>& coordinates() const & noexcept{return coordinates_;}
    const std::vector<numeric::Integer>& coordinates() const &&=delete;
private:
    friend class PreparedIntegralPeriodConstruction;
    IntegralPeriodPathComparison(IntegralPeriodPoint a,IntegralPeriodPoint b,IntegralPeriodChain d,std::vector<numeric::Integer> c)
        :first_(std::move(a)),second_(std::move(b)),difference_(std::move(d)),coordinates_(std::move(c)){}
    IntegralPeriodPoint first_,second_;IntegralPeriodChain difference_;std::vector<numeric::Integer> coordinates_;
};
inline IntegralPeriodPathComparison PreparedIntegralPeriodConstruction::compare_paths(IntegralPeriodPoint first,IntegralPeriodPoint second,Guard& guard) const {
    if(!lattice().same_owner(first.connecting_chain().owner())||!lattice().same_owner(second.connecting_chain().owner())||
        first.base_vertex()!=second.base_vertex()||first.vertex()!=second.vertex())throw std::invalid_argument("path comparison requires the same actual period owner and endpoints");
    auto action=period_construction_detail::phase(guard,period_construction_detail::add(work().retained_entry_allowance,
        period_construction_detail::mul(8,period_construction_detail::add(first.connecting_chain().values().size(),second.connecting_chain().values().size()))));
    auto values=second.connecting_chain().values();
    for(const auto& [i,z]:first.connecting_chain().values()){
        const auto q=action.sub(Q{values[i]},Q{z});values[i]=q.get_num();
    }
    auto difference=lattice().chain(std::move(values),action);
    auto coordinates=homology_coordinates(difference,guard);
    if(first.coordinates()!=second.coordinates())throw std::logic_error("closed integral path change altered the period point");
    guard.step();return IntegralPeriodPathComparison(std::move(first),std::move(second),std::move(difference),std::move(coordinates));
}
} // namespace rfc::field
