#pragma once
#include "rfc/learning/transport_observations.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/algebra/owned_gluing.hpp"
#include <map>

namespace rfc::learning {
struct TransportChainConstraint { std::size_t grade{}, row{}, column{}; };

// Learn one original boundary grade with all other transport families fixed.
// Every parameter satisfies both adjacent chain laws and all observed fields.
class PreparedGradedTransportFit {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    PreparedTransportObservations observations_;
    std::vector<TransportObservation> samples_;
    TransportObservationSystem evidence_;
    M chain_,design_,kernel_;
    V target_,particular_;
    std::vector<TransportChainConstraint> constraints_;
    std::vector<std::size_t> pivots_,free_columns_;
    std::size_t chain_rank_{},observation_rank_{},retained_{},steps_{};
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
    std::vector<M> boundaries(const local::LinearRelationalFiberSystem& fibers,Guard& guard) const {
        std::vector<M> result;std::size_t live{};
        for(std::size_t k=1;k<fibers.complex().grade_count();++k) {
            guard.step();guard.dimension(fibers.stalks().total_dimension(grade(k)));
            guard.dimension(fibers.stalks().total_dimension(grade(k-1)));
            const auto& arena=fibers.complex().grade(grade(k));
            for(std::size_t p=0;p<arena.incidence_count();++p) {
                guard.step();const auto& map=fibers.transport_override(grade(k),p);
                if(!map) throw std::invalid_argument("explicit maps required at every original grade");
                guard.matrix(*map);live=add(live,map->nonzeros());admit(live,guard);
            }
            auto boundary=local::GradeTensorField(fibers,grade(k)).matrix();guard.matrix(boundary);
            live=add(live,boundary.nonzeros());admit(live,guard);
            if(!result.empty()) {
                const auto product=numeric::checked_multiply(result.back(),boundary,guard);
                admit(add(live,product.nonzeros()),guard);
                if(product.nonzeros()) throw std::invalid_argument("complete fixed transport chain family required");
            }
            result.push_back(std::move(boundary));
        }
        return result;
    }
    void chain_constraints(Guard& guard) {
        const auto& fibers=observations_.owner().fibers();const auto& source=fibers.complex();
        const std::size_t k=observations_.grade().value;const auto& arena=source.grade(grade(k));
        const auto lower=fibers.stalks().offsets(grade(k-1)),upper=fibers.stalks().offsets(grade(k));
        const auto fixed=boundaries(fibers,guard);
        using Entries=std::vector<std::vector<std::pair<std::size_t,Q>>>;
        std::size_t scratch=add(lower.back(),upper.back());admit(scratch,guard);
        Entries down(lower.back()),up(upper.back());
        for(const auto& b:fixed)scratch=add(scratch,b.nonzeros());
        if(k>1) for(const auto& [key,q]:fixed[k-2].entries()) {
            guard.step();down[key.second].emplace_back(key.first,q);scratch=add(scratch,2);admit(scratch,guard);
        }
        if(k+1<source.grade_count()) for(const auto& [key,q]:fixed[k].entries()) {
            guard.step();up[key.first].emplace_back(key.second,q);scratch=add(scratch,2);admit(scratch,guard);
        }
        const auto n=observations_.parameter_count();
        using Key=std::tuple<std::size_t,std::size_t,std::size_t>;
        std::map<Key,std::map<std::size_t,Q>> rows;std::size_t entries{};
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
            for(const auto& [i,value]:down[r])contribute(Key{k,i,c},parameter,guard.mul(value,q));
            for(const auto& [j,value]:up[c])contribute(Key{k+1,r,j},parameter,guard.mul(q,value));
        };
        if(observations_.parameter_basis()) {
            for(const auto& [key,q]:observations_.parameter_basis()->entries())direction(key.first,key.second,q);
        } else for(std::size_t i=0;i<observations_.variable_coordinates().size();++i)direction(i,i,Q{1});
        chain_=M(rows.size(),n);std::size_t row{};
        for(const auto& [key,values]:rows) {
            guard.step();constraints_.push_back({std::get<0>(key),std::get<1>(key),std::get<2>(key)});
            for(const auto& [parameter,q]:values) {guard.step();chain_.set(row,parameter,q);}
            ++row;
        }
        retained_=add(retained_,add(chain_.nonzeros(),mul(constraints_.size(),3)));admit(scratch,guard);
    }
public:
    PreparedGradedTransportFit(PreparedTransportObservations observations,
        std::vector<TransportObservation> samples,Guard& guard)
        :observations_(std::move(observations)),samples_(std::move(samples)) {
        const auto start=guard.steps();guard.step();
        retained_=add(observations_.retained_entries(),
            algebra::owned_gluing_detail::words(observations_.owner().fibers().complex()));
        const auto& fibers=observations_.owner().fibers();
        for(std::size_t k=0;k<fibers.complex().grade_count();++k) {
            guard.step();guard.dimension(fibers.stalks().total_dimension(grade(k)));
            retained_=add(retained_,fibers.stalks().total_dimension(grade(k)));
            if(k)for(std::size_t p=0;p<fibers.complex().grade(grade(k)).incidence_count();++p) {
                guard.step();const auto& map=fibers.transport_override(grade(k),p);
                if(!map)throw std::invalid_argument("explicit maps required at every original grade");
                retained_=add(retained_,add(4,map->nonzeros()));admit(0,guard);
            }
        }
        for(const auto& sample:samples_) {
            retained_=add(retained_,add(sample.input.size(),add(sample.target.size(),sample.observed.size())));admit(0,guard);
        }
        chain_constraints(guard);evidence_=observations_.assemble(samples_,guard);
        retained_=add(retained_,add(evidence_.design.nonzeros(),add(evidence_.augmented.nonzeros(),mul(evidence_.target.size(),4))));
        const auto n=observations_.parameter_count(),rows=add(chain_.rows(),evidence_.design.rows());
        guard.dimension(rows);guard.dimension(add(n,1));design_=M(rows,n);M augmented(rows,add(n,1));
        for(const auto& [key,q]:chain_.entries()) {guard.step();design_.set(key.first,key.second,q);augmented.set(key.first,key.second,q);}
        for(const auto& [key,q]:evidence_.design.entries()) {
            guard.step();design_.set(add(chain_.rows(),key.first),key.second,q);augmented.set(add(chain_.rows(),key.first),key.second,q);
        }
        target_.resize(rows);
        for(std::size_t i=0;i<evidence_.target.size();++i) {
            guard.step();target_[chain_.rows()+i]=evidence_.target[i];augmented.set(chain_.rows()+i,n,evidence_.target[i]);
        }
        const auto live=add(design_.nonzeros(),add(augmented.nonzeros(),rows));admit(live,guard);
        const numeric::PreparedExactFrames chain(chain_,guard),observed(evidence_.design,guard);
        const numeric::PreparedExactFrames frames(design_,guard),joint(augmented,guard);
        admit(add(live,add(chain.stored_entries(),add(observed.stored_entries(),add(frames.stored_entries(),joint.stored_entries())))),guard);
        if(frames.rank()!=joint.rank())throw std::invalid_argument("observations conflict with the complete transport chain family");
        chain_rank_=chain.rank();observation_rank_=observed.rank();pivots_=frames.pivots();free_columns_=frames.free_columns();
        kernel_=frames.kernel();particular_.resize(n);
        for(std::size_t i=0;i<joint.rank();++i)particular_[joint.pivots()[i]]=joint.reduced().get(i,n);
        if(numeric::checked_apply(design_,particular_,guard)!=target_ || numeric::checked_multiply(design_,kernel_,guard).nonzeros())
            throw std::logic_error("complete graded transport family certificate failed");
        retained_=add(retained_,add(design_.nonzeros(),add(target_.size(),add(kernel_.nonzeros(),add(n,add(pivots_.size(),free_columns_.size()))))));
        admit(0,guard);steps_=guard.steps()-start;
    }
    const PreparedTransportObservations& model() const & {return observations_;}
    const std::vector<TransportObservation>& observations() const & {return samples_;}
    const TransportObservationSystem& evidence() const & {return evidence_;}
    const M& chain() const & {return chain_;}
    const M& design() const & {return design_;}
    const M& kernel() const & {return kernel_;}
    const V& target() const & {return target_;}
    const V& particular() const & {return particular_;}
    const std::vector<TransportChainConstraint>& chain_coordinates() const & {return constraints_;}
    const std::vector<std::size_t>& pivots() const & {return pivots_;}
    const std::vector<std::size_t>& free_columns() const & {return free_columns_;}
    const PreparedTransportObservations& model() const &&=delete;
    const std::vector<TransportObservation>& observations() const &&=delete;
    const TransportObservationSystem& evidence() const &&=delete;
    const M& chain() const &&=delete;
    const M& design() const &&=delete;
    const M& kernel() const &&=delete;
    const V& target() const &&=delete;
    const V& particular() const &&=delete;
    const std::vector<TransportChainConstraint>& chain_coordinates() const &&=delete;
    const std::vector<std::size_t>& pivots() const &&=delete;
    const std::vector<std::size_t>& free_columns() const &&=delete;
    std::size_t rank() const noexcept {return pivots_.size();}
    std::size_t chain_rank() const noexcept {return chain_rank_;}
    std::size_t observation_rank() const noexcept {return observation_rank_;}
    std::size_t retained_entries() const noexcept {return retained_;}
    std::size_t preparation_steps() const noexcept {return steps_;}

    V parameters(const V& free,Guard& guard) const {
        if(free.size()!=kernel_.cols())throw std::invalid_argument("every original free parameter value required");
        auto out=numeric::checked_apply(kernel_,guard.canonical_input(free),guard);
        for(std::size_t i=0;i<out.size();++i)out[i]=guard.add(out[i],particular_[i]);
        return out;
    }
    V entries(const V& parameters,Guard& guard) const {
        if(parameters.size()!=observations_.parameter_count())throw std::invalid_argument("complete original parameter field required");
        return observations_.parameter_basis()?numeric::checked_apply(*observations_.parameter_basis(),parameters,guard):guard.canonical_input(parameters);
    }
    local::OwnedLinearFiberSystem materialize(const V& free,Guard& guard) const {
        const auto values=entries(parameters(free,guard),guard);const auto& original=observations_.owner().fibers();
        admit(add(mul(retained_,3),mul(values.size(),4)),guard);
        local::LinearRelationalFiberSystem result(original.stalks());
        const auto k=observations_.grade();
        for(std::size_t g=1;g<original.complex().grade_count();++g)
            for(std::size_t p=0;p<original.complex().grade(grade(g)).incidence_count();++p) {
                guard.step();result.set_transport(grade(g),p,*original.transport_override(grade(g),p));
            }
        std::map<std::size_t,M> changed;
        for(std::size_t i=0;i<values.size();++i) {
            guard.step();const auto& s=observations_.variable_coordinates()[i];
            if(!changed.contains(s.occurrence))changed.emplace(s.occurrence,*result.transport_override(k,s.occurrence));
            auto& map=changed.at(s.occurrence);
            const auto q=guard.mul(coefficient(original.complex(),k,s.occurrence,guard),values[i]);
            map.set(s.row,s.column,guard.add(map.get(s.row,s.column),q));
        }
        for(auto& [p,map]:changed)result.set_transport(k,p,std::move(map));
        (void)boundaries(result,guard);return local::OwnedLinearFiberSystem(result);
    }
};
} // namespace rfc::learning
