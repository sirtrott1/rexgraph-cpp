#pragma once
#include "rfc/learning/transport_evidence.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/algebra/owned_gluing.hpp"
#include <map>

namespace rfc::learning {
struct TransportChainConstraint { std::size_t grade{}, row{}, column{}; };
struct GradedTransportUpdate {
    bool chain_reused{};
    std::size_t reused_observations{}, assembled_observations{}, execution_steps{};
};

// Complete one original boundary grade with all other transport families fixed.
// Fixed curvature adjacent to that grade becomes an affine right-hand side.
// Appends retain its full chain constraints and insert only new field equations.
class PreparedGradedTransportFit {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct Fixed {
        PreparedTransportObservations observations_;
        M chain_;
        V chain_target_;
        std::vector<TransportChainConstraint> constraints_;
        std::size_t retained_{},steps_{};
        static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
        static std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
        void admit(std::size_t scratch,Guard& guard) const {guard.entries(add(retained_,scratch));}
        static core::Grade grade(std::size_t k){return {static_cast<std::uint16_t>(k)};}
        static Q coefficient(const core::RelationalComplex& source,core::Grade g,std::size_t p,Guard& guard) {
            const auto& arena=source.grade(g);
            if(!arena.aspects.get(source.signature().existence_aspect(),p))return Q{0};
            const auto& lower=source.grade(grade(g.value-1));
            guard.value(lower.cell_scales[arena.lower_index[p]]);guard.value(arena.lattice_coefficient[p]);
            guard.value(arena.cell_scales[arena.upper_index[p]]);
            return guard.div(guard.mul(lower.cell_scales[arena.lower_index[p]],arena.lattice_coefficient[p]),
                arena.cell_scales[arena.upper_index[p]]);
        }
        std::vector<M> boundaries(const local::LinearRelationalFiberSystem& fibers,Guard& guard,bool completion=false) const {
            std::vector<M> result;std::size_t live{};
            for(std::size_t k=1;k<fibers.complex().grade_count();++k) {
                guard.step();guard.dimension(fibers.stalks().total_dimension(grade(k)));
                guard.dimension(fibers.stalks().total_dimension(grade(k-1)));
                const auto& arena=fibers.complex().grade(grade(k));
                // Offsets describe retained cells, not one slot per stalk coordinate.
                // Explicit maps are already realized: do not apply the incidence
                // coefficient again or expand the occurrence assembly identity.
                const auto lower=fibers.stalks().offsets(grade(k-1));
                const auto upper=fibers.stalks().offsets(grade(k));
                const auto offsets=add(lower.size(),upper.size());
                M boundary(lower.back(),upper.back());
                const auto exists=fibers.complex().signature().existence_aspect();
                admit(add(live,offsets),guard);
                for(std::size_t p=0;p<arena.incidence_count();++p) {
                    guard.step();const auto& map=fibers.transport_override(grade(k),p);
                    if(!map) throw std::invalid_argument("explicit maps required at every original grade");
                    guard.matrix(*map);live=add(live,map->nonzeros());
                    admit(add(add(live,offsets),boundary.nonzeros()),guard);
                    if(!arena.aspects.get(exists,p))continue;
                    for(const auto& [key,q]:map->entries()) {
                        guard.step();
                        const auto row=add(lower[arena.lower_index[p]],key.first);
                        const auto col=add(upper[arena.upper_index[p]],key.second);
                        admit(add(add(live,offsets),add(boundary.nonzeros(),1)),guard);
                        boundary.set(row,col,guard.add(boundary.get(row,col),q));
                    }
                }
                guard.matrix(boundary);
                live=add(live,boundary.nonzeros());admit(add(live,offsets),guard);
                if(!result.empty()) {
                    const auto product=numeric::checked_multiply(result.back(),boundary,guard);
                    admit(add(live,product.nonzeros()),guard);
                    const auto selected=observations_.grade().value;
                    if(product.nonzeros() && (!completion || (k!=selected && k!=selected+1u)))
                        throw std::invalid_argument("fixed transport defect outside the completed grade");
                }
                result.push_back(std::move(boundary));
            }
            return result;
        }
        void chain_constraints(Guard& guard) {
            const auto& fibers=observations_.owner().fibers();const auto& source=fibers.complex();
            const std::size_t k=observations_.grade().value;const auto& arena=source.grade(grade(k));
            const auto lower=fibers.stalks().offsets(grade(k-1)),upper=fibers.stalks().offsets(grade(k));
            const auto fixed=boundaries(fibers,guard,true);
            // Only coordinates reached by the adjacent fixed maps need a bucket.
            using Entries=std::map<std::size_t,std::vector<std::pair<std::size_t,Q>>>;
            std::size_t scratch=add(lower.size(),upper.size());admit(scratch,guard);
            Entries down,up;
            for(const auto& b:fixed)scratch=add(scratch,b.nonzeros());
            admit(scratch,guard);
            const auto index=[&](Entries& entries,std::size_t key,std::size_t other,const Q& q) {
                guard.step();
                scratch=add(scratch,entries.contains(key)?2:3);admit(scratch,guard);
                entries[key].emplace_back(other,q);
            };
            if(k>1) for(const auto& [key,q]:fixed[k-2].entries())
                index(down,key.second,key.first,q);
            if(k+1<source.grade_count()) for(const auto& [key,q]:fixed[k].entries())
                index(up,key.first,key.second,q);
            const auto n=observations_.parameter_count();
            using Key=std::tuple<std::size_t,std::size_t,std::size_t>;
            std::map<Key,std::map<std::size_t,Q>> rows;std::size_t entries{};
            std::map<Key,Q> target;
            for(const auto adjacent:{k,k+1}) {
                if(adjacent<2 || adjacent>=source.grade_count())continue;
                const auto defect=numeric::checked_multiply(fixed[adjacent-2],fixed[adjacent-1],guard);
                admit(add(scratch,defect.nonzeros()),guard);
                for(const auto& [key,q]:defect.entries()) {
                    guard.step();const Key coordinate{adjacent,key.first,key.second};
                    rows.emplace(coordinate,std::map<std::size_t,Q>{});
                    target.emplace(coordinate,guard.sub(0,q));
                    scratch=add(scratch,8);guard.dimension(rows.size());admit(scratch,guard);
                }
            }
            const auto contribute=[&](const Key& key,std::size_t parameter,const Q& q) {
                guard.step();if(!rows.contains(key)) {scratch=add(scratch,4);guard.dimension(add(rows.size(),1));}
                auto& row=rows[key];if(!row.contains(parameter)) {++entries;admit(add(scratch,mul(entries,2)),guard);}
                row[parameter]=guard.add(row[parameter],q);
            };
            const auto direction=[&](std::size_t entry,std::size_t parameter,const Q& weight) {
                guard.step();const auto& s=observations_.variable_coordinates()[entry];
                const auto r=lower[arena.lower_index[s.occurrence]]+s.row;
                const auto c=upper[arena.upper_index[s.occurrence]]+s.column;
                const auto q=guard.mul(weight,coefficient(source,grade(k),s.occurrence,guard));
                if(const auto at=down.find(r);at!=down.end())
                    for(const auto& [i,value]:at->second)contribute(Key{k,i,c},parameter,guard.mul(value,q));
                if(const auto at=up.find(c);at!=up.end())
                    for(const auto& [j,value]:at->second)contribute(Key{k+1,r,j},parameter,guard.mul(q,value));
            };
            if(observations_.parameter_basis()) {
                for(const auto& [key,q]:observations_.parameter_basis()->entries())direction(key.first,key.second,q);
            } else for(std::size_t i=0;i<observations_.variable_coordinates().size();++i)direction(i,i,Q{1});
            chain_=M(rows.size(),n);chain_target_=V(rows.size());std::size_t row{};
            for(const auto& [key,values]:rows) {
                guard.step();constraints_.push_back({std::get<0>(key),std::get<1>(key),std::get<2>(key)});
                for(const auto& [parameter,q]:values) {guard.step();chain_.set(row,parameter,q);}
                if(const auto rhs=target.find(key);rhs!=target.end())chain_target_[row]=rhs->second;
                ++row;
            }
            retained_=add(retained_,add(chain_.nonzeros(),add(chain_target_.size(),mul(constraints_.size(),3))));admit(scratch,guard);
        }
        Fixed(PreparedTransportObservations observations,Guard& guard):observations_(std::move(observations)) {
            const auto start=guard.steps();guard.step();
            retained_=add(observations_.retained_entries(),
                algebra::owned_gluing_detail::words(observations_.owner().fibers().complex()));
            const auto& fibers=observations_.owner().fibers();
            for(std::size_t k=0;k<fibers.complex().grade_count();++k) {
                guard.step();guard.dimension(fibers.stalks().total_dimension(grade(k)));
                // LinearStalkBundle stores one dimension per retained cell.
                // A declared coordinate extent is not that many stored entries.
                retained_=add(retained_,fibers.complex().grade(grade(k)).cell_count());
                if(k)for(std::size_t p=0;p<fibers.complex().grade(grade(k)).incidence_count();++p) {
                    guard.step();const auto& map=fibers.transport_override(grade(k),p);
                    if(!map)throw std::invalid_argument("explicit maps required at every original grade");
                    retained_=add(retained_,add(4,map->nonzeros()));admit(0,guard);
                }
            }
            chain_constraints(guard);steps_=guard.steps()-start;
        }
    };
    std::size_t started_{},retained_{},steps_{};
    std::shared_ptr<const Fixed> fixed_;
    PreparedTransportEvidence evidence_;
    M design_;
    V target_;
    GradedTransportUpdate update_;
    static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
    static std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
    static core::Grade grade(std::size_t k){return {static_cast<std::uint16_t>(k)};}
    void admit(std::size_t scratch,Guard& guard) const {guard.entries(add(retained_,scratch));}
    static PreparedTransportEvidence prepare(const Fixed& fixed,const std::vector<TransportObservation>& samples,Guard& guard) {
        PreparedTransportEvidence evidence(fixed.observations_,fixed.chain_,fixed.chain_target_,guard);
        return samples.empty()?evidence:evidence.append(samples,guard);
    }
    void retain(Guard& guard) {
        const auto start=guard.steps();const auto& original=evidence_.system();
        const auto rows=add(fixed_->chain_.rows(),original.design.rows()),n=model().parameter_count();
        guard.dimension(rows);retained_=add(fixed_->retained_,evidence_.retained_entries());
        admit(add(rows,add(fixed_->chain_.nonzeros(),original.design.nonzeros())),guard);
        design_=M(rows,n);target_=V(rows);
        for(const auto& [key,q]:fixed_->chain_.entries()){guard.step();design_.set(key.first,key.second,q);}
        for(std::size_t i=0;i<fixed_->chain_target_.size();++i){guard.step();target_[i]=fixed_->chain_target_[i];}
        for(const auto& [key,q]:original.design.entries()) {
            guard.step();design_.set(add(fixed_->chain_.rows(),key.first),key.second,q);
        }
        for(std::size_t i=0;i<original.target.size();++i){guard.step();target_[fixed_->chain_.rows()+i]=original.target[i];}
        if(numeric::checked_apply(design_,particular(),guard)!=target_ || numeric::checked_multiply(design_,kernel(),guard).nonzeros())
            throw std::logic_error("complete graded transport family certificate failed");
        retained_=add(retained_,add(design_.nonzeros(),target_.size()));admit(0,guard);
        steps_=add(fixed_->steps_,add(evidence_.preparation_steps(),guard.steps()-start));
        if(steps_>guard.limits().steps)throw std::length_error("complete graded transport replay exceeds step admission");
        const auto update=evidence_.update();
        update_.reused_observations=update.reused_observations;update_.assembled_observations=update.assembled_observations;
        update_.execution_steps=guard.steps()-started_;
    }
    PreparedGradedTransportFit(const PreparedGradedTransportFit& parent,const std::vector<TransportObservation>& suffix,Guard& guard)
        :started_(guard.steps()),fixed_(parent.fixed_),evidence_(parent.evidence_.append(suffix,guard)) {
        update_.chain_reused=true;retain(guard);
    }
public:
    PreparedGradedTransportFit(PreparedTransportObservations observations,std::vector<TransportObservation> samples,Guard& guard)
        :started_(guard.steps()),fixed_(std::make_shared<const Fixed>(std::move(observations),guard)),evidence_(prepare(*fixed_,samples,guard)) {
        retain(guard);
    }
    PreparedGradedTransportFit append(const std::vector<TransportObservation>& suffix,Guard& guard) const {
        guard.entries(mul(retained_,2));
        return PreparedGradedTransportFit(*this,suffix,guard);
    }
    const PreparedTransportObservations& model() const & {return fixed_->observations_;}
    const std::vector<TransportObservation>& observations() const & {return evidence_.observations();}
    const TransportObservationSystem& evidence() const & {return evidence_.system();}
    const M& chain() const & {return fixed_->chain_;}
    const V& chain_target() const & {return fixed_->chain_target_;}
    const M& design() const & {return design_;}
    const M& kernel() const & {return evidence_.kernel();}
    const V& target() const & {return target_;}
    const V& particular() const & {return evidence_.particular();}
    const std::vector<TransportChainConstraint>& chain_coordinates() const & {return fixed_->constraints_;}
    const std::vector<std::size_t>& pivots() const & {return evidence_.pivots();}
    const std::vector<std::size_t>& free_columns() const & {return evidence_.free_columns();}
    const PreparedTransportObservations& model() const &&=delete;
    const std::vector<TransportObservation>& observations() const &&=delete;
    const TransportObservationSystem& evidence() const &&=delete;
    const M& chain() const &&=delete;
    const V& chain_target() const &&=delete;
    const M& design() const &&=delete;
    const M& kernel() const &&=delete;
    const V& target() const &&=delete;
    const V& particular() const &&=delete;
    const std::vector<TransportChainConstraint>& chain_coordinates() const &&=delete;
    const std::vector<std::size_t>& pivots() const &&=delete;
    const std::vector<std::size_t>& free_columns() const &&=delete;
    std::size_t rank() const noexcept {return evidence_.rank();}
    std::size_t chain_rank() const noexcept {return evidence_.fixed_rank();}
    std::size_t observation_rank() const noexcept {return evidence_.observation_rank();}
    std::size_t retained_entries() const noexcept {return retained_;}
    // Complete ordered preparation cost; append execution is reported by update().
    std::size_t preparation_steps() const noexcept {return steps_;}
    GradedTransportUpdate update() const noexcept {return update_;}

    V parameters(const V& free,Guard& guard) const {
        if(free.size()!=kernel().cols())throw std::invalid_argument("every original free parameter value required");
        auto out=numeric::checked_apply(kernel(),guard.canonical_input(free),guard);
        for(std::size_t i=0;i<out.size();++i)out[i]=guard.add(out[i],particular()[i]);
        return out;
    }
    V entries(const V& parameters,Guard& guard) const {
        if(parameters.size()!=model().parameter_count())throw std::invalid_argument("complete original parameter field required");
        return model().parameter_basis()?numeric::checked_apply(*model().parameter_basis(),parameters,guard):guard.canonical_input(parameters);
    }
    local::OwnedLinearFiberSystem materialize(const V& free,Guard& guard) const {
        const auto values=entries(parameters(free,guard),guard);const auto& original=model().owner().fibers();
        admit(add(mul(retained_,3),mul(values.size(),4)),guard);
        local::LinearRelationalFiberSystem result(original.stalks());
        const auto k=model().grade();
        for(std::size_t g=1;g<original.complex().grade_count();++g)
            for(std::size_t p=0;p<original.complex().grade(grade(g)).incidence_count();++p) {
                guard.step();result.set_transport(grade(g),p,*original.transport_override(grade(g),p));
            }
        std::map<std::size_t,M> changed;
        for(std::size_t i=0;i<values.size();++i) {
            guard.step();const auto& s=model().variable_coordinates()[i];
            if(!changed.contains(s.occurrence))changed.emplace(s.occurrence,*result.transport_override(k,s.occurrence));
            auto& map=changed.at(s.occurrence);
            const auto q=guard.mul(Fixed::coefficient(original.complex(),k,s.occurrence,guard),values[i]);
            map.set(s.row,s.column,guard.add(map.get(s.row,s.column),q));
        }
        for(auto& [p,map]:changed)result.set_transport(k,p,std::move(map));
        (void)fixed_->boundaries(result,guard);return local::OwnedLinearFiberSystem(result);
    }
};
} // namespace rfc::learning
