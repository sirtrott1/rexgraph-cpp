#pragma once
#include "rfc/field/execution_tower.hpp"
#include "rfc/field/factored_action.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"

namespace rfc::field {
// Shares source, sparse boundary factors, and exact metric solves across a
// whole family. No Hodge, inverse, Dirac or projector matrix is constructed.
class PreparedOperatorFamily {
    using Guard=numeric::ExactActionGuard;
    struct State {
        PreparedExactExecutionTower tower;
        std::vector<FactoredAction> boundary,metric,inverse_metric;
        std::size_t preparation{};
        State(PreparedExactExecutionTower owner,Guard& guard):tower(std::move(owner)) {
            const auto start=guard.steps();const auto source=tower.source();
            hodge_frame_detail::source_values(*source,guard);
            for(std::size_t k=0;k<source->grade_count();++k){
                const auto n=source->grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
                auto form=tower.metrics()[k].materialize();guard.matrix(form);
                metric.push_back(FactoredAction::matrix(form));
                inverse_metric.push_back(FactoredAction::solve(numeric::PreparedExactSolve(std::move(form),guard)));
                boundary.push_back(FactoredAction::matrix(k?hodge_frame_detail::boundary(*source,core::Grade{static_cast<std::uint16_t>(k)},guard):numeric::ExactMatrix(0,n)));
            }
            preparation=guard.steps()-start;
        }
    };
    std::shared_ptr<const State> state_;
public:
    PreparedOperatorFamily(PreparedExactExecutionTower tower,Guard& guard)
        :state_(std::make_shared<const State>(std::move(tower),guard)){}
    PreparedOperatorFamily(PreparedExactExecutionTower tower,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        Guard guard(limits,std::move(check));state_=std::make_shared<const State>(std::move(tower),guard);
    }
    const PreparedExactExecutionTower& tower() const & {return state_->tower;}
    const PreparedExactExecutionTower& tower() const &&=delete;
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
    void validate_source(numeric::ExactActionGuard& guard) const {
        hodge_frame_detail::source_values(*state_->tower.source(),guard);
        for(const auto& metric:state_->tower.metrics())guard.matrix(metric.materialize());
    }
    FactoredAction metric(core::Grade k) const {return state_->metric.at(k.value);}
    FactoredAction inverse_metric(core::Grade k) const {return state_->inverse_metric.at(k.value);}
    FactoredAction boundary(core::Grade k) const {
        if(!k.value)throw std::invalid_argument("grade zero has no boundary");
        return state_->boundary.at(k.value);
    }
    FactoredAction adjoint(core::Grade k) const {
        if(!k.value)throw std::invalid_argument("grade zero has no adjoint");
        return metric(core::Grade{static_cast<std::uint16_t>(k.value-1u)}).then(boundary(k).transpose()).then(inverse_metric(k));
    }
    FactoredAction action(OperatorRequest op,Guard& guard) const {
        const auto pin=state_;PreparedExactExecutionTower::validate_request(op,pin->tower.profile());
        const auto k=op.grade;const auto source=pin->tower.source();
        if(k.value>=source->grade_count())throw std::out_of_range("operator family grade");
        const auto n=source->grade(k).cell_count();
        if(op.kind==OperatorKind::Boundary)return boundary(k);
        if(op.kind==OperatorKind::Adjoint)return adjoint(k);
        const auto zero=FactoredAction::matrix(numeric::ExactMatrix(n,n));
        const auto down=k.value?boundary(k).then(adjoint(k)):zero;
        const auto up=k.value+1u<source->grade_count()?adjoint(core::Grade{static_cast<std::uint16_t>(k.value+1u)}).then(boundary(core::Grade{static_cast<std::uint16_t>(k.value+1u)})):zero;
        if(op.kind==OperatorKind::HodgeDown)return down;
        if(op.kind==OperatorKind::HodgeUp)return up;
        if(op.kind==OperatorKind::Hodge)return down.plus(up);
        // Green, harmonic inverse and projector are metric self-adjoint.
        // Potential B L# has transpose (L#)^T B^T.
        if(op.kind==OperatorKind::GradientPotential){
            auto inverse=action({OperatorKind::HarmonicInverse,k,0},guard);
            return inverse.then(boundary(k));
        }
        auto prepared=pin->tower.prepare_action(op,guard);
        const auto input_metric=metric(k),input_inverse=inverse_metric(k);
        const auto output_grade=prepared.output_grade();const auto output_size=prepared.output_size();
        (void)output_grade;
        struct Owner {std::shared_ptr<const State> family;PreparedGlobalAction solve;FactoredAction metric,inverse;};
        const auto entries=prepared.retained_entries()+input_metric.retained_entries()+input_inverse.retained_entries();
        return FactoredAction::owned(Owner{pin,std::move(prepared),input_metric,input_inverse},output_size,n,entries,
            [](const Owner& a,const auto& x,auto& g){return a.solve.apply(x,g);},
            [](const Owner& a,const auto& x,auto& g){return a.metric.apply(a.solve.apply(a.inverse.apply(x,g),g),g);});
    }
    FactoredAction action(OperatorRequest op,numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return action(std::move(op),guard);
    }
};
} // namespace rfc::field
