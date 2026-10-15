#pragma once
#include "rfc/inference/native_hodge_frames.hpp"
#include "rfc/inference/native_factored_action.hpp"
#include "rfc/field/prepared_hodge_invariants.hpp"

namespace rfc::inference {
struct NativeHodgeInvariantValues {
    numeric::Rational pseudodeterminant,harmonic_volume;
    std::uint64_t source_namespace{},sequence{},carrier{};
    core::Grade grade;
    std::shared_ptr<const void> plan_owner;
};
class PreparedNativeHodgeInvariants {
    struct State {
        PreparedNativeHodgeFrames frames;
        PreparedNativeOperatorFamily operators;
        field::PreparedHodgeInvariants invariants;
        std::vector<storage::PinnedNativeAccession> publications;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeHodgeInvariants(State state):state_(std::make_shared<const State>(std::move(state))){}
public:
    static PreparedNativeHodgeInvariants prepare(PreparedNativeHodgeFrames frames,PreparedNativeOperatorFamily operators,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        native_update_detail::same(frames.publication(),operators.tower().publication());
        auto publications=operators.publications();native_action_detail::append_pin(publications,frames.publication());
        for(const auto& metric:frames.request().metric_tensors)native_action_detail::append_pin(publications,metric);
        for(const auto& publication:publications)native_action_detail::available(publication,availability);
        runtime::admit_execution(frames.retained_requirements(),lease.policy());
        const auto dimension=std::min(frames.request().dimension_limit,operators.dimension_limit());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,publications,dimension),[&lease]{lease.check_cancelled();});
        field::PreparedHodgeInvariants invariants(frames.numeric_plan(),operators.numeric_family(),guard);
        return PreparedNativeHodgeInvariants({std::move(frames),std::move(operators),std::move(invariants),std::move(publications),dimension});
    }
    const field::PreparedHodgeInvariants& numeric_plan() const & {return state_->invariants;}
    const field::PreparedHodgeInvariants& numeric_plan() const &&=delete;
    const std::vector<storage::PinnedNativeAccession>& publications() const & {return state_->publications;}
    const std::vector<storage::PinnedNativeAccession>& publications() const &&=delete;
    std::size_t dimension_limit() const noexcept {return state_->dimension_limit;}
    runtime::ExecutionRequirements retained_requirements() const {return state_->frames.retained_requirements();}
    NativeHodgeInvariantValues execute(const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;const auto publication=pin->frames.publication();
        for(const auto& source:pin->publications)native_action_detail::available(source,availability);
        runtime::admit_execution(pin->frames.retained_requirements(),lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pin->publications,pin->dimension_limit),[&lease]{lease.check_cancelled();});
        const auto grade=pin->frames.numeric_plan().grade();
        return {pin->invariants.pseudodeterminant(guard),pin->invariants.harmonic_volume(guard),publication.source_namespace(),
            publication.sequence(),pin->operators.carrier(grade).key,grade,pin};
    }
};
struct NativeHarmonicDeterminantValue {
    numeric::Rational determinant;
    std::size_t harmonic_dimension{},execution_steps{};
    std::shared_ptr<const void> plan_owner;
};
// Sigma is explicitly declared in this retained plan's harmonic-frame
// coordinates. The source frames, restricted stiffness and Gram factors are
// adopted from the actual native owner and are never recomputed here.
class PreparedNativeHarmonicDeterminant {
    struct State {
        PreparedNativeHodgeInvariants owner;
        numeric::ExactMatrix coefficients;
        field::PreparedHarmonicDeterminant completion;
        std::size_t preparation_steps;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeHarmonicDeterminant(State state):state_(std::make_shared<const State>(std::move(state))){}
public:
    static PreparedNativeHarmonicDeterminant prepare(PreparedNativeHodgeInvariants owner,numeric::ExactMatrix coefficients,
        const NativeActionAvailability& availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        for(const auto& pin:owner.publications())native_action_detail::available(pin,availability);
        runtime::admit_execution(owner.retained_requirements(),lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,owner.publications(),owner.dimension_limit()),[&lease]{lease.check_cancelled();});
        field::PreparedHarmonicDeterminant completion(owner.numeric_plan(),coefficients,guard);
        return PreparedNativeHarmonicDeterminant({std::move(owner),std::move(coefficients),std::move(completion),guard.steps()});
    }
    const numeric::ExactMatrix& coefficient_matrix() const & {return state_->coefficients;}
    const numeric::ExactMatrix& coefficient_matrix() const &&=delete;
    const numeric::ExactMatrix& harmonic_frame() const & {return state_->owner.numeric_plan().frames().harmonic_frame();}
    const numeric::ExactMatrix& harmonic_frame() const &&=delete;
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    NativeHarmonicDeterminantValue execute(const NativeActionAvailability& availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto pin=state_;
        for(const auto& source:pin->owner.publications())native_action_detail::available(source,availability);
        runtime::admit_execution(pin->owner.retained_requirements(),lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease,pin->owner.publications(),pin->owner.dimension_limit()),[&lease]{lease.check_cancelled();});
        guard.matrix(pin->coefficients);const auto value=pin->completion.determinant(guard);
        return {value,pin->coefficients.rows(),guard.steps(),pin};
    }
};
} // namespace rfc::inference
