#pragma once
#include "rfc/inference/native_temporal_actions.hpp"
#include "rfc/statistics/ensemble.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"

namespace rfc::inference {
class PreparedNativeEnsemble;
class NativeBilinearForm {
    struct State {
        NativeCarrier left,right;
        numeric::ExactMatrix form;
        std::optional<numeric::PreparedPositiveSemidefinite> positive;
        std::vector<storage::PinnedNativeAccession> pins;
        bool unit{};
        std::size_t dimension_limit{4096};
    };
    std::shared_ptr<const State> state_;
    explicit NativeBilinearForm(State state):state_(std::make_shared<const State>(std::move(state))){}
    friend class PreparedNativeMoments;
    friend class PreparedNativeEnsemble;
    friend class PreparedNativeMomentChange;
    void validate(numeric::ExactActionGuard& guard) const {
        const auto pin=state_;guard.matrix(pin->form);
        if(pin->positive){guard.matrix(pin->positive->unit_lower_entries());
            for(const auto& [i,q]:pin->positive->diagonal_entries()){(void)i;guard.step();guard.value(q);}}
    }
    numeric::Rational pair(const NativeFieldValue& left,const NativeFieldValue& right,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;
        if(!pin->left.same(left.carrier()) || !pin->right.same(right.carrier()))throw std::invalid_argument("native pairing requires actual declared left/right carriers");
        return coordinate_pair(left.sparse_values(),right.sparse_values(),guard);
    }
    numeric::Rational coordinate_pair(const numeric::SparseRationalVector& left,const numeric::SparseRationalVector& right,
        numeric::ExactActionGuard& guard) const {
        validate(guard);const auto pin=state_;
        if(left.size()!=pin->left.size() || right.size()!=pin->right.size())throw std::invalid_argument("bilinear sparse field dimensions");
        if(pin->unit)return field::factored_action_detail::dot(left,right,guard);
        return field::factored_action_detail::dot(left,numeric::checked_apply(pin->form,right,guard),guard);
    }
public:
    static NativeBilinearForm tensor(storage::PinnedNativeAccession pin,numeric::ExactActionLimits limits={}) {
        if(pin.kind()!=storage::NativeAccessionKind::Tensor)throw std::invalid_argument("bilinear form requires a declared tensor pin");
        const auto d=pin.tensor_declaration();
        if((d->role!=storage::TemporalTensorRole::Metric && d->role!=storage::TemporalTensorRole::CrossForm) || !d->input || !d->output ||
           d->tensor.support().count()!=d->tensor.domain().logical_size())throw std::invalid_argument("complete declared metric/cross form required");
        auto left=NativeCarrier::bind(pin,*d->output),right=NativeCarrier::bind(pin,*d->input);auto form=storage::temporal_tensor_matrix(*d);
        if(form.rows()!=left.size() || form.cols()!=right.size())throw std::invalid_argument("native bilinear form dimensions");
        numeric::ExactActionGuard guard(limits);guard.matrix(form);std::optional<numeric::PreparedPositiveSemidefinite> positive;
        if(d->role==storage::TemporalTensorRole::Metric){
            if(!left.same(right))throw std::invalid_argument("metric requires the identical native carrier on both sides");
            positive.emplace(form,guard);if(positive->rank()!=form.rows())throw std::invalid_argument("native moment metric is not positive definite");
        }
        return NativeBilinearForm({std::move(left),std::move(right),std::move(form),std::move(positive),{pin},false,limits.dimension});
    }
    static NativeBilinearForm named_unit(NativeCarrier carrier,numeric::ExactActionLimits limits={}) {
        if(!carrier.same(NativeCarrier::bind(carrier.publication,carrier.key)))throw std::invalid_argument("named metric carrier is not actual");
        numeric::ExactActionGuard guard(limits);guard.dimension(carrier.size());const auto pin=carrier.publication;
        return NativeBilinearForm({carrier,carrier,numeric::ExactMatrix(carrier.size(),carrier.size()),{},{pin},true,limits.dimension});
    }
    static NativeBilinearForm declared_cross(NativeCarrier left,NativeCarrier right,numeric::ExactMatrix form,
        numeric::ExactActionLimits limits={}) {
        if(!left.same(NativeCarrier::bind(left.publication,left.key)) || !right.same(NativeCarrier::bind(right.publication,right.key)) ||
           form.rows()!=left.size() || form.cols()!=right.size())throw std::invalid_argument("cross form needs its actual declared carrier owners");
        numeric::ExactActionGuard guard(limits);guard.matrix(form);const auto a=left.publication,b=right.publication;
        return NativeBilinearForm({std::move(left),std::move(right),std::move(form),{},{a,b},false,limits.dimension});
    }
    const NativeCarrier& left() const & {return state_->left;}
    const NativeCarrier& right() const & {return state_->right;}
    bool is_positive_metric() const noexcept {return state_->unit || state_->positive.has_value();}
    std::size_t dimension_limit() const noexcept {return state_->dimension_limit;}
    const std::vector<storage::PinnedNativeAccession>& publications() const & {return state_->pins;}
};
struct NativeMomentChannel {std::string label;NativeFactoredAction operation;};
struct NativeMomentPair {std::size_t left{},right{};NativeBilinearForm form;};
struct NativeSelectedMoments {
    std::vector<numeric::Rational> values;
    std::vector<NativeFieldValue> realized_channels;
    std::vector<std::size_t> channel_indices;
    std::shared_ptr<const void> plan_owner;
    std::size_t execution_steps{};
};
class PreparedNativeMoments {
    struct State {std::vector<NativeMomentChannel> channels;std::vector<NativeMomentPair> pairs;};
    std::shared_ptr<const State> state_;
public:
    PreparedNativeMoments(std::vector<NativeMomentChannel> channels,std::vector<NativeMomentPair> pairs) {
        for(const auto& pair:pairs){
            if(pair.left>=channels.size() || pair.right>=channels.size())throw std::out_of_range("selected native moment channel");
            if(!pair.form.left().same(channels[pair.left].operation.output()) || !pair.form.right().same(channels[pair.right].operation.output()))
                throw std::invalid_argument("native moment form binds different channel output carriers");
        }
        state_=std::make_shared<const State>(State{std::move(channels),std::move(pairs)});
    }
    NativeSelectedMoments execute(std::vector<NativeFieldValue> inputs,const NativeActionAvailability& availability,
                                  const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;if(inputs.size()!=pin->channels.size())throw std::invalid_argument("one native field per operation channel required");
        std::vector<storage::PinnedNativeAccession> pins;std::size_t dimension=std::numeric_limits<std::size_t>::max();
        for(const auto& pair:pin->pairs){
            for(const auto index:{pair.left,pair.right}){
                for(const auto& p:pin->channels[index].operation.publications())native_action_detail::append_pin(pins,p);
                for(const auto& p:inputs[index].publications())native_action_detail::append_pin(pins,p);
                dimension=std::min(dimension,pin->channels[index].operation.dimension_limit());
            }
            for(const auto& p:pair.form.publications())native_action_detail::append_pin(pins,p);
            dimension=std::min(dimension,pair.form.dimension_limit());
        }
        for(const auto& p:pins)native_action_detail::available(p,availability);
        runtime::admit_execution({pins.size()+pin->pairs.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pins,dimension),[&lease]{lease.check_cancelled();});
        std::vector<std::optional<NativeFieldValue>> realized(inputs.size());NativeSelectedMoments result;result.plan_owner=pin;
        const auto realize=[&](std::size_t i)->const NativeFieldValue&{
            if(!realized[i])realized[i]=pin->channels[i].operation.execute_with_guard(inputs[i],availability,lease,guard);
            return *realized[i];
        };
        for(const auto& pair:pin->pairs)result.values.push_back(pair.form.pair(realize(pair.left),realize(pair.right),guard));
        for(std::size_t i=0;i<realized.size();++i)if(realized[i]){result.channel_indices.push_back(i);result.realized_channels.push_back(std::move(*realized[i]));}
        result.execution_steps=guard.steps();return result;
    }
    NativeSelectedMoments execute(NativeFieldValue input,const NativeActionAvailability& availability,
                                  const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        return execute(std::vector<NativeFieldValue>(state_->channels.size(),input),availability,lease);
    }
};
struct NativeEnsembleContractions {
    statistics::EffectiveDimension2 moments;
    std::size_t covariance_rank{},samples{};
    std::shared_ptr<const void> plan_owner;
};
struct NativeEnsembleCoupling {
    numeric::Rational left_variance,right_variance,cross_contraction,coupling;
    std::shared_ptr<const void> left_owner,right_owner;
    std::size_t execution_steps{};
};
// Covariance PSD is established constructively from nonnegative sample weights.
// No ambient covariance matrix is formed; V and S2 are selected sample pairings.
class PreparedNativeEnsemble {
    struct State {
        NativeBilinearForm metric;
        std::vector<NativeFieldValue> samples;
        numeric::RationalVector weights;
        numeric::PreparedExactFrames centered_frame;
        statistics::EffectiveDimension2 moments;
        std::vector<storage::PinnedNativeAccession> pins;
        std::size_t preparation_steps;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeEnsemble(State state):state_(std::make_shared<const State>(std::move(state))){}
public:
    static PreparedNativeEnsemble prepare(NativeBilinearForm metric,std::vector<NativeFieldValue> samples,
        numeric::RationalVector weights,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        if(!metric.is_positive_metric() || samples.empty() || samples.size()!=weights.size())throw std::invalid_argument("native ensemble needs a positive metric, samples and matching nonnegative weights");
        auto pins=metric.publications();for(const auto& sample:samples){
            if(!sample.carrier().same(metric.left()))throw std::invalid_argument("native ensemble samples need a common actual carrier");
            for(const auto& p:sample.publications())native_action_detail::append_pin(pins,p);
        }
        for(const auto& p:pins)native_action_detail::available(p,availability);
        runtime::admit_execution({pins.size()+samples.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pins,metric.dimension_limit()),[&lease]{lease.check_cancelled();});
        weights=guard.canonical_input(weights);numeric::Rational total=0;std::size_t positive=0;
        for(const auto& w:weights){if(w<0)throw std::invalid_argument("negative native ensemble weight");total=guard.add(total,w);if(w>0)++positive;}
        if(total==0)throw std::invalid_argument("native ensemble has zero total weight");
        for(auto& w:weights)w=guard.div(w,total);
        auto mean=samples.front().scaled(0,guard);for(std::size_t i=0;i<samples.size();++i)mean=mean.plus(samples[i].scaled(weights[i],guard),guard);
        std::vector<NativeFieldValue> centered;for(const auto& sample:samples)centered.push_back(sample.plus(mean.scaled(-1,guard),guard));
        statistics::EffectiveDimension2 value;numeric::ExactMatrix columns(metric.left().size(),positive);std::size_t column=0;
        for(std::size_t i=0;i<centered.size();++i){
            if(weights[i]==0)continue;
            for(const auto& entry:centered[i].sparse_values().entries()){guard.step();columns.set(entry.index,column,entry.value);guard.entries(columns.nonzeros());}++column;
            value.variance=guard.add(value.variance,guard.mul(weights[i],metric.pair(centered[i],centered[i],guard)));
            for(std::size_t j=0;j<centered.size();++j)if(weights[j]!=0){const auto q=metric.pair(centered[i],centered[j],guard);
                value.second_contraction=guard.add(value.second_contraction,guard.mul(guard.mul(weights[i],weights[j]),guard.mul(q,q)));}
        }
        numeric::PreparedExactFrames frame(std::move(columns),guard);
        if(value.variance!=0){value.dimension=guard.div(guard.mul(value.variance,value.variance),value.second_contraction);value.collision_probability=guard.div(1,value.dimension);
            if(value.dimension<1 || value.dimension>numeric::Rational{static_cast<unsigned long>(frame.rank())})throw std::logic_error("native ensemble dimension bound");}
        return PreparedNativeEnsemble({std::move(metric),std::move(samples),std::move(weights),std::move(frame),value,std::move(pins),guard.steps()});
    }
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    NativeEnsembleContractions contractions(const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;for(const auto& p:pin->pins)native_action_detail::available(p,availability);
        runtime::admit_execution({pin->pins.size()+pin->samples.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pin->pins,pin->metric.dimension_limit()),[&lease]{lease.check_cancelled();});
        pin->metric.validate(guard);pin->centered_frame.validate(guard);guard.vector(pin->weights);
        for(const auto& sample:pin->samples)guard.sparse_vector(sample.sparse_values());
        for(const auto& q:{pin->moments.variance,pin->moments.second_contraction,pin->moments.dimension,pin->moments.collision_probability})guard.value(q);
        if(pin->moments.variance==0)throw std::invalid_argument("native effective dimension is undefined for zero covariance");
        return {pin->moments,pin->centered_frame.rank(),pin->samples.size(),pin};
    }
    // Pairing is by the explicitly supplied sample positions and common
    // weights. A nonnegative weighted sum of joint centered rank-one fields
    // certifies the joint covariance PSD without constructing that covariance.
    NativeEnsembleCoupling coupling(const PreparedNativeEnsemble& other,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto left=state_,right=other.state_;
        if(left->weights!=right->weights)throw std::invalid_argument("paired native ensembles require identical ordered normalized weights");
        auto pins=left->pins;for(const auto& p:right->pins)native_action_detail::append_pin(pins,p);
        for(const auto& p:pins)native_action_detail::available(p,availability);
        runtime::admit_execution({pins.size()+left->samples.size()+right->samples.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pins,std::min(left->metric.dimension_limit(),right->metric.dimension_limit())),[&lease]{lease.check_cancelled();});
        for(const auto* owner:{left.get(),right.get()}){
            owner->metric.validate(guard);owner->centered_frame.validate(guard);guard.vector(owner->weights);
            for(const auto& sample:owner->samples)guard.sparse_vector(sample.sparse_values());
            guard.value(owner->moments.variance);if(owner->moments.variance==0)throw std::invalid_argument("cross-sector coupling requires two nonzero covariances");
        }
        const auto columns=[&](const numeric::ExactMatrix& matrix){
            std::vector<std::vector<numeric::SparseRationalEntry>> entries(matrix.cols());
            for(const auto& [ij,q]:matrix.entries()){guard.step();entries[ij.second].push_back({ij.first,q});}
            std::vector<numeric::SparseRationalVector> result;for(auto& values:entries)result.emplace_back(matrix.rows(),std::move(values));return result;
        };
        const auto x=columns(left->centered_frame.original()),y=columns(right->centered_frame.original());
        numeric::RationalVector weights;for(const auto& w:left->weights)if(w>0)weights.push_back(w);
        numeric::Rational numerator=0;
        for(std::size_t i=0;i<weights.size();++i)for(std::size_t j=0;j<weights.size();++j){
            const auto s=left->metric.coordinate_pair(x[i],x[j],guard),t=right->metric.coordinate_pair(y[i],y[j],guard);
            numerator=guard.add(numerator,guard.mul(guard.mul(weights[i],weights[j]),guard.mul(s,t)));
        }
        const auto kappa=guard.div(numerator,guard.mul(left->moments.variance,right->moments.variance));
        if(kappa<0 || kappa>1)throw std::logic_error("native paired ensemble coupling bound");
        return {left->moments.variance,right->moments.variance,numerator,kappa,left,right,guard.steps()};
    }
};
} // namespace rfc::inference
