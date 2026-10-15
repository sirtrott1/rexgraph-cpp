#pragma once
#include "rfc/algebra/owned_product_chain_map.hpp"
#include "rfc/inference/native_product_hexagon.hpp"

namespace rfc::inference {
struct NativeProductMapAvailability {
    NativeProductHexagonAvailability source, target;
};
enum class NativeProductMapAction { Lift, CoherenceWitness };
struct NativeProductMapProvenance {
    NativeProductMapAction action{NativeProductMapAction::Lift};
    std::uint64_t declaration{}, revision{};
    // The two sides and their original A/B/C roles never follow permutations.
    std::array<std::array<std::uint64_t, 3>, 2> factors, namespaces, sequences;
    std::array<NativeProductHexagonAvailability, 2> product_preparation;
    NativeProductMapAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
    algebra::ProductHexagonCarrier carrier;
};
namespace native_product_map_detail {
using namespace native_product_detail;
struct State {
    PreparedNativeProductHexagon source, target;
    algebra::PreparedProductChainMap numeric;
    NativeProductMapAvailability preparation;
    std::size_t dimension{}, steps{};
    std::string policy;
    bool fallback{};
};
inline std::array<Pin, 6> pins(const PreparedNativeProductHexagon& source, const PreparedNativeProductHexagon& target) {
    return {source.factor(0), source.factor(1), source.factor(2), target.factor(0), target.factor(1), target.factor(2)};
}
inline void available(const PreparedNativeProductHexagon& source, const PreparedNativeProductHexagon& target, NativeProductMapAvailability a) {
    for (std::size_t f = 0; f < 3; ++f) {
        runtime::admit_work_availability(source.factor(f).source_namespace(), source.factor(f).sequence(), a.source[f]);
        runtime::admit_work_availability(target.factor(f).source_namespace(), target.factor(f).sequence(), a.target[f]);
    }
}
inline runtime::ExecutionRequirements requirements(const State& s) {
    return native_product_detail::requirements(s.numeric.retained_words(), mul(s.numeric.entry_envelope(), 2));
}
inline void admit(const State& s, NativeProductMapAvailability a, const runtime::ExecutionLease& lease) {
    lease.check_cancelled(); available(s.source, s.target, s.preparation); available(s.source, s.target, a);
    available(s.source, s.target, {s.source.preparation_availability(), s.target.preparation_availability()});
    runtime::admit_execution(requirements(s), lease.policy());
}
inline NativeProductMapProvenance report(const State& s, NativeProductMapAction action, NativeProductMapAvailability a,
    const runtime::ExecutionLease& lease, Guard& guard, algebra::ProductHexagonCarrier c) {
    std::array<std::array<std::uint64_t, 3>, 2> roles{}, namespaces{}, sequences{};
    for (std::size_t j = 0; j < 2; ++j) {
        const auto& h = j ? s.target : s.source;
        roles[j] = h.declaration().factor_identities;
        for (std::size_t f = 0; f < 3; ++f) { namespaces[j][f] = h.factor(f).source_namespace(); sequences[j][f] = h.factor(f).sequence(); }
    }
    return {action, s.numeric.declaration().key, s.numeric.declaration().revision, roles, namespaces, sequences,
        {s.source.preparation_availability(), s.target.preparation_availability()}, s.preparation, a,
        requirements(s), guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu, c};
}
} // namespace native_product_map_detail
class NativeProductMapLift;
class NativeProductMapCoherenceWitness;
class PreparedNativeProductMapComposition;
class PreparedNativeProductMapAssociativity;
class PreparedNativeProductChainMap {
public:
    static PreparedNativeProductChainMap prepare(PreparedNativeProductHexagon source, PreparedNativeProductHexagon target,
        algebra::ProductChainMapDeclaration input, NativeProductMapAvailability a,
        runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_product_map_detail;
        lease.check_cancelled(); detail::available(source, target, a);
        detail::available(source, target, {source.preparation_availability(), target.preparation_availability()});
        const auto sn = source.numeric_plan(), tn = target.numeric_plan();
        const auto [words, envelope] = algebra::product_map_detail::bounds(sn, tn, input);
        runtime::admit_execution(native_product_detail::requirements(words, detail::mul(envelope, 2)), lease.policy());
        auto d = input;
        const auto dimension = std::min(source.declaration().dimension_limit, target.declaration().dimension_limit);
        detail::Guard guard(native_product_detail::limits(lease, detail::pins(source, target), dimension), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::PreparedProductChainMap::prepare(sn, tn, std::move(d), guard); lease.check_cancelled();
        return PreparedNativeProductChainMap(std::make_shared<const detail::State>(detail::State{
            std::move(source), std::move(target), std::move(numeric), a, dimension, guard.steps(), lease.policy().name,
            lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeProductHexagon source() const { return state_->source; }
    PreparedNativeProductHexagon target() const { return state_->target; }
    algebra::PreparedProductChainMap numeric_plan() const { return state_->numeric; }
    bool same_owner(const PreparedNativeProductChainMap& p) const noexcept { return state_ == p.state_; }
    NativeProductMapLift apply(DerivedNativeHexagonField, NativeProductMapAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductMapCoherenceWitness witness(DerivedNativeHexagonField, NativeProductMapAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class NativeProductMapLift;
    friend class NativeProductMapCoherenceWitness;
    friend class PreparedNativeProductMapComposition;
    friend class PreparedNativeProductMapAssociativity;
    explicit PreparedNativeProductChainMap(std::shared_ptr<const native_product_map_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_product_map_detail::State> state_;
};
class NativeProductMapLift {
public:
    PreparedNativeProductChainMap plan() const { return plan_; }
    const algebra::ProductMapLift& lift() const & { return lift_; }
    const algebra::ProductMapLift& lift() const && = delete;
    std::shared_ptr<const DerivedNativeHexagonField> input() const { return input_; }
    // A staged map retains its actual prior map receipt. input() is the native
    // origin; it is never rebound to a fabricated declaration at the midpoint.
    std::shared_ptr<const NativeProductMapLift> prior_input() const { return prior_; }
    const NativeProductMapProvenance& provenance() const & { return report_; }
    const NativeProductMapProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductChainMap;
    friend class PreparedNativeProductMapComposition;
    friend class PreparedNativeProductMapAssociativity;
    NativeProductMapLift(PreparedNativeProductChainMap p, DerivedNativeHexagonField input, algebra::ProductMapLift lift,
        NativeProductMapProvenance r) : plan_(std::move(p)), input_(std::make_shared<const DerivedNativeHexagonField>(std::move(input))),
        lift_(std::move(lift)), report_(std::move(r)) {}
    NativeProductMapLift(PreparedNativeProductChainMap p, std::shared_ptr<const NativeProductMapLift> prior,
        algebra::ProductMapLift lift, NativeProductMapProvenance r)
        : plan_(std::move(p)), input_(prior->input()), lift_(std::move(lift)), report_(std::move(r)), prior_(std::move(prior)) {}
    PreparedNativeProductChainMap plan_;
    std::shared_ptr<const DerivedNativeHexagonField> input_;
    algebra::ProductMapLift lift_;
    NativeProductMapProvenance report_;
    std::shared_ptr<const NativeProductMapLift> prior_;
};
class NativeProductMapCoherenceWitness {
public:
    PreparedNativeProductChainMap plan() const { return plan_; }
    const algebra::ProductMapCoherenceWitness& witness() const & { return witness_; }
    const algebra::ProductMapCoherenceWitness& witness() const && = delete;
    std::shared_ptr<const DerivedNativeHexagonField> input() const { return input_; }
    const NativeProductMapProvenance& provenance() const & { return report_; }
    const NativeProductMapProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductChainMap;
    NativeProductMapCoherenceWitness(PreparedNativeProductChainMap p, DerivedNativeHexagonField input,
        algebra::ProductMapCoherenceWitness witness, NativeProductMapProvenance r)
        : plan_(std::move(p)), input_(std::make_shared<const DerivedNativeHexagonField>(std::move(input))),
          witness_(std::move(witness)), report_(std::move(r)) {}
    PreparedNativeProductChainMap plan_;
    std::shared_ptr<const DerivedNativeHexagonField> input_;
    algebra::ProductMapCoherenceWitness witness_;
    NativeProductMapProvenance report_;
};
inline NativeProductMapLift PreparedNativeProductChainMap::apply(DerivedNativeHexagonField input,
    NativeProductMapAvailability a, runtime::ExecutionLease lease) const {
    namespace detail = native_product_map_detail; const auto pin = state_; detail::admit(*pin, a, lease);
    detail::require(pin->source.same_owner(input.plan()), "native product map needs its actual source field owner");
    detail::Guard guard(native_product_detail::limits(lease, detail::pins(pin->source, pin->target), pin->dimension), [&lease]{ lease.check_cancelled(); });
    const auto carrier = pin->numeric.source().carrier(input.field()); auto lift = pin->numeric.apply(input.field(), guard); lease.check_cancelled();
    return NativeProductMapLift(PreparedNativeProductChainMap(pin), std::move(input), std::move(lift),
        detail::report(*pin, NativeProductMapAction::Lift, a, lease, guard, carrier));
}
inline NativeProductMapCoherenceWitness PreparedNativeProductChainMap::witness(DerivedNativeHexagonField input,
    NativeProductMapAvailability a, runtime::ExecutionLease lease) const {
    namespace detail = native_product_map_detail; const auto pin = state_; detail::admit(*pin, a, lease);
    detail::require(pin->source.same_owner(input.plan()), "native product map witness needs its actual source field owner");
    detail::Guard guard(native_product_detail::limits(lease, detail::pins(pin->source, pin->target), pin->dimension), [&lease]{ lease.check_cancelled(); });
    const auto carrier = pin->numeric.source().carrier(input.field()); auto witness = pin->numeric.witness(input.field(), guard); lease.check_cancelled();
    return NativeProductMapCoherenceWitness(PreparedNativeProductChainMap(pin), std::move(input), std::move(witness),
        detail::report(*pin, NativeProductMapAction::CoherenceWitness, a, lease, guard, carrier));
}
} // namespace rfc::inference
