#pragma once
#include "rfc/inference/native_field_updates.hpp"
#include "rfc/field/prepared_metric_novelty.hpp"

namespace rfc::inference {
struct NativeMetricAvailability {
    runtime::WorkAvailability old_endpoint,new_endpoint;
};
struct NativeMetricNoveltyRequest {
    storage::PinnedNativeAccession old_publication,new_publication;
    std::uint64_t old_carrier{},new_carrier{};
    numeric::SparseRationalTensor comparison;
    storage::PinnedNativeAccession new_metric;
    std::size_t dimension_limit{64};
};
struct NativeMetricEndpointWitness {
    std::uint64_t source_namespace{},sequence{},carrier{},input_field{};
    core::Grade grade;
};
struct NativeMetricNoveltyProvenance {
    std::string profile{"source-grade-metric-relative-novelty-v1"};
    NativeMetricEndpointWitness old_endpoint,new_endpoint;
    std::uint64_t new_metric{};
    std::vector<std::uint64_t> cause_fields;
    runtime::ExecutionRequirements selected_requirements;
    numeric::ExactActionLimits action_limits;
    NativeMetricAvailability preparation_availability,execution_availability;
    std::string preparation_policy,execution_policy;
    std::size_t preparation_steps{},execution_steps{};
    bool declared_causes{},fallback{};
};
namespace native_metric_detail {
using namespace native_update_detail;
using Carrier=std::shared_ptr<const storage::TemporalCarrier>;
inline void availability(const NativeMetricNoveltyRequest& r,const NativeMetricAvailability& available) {
    runtime::admit_work_availability(r.old_publication.source_namespace(),r.old_publication.sequence(),available.old_endpoint);
    runtime::admit_work_availability(r.new_publication.source_namespace(),r.new_publication.sequence(),available.new_endpoint);
}
inline Carrier carrier(const Pin& pin,std::uint64_t key,Guard& guard) {
    const auto endpoint=pin.endpoint();
    for(const auto& value:endpoint->carriers){guard.step();if(value.key==key)return Carrier{endpoint,&value};}
    throw std::invalid_argument("native metric declared carrier absent");
}
inline void field(const Pin& input,const Pin& publication,const Carrier& owner) {
    same(input,publication);require(input.kind()==storage::NativeAccessionKind::Field,"native metric input requires a stored Field");
    const auto t=input.tensor_declaration();
    require(t->role==storage::TemporalTensorRole::Field && input.carrier().get()==owner.get() &&
            t->tensor.domain()==owner->domain && t->tensor.support().count()==owner->domain.logical_size(),
            "native metric field requires its actual carrier and complete support");
}
inline runtime::ExecutionRequirements payload_requirements(const NativeMetricNoveltyRequest& r) {
    const std::array<Pin,3> pins{r.old_publication,r.new_publication,r.new_metric};auto out=selected(pins,3);
    out.rational_slots=add(out.rational_slots,r.comparison.values().nonzeros());out.sparse_entries=out.rational_slots;
    out.integer_bits=std::max(out.integer_bits,native_field_detail::tensor_bits(r.comparison));return out;
}
inline Matrix comparison(const numeric::SparseRationalTensor& tensor,Guard& guard) {
    const auto& shape=tensor.domain().extents();
    require(shape.size()==2 && tensor.support().count()==tensor.domain().logical_size(),
            "native metric correspondence requires every declared direction, including supported zero");
    guard.dimension(shape[0]);guard.dimension(shape[1]);Matrix result(shape[0],shape[1]);
    for(const auto& e:tensor.values().entries()){
        guard.step();guard.value(e.value);result.set(e.index/shape[1],e.index%shape[1],e.value);
    }
    return result;
}
} // namespace native_metric_detail
class DerivedNativeMetricNovelty;
class PreparedNativeMetricNovelty {
    using Pin=storage::PinnedNativeAccession;
    struct State {
        NativeMetricNoveltyRequest request;
        native_metric_detail::Carrier old_carrier,new_carrier;
        field::PreparedMetricNovelty numeric;
        runtime::ExecutionRequirements payload,retained;
        NativeMetricAvailability available;
        std::string policy;
        std::size_t preparation_steps;
        bool fallback;
    };
public:
    PreparedNativeMetricNovelty(const PreparedNativeMetricNovelty&)=default;
    PreparedNativeMetricNovelty& operator=(const PreparedNativeMetricNovelty&)=default;
    static PreparedNativeMetricNovelty prepare(NativeMetricNoveltyRequest request,NativeMetricAvailability available,
                                               runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_metric_detail;
        lease.check_cancelled();availability(request,available);same(request.new_metric,request.new_publication);
        require(request.dimension_limit>0,"native metric novelty dimension limit must be positive");
        const std::array<Pin,3> pins{request.old_publication,request.new_publication,request.new_metric};
        const auto payload=payload_requirements(request);runtime::admit_execution(payload,lease.policy());
        Guard guard(limits(lease,pins,request.dimension_limit,payload.rational_slots),[&lease]{lease.check_cancelled();});
        guard.dimension(2); // The complete split returns two channel axes.
        auto old=carrier(request.old_publication,request.old_carrier,guard),next=carrier(request.new_publication,request.new_carrier,guard);
        const auto n=old->domain.logical_size(),m=next->domain.logical_size();guard.dimension(n);guard.dimension(m);
        require(old->kind==storage::TemporalCarrierKind::SourceGrade && next->kind==storage::TemporalCarrierKind::SourceGrade &&
                old->grade==next->grade && old->domain==numeric::IndexDomain::linear(n) && next->domain==numeric::IndexDomain::linear(m),
                "native metric novelty requires actual linear SourceGrade carriers at the same grade");
        require(request.comparison.domain().extents()==std::vector<std::size_t>{m,n},
                "native metric correspondence dimensions disagree with its actual carriers");
        require(request.new_metric.kind()==storage::NativeAccessionKind::Tensor,"native metric requires a bilinear Metric Tensor pin");
        const auto t=request.new_metric.tensor_declaration();
        require(t->role==storage::TemporalTensorRole::Metric && t->input==next->key && t->output==next->key &&
                request.new_metric.carrier().get()==next.get() && t->tensor.domain().extents()==std::vector<std::size_t>{m,m} &&
                t->tensor.support().count()==t->tensor.domain().logical_size(),
                "native metric requires complete form support on the actual new carrier");
        auto preparing=payload;preparing.rational_slots=add(preparing.rational_slots,field::metric_novelty_detail::preparation_workspace(n,m));
        preparing.sparse_entries=preparing.rational_slots;runtime::admit_execution(preparing,lease.policy());
        auto plan=field::PreparedMetricNovelty(comparison(request.comparison,guard),materialize(request.new_metric,guard),guard);
        auto retained=payload;retained.rational_slots=add(retained.rational_slots,plan.stored_entries());
        retained.sparse_entries=add(retained.rational_slots,plan.stored_index_entries());runtime::admit_execution(retained,lease.policy());
        preparing=retained;preparing.rational_slots=add(preparing.rational_slots,field::metric_novelty_detail::preparation_workspace(n,m));
        preparing.sparse_entries=add(preparing.rational_slots,plan.stored_index_entries());runtime::admit_execution(preparing,lease.policy());
        lease.check_cancelled();return PreparedNativeMetricNovelty(std::make_shared<const State>(State{std::move(request),std::move(old),std::move(next),
            std::move(plan),payload,retained,available,lease.policy().name,guard.steps(),lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    const NativeMetricNoveltyRequest& request() const & {return state_->request;}
    const NativeMetricNoveltyRequest& request() const &&=delete;
    const field::PreparedMetricNovelty& numeric_plan() const & {return state_->numeric;}
    const field::PreparedMetricNovelty& numeric_plan() const &&=delete;
    Pin old_publication() const {return state_->request.old_publication;}
    Pin new_publication() const {return state_->request.new_publication;}
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    DerivedNativeMetricNovelty execute(Pin old_input,Pin new_input,NativeMetricAvailability available,
                                      runtime::ExecutionLease lease=runtime::ExecutionLease{}) const;
    DerivedNativeMetricNovelty execute_causes(Pin old_input,Pin new_input,std::vector<Pin> causes,NativeMetricAvailability available,
                                             runtime::ExecutionLease lease=runtime::ExecutionLease{}) const;
private:
    explicit PreparedNativeMetricNovelty(std::shared_ptr<const State> state):state_(std::move(state)){}
    DerivedNativeMetricNovelty apply(Pin old_input,Pin new_input,std::optional<std::vector<Pin>> causes,
                                    NativeMetricAvailability available,runtime::ExecutionLease lease) const;
    std::shared_ptr<const State> state_;
};
class DerivedNativeMetricNovelty {
public:
    const field::MetricRelativeNovelty& values() const & {return values_;}
    field::MetricRelativeNovelty values() const && {return values_;}
    const std::optional<field::MetricCausePairings>& cause_pairings() const & {return causes_;}
    std::optional<field::MetricCausePairings> cause_pairings() const && {return causes_;}
    const NativeMetricNoveltyProvenance& provenance() const & {return report_;}
    NativeMetricNoveltyProvenance provenance() const && {return report_;}
    const PreparedNativeMetricNovelty& plan() const & {return owner_;}
    const PreparedNativeMetricNovelty& plan() const &&=delete;
    storage::PinnedNativeAccession old_input() const {return old_input_;}
    storage::PinnedNativeAccession new_input() const {return new_input_;}
    std::vector<storage::PinnedNativeAccession> cause_inputs() const {return cause_inputs_;}
private:
    friend class PreparedNativeMetricNovelty;
    DerivedNativeMetricNovelty(PreparedNativeMetricNovelty owner,storage::PinnedNativeAccession old_input,storage::PinnedNativeAccession new_input,
            std::vector<storage::PinnedNativeAccession> inputs,field::MetricRelativeNovelty values,std::optional<field::MetricCausePairings> causes,
            NativeMetricNoveltyProvenance report):owner_(std::move(owner)),old_input_(std::move(old_input)),new_input_(std::move(new_input)),
        cause_inputs_(std::move(inputs)),values_(std::move(values)),causes_(std::move(causes)),report_(std::move(report)){}
    PreparedNativeMetricNovelty owner_;
    storage::PinnedNativeAccession old_input_,new_input_;
    std::vector<storage::PinnedNativeAccession> cause_inputs_;
    field::MetricRelativeNovelty values_;
    std::optional<field::MetricCausePairings> causes_;
    NativeMetricNoveltyProvenance report_;
};
inline DerivedNativeMetricNovelty PreparedNativeMetricNovelty::execute(Pin old_input,Pin new_input,NativeMetricAvailability available,
                                                                    runtime::ExecutionLease lease) const {
    return apply(std::move(old_input),std::move(new_input),{},available,std::move(lease));
}
inline DerivedNativeMetricNovelty PreparedNativeMetricNovelty::execute_causes(Pin old_input,Pin new_input,std::vector<Pin> causes,
        NativeMetricAvailability available,runtime::ExecutionLease lease) const {
    return apply(std::move(old_input),std::move(new_input),std::move(causes),available,std::move(lease));
}
inline DerivedNativeMetricNovelty PreparedNativeMetricNovelty::apply(Pin old_input,Pin new_input,std::optional<std::vector<Pin>> causes,
        NativeMetricAvailability available,runtime::ExecutionLease lease) const {
    using namespace native_metric_detail;
    const auto pin=state_;lease.check_cancelled();availability(pin->request,pin->available);availability(pin->request,available);
    native_metric_detail::field(old_input,pin->request.old_publication,pin->old_carrier);
    native_metric_detail::field(new_input,pin->request.new_publication,pin->new_carrier);
    const auto count=causes?causes->size():0;
    if(count>pin->request.dimension_limit)throw std::length_error("native metric complete cause-axis limit");
    std::vector<Pin> selected{pin->request.old_publication,pin->request.new_publication,pin->request.new_metric,old_input,new_input};
    if(causes)for(const auto& cause:*causes){native_metric_detail::field(cause,pin->request.new_publication,pin->new_carrier);selected.push_back(cause);}
    auto raw=payload_requirements(pin->request);auto required=pin->retained;required.records=add(required.records,add(2,count));
    for(std::size_t i=3;i<selected.size();++i){
        raw.rational_slots=add(raw.rational_slots,payload(selected[i]));
        raw.integer_bits=std::max(raw.integer_bits,native_field_detail::tensor_bits(*selected[i].tensor()));
        required.rational_slots=add(required.rational_slots,payload(selected[i]));
    }
    const auto workspace=pin->numeric.workspace_slots(std::max<std::size_t>(2,count));
    required.rational_slots=add(required.rational_slots,workspace);required.sparse_entries=add(required.rational_slots,pin->numeric.stored_index_entries());
    required.integer_bits=std::max(required.integer_bits,raw.integer_bits);runtime::admit_execution(required,lease.policy());
    const auto action=limits(lease,selected,pin->request.dimension_limit,raw.rational_slots);
    Guard guard(action,[&lease]{lease.check_cancelled();});
    auto values=pin->numeric.decompose(field_values(old_input,guard),field_values(new_input,guard),guard);
    std::optional<field::MetricCausePairings> pairings;std::vector<Pin> inputs;
    if(causes){
        std::vector<numeric::RationalVector> channels;channels.reserve(count);
        for(const auto& cause:*causes)channels.push_back(field_values(cause,guard));
        pairings.emplace(pin->numeric.pair_causes(std::move(channels),values.innovation,guard));inputs=std::move(*causes);
    }
    NativeMetricNoveltyProvenance report;
    report.old_endpoint={old_input.source_namespace(),old_input.sequence(),pin->old_carrier->key,old_input.tensor_declaration()->key,pin->old_carrier->grade};
    report.new_endpoint={new_input.source_namespace(),new_input.sequence(),pin->new_carrier->key,new_input.tensor_declaration()->key,pin->new_carrier->grade};
    report.new_metric=pin->request.new_metric.tensor_declaration()->key;
    for(const auto& input:inputs)report.cause_fields.push_back(input.tensor_declaration()->key);
    report.selected_requirements=required;report.action_limits=action;report.preparation_steps=pin->preparation_steps;report.execution_steps=guard.steps();
    report.preparation_availability=pin->available;report.execution_availability=available;report.preparation_policy=pin->policy;report.execution_policy=lease.policy().name;
    report.declared_causes=causes.has_value();report.fallback=pin->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
    lease.check_cancelled();return DerivedNativeMetricNovelty(PreparedNativeMetricNovelty(pin),std::move(old_input),std::move(new_input),std::move(inputs),
        std::move(values),std::move(pairings),std::move(report));
}
} // namespace rfc::inference
