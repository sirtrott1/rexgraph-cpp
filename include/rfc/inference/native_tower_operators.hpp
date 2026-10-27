#pragma once
#include "rfc/inference/native_factored_action.hpp"

namespace rfc::inference {
enum class NativeTowerOperatorKind {Dirac,AntiDirac,AntiDiracCayley,DiracCayley};
struct NativeTowerField {
    std::vector<NativeFieldValue> grades;
    std::vector<std::shared_ptr<const void>> operator_owners;
    std::string profile{"native-full-tower-field-v1"};
    static NativeTowerField stored(const std::vector<storage::PinnedNativeAccession>& pins) {
        NativeTowerField out;for(const auto& pin:pins)out.grades.push_back(NativeFieldValue::stored(pin));return out;
    }
};
struct NativeComplexTowerField {NativeTowerField real,imaginary;};
class PreparedNativeTowerOperator {
    friend class PreparedNativeCayleyOrbit;
    struct State {
        PreparedNativeOperatorFamily family;
        NativeTowerOperatorKind kind;
        numeric::Rational t;
        std::vector<NativeFactoredAction> lowering,raising,green;
        std::size_t preparation_steps{};
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeTowerOperator(State state):state_(std::make_shared<const State>(std::move(state))){}
    void validate(const NativeTowerField& value) const {
        const auto count=state_->family.tower().publication().endpoint()->source.grade_count();
        if(value.grades.size()!=count)throw std::invalid_argument("native tower must contain every retained grade");
        for(std::size_t k=0;k<count;++k)if(!value.grades[k].carrier().same(state_->family.carrier(core::Grade{static_cast<std::uint16_t>(k)})))
            throw std::invalid_argument("native tower grade has a different actual carrier/publication");
    }
    std::vector<storage::PinnedNativeAccession> pins(const NativeTowerField& value) const {
        auto result=state_->family.publications();
        for(const auto& grade:value.grades)for(const auto& pin:grade.publications())native_action_detail::append_pin(result,pin);
        return result;
    }
    NativeFieldValue retain(const NativeFieldValue& value,const char* profile,numeric::ExactActionGuard& guard) const {
        guard.sparse_vector(value.sparse_values());auto out=*value.state_;
        native_action_detail::append_owner(out.owners,state_);
        for(const auto& publication:state_->family.publications())native_action_detail::append_pin(out.dependencies,publication);
        out.profiles.push_back(profile);
        return NativeFieldValue(std::move(out));
    }
    NativeTowerField dirac(const NativeTowerField& input,bool anti,const NativeActionAvailability& availability,
                          const runtime::ExecutionLease& lease,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;NativeTowerField out;out.operator_owners=input.operator_owners;
        native_action_detail::append_owner(out.operator_owners,pin);out.profile=anti?"native-metric-anti-dirac-v1":"native-metric-dirac-v1";
        for(std::size_t k=0;k<input.grades.size();++k){
            auto value=input.grades[k].scaled(0,guard);
            if(k+1<input.grades.size())value=value.plus(pin->lowering[k].execute_with_guard(input.grades[k+1],availability,lease,guard),guard);
            if(k)value=value.plus(pin->raising[k-1].execute_with_guard(input.grades[k-1],availability,lease,guard).scaled(anti?-1:1,guard),guard);
            out.grades.push_back(retain(value,anti?"native-metric-anti-dirac-v1":"native-metric-dirac-v1",guard));
        }
        return out;
    }
    NativeTowerField green(const NativeTowerField& input,const NativeActionAvailability& availability,
                          const runtime::ExecutionLease& lease,numeric::ExactActionGuard& guard) const {
        NativeTowerField out;out.operator_owners=input.operator_owners;native_action_detail::append_owner(out.operator_owners,state_);
        for(std::size_t k=0;k<input.grades.size();++k)out.grades.push_back(state_->green[k].execute_with_guard(input.grades[k],availability,lease,guard));
        return out;
    }
public:
    static PreparedNativeTowerOperator prepare(PreparedNativeOperatorFamily family,NativeTowerOperatorKind kind,
        numeric::Rational t,const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        switch(kind){case NativeTowerOperatorKind::Dirac:case NativeTowerOperatorKind::AntiDirac:
            if(t!=0)throw std::invalid_argument("Dirac action has no Cayley parameter");
            break;
        case NativeTowerOperatorKind::AntiDiracCayley:case NativeTowerOperatorKind::DiracCayley:break;
        default:throw std::invalid_argument("unknown native tower operator");}
        t=numeric::canonical_rational(t);State out{family,kind,t,{},{},{}};
        for(const auto& publication:family.publications())native_action_detail::available(publication,availability);
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,family.publications(),family.dimension_limit()),[&lease]{lease.check_cancelled();});
        guard.value(t);const auto lambda=guard.mul(t,t);
        const auto count=family.tower().publication().endpoint()->source.grade_count();
        for(std::size_t k=1;k<count;++k){
            const core::Grade grade{static_cast<std::uint16_t>(k)};
            out.lowering.push_back(family.action_with_guard({field::OperatorKind::Boundary,grade,0},availability,lease,guard));
            out.raising.push_back(family.action_with_guard({field::OperatorKind::Adjoint,grade,0},availability,lease,guard));
        }
        if(t!=0)for(std::size_t k=0;k<count;++k)
            out.green.push_back(family.action_with_guard({field::OperatorKind::Green,core::Grade{static_cast<std::uint16_t>(k)},lambda},availability,lease,guard));
        out.preparation_steps=guard.steps();
        return PreparedNativeTowerOperator(std::move(out));
    }
    const PreparedNativeOperatorFamily& family() const & {return state_->family;}
    std::size_t prepared_green_count() const noexcept {return state_->green.size();}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    NativeTowerField execute(NativeTowerField input,const NativeActionAvailability& availability,
                            const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;validate(input);lease.check_cancelled();
        const auto selected=pins(input);for(const auto& p:selected)native_action_detail::available(p,availability);
        runtime::admit_execution({selected.size()+input.grades.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,selected,pin->family.dimension_limit()),[&lease]{lease.check_cancelled();});
        pin->family.numeric_family().validate_source(guard);
        guard.value(pin->t);for(const auto& grade:input.grades)guard.sparse_vector(grade.sparse_values());
        if(pin->kind==NativeTowerOperatorKind::DiracCayley)throw std::invalid_argument("Dirac Cayley requires explicit Q(i) tower components");
        if(pin->kind==NativeTowerOperatorKind::Dirac || pin->kind==NativeTowerOperatorKind::AntiDirac)
            return dirac(input,pin->kind==NativeTowerOperatorKind::AntiDirac,availability,lease,guard);
        if(pin->t==0){native_action_detail::append_owner(input.operator_owners,pin);input.profile="native-rational-anti-dirac-cayley-v1";
            for(auto& grade:input.grades)grade=retain(grade,"native-rational-anti-dirac-cayley-v1",guard);
            return input;}
        const auto y=green(input,availability,lease,guard),d=dirac(y,true,availability,lease,guard);
        NativeTowerField out;out.operator_owners=y.operator_owners;out.profile="native-rational-anti-dirac-cayley-v1";
        for(std::size_t k=0;k<input.grades.size();++k)out.grades.push_back(retain(y.grades[k].scaled(2,guard)
            .plus(input.grades[k].scaled(-1,guard),guard).plus(d.grades[k].scaled(guard.mul(2,pin->t),guard),guard),"native-rational-anti-dirac-cayley-v1",guard));
        return out;
    }
private:
    NativeComplexTowerField execute_with_guard(NativeComplexTowerField input,const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;validate(input.real);validate(input.imaginary);lease.check_cancelled();
        if(pin->kind!=NativeTowerOperatorKind::DiracCayley)throw std::invalid_argument("complex native tower requires Dirac Cayley profile");
        auto selected=pins(input.real);for(const auto& p:pins(input.imaginary))native_action_detail::append_pin(selected,p);
        for(const auto& p:selected)native_action_detail::available(p,availability);
        runtime::admit_execution({selected.size()+input.real.grades.size()*2,0,0,0,0},lease.policy());
        const auto admitted=native_action_detail::limits(lease,selected,pin->family.dimension_limit());
        if(guard.limits().dimension>admitted.dimension || guard.limits().steps>admitted.steps ||
           guard.limits().entries>admitted.entries || guard.limits().integer_bits>admitted.integer_bits)
            throw std::invalid_argument("shared tower guard exceeds current operand or host limits");
        pin->family.numeric_family().validate_source(guard);
        guard.value(pin->t);for(const auto* part:{&input.real,&input.imaginary})for(const auto& grade:part->grades)guard.sparse_vector(grade.sparse_values());
        if(pin->t==0){native_action_detail::append_owner(input.real.operator_owners,pin);native_action_detail::append_owner(input.imaginary.operator_owners,pin);
            for(auto* part:{&input.real,&input.imaginary})for(auto& grade:part->grades)grade=retain(grade,"native-Qi-dirac-cayley-v1",guard);
            return input;}
        const auto re=green(input.real,availability,lease,guard),im=green(input.imaginary,availability,lease,guard);
        const auto dre=dirac(re,false,availability,lease,guard),dim=dirac(im,false,availability,lease,guard);
        NativeComplexTowerField out;out.real.operator_owners=re.operator_owners;out.imaginary.operator_owners=im.operator_owners;
        out.real.profile=out.imaginary.profile="native-Qi-dirac-cayley-v1";
        for(std::size_t k=0;k<input.real.grades.size();++k){
            out.real.grades.push_back(retain(re.grades[k].scaled(2,guard).plus(input.real.grades[k].scaled(-1,guard),guard)
                .plus(dim.grades[k].scaled(guard.mul(-2,pin->t),guard),guard),"native-Qi-dirac-cayley-v1",guard));
            out.imaginary.grades.push_back(retain(im.grades[k].scaled(2,guard).plus(input.imaginary.grades[k].scaled(-1,guard),guard)
                .plus(dre.grades[k].scaled(guard.mul(2,pin->t),guard),guard),"native-Qi-dirac-cayley-v1",guard));
        }
        return out;
    }
    NativeTowerField zero_tower(numeric::ExactActionGuard& guard) const {
        NativeTowerField out;const auto count=state_->family.tower().publication().endpoint()->source.grade_count();
        for(std::size_t k=0;k<count;++k){
            const auto carrier=state_->family.carrier(core::Grade{static_cast<std::uint16_t>(k)});
            guard.step();guard.dimension(carrier.size());
            out.grades.push_back(NativeFieldValue({carrier,numeric::SparseRationalVector(carrier.size()),{},state_->family.publications(),{state_},{"native-source-zero-v1"}}));
        }
        return out;
    }
public:
    NativeComplexTowerField execute(NativeComplexTowerField input,const NativeActionAvailability& availability,
                            const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        validate(input.real);validate(input.imaginary);
        auto selected=pins(input.real);for(const auto& p:pins(input.imaginary))native_action_detail::append_pin(selected,p);
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,selected,state_->family.dimension_limit()),[&lease]{lease.check_cancelled();});
        return execute_with_guard(std::move(input),availability,lease,guard);
    }
    numeric::Rational quadrance(const NativeTowerField& input,const NativeActionAvailability& availability,
                               const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        validate(input);const auto selected=pins(input);for(const auto& p:selected)native_action_detail::available(p,availability);
        runtime::admit_execution({selected.size(),0,0,0,0},lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,selected,state_->family.dimension_limit()),[&lease]{lease.check_cancelled();});
        state_->family.numeric_family().validate_source(guard);
        numeric::Rational q=0;
        for(std::size_t k=0;k<input.grades.size();++k){
            const auto& x=input.grades[k].sparse_values();const auto y=state_->family.numeric_family().metric(core::Grade{static_cast<std::uint16_t>(k)}).apply(x,guard);
            q=guard.add(q,field::factored_action_detail::dot(x,y,guard));
        }
        return q;
    }
};
} // namespace rfc::inference
