#pragma once
#include "rfc/inference/native_factored_action.hpp"

namespace rfc::inference {
class NativeSourceGreenResponse {
    std::shared_ptr<const void> owner_;
    friend class PreparedNativeSourceGreenResponse;
    NativeSourceGreenResponse(NativeFieldValue old_value,NativeFieldValue new_value,NativeFieldValue innovation,
        NativeFieldValue realization,NativeFieldValue source_change,NativeFieldValue operator_change,NativeFieldValue total,
        std::size_t steps,std::shared_ptr<const void> owner):owner_(std::move(owner)),old_response(std::move(old_value)),
        new_response(std::move(new_value)),coordinate_innovation(std::move(innovation)),realization_term(std::move(realization)),
        innovation_term(std::move(source_change)),operator_term(std::move(operator_change)),aggregate(std::move(total)),execution_steps(steps){}
public:
    const NativeFieldValue old_response,new_response,coordinate_innovation;
    const NativeFieldValue realization_term,innovation_term,operator_term,aggregate;
    const std::size_t execution_steps;
    std::shared_ptr<const void> plan_owner() const {return owner_;}
};
struct NativeSourceObservedGreenResponse {
    NativeSourceGreenResponse response;
    NativeFieldValue accession_term,realization_term,innovation_term,operator_term,aggregate;
};
// Source-factor version of the interval Green identity. It adopts native
// towers and prepared Green factors, not resolving-operator tensor snapshots.
class PreparedNativeSourceGreenResponse {
    struct State {
        PreparedNativeOperatorFamily old_family,new_family;
        NativeFactoredAction old_injection,new_injection,ambient,source;
        NativeFactoredAction old_hodge,new_hodge,old_green,new_green;
        numeric::Rational lambda;
        std::vector<storage::PinnedNativeAccession> publications;
        std::size_t dimension_limit,preparation_steps;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeSourceGreenResponse(State state):state_(std::make_shared<const State>(std::move(state))){}
public:
    static PreparedNativeSourceGreenResponse prepare(PreparedNativeOperatorFamily old_family,
        PreparedNativeOperatorFamily new_family,core::Grade grade,
        storage::PinnedNativeAccession old_injection,storage::PinnedNativeAccession new_injection,
        NativeFactoredAction ambient,NativeFactoredAction source,numeric::Rational lambda,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        if(old_injection.tensor_declaration()->role!=storage::TemporalTensorRole::SourceRealization ||
           new_injection.tensor_declaration()->role!=storage::TemporalTensorRole::SourceRealization)
            throw std::invalid_argument("source Green requires declared source realizations");
        auto I=NativeFactoredAction::tensor(old_injection,old_family.dimension_limit()),Ip=NativeFactoredAction::tensor(new_injection,new_family.dimension_limit());
        const auto old_X=old_family.carrier(grade),new_X=new_family.carrier(grade);
        if(!I.output().same(old_X) || !Ip.output().same(new_X) || !ambient.input().same(old_X) ||
           !ambient.output().same(new_X) || !source.input().same(I.input()) || !source.output().same(Ip.input()))
            throw std::invalid_argument("source Green factors must bind their actual native carriers");
        lambda=numeric::canonical_rational(lambda);
        auto pins=old_family.publications();
        for(const auto& publication:new_family.publications())native_action_detail::append_pin(pins,publication);
        for(const auto& action:{I,Ip,ambient,source})for(const auto& pin:action.publications())native_action_detail::append_pin(pins,pin);
        for(const auto& pin:pins)native_action_detail::available(pin,availability);
        const auto dimension=std::min({old_family.dimension_limit(),new_family.dimension_limit(),ambient.dimension_limit(),source.dimension_limit()});
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pins,dimension),[&lease]{lease.check_cancelled();});guard.value(lambda);
        auto L=old_family.action_with_guard({field::OperatorKind::Hodge,grade,0},availability,lease,guard);
        auto Lp=new_family.action_with_guard({field::OperatorKind::Hodge,grade,0},availability,lease,guard);
        auto G=old_family.action_with_guard({field::OperatorKind::Green,grade,lambda},availability,lease,guard);
        auto Gp=new_family.action_with_guard({field::OperatorKind::Green,grade,lambda},availability,lease,guard);
        return PreparedNativeSourceGreenResponse({std::move(old_family),std::move(new_family),std::move(I),std::move(Ip),
            std::move(ambient),std::move(source),std::move(L),std::move(Lp),std::move(G),std::move(Gp),std::move(lambda),std::move(pins),dimension,guard.steps()});
    }
    const PreparedNativeOperatorFamily& old_family() const & {return state_->old_family;}
    const PreparedNativeOperatorFamily& new_family() const & {return state_->new_family;}
    const NativeFactoredAction& old_green() const & {return state_->old_green;}
    const NativeFactoredAction& new_green() const & {return state_->new_green;}
    const NativeFactoredAction& ambient() const & {return state_->ambient;}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    NativeSourceGreenResponse execute(NativeFieldValue old_input,NativeFieldValue new_input,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;lease.check_cancelled();
        auto pins=pin->publications;
        for(const auto& x:old_input.publications())native_action_detail::append_pin(pins,x);
        for(const auto& x:new_input.publications())native_action_detail::append_pin(pins,x);
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pins,pin->dimension_limit),[&lease]{lease.check_cancelled();});
        const auto run=[&](const NativeFactoredAction& action,const NativeFieldValue& input){return action.execute_with_guard(input,availability,lease,guard);};
        const auto difference=[&](const NativeFieldValue& a,const NativeFieldValue& b){return a.plus(b.scaled(-1,guard),guard);};
        const auto j=run(pin->old_injection,old_input),jp=run(pin->new_injection,new_input);
        const auto g=run(pin->old_green,j),gp=run(pin->new_green,jp);
        const auto zeta=difference(new_input,run(pin->source,old_input));
        const auto realization=run(pin->new_green,difference(run(pin->new_injection,run(pin->source,old_input)),run(pin->ambient,j)));
        const auto innovation=run(pin->new_green,run(pin->new_injection,zeta));
        const auto operator_defect=difference(run(pin->new_hodge,run(pin->ambient,g)),run(pin->ambient,run(pin->old_hodge,g)));
        const auto operation=run(pin->new_green,operator_defect).scaled(-pin->lambda,guard);
        const auto aggregate=realization.plus(innovation,guard).plus(operation,guard);
        if(!field::factored_action_detail::equal(aggregate.sparse_values(),difference(gp,run(pin->ambient,g)).sparse_values()))
            throw std::logic_error("source-factor interval Green identity failed");
        lease.check_cancelled();return {g,gp,zeta,realization,innovation,operation,aggregate,guard.steps(),pin};
    }
    NativeSourceGreenResponse execute(storage::PinnedNativeAccession old_input,storage::PinnedNativeAccession new_input,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        return execute(NativeFieldValue::stored(std::move(old_input)),NativeFieldValue::stored(std::move(new_input)),availability,lease);
    }
    NativeSourceObservedGreenResponse observe(const NativeSourceGreenResponse& response,
        const NativeFactoredAction& old_observation,const NativeFactoredAction& new_observation,
        const NativeFactoredAction& output_correspondence,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;
        if(response.owner_.get()!=pin.get() || !old_observation.input().same(pin->ambient.input()) || !new_observation.input().same(pin->ambient.output()) ||
           !output_correspondence.input().same(old_observation.output()) || !output_correspondence.output().same(new_observation.output()) ||
           !response.old_response.carrier().same(pin->ambient.input()) || !response.new_response.carrier().same(pin->ambient.output()))
            throw std::invalid_argument("observed source Green carrier binding");
        auto pins=pin->publications;
        for(const auto& action:{old_observation,new_observation,output_correspondence})
            for(const auto& x:action.publications())native_action_detail::append_pin(pins,x);
        for(const auto& value:{response.old_response,response.new_response,response.aggregate})
            for(const auto& x:value.publications())native_action_detail::append_pin(pins,x);
        const auto dimension=std::min({pin->dimension_limit,old_observation.dimension_limit(),new_observation.dimension_limit(),output_correspondence.dimension_limit()});
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pins,dimension),[&lease]{lease.check_cancelled();});
        const auto run=[&](const NativeFactoredAction& action,const NativeFieldValue& input){return action.execute_with_guard(input,availability,lease,guard);};
        const auto old_y=run(output_correspondence,run(old_observation,response.old_response));
        const auto accession=run(new_observation,run(pin->ambient,response.old_response)).plus(old_y.scaled(-1,guard),guard);
        const auto realization=run(new_observation,response.realization_term),innovation=run(new_observation,response.innovation_term),
                   operation=run(new_observation,response.operator_term);
        const auto aggregate=accession.plus(realization,guard).plus(innovation,guard).plus(operation,guard);
        if(!field::factored_action_detail::equal(aggregate.sparse_values(),run(new_observation,response.new_response).plus(old_y.scaled(-1,guard),guard).sparse_values()))
            throw std::logic_error("source-factor observed Green identity failed");
        return {response,accession,realization,innovation,operation,aggregate};
    }
};
} // namespace rfc::inference
