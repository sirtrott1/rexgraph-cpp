#pragma once
#include "rfc/learning/transport_observations.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"

namespace rfc::learning {
struct TransportParameterRelationTerm {
    std::size_t entry{};
    numeric::Rational coefficient;
};
struct TransportParameterRelation {
    std::string name;
    std::vector<TransportParameterRelationTerm> terms;
    numeric::Rational expected;
};
enum class TransportParameterRelationStatus { Forced, Excluded, Open };
enum class TransportParameterSupportPolicy { VariableEntries, IncludeConstants };
struct TransportParameterCondition {
    bool consistent{};
    numeric::RationalVector origin, coordinate_origin;
    numeric::ExactMatrix kernel, coordinate_kernel;
    std::vector<std::size_t> free_columns;
    std::size_t rank{}, steps{};
};
struct TransportParameterConditionStatus {
    bool consistent{};
    std::size_t rank{}, steps{};
};

// Evaluate declared linear relations on supported map entries throughout the
// complete observation family. A declared parameter basis remains an assumption.
class PreparedTransportParameterRelations {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    PreparedTransportObservations observations_;
    std::vector<TransportObservation> samples_;
    std::vector<TransportParameterRelation> relations_;
    V origin_, residual_origin_;
    M kernel_, residual_directions_;
    std::vector<std::size_t> free_columns_;
    std::vector<TransportParameterRelationStatus> statuses_;
    std::size_t rank_{}, retained_{}, preparation_steps_{};
    void workspace(std::size_t count,Guard& guard) const {
        guard.entries(machine::checked_size_add(retained_,count));
    }
    M product(const M& a,const M& b,Guard& guard,std::size_t live=0) const {
        if(a.cols()!=b.rows()) throw std::logic_error("parameter relation product dimensions");
        guard.matrix(a);guard.matrix(b);
        const auto scratch=machine::checked_size_add(live,machine::checked_size_add(b.rows(),
            machine::checked_size_add(a.nonzeros(),machine::checked_size_mul(b.nonzeros(),2))));
        workspace(scratch,guard);
        std::vector<std::vector<std::pair<std::size_t,Q>>> rows(b.rows());
        for(const auto& [key,q]:b.entries()) {guard.step();rows[key.first].emplace_back(key.second,q);}
        M out(a.rows(),b.cols());
        for(const auto& [key,q]:a.entries()) for(const auto& [j,v]:rows[key.second]) {
            out.set(key.first,j,guard.add(out.get(key.first,j),guard.mul(q,v)));
            workspace(machine::checked_size_add(scratch,out.nonzeros()),guard);
        }
        return out;
    }
    struct ConditionFrames {
        M map;
        numeric::PreparedExactFrames frames, target;
        ConditionFrames(M a,M b,Guard& guard):map(std::move(a)),frames(map,guard),target(std::move(b),guard) {}
    };
    ConditionFrames condition_frames(const std::vector<std::size_t>& selected,Guard& guard) const {
        guard.step();guard.dimension(selected.size());
        guard.vector(origin_);guard.matrix(kernel_);guard.vector(residual_origin_);guard.matrix(residual_directions_);
        const auto d=kernel_.cols();guard.dimension(machine::checked_size_add(d,1));
        workspace(machine::checked_size_add(selected.size(),relations_.size()),guard);
        M map(selected.size(),d),augmented(selected.size(),machine::checked_size_add(d,1));
        std::vector<std::size_t> rows(relations_.size(),selected.size());
        for(std::size_t i=0;i<selected.size();++i) {
            guard.step();const auto row=selected[i];
            if(row>=relations_.size() || rows[row]!=selected.size())
                throw std::invalid_argument("distinct selected original parameter relation indices required");
            rows[row]=i;augmented.set(i,d,guard.sub(Q{0},residual_origin_[row]));
        }
        for(const auto& [key,q]:residual_directions_.entries()) {
            guard.step();if(rows[key.first]==selected.size()) continue;
            map.set(rows[key.first],key.second,q);augmented.set(rows[key.first],key.second,q);
            workspace(machine::checked_size_add(relations_.size(),machine::checked_size_add(map.nonzeros(),augmented.nonzeros())),guard);
        }
        ConditionFrames out(std::move(map),std::move(augmented),guard);
        workspace(machine::checked_size_add(relations_.size(),machine::checked_size_add(out.map.nonzeros(),
            machine::checked_size_add(out.target.original().nonzeros(),
                machine::checked_size_add(out.frames.stored_entries(),out.target.stored_entries())))),guard);
        return out;
    }
public:
    PreparedTransportParameterRelations(PreparedTransportObservations observations,
        std::vector<TransportObservation> samples,std::vector<TransportParameterRelation> relations,Guard& guard,
        TransportParameterSupportPolicy support_policy=TransportParameterSupportPolicy::VariableEntries)
        : observations_(std::move(observations)),samples_(std::move(samples)),relations_(std::move(relations)) {
        observations_.require_grade_one_profile();
        const auto start=guard.steps();guard.step();
        guard.dimension(relations_.size());
        if(relations_.empty()) throw std::invalid_argument("named parameter relations required");
        retained_=observations_.retained_entries();
        for(auto& sample:samples_) {
            retained_=machine::checked_size_add(retained_,machine::checked_size_add(sample.input.size(),
                machine::checked_size_add(sample.target.size(),sample.observed.size())));workspace(0,guard);
            sample.input=guard.canonical_input(sample.input);sample.target=guard.canonical_input(sample.target);
        }
        {
            const auto system=observations_.assemble(samples_,guard);
            const auto live=machine::checked_size_add(system.design.nonzeros(),machine::checked_size_add(system.augmented.nonzeros(),
                machine::checked_size_mul(system.target.size(),4)));
            workspace(live,guard);
            const numeric::PreparedExactFrames frames(system.design,guard),augmented(system.augmented,guard);
            workspace(machine::checked_size_add(live,machine::checked_size_add(frames.stored_entries(),augmented.stored_entries())),guard);
            if(frames.rank()!=augmented.rank()) throw std::invalid_argument("inconsistent original transport observations");
            rank_=frames.rank();free_columns_=frames.free_columns();kernel_=frames.kernel();
            origin_=V(observations_.parameter_count());
            for(std::size_t i=0;i<augmented.rank();++i) {
                guard.step();origin_[augmented.pivots()[i]]=augmented.reduced().get(i,origin_.size());
            }
            if(numeric::checked_apply(system.design,origin_,guard)!=system.target)
                throw std::logic_error("parameter evidence origin certificate");
            retained_=machine::checked_size_add(retained_,machine::checked_size_add(origin_.size(),
                    machine::checked_size_add(kernel_.nonzeros(),free_columns_.size())));
            workspace(machine::checked_size_add(live,machine::checked_size_add(frames.stored_entries(),augmented.stored_entries())),guard);
        }
        const std::size_t live{};
        const auto count=observations_.variable_coordinates().size();M entry_map(relations_.size(),count);
        std::set<std::string> names;
        for(std::size_t i=0;i<relations_.size();++i) {
            guard.step();auto& relation=relations_[i];
            if(relation.name.empty() || relation.name.size()>256 || !names.insert(relation.name).second ||
                (relation.terms.empty() && support_policy!=TransportParameterSupportPolicy::IncludeConstants))
                throw std::invalid_argument("distinct bounded names and nonempty parameter relation support required");
            relation.expected=numeric::canonical_rational(relation.expected);guard.value(relation.expected);
            retained_=machine::checked_size_add(retained_,machine::checked_size_add(relation.name.size(),2));
            std::set<std::size_t> support;
            for(auto& term:relation.terms) {
                guard.step();term.coefficient=numeric::canonical_rational(term.coefficient);guard.value(term.coefficient);
                if(term.entry>=count || !support.insert(term.entry).second)
                    throw std::invalid_argument("distinct original supported variable map entries required");
                retained_=machine::checked_size_add(retained_,2);entry_map.set(i,term.entry,term.coefficient);
                workspace(machine::checked_size_add(live,entry_map.nonzeros()),guard);
            }
        }
        const auto map=observations_.parameter_basis() ? product(entry_map,*observations_.parameter_basis(),guard,live) : entry_map;
        residual_origin_=numeric::checked_apply(map,origin_,guard);
        for(std::size_t i=0;i<relations_.size();++i) residual_origin_[i]=guard.sub(residual_origin_[i],relations_[i].expected);
        residual_directions_=product(map,kernel_,guard,machine::checked_size_add(live,residual_origin_.size()));
        statuses_.resize(relations_.size());std::vector<bool> moving(relations_.size());
        for(const auto& [key,q]:residual_directions_.entries()) {(void)q;guard.step();moving[key.first]=true;}
        for(std::size_t i=0;i<relations_.size();++i) {
            guard.step();statuses_[i]=moving[i] ? TransportParameterRelationStatus::Open :
                (residual_origin_[i]==0 ? TransportParameterRelationStatus::Forced : TransportParameterRelationStatus::Excluded);
        }
        retained_=machine::checked_size_add(retained_,machine::checked_size_add(residual_origin_.size(),
            machine::checked_size_add(residual_directions_.nonzeros(),statuses_.size())));
        workspace(machine::checked_size_add(live,machine::checked_size_add(entry_map.nonzeros(),map.nonzeros())),guard);
        preparation_steps_=guard.steps()-start;
    }
    const PreparedTransportObservations& observations() const & {return observations_;}
    const PreparedTransportObservations& observations() const &&=delete;
    const std::vector<TransportParameterRelation>& relations() const & {return relations_;}
    const std::vector<TransportParameterRelation>& relations() const &&=delete;
    const V& evidence_origin() const & {return origin_;}
    const V& evidence_origin() const &&=delete;
    const M& evidence_kernel() const & {return kernel_;}
    const M& evidence_kernel() const &&=delete;
    const V& residual_origin() const & {return residual_origin_;}
    const V& residual_origin() const &&=delete;
    const M& residual_directions() const & {return residual_directions_;}
    const M& residual_directions() const &&=delete;
    const std::vector<TransportParameterRelationStatus>& statuses() const & {return statuses_;}
    const std::vector<TransportParameterRelationStatus>& statuses() const &&=delete;
    const std::vector<std::size_t>& free_columns() const & {return free_columns_;}
    const std::vector<std::size_t>& free_columns() const &&=delete;
    std::size_t rank() const noexcept {return rank_;}
    std::size_t retained_entries() const noexcept {return retained_;}
    std::size_t preparation_steps() const noexcept {return preparation_steps_;}

    // Classify a joint intersection without materializing its parameter family.
    TransportParameterConditionStatus condition_status(const std::vector<std::size_t>& selected,Guard& guard) const {
        const auto start=guard.steps();const auto system=condition_frames(selected,guard);
        return {system.frames.rank()==system.target.rank(),system.frames.rank(),guard.steps()-start};
    }

    // Intersect explicitly selected equations with the original evidence family.
    // Individual feasibility does not imply that a selected set is consistent.
    TransportParameterCondition condition(const std::vector<std::size_t>& selected,Guard& guard) const {
        const auto start=guard.steps();const auto system=condition_frames(selected,guard);
        const auto& map=system.map;const auto& frames=system.frames;const auto& target=system.target;
        const auto d=kernel_.cols();
        TransportParameterCondition out;out.rank=frames.rank();out.consistent=frames.rank()==target.rank();
        out.kernel=M(origin_.size(),0);out.coordinate_kernel=M(d,0);
        if(out.consistent) {
            out.coordinate_origin=V(d);out.coordinate_kernel=frames.kernel();out.free_columns=frames.free_columns();
            for(std::size_t i=0;i<target.rank();++i) {guard.step();out.coordinate_origin[target.pivots()[i]]=target.reduced().get(i,d);}
            out.origin=numeric::checked_apply(kernel_,out.coordinate_origin,guard);
            for(std::size_t i=0;i<origin_.size();++i) out.origin[i]=guard.add(out.origin[i],origin_[i]);
            out.kernel=product(kernel_,out.coordinate_kernel,guard,machine::checked_size_add(out.origin.size(),out.coordinate_origin.size()));
            const auto witness=numeric::checked_apply(map,out.coordinate_origin,guard);
            for(std::size_t i=0;i<selected.size();++i) if(guard.add(witness[i],residual_origin_[selected[i]])!=0)
                throw std::logic_error("conditional parameter relation origin certificate");
            if(product(map,out.coordinate_kernel,guard).nonzeros())
                throw std::logic_error("conditional parameter relation kernel certificate");
        }
        workspace(machine::checked_size_add(out.origin.size(),machine::checked_size_add(out.coordinate_origin.size(),
            machine::checked_size_add(out.kernel.nonzeros(),out.coordinate_kernel.nonzeros()))),guard);
        out.steps=guard.steps()-start;return out;
    }
};
} // namespace rfc::learning
