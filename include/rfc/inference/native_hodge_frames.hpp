#pragma once
#include "rfc/inference/native_field_execution.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/runtime/work_graph.hpp"

namespace rfc::inference {
struct NativeHodgeFrameRequest {
    NativeMetricSelection metrics{NativeMetricSelection::NamedUnit};
    std::vector<std::uint64_t> grade_carriers;
    std::vector<storage::PinnedNativeAccession> metric_tensors;
    core::Grade grade{};
    // Bounds whole coordinate maps, including the stacked harmonic constraints.
    std::size_t dimension_limit{64};
};
struct NativeHodgeFrameProvenance {
    std::string profile{"source-grade-complete-hodge-frames-v1"};
    std::uint64_t source_namespace{},sequence{},input_field{},carrier{};
    core::Grade grade;
    NativeMetricSelection metric_selection;
    std::vector<std::uint64_t> grade_carriers,metric_tensors;
    std::size_t gradient_dimension{},curl_dimension{},harmonic_dimension{},chain_interfaces_checked{};
    runtime::ExecutionRequirements selected_requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{},execution_steps{};
    std::string preparation_policy,execution_policy;
    runtime::WorkAvailability preparation_availability,execution_availability;
    bool fallback{};
};
namespace native_frame_detail {
using Pin=storage::PinnedNativeAccession;
using Matrix=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Carrier=std::shared_ptr<const storage::TemporalCarrier>;
using native_field_detail::add;
using native_field_detail::mul;
using native_field_detail::require;
inline void same(const Pin& a,const Pin& b) {
    native_field_detail::same_publication(a,b);
    require(a.same_execution_authority(b),"native frames require the actual execution authority");
}
inline Carrier carrier(const Pin& publication,std::uint64_t key,Guard& guard) {
    const auto endpoint=publication.endpoint();
    for(const auto& value:endpoint->carriers){guard.step();if(value.key==key)return Carrier{endpoint,&value};}
    throw std::invalid_argument("native frame declared grade carrier absent");
}
inline runtime::ExecutionRequirements requirements(const Pin& publication,const NativeHodgeFrameRequest& request) {
    runtime::ExecutionRequirements result;
    result.records=add(1,mul(request.grade_carriers.size(),2));
    for(std::size_t k=0;k<publication.endpoint()->source.grade_count();++k){
        const auto& arena=publication.endpoint()->source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        result.records=add(result.records,add(arena.cell_count(),arena.incidence_count()));
        result.rational_slots=add(result.rational_slots,add(arena.cell_count(),arena.incidence_count()));
    }
    result.records=add(result.records,request.metric_tensors.size());
    for(const auto& pin:request.metric_tensors)result.rational_slots=add(result.rational_slots,pin.tensor()->values().nonzeros());
    result.sparse_entries=result.rational_slots;return result;
}
inline std::size_t minimum_bits(const Pin& publication,const NativeHodgeFrameRequest& request,Guard& guard) {
    std::size_t bits=1;
    for(std::size_t k=0;k<publication.endpoint()->source.grade_count();++k){
        const auto& arena=publication.endpoint()->source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        for(const auto& value:arena.cell_scales){guard.step();guard.value(value);bits=std::max(bits,native_field_detail::integer_bits(value));}
        for(const auto& value:arena.lattice_coefficient){
            guard.step();const numeric::Rational q{value};guard.value(q);bits=std::max(bits,native_field_detail::integer_bits(q));
        }
    }
    for(const auto& pin:request.metric_tensors)for(const auto& e:pin.tensor()->values().entries()){
        guard.step();guard.value(e.value);bits=std::max(bits,native_field_detail::integer_bits(e.value));
    }
    return bits;
}
inline numeric::ExactActionLimits limits(const runtime::ExecutionLease& lease,std::span<const Pin> pins,
                                         std::size_t dimension,std::size_t resident) {
    const auto& host=lease.policy().limits;const auto slots=std::min(host.rational_slots,host.sparse_entries);
    if(resident>slots)throw runtime::ExecutionLimitExceeded("native frame selected payload",resident,slots);
    auto work=host.scalar_work;for(const auto& pin:pins)work=std::min(work,pin.limits().max_action_work);
    return {dimension,slots-resident,work,host.integer_bits};
}
inline std::vector<Pin> pins(const Pin& publication,const NativeHodgeFrameRequest& request) {
    std::vector<Pin> result;result.reserve(add(request.metric_tensors.size(),1));result.push_back(publication);
    result.insert(result.end(),request.metric_tensors.begin(),request.metric_tensors.end());return result;
}
inline Matrix metric(const Pin& pin,const Carrier& owner,NativeMetricSelection selection,Guard& guard) {
    require(pin.kind()==storage::NativeAccessionKind::Tensor,"native frame metric requires a declared bilinear-form tensor pin");
    const auto tensor=pin.tensor_declaration();const auto n=owner->domain.logical_size();
    require(tensor->role==storage::TemporalTensorRole::Metric && tensor->input==owner->key && tensor->output==owner->key &&
            tensor->tensor.domain().extents()==std::vector<std::size_t>{n,n} &&
            tensor->tensor.support().count()==tensor->tensor.domain().logical_size(),
            "native frame metric requires complete support on the actual grade carrier");
    Matrix result(n,n);std::size_t diagonal{};
    for(const auto& e:tensor->tensor.values().entries()){
        guard.step();guard.value(e.value);const auto i=e.index/n,j=e.index%n;
        if(selection==NativeMetricSelection::DeclaredDiagonal){require(i==j && e.value>0,"native frame positive diagonal metric");++diagonal;}
        result.set(i,j,e.value);guard.entries(result.nonzeros());
    }
    if(selection==NativeMetricSelection::DeclaredDiagonal)require(diagonal==n,"native frame diagonal measures every direction");
    return result;
}
} // namespace native_frame_detail

class DerivedNativeHodgeDecomposition;
class PreparedNativeHodgeFrames {
    using Pin=storage::PinnedNativeAccession;
    struct State {
        Pin publication;
        NativeHodgeFrameRequest request;
        std::vector<native_frame_detail::Carrier> carriers;
        field::PreparedHodgeFrames frames;
        runtime::ExecutionRequirements payload,retained;
        runtime::WorkAvailability availability;
        std::string policy;
        std::size_t preparation_steps;
        bool fallback;
    };
public:
    PreparedNativeHodgeFrames(const PreparedNativeHodgeFrames&)=default;
    PreparedNativeHodgeFrames& operator=(const PreparedNativeHodgeFrames&)=default;
    static PreparedNativeHodgeFrames prepare(Pin publication,NativeHodgeFrameRequest request,runtime::WorkAvailability available,
                                             runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_frame_detail;
        lease.check_cancelled();runtime::admit_work_availability(publication.source_namespace(),publication.sequence(),available);
        const auto endpoint=publication.endpoint();const auto& source=endpoint->source;
        require(request.dimension_limit>0 && request.grade.value<source.grade_count(),"native frame grade or dimension limit");
        require(request.grade_carriers.size()==source.grade_count(),"native frame requires every declared grade carrier");
        require(request.metrics==NativeMetricSelection::NamedUnit || request.metrics==NativeMetricSelection::DeclaredDiagonal ||
                request.metrics==NativeMetricSelection::DeclaredPositive,"native frame metric selection");
        require(request.metrics==NativeMetricSelection::NamedUnit?request.metric_tensors.empty():request.metric_tensors.size()==source.grade_count(),
                "native frame requires the full declared metric family");
        for(const auto& metric_pin:request.metric_tensors)same(publication,metric_pin);
        auto payload=requirements(publication,request);runtime::admit_execution(payload,lease.policy());
        const auto selected=pins(publication,request);
        Guard guard(limits(lease,selected,request.dimension_limit,payload.rational_slots),[&lease]{lease.check_cancelled();});
        payload.integer_bits=minimum_bits(publication,request,guard);runtime::admit_execution(payload,lease.policy());
        std::vector<Carrier> carriers;carriers.reserve(source.grade_count());
        std::vector<Matrix> metrics;metrics.reserve(source.grade_count());
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};const auto n=source.grade(grade).cell_count();guard.dimension(n);
            auto owner=native_frame_detail::carrier(publication,request.grade_carriers[k],guard);
            require(owner->kind==storage::TemporalCarrierKind::SourceGrade && owner->grade==grade && owner->domain==numeric::IndexDomain::linear(n),
                    "native frames require actual SourceGrade carriers in grade order");
            carriers.push_back(owner);
            if(request.metrics==NativeMetricSelection::NamedUnit)metrics.push_back(Matrix::identity(n));
            else metrics.push_back(metric(request.metric_tensors[k],owner,request.metrics,guard));
        }
        auto source_owner=std::shared_ptr<const core::RelationalComplex>{endpoint,&endpoint->source};
        field::PreparedHodgeFrames frames(std::move(source_owner),std::move(metrics),request.grade,guard);
        auto retained=payload;retained.rational_slots=add(retained.rational_slots,frames.stored_entries());
        retained.sparse_entries=add(retained.rational_slots,frames.stored_index_entries());runtime::admit_execution(retained,lease.policy());
        lease.check_cancelled();return PreparedNativeHodgeFrames(std::make_shared<const State>(State{std::move(publication),std::move(request),
            std::move(carriers),std::move(frames),payload,retained,available,lease.policy().name,guard.steps(),
            lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    Pin publication() const {return state_->publication;}
    const field::PreparedHodgeFrames& numeric_plan() const & {return state_->frames;}
    const field::PreparedHodgeFrames& numeric_plan() const &&=delete;
    std::uint64_t carrier() const noexcept {return state_->carriers[state_->request.grade.value]->key;}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    DerivedNativeHodgeDecomposition execute(Pin input,runtime::WorkAvailability available,
                                            runtime::ExecutionLease lease=runtime::ExecutionLease{}) const;
private:
    explicit PreparedNativeHodgeFrames(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
class DerivedNativeHodgeDecomposition {
public:
    DerivedNativeHodgeDecomposition(const DerivedNativeHodgeDecomposition&)=default;
    DerivedNativeHodgeDecomposition& operator=(const DerivedNativeHodgeDecomposition&)=default;
    const field::HodgeFrameDecomposition& values() const & {return values_;}
    field::HodgeFrameDecomposition values() const && {return values_;}
    const NativeHodgeFrameProvenance& provenance() const & {return report_;}
    NativeHodgeFrameProvenance provenance() const && {return report_;}
    const PreparedNativeHodgeFrames& plan() const & {return owner_;}
    const PreparedNativeHodgeFrames& plan() const &&=delete;
    storage::PinnedNativeAccession input() const {return input_;}
private:
    friend class PreparedNativeHodgeFrames;
    DerivedNativeHodgeDecomposition(PreparedNativeHodgeFrames owner,storage::PinnedNativeAccession input,
                                   field::HodgeFrameDecomposition values,NativeHodgeFrameProvenance report)
        :owner_(std::move(owner)),input_(std::move(input)),values_(std::move(values)),report_(std::move(report)){}
    PreparedNativeHodgeFrames owner_;
    storage::PinnedNativeAccession input_;
    field::HodgeFrameDecomposition values_;
    NativeHodgeFrameProvenance report_;
};
inline DerivedNativeHodgeDecomposition PreparedNativeHodgeFrames::execute(Pin input,runtime::WorkAvailability available,
                                                                        runtime::ExecutionLease lease) const {
    using namespace native_frame_detail;
    const auto pin=state_;lease.check_cancelled();same(pin->publication,input);
    runtime::admit_work_availability(pin->publication.source_namespace(),pin->publication.sequence(),pin->availability);
    runtime::admit_work_availability(pin->publication.source_namespace(),pin->publication.sequence(),available);
    require(input.kind()==storage::NativeAccessionKind::Field,"native frame input requires a stored Field");
    const auto declaration=input.tensor_declaration();const auto owner=pin->carriers[pin->request.grade.value];
    require(declaration->role==storage::TemporalTensorRole::Field && input.carrier().get()==owner.get() &&
            declaration->tensor.domain()==owner->domain && declaration->tensor.support().count()==owner->domain.logical_size(),
            "native frame Field requires its actual selected grade carrier and complete support");
    const auto output_slots=mul(owner->domain.logical_size(),5);
    auto required=pin->retained;required.records=add(required.records,1);
    required.rational_slots=add(required.rational_slots,add(declaration->tensor.values().nonzeros(),output_slots));
    required.sparse_entries=add(required.rational_slots,pin->frames.stored_index_entries());
    required.integer_bits=std::max(required.integer_bits,native_field_detail::tensor_bits(declaration->tensor));
    runtime::admit_execution(required,lease.policy());
    auto selected=pins(pin->publication,pin->request);selected.push_back(input);
    const auto resident=add(pin->payload.rational_slots,add(declaration->tensor.values().nonzeros(),output_slots));
    const auto action=limits(lease,selected,pin->request.dimension_limit,resident);
    Guard guard(action,[&lease]{lease.check_cancelled();});
    numeric::RationalVector field(owner->domain.logical_size());
    for(const auto& e:declaration->tensor.values().entries()){guard.step();guard.value(e.value);field[e.index]=e.value;}
    auto values=pin->frames.decompose(field,guard);
    NativeHodgeFrameProvenance report;
    report.source_namespace=pin->publication.source_namespace();report.sequence=pin->publication.sequence();
    report.input_field=declaration->key;report.carrier=owner->key;report.grade=pin->request.grade;report.metric_selection=pin->request.metrics;
    report.grade_carriers=pin->request.grade_carriers;
    for(const auto& metric_pin:pin->request.metric_tensors)report.metric_tensors.push_back(metric_pin.tensor_declaration()->key);
    report.gradient_dimension=pin->frames.gradient_frame().cols();report.curl_dimension=pin->frames.curl_frame().cols();
    report.harmonic_dimension=pin->frames.harmonic_frame().cols();report.chain_interfaces_checked=pin->frames.chain_interfaces_checked();
    report.selected_requirements=required;report.action_limits=action;report.preparation_steps=pin->preparation_steps;report.execution_steps=guard.steps();
    report.preparation_policy=pin->policy;report.execution_policy=lease.policy().name;
    report.preparation_availability=pin->availability;report.execution_availability=available;
    report.fallback=pin->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
    lease.check_cancelled();return DerivedNativeHodgeDecomposition(PreparedNativeHodgeFrames(pin),std::move(input),std::move(values),std::move(report));
}
} // namespace rfc::inference
