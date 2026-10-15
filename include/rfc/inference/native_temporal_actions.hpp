#pragma once
#include "rfc/inference/native_factored_action.hpp"

namespace rfc::inference {
struct NativeSelectedWordDefect {
    NativeFieldValue input;
    NativeCarrier output;
    std::vector<std::size_t> selected;
    std::vector<numeric::RationalVector> contributions;
    numeric::RationalVector direct,expanded;
    std::shared_ptr<const void> plan_owner;
    std::size_t execution_steps{};
};
class PreparedNativeWordDefect {
    struct State {
        std::vector<NativeFactoredAction> old_word,new_word,correspondences,defects,contributions;
        NativeFactoredAction direct;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeWordDefect(State state):state_(std::make_shared<const State>(std::move(state))){}
public:
    static PreparedNativeWordDefect prepare(std::vector<NativeFactoredAction> old_word,
        std::vector<NativeFactoredAction> new_word,std::vector<NativeFactoredAction> correspondences) {
        if(old_word.size()!=new_word.size() || correspondences.size()!=old_word.size()+1)
            throw std::invalid_argument("native word defect requires corresponding positions and every intermediate carrier map");
        auto old_prefix=NativeFactoredAction::identity(correspondences.front().input(),correspondences.front().dimension_limit());
        std::vector<NativeFactoredAction> suffix;
        suffix.reserve(machine::checked_size_add(new_word.size(),1));
        suffix.push_back(NativeFactoredAction::identity(correspondences.back().output(),correspondences.back().dimension_limit()));
        for(std::size_t i=new_word.size();i-->0;)suffix.push_back(new_word[i].then(suffix.back()));
        std::reverse(suffix.begin(),suffix.end());
        std::vector<NativeFactoredAction> defects,contributions;
        for(std::size_t i=0;i<old_word.size();++i){
            auto defect=correspondences[i].then(new_word[i]).plus(old_word[i].then(correspondences[i+1]).scaled(-1));
            auto term=old_prefix.then(defect).then(suffix[i+1]);
            defects.push_back(std::move(defect));contributions.push_back(std::move(term));
            old_prefix=old_prefix.then(old_word[i]);
        }
        auto direct=correspondences.front().then(suffix.front()).plus(old_prefix.then(correspondences.back()).scaled(-1));
        return PreparedNativeWordDefect({std::move(old_word),std::move(new_word),std::move(correspondences),
            std::move(defects),std::move(contributions),std::move(direct)});
    }
    const std::vector<NativeFactoredAction>& position_defects() const & {return state_->defects;}
    const std::vector<NativeFactoredAction>& contributions() const & {return state_->contributions;}
    const NativeFactoredAction& direct_action() const & {return state_->direct;}
    NativeSelectedWordDefect execute_selected(NativeFieldValue input,std::vector<std::size_t> selected,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;auto pins=pin->direct.publications();
        for(const auto& root:input.publications())native_action_detail::append_pin(pins,root);
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pins,pin->direct.dimension_limit()),[&lease]{lease.check_cancelled();});
        const auto direct=pin->direct.selected_with_guard(input,selected,availability,lease,guard);
        numeric::RationalVector expanded(selected.size());std::vector<numeric::RationalVector> terms;
        for(const auto& action:pin->contributions){
            auto term=action.selected_with_guard(input,selected,availability,lease,guard);
            for(std::size_t i=0;i<term.size();++i)expanded[i]=guard.add(expanded[i],term[i]);
            terms.push_back(std::move(term));
        }
        if(expanded!=direct)throw std::logic_error("native selected word telescoping identity failed");
        return {std::move(input),pin->direct.output(),std::move(selected),std::move(terms),direct,expanded,pin,guard.steps()};
    }
};
// The declared reconstruction and observation may change dimension. Actual
// carrier composition checks every map, sector and temporal interface.
inline NativeFactoredAction native_sector_transport(const NativeFactoredAction& old_sector,
    const NativeFactoredAction& new_sector,const NativeFactoredAction& correspondence) {
    return old_sector.then(correspondence).then(new_sector);
}
inline NativeFactoredAction native_typed_sector_transport(const NativeFactoredAction& reconstruction,
    const NativeFactoredAction& old_sector,const NativeFactoredAction& correspondence,
    const NativeFactoredAction& new_sector,const NativeFactoredAction& observation) {
    return reconstruction.then(old_sector).then(correspondence).then(new_sector).then(observation);
}
inline NativeFactoredAction native_power(NativeFactoredAction operation,std::size_t exponent) {
    if(!operation.input().same(operation.output()))throw std::invalid_argument("native power requires an actual endomorphism");
    auto result=NativeFactoredAction::identity(operation.input(),operation.dimension_limit());
    while(exponent){if(exponent&1u)result=result.then(operation);exponent>>=1u;if(exponent)operation=operation.then(operation);}
    return result;
}
// Selected terminal channels share the actual action DAG. They do not retain
// suffix matrices or inspect maps beyond the explicitly selected prefix.
inline NativeFactoredAction native_terminal_channel(const std::vector<NativeFactoredAction>& evolution,
    std::size_t begin,std::size_t end,NativeFactoredAction injection,const NativeFactoredAction& observation) {
    if(begin>end || end>evolution.size())throw std::out_of_range("native terminal channel prefix");
    for(std::size_t i=begin;i<end;++i)injection=injection.then(evolution[i]);
    return injection.then(observation);
}
} // namespace rfc::inference
