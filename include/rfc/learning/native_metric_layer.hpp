#pragma once
#include "rfc/learning/exact_metric_channels.hpp"
#include "rfc/inference/native_field_execution.hpp"

namespace rfc::learning {

struct NativeMetricLayerBinding {
    std::string model;
    // Explicit coordinates for the original ordered channels and uniform bias.
    std::uint64_t channel_coordinates{},bias_coordinates{};
};
struct NativeMetricLayerProvenance {
    std::string profile{"native-diagonal-finite-metric-layer-v1"};
    NativeMetricLayerBinding binding;
    std::uint64_t input_field{},gain_field{},bias_field{},output_carrier{};
    std::uint64_t source_namespace{},sequence{};
    inference::NativeMetricSelection metric_selection{};
    std::vector<std::uint64_t> metrics;
    std::string preparation_policy;
    runtime::ExecutionHostPolicy tower_preparation_policy;
    std::vector<ExactMetricChannel> original_channels;
    runtime::ExecutionRequirements requirements;
    std::string execution_policy;
    bool prospective{true},fallback{};
};
class PreparedNativeMetricLayer;
class NativeMetricLayerProposal {
public:
    const numeric::RationalVector& values() const & {return values_;}
    numeric::RationalVector values() const && {return values_;}
    const NativeMetricLayerProvenance& provenance() const & {return report_;}
    NativeMetricLayerProvenance provenance() const && {return report_;}
    storage::PinnedNativeAccession input() const {return input_;}
    storage::PinnedNativeAccession gains() const {return gains_;}
    storage::PinnedNativeAccession bias() const {return bias_;}
private:
    friend class PreparedNativeMetricLayer;
    NativeMetricLayerProposal(storage::PinnedNativeAccession input,storage::PinnedNativeAccession gains,
        storage::PinnedNativeAccession bias,numeric::RationalVector values,NativeMetricLayerProvenance report)
        :input_(std::move(input)),gains_(std::move(gains)),bias_(std::move(bias)),values_(std::move(values)),report_(std::move(report)){}
    storage::PinnedNativeAccession input_,gains_,bias_;
    numeric::RationalVector values_;
    NativeMetricLayerProvenance report_;
};
class NativeMetricBatchProposal {
public:
    const numeric::RationalVector& values() const & { return values_; }
    const NativeMetricLayerProvenance& provenance() const & { return report_; }
    const std::vector<storage::PinnedNativeAccession>& inputs() const & { return inputs_; }
    storage::PinnedNativeAccession gains() const { return gains_; }
    storage::PinnedNativeAccession bias() const { return bias_; }
    std::size_t samples() const noexcept { return inputs_.size(); }
    std::size_t cells() const noexcept { return values_.size() / inputs_.size(); }
private:
    friend class PreparedNativeMetricLayer;
    NativeMetricBatchProposal(std::vector<storage::PinnedNativeAccession> inputs,
            storage::PinnedNativeAccession gains, storage::PinnedNativeAccession bias,
            numeric::RationalVector values, NativeMetricLayerProvenance report)
        : inputs_(std::move(inputs)), gains_(std::move(gains)), bias_(std::move(bias)),
          values_(std::move(values)), report_(std::move(report)) {}
    std::vector<storage::PinnedNativeAccession> inputs_;
    storage::PinnedNativeAccession gains_, bias_;
    numeric::RationalVector values_;
    NativeMetricLayerProvenance report_;
};

// Retains the original channels and native parameter fields. No basis reduction
// or second trainable parameter vector is introduced. Uniform bias broadcast is
// the named scalar-bias semantics of CompiledExactMetricLayer.
class PreparedNativeMetricLayer {
public:
    PreparedNativeMetricLayer(inference::PreparedNativeExactTower tower,core::Grade grade,
        std::vector<ExactMetricChannel> channels,storage::PinnedNativeAccession gains,
        storage::PinnedNativeAccession bias,NativeMetricLayerBinding binding,
        const runtime::ExecutionHostPolicy& policy={}) {
        using namespace inference::native_field_detail;
        require(!binding.model.empty() && binding.channel_coordinates && binding.bias_coordinates,"model and parameter coordinates must be declared");
        require(!channels.empty() && channels.size()<=64,"finite native model channel budget");
        for(const auto& channel:channels){validate_exact_metric_channel(channel);(void)operation(channel);}
        const auto publication=tower.publication();same_publication(publication,gains);same_publication(publication,bias);
        validate_parameter(gains,binding.channel_coordinates,channels.size());validate_parameter(bias,binding.bias_coordinates,1);
        auto required=tower.requirements(1,field::OperatorKind::Hodge);
        required.records=add(required.records,add(channels.size(),2));
        required.rational_slots=add(required.rational_slots,add(channels.size(),tower.grade_size(grade)));
        required.scalar_work=mul(required.scalar_work,add(channels.size(),1));
        required.integer_bits=std::max({required.integer_bits,tensor_bits(*gains.tensor()),tensor_bits(*bias.tensor())});
        runtime::admit_execution(required,policy);
        if(required.scalar_work>publication.limits().max_action_work)throw std::length_error("native model accession work budget");
        state_=std::make_shared<const State>(std::move(tower),grade,std::move(channels),std::move(gains),std::move(bias),std::move(binding),policy);
    }
    PreparedNativeMetricLayer(const PreparedNativeMetricLayer&)=default;
    PreparedNativeMetricLayer& operator=(const PreparedNativeMetricLayer&)=default;
    runtime::ExecutionRequirements batch_requirements(std::span<const storage::PinnedNativeAccession> inputs,
                                                      std::size_t scratch_slots) const {
        using namespace inference::native_field_detail;
        auto required = state_->tower.batch_requirements({field::OperatorKind::Hodge, state_->grade, 0}, inputs, scratch_slots);
        required.records = add(required.records, add(state_->channels.size(), 2));
        required.rational_slots = add(required.rational_slots,
            add(state_->channels.size(), mul(mul(state_->tower.grade_size(state_->grade), inputs.size()), 2)));
        required.scalar_work = mul(required.scalar_work, add(state_->channels.size(), 1));
        required.integer_bits = sum_bound(std::max(add(required.integer_bits, tensor_bits(*state_->gains.tensor())),
                                                  tensor_bits(*state_->bias.tensor())), add(state_->channels.size(), 1));
        return required;
    }
    NativeMetricBatchProposal forward_batch(std::span<const storage::PinnedNativeAccession> inputs,
            field::PreparedExactExecutionTower::BatchWorkspace& workspace,
            const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) const {
        using namespace inference::native_field_detail;
        const auto pin = state_; lease.check_cancelled();
        pin->tower.tower().validate_batch_workspace(pin->grade, inputs.size(), workspace);
        const auto required = batch_requirements(inputs, workspace.retained_slots());
        runtime::admit_execution(required, lease.policy());
        if (required.scalar_work > pin->tower.publication().limits().max_action_work)
            throw std::length_error("native metric batch accession work budget");
        const auto x = pin->tower.input_batch_values(inputs, pin->grade);
        numeric::RationalVector output(x.size(), *pin->bias.tensor()->state(0));
        numeric::RationalVector down, up;
        const auto response = [&](field::OperatorKind kind, numeric::RationalVector& values) {
            if (!values.empty()) return;
            values.resize(x.size());
            pin->tower.tower().apply_batch_into({kind, pin->grade, 0}, x, values, inputs.size(), workspace,
                                               [&lease] { lease.check_cancelled(); });
        };
        for (std::size_t c = 0; c < pin->channels.size(); ++c) {
            lease.check_cancelled();
            const auto gain = *pin->gains.tensor()->state(c);
            if (gain == 0) continue;
            const auto kind = pin->channels[c].kind;
            if (kind == ExactMetricChannelKind::HodgeDown || kind == ExactMetricChannelKind::Hodge)
                response(field::OperatorKind::HodgeDown, down);
            if (kind == ExactMetricChannelKind::HodgeUp || kind == ExactMetricChannelKind::Hodge)
                response(field::OperatorKind::HodgeUp, up);
            for (std::size_t i = 0; i < output.size(); ++i) {
                if (kind == ExactMetricChannelKind::Identity) output[i] += gain * x[i];
                else if (kind == ExactMetricChannelKind::HodgeDown) output[i] += gain * down[i];
                else if (kind == ExactMetricChannelKind::HodgeUp) output[i] += gain * up[i];
                else output[i] += gain * (down[i] + up[i]);
            }
        }
        for (const auto& value : output)
            if (integer_bits(value) > required.integer_bits) throw std::runtime_error("native metric batch integer envelope violated");
        NativeMetricLayerProvenance report; report.profile = "native-diagonal-finite-metric-batch-v1";
        report.binding = pin->binding; report.gain_field = pin->gains.tensor_declaration()->key;
        report.bias_field = pin->bias.tensor_declaration()->key; report.output_carrier = pin->tower.grade_carrier(pin->grade);
        report.original_channels = pin->channels; report.source_namespace = pin->tower.publication().source_namespace();
        report.sequence = pin->tower.publication().sequence(); report.metric_selection = pin->tower.metric_selection();
        report.metrics = pin->tower.metric_keys(); report.preparation_policy = pin->policy.name;
        report.tower_preparation_policy = pin->tower.preparation_policy(); report.requirements = required;
        report.execution_policy = lease.policy().name;
        report.fallback = report.tower_preparation_policy.backend != runtime::ExecutionBackend::ExactCpu ||
            pin->policy.backend != runtime::ExecutionBackend::ExactCpu || lease.policy().backend != runtime::ExecutionBackend::ExactCpu;
        lease.check_cancelled();
        return NativeMetricBatchProposal({inputs.begin(), inputs.end()}, pin->gains, pin->bias, std::move(output), std::move(report));
    }
    NativeMetricLayerProposal forward(const storage::PinnedNativeAccession& input,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        using namespace inference::native_field_detail;
        const auto pin=state_;lease.check_cancelled();pin->tower.validate_input(input,pin->grade);
        const auto bits=tensor_bits(*input.tensor());
        auto required=pin->tower.requirements(bits,field::OperatorKind::Hodge);
        required.records=add(required.records,add(pin->channels.size(),2));
        required.rational_slots=add(required.rational_slots,add(pin->channels.size(),pin->tower.grade_size(pin->grade)));
        required.scalar_work=mul(required.scalar_work,add(pin->channels.size(),1));
        required.integer_bits=sum_bound(std::max(add(required.integer_bits,tensor_bits(*pin->gains.tensor())),
                                                tensor_bits(*pin->bias.tensor())),add(pin->channels.size(),1));
        runtime::admit_execution(required,lease.policy());
        const auto x=pin->tower.input_values(input,pin->grade);
        numeric::RationalVector out(x.size(),*pin->bias.tensor()->state(0));
        for(std::size_t c=0;c<pin->channels.size();++c){
            const auto gain=*pin->gains.tensor()->state(c);
            if(gain==0)continue;
            const auto channel=pin->channels[c];
            const auto response=channel.kind==ExactMetricChannelKind::Identity?x:
                pin->tower.tower().apply({operation(channel),pin->grade,numeric::Rational{0}},x);
            for(std::size_t i=0;i<out.size();++i)out[i]+=gain*response[i];
        }
        for(const auto& value:out)if(integer_bits(value)>required.integer_bits)throw std::runtime_error("native model integer envelope violated");
        NativeMetricLayerProvenance report;report.binding=pin->binding;report.input_field=input.tensor_declaration()->key;
        report.gain_field=pin->gains.tensor_declaration()->key;report.bias_field=pin->bias.tensor_declaration()->key;
        report.output_carrier=pin->tower.grade_carrier(pin->grade);report.original_channels=pin->channels;
        report.source_namespace=pin->tower.publication().source_namespace();report.sequence=pin->tower.publication().sequence();
        report.metric_selection=pin->tower.metric_selection();report.metrics=pin->tower.metric_keys();report.preparation_policy=pin->policy.name;
        report.tower_preparation_policy=pin->tower.preparation_policy();
        report.requirements=required;report.execution_policy=lease.policy().name;
        report.fallback=report.tower_preparation_policy.backend!=runtime::ExecutionBackend::ExactCpu ||
            pin->policy.backend!=runtime::ExecutionBackend::ExactCpu || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        lease.check_cancelled();return NativeMetricLayerProposal(input,pin->gains,pin->bias,std::move(out),std::move(report));
    }
private:
    static field::OperatorKind operation(const ExactMetricChannel& channel){
        switch(channel.kind){
        case ExactMetricChannelKind::Identity:return field::OperatorKind::Hodge; // only an admission envelope
        case ExactMetricChannelKind::HodgeDown:return field::OperatorKind::HodgeDown;
        case ExactMetricChannelKind::HodgeUp:return field::OperatorKind::HodgeUp;
        case ExactMetricChannelKind::Hodge:return field::OperatorKind::Hodge;
        default:throw std::invalid_argument("channel outside native finite metric profile");
        }
    }
    static void validate_parameter(const storage::PinnedNativeAccession& pin,std::uint64_t coordinates,std::size_t size){
        using namespace inference::native_field_detail;
        require(pin.kind()==storage::NativeAccessionKind::Field,"model parameter requires a native field pin");
        const auto declaration=pin.tensor_declaration();const auto carrier=pin.carrier();const auto tensor=pin.tensor();
        require(declaration->role==storage::TemporalTensorRole::Field && declaration->owner.kind==storage::TemporalOwnerKind::Global &&
                carrier->kind==storage::TemporalCarrierKind::Explicit && carrier->coordinate_system==coordinates,
                "model parameter coordinate/owner mismatch");
        require(tensor->domain()==numeric::IndexDomain::linear(size) && tensor->support().count()==size,"model parameter requires complete original support");
    }
    struct State {
        inference::PreparedNativeExactTower tower;
        core::Grade grade;
        std::vector<ExactMetricChannel> channels;
        storage::PinnedNativeAccession gains,bias;
        NativeMetricLayerBinding binding;
        runtime::ExecutionHostPolicy policy;
        State(inference::PreparedNativeExactTower t,core::Grade k,std::vector<ExactMetricChannel> c,
              storage::PinnedNativeAccession g,storage::PinnedNativeAccession b,NativeMetricLayerBinding m,runtime::ExecutionHostPolicy p)
            :tower(std::move(t)),grade(k),channels(std::move(c)),gains(std::move(g)),bias(std::move(b)),binding(std::move(m)),policy(std::move(p)){}
    };
    std::shared_ptr<const State> state_;
};
} // namespace rfc::learning
