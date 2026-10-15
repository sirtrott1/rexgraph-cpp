#pragma once
#include "rfc/core/axioms.hpp"
#include "rfc/numeric/algebraic_phase.hpp"
#include "rfc/numeric/rational_phase.hpp"
#include "rfc/temporal/factor_attribution.hpp"
#include "rfc/temporal/operation_word.hpp"
#include "rfc/topology/cyclic_cover.hpp"
#include <memory>
#include <type_traits>

namespace rfc::temporal {
struct ExactHistoryLimits {
    std::size_t max_steps{1024};
    std::size_t max_coordinate_slots{1000000};
    std::size_t max_integer_bits{16384};
    std::size_t max_source_cells{4096};
    std::size_t max_source_occurrences{65536};
    std::size_t max_interaction_terms{4095};
};
namespace history_detail {
inline void admit(const numeric::ExactMatrix& matrix,const ExactHistoryLimits& limits) {
    if (machine::checked_size_mul(matrix.rows(),matrix.cols())>limits.max_coordinate_slots)
        throw std::length_error("history matrix coordinate budget");
    for (const auto& entry:matrix.entries()) {
        const auto& q=entry.second;
        if (mpz_sizeinbase(mpq_numref(q.get_mpq_t()),2)>limits.max_integer_bits ||
            mpz_sizeinbase(mpq_denref(q.get_mpq_t()),2)>limits.max_integer_bits)
            throw std::length_error("history rational integer bit budget");
    }
}
inline void admit_source(const core::RelationalComplex& source,const ExactHistoryLimits& limits) {
    std::size_t cells{},occurrences{};
    for (std::size_t k=0;k<source.grade_count();++k) {
        const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        cells=machine::checked_size_add(cells,arena.cell_count());
        occurrences=machine::checked_size_add(occurrences,arena.incidence_count());
    }
    if (cells>limits.max_source_cells || occurrences>limits.max_source_occurrences)
        throw std::length_error("history source budget");
    if (!core::bianchi_through(source,source.max_grade()).satisfied())
        throw std::invalid_argument("history source fails complete chain family");
}
inline void admit_word(std::size_t factors,std::size_t dimension,const ExactHistoryLimits& limits) {
    if (machine::checked_size_mul(factors,machine::checked_size_mul(dimension,dimension))>limits.max_coordinate_slots)
        throw std::length_error("history word coordinate budget");
}
inline void admit_interactions(std::size_t factors,std::size_t dimension,const ExactHistoryLimits& limits) {
    if (factors>=63 || ((std::uint64_t{1}<<factors)-1)>limits.max_interaction_terms)
        throw std::length_error("history finite interaction term budget");
    admit_word(static_cast<std::size_t>((std::uint64_t{1}<<factors)-1),dimension,limits);
}
inline numeric::RationalPhase identity(const numeric::RationalPhase&) {return numeric::RationalPhase::identity();}
inline numeric::AlgebraicUnitPhase identity(const numeric::AlgebraicUnitPhase& phase) {
    return {phase.involution(),phase.involution().field().scalar(numeric::Rational{1})};
}
inline bool same_domain(const numeric::RationalPhase&,const numeric::RationalPhase&) noexcept {return true;}
inline bool same_domain(const numeric::AlgebraicUnitPhase& a,const numeric::AlgebraicUnitPhase& b) noexcept {
    return a.involution().same_owner(b.involution());
}
inline numeric::ExactMatrix action(const numeric::RationalPhase& phase) {return phase.real_action();}
inline numeric::ExactMatrix action(const numeric::AlgebraicUnitPhase& phase) {return phase.regular_action();}
} // namespace history_detail

template<class Phase> concept ExactPhase = std::same_as<Phase,numeric::RationalPhase> ||
                                          std::same_as<Phase,numeric::AlgebraicUnitPhase>;
template<ExactPhase Phase> struct PhaseOperation {
    core::IdentityKey identity{};
    Phase phase;
    bool active{true};
};

// Logical operation prefixes, not timestamps or durable publication records.
// Each begin() declares an owning source/cell carrier and phase coefficient
// domain. Equal values or independently declared source bytes do not identify it.
template<ExactPhase Phase> class PhaseOperationHistory {
    struct Carrier {
        core::RelationalComplex source;
        core::CellId cell;
        Phase initial;
        ExactHistoryLimits limits;
    };
    struct State {
        std::shared_ptr<const Carrier> carrier;
        std::vector<PhaseOperation<Phase>> operations;
        std::vector<Phase> endpoints;
    };
public:
    class Edit {
        friend class PhaseOperationHistory;
        Edit(std::shared_ptr<const State> base,std::size_t position,Phase value,bool active)
            :base_(std::move(base)),position_(position),value_(std::move(value)),active_(active) {}
        std::shared_ptr<const State> base_;
        std::size_t position_;
        Phase value_;
        bool active_;
    };
    class Endpoint {
        friend class PhaseOperationHistory;
        Endpoint(std::shared_ptr<const Carrier> carrier,std::size_t count,Phase value)
            :carrier_(std::move(carrier)),count_(count),value_(std::move(value)) {}
    public:
        const Phase& phase() const & noexcept {return value_;}
        const Phase& phase() const && = delete;
        const core::RelationalComplex& source() const & noexcept {return carrier_->source;}
        const core::RelationalComplex& source() const && = delete;
        core::CellId cell() const noexcept {return carrier_->cell;}
        std::size_t operation_count() const noexcept {return count_;}
    private:
        std::shared_ptr<const Carrier> carrier_;
        std::size_t count_;
        Phase value_;
    };
    PhaseOperationHistory(const PhaseOperationHistory&)=default;
    PhaseOperationHistory& operator=(const PhaseOperationHistory&)=default;
    static PhaseOperationHistory begin(const core::RelationalComplex& source,core::CellId cell,
                                      Phase initial,ExactHistoryLimits limits={}) {
        history_detail::admit_source(source,limits);
        if (cell.grade.value>=source.grade_count() || cell.index>=source.grade(cell.grade).cell_count())
            throw std::out_of_range("phase history carrier cell");
        history_detail::admit(history_detail::action(initial),limits);
        auto carrier=std::make_shared<const Carrier>(Carrier{source,cell,initial,limits});
        return PhaseOperationHistory(std::make_shared<const State>(State{std::move(carrier),{},{initial}}));
    }
    const core::RelationalComplex& source() const & noexcept {return state_->carrier->source;}
    const core::RelationalComplex& source() const && = delete;
    core::CellId cell() const noexcept {return state_->carrier->cell;}
    const std::vector<PhaseOperation<Phase>>& operations() const & noexcept {return state_->operations;}
    const std::vector<PhaseOperation<Phase>>& operations() const && = delete;
    const Phase& endpoint() const & noexcept {return state_->endpoints.back();}
    const Phase& endpoint() const && = delete;
    const Phase& phase_at(std::size_t prefix) const & {return state_->endpoints.at(prefix);}
    const Phase& phase_at(std::size_t) const && = delete;
    bool same_carrier(const PhaseOperationHistory& other) const noexcept {return state_->carrier==other.state_->carrier;}
    bool same_history(const PhaseOperationHistory& other) const noexcept {return state_==other.state_;}
    Endpoint observe() const {return Endpoint(state_->carrier,state_->operations.size(),endpoint());}
    PhaseOperationHistory append(PhaseOperation<Phase> operation) const {
        const auto& limits=state_->carrier->limits;
        if (state_->operations.size()>=limits.max_steps) throw std::length_error("phase history step budget");
        if (!history_detail::same_domain(state_->carrier->initial,operation.phase))
            throw std::invalid_argument("phase operation coefficient owner differs");
        history_detail::admit(history_detail::action(operation.phase),limits); // inactive values also admitted
        const auto next=operation.active ? operation.phase*endpoint() : endpoint();
        history_detail::admit(history_detail::action(next),limits);
        const auto coordinates=machine::checked_size_mul(state_->operations.size()+2,history_detail::action(next).rows());
        if (coordinates>limits.max_coordinate_slots) throw std::length_error("phase history endpoint coordinate budget");
        auto state=std::make_shared<State>(*state_);
        state->operations.push_back(std::move(operation)); state->endpoints.push_back(next);
        return PhaseOperationHistory(std::move(state));
    }
    PhaseOperationHistory prefix(std::size_t count) const {
        if (count>state_->operations.size()) throw std::out_of_range("phase history prefix");
        auto state=std::make_shared<State>(State{state_->carrier,{},{}});
        state->operations.assign(state_->operations.begin(),state_->operations.begin()+static_cast<std::ptrdiff_t>(count));
        state->endpoints.assign(state_->endpoints.begin(),state_->endpoints.begin()+static_cast<std::ptrdiff_t>(count+1));
        return PhaseOperationHistory(std::move(state));
    }
    Edit propose(std::size_t position,Phase replacement,bool active) const {
        (void)state_->operations.at(position);
        if (!history_detail::same_domain(state_->carrier->initial,replacement))
            throw std::invalid_argument("phase edit coefficient owner differs");
        history_detail::admit(history_detail::action(replacement),state_->carrier->limits);
        return Edit(state_,position,std::move(replacement),active);
    }
    PhaseOperationHistory apply(const Edit& edit) const {
        if (edit.base_!=state_) throw std::invalid_argument("phase edit base history differs");
        auto next=prefix(edit.position_);
        for (std::size_t i=edit.position_;i<state_->operations.size();++i) {
            auto op=state_->operations[i];
            if (i==edit.position_) {op.phase=edit.value_; op.active=edit.active_;}
            next=next.append(std::move(op));
        }
        return next;
    }
    numeric::ExactMatrix composite() const {
        const auto factors=actions();
        return factors.empty() ? numeric::ExactMatrix::identity(history_detail::action(endpoint()).rows()) :
                                 ordered_product(factors,FactorOrder::AppliedFirst);
    }
    std::vector<OrderedFactorInteraction> attribution(const PhaseOperationHistory& edited) const {
        aligned(edited); history_detail::admit_interactions(state_->operations.size(),history_detail::action(endpoint()).rows(),state_->carrier->limits);
        if (state_->operations.empty()) return {};
        auto terms=finite_ordered_factor_attribution(actions(),edited.actions(),FactorOrder::AppliedFirst);
        for (const auto& term:terms) history_detail::admit(term.contribution,state_->carrier->limits);
        return terms;
    }
    OperationWordDefect defect(const PhaseOperationHistory& edited) const {
        aligned(edited);
        const auto dimension=history_detail::action(endpoint()).rows();
        if (state_->operations.empty()) return {numeric::ExactMatrix(dimension,dimension),numeric::ExactMatrix(dimension,dimension),{}, {}};
        auto result=finite_word_defect(OperationWord(actions()),OperationWord(edited.actions()),
            std::vector<numeric::ExactMatrix>(state_->operations.size()+1,numeric::ExactMatrix::identity(dimension)));
        history_detail::admit(result.direct,state_->carrier->limits); history_detail::admit(result.expanded,state_->carrier->limits);
        for (const auto& term:result.contributions) history_detail::admit(term,state_->carrier->limits);
        return result;
    }
private:
    explicit PhaseOperationHistory(std::shared_ptr<const State> state):state_(std::move(state)) {}
    std::vector<numeric::ExactMatrix> actions() const {
        std::vector<numeric::ExactMatrix> result; result.reserve(state_->operations.size());
        const auto dimension=history_detail::action(endpoint()).rows();
        history_detail::admit_word(state_->operations.size(),dimension,state_->carrier->limits);
        for (const auto& operation:state_->operations)
            result.push_back(operation.active ? history_detail::action(operation.phase) : numeric::ExactMatrix::identity(dimension));
        return result;
    }
    void aligned(const PhaseOperationHistory& other) const {
        if (!same_carrier(other) || state_->operations.size()!=other.state_->operations.size())
            throw std::invalid_argument("phase history attribution carrier or length differs");
        for (std::size_t i=0;i<state_->operations.size();++i)
            if (state_->operations[i].identity!=other.state_->operations[i].identity)
                throw std::invalid_argument("phase history attribution operation identity differs");
    }
    std::shared_ptr<const State> state_;
};
using RationalPhaseHistory=PhaseOperationHistory<numeric::RationalPhase>;
using AlgebraicPhaseHistory=PhaseOperationHistory<numeric::AlgebraicUnitPhase>;

// This reading declares how the retained cyclic path acts in a phase domain.
// Its generator must respect the cover period. A nonfaithful action is allowed
// and never changes the winding of the source path. No spatial root is inferred.
template<ExactPhase Phase> class CyclicPhaseWalk {
public:
    CyclicPhaseWalk(const CyclicPhaseWalk&)=default;
    CyclicPhaseWalk& operator=(const CyclicPhaseWalk&)=default;
    CyclicPhaseWalk(topology::CyclicPathLift lift,Phase generator):lift_(std::move(lift)),generator_(std::move(generator)) {
        if (generator_.power(lift_.cover().period())!=history_detail::identity(generator_))
            throw std::invalid_argument("phase generator does not respect the declared cyclic period");
    }
    const topology::CyclicPathLift& lift() const & noexcept {return lift_;}
    const topology::CyclicPathLift& lift() const && = delete;
    const Phase& generator() const & noexcept {return generator_;}
    const Phase& generator() const && = delete;
    Phase phase_at(std::size_t prefix) const {return generator_.power(lift_.cover().residue(lift_.positions().at(prefix)));}
    Phase endpoint() const {return phase_at(lift_.path().steps().size());}
    CyclicPhaseWalk prefix(std::size_t count) const {return {lift_.prefix(count),generator_};}
private:
    topology::CyclicPathLift lift_;
    Phase generator_;
};
} // namespace rfc::temporal
