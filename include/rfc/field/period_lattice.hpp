#pragma once
#include "rfc/field/exact_metric.hpp"
#include "rfc/topology/integral_homology.hpp"
#include <map>
#include <memory>
#include <set>

namespace rfc::field {

enum class PeriodCoordinateBasis { CoordinatedIntegralLattice };
// A declared cellular cup contribution. Both flags must be actual retained
// occurrences of the named face; their signed lattice incidences orient the
// two cochain readings. This data is retained before evaluation/aggregation.
struct PeriodCupTerm {
    std::uint32_t face;
    std::size_t first_slot,second_slot;
    numeric::Rational coefficient;
};
namespace period_detail {
using Q=numeric::Rational;
using I=numeric::Integer;
using M=numeric::ExactMatrix;
using Z=numeric::IntegerMatrix;
using Guard=numeric::ExactActionGuard;
inline M rational(const Z& input,Guard& guard){
    guard.dimension(input.rows());guard.dimension(input.cols());guard.entries(input.nonzeros());M out(input.rows(),input.cols());
    for(const auto& [key,value]:input.entries())if(value!=0){guard.step();Q q{value};guard.value(q);out.set(key.first,key.second,std::move(q));}
    return out;
}
inline M product(const M& a,const M& b,Guard& guard){
    if(a.cols()!=b.rows())throw std::invalid_argument("period product shape");
    guard.matrix(a);guard.matrix(b);M out(a.rows(),b.cols());
    for(const auto& [ik,left]:a.entries()){
        auto it=b.entries().lower_bound(M::Key{ik.second,0});
        for(;it!=b.entries().end()&&it->first.first==ik.second;++it){
            const auto j=it->first.second;out.set(ik.first,j,guard.add(out.get(ik.first,j),guard.mul(left,it->second)));guard.entries(out.nonzeros());
        }
    }
    return out;
}
inline Q fractional(Q value){value.canonicalize();I floor;mpz_fdiv_q(floor.get_mpz_t(),mpq_numref(value.get_mpq_t()),mpq_denref(value.get_mpq_t()));return Q(value-Q{floor});}
inline void integral_values(const Z::SparseVector& values,std::size_t dimension,Guard& guard){
    guard.dimension(dimension);guard.entries(values.size());for(const auto& [index,value]:values){guard.step();if(index>=dimension)throw std::out_of_range("integral period chain coordinate");guard.value(Q{value});}
}
struct State {
    core::RelationalComplex source;
    std::uint64_t key,revision;
    Z cycles,boundary_one,boundary_two;
    M cochains,harmonic_duals,gram,cup;
    field::ExactMetric metric;
    Z::SparseVector fundamental_cycle;
    std::vector<PeriodCupTerm> cup_terms;
};
} // namespace period_detail

class IntegralPeriodChain;
class IntegralPeriodPoint;

// A finite, complete integral H1 period declaration. Cochains and the metric
// are explicitly expressed on the source's coordinated integral cell basis;
// native rational cell scales remain retained in source(), never silently
// identified with that lattice. No choice of an arbitrary graph cycle basis is
// promoted to a primitive homology basis without its integral certificate.
class PreparedIntegralPeriodLattice {
public:
    static PreparedIntegralPeriodLattice prepare(core::RelationalComplex source,numeric::IntegerMatrix cycles,
        numeric::ExactMatrix dual_cochains,field::ExactMetric metric,
        numeric::IntegerMatrix::SparseVector fundamental_cycle,std::vector<PeriodCupTerm> cup_terms,
        PeriodCoordinateBasis coordinates,std::uint64_t key,std::uint64_t revision,numeric::ExactActionGuard& guard){
        using namespace period_detail;
        if(coordinates!=PeriodCoordinateBasis::CoordinatedIntegralLattice||!key||!revision)
            throw std::invalid_argument("period declaration needs an integral coordinate domain and source identity/revision");
        if(source.grade_count()<3)throw std::invalid_argument("cellular period pairing requires the retained grade-two source");
        const auto edges=source.grade(core::Grade{1}).cell_count(),h=cycles.cols();
        if(h%2||cycles.rows()!=edges||dual_cochains.rows()!=edges||dual_cochains.cols()!=h||metric.size()!=edges)
            throw std::invalid_argument("period cycle/cochain/metric carrier mismatch");
        if(!metric.supports_sparse_apply()||!metric.supports_sparse_solve())throw std::invalid_argument("period preparation requires declared sparse metric actions");
        guard.matrix(dual_cochains);const auto cycle_matrix=rational(cycles,guard);
        const auto d1=topology::integral_boundary_matrix(source,core::Grade{1});
        const auto d2=topology::integral_boundary_matrix(source,core::Grade{2});
        const auto b1=rational(d1,guard),b2=rational(d2,guard);
        // Retain and validate the complete source. A surface period consumer
        // must not hide an inconsistent higher grade behind its H1 slice.
        Z previous=d2,third(d2.cols(),0);
        for(std::size_t k=3;k<source.grade_count();++k){
            const auto next=topology::integral_boundary_matrix(source,core::Grade{static_cast<std::uint16_t>(k)});
            (void)rational(next,guard);
            if(previous.multiply(next,guard).nonzeros())throw std::invalid_argument("period source has an inconsistent retained higher chain");
            if(k==3)third=next;
            previous=next;
        }
        if(d1.multiply(d2,guard).nonzeros()||d1.multiply(cycles,guard).nonzeros())throw std::invalid_argument("period cycles do not belong to the source chain complex");
        const auto smith1=numeric::smith_normal_form_actions(d1,guard),smith2=numeric::smith_normal_form_actions(d2,guard);
        if(machine::checked_size_add(smith1.rank,smith2.rank)>edges||edges-smith1.rank-smith2.rank!=h)
            throw std::invalid_argument("declared period cycles do not cover the complete first homology");
        const auto kernel=edges-smith1.rank;
        const auto boundaries=topology::select_rows(smith1.right_inverse.apply(d2,guard),smith1.rank,kernel);
        const auto selected=topology::select_rows(smith1.right_inverse.apply(cycles,guard),smith1.rank,kernel);
        Z generators(kernel,machine::checked_size_add(boundaries.cols(),selected.cols()));
        for(const auto& [ij,value]:boundaries.entries())generators.set(ij.first,ij.second,value);
        for(const auto& [ij,value]:selected.entries())generators.set(ij.first,boundaries.cols()+ij.second,value);
        const auto generating_smith=numeric::smith_normal_form_actions(generators,guard);
        if(generating_smith.rank!=kernel)throw std::invalid_argument("period cycles miss an integral homology generator");
        for(const auto& invariant:generating_smith.invariant_factors)if(invariant!=1)
            throw std::invalid_argument("period cycles span a proper sublattice, not an integral homology basis");
        if(product(b2.transpose(),dual_cochains,guard).nonzeros())throw std::invalid_argument("period cochains are not closed");
        if(!(product(cycle_matrix.transpose(),dual_cochains,guard)==M::identity(h)))
            throw std::invalid_argument("source cycle and cochain periods are not the declared dual basis");

        M harmonic_duals(edges,h);
        for(std::size_t j=0;j<h;++j){
            std::vector<numeric::SparseRationalEntry> entries;
            for(const auto& [ij,value]:dual_cochains.entries())if(ij.second==j)entries.push_back({ij.first,value});
            const numeric::SparseRationalVector input(edges,std::move(entries));
            const auto solved=metric.solve(input.view()),residual=metric.apply(solved.view());
            if(residual.size()!=input.size()||residual.entries()!=input.entries())throw std::invalid_argument("period metric solve failed its exact residual");
            for(const auto& entry:solved.entries()){guard.step();guard.value(entry.value);harmonic_duals.set(entry.index,j,entry.value);}
        }
        if(product(b1,harmonic_duals,guard).nonzeros())throw std::invalid_argument("declared period cochains are not coclosed for the metric");
        const auto gram=product(dual_cochains.transpose(),harmonic_duals,guard);
        if(numeric::PreparedPositiveSemidefinite(gram,guard).rank()!=h)throw std::invalid_argument("period metric Gram form is not positive definite");

        integral_values(fundamental_cycle,d2.cols(),guard);I gcd{0};
        for(const auto& [face,value]:fundamental_cycle){(void)face;mpz_gcd(gcd.get_mpz_t(),gcd.get_mpz_t(),value.get_mpz_t());}
        if(gcd!=1||!d2.apply_sparse(fundamental_cycle,guard).empty())throw std::invalid_argument("cup evaluation needs a primitive closed integral two-chain");
        M raw_cup(edges,edges);std::map<std::uint32_t,M> face_cups;std::size_t face_entries=0;
        std::set<std::uint32_t> covered;const auto& faces=source.grade(core::Grade{2});
        const auto exists=source.signature().existence_aspect();guard.entries(cup_terms.size());
        for(auto& term:cup_terms){
            guard.step();term.coefficient.canonicalize();guard.value(term.coefficient);
            if(term.face>=faces.cell_count()||term.first_slot>=faces.incidence_count()||term.second_slot>=faces.incidence_count()||
                faces.upper_index[term.first_slot]!=term.face||faces.upper_index[term.second_slot]!=term.face)
                throw std::invalid_argument("cup term does not belong to the declared retained face");
            covered.insert(term.face);
            if(!faces.aspects.get(exists,term.first_slot)||!faces.aspects.get(exists,term.second_slot))continue;
            auto coefficient=guard.mul(term.coefficient,Q{faces.lattice_coefficient[term.first_slot]});
            coefficient=guard.mul(coefficient,Q{faces.lattice_coefficient[term.second_slot]});
            const auto i=faces.lower_index[term.first_slot],j=faces.lower_index[term.second_slot];
            auto& local=face_cups.try_emplace(term.face,edges,edges).first->second;
            face_entries-=local.nonzeros();local.set(i,j,guard.add(local.get(i,j),coefficient));
            face_entries=machine::checked_size_add(face_entries,local.nonzeros());
            const auto at=fundamental_cycle.find(term.face);
            if(at!=fundamental_cycle.end())raw_cup.set(i,j,guard.add(raw_cup.get(i,j),guard.mul(Q{at->second},coefficient)));
            guard.entries(machine::checked_size_add(face_entries,raw_cup.nonzeros()));
        }
        for(const auto& [face,value]:fundamental_cycle)if(value!=0&&!covered.contains(static_cast<std::uint32_t>(face)))
            throw std::invalid_argument("cup declaration omits a face in the fundamental cycle");
        const auto exact_cup=product(b1,raw_cup,guard);
        if(product(exact_cup,dual_cochains,guard).nonzeros()||product(exact_cup,b1.transpose(),guard).nonzeros()||
            product(product(b1,raw_cup.transpose(),guard),dual_cochains,guard).nonzeros())
            throw std::invalid_argument("declared cup pairing does not descend through source coboundaries");
        // The declared facewise product of closed one-cochains must itself be
        // closed across every retained three-cell. H and im(B1^T) span all
        // closed one-cochains, so four sparse block checks certify this claim.
        // In particular, attaching a three-cell that kills the fundamental
        // two-cycle cannot preserve an incompatible surface cup certificate.
        std::map<std::size_t,M> upper_cups;std::size_t upper_entries=0;
        for(const auto& [face_cell,weight]:third.entries()){
            const auto local=face_cups.find(static_cast<std::uint32_t>(face_cell.first));
            if(local==face_cups.end())continue;
            auto& boundary_cup=upper_cups.try_emplace(face_cell.second,edges,edges).first->second;
            upper_entries-=boundary_cup.nonzeros();
            for(const auto& [ij,value]:local->second.entries())boundary_cup.set(ij.first,ij.second,
                guard.add(boundary_cup.get(ij.first,ij.second),guard.mul(Q{weight},value)));
            upper_entries=machine::checked_size_add(upper_entries,boundary_cup.nonzeros());
            guard.entries(machine::checked_size_add(face_entries,upper_entries));
        }
        for(const auto& [cell,boundary_cup]:upper_cups){
            (void)cell;const auto on_h=product(boundary_cup,dual_cochains,guard);
            const auto on_exact=product(boundary_cup,b1.transpose(),guard);
            if(product(dual_cochains.transpose(),on_h,guard).nonzeros()||product(b1,on_h,guard).nonzeros()||
                product(dual_cochains.transpose(),on_exact,guard).nonzeros()||product(b1,on_exact,guard).nonzeros())
                throw std::invalid_argument("declared period cup product is not closed on the full retained chain");
        }
        const auto cup=product(dual_cochains.transpose(),product(raw_cup,dual_cochains,guard),guard);
        Z integral_cup(h,h);
        for(const auto& [ij,value]:cup.entries()){
            if(value!=-cup.get(ij.second,ij.first)||numeric::denominator(value)!=1)
                throw std::invalid_argument("period cup form must be integral and alternating");
            integral_cup.set(ij.first,ij.second,numeric::numerator(value));
        }
        const auto polarization=numeric::smith_normal_form_actions(integral_cup,guard);
        if(polarization.rank!=h)throw std::invalid_argument("period cup form is degenerate");
        for(const auto& invariant:polarization.invariant_factors)if(invariant!=1)throw std::invalid_argument("period cup form is not unimodular");
        return PreparedIntegralPeriodLattice(std::make_shared<const State>(State{std::move(source),key,revision,std::move(cycles),d1,d2,
            std::move(dual_cochains),std::move(harmonic_duals),gram,cup,std::move(metric),std::move(fundamental_cycle),std::move(cup_terms)}));
    }
    const core::RelationalComplex& source() const & noexcept{return state_->source;}
    const core::RelationalComplex& source() const &&=delete;
    const numeric::IntegerMatrix& integral_cycles() const & noexcept{return state_->cycles;}
    const numeric::IntegerMatrix& integral_cycles() const &&=delete;
    const numeric::ExactMatrix& dual_cochains() const & noexcept{return state_->cochains;}
    const numeric::ExactMatrix& dual_cochains() const &&=delete;
    const numeric::ExactMatrix& gram() const & noexcept{return state_->gram;}
    const numeric::ExactMatrix& gram() const &&=delete;
    const numeric::ExactMatrix& cup_form() const & noexcept{return state_->cup;}
    const numeric::ExactMatrix& cup_form() const &&=delete;
    const std::vector<PeriodCupTerm>& cup_terms() const & noexcept{return state_->cup_terms;}
    const std::vector<PeriodCupTerm>& cup_terms() const &&=delete;
    std::size_t period_rank() const noexcept{return state_->cycles.cols();}
    std::size_t genus() const noexcept{return period_rank()/2;}
    std::uint64_t key() const noexcept{return state_->key;}
    std::uint64_t revision() const noexcept{return state_->revision;}
    bool same_owner(const PreparedIntegralPeriodLattice& other) const noexcept{return state_==other.state_;}
    IntegralPeriodChain chain(numeric::IntegerMatrix::SparseVector values,numeric::ExactActionGuard& guard) const;
    numeric::RationalVector periods(const IntegralPeriodChain& chain,numeric::ExactActionGuard& guard) const;
    IntegralPeriodPoint point(std::uint32_t base,std::uint32_t vertex,const IntegralPeriodChain& connecting_chain,numeric::ExactActionGuard& guard) const;
private:
    explicit PreparedIntegralPeriodLattice(std::shared_ptr<const period_detail::State> state):state_(std::move(state)){}
    std::shared_ptr<const period_detail::State> state_;
};

class IntegralPeriodChain {
public:
    const PreparedIntegralPeriodLattice& owner() const & noexcept{return owner_;}
    const PreparedIntegralPeriodLattice& owner() const &&=delete;
    const numeric::IntegerMatrix::SparseVector& values() const & noexcept{return values_;}
    const numeric::IntegerMatrix::SparseVector& values() const &&=delete;
private:
    friend class PreparedIntegralPeriodLattice;
    IntegralPeriodChain(PreparedIntegralPeriodLattice owner,numeric::IntegerMatrix::SparseVector values):owner_(std::move(owner)),values_(std::move(values)){}
    PreparedIntegralPeriodLattice owner_;numeric::IntegerMatrix::SparseVector values_;
};
class IntegralPeriodPoint {
public:
    const IntegralPeriodChain& connecting_chain() const & noexcept{return chain_;}
    const IntegralPeriodChain& connecting_chain() const &&=delete;
    const numeric::RationalVector& coordinates() const & noexcept{return coordinates_;}
    const numeric::RationalVector& coordinates() const &&=delete;
    std::uint32_t base_vertex() const noexcept{return base_;}
    std::uint32_t vertex() const noexcept{return vertex_;}
private:
    friend class PreparedIntegralPeriodLattice;
    IntegralPeriodPoint(IntegralPeriodChain chain,std::uint32_t base,std::uint32_t vertex,numeric::RationalVector coordinates)
        :chain_(std::move(chain)),base_(base),vertex_(vertex),coordinates_(std::move(coordinates)){}
    IntegralPeriodChain chain_;std::uint32_t base_,vertex_;numeric::RationalVector coordinates_;
};
inline IntegralPeriodChain PreparedIntegralPeriodLattice::chain(numeric::IntegerMatrix::SparseVector values,numeric::ExactActionGuard& guard) const{
    period_detail::integral_values(values,state_->cycles.rows(),guard);
    for(auto it=values.begin();it!=values.end();)if(it->second==0)it=values.erase(it);else ++it;
    return IntegralPeriodChain(*this,std::move(values));
}
inline numeric::RationalVector PreparedIntegralPeriodLattice::periods(const IntegralPeriodChain& chain,numeric::ExactActionGuard& guard) const{
    if(!same_owner(chain.owner()))throw std::invalid_argument("period chain source owner differs");
    numeric::RationalVector out(period_rank(),numeric::Rational{0});
    for(const auto& [ij,value]:state_->cochains.entries()){
        const auto it=chain.values().find(ij.first);if(it!=chain.values().end())out[ij.second]=guard.add(out[ij.second],guard.mul(value,numeric::Rational{it->second}));
    }
    return out;
}
inline IntegralPeriodPoint PreparedIntegralPeriodLattice::point(std::uint32_t base,std::uint32_t vertex,const IntegralPeriodChain& chain,numeric::ExactActionGuard& guard) const{
    if(!same_owner(chain.owner()))throw std::invalid_argument("Abel-Jacobi chain source owner differs");
    const auto vertices=source().grade(core::Grade{0}).cell_count();
    if(base>=vertices||vertex>=vertices)throw std::out_of_range("Abel-Jacobi source vertex");
    numeric::IntegerMatrix::SparseVector target;if(base!=vertex){target.emplace(base,numeric::Integer{-1});target.emplace(vertex,numeric::Integer{1});}
    if(state_->boundary_one.apply_sparse(chain.values(),guard)!=target)throw std::invalid_argument("declared vertex chain has a different source boundary");
    auto coordinates=periods(chain,guard);for(auto& value:coordinates)value=period_detail::fractional(std::move(value));
    return IntegralPeriodPoint(chain,base,vertex,std::move(coordinates));
}
} // namespace rfc::field
