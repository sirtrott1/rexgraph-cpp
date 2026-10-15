#pragma once
#include "rfc/algebra/owned_product_hexagon.hpp"
#include "rfc/inference/native_tensor_product.hpp"

namespace rfc::inference {
using NativeProductHexagonAvailability = std::array<runtime::WorkAvailability, 3>;
struct NativeProductHexagonRequest {
    std::array<storage::PinnedNativeAccession, 3> factors;
    std::array<std::uint64_t, 3> factor_identities;
    algebra::ProductHexagonDeclaration declaration;
    std::size_t dimension_limit{64};
};
enum class NativeProductHexagonAction { DeclaredField, Boundary, Step, ChainWitness, CellWitness };
struct NativeProductHexagonProvenance {
    NativeProductHexagonAction action{NativeProductHexagonAction::DeclaredField};
    std::uint64_t declaration{}, revision{};
    std::array<std::uint64_t, 3> factors, namespaces, sequences;
    NativeProductHexagonAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
    algebra::ProductHexagonCarrier source_carrier, target_carrier;
    std::optional<algebra::ProductHexagonAction> step;
};
namespace native_hexagon_detail {
using algebra::product_hexagon_detail::add;
using algebra::product_hexagon_detail::mul;
using algebra::product_hexagon_detail::require;
using native_product_detail::Guard;
inline void available(const NativeProductHexagonRequest& r, NativeProductHexagonAvailability a) {
    for (std::size_t f = 0; f < 3; ++f) runtime::admit_work_availability(r.factors[f].source_namespace(), r.factors[f].sequence(), a[f]);
}
struct State {
    NativeProductHexagonRequest request;
    algebra::OwnedProductHexagon numeric;
    NativeProductHexagonAvailability preparation;
    std::size_t steps{};
    std::string policy;
    bool fallback{};
};
inline runtime::ExecutionRequirements requirements(const State& s, std::size_t support = 0) {
    return native_product_detail::requirements(s.numeric.retained_words(), add(mul(s.numeric.entry_envelope(), 2), mul(support, 512)));
}
inline void admit(const State& s, NativeProductHexagonAvailability a, const runtime::ExecutionLease& lease, std::size_t support = 0) {
    lease.check_cancelled(); available(s.request, s.preparation); available(s.request, a);
    runtime::admit_execution(requirements(s, support), lease.policy());
}
inline NativeProductHexagonProvenance report(const State& s, NativeProductHexagonAction action,
    NativeProductHexagonAvailability a, const runtime::ExecutionLease& lease, Guard& guard, algebra::ProductHexagonCarrier source_carrier, algebra::ProductHexagonCarrier target_carrier, std::size_t support = 0, std::optional<algebra::ProductHexagonAction> step = {}) {
    std::array<std::uint64_t, 3> namespaces{}, sequences{};
    for (std::size_t f = 0; f < 3; ++f) { namespaces[f] = s.request.factors[f].source_namespace(); sequences[f] = s.request.factors[f].sequence(); }
    return {action, s.request.declaration.key, s.request.declaration.revision, s.request.factor_identities, namespaces, sequences,
        s.preparation, a, requirements(s, support), guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu, source_carrier, target_carrier, step};
}
} // namespace native_hexagon_detail
class DerivedNativeHexagonField;
class NativeProductHexagonChainWitness;
class NativeProductHexagonCellWitness;
class PreparedNativeProductHexagon {
public:
    using Values = algebra::OwnedProductHexagon::Values;
    static PreparedNativeProductHexagon prepare(NativeProductHexagonRequest input, NativeProductHexagonAvailability a,
        runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_hexagon_detail;
        auto r = input;
        lease.check_cancelled(); detail::available(r, a);
        detail::require(r.dimension_limit > 0, "native hexagon requires a positive dimension limit");
        for (std::size_t f = 0; f < 3; ++f) {
            detail::require(r.factor_identities[f] && r.factors[f].kind() == storage::NativeAccessionKind::Chain && r.factors[f].support_count() == 0,
                "native hexagon requires named empty owning Chain anchors");
            for (std::size_t j = 0; j < f; ++j) if (r.factor_identities[j] == r.factor_identities[f])
                detail::require(r.factors[j].endpoint() == r.factors[f].endpoint() && r.factors[j].same_execution_authority(r.factors[f]),
                    "native hexagon aliases different publications or execution authorities");
        }
        const std::array<const core::RelationalComplex*, 3> sources{&r.factors[0].endpoint()->source, &r.factors[1].endpoint()->source, &r.factors[2].endpoint()->source};
        const auto words = algebra::product_hexagon_detail::words(sources, r.declaration), envelope = detail::add(1, detail::mul(words, 16));
        runtime::admit_execution(native_product_detail::requirements(words, detail::mul(envelope,2)), lease.policy());
        detail::Guard guard(native_product_detail::limits(lease, r.factors, r.dimension_limit), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::OwnedProductHexagon::prepare({*sources[0], *sources[1], *sources[2]}, r.declaration, guard); lease.check_cancelled();
        return PreparedNativeProductHexagon(std::make_shared<const native_hexagon_detail::State>(native_hexagon_detail::State{
            std::move(r), std::move(numeric), a, guard.steps(), lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    algebra::OwnedProductHexagon numeric_plan() const { return state_->numeric; }
    storage::PinnedNativeAccession factor(std::size_t f) const { return state_->request.factors.at(f); }
    const NativeProductHexagonRequest& declaration() const & { return state_->request; }
    const NativeProductHexagonRequest& declaration() const && = delete;
    NativeProductHexagonAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeProductHexagon& other) const noexcept { return state_ == other.state_; }
    DerivedNativeHexagonField field(algebra::ProductHexagonCarrier, core::Grade, Values, NativeProductHexagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeHexagonField boundary(DerivedNativeHexagonField, NativeProductHexagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeHexagonField step(DerivedNativeHexagonField, algebra::ProductHexagonAction, NativeProductHexagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductHexagonChainWitness witness(DerivedNativeHexagonField, NativeProductHexagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductHexagonCellWitness cell_witness(std::array<core::CellId,3>, NativeProductHexagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeHexagonField;
    explicit PreparedNativeProductHexagon(std::shared_ptr<const native_hexagon_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_hexagon_detail::State> state_;
};
class DerivedNativeHexagonField {
public:
    PreparedNativeProductHexagon plan() const { return plan_; }
    const algebra::OwnedAssociatedProductField& field() const & { return field_; }
    const algebra::OwnedAssociatedProductField& field() const && = delete;
    const NativeProductHexagonProvenance& provenance() const & { return report_; }
    const NativeProductHexagonProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeHexagonField> input() const { return input_; }
private:
    friend class PreparedNativeProductHexagon;
    DerivedNativeHexagonField(PreparedNativeProductHexagon p, algebra::OwnedAssociatedProductField f, NativeProductHexagonProvenance r,
        std::shared_ptr<const DerivedNativeHexagonField> input = {}) : plan_(std::move(p)), field_(std::move(f)), report_(std::move(r)), input_(std::move(input)) {}
    PreparedNativeProductHexagon plan_;
    algebra::OwnedAssociatedProductField field_;
    NativeProductHexagonProvenance report_;
    std::shared_ptr<const DerivedNativeHexagonField> input_;
};
class NativeProductHexagonChainWitness {
public:
    PreparedNativeProductHexagon plan() const { return plan_; }
    std::shared_ptr<const DerivedNativeHexagonField> input() const { return input_; }
    const algebra::ProductHexagonChainWitness& witness() const & { return witness_; }
    const algebra::ProductHexagonChainWitness& witness() const && = delete;
    const NativeProductHexagonProvenance& provenance() const & { return report_; }
    const NativeProductHexagonProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductHexagon;
    NativeProductHexagonChainWitness(PreparedNativeProductHexagon p, DerivedNativeHexagonField input,
        algebra::ProductHexagonChainWitness w, NativeProductHexagonProvenance r)
        : plan_(std::move(p)), input_(std::make_shared<const DerivedNativeHexagonField>(std::move(input))), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductHexagon plan_;
    std::shared_ptr<const DerivedNativeHexagonField> input_;
    algebra::ProductHexagonChainWitness witness_;
    NativeProductHexagonProvenance report_;
};
class NativeProductHexagonCellWitness {
public:
    PreparedNativeProductHexagon plan() const { return plan_; }
    const algebra::ProductHexagonCellWitness& witness() const & { return witness_; }
    const algebra::ProductHexagonCellWitness& witness() const && = delete;
    const NativeProductHexagonProvenance& provenance() const & { return report_; }
    const NativeProductHexagonProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductHexagon;
    NativeProductHexagonCellWitness(PreparedNativeProductHexagon p, algebra::ProductHexagonCellWitness w, NativeProductHexagonProvenance r)
        : plan_(std::move(p)), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductHexagon plan_;
    algebra::ProductHexagonCellWitness witness_;
    NativeProductHexagonProvenance report_;
};
inline DerivedNativeHexagonField PreparedNativeProductHexagon::field(algebra::ProductHexagonCarrier a, core::Grade g, Values values,
    NativeProductHexagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_hexagon_detail; const auto pin = state_; auto frozen = values; const auto n = frozen.size(); detail::admit(*pin, available, lease, n);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.field(a, g, std::move(frozen), guard); lease.check_cancelled();
    return DerivedNativeHexagonField(PreparedNativeProductHexagon(pin), std::move(out), detail::report(*pin, NativeProductHexagonAction::DeclaredField, available, lease, guard, a, a, n));
}
inline DerivedNativeHexagonField PreparedNativeProductHexagon::boundary(DerivedNativeHexagonField input,
    NativeProductHexagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_hexagon_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native hexagon boundary needs its actual owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.boundary(input.field_, guard); lease.check_cancelled();
    auto report = detail::report(*pin, NativeProductHexagonAction::Boundary, available, lease, guard, pin->numeric.carrier(input.field_), pin->numeric.carrier(input.field_), n);
    return DerivedNativeHexagonField(PreparedNativeProductHexagon(pin), std::move(out), std::move(report),
        std::make_shared<const DerivedNativeHexagonField>(std::move(input)));
}
inline DerivedNativeHexagonField PreparedNativeProductHexagon::step(DerivedNativeHexagonField input, algebra::ProductHexagonAction action,
    NativeProductHexagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_hexagon_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native hexagon needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.step(input.field_, action, guard); lease.check_cancelled();
    auto report = detail::report(*pin, NativeProductHexagonAction::Step, available, lease, guard, pin->numeric.carrier(input.field_), pin->numeric.carrier(out), n, action);
    return DerivedNativeHexagonField(PreparedNativeProductHexagon(pin), std::move(out), std::move(report),
        std::make_shared<const DerivedNativeHexagonField>(std::move(input)));
}
inline NativeProductHexagonChainWitness PreparedNativeProductHexagon::witness(DerivedNativeHexagonField input,
    NativeProductHexagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_hexagon_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native chain witness needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.witness(input.field_, guard); lease.check_cancelled();
    auto report = detail::report(*pin, NativeProductHexagonAction::ChainWitness, available, lease, guard, pin->numeric.carrier(input.field_), algebra::product_hexagon_detail::target(pin->numeric.carrier(input.field_),algebra::ProductHexagonAction::BlockInterchange), n);
    return NativeProductHexagonChainWitness(PreparedNativeProductHexagon(pin), std::move(input), std::move(out), std::move(report));
}
inline NativeProductHexagonCellWitness PreparedNativeProductHexagon::cell_witness(std::array<core::CellId,3> c,
    NativeProductHexagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_hexagon_detail; const auto pin = state_; detail::admit(*pin, available, lease);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.cell_witness(c, guard); lease.check_cancelled();
    return NativeProductHexagonCellWitness(PreparedNativeProductHexagon(pin), std::move(out), detail::report(*pin, NativeProductHexagonAction::CellWitness, available, lease, guard, {algebra::ProductPermutation::ABC,algebra::ProductAssociation::Left}, {algebra::ProductPermutation::ABC,algebra::ProductAssociation::Left}));
}
} // namespace rfc::inference
