#pragma once
#include "rfc/algebra/owned_product_map_composition.hpp"
#include "rfc/inference/native_product_chain_map.hpp"

namespace rfc::inference {
struct NativeProductCompositionAvailability {
    NativeProductHexagonAvailability source, intermediate, target;
};
enum class NativeProductCompositionAction { Lift, CoherenceWitness };
struct NativeProductCompositionProvenance {
    NativeProductCompositionAction action{NativeProductCompositionAction::Lift};
    std::uint64_t declaration{}, revision{};
    std::array<std::uint64_t, 2> map_declarations, map_revisions;
    // Original source/midpoint/target A/B/C roles; permutations do not reorder them.
    std::array<std::array<std::uint64_t, 3>, 3> factors, namespaces, sequences;
    std::array<NativeProductHexagonAvailability, 3> product_preparation;
    std::array<NativeProductMapAvailability, 2> map_preparation;
    NativeProductCompositionAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
    algebra::ProductHexagonCarrier carrier;
};
namespace native_product_composition_detail {
using namespace native_product_detail;
struct State {
    PreparedNativeProductChainMap first, second;
    algebra::PreparedProductMapComposition numeric;
    NativeProductCompositionAvailability preparation;
    std::size_t dimension{}, steps{};
    std::string policy;
    bool fallback{};
};
inline std::array<Pin, 9> pins(const PreparedNativeProductChainMap& first, const PreparedNativeProductChainMap& second) {
    return {first.source().factor(0), first.source().factor(1), first.source().factor(2),
        first.target().factor(0), first.target().factor(1), first.target().factor(2),
        second.target().factor(0), second.target().factor(1), second.target().factor(2)};
}
inline NativeProductMapAvailability first_availability(NativeProductCompositionAvailability a) { return {a.source, a.intermediate}; }
inline NativeProductMapAvailability second_availability(NativeProductCompositionAvailability a) { return {a.intermediate, a.target}; }
inline void available(const State& s, NativeProductCompositionAvailability a) {
    native_product_map_detail::available(s.first.source(), s.first.target(), first_availability(a));
    native_product_map_detail::available(s.second.source(), s.second.target(), second_availability(a));
}
inline runtime::ExecutionRequirements requirements(const State& s) {
    return native_product_detail::requirements(s.numeric.retained_words(), mul(s.numeric.entry_envelope(), 2));
}
inline NativeProductCompositionProvenance report(const State& s, NativeProductCompositionAction action,
    NativeProductCompositionAvailability a, const runtime::ExecutionLease& lease, Guard& guard, algebra::ProductHexagonCarrier carrier) {
    const std::array h{s.first.source(), s.first.target(), s.second.target()};
    std::array<std::array<std::uint64_t, 3>, 3> roles{}, namespaces{}, sequences{};
    std::array<NativeProductHexagonAvailability, 3> preparation{};
    for (std::size_t j = 0; j < h.size(); ++j) {
        roles[j] = h[j].declaration().factor_identities; preparation[j] = h[j].preparation_availability();
        for (std::size_t f = 0; f < 3; ++f) { namespaces[j][f] = h[j].factor(f).source_namespace(); sequences[j][f] = h[j].factor(f).sequence(); }
    }
    const auto direct = s.numeric.direct(), first = s.first.numeric_plan(), second = s.second.numeric_plan();
    return {action, direct.declaration().key, direct.declaration().revision,
        {first.declaration().key, second.declaration().key},
        {first.declaration().revision, second.declaration().revision},
        roles, namespaces, sequences, preparation, {}, s.preparation, a, requirements(s), guard.limits(),
        s.steps, guard.steps(), s.policy, lease.policy().name, s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu, carrier};
}
} // namespace native_product_composition_detail
class NativeProductCompositionLift;
class NativeProductCompositionWitness;
class PreparedNativeProductMapComposition {
public:
    static PreparedNativeProductMapComposition prepare(PreparedNativeProductChainMap first,
        PreparedNativeProductChainMap second, std::uint64_t key, std::uint64_t revision,
        NativeProductCompositionAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_product_composition_detail;
        lease.check_cancelled();
        detail::require(first.target().same_owner(second.source()), "native composition needs its actual intermediate product owner");
        const auto [words, envelope] = algebra::product_composition_detail::bounds(first.numeric_plan(), second.numeric_plan());
        runtime::admit_execution(native_product_detail::requirements(words, detail::mul(envelope, 2)), lease.policy());
        native_product_map_detail::admit(*first.state_, detail::first_availability(a), lease);
        native_product_map_detail::admit(*second.state_, detail::second_availability(a), lease);
        const auto source = first.source(), middle = first.target(), target = second.target();
        const auto dimension = std::min({source.declaration().dimension_limit,
            middle.declaration().dimension_limit, target.declaration().dimension_limit});
        detail::Guard guard(native_product_detail::limits(lease, detail::pins(first, second), dimension), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::PreparedProductMapComposition::prepare(first.numeric_plan(), second.numeric_plan(), key, revision, guard);
        lease.check_cancelled();
        return PreparedNativeProductMapComposition(std::make_shared<const detail::State>(detail::State{
            std::move(first), std::move(second), std::move(numeric), a, dimension, guard.steps(), lease.policy().name,
            lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    static PreparedNativeProductChainMap identity(PreparedNativeProductHexagon h, std::uint64_t key,
        std::uint64_t revision, NativeProductHexagonAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_product_composition_detail; lease.check_cancelled();
        native_product_map_detail::available(h, h, {a, a});
        native_product_map_detail::available(h, h, {h.preparation_availability(), h.preparation_availability()});
        const auto [words, envelope] = algebra::product_composition_detail::identity_bounds(h.numeric_plan());
        runtime::admit_execution(native_product_detail::requirements(words, detail::mul(envelope, 2)), lease.policy());
        detail::Guard guard(native_product_detail::limits(lease, native_product_map_detail::pins(h, h), h.declaration().dimension_limit), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::PreparedProductMapComposition::identity(h.numeric_plan(), key, revision, guard); lease.check_cancelled();
        return PreparedNativeProductChainMap(std::make_shared<const native_product_map_detail::State>(native_product_map_detail::State{
            h, h, std::move(numeric), {a, a}, h.declaration().dimension_limit, guard.steps(), lease.policy().name,
            lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeProductChainMap first() const { return state_->first; }
    PreparedNativeProductChainMap second() const { return state_->second; }
    algebra::PreparedProductMapComposition numeric_plan() const { return state_->numeric; }
    bool same_owner(const PreparedNativeProductMapComposition& p) const noexcept { return state_ == p.state_; }
    NativeProductCompositionLift apply(DerivedNativeHexagonField, NativeProductCompositionAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductCompositionWitness witness(DerivedNativeHexagonField, NativeProductCompositionAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class NativeProductCompositionLift;
    friend class NativeProductCompositionWitness;
    friend class PreparedNativeProductMapAssociativity;
    explicit PreparedNativeProductMapComposition(std::shared_ptr<const native_product_composition_detail::State> s) : state_(std::move(s)) {}
    static void admit(const native_product_composition_detail::State& s, NativeProductCompositionAvailability a,
        const runtime::ExecutionLease& lease) {
        namespace detail = native_product_composition_detail; lease.check_cancelled();
        detail::available(s, s.preparation); detail::available(s, a);
        native_product_map_detail::admit(*s.first.state_, detail::first_availability(a), lease);
        native_product_map_detail::admit(*s.second.state_, detail::second_availability(a), lease);
        runtime::admit_execution(detail::requirements(s), lease.policy());
    }
    static std::array<std::shared_ptr<const NativeProductMapLift>, 2> stages(
        const native_product_composition_detail::State& s, DerivedNativeHexagonField input,
        const algebra::ProductMapCompositionLift& lift, NativeProductCompositionAvailability a,
        const runtime::ExecutionLease& lease, numeric::ExactActionGuard& guard) {
        namespace detail = native_product_composition_detail;
        const auto carrier = s.numeric.first().source().carrier(lift.first().input());
        auto first_report = native_product_map_detail::report(*s.first.state_, NativeProductMapAction::Lift,
            detail::first_availability(a), lease, guard, carrier);
        auto second_report = native_product_map_detail::report(*s.second.state_, NativeProductMapAction::Lift,
            detail::second_availability(a), lease, guard, carrier);
        first_report.execution_steps = lift.staged_work()[0].steps(); second_report.execution_steps = lift.staged_work()[1].steps();
        auto first = std::make_shared<const NativeProductMapLift>(NativeProductMapLift(s.first, std::move(input), lift.first(), std::move(first_report)));
        auto second = std::make_shared<const NativeProductMapLift>(NativeProductMapLift(s.second, first, lift.second(), std::move(second_report)));
        return {std::move(first), std::move(second)};
    }
    static NativeProductCompositionProvenance report(const native_product_composition_detail::State& s,
        NativeProductCompositionAction action, NativeProductCompositionAvailability a,
        const runtime::ExecutionLease& lease, numeric::ExactActionGuard& guard, algebra::ProductHexagonCarrier carrier) {
        auto out = native_product_composition_detail::report(s, action, a, lease, guard, carrier);
        out.map_preparation = {s.first.state_->preparation, s.second.state_->preparation};
        out.fallback = out.fallback || s.first.state_->fallback || s.second.state_->fallback;
        return out;
    }
    std::shared_ptr<const native_product_composition_detail::State> state_;
};
class NativeProductCompositionLift {
public:
    PreparedNativeProductMapComposition plan() const { return plan_; }
    const algebra::ProductMapCompositionLift& lift() const & { return lift_; }
    const algebra::ProductMapCompositionLift& lift() const && = delete;
    std::shared_ptr<const DerivedNativeHexagonField> input() const { return stages_[0]->input(); }
    const std::array<std::shared_ptr<const NativeProductMapLift>, 2>& stages() const & { return stages_; }
    const std::array<std::shared_ptr<const NativeProductMapLift>, 2>& stages() const && = delete;
    const NativeProductCompositionProvenance& provenance() const & { return report_; }
    const NativeProductCompositionProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductMapComposition;
    friend class PreparedNativeProductMapAssociativity;
    NativeProductCompositionLift(PreparedNativeProductMapComposition p, algebra::ProductMapCompositionLift lift,
        std::array<std::shared_ptr<const NativeProductMapLift>, 2> stages, NativeProductCompositionProvenance report)
        : plan_(std::move(p)), lift_(std::move(lift)), stages_(std::move(stages)), report_(std::move(report)) {}
    PreparedNativeProductMapComposition plan_;
    algebra::ProductMapCompositionLift lift_;
    std::array<std::shared_ptr<const NativeProductMapLift>, 2> stages_;
    NativeProductCompositionProvenance report_;
};
class NativeProductCompositionWitness {
public:
    PreparedNativeProductMapComposition plan() const { return plan_; }
    const algebra::ProductMapCompositionWitness& witness() const & { return witness_; }
    const algebra::ProductMapCompositionWitness& witness() const && = delete;
    std::shared_ptr<const DerivedNativeHexagonField> input() const { return stages_[0]->input(); }
    const std::array<std::shared_ptr<const NativeProductMapLift>, 2>& stages() const & { return stages_; }
    const std::array<std::shared_ptr<const NativeProductMapLift>, 2>& stages() const && = delete;
    const NativeProductCompositionProvenance& provenance() const & { return report_; }
    const NativeProductCompositionProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductMapComposition;
    NativeProductCompositionWitness(PreparedNativeProductMapComposition p, algebra::ProductMapCompositionWitness witness,
        std::array<std::shared_ptr<const NativeProductMapLift>, 2> stages, NativeProductCompositionProvenance report)
        : plan_(std::move(p)), witness_(std::move(witness)), stages_(std::move(stages)), report_(std::move(report)) {}
    PreparedNativeProductMapComposition plan_;
    algebra::ProductMapCompositionWitness witness_;
    std::array<std::shared_ptr<const NativeProductMapLift>, 2> stages_;
    NativeProductCompositionProvenance report_;
};
inline NativeProductCompositionLift PreparedNativeProductMapComposition::apply(DerivedNativeHexagonField input,
    NativeProductCompositionAvailability a, runtime::ExecutionLease lease) const {
    namespace detail = native_product_composition_detail; const auto pin = state_; admit(*pin, a, lease);
    detail::require(pin->first.source().same_owner(input.plan()), "native composition needs its actual source field owner");
    detail::Guard guard(native_product_detail::limits(lease, detail::pins(pin->first, pin->second), pin->dimension), [&lease]{ lease.check_cancelled(); });
    const auto carrier = pin->numeric.first().source().carrier(input.field());
    auto lift = pin->numeric.apply(input.field(), guard); lease.check_cancelled();
    auto receipts = stages(*pin, std::move(input), lift, a, lease, guard);
    return NativeProductCompositionLift(PreparedNativeProductMapComposition(pin), std::move(lift), std::move(receipts),
        report(*pin, NativeProductCompositionAction::Lift, a, lease, guard, carrier));
}
inline NativeProductCompositionWitness PreparedNativeProductMapComposition::witness(DerivedNativeHexagonField input,
    NativeProductCompositionAvailability a, runtime::ExecutionLease lease) const {
    namespace detail = native_product_composition_detail; const auto pin = state_; admit(*pin, a, lease);
    detail::require(pin->first.source().same_owner(input.plan()), "native composition witness needs its actual source field owner");
    detail::Guard guard(native_product_detail::limits(lease, detail::pins(pin->first, pin->second), pin->dimension), [&lease]{ lease.check_cancelled(); });
    const auto carrier = pin->numeric.first().source().carrier(input.field());
    auto witness = pin->numeric.witness(input.field(), guard); lease.check_cancelled();
    // Only the actual starting field becomes a native staged receipt. Numerical
    // coherence prefixes stay derived observations with their own retained maps.
    auto receipts = stages(*pin, std::move(input), witness.routes()[0].prefixes[0], a, lease, guard);
    return NativeProductCompositionWitness(PreparedNativeProductMapComposition(pin), std::move(witness), std::move(receipts),
        report(*pin, NativeProductCompositionAction::CoherenceWitness, a, lease, guard, carrier));
}
} // namespace rfc::inference
