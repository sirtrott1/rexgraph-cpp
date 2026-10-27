#pragma once
#include "rfc/inference/native_field_updates.hpp"
#include "rfc/field/prepared_operator_family.hpp"
#include <algorithm>

namespace rfc::inference {
struct NativeCarrier {
    storage::PinnedNativeAccession publication;
    std::uint64_t key{};
    std::shared_ptr<const storage::TemporalCarrier> declaration;
    static NativeCarrier bind(storage::PinnedNativeAccession pin,std::uint64_t key) {
        const auto endpoint=pin.endpoint();
        for(const auto& carrier:endpoint->carriers)if(carrier.key==key){
            if(carrier.domain.extents().size()!=1)throw std::invalid_argument("factored native action requires a linear declared carrier");
            return {std::move(pin),key,{endpoint,&carrier}};
        }
        throw std::invalid_argument("native factored carrier absent");
    }
    std::size_t size() const {if(!declaration)throw std::invalid_argument("native carrier declaration missing");return declaration->domain.logical_size();}
    bool same(const NativeCarrier& other) const {
        return key==other.key && declaration.get()==other.declaration.get() &&
            publication.endpoint().get()==other.publication.endpoint().get() &&
            publication.source_namespace()==other.publication.source_namespace() &&
            publication.same_execution_authority(other.publication);
    }
};
using NativeActionAvailability=std::vector<runtime::WorkAvailability>;
namespace native_action_detail {
using Pin=storage::PinnedNativeAccession;
inline void available(const Pin& pin,const NativeActionAvailability& cutoffs) {
    std::optional<std::uint64_t> selected;
    for(const auto& cutoff:cutoffs)if(cutoff.source_namespace==pin.source_namespace()){
        if(selected && *selected!=cutoff.max_sequence)throw std::invalid_argument("conflicting native availability cutoffs for one namespace");
        selected=cutoff.max_sequence;runtime::admit_work_availability(pin.source_namespace(),pin.sequence(),cutoff);
    }
    if(selected)return;
    throw std::invalid_argument("native factored action requires every publication's availability");
}
inline void append_pin(std::vector<Pin>& out,const Pin& pin) {
    for(const auto& old:out)if(old.endpoint().get()==pin.endpoint().get() && old.source_namespace()==pin.source_namespace() &&
        old.same_execution_authority(pin) && old.kind()==pin.kind()){
        const auto kind=pin.kind();
        if(kind!=storage::NativeAccessionKind::Field && kind!=storage::NativeAccessionKind::Tensor && kind!=storage::NativeAccessionKind::Operator)return;
        if(old.tensor_declaration()->key==pin.tensor_declaration()->key)return;
    }
    out.push_back(pin);
}
inline void append_owner(std::vector<std::shared_ptr<const void>>& out,std::shared_ptr<const void> owner) {
    for(const auto& old:out)if(old.get()==owner.get())return;
    out.push_back(std::move(owner));
}
inline numeric::ExactActionLimits limits(const runtime::ExecutionLease& lease,const std::vector<Pin>& pins,std::size_t dimension) {
    const auto& p=lease.policy().limits;std::size_t work=p.scalar_work;
    for(const auto& pin:pins)work=std::min(work,pin.limits().max_action_work);
    return {dimension,std::min(p.rational_slots,p.sparse_entries),work,p.integer_bits};
}
}

class NativeFactoredAction;
// An ephemeral derived field never becomes a stored source assertion. Every
// result retains its actual stored inputs and immutable action owners.
class NativeFieldValue {
    friend class PreparedNativeFiniteSheafSections;
    struct State {
        NativeCarrier carrier;
        numeric::SparseRationalVector values;
        std::vector<storage::PinnedNativeAccession> roots;
        std::vector<storage::PinnedNativeAccession> dependencies;
        std::vector<std::shared_ptr<const void>> owners;
        std::vector<std::string> profiles;
    };
    std::shared_ptr<const State> state_;
    explicit NativeFieldValue(State state):state_(std::make_shared<const State>(std::move(state))){}
    friend class NativeFactoredAction;
    friend class PreparedNativeTowerOperator;
    friend class PreparedNativeKrylov;
    friend class PreparedNativeCayleyOrbit;
    friend class PreparedNativeLocalGluing;
    friend class PreparedNativeInjectiveGluing;
    friend class PreparedNativeFiniteCoverGluing;
    friend class PreparedNativeColimitGluing;
    friend class PreparedNativeSheafGluing;
public:
    static NativeFieldValue stored(storage::PinnedNativeAccession pin) {
        if(pin.kind()!=storage::NativeAccessionKind::Field)throw std::invalid_argument("native field value requires stored Field pin");
        const auto d=pin.tensor_declaration();
        if(d->role!=storage::TemporalTensorRole::Field || !d->output ||
           d->tensor.support().count()!=d->tensor.domain().logical_size())
            throw std::invalid_argument("native field requires complete declared field support");
        auto carrier=NativeCarrier::bind(pin,*d->output);
        if(carrier.declaration.get()!=pin.carrier().get() || d->tensor.domain()!=carrier.declaration->domain)
            throw std::invalid_argument("stored field carrier mismatch");
        return NativeFieldValue({std::move(carrier),d->tensor.values(),{pin},{pin},{},{"stored-native-sparse-field-v1"}});
    }
    const NativeCarrier& carrier() const & {return state_->carrier;}
    const NativeCarrier& carrier() const &&=delete;
    const numeric::SparseRationalVector& sparse_values() const & {return state_->values;}
    numeric::SparseRationalVector sparse_values() const && {return state_->values;}
    // A full coordinate vector is produced only on this explicit request.
    numeric::RationalVector dense_values(numeric::ExactActionLimits limits={}) const {
        numeric::ExactActionGuard guard(limits);guard.sparse_vector(state_->values);guard.entries(state_->values.size());return state_->values.dense();
    }
    numeric::RationalVector values(numeric::ExactActionLimits limits={}) const {return dense_values(limits);}
    const std::vector<storage::PinnedNativeAccession>& inputs() const & {return state_->roots;}
    const std::vector<storage::PinnedNativeAccession>& publications() const & {return state_->dependencies;}
    const std::vector<std::string>& profiles() const & {return state_->profiles;}
    std::size_t retained_action_owners() const noexcept {return state_->owners.size();}
    NativeFieldValue scaled(numeric::Rational q,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;auto out=*pin;q=numeric::canonical_rational(q);
        out.values=field::factored_action_detail::scale(pin->values,q,guard);
        out.profiles.push_back("native-sparse-field-scale-v1");return NativeFieldValue(std::move(out));
    }
    NativeFieldValue plus(const NativeFieldValue& rhs,numeric::ExactActionGuard& guard) const {
        const auto pin=state_,other=rhs.state_;
        if(!pin->carrier.same(other->carrier))throw std::invalid_argument("native field sum requires identical actual carriers");
        auto out=*pin;
        out.values=field::factored_action_detail::add(pin->values,other->values,guard);
        for(const auto& root:other->roots)native_action_detail::append_pin(out.roots,root);
        for(const auto& dependency:other->dependencies)native_action_detail::append_pin(out.dependencies,dependency);
        for(const auto& owner:other->owners)native_action_detail::append_owner(out.owners,owner);
        out.profiles.insert(out.profiles.end(),other->profiles.begin(),other->profiles.end());
        out.profiles.push_back("native-field-sum-v1");return NativeFieldValue(std::move(out));
    }
};

class PreparedNativeOperatorFamily;
class PreparedNativeSourceGreenResponse;
class PreparedNativeTowerOperator;
class PreparedNativeMoments;
class PreparedNativeWordDefect;
class NativeFactoredAction {
    struct State {
        NativeCarrier input,output;
        field::FactoredAction action;
        std::vector<storage::PinnedNativeAccession> pins;
        std::vector<std::shared_ptr<const void>> owners;
        std::string profile;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    explicit NativeFactoredAction(State state) {
        if(!state.input.same(NativeCarrier::bind(state.input.publication,state.input.key)) ||
           !state.output.same(NativeCarrier::bind(state.output.publication,state.output.key)))
            throw std::invalid_argument("native carrier must be its actual publication declaration");
        if(state.action.cols()!=state.input.size() || state.action.rows()!=state.output.size() || !state.dimension_limit)
            throw std::invalid_argument("native factored action carrier dimensions or limit");
        state_=std::make_shared<const State>(std::move(state));
    }
    friend class PreparedNativeOperatorFamily;
    friend class PreparedNativeSourceGreenResponse;
    friend class PreparedNativeTowerOperator;
    friend class PreparedNativeKrylov;
    friend class PreparedNativeCayleyOrbit;
    friend class PreparedNativeLocalGluing;
    friend class PreparedNativeInjectiveGluing;
    friend class PreparedNativeFiniteCoverGluing;
    friend class PreparedNativeColimitGluing;
    friend class PreparedNativeSheafGluing;
    friend class PreparedNativeMoments;
    friend class PreparedNativeActionMoments;
    friend class PreparedNativeWordDefect;
    friend class PreparedNativeMultitimeObservations;
    friend class PreparedNativeMomentChange;
    static void same_space(const NativeCarrier& a,const NativeCarrier& b) {
        if(!a.same(b))throw std::invalid_argument("native action composition requires actual carrier identity, not equal dimension");
    }
    NativeFieldValue execute_with_guard(NativeFieldValue input,const NativeActionAvailability& availability,
                            const runtime::ExecutionLease& lease,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;same_space(pin->input,input.carrier());lease.check_cancelled();
        auto all=pin->pins;for(const auto& root:input.publications())native_action_detail::append_pin(all,root);
        for(const auto& p:all)native_action_detail::available(p,availability);
        const auto admitted=native_action_detail::limits(lease,all,pin->dimension_limit);
        if(guard.limits().dimension>admitted.dimension || guard.limits().steps>admitted.steps || guard.limits().entries>admitted.entries ||
           guard.limits().integer_bits>admitted.integer_bits)
            throw std::invalid_argument("shared native guard exceeds current operand or host limits");
        const auto slots=machine::checked_size_add(pin->action.retained_entries(),input.sparse_values().nonzeros());
        runtime::admit_execution({all.size()+pin->owners.size()+2,slots,slots,0,0},lease.policy());
        if(pin->input.size()>pin->dimension_limit || pin->output.size()>pin->dimension_limit)
            throw std::length_error("native action carrier dimension limit");
        auto values=pin->action.apply(input.sparse_values(),guard);
        const auto actual_slots=machine::checked_size_add(slots,values.nonzeros());
        runtime::admit_execution({all.size()+pin->owners.size()+2,actual_slots,actual_slots,0,0},lease.policy());
        auto out=*input.state_;out.carrier=pin->output;out.values=std::move(values);
        for(const auto& dependency:pin->pins)native_action_detail::append_pin(out.dependencies,dependency);
        native_action_detail::append_owner(out.owners,pin);out.profiles.push_back(pin->profile);
        lease.check_cancelled();return NativeFieldValue(std::move(out));
    }
    numeric::RationalVector selected_with_guard(NativeFieldValue input,std::span<const std::size_t> coordinates,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;same_space(pin->input,input.carrier());lease.check_cancelled();
        auto all=pin->pins;for(const auto& root:input.publications())native_action_detail::append_pin(all,root);
        for(const auto& p:all)native_action_detail::available(p,availability);
        const auto admitted=native_action_detail::limits(lease,all,pin->dimension_limit);
        if(guard.limits().dimension>admitted.dimension || guard.limits().steps>admitted.steps || guard.limits().entries>admitted.entries || guard.limits().integer_bits>admitted.integer_bits)
            throw std::invalid_argument("shared selected guard exceeds current native limits");
        const auto slots=machine::checked_size_add(pin->action.retained_entries(),machine::checked_size_add(input.sparse_values().nonzeros(),coordinates.size()));
        runtime::admit_execution({all.size()+2,slots,slots,0,0},lease.policy());
        return pin->action.selected(input.sparse_values(),coordinates,guard);
    }
public:
    static NativeFactoredAction tensor(storage::PinnedNativeAccession pin,std::size_t dimension_limit=4096) {
        native_update_detail::complete_map(pin);const auto d=pin.tensor_declaration();
        if(!d->input || !d->output || d->role==storage::TemporalTensorRole::Metric || d->role==storage::TemporalTensorRole::CrossForm)
            throw std::invalid_argument("native tensor action requires a declared linear map, not a bilinear form");
        auto input=NativeCarrier::bind(pin,*d->input),output=NativeCarrier::bind(pin,*d->output);
        auto map=storage::temporal_tensor_matrix(*d);
        if(map.rows()!=output.size() || map.cols()!=input.size())throw std::invalid_argument("native map shape differs from actual carriers");
        return NativeFactoredAction({std::move(input),std::move(output),field::FactoredAction::matrix(std::move(map)),{pin},{},"native-declared-factored-map-v1",dimension_limit});
    }
    // A correspondence is explicitly declared between actual publications; it
    // does not change either source or infer transport from equal coordinates.
    static NativeFactoredAction correspondence(NativeCarrier input,NativeCarrier output,numeric::ExactMatrix map,
                                                std::size_t dimension_limit=4096) {
        if(map.rows()!=output.size() || map.cols()!=input.size())throw std::invalid_argument("native correspondence dimensions");
        std::vector<storage::PinnedNativeAccession> pins{input.publication,output.publication};
        return NativeFactoredAction({std::move(input),std::move(output),field::FactoredAction::matrix(std::move(map)),std::move(pins),{},"native-declared-correspondence-v1",dimension_limit});
    }
    static NativeFactoredAction identity(NativeCarrier carrier,std::size_t dimension_limit=4096) {
        const auto n=carrier.size();const auto pin=carrier.publication;
        return NativeFactoredAction({carrier,carrier,field::FactoredAction::identity(n),{pin},{},"native-carrier-identity-v1",dimension_limit});
    }
    const NativeCarrier& input() const & {return state_->input;}
    const NativeCarrier& output() const & {return state_->output;}
    const field::FactoredAction& numeric_action() const & {return state_->action;}
    const field::FactoredAction& numeric_action() const &&=delete;
    const std::string& profile() const & {return state_->profile;}
    std::size_t retained_entries() const noexcept {return state_->action.retained_entries();}
    std::size_t dimension_limit() const noexcept {return state_->dimension_limit;}
    const std::vector<storage::PinnedNativeAccession>& publications() const & {return state_->pins;}
    NativeFactoredAction algebraic_transpose() const {
        auto out=*state_;std::swap(out.input,out.output);out.action=out.action.transpose();
        native_action_detail::append_owner(out.owners,state_);out.profile="native-algebraic-transpose-v1";
        return NativeFactoredAction(std::move(out));
    }
    NativeFactoredAction then(const NativeFactoredAction& next) const {
        same_space(output(),next.input());auto out=*state_;const auto other=next.state_;
        out.output=other->output;out.action=out.action.then(other->action);
        for(const auto& pin:other->pins)native_action_detail::append_pin(out.pins,pin);
        native_action_detail::append_owner(out.owners,state_);native_action_detail::append_owner(out.owners,other);
        out.dimension_limit=std::min(out.dimension_limit,other->dimension_limit);out.profile="native-factored-word-v1";
        return NativeFactoredAction(std::move(out));
    }
    NativeFactoredAction plus(const NativeFactoredAction& other) const {
        same_space(input(),other.input());same_space(output(),other.output());auto out=*state_;
        out.action=out.action.plus(other.state_->action);
        for(const auto& pin:other.state_->pins)native_action_detail::append_pin(out.pins,pin);
        native_action_detail::append_owner(out.owners,state_);native_action_detail::append_owner(out.owners,other.state_);
        out.dimension_limit=std::min(out.dimension_limit,other.state_->dimension_limit);out.profile="native-factored-sum-v1";
        return NativeFactoredAction(std::move(out));
    }
    NativeFactoredAction scaled(numeric::Rational q) const {
        auto out=*state_;out.action=out.action.scaled(std::move(q));native_action_detail::append_owner(out.owners,state_);
        return NativeFactoredAction(std::move(out));
    }
    NativeFieldValue execute(NativeFieldValue input,const NativeActionAvailability& availability,
                            const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;same_space(pin->input,input.carrier());lease.check_cancelled();
        auto all=pin->pins;for(const auto& root:input.publications())native_action_detail::append_pin(all,root);
        for(const auto& p:all)native_action_detail::available(p,availability);
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,all,pin->dimension_limit),[&lease]{lease.check_cancelled();});
        return execute_with_guard(std::move(input),availability,lease,guard);
    }
    NativeFieldValue execute(storage::PinnedNativeAccession input,const NativeActionAvailability& availability,
                            const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        return execute(NativeFieldValue::stored(std::move(input)),availability,lease);
    }
    numeric::RationalVector selected(NativeFieldValue input,std::span<const std::size_t> coordinates,
                                     const NativeActionAvailability& availability,
                                     const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;same_space(pin->input,input.carrier());lease.check_cancelled();
        auto all=pin->pins;for(const auto& root:input.publications())native_action_detail::append_pin(all,root);
        for(const auto& p:all)native_action_detail::available(p,availability);
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,all,pin->dimension_limit),[&lease]{lease.check_cancelled();});
        return selected_with_guard(std::move(input),coordinates,availability,lease,guard);
    }
};

class PreparedNativeOperatorFamily {
    struct State {
        PreparedNativeExactTower native;
        field::PreparedOperatorFamily fields;
        std::vector<storage::PinnedNativeAccession> publications;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeOperatorFamily(State state):state_(std::make_shared<const State>(std::move(state))){}
    friend class PreparedNativeSourceGreenResponse;
    friend class PreparedNativeTowerOperator;
    friend class PreparedNativeKrylov;
    friend class PreparedNativeCayleyOrbit;
    friend class PreparedNativeLocalGluing;
    friend class PreparedNativeInjectiveGluing;
    friend class PreparedNativeFiniteCoverGluing;
    friend class PreparedNativeColimitGluing;
    friend class PreparedNativeSheafGluing;
    NativeFactoredAction action_with_guard(field::OperatorRequest op,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;lease.check_cancelled();
        for(const auto& publication:pin->publications)native_action_detail::available(publication,availability);
        const auto admitted=native_action_detail::limits(lease,pin->publications,pin->dimension_limit);
        if(guard.limits().dimension>admitted.dimension || guard.limits().steps>admitted.steps ||
           guard.limits().entries>admitted.entries || guard.limits().integer_bits>admitted.integer_bits)
            throw std::invalid_argument("shared operator-family guard exceeds current native limits");
        runtime::admit_execution({pin->publications.size(),0,0,0,0},lease.policy());
        auto a=pin->fields.action(op,guard);
        struct OwnedAction {std::shared_ptr<const State> family;field::FactoredAction action;};
        const auto rows=a.rows(),columns=a.cols(),entries=a.retained_entries();
        a=field::FactoredAction::owned(OwnedAction{pin,std::move(a)},rows,columns,entries,
            [](const auto& owner,const auto& input,auto& current){owner.family->fields.validate_source(current);return owner.action.apply(input,current);},
            [](const auto& owner,const auto& input,auto& current){owner.family->fields.validate_source(current);return owner.action.transpose_apply(input,current);});
        const auto in=op.kind==field::OperatorKind::Adjoint?core::Grade{static_cast<std::uint16_t>(op.grade.value-1u)}:op.grade;
        const auto out=op.kind==field::OperatorKind::Boundary || op.kind==field::OperatorKind::GradientPotential?
            core::Grade{static_cast<std::uint16_t>(op.grade.value-1u)}:op.grade;
        return NativeFactoredAction({carrier(in),carrier(out),std::move(a),pin->publications,{pin},"native-source-factor-action-v1",pin->dimension_limit});
    }
public:
    static PreparedNativeOperatorFamily prepare(PreparedNativeExactTower tower,const NativeActionAvailability& availability,
            const runtime::ExecutionLease& lease=runtime::ExecutionLease{},std::size_t dimension_limit=4096) {
        lease.check_cancelled();std::vector<storage::PinnedNativeAccession> publications{tower.publication()};
        for(const auto& metric:tower.metric_pins())native_action_detail::append_pin(publications,metric);
        for(const auto& publication:publications)native_action_detail::available(publication,availability);
        runtime::admit_execution({publications.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,publications,dimension_limit),[&lease]{lease.check_cancelled();});
        field::PreparedOperatorFamily fields(tower.tower(),guard);
        return PreparedNativeOperatorFamily({std::move(tower),std::move(fields),std::move(publications),dimension_limit});
    }
    NativeCarrier carrier(core::Grade k) const {return NativeCarrier::bind(state_->native.publication(),state_->native.grade_carrier(k));}
    const PreparedNativeExactTower& tower() const & {return state_->native;}
    const field::PreparedOperatorFamily& numeric_family() const & {return state_->fields;}
    const std::vector<storage::PinnedNativeAccession>& publications() const & {return state_->publications;}
    const std::vector<storage::PinnedNativeAccession>& publications() const &&=delete;
    std::size_t preparation_steps() const noexcept {return state_->fields.preparation_steps();}
    std::size_t dimension_limit() const noexcept {return state_->dimension_limit;}
    NativeFactoredAction action(field::OperatorRequest op,const NativeActionAvailability& availability,
                               const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;lease.check_cancelled();
        for(const auto& publication:pin->publications)native_action_detail::available(publication,availability);
        runtime::admit_execution({pin->publications.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pin->publications,pin->dimension_limit),[&lease]{lease.check_cancelled();});
        return action_with_guard(op,availability,lease,guard);
    }
};
} // namespace rfc::inference
