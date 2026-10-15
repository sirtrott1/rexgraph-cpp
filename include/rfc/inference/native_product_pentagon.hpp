#pragma once
#include "rfc/algebra/owned_product_pentagon.hpp"
#include "rfc/inference/native_tensor_product.hpp"

namespace rfc::inference {
using NativeProductPentagonAvailability = std::array<runtime::WorkAvailability, 4>;
struct NativeProductPentagonRequest {
    std::array<storage::PinnedNativeAccession, 4> factors;
    std::array<std::uint64_t, 4> factor_identities;
    algebra::ProductPentagonDeclaration declaration;
    std::size_t dimension_limit{64};
};
enum class NativeProductPentagonAction { DeclaredField, Boundary, Edge, ChainWitness, CellWitness };
struct NativeProductPentagonProvenance {
    NativeProductPentagonAction action{NativeProductPentagonAction::DeclaredField};
    std::uint64_t declaration{}, revision{};
    std::array<std::uint64_t, 4> factors, namespaces, sequences;
    NativeProductPentagonAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
    std::optional<std::size_t> edge;
    bool reverse{};
};
namespace native_pentagon_detail {
using algebra::product_pentagon_detail::add;
using algebra::product_pentagon_detail::mul;
using algebra::product_pentagon_detail::require;
using native_product_detail::Guard;
inline void available(const NativeProductPentagonRequest& r, NativeProductPentagonAvailability a) {
    for (std::size_t f = 0; f < 4; ++f) runtime::admit_work_availability(r.factors[f].source_namespace(), r.factors[f].sequence(), a[f]);
}
struct State {
    NativeProductPentagonRequest request;
    algebra::OwnedProductPentagon numeric;
    NativeProductPentagonAvailability preparation;
    std::size_t steps{};
    std::string policy;
    bool fallback{};
};
inline runtime::ExecutionRequirements requirements(const State& s, std::size_t support = 0) {
    return native_product_detail::requirements(s.numeric.retained_words(), add(mul(s.numeric.entry_envelope(), 2), mul(support, 128)));
}
inline void admit(const State& s, NativeProductPentagonAvailability a, const runtime::ExecutionLease& lease, std::size_t support = 0) {
    lease.check_cancelled(); available(s.request, s.preparation); available(s.request, a);
    runtime::admit_execution(requirements(s, support), lease.policy());
}
inline NativeProductPentagonProvenance report(const State& s, NativeProductPentagonAction action,
    NativeProductPentagonAvailability a, const runtime::ExecutionLease& lease, Guard& guard, std::size_t support = 0, std::optional<std::size_t> edge = {}, bool reverse = false) {
    std::array<std::uint64_t, 4> namespaces{}, sequences{};
    for (std::size_t f = 0; f < 4; ++f) { namespaces[f] = s.request.factors[f].source_namespace(); sequences[f] = s.request.factors[f].sequence(); }
    return {action, s.request.declaration.key, s.request.declaration.revision, s.request.factor_identities, namespaces, sequences,
        s.preparation, a, requirements(s, support), guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu, edge, reverse};
}
} // namespace native_pentagon_detail
class DerivedNativePentagonField;
class NativeProductPentagonChainWitness;
class NativeProductPentagonCellWitness;
class PreparedNativeProductPentagon {
public:
    using Values = algebra::OwnedProductPentagon::Values;
    static PreparedNativeProductPentagon prepare(NativeProductPentagonRequest input, NativeProductPentagonAvailability a,
        runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_pentagon_detail;
        auto r = input;
        lease.check_cancelled(); detail::available(r, a);
        detail::require(r.dimension_limit > 0, "native pentagon requires a positive dimension limit");
        for (std::size_t f = 0; f < 4; ++f) {
            detail::require(r.factor_identities[f] && r.factors[f].kind() == storage::NativeAccessionKind::Chain && r.factors[f].support_count() == 0,
                "native pentagon requires named empty owning Chain anchors");
            for (std::size_t j = 0; j < f; ++j) if (r.factor_identities[j] == r.factor_identities[f])
                detail::require(r.factors[j].endpoint() == r.factors[f].endpoint() && r.factors[j].same_execution_authority(r.factors[f]),
                    "native pentagon aliases different publications or execution authorities");
        }
        const std::array<const core::RelationalComplex*, 4> sources{&r.factors[0].endpoint()->source, &r.factors[1].endpoint()->source, &r.factors[2].endpoint()->source, &r.factors[3].endpoint()->source};
        const auto words = algebra::product_pentagon_detail::words(sources, r.declaration), envelope = detail::add(1, detail::mul(words, 16));
        runtime::admit_execution(native_product_detail::requirements(words, detail::mul(envelope,2)), lease.policy());
        detail::Guard guard(native_product_detail::limits(lease, r.factors, r.dimension_limit), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::OwnedProductPentagon::prepare({*sources[0], *sources[1], *sources[2], *sources[3]}, r.declaration, guard); lease.check_cancelled();
        return PreparedNativeProductPentagon(std::make_shared<const native_pentagon_detail::State>(native_pentagon_detail::State{
            std::move(r), std::move(numeric), a, guard.steps(), lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    algebra::OwnedProductPentagon numeric_plan() const { return state_->numeric; }
    storage::PinnedNativeAccession factor(std::size_t f) const { return state_->request.factors.at(f); }
    const NativeProductPentagonRequest& declaration() const & { return state_->request; }
    const NativeProductPentagonRequest& declaration() const && = delete;
    NativeProductPentagonAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeProductPentagon& other) const noexcept { return state_ == other.state_; }
    DerivedNativePentagonField field(algebra::ProductParenthesization, core::Grade, Values, NativeProductPentagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativePentagonField boundary(DerivedNativePentagonField, NativeProductPentagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativePentagonField traverse(DerivedNativePentagonField, std::size_t edge, bool reverse, NativeProductPentagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductPentagonChainWitness witness(DerivedNativePentagonField, NativeProductPentagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductPentagonCellWitness cell_witness(algebra::PentagonCell, NativeProductPentagonAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativePentagonField;
    explicit PreparedNativeProductPentagon(std::shared_ptr<const native_pentagon_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_pentagon_detail::State> state_;
};
class DerivedNativePentagonField {
public:
    PreparedNativeProductPentagon plan() const { return plan_; }
    const algebra::OwnedPentagonField& field() const & { return field_; }
    const algebra::OwnedPentagonField& field() const && = delete;
    const NativeProductPentagonProvenance& provenance() const & { return report_; }
    const NativeProductPentagonProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativePentagonField> input() const { return input_; }
private:
    friend class PreparedNativeProductPentagon;
    DerivedNativePentagonField(PreparedNativeProductPentagon p, algebra::OwnedPentagonField f, NativeProductPentagonProvenance r,
        std::shared_ptr<const DerivedNativePentagonField> input = {}) : plan_(std::move(p)), field_(std::move(f)), report_(std::move(r)), input_(std::move(input)) {}
    PreparedNativeProductPentagon plan_;
    algebra::OwnedPentagonField field_;
    NativeProductPentagonProvenance report_;
    std::shared_ptr<const DerivedNativePentagonField> input_;
};
class NativeProductPentagonChainWitness {
public:
    PreparedNativeProductPentagon plan() const { return plan_; }
    std::shared_ptr<const DerivedNativePentagonField> input() const { return input_; }
    const algebra::ProductPentagonChainWitness& witness() const & { return witness_; }
    const algebra::ProductPentagonChainWitness& witness() const && = delete;
    const NativeProductPentagonProvenance& provenance() const & { return report_; }
    const NativeProductPentagonProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductPentagon;
    NativeProductPentagonChainWitness(PreparedNativeProductPentagon p, DerivedNativePentagonField input,
        algebra::ProductPentagonChainWitness w, NativeProductPentagonProvenance r)
        : plan_(std::move(p)), input_(std::make_shared<const DerivedNativePentagonField>(std::move(input))), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductPentagon plan_;
    std::shared_ptr<const DerivedNativePentagonField> input_;
    algebra::ProductPentagonChainWitness witness_;
    NativeProductPentagonProvenance report_;
};
class NativeProductPentagonCellWitness {
public:
    PreparedNativeProductPentagon plan() const { return plan_; }
    const algebra::ProductPentagonCellWitness& witness() const & { return witness_; }
    const algebra::ProductPentagonCellWitness& witness() const && = delete;
    const NativeProductPentagonProvenance& provenance() const & { return report_; }
    const NativeProductPentagonProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductPentagon;
    NativeProductPentagonCellWitness(PreparedNativeProductPentagon p, algebra::ProductPentagonCellWitness w, NativeProductPentagonProvenance r)
        : plan_(std::move(p)), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductPentagon plan_;
    algebra::ProductPentagonCellWitness witness_;
    NativeProductPentagonProvenance report_;
};
inline DerivedNativePentagonField PreparedNativeProductPentagon::field(algebra::ProductParenthesization a, core::Grade g, Values values,
    NativeProductPentagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_pentagon_detail; const auto pin = state_; auto frozen = values; const auto n = frozen.size(); detail::admit(*pin, available, lease, n);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.field(a, g, std::move(frozen), guard); lease.check_cancelled();
    return DerivedNativePentagonField(PreparedNativeProductPentagon(pin), std::move(out), detail::report(*pin, NativeProductPentagonAction::DeclaredField, available, lease, guard, n));
}
inline DerivedNativePentagonField PreparedNativeProductPentagon::boundary(DerivedNativePentagonField input,
    NativeProductPentagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_pentagon_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native pentagon boundary needs its actual owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.boundary(input.field_, guard); lease.check_cancelled();
    return DerivedNativePentagonField(PreparedNativeProductPentagon(pin), std::move(out), detail::report(*pin, NativeProductPentagonAction::Boundary, available, lease, guard, n),
        std::make_shared<const DerivedNativePentagonField>(std::move(input)));
}
inline DerivedNativePentagonField PreparedNativeProductPentagon::traverse(DerivedNativePentagonField input, std::size_t edge, bool reverse,
    NativeProductPentagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_pentagon_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native pentagon needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.traverse(input.field_, edge, reverse, guard); lease.check_cancelled();
    return DerivedNativePentagonField(PreparedNativeProductPentagon(pin), std::move(out), detail::report(*pin, NativeProductPentagonAction::Edge, available, lease, guard, n, edge, reverse),
        std::make_shared<const DerivedNativePentagonField>(std::move(input)));
}
inline NativeProductPentagonChainWitness PreparedNativeProductPentagon::witness(DerivedNativePentagonField input,
    NativeProductPentagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_pentagon_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native chain witness needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.witness(input.field_, guard); lease.check_cancelled();
    return NativeProductPentagonChainWitness(PreparedNativeProductPentagon(pin), std::move(input), std::move(out),
        detail::report(*pin, NativeProductPentagonAction::ChainWitness, available, lease, guard, n));
}
inline NativeProductPentagonCellWitness PreparedNativeProductPentagon::cell_witness(algebra::PentagonCell c,
    NativeProductPentagonAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_pentagon_detail; const auto pin = state_; detail::admit(*pin, available, lease);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.cell_witness(c, guard); lease.check_cancelled();
    return NativeProductPentagonCellWitness(PreparedNativeProductPentagon(pin), std::move(out), detail::report(*pin, NativeProductPentagonAction::CellWitness, available, lease, guard));
}
} // namespace rfc::inference
