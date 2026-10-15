#pragma once
#include "rfc/inference/native_moments.hpp"

namespace rfc::inference {
struct NativeSelectedMultitimeObservation {
    std::vector<NativeFieldValue> inputs;
    NativeCarrier output;
    std::vector<std::size_t> selected;
    std::vector<numeric::RationalVector> channel_values;
    numeric::RationalVector aggregate;
    std::shared_ptr<const void> plan_owner;
    std::size_t execution_steps{};
};
struct NativeMultitimeObservation {
    std::vector<NativeFieldValue> channels;
    NativeFieldValue aggregate;
    std::shared_ptr<const void> plan_owner;
    std::size_t execution_steps{};
};

// x_{i+1}=F_i x_i+B_i u_i, followed by the declared terminal observation.
// Every suffix is an immutable shared action; no suffix matrix is constructed.
// Only [0,terminal) is examined, so a caller may retain a longer future plan.
class PreparedNativeMultitimeObservations {
    struct State {std::vector<NativeMomentChannel> channels;std::size_t terminal;};
    std::shared_ptr<const State> state_;
    explicit PreparedNativeMultitimeObservations(State state):state_(std::make_shared<const State>(std::move(state))){}
    std::vector<storage::PinnedNativeAccession> pins(const std::vector<NativeFieldValue>& inputs) const {
        if(inputs.size()!=state_->channels.size())throw std::invalid_argument("native multitime input count must match initial and selected innovations");
        std::vector<storage::PinnedNativeAccession> result;
        for(std::size_t i=0;i<inputs.size();++i){
            if(!inputs[i].carrier().same(state_->channels[i].operation.input()))throw std::invalid_argument("native multitime input has a different actual source carrier");
            for(const auto& pin:state_->channels[i].operation.publications())native_action_detail::append_pin(result,pin);
            for(const auto& pin:inputs[i].publications())native_action_detail::append_pin(result,pin);
        }
        return result;
    }
    std::size_t dimension_limit() const {
        auto dimension=std::numeric_limits<std::size_t>::max();
        for(const auto& channel:state_->channels)dimension=std::min(dimension,channel.operation.dimension_limit());
        return dimension;
    }
public:
    static PreparedNativeMultitimeObservations prepare(const std::vector<NativeFactoredAction>& evolution,
        const std::vector<NativeFactoredAction>& injections,std::size_t terminal,
        NativeFactoredAction initial,NativeFactoredAction observation) {
        if(terminal>evolution.size() || terminal>injections.size())throw std::out_of_range("native multitime terminal prefix");
        const auto count=machine::checked_size_add(terminal,1);
        std::vector<NativeFactoredAction> suffix;suffix.reserve(count);suffix.push_back(std::move(observation));
        for(std::size_t i=terminal;i-->0;)suffix.push_back(evolution[i].then(suffix.back()));
        std::reverse(suffix.begin(),suffix.end());
        std::vector<NativeMomentChannel> channels;channels.reserve(count);
        channels.push_back({"transported initial",initial.then(suffix.front())});
        for(std::size_t i=0;i<terminal;++i)channels.push_back({"innovation "+std::to_string(i),injections[i].then(suffix[i+1])});
        return PreparedNativeMultitimeObservations({std::move(channels),terminal});
    }
    const std::vector<NativeMomentChannel>& channels() const & {return state_->channels;}
    const std::vector<NativeMomentChannel>& channels() const &&=delete;
    std::size_t terminal() const noexcept {return state_->terminal;}
    PreparedNativeMoments moments(std::vector<NativeMomentPair> pairs) const {
        return PreparedNativeMoments(state_->channels,std::move(pairs));
    }
    NativeSelectedMultitimeObservation execute_selected(std::vector<NativeFieldValue> inputs,
        std::vector<std::size_t> coordinates,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;const auto selected_pins=pins(inputs);
        for(const auto& publication:selected_pins)native_action_detail::available(publication,availability);
        runtime::admit_execution({machine::checked_size_add(selected_pins.size(),inputs.size()),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,selected_pins,dimension_limit()),[&lease]{lease.check_cancelled();});
        guard.dimension(coordinates.size());guard.entries(coordinates.size());
        numeric::RationalVector aggregate(coordinates.size());std::vector<numeric::RationalVector> channel_values;
        channel_values.reserve(pin->channels.size());
        for(std::size_t i=0;i<pin->channels.size();++i){
            auto value=pin->channels[i].operation.selected_with_guard(inputs[i],coordinates,availability,lease,guard);
            for(std::size_t j=0;j<coordinates.size();++j)aggregate[j]=guard.add(aggregate[j],value[j]);
            channel_values.push_back(std::move(value));
            guard.entries(machine::checked_size_mul(machine::checked_size_add(channel_values.size(),1),coordinates.size()));
        }
        return {std::move(inputs),pin->channels.front().operation.output(),std::move(coordinates),std::move(channel_values),std::move(aggregate),pin,guard.steps()};
    }
    NativeMultitimeObservation execute(std::vector<NativeFieldValue> inputs,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;const auto selected_pins=pins(inputs);
        for(const auto& publication:selected_pins)native_action_detail::available(publication,availability);
        runtime::admit_execution({machine::checked_size_add(selected_pins.size(),inputs.size()),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,selected_pins,dimension_limit()),[&lease]{lease.check_cancelled();});
        std::vector<NativeFieldValue> values;values.reserve(pin->channels.size());
        for(std::size_t i=0;i<pin->channels.size();++i)values.push_back(pin->channels[i].operation.execute_with_guard(inputs[i],availability,lease,guard));
        auto total=values.front();std::size_t entries=total.sparse_values().nonzeros();
        for(std::size_t i=1;i<values.size();++i){total=total.plus(values[i],guard);entries=machine::checked_size_add(entries,values[i].sparse_values().nonzeros());}
        guard.entries(machine::checked_size_add(entries,total.sparse_values().nonzeros()));
        return {std::move(values),std::move(total),pin,guard.steps()};
    }
};

struct NativeMomentChange {
    numeric::Rational old_moment,new_moment,metric_term,left_term,right_term,mixed_term,direct,expanded;
    NativeFieldValue transported_left,transported_right,left_defect,right_defect;
    std::shared_ptr<const void> plan_owner;
    std::size_t execution_steps{};
};
// Exact finite four-term change. The forms may be nonsymmetric cross forms on
// different left/right carriers. All transports are explicitly declared.
class PreparedNativeMomentChange {
    struct State {
        NativeBilinearForm old_form,new_form;
        NativeFactoredAction left,right;
        std::vector<storage::PinnedNativeAccession> publications;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeMomentChange(State state):state_(std::make_shared<const State>(std::move(state))){}
public:
    static PreparedNativeMomentChange prepare(NativeBilinearForm old_form,NativeBilinearForm new_form,
        NativeFactoredAction left,NativeFactoredAction right) {
        if(!old_form.left().same(left.input()) || !new_form.left().same(left.output()) ||
           !old_form.right().same(right.input()) || !new_form.right().same(right.output()))
            throw std::invalid_argument("native moment change forms and declared transports bind different carriers");
        auto publications=old_form.publications();
        for(const auto* values:{&new_form.publications(),&left.publications(),&right.publications()})
            for(const auto& pin:*values)native_action_detail::append_pin(publications,pin);
        const auto dimension=std::min({old_form.dimension_limit(),new_form.dimension_limit(),left.dimension_limit(),right.dimension_limit()});
        return PreparedNativeMomentChange({std::move(old_form),std::move(new_form),std::move(left),std::move(right),std::move(publications),dimension});
    }
    NativeMomentChange execute(NativeFieldValue old_left,NativeFieldValue old_right,
        NativeFieldValue new_left,NativeFieldValue new_right,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;auto publications=pin->publications;
        for(const auto* value:{&old_left,&old_right,&new_left,&new_right})
            for(const auto& publication:value->publications())native_action_detail::append_pin(publications,publication);
        for(const auto& publication:publications)native_action_detail::available(publication,availability);
        runtime::admit_execution({machine::checked_size_add(publications.size(),4),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,publications,pin->dimension_limit),[&lease]{lease.check_cancelled();});
        const auto transported_left=pin->left.execute_with_guard(old_left,availability,lease,guard);
        const auto transported_right=pin->right.execute_with_guard(old_right,availability,lease,guard);
        const auto dx=new_left.plus(transported_left.scaled(-1,guard),guard),dy=new_right.plus(transported_right.scaled(-1,guard),guard);
        const auto old_value=pin->old_form.pair(old_left,old_right,guard),new_value=pin->new_form.pair(new_left,new_right,guard);
        const auto metric=guard.sub(pin->new_form.pair(transported_left,transported_right,guard),old_value);
        const auto left_value=pin->new_form.pair(dx,transported_right,guard),right_value=pin->new_form.pair(transported_left,dy,guard),mixed=pin->new_form.pair(dx,dy,guard);
        const auto expanded=guard.add(guard.add(metric,left_value),guard.add(right_value,mixed)),direct=guard.sub(new_value,old_value);
        if(expanded!=direct)throw std::logic_error("native four-term moment-change identity failed");
        return {old_value,new_value,metric,left_value,right_value,mixed,direct,expanded,transported_left,transported_right,dx,dy,pin,guard.steps()};
    }
};

// Coefficients are in ascending powers. Horner's rule retains the operation's
// actual owner and factors; only the requested finite polynomial is executed.
inline NativeFactoredAction native_polynomial(const NativeFactoredAction& operation,
    numeric::RationalVector coefficients,numeric::ExactActionLimits limits={}) {
    if(!operation.input().same(operation.output()))throw std::invalid_argument("native polynomial requires an actual endomorphism");
    numeric::ExactActionGuard guard(limits);coefficients=guard.canonical_input(coefficients);guard.entries(coefficients.size());
    const auto identity=NativeFactoredAction::identity(operation.input(),operation.dimension_limit());
    if(coefficients.empty())return identity.scaled(0);
    auto result=identity.scaled(coefficients.back());
    for(std::size_t i=coefficients.size()-1;i-->0;)result=operation.then(result).plus(identity.scaled(coefficients[i]));
    return result;
}
// Exact [t^n] exp(t O)=O^n/n! in the formal series ring. This returns a finite
// coefficient action, and makes no claim to evaluate an analytic exponential.
inline NativeFactoredAction native_exponential_series_coefficient(const NativeFactoredAction& operation,
    std::size_t degree,numeric::ExactActionLimits limits={}) {
    numeric::ExactActionGuard guard(limits);numeric::Rational factorial=1;
    for(std::size_t i=0;i<degree;++i)factorial=guard.mul(factorial,numeric::Rational{std::to_string(i+1)});
    return native_power(operation,degree).scaled(guard.div(1,factorial));
}
} // namespace rfc::inference
