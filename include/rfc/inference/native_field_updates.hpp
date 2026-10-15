#pragma once

#include "rfc/inference/native_field_execution.hpp"
#include "rfc/field/low_rank_update.hpp"
#include "rfc/runtime/work_graph.hpp"
#include <array>

namespace rfc::inference {

struct NativeWoodburyRequest {
    storage::PinnedNativeAccession base, directions, coupling, row_map, updated;
    std::size_t dimension_limit{64};
};
struct NativeWoodburyProvenance {
    std::string profile{"source-grade-native-woodbury-v1"};
    std::uint64_t source_namespace{}, sequence{}, input_field{}, carrier{}, update_carrier{};
    std::uint64_t base{}, directions{}, coupling{}, row_map{}, updated{};
    // Operand bit minimum and scoped container counts. Retained factors and
    // intermediate integer growth are checked under action_limits on each use.
    runtime::ExecutionRequirements selected_requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string base_preparation_policy, preparation_policy, execution_policy;
    std::optional<runtime::WorkAvailability> base_preparation_availability, preparation_availability, execution_availability;
    bool reused_base{}, fallback{};
};

namespace native_update_detail {
using Pin=storage::PinnedNativeAccession;
using Matrix=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using native_field_detail::require;
using native_field_detail::add;
using native_field_detail::mul;
inline void same(const Pin& a,const Pin& b) {
    native_field_detail::same_publication(a,b);
    require(a.same_execution_authority(b),"native update requires the same execution authority");
}
inline void available(const Pin& pin,const std::optional<runtime::WorkAvailability>& cutoff) {
    runtime::admit_work_availability(pin.source_namespace(),pin.sequence(),cutoff);
}
inline void complete_map(const Pin& pin) {
    require(pin.kind()==storage::NativeAccessionKind::Operator,"native update requires a declared operator pin");
    const auto t=pin.tensor();
    require(t->domain().extents().size()==2 && t->support().count()==t->domain().logical_size(),
            "native update requires complete declared map support");
}
inline std::size_t payload(const Pin& pin) {return pin.tensor()->values().nonzeros();}
inline Matrix materialize(const Pin& pin,Guard& guard) {
    const auto tensor=pin.tensor();const auto& shape=tensor->domain().extents();
    guard.dimension(shape.at(0));guard.dimension(shape.at(1));guard.entries(payload(pin));
    Matrix out(shape[0],shape[1]);
    for(const auto& e:tensor->values().entries()) {
        guard.step();guard.value(e.value);out.set(e.index/shape[1],e.index%shape[1],e.value);
    }
    return out;
}
inline runtime::ExecutionRequirements selected(std::span<const Pin> pins,std::size_t carriers) {
    runtime::ExecutionRequirements result;result.records=add(pins.size(),carriers);
    for(const auto& pin:pins) {
        result.rational_slots=add(result.rational_slots,payload(pin));
        for(const auto& e:pin.tensor()->values().entries())
            result.integer_bits=std::max(result.integer_bits,native_field_detail::integer_bits(e.value));
    }
    result.sparse_entries=result.rational_slots;return result;
}
inline numeric::ExactActionLimits limits(const runtime::ExecutionLease& lease,std::span<const Pin> pins,
                                        std::size_t dimension,std::size_t resident) {
    const auto& host=lease.policy().limits;
    const auto slots=std::min(host.rational_slots,host.sparse_entries);
    if(resident>slots)throw runtime::ExecutionLimitExceeded("native update selected payload",resident,slots);
    std::size_t work=host.scalar_work;
    for(const auto& pin:pins)work=std::min(work,pin.limits().max_action_work);
    return {dimension,slots-resident,work,host.integer_bits};
}
inline numeric::RationalVector field_values(const Pin& input,Guard& guard) {
    const auto tensor=input.tensor();guard.dimension(tensor->domain().logical_size());
    numeric::RationalVector out(tensor->domain().logical_size());
    for(const auto& e:tensor->values().entries()){guard.step();guard.value(e.value);out[e.index]=e.value;}
    return out;
}
} // namespace native_update_detail

class DerivedNativeWoodburyField;
// Bound to five complete native declarations on one actual endpoint. The row
// map is supplied independently; no bilinear form or adjoint is inferred.
class PreparedNativeWoodburySolve {
    using Pin=storage::PinnedNativeAccession;
    using Matrix=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct BaseState {
        Pin operation;
        numeric::PreparedExactSolve solve;
        std::string policy;
        std::optional<runtime::WorkAvailability> cutoff;
        bool fallback;
        BaseState(Pin pin,numeric::PreparedExactSolve value,std::string name,
                  std::optional<runtime::WorkAvailability> availability,bool cpu_fallback)
            :operation(std::move(pin)),solve(std::move(value)),policy(std::move(name)),cutoff(availability),fallback(cpu_fallback){}
    };
public:
    class Base {
    public:
        Base(const Base&)=default;
        Base& operator=(const Base&)=default;
        Pin publication() const {return state_->operation;}
        const numeric::PreparedExactSolve& numeric_solve() const & {return state_->solve;}
        const numeric::PreparedExactSolve& numeric_solve() const &&=delete;
        const std::string& preparation_policy() const & {return state_->policy;}
        const std::string& preparation_policy() const &&=delete;
        std::optional<runtime::WorkAvailability> preparation_availability() const {return state_->cutoff;}
        bool fallback() const noexcept {return state_->fallback;}
    private:
        friend class PreparedNativeWoodburySolve;
        explicit Base(std::shared_ptr<const BaseState> value):state_(std::move(value)){}
        std::shared_ptr<const BaseState> state_;
    };
    PreparedNativeWoodburySolve(const PreparedNativeWoodburySolve&)=default;
    PreparedNativeWoodburySolve& operator=(const PreparedNativeWoodburySolve&)=default;
    static Base prepare_base(Pin operation,runtime::ExecutionLease lease=runtime::ExecutionLease{},
                            std::optional<runtime::WorkAvailability> cutoff={},std::size_t dimension_limit=64) {
        return prepare_base_impl(std::move(operation),{},std::move(lease),cutoff,dimension_limit);
    }
    static Base prepare_base_ordered(Pin operation,numeric::ExactSolveOrdering ordering,
                            runtime::ExecutionLease lease=runtime::ExecutionLease{},
                            std::optional<runtime::WorkAvailability> cutoff={},std::size_t dimension_limit=64) {
        return prepare_base_impl(std::move(operation),std::move(ordering),std::move(lease),cutoff,dimension_limit);
    }
    static PreparedNativeWoodburySolve prepare(NativeWoodburyRequest request,runtime::ExecutionLease lease=runtime::ExecutionLease{},
                                               std::optional<runtime::WorkAvailability> cutoff={}) {
        return prepare_impl(std::move(request),{},std::move(lease),cutoff);
    }
    static PreparedNativeWoodburySolve prepare(NativeWoodburyRequest request,Base base,runtime::ExecutionLease lease=runtime::ExecutionLease{},
                                               std::optional<runtime::WorkAvailability> cutoff={}) {
        return prepare_impl(std::move(request),std::move(base),std::move(lease),cutoff);
    }
    Base base_solve() const {return state_->base;}
    Pin publication() const {return state_->request.updated;}
    const field::PreparedWoodburySolve& numeric_plan() const & {return state_->solve;}
    const field::PreparedWoodburySolve& numeric_plan() const &&=delete;
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    DerivedNativeWoodburyField execute(Pin input,runtime::ExecutionLease lease=runtime::ExecutionLease{},
                                       std::optional<runtime::WorkAvailability> cutoff={}) const;
private:
    struct State {
        NativeWoodburyRequest request;
        Base base;
        field::PreparedWoodburySolve solve;
        Matrix updated;
        runtime::ExecutionRequirements retained;
        std::string policy;
        std::optional<runtime::WorkAvailability> cutoff;
        std::size_t preparation_steps;
        bool reused_base,fallback;
        State(NativeWoodburyRequest r,Base b,field::PreparedWoodburySolve s,Matrix a,
              runtime::ExecutionRequirements use,std::string name,std::optional<runtime::WorkAvailability> availability,
              std::size_t work,bool reused,bool cpu_fallback)
            :request(std::move(r)),base(std::move(b)),solve(std::move(s)),updated(std::move(a)),retained(use),
             policy(std::move(name)),cutoff(availability),preparation_steps(work),reused_base(reused),fallback(cpu_fallback){}
    };
    explicit PreparedNativeWoodburySolve(std::shared_ptr<const State> state):state_(std::move(state)){}
    static Base prepare_base_impl(Pin operation,std::optional<numeric::ExactSolveOrdering> ordering,
            runtime::ExecutionLease lease,std::optional<runtime::WorkAvailability> cutoff,std::size_t dimension_limit) {
        using namespace native_update_detail;
        lease.check_cancelled();available(operation,cutoff);validate_base(operation,dimension_limit);
        const std::array<Pin,1> pins{operation};const auto required=selected(pins,1);
        runtime::admit_execution(required,lease.policy());
        Guard guard(limits(lease,pins,dimension_limit,required.rational_slots),[&lease]{lease.check_cancelled();});
        auto matrix=materialize(operation,guard);
        auto factor=ordering?numeric::PreparedExactSolve::prepare_ordered(std::move(matrix),std::move(*ordering),guard):
                            numeric::PreparedExactSolve(std::move(matrix),guard);
        guard.entries(factor.stored_entries());lease.check_cancelled();
        return Base(std::make_shared<const BaseState>(std::move(operation),std::move(factor),lease.policy().name,
            cutoff,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu));
    }
    static void validate_base(const Pin& operation,std::size_t limit) {
        using namespace native_update_detail;complete_map(operation);
        const auto declaration=operation.tensor_declaration();const auto carrier=operation.carrier();
        require(declaration->input==declaration->output && carrier->kind==storage::TemporalCarrierKind::SourceGrade &&
                carrier->domain==numeric::IndexDomain::linear(carrier->domain.logical_size()),
                "native baseline requires an endomorphism on its declared SourceGrade");
        require(limit>0,"native update dimension limit must be positive");
        if(carrier->domain.logical_size()>limit)throw std::length_error("native update dimension limit");
    }
    static PreparedNativeWoodburySolve prepare_impl(NativeWoodburyRequest request,std::optional<Base> base,
            runtime::ExecutionLease lease,std::optional<runtime::WorkAvailability> cutoff) {
        using namespace native_update_detail;
        lease.check_cancelled();validate_base(request.base,request.dimension_limit);
        const std::array<Pin,5> pins{request.base,request.directions,request.coupling,request.row_map,request.updated};
        for(const auto& pin:pins){same(request.base,pin);available(pin,cutoff);complete_map(pin);}
        const auto a=request.base.tensor_declaration(),u=request.directions.tensor_declaration(),
                   c=request.coupling.tensor_declaration(),row=request.row_map.tensor_declaration(),target=request.updated.tensor_declaration();
        require(u->output==a->output && c->input==u->input && c->output==u->input &&
                row->input==a->output && row->output==u->input && target->input==a->input && target->output==a->output,
                "native update declaration carrier mismatch");
        const auto update=request.coupling.carrier();
        require(update->domain==numeric::IndexDomain::linear(update->domain.logical_size()),"native update carrier must have a declared linear domain");
        if(update->domain.logical_size()>request.dimension_limit)throw std::length_error("native update dimension limit");
        auto required=selected(pins,2);runtime::admit_execution(required,lease.policy());
        Guard guard(limits(lease,pins,request.dimension_limit,required.rational_slots),[&lease]{lease.check_cancelled();});
        const bool reused=base.has_value();
        if(base){
            same(base->state_->operation,request.base);available(base->state_->operation,base->state_->cutoff);
            require(base->state_->operation.tensor_declaration().get()==a.get(),"native update requires the actual prepared base operator");
            if(base->state_->solve.matrix().rows()>request.dimension_limit)throw std::length_error("native update dimension limit");
        }else{
            auto factor=numeric::PreparedExactSolve(materialize(request.base,guard),guard);
            base.emplace(Base(std::make_shared<const BaseState>(request.base,std::move(factor),lease.policy().name,
                cutoff,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu)));
        }
        const auto U=materialize(request.directions,guard),C=materialize(request.coupling,guard),R=materialize(request.row_map,guard);
        auto target_matrix=materialize(request.updated,guard);
        // This verifies every direction, including supported zeros. Agreement
        // for one response does not certify the declared updated operator.
        const auto expected=field::update_detail::add(base->state_->solve.matrix(),numeric::checked_multiply(
            numeric::checked_multiply(U,C,guard),R,guard),guard);
        require(expected==target_matrix,"native declared updated operator differs from the complete update");
        field::PreparedWoodburySolve solve(base->state_->solve,U,C,field::update_detail::transpose(R,guard),guard);
        required.rational_slots=add(required.rational_slots,add(solve.stored_entries(),target_matrix.nonzeros()));
        required.sparse_entries=required.rational_slots;runtime::admit_execution(required,lease.policy());
        guard.entries(add(solve.stored_entries(),target_matrix.nonzeros()));lease.check_cancelled();
        const bool fallback=base->state_->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        return PreparedNativeWoodburySolve(std::make_shared<const State>(std::move(request),std::move(*base),std::move(solve),
            std::move(target_matrix),required,lease.policy().name,cutoff,guard.steps(),reused,fallback));
    }
    std::shared_ptr<const State> state_;
};

class DerivedNativeWoodburyField {
public:
    DerivedNativeWoodburyField(const DerivedNativeWoodburyField&)=default;
    DerivedNativeWoodburyField& operator=(const DerivedNativeWoodburyField&)=default;
    const numeric::RationalVector& values() const & {return values_;}
    numeric::RationalVector values() const && {return values_;}
    const NativeWoodburyProvenance& provenance() const & {return report_;}
    NativeWoodburyProvenance provenance() const && {return report_;}
    storage::PinnedNativeAccession publication() const {return plan_.publication();}
    storage::PinnedNativeAccession input() const {return input_;}
    const PreparedNativeWoodburySolve& plan() const & {return plan_;}
    const PreparedNativeWoodburySolve& plan() const &&=delete;
private:
    friend class PreparedNativeWoodburySolve;
    DerivedNativeWoodburyField(PreparedNativeWoodburySolve owner,storage::PinnedNativeAccession input,
                             numeric::RationalVector values,NativeWoodburyProvenance report)
        :plan_(std::move(owner)),input_(std::move(input)),values_(std::move(values)),report_(std::move(report)){}
    PreparedNativeWoodburySolve plan_;
    storage::PinnedNativeAccession input_;
    numeric::RationalVector values_;
    NativeWoodburyProvenance report_;
};
inline DerivedNativeWoodburyField PreparedNativeWoodburySolve::execute(Pin input,runtime::ExecutionLease lease,
        std::optional<runtime::WorkAvailability> cutoff) const {
    using namespace native_update_detail;
    const auto pin=state_;lease.check_cancelled();const auto& request=pin->request;
    same(request.base,input);available(request.base,pin->cutoff);available(input,cutoff);
    require(input.kind()==storage::NativeAccessionKind::Field,"native update requires a stored field pin");
    const auto field=input.tensor_declaration();const auto carrier=request.base.carrier();
    require(field->role==storage::TemporalTensorRole::Field && field->output==carrier->key &&
            input.tensor()->domain()==carrier->domain && input.tensor()->support().count()==carrier->domain.logical_size(),
            "native update field requires the exact carrier and complete support");
    const std::array<Pin,6> pins{request.base,request.directions,request.coupling,request.row_map,request.updated,input};
    const auto payload_requirements=selected(pins,2);auto required=pin->retained;
    required.records=add(required.records,1);required.rational_slots=add(required.rational_slots,payload(input));
    required.sparse_entries=required.rational_slots;required.integer_bits=std::max(required.integer_bits,payload_requirements.integer_bits);
    runtime::admit_execution(required,lease.policy());
    const auto action_limits=limits(lease,pins,request.dimension_limit,payload_requirements.rational_slots);
    Guard guard(action_limits,[&lease]{lease.check_cancelled();});
    guard.entries(add(pin->solve.stored_entries(),pin->updated.nonzeros()));guard.matrix(pin->updated);
    const auto rhs=field_values(input,guard);auto values=pin->solve.apply(rhs,guard);
    if(numeric::checked_apply(pin->updated,values,guard)!=rhs)throw std::logic_error("native declared updated equation residual");
    NativeWoodburyProvenance report;
    report.source_namespace=request.base.source_namespace();report.sequence=request.base.sequence();
    report.input_field=field->key;report.carrier=carrier->key;report.update_carrier=request.coupling.carrier()->key;
    report.base=request.base.tensor_declaration()->key;report.directions=request.directions.tensor_declaration()->key;
    report.coupling=request.coupling.tensor_declaration()->key;report.row_map=request.row_map.tensor_declaration()->key;
    report.updated=request.updated.tensor_declaration()->key;report.selected_requirements=required;report.action_limits=action_limits;
    report.preparation_steps=pin->preparation_steps;report.execution_steps=guard.steps();report.base_preparation_policy=pin->base.state_->policy;
    report.preparation_policy=pin->policy;report.execution_policy=lease.policy().name;report.reused_base=pin->reused_base;
    report.base_preparation_availability=pin->base.state_->cutoff;
    report.preparation_availability=pin->cutoff;report.execution_availability=cutoff?cutoff:pin->cutoff;
    report.fallback=pin->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
    lease.check_cancelled();return DerivedNativeWoodburyField(PreparedNativeWoodburySolve(pin),std::move(input),std::move(values),std::move(report));
}
} // namespace rfc::inference
