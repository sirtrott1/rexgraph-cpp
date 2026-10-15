#pragma once
#include "rfc/inference/native_field_updates.hpp"

namespace rfc::inference {
struct NativeSelectedObservationRequest {
    storage::PinnedNativeAccession accession;
    // Original source coordinates, in requested order, including repetitions.
    std::vector<std::size_t> coordinates;
    std::size_t dimension_limit{64};
};
struct NativeSelectedObservationProvenance {
    std::string profile{"source-grade-selected-observation-v1"};
    std::uint64_t source_namespace{},sequence{},operation{},accession{},input_field{},input_carrier{},output_carrier{};
    core::Grade source_grade;
    std::vector<std::size_t> coordinates;
    std::vector<storage::StableCellKey> source_cells;
    runtime::ExecutionRequirements selected_requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{},execution_steps{};
    std::string base_preparation_policy,preparation_policy,execution_policy;
    std::optional<runtime::WorkAvailability> base_preparation_availability;
    runtime::WorkAvailability preparation_availability,execution_availability;
    bool uses_full_solve{},fallback{};
};
class DerivedNativeSelectedObservation;
// A coordinate observation S A^{-1} b on declared native carriers. S must be
// supplied completely and equal the ordered selector; it is not a realization,
// reconstruction, chain restriction or inferred transport.
class PreparedNativeSelectedObservation {
    using Pin=storage::PinnedNativeAccession;
    using Guard=numeric::ExactActionGuard;
    struct State {
        PreparedNativeWoodburySolve::Base base;
        NativeSelectedObservationRequest request;
        numeric::PreparedSelectedExactSolve solve;
        numeric::ExactMatrix selector;
        std::vector<storage::StableCellKey> source_cells;
        runtime::ExecutionRequirements payload,retained;
        runtime::WorkAvailability availability;
        std::string policy;
        std::size_t preparation_steps;
        bool fallback;
    };
public:
    using Base=PreparedNativeWoodburySolve::Base;
    PreparedNativeSelectedObservation(const PreparedNativeSelectedObservation&)=default;
    PreparedNativeSelectedObservation& operator=(const PreparedNativeSelectedObservation&)=default;
    static PreparedNativeSelectedObservation prepare(Base base,NativeSelectedObservationRequest request,
            runtime::WorkAvailability available,runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_update_detail;
        lease.check_cancelled();const auto operation=base.publication();same(operation,request.accession);
        runtime::admit_work_availability(operation.source_namespace(),operation.sequence(),available);
        native_update_detail::available(operation,base.preparation_availability());complete_map(request.accession);
        const auto origin=operation.carrier(),target=request.accession.carrier();
        const auto declaration=request.accession.tensor_declaration();const auto n=origin->domain.logical_size(),q=request.coordinates.size();
        require(request.dimension_limit>0,"native observation dimension limit must be positive");
        require(origin->kind==storage::TemporalCarrierKind::SourceGrade &&
                origin->domain==numeric::IndexDomain::linear(n) && base.numeric_solve().matrix().rows()==n,
                "native observation requires the actual SourceGrade solve carrier");
        require(declaration->role==storage::TemporalTensorRole::Accession && declaration->input==origin->key &&
                target->domain==numeric::IndexDomain::linear(q) &&
                declaration->tensor.domain().extents()==std::vector<std::size_t>{q,n},
                "native observation requires a declared accession on its actual input and output carriers");
        if(n>request.dimension_limit || q>request.dimension_limit)throw std::length_error("native observation dimension limit");
        const std::array<Pin,2> pins{operation,request.accession};auto payload_use=selected(pins,2);
        payload_use.records=add(payload_use.records,q);runtime::admit_execution(payload_use,lease.policy());
        Guard guard(limits(lease,pins,request.dimension_limit,payload_use.rational_slots),[&lease]{lease.check_cancelled();});
        auto selector=materialize(request.accession,guard);numeric::ExactMatrix expected(q,n);
        std::vector<storage::StableCellKey> cells;cells.reserve(q);
        const auto& arena=operation.endpoint()->source.grade(origin->grade);
        for(std::size_t i=0;i<q;++i){
            guard.step();const auto coordinate=request.coordinates[i];
            if(coordinate>=n)throw std::out_of_range("native observation original coordinate");
            expected.set(i,coordinate,1);
            cells.push_back({origin->grade,origin->grade.value==0?arena.identities.at(coordinate):arena.relation_identities.at(coordinate).value});
        }
        require(selector==expected,"native accession differs from the complete ordered coordinate selector");
        auto solve=base.numeric_solve().prepare_selected(request.coordinates,guard);
        auto retained=payload_use;retained.rational_slots=add(retained.rational_slots,add(solve.stored_entries(),selector.nonzeros()));
        const auto indexes=add(solve.stored_index_entries(),q);
        retained.sparse_entries=add(retained.rational_slots,indexes);runtime::admit_execution(retained,lease.policy());
        guard.entries(add(solve.stored_entries(),selector.nonzeros()));guard.entries(indexes);lease.check_cancelled();
        const bool fallback=base.fallback() || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        auto state=std::make_shared<const State>(State{std::move(base),std::move(request),std::move(solve),std::move(selector),
            std::move(cells),payload_use,retained,available,lease.policy().name,guard.steps(),fallback});
        return PreparedNativeSelectedObservation(std::move(state));
    }
    Base base_solve() const {return state_->base;}
    Pin publication() const {return state_->request.accession;}
    const numeric::PreparedSelectedExactSolve& numeric_plan() const & {return state_->solve;}
    const numeric::PreparedSelectedExactSolve& numeric_plan() const &&=delete;
    const std::vector<storage::StableCellKey>& source_cells() const & {return state_->source_cells;}
    const std::vector<storage::StableCellKey>& source_cells() const &&=delete;
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    DerivedNativeSelectedObservation execute(Pin input,runtime::WorkAvailability available,
                                             runtime::ExecutionLease lease=runtime::ExecutionLease{}) const;
private:
    explicit PreparedNativeSelectedObservation(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
class DerivedNativeSelectedObservation {
public:
    const numeric::RationalVector& values() const & {return values_;}
    numeric::RationalVector values() const && {return values_;}
    const NativeSelectedObservationProvenance& provenance() const & {return report_;}
    NativeSelectedObservationProvenance provenance() const && {return report_;}
    const PreparedNativeSelectedObservation& plan() const & {return owner_;}
    const PreparedNativeSelectedObservation& plan() const &&=delete;
    storage::PinnedNativeAccession publication() const {return owner_.publication();}
    storage::PinnedNativeAccession input() const {return input_;}
    std::shared_ptr<const storage::TemporalCarrier> carrier() const {return publication().carrier();}
private:
    friend class PreparedNativeSelectedObservation;
    DerivedNativeSelectedObservation(PreparedNativeSelectedObservation owner,storage::PinnedNativeAccession input,
                                    numeric::RationalVector values,NativeSelectedObservationProvenance report)
        :owner_(std::move(owner)),input_(std::move(input)),values_(std::move(values)),report_(std::move(report)){}
    PreparedNativeSelectedObservation owner_;
    storage::PinnedNativeAccession input_;
    numeric::RationalVector values_;
    NativeSelectedObservationProvenance report_;
};
inline DerivedNativeSelectedObservation PreparedNativeSelectedObservation::execute(Pin input,runtime::WorkAvailability available,
                                                                                  runtime::ExecutionLease lease) const {
    using namespace native_update_detail;
    const auto pin=state_;lease.check_cancelled();const auto operation=pin->base.publication();same(operation,input);
    runtime::admit_work_availability(operation.source_namespace(),operation.sequence(),pin->availability);
    runtime::admit_work_availability(operation.source_namespace(),operation.sequence(),available);
    native_update_detail::available(operation,pin->base.preparation_availability());
    require(input.kind()==storage::NativeAccessionKind::Field,"native observation input requires a stored Field");
    const auto declaration=input.tensor_declaration();const auto origin=operation.carrier();
    require(declaration->role==storage::TemporalTensorRole::Field && input.carrier().get()==origin.get() &&
            declaration->tensor.domain()==origin->domain && declaration->tensor.support().count()==origin->domain.logical_size(),
            "native observation input requires its actual source carrier and complete support");
    const auto n=origin->domain.logical_size(),q=pin->request.coordinates.size();
    const std::array<Pin,3> pins{operation,pin->request.accession,input};const auto raw=selected(pins,2);
    const auto workspace=add(n,q);auto required=pin->retained;required.records=add(required.records,1);
    required.rational_slots=add(required.rational_slots,add(payload(input),workspace));
    const auto indexes=add(pin->solve.stored_index_entries(),q);required.sparse_entries=add(required.rational_slots,indexes);
    required.integer_bits=std::max(required.integer_bits,raw.integer_bits);runtime::admit_execution(required,lease.policy());
    const auto action=limits(lease,pins,pin->request.dimension_limit,add(raw.rational_slots,workspace));
    Guard guard(action,[&lease]{lease.check_cancelled();});
    guard.entries(add(pin->solve.stored_entries(),pin->selector.nonzeros()));guard.entries(indexes);guard.matrix(pin->selector);
    const auto rhs=field_values(input,guard);auto values=pin->solve.apply(rhs,guard);
    NativeSelectedObservationProvenance report;
    report.source_namespace=operation.source_namespace();report.sequence=operation.sequence();
    report.operation=operation.tensor_declaration()->key;report.accession=pin->request.accession.tensor_declaration()->key;
    report.input_field=declaration->key;report.input_carrier=origin->key;report.output_carrier=pin->request.accession.carrier()->key;
    report.source_grade=origin->grade;report.coordinates=pin->request.coordinates;report.source_cells=pin->source_cells;
    report.selected_requirements=required;report.action_limits=action;report.preparation_steps=pin->preparation_steps;report.execution_steps=guard.steps();
    report.base_preparation_policy=pin->base.preparation_policy();report.base_preparation_availability=pin->base.preparation_availability();
    report.preparation_policy=pin->policy;report.preparation_availability=pin->availability;
    report.execution_policy=lease.policy().name;report.execution_availability=available;report.uses_full_solve=pin->solve.uses_full_solve();
    report.fallback=pin->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
    lease.check_cancelled();return DerivedNativeSelectedObservation(PreparedNativeSelectedObservation(pin),std::move(input),std::move(values),std::move(report));
}
} // namespace rfc::inference
