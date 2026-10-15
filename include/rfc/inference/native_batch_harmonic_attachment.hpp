#pragma once
#include "rfc/field/batch_harmonic_attachment.hpp"
#include "rfc/inference/native_hodge_frames.hpp"

namespace rfc::inference {
struct NativeBatchAttachmentRequest {
    std::vector<core::BatchAttachmentCell> cells;
    std::size_t dimension_limit{64};
};
struct NativeBatchAttachmentProvenance {
    std::string profile{"source-grade-staged-batch-harmonic-transfer-v1"};
    std::uint64_t source_namespace{},sequence{},input_field{},carrier{};
    core::Grade grade{},attachment_grade{};
    std::vector<core::RelationIdentity> appended;
    std::size_t harmonic_removed{},harmonic_remaining{},upper_homology_added{};
    std::size_t preparation_steps{},execution_steps{};
    runtime::ExecutionRequirements selected_requirements;
    numeric::ExactActionLimits action_limits;
    runtime::WorkAvailability preparation_availability,execution_availability;
    std::string preparation_policy,execution_policy;
    bool exact_cpu_fallback{};
};
class DerivedNativeBatchHodgeDecomposition;

// An owned staged source mutation and its exact selected-grade field update.
// The original publication, metrics and input remain actual retained pins. The
// staged source has no publication pin until a caller actually publishes it.
class PreparedNativeBatchHarmonicAttachment {
    using Pin=storage::PinnedNativeAccession;
    struct State {
        PreparedNativeHodgeFrames original;
        field::PreparedBatchHarmonicAttachment plan;
        runtime::ExecutionRequirements retained;
        runtime::WorkAvailability availability;
        std::size_t dimension,steps;
        std::string policy;
        bool fallback;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeBatchHarmonicAttachment(std::shared_ptr<const State> state):state_(std::move(state)){}
public:
    static PreparedNativeBatchHarmonicAttachment prepare(PreparedNativeHodgeFrames original,
            NativeBatchAttachmentRequest request,runtime::WorkAvailability available,
            runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_frame_detail;lease.check_cancelled();const auto publication=original.publication();
        runtime::admit_work_availability(publication.source_namespace(),publication.sequence(),original.preparation_availability());
        runtime::admit_work_availability(publication.source_namespace(),publication.sequence(),available);
        require(request.dimension_limit>0 && !request.cells.empty(),"native batch requires actual attachment declarations and a positive dimension limit");
        auto required=original.retained_requirements();
        for(const auto& cell:request.cells) {
            lease.check_cancelled();required.records=add(required.records,add(1,cell.incidences.size()));
            required.rational_slots=add(required.rational_slots,cell.incidences.size());
            for(const auto& incidence:cell.incidences)required.integer_bits=std::max(required.integer_bits,native_field_detail::integer_bits(incidence.magnitude));
        }
        required.sparse_entries=std::max(required.sparse_entries,required.rational_slots);runtime::admit_execution(required,lease.policy());
        const auto selected=pins(publication,original.request());
        Guard guard(limits(lease,selected,request.dimension_limit,required.rational_slots),[&lease]{lease.check_cancelled();});
        auto plan=field::PreparedBatchHarmonicAttachment::prepare(original.numeric_plan(),std::move(request.cells),guard);
        const auto extra=plan.stored_entries()-original.numeric_plan().stored_entries();
        required.rational_slots=add(required.rational_slots,extra);
        required.sparse_entries=add(required.rational_slots,plan.stored_index_entries());
        for(std::size_t k=0;k<plan.attachment().after()->grade_count();++k) {
            const auto& arena=plan.attachment().after()->grade(core::Grade{static_cast<std::uint16_t>(k)});
            required.records=add(required.records,add(arena.cell_count(),arena.incidence_count()));
        }
        runtime::admit_execution(required,lease.policy());lease.check_cancelled();
        return PreparedNativeBatchHarmonicAttachment(std::make_shared<const State>(State{std::move(original),std::move(plan),required,
            available,request.dimension_limit,guard.steps(),lease.policy().name,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    const PreparedNativeHodgeFrames& original_frames() const & {return state_->original;}
    const field::PreparedBatchHarmonicAttachment& numeric_plan() const & {return state_->plan;}
    const PreparedNativeHodgeFrames& original_frames() const &&=delete;
    const field::PreparedBatchHarmonicAttachment& numeric_plan() const &&=delete;
    std::shared_ptr<const core::RelationalComplex> staged_source() const {return state_->plan.attachment().after();}
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->steps;}
    DerivedNativeBatchHodgeDecomposition execute(Pin input,runtime::WorkAvailability available,
        runtime::ExecutionLease lease=runtime::ExecutionLease{}) const;
};
class DerivedNativeBatchHodgeDecomposition {
    friend class PreparedNativeBatchHarmonicAttachment;
    PreparedNativeBatchHarmonicAttachment owner_;
    storage::PinnedNativeAccession input_;
    field::BatchHodgeDecomposition values_;
    NativeBatchAttachmentProvenance report_;
    DerivedNativeBatchHodgeDecomposition(PreparedNativeBatchHarmonicAttachment owner,storage::PinnedNativeAccession input,
            field::BatchHodgeDecomposition values,NativeBatchAttachmentProvenance report)
        :owner_(std::move(owner)),input_(std::move(input)),values_(std::move(values)),report_(std::move(report)){}
public:
    const field::BatchHodgeDecomposition& sparse_values() const & {return values_;}
    const NativeBatchAttachmentProvenance& provenance() const & {return report_;}
    const PreparedNativeBatchHarmonicAttachment& plan() const & {return owner_;}
    const field::BatchHodgeDecomposition& sparse_values() const &&=delete;
    const NativeBatchAttachmentProvenance& provenance() const &&=delete;
    const PreparedNativeBatchHarmonicAttachment& plan() const &&=delete;
    storage::PinnedNativeAccession input() const {return input_;}
};
inline DerivedNativeBatchHodgeDecomposition PreparedNativeBatchHarmonicAttachment::execute(Pin input,
        runtime::WorkAvailability available,runtime::ExecutionLease lease) const {
    using namespace native_frame_detail;const auto pin=state_;lease.check_cancelled();const auto publication=pin->original.publication();
    same(publication,input);
    runtime::admit_work_availability(publication.source_namespace(),publication.sequence(),pin->availability);
    runtime::admit_work_availability(publication.source_namespace(),publication.sequence(),available);
    require(input.kind()==storage::NativeAccessionKind::Field,"native batch input must be an actual stored Field pin");
    const auto declaration=input.tensor_declaration();const auto carrier=input.carrier();
    require(carrier->kind==storage::TemporalCarrierKind::SourceGrade && carrier->key==pin->original.carrier() &&
        carrier->grade==pin->original.numeric_plan().grade() && declaration->role==storage::TemporalTensorRole::Field &&
        declaration->tensor.domain()==carrier->domain && declaration->tensor.support().count()==carrier->domain.logical_size(),
        "native batch Field requires its original complete selected SourceGrade carrier");
    auto required=pin->retained;required.records=add(required.records,1);
    required.rational_slots=add(required.rational_slots,declaration->tensor.values().nonzeros());
    required.sparse_entries=add(required.sparse_entries,declaration->tensor.values().nonzeros());
    required.integer_bits=std::max(required.integer_bits,native_field_detail::tensor_bits(declaration->tensor));
    runtime::admit_execution(required,lease.policy());
    auto selected=pins(publication,pin->original.request());selected.push_back(input);
    const auto action=limits(lease,selected,pin->dimension,declaration->tensor.values().nonzeros());
    Guard guard(action,[&lease]{lease.check_cancelled();});
    const auto actual_carrier=native_frame_detail::carrier(publication,pin->original.carrier(),guard);
    require(carrier.get()==actual_carrier.get(),"native batch requires the actual original selected carrier owner");
    auto values=pin->plan.decompose(declaration->tensor.values(),guard);
    const auto output=native_field_detail::add(values.gradient.nonzeros(),add(values.curl.nonzeros(),add(values.harmonic.nonzeros(),
        add(values.transferred.nonzeros(),add(values.transfer_coordinates.nonzeros(),values.reconstructed.nonzeros())))));
    required.rational_slots=add(required.rational_slots,output);required.sparse_entries=add(required.sparse_entries,output);
    runtime::admit_execution(required,lease.policy());
    NativeBatchAttachmentProvenance report;
    report.source_namespace=publication.source_namespace();report.sequence=publication.sequence();report.input_field=declaration->key;
    report.carrier=carrier->key;report.grade=pin->original.numeric_plan().grade();report.attachment_grade=pin->plan.attachment().upper_grade();
    for(const auto cell:pin->plan.attachment().appended_cells())report.appended.push_back(pin->plan.attachment().after()->relation_identity({cell.grade,cell.index}));
    report.harmonic_removed=pin->plan.harmonic_removed();report.harmonic_remaining=pin->plan.harmonic_remaining();
    report.upper_homology_added=pin->plan.upper_homology_added();report.preparation_steps=pin->steps;report.execution_steps=guard.steps();
    report.selected_requirements=required;report.action_limits=action;report.preparation_availability=pin->availability;report.execution_availability=available;
    report.preparation_policy=pin->policy;report.execution_policy=lease.policy().name;
    report.exact_cpu_fallback=pin->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
    lease.check_cancelled();return DerivedNativeBatchHodgeDecomposition(PreparedNativeBatchHarmonicAttachment(pin),std::move(input),std::move(values),std::move(report));
}
} // namespace rfc::inference
