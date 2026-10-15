#pragma once
#include "rfc/inference/native_field_updates.hpp"
#include "rfc/numeric/prepared_linear_observation.hpp"

namespace rfc::inference {
struct NativeLinearObservationRequest {
    storage::PinnedNativeAccession accession;
    std::size_t dimension_limit{64};
};
struct NativeLinearObservationProvenance {
    std::string profile{"source-grade-linear-observation-v1"};
    std::uint64_t source_namespace{},sequence{},operation{},accession{},input_field{},input_carrier{},output_carrier{};
    core::Grade source_grade;
    std::size_t output_rows{},distinct_coefficient_rows{},certified_nonzero_rows{};
    runtime::ExecutionRequirements selected_requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{},execution_steps{};
    std::string base_preparation_policy,preparation_policy,execution_policy;
    std::optional<runtime::WorkAvailability> base_preparation_availability;
    runtime::WorkAvailability preparation_availability,execution_availability;
    bool uses_full_solve{},fallback{};
};
class DerivedNativeLinearObservation;
class PreparedNativeLinearObservation {
    using Pin=storage::PinnedNativeAccession;
    struct State {
        PreparedNativeWoodburySolve::Base base;
        NativeLinearObservationRequest request;
        numeric::PreparedExactLinearObservation observation;
        runtime::ExecutionRequirements retained;
        runtime::WorkAvailability availability;
        std::string policy;
        std::size_t preparation_steps;
        bool fallback;
    };
public:
    using Base=PreparedNativeWoodburySolve::Base;
    PreparedNativeLinearObservation(const PreparedNativeLinearObservation&)=default;
    PreparedNativeLinearObservation& operator=(const PreparedNativeLinearObservation&)=default;
    static PreparedNativeLinearObservation prepare(Base base,NativeLinearObservationRequest request,runtime::WorkAvailability available,
                                                  runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_update_detail;
        lease.check_cancelled();const auto operation=base.publication();same(operation,request.accession);
        runtime::admit_work_availability(operation.source_namespace(),operation.sequence(),available);
        native_update_detail::available(operation,base.preparation_availability());complete_map(request.accession);
        const auto origin=operation.carrier(),target=request.accession.carrier();const auto declaration=request.accession.tensor_declaration();
        const auto n=origin->domain.logical_size(),q=target->domain.logical_size();
        require(request.dimension_limit>0,"native linear observation dimension limit must be positive");
        require(origin->kind==storage::TemporalCarrierKind::SourceGrade && origin->domain==numeric::IndexDomain::linear(n) &&
                base.numeric_solve().matrix().rows()==n,"native linear observation requires the actual SourceGrade solve carrier");
        require(declaration->role==storage::TemporalTensorRole::Accession && declaration->input==origin->key &&
                target->domain==numeric::IndexDomain::linear(q) && declaration->tensor.domain().extents()==std::vector<std::size_t>{q,n},
                "native linear observation requires a complete declared accession on its actual input and linear output carriers");
        if(n>request.dimension_limit || q>request.dimension_limit)throw std::length_error("native linear observation dimension limit");
        const std::array<Pin,2> pins{operation,request.accession};auto required=selected(pins,2);
        required.records=add(required.records,q);runtime::admit_execution(required,lease.policy());
        Guard guard(limits(lease,pins,request.dimension_limit,required.rational_slots),[&lease]{lease.check_cancelled();});
        auto observation=numeric::PreparedExactLinearObservation::prepare(base.numeric_solve(),materialize(request.accession,guard),guard);
        required.rational_slots=add(required.rational_slots,observation.stored_entries());
        required.sparse_entries=add(required.rational_slots,observation.stored_index_entries());runtime::admit_execution(required,lease.policy());
        auto preparing=required;preparing.rational_slots=add(preparing.rational_slots,observation.workspace_slots());
        preparing.sparse_entries=add(preparing.rational_slots,observation.stored_index_entries());runtime::admit_execution(preparing,lease.policy());
        lease.check_cancelled();const bool fallback=base.fallback() || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        auto state=std::make_shared<const State>(State{std::move(base),std::move(request),std::move(observation),required,available,
            lease.policy().name,guard.steps(),fallback});return PreparedNativeLinearObservation(std::move(state));
    }
    Base base_solve() const {return state_->base;}
    Pin publication() const {return state_->request.accession;}
    const numeric::PreparedExactLinearObservation& numeric_plan() const & {return state_->observation;}
    const numeric::PreparedExactLinearObservation& numeric_plan() const &&=delete;
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    DerivedNativeLinearObservation execute(Pin input,runtime::WorkAvailability available,
                                           runtime::ExecutionLease lease=runtime::ExecutionLease{}) const;
private:
    explicit PreparedNativeLinearObservation(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
class DerivedNativeLinearObservation {
public:
    const numeric::RationalVector& values() const & {return values_;}
    numeric::RationalVector values() const && {return values_;}
    const NativeLinearObservationProvenance& provenance() const & {return report_;}
    NativeLinearObservationProvenance provenance() const && {return report_;}
    const PreparedNativeLinearObservation& plan() const & {return owner_;}
    const PreparedNativeLinearObservation& plan() const &&=delete;
    storage::PinnedNativeAccession publication() const {return owner_.publication();}
    storage::PinnedNativeAccession input() const {return input_;}
    std::shared_ptr<const storage::TemporalCarrier> carrier() const {return publication().carrier();}
private:
    friend class PreparedNativeLinearObservation;
    DerivedNativeLinearObservation(PreparedNativeLinearObservation owner,storage::PinnedNativeAccession input,
                                  numeric::RationalVector values,NativeLinearObservationProvenance report)
        :owner_(std::move(owner)),input_(std::move(input)),values_(std::move(values)),report_(std::move(report)){}
    PreparedNativeLinearObservation owner_;
    storage::PinnedNativeAccession input_;
    numeric::RationalVector values_;
    NativeLinearObservationProvenance report_;
};
inline DerivedNativeLinearObservation PreparedNativeLinearObservation::execute(Pin input,runtime::WorkAvailability available,
                                                                               runtime::ExecutionLease lease) const {
    using namespace native_update_detail;
    const auto pin=state_;lease.check_cancelled();const auto operation=pin->base.publication();same(operation,input);
    runtime::admit_work_availability(operation.source_namespace(),operation.sequence(),pin->availability);
    runtime::admit_work_availability(operation.source_namespace(),operation.sequence(),available);
    native_update_detail::available(operation,pin->base.preparation_availability());
    require(input.kind()==storage::NativeAccessionKind::Field,"native linear observation input requires a stored Field");
    const auto declaration=input.tensor_declaration(),origin=operation.tensor_declaration();const auto carrier=operation.carrier();
    require(declaration->role==storage::TemporalTensorRole::Field && input.carrier().get()==carrier.get() &&
            declaration->tensor.domain()==carrier->domain && declaration->tensor.support().count()==carrier->domain.logical_size(),
            "native linear observation input requires its actual source carrier and complete support");
    const std::array<Pin,3> pins{operation,pin->request.accession,input};const auto raw=selected(pins,2);
    auto required=pin->retained;required.records=add(required.records,1);
    required.rational_slots=add(required.rational_slots,add(payload(input),pin->observation.workspace_slots()));
    required.sparse_entries=add(required.rational_slots,pin->observation.stored_index_entries());
    required.integer_bits=std::max(required.integer_bits,raw.integer_bits);runtime::admit_execution(required,lease.policy());
    const auto action=limits(lease,pins,pin->request.dimension_limit,raw.rational_slots);
    Guard guard(action,[&lease]{lease.check_cancelled();});auto rhs=field_values(input,guard);
    auto values=pin->observation.apply(std::move(rhs),guard);NativeLinearObservationProvenance report;
    report.source_namespace=operation.source_namespace();report.sequence=operation.sequence();report.operation=origin->key;
    report.accession=pin->request.accession.tensor_declaration()->key;report.input_field=declaration->key;
    report.input_carrier=carrier->key;report.output_carrier=pin->request.accession.carrier()->key;report.source_grade=carrier->grade;
    report.output_rows=values.size();report.distinct_coefficient_rows=pin->observation.distinct_rows();
    report.certified_nonzero_rows=pin->observation.certified_nonzero_rows();report.uses_full_solve=pin->observation.uses_full_solve();
    report.selected_requirements=required;report.action_limits=action;report.preparation_steps=pin->preparation_steps;report.execution_steps=guard.steps();
    report.base_preparation_policy=pin->base.preparation_policy();report.base_preparation_availability=pin->base.preparation_availability();
    report.preparation_policy=pin->policy;report.preparation_availability=pin->availability;
    report.execution_policy=lease.policy().name;report.execution_availability=available;
    report.fallback=pin->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
    lease.check_cancelled();return DerivedNativeLinearObservation(PreparedNativeLinearObservation(pin),std::move(input),std::move(values),std::move(report));
}
} // namespace rfc::inference
