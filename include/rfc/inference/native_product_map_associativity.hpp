#pragma once
#include "rfc/algebra/owned_product_map_associativity.hpp"
#include "rfc/inference/native_product_map_composition.hpp"

namespace rfc::inference {
// X, Y, Z, W in original A/B/C order, regardless of tensor permutation.
using NativeProductAssociativityAvailability = std::array<NativeProductHexagonAvailability, 4>;
enum class NativeProductAssociativityAction { Lift, Continuation, CoherenceWitness };
struct NativeProductAssociativityProvenance {
    NativeProductAssociativityAction action{NativeProductAssociativityAction::Lift};
    algebra::ProductMapAssociativityDeclaration declaration;
    std::array<std::uint64_t, 3> map_declarations, map_revisions;
    std::array<std::array<std::uint64_t, 3>, 4> factors, namespaces, sequences;
    NativeProductAssociativityAvailability product_preparation, preparation_availability, execution_availability;
    std::array<NativeProductMapAvailability, 3> map_preparation;
    NativeProductCompositionAvailability prefix_preparation;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, prior_execution_steps{}, execution_steps{}, aggregate_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
    algebra::ProductHexagonCarrier carrier;
};
namespace native_product_associativity_detail {
using namespace native_product_detail;
struct State {
    PreparedNativeProductMapComposition prefix;
    PreparedNativeProductChainMap last;
    algebra::PreparedProductMapAssociativity numeric;
    NativeProductAssociativityAvailability preparation;
    std::size_t dimension{}, steps{};
    std::string policy;
    bool fallback{};
};
inline NativeProductCompositionAvailability prefix_availability(NativeProductAssociativityAvailability a) { return {a[0], a[1], a[2]}; }
inline std::array<PreparedNativeProductHexagon, 4> products(const PreparedNativeProductMapComposition& p,
    const PreparedNativeProductChainMap& last) { return {p.first().source(), p.first().target(), p.second().target(), last.target()}; }
inline std::array<Pin, 12> pins(const PreparedNativeProductMapComposition& p, const PreparedNativeProductChainMap& last) {
    const auto h = products(p, last);
    return {h[0].factor(0), h[0].factor(1), h[0].factor(2), h[1].factor(0), h[1].factor(1), h[1].factor(2),
        h[2].factor(0), h[2].factor(1), h[2].factor(2), h[3].factor(0), h[3].factor(1), h[3].factor(2)};
}
inline runtime::ExecutionRequirements requirements(const State& s) {
    return native_product_detail::requirements(s.numeric.retained_words(), mul(s.numeric.entry_envelope(), 2));
}
} // namespace native_product_associativity_detail
class NativeProductAssociativityLift;
class NativeProductAssociativityWitness;
class PreparedNativeProductMapAssociativity {
public:
    static PreparedNativeProductMapAssociativity prepare(PreparedNativeProductMapComposition prefix,
        PreparedNativeProductChainMap last, algebra::ProductMapAssociativityDeclaration d,
        NativeProductAssociativityAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_product_associativity_detail; lease.check_cancelled();
        detail::require(prefix.second().target().same_owner(last.source()), "native associativity needs its actual second intermediate product owner");
        const auto [words, envelope] = algebra::product_associativity_detail::bounds(prefix.numeric_plan(), last.numeric_plan());
        runtime::admit_execution(native_product_detail::requirements(words, detail::mul(envelope, 2)), lease.policy());
        PreparedNativeProductMapComposition::admit(*prefix.state_, detail::prefix_availability(a), lease);
        native_product_map_detail::admit(*last.state_, {a[2], a[3]}, lease);
        const auto h = detail::products(prefix, last);
        const auto dimension = std::min({h[0].declaration().dimension_limit, h[1].declaration().dimension_limit,
            h[2].declaration().dimension_limit, h[3].declaration().dimension_limit});
        detail::Guard guard(native_product_detail::limits(lease, detail::pins(prefix, last), dimension), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::PreparedProductMapAssociativity::prepare(prefix.numeric_plan(), last.numeric_plan(), d, guard);
        lease.check_cancelled();
        return PreparedNativeProductMapAssociativity(std::make_shared<const detail::State>(detail::State{
            std::move(prefix), std::move(last), std::move(numeric), a, dimension, guard.steps(), lease.policy().name,
            lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeProductMapComposition prefix() const { return state_->prefix; }
    PreparedNativeProductChainMap last() const { return state_->last; }
    algebra::PreparedProductMapAssociativity numeric_plan() const { return state_->numeric; }
    bool same_owner(const PreparedNativeProductMapAssociativity& p) const noexcept { return state_ == p.state_; }
    NativeProductAssociativityLift apply(DerivedNativeHexagonField, NativeProductAssociativityAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductAssociativityLift continue_from(std::shared_ptr<const NativeProductCompositionLift>,
        NativeProductAssociativityAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductAssociativityWitness witness(DerivedNativeHexagonField, NativeProductAssociativityAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class NativeProductAssociativityLift;
    friend class NativeProductAssociativityWitness;
    explicit PreparedNativeProductMapAssociativity(std::shared_ptr<const native_product_associativity_detail::State> s) : state_(std::move(s)) {}
    static void admit(const native_product_associativity_detail::State& s, NativeProductAssociativityAvailability a,
        const runtime::ExecutionLease& lease) {
        namespace detail = native_product_associativity_detail; lease.check_cancelled();
        for (const auto& v : {s.preparation, a}) {
            PreparedNativeProductMapComposition::admit(*s.prefix.state_, detail::prefix_availability(v), lease);
            native_product_map_detail::admit(*s.last.state_, {v[2], v[3]}, lease);
        }
        runtime::admit_execution(detail::requirements(s), lease.policy());
    }
    static NativeProductAssociativityProvenance report(const native_product_associativity_detail::State& s,
        NativeProductAssociativityAction action, NativeProductAssociativityAvailability a,
        const runtime::ExecutionLease& lease, numeric::ExactActionGuard& guard,
        algebra::ProductHexagonCarrier carrier, std::size_t prior_steps = 0) {
        namespace detail = native_product_associativity_detail; const auto h = detail::products(s.prefix, s.last);
        std::array<std::array<std::uint64_t, 3>, 4> roles{}, namespaces{}, sequences{};
        NativeProductAssociativityAvailability preparation{};
        for (std::size_t j = 0; j < h.size(); ++j) {
            roles[j] = h[j].declaration().factor_identities; preparation[j] = h[j].preparation_availability();
            for (std::size_t f = 0; f < 3; ++f) { namespaces[j][f] = h[j].factor(f).source_namespace(); sequences[j][f] = h[j].factor(f).sequence(); }
        }
        const auto f = s.prefix.first().numeric_plan(), g = s.prefix.second().numeric_plan(), last = s.last.numeric_plan();
        return {action, s.numeric.declaration(), {f.declaration().key, g.declaration().key, last.declaration().key},
            {f.declaration().revision, g.declaration().revision, last.declaration().revision}, roles, namespaces, sequences,
            preparation, s.preparation, a, {s.prefix.first().state_->preparation, s.prefix.second().state_->preparation, s.last.state_->preparation},
            s.prefix.state_->preparation, detail::requirements(s), guard.limits(), s.steps, prior_steps, guard.steps()-prior_steps, guard.steps(),
            s.policy, lease.policy().name, s.fallback || s.prefix.state_->fallback || s.last.state_->fallback ||
                s.prefix.first().state_->fallback || s.prefix.second().state_->fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu, carrier};
    }
    static std::shared_ptr<const NativeProductCompositionLift> prefix_receipt(
        const native_product_associativity_detail::State& s, DerivedNativeHexagonField input,
        const algebra::ProductMapAssociativityLift& lift, NativeProductAssociativityAvailability a,
        const runtime::ExecutionLease& lease, numeric::ExactActionGuard& guard) {
        const auto available = native_product_associativity_detail::prefix_availability(a);
        auto stages = PreparedNativeProductMapComposition::stages(*s.prefix.state_, std::move(input), lift.prefix(), available, lease, guard);
        auto r = PreparedNativeProductMapComposition::report(*s.prefix.state_, NativeProductCompositionAction::Lift,
            available, lease, guard, s.prefix.first().source().numeric_plan().carrier(lift.prefix().first().input()));
        r.execution_steps = lift.staged_work()[0].steps();
        return std::make_shared<const NativeProductCompositionLift>(NativeProductCompositionLift(s.prefix, lift.prefix(), std::move(stages), std::move(r)));
    }
    static std::shared_ptr<const NativeProductMapLift> last_receipt(const native_product_associativity_detail::State& s,
        std::shared_ptr<const NativeProductCompositionLift> prior, const algebra::ProductMapAssociativityLift& lift,
        NativeProductAssociativityAvailability a, const runtime::ExecutionLease& lease, numeric::ExactActionGuard& guard) {
        auto r = native_product_map_detail::report(*s.last.state_, NativeProductMapAction::Lift, {a[2], a[3]}, lease,
            guard, s.last.source().numeric_plan().carrier(lift.last().input()));
        r.execution_steps = lift.staged_work()[1].steps();
        return std::make_shared<const NativeProductMapLift>(NativeProductMapLift(s.last, prior->stages()[1], lift.last(), std::move(r)));
    }
    std::shared_ptr<const native_product_associativity_detail::State> state_;
};
class NativeProductAssociativityLift {
public:
    PreparedNativeProductMapAssociativity plan() const { return plan_; }
    const algebra::ProductMapAssociativityLift& lift() const & { return lift_; }
    const algebra::ProductMapAssociativityLift& lift() const && = delete;
    std::shared_ptr<const NativeProductCompositionLift> prefix_input() const { return prefix_; }
    std::shared_ptr<const NativeProductMapLift> last_stage() const { return last_; }
    std::shared_ptr<const DerivedNativeHexagonField> input() const { return prefix_->input(); }
    const NativeProductAssociativityProvenance& provenance() const & { return report_; }
    const NativeProductAssociativityProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductMapAssociativity;
    NativeProductAssociativityLift(PreparedNativeProductMapAssociativity p, algebra::ProductMapAssociativityLift lift,
        std::shared_ptr<const NativeProductCompositionLift> prefix, std::shared_ptr<const NativeProductMapLift> last,
        NativeProductAssociativityProvenance r)
        : plan_(std::move(p)), lift_(std::move(lift)), prefix_(std::move(prefix)), last_(std::move(last)), report_(std::move(r)) {}
    PreparedNativeProductMapAssociativity plan_;
    algebra::ProductMapAssociativityLift lift_;
    std::shared_ptr<const NativeProductCompositionLift> prefix_;
    std::shared_ptr<const NativeProductMapLift> last_;
    NativeProductAssociativityProvenance report_;
};
class NativeProductAssociativityWitness {
public:
    PreparedNativeProductMapAssociativity plan() const { return plan_; }
    const algebra::ProductMapAssociativityWitness& witness() const & { return witness_; }
    const algebra::ProductMapAssociativityWitness& witness() const && = delete;
    std::shared_ptr<const NativeProductCompositionLift> prefix_input() const { return prefix_; }
    std::shared_ptr<const NativeProductMapLift> last_stage() const { return last_; }
    const NativeProductAssociativityProvenance& provenance() const & { return report_; }
    const NativeProductAssociativityProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductMapAssociativity;
    NativeProductAssociativityWitness(PreparedNativeProductMapAssociativity p, algebra::ProductMapAssociativityWitness w,
        std::shared_ptr<const NativeProductCompositionLift> prefix, std::shared_ptr<const NativeProductMapLift> last,
        NativeProductAssociativityProvenance r)
        : plan_(std::move(p)), witness_(std::move(w)), prefix_(std::move(prefix)), last_(std::move(last)), report_(std::move(r)) {}
    PreparedNativeProductMapAssociativity plan_;
    algebra::ProductMapAssociativityWitness witness_;
    std::shared_ptr<const NativeProductCompositionLift> prefix_;
    std::shared_ptr<const NativeProductMapLift> last_;
    NativeProductAssociativityProvenance report_;
};
inline NativeProductAssociativityLift PreparedNativeProductMapAssociativity::apply(DerivedNativeHexagonField input,
    NativeProductAssociativityAvailability a, runtime::ExecutionLease lease) const {
    namespace detail = native_product_associativity_detail; const auto pin = state_; admit(*pin, a, lease);
    detail::require(pin->prefix.first().source().same_owner(input.plan()), "native associativity requires its actual source field owner");
    detail::Guard guard(native_product_detail::limits(lease, detail::pins(pin->prefix, pin->last), pin->dimension), [&lease]{ lease.check_cancelled(); });
    const auto carrier = pin->numeric.prefix().first().source().carrier(input.field());
    auto lift = pin->numeric.apply(input.field(), guard); lease.check_cancelled();
    auto prior = prefix_receipt(*pin, std::move(input), lift, a, lease, guard);
    auto last = last_receipt(*pin, prior, lift, a, lease, guard);
    return NativeProductAssociativityLift(PreparedNativeProductMapAssociativity(pin), std::move(lift), std::move(prior), std::move(last),
        report(*pin, NativeProductAssociativityAction::Lift, a, lease, guard, carrier));
}
inline NativeProductAssociativityLift PreparedNativeProductMapAssociativity::continue_from(
    std::shared_ptr<const NativeProductCompositionLift> prior, NativeProductAssociativityAvailability a, runtime::ExecutionLease lease) const {
    namespace detail = native_product_associativity_detail; const auto pin = state_; admit(*pin, a, lease);
    detail::require(prior && pin->prefix.same_owner(prior->plan()), "native continuation requires its actual prefix composition receipt");
    auto limits = native_product_detail::limits(lease, detail::pins(pin->prefix, pin->last), pin->dimension);
    const auto previous = prior->provenance().action_limits;
    limits.dimension = std::min(limits.dimension, previous.dimension);
    limits.integer_bits = std::min(limits.integer_bits, previous.integer_bits);
    limits.steps = std::min(limits.steps, previous.steps);
    detail::Guard guard(limits, [&lease]{ lease.check_cancelled(); });
    const auto prior_steps = prior->provenance().execution_steps;
    // Charge the retained execution before doing new work. Checkpoints remain
    // cancellable; no new scalar execution is claimed for these accounting steps.
    for (std::size_t j = 0; j < prior_steps; ++j) guard.step();
    const auto carrier = pin->numeric.prefix().first().source().carrier(prior->lift().first().input());
    auto lift = pin->numeric.continue_from(prior->lift(), guard); lease.check_cancelled();
    auto last = last_receipt(*pin, prior, lift, a, lease, guard);
    return NativeProductAssociativityLift(PreparedNativeProductMapAssociativity(pin), std::move(lift), std::move(prior), std::move(last),
        report(*pin, NativeProductAssociativityAction::Continuation, a, lease, guard, carrier, prior_steps));
}
inline NativeProductAssociativityWitness PreparedNativeProductMapAssociativity::witness(DerivedNativeHexagonField input,
    NativeProductAssociativityAvailability a, runtime::ExecutionLease lease) const {
    namespace detail = native_product_associativity_detail; const auto pin = state_; admit(*pin, a, lease);
    detail::require(pin->prefix.first().source().same_owner(input.plan()), "native associativity witness requires its actual source field owner");
    detail::Guard guard(native_product_detail::limits(lease, detail::pins(pin->prefix, pin->last), pin->dimension), [&lease]{ lease.check_cancelled(); });
    const auto carrier = pin->numeric.prefix().first().source().carrier(input.field());
    auto w = pin->numeric.witness(input.field(), guard); lease.check_cancelled();
    auto prior = prefix_receipt(*pin, std::move(input), w.routes[0].prefixes[0], a, lease, guard);
    auto last = last_receipt(*pin, prior, w.routes[0].prefixes[0], a, lease, guard);
    return NativeProductAssociativityWitness(PreparedNativeProductMapAssociativity(pin), std::move(w), std::move(prior), std::move(last),
        report(*pin, NativeProductAssociativityAction::CoherenceWitness, a, lease, guard, carrier));
}
} // namespace rfc::inference
