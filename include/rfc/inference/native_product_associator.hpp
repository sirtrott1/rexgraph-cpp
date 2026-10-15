#pragma once
#include "rfc/algebra/owned_product_associator.hpp"
#include "rfc/inference/native_tensor_product.hpp"

namespace rfc::inference {
using NativeProductAssociatorAvailability = std::array<runtime::WorkAvailability, 3>;
struct NativeProductAssociatorRequest {
    std::array<storage::PinnedNativeAccession, 3> factors;
    std::array<std::uint64_t, 3> factor_identities;
    algebra::ProductAssociatorDeclaration declaration;
    std::size_t dimension_limit{64};
};
enum class NativeProductAssociatorAction { DeclaredField, Boundary, Reassociate, ChainWitness, CellWitness };
struct NativeProductAssociatorProvenance {
    NativeProductAssociatorAction action{NativeProductAssociatorAction::DeclaredField};
    std::uint64_t declaration{}, revision{};
    std::array<std::uint64_t, 3> factors, namespaces, sequences;
    NativeProductAssociatorAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
namespace native_associator_detail {
using algebra::product_associator_detail::add;
using algebra::product_associator_detail::mul;
using algebra::product_associator_detail::require;
using native_product_detail::Guard;
inline void available(const NativeProductAssociatorRequest& r, NativeProductAssociatorAvailability a) {
    for (std::size_t f = 0; f < 3; ++f) runtime::admit_work_availability(r.factors[f].source_namespace(), r.factors[f].sequence(), a[f]);
}
struct State {
    NativeProductAssociatorRequest request;
    algebra::OwnedProductAssociator numeric;
    NativeProductAssociatorAvailability preparation;
    std::size_t steps{};
    std::string policy;
    bool fallback{};
};
inline runtime::ExecutionRequirements requirements(const State& s, std::size_t support = 0) {
    return native_product_detail::requirements(s.numeric.retained_words(), add(mul(s.numeric.entry_envelope(), 2), mul(support, 64)));
}
inline void admit(const State& s, NativeProductAssociatorAvailability a, const runtime::ExecutionLease& lease, std::size_t support = 0) {
    lease.check_cancelled(); available(s.request, s.preparation); available(s.request, a);
    runtime::admit_execution(requirements(s, support), lease.policy());
}
inline NativeProductAssociatorProvenance report(const State& s, NativeProductAssociatorAction action,
    NativeProductAssociatorAvailability a, const runtime::ExecutionLease& lease, Guard& guard, std::size_t support = 0) {
    std::array<std::uint64_t, 3> namespaces{}, sequences{};
    for (std::size_t f = 0; f < 3; ++f) { namespaces[f] = s.request.factors[f].source_namespace(); sequences[f] = s.request.factors[f].sequence(); }
    return {action, s.request.declaration.key, s.request.declaration.revision, s.request.factor_identities, namespaces, sequences,
        s.preparation, a, requirements(s, support), guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
} // namespace native_associator_detail
class DerivedNativeAssociatedField;
class NativeProductAssociatorChainWitness;
class NativeProductAssociatorCellWitness;
class PreparedNativeProductAssociator {
public:
    using Values = algebra::OwnedProductAssociator::Values;
    static PreparedNativeProductAssociator prepare(NativeProductAssociatorRequest input, NativeProductAssociatorAvailability a,
        runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_associator_detail;
        auto r = input;
        lease.check_cancelled(); detail::available(r, a);
        detail::require(r.dimension_limit > 0, "native associator requires a positive dimension limit");
        for (std::size_t f = 0; f < 3; ++f) {
            detail::require(r.factor_identities[f] && r.factors[f].kind() == storage::NativeAccessionKind::Chain && r.factors[f].support_count() == 0,
                "native associator requires named empty owning Chain anchors");
            for (std::size_t j = 0; j < f; ++j) if (r.factor_identities[j] == r.factor_identities[f])
                detail::require(r.factors[j].endpoint() == r.factors[f].endpoint() && r.factors[j].same_execution_authority(r.factors[f]),
                    "native associator aliases different publications or execution authorities");
        }
        const std::array<const core::RelationalComplex*, 3> sources{&r.factors[0].endpoint()->source, &r.factors[1].endpoint()->source, &r.factors[2].endpoint()->source};
        const auto words = algebra::product_associator_detail::words(sources, r.declaration), envelope = detail::add(1, detail::mul(words, 16));
        runtime::admit_execution(native_product_detail::requirements(words, envelope), lease.policy());
        detail::Guard guard(native_product_detail::limits(lease, r.factors, r.dimension_limit), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::OwnedProductAssociator::prepare({*sources[0], *sources[1], *sources[2]}, r.declaration, guard); lease.check_cancelled();
        return PreparedNativeProductAssociator(std::make_shared<const native_associator_detail::State>(native_associator_detail::State{
            std::move(r), std::move(numeric), a, guard.steps(), lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    algebra::OwnedProductAssociator numeric_plan() const { return state_->numeric; }
    storage::PinnedNativeAccession factor(std::size_t f) const { return state_->request.factors.at(f); }
    const NativeProductAssociatorRequest& declaration() const & { return state_->request; }
    const NativeProductAssociatorRequest& declaration() const && = delete;
    NativeProductAssociatorAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeProductAssociator& other) const noexcept { return state_ == other.state_; }
    DerivedNativeAssociatedField field(algebra::ProductAssociation, core::Grade, Values, NativeProductAssociatorAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeAssociatedField boundary(DerivedNativeAssociatedField, NativeProductAssociatorAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeAssociatedField reassociate(DerivedNativeAssociatedField, NativeProductAssociatorAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductAssociatorChainWitness witness(DerivedNativeAssociatedField, NativeProductAssociatorAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductAssociatorCellWitness cell_witness(algebra::LeftAssociatedCell, NativeProductAssociatorAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeAssociatedField;
    explicit PreparedNativeProductAssociator(std::shared_ptr<const native_associator_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_associator_detail::State> state_;
};
class DerivedNativeAssociatedField {
public:
    PreparedNativeProductAssociator plan() const { return plan_; }
    const algebra::OwnedAssociatedProductField& field() const & { return field_; }
    const algebra::OwnedAssociatedProductField& field() const && = delete;
    const NativeProductAssociatorProvenance& provenance() const & { return report_; }
    const NativeProductAssociatorProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeAssociatedField> input() const { return input_; }
private:
    friend class PreparedNativeProductAssociator;
    DerivedNativeAssociatedField(PreparedNativeProductAssociator p, algebra::OwnedAssociatedProductField f, NativeProductAssociatorProvenance r,
        std::shared_ptr<const DerivedNativeAssociatedField> input = {}) : plan_(std::move(p)), field_(std::move(f)), report_(std::move(r)), input_(std::move(input)) {}
    PreparedNativeProductAssociator plan_;
    algebra::OwnedAssociatedProductField field_;
    NativeProductAssociatorProvenance report_;
    std::shared_ptr<const DerivedNativeAssociatedField> input_;
};
class NativeProductAssociatorChainWitness {
public:
    PreparedNativeProductAssociator plan() const { return plan_; }
    std::shared_ptr<const DerivedNativeAssociatedField> input() const { return input_; }
    const algebra::ProductAssociatorChainWitness& witness() const & { return witness_; }
    const algebra::ProductAssociatorChainWitness& witness() const && = delete;
    const NativeProductAssociatorProvenance& provenance() const & { return report_; }
    const NativeProductAssociatorProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductAssociator;
    NativeProductAssociatorChainWitness(PreparedNativeProductAssociator p, DerivedNativeAssociatedField input,
        algebra::ProductAssociatorChainWitness w, NativeProductAssociatorProvenance r)
        : plan_(std::move(p)), input_(std::make_shared<const DerivedNativeAssociatedField>(std::move(input))), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductAssociator plan_;
    std::shared_ptr<const DerivedNativeAssociatedField> input_;
    algebra::ProductAssociatorChainWitness witness_;
    NativeProductAssociatorProvenance report_;
};
class NativeProductAssociatorCellWitness {
public:
    PreparedNativeProductAssociator plan() const { return plan_; }
    const algebra::ProductAssociatorCellWitness& witness() const & { return witness_; }
    const algebra::ProductAssociatorCellWitness& witness() const && = delete;
    const NativeProductAssociatorProvenance& provenance() const & { return report_; }
    const NativeProductAssociatorProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductAssociator;
    NativeProductAssociatorCellWitness(PreparedNativeProductAssociator p, algebra::ProductAssociatorCellWitness w, NativeProductAssociatorProvenance r)
        : plan_(std::move(p)), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductAssociator plan_;
    algebra::ProductAssociatorCellWitness witness_;
    NativeProductAssociatorProvenance report_;
};
inline DerivedNativeAssociatedField PreparedNativeProductAssociator::field(algebra::ProductAssociation a, core::Grade g, Values values,
    NativeProductAssociatorAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_associator_detail; const auto pin = state_; const auto n = values.size(); detail::admit(*pin, available, lease, n);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.field(a, g, std::move(values), guard); lease.check_cancelled();
    return DerivedNativeAssociatedField(PreparedNativeProductAssociator(pin), std::move(out), detail::report(*pin, NativeProductAssociatorAction::DeclaredField, available, lease, guard, n));
}
inline DerivedNativeAssociatedField PreparedNativeProductAssociator::boundary(DerivedNativeAssociatedField input,
    NativeProductAssociatorAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_associator_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native associated boundary needs its actual owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.boundary(input.field_, guard); lease.check_cancelled();
    return DerivedNativeAssociatedField(PreparedNativeProductAssociator(pin), std::move(out), detail::report(*pin, NativeProductAssociatorAction::Boundary, available, lease, guard, n),
        std::make_shared<const DerivedNativeAssociatedField>(std::move(input)));
}
inline DerivedNativeAssociatedField PreparedNativeProductAssociator::reassociate(DerivedNativeAssociatedField input,
    NativeProductAssociatorAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_associator_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native associator needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.reassociate(input.field_, guard); lease.check_cancelled();
    return DerivedNativeAssociatedField(PreparedNativeProductAssociator(pin), std::move(out), detail::report(*pin, NativeProductAssociatorAction::Reassociate, available, lease, guard, n),
        std::make_shared<const DerivedNativeAssociatedField>(std::move(input)));
}
inline NativeProductAssociatorChainWitness PreparedNativeProductAssociator::witness(DerivedNativeAssociatedField input,
    NativeProductAssociatorAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_associator_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native chain witness needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.witness(input.field_, guard); lease.check_cancelled();
    return NativeProductAssociatorChainWitness(PreparedNativeProductAssociator(pin), std::move(input), std::move(out),
        detail::report(*pin, NativeProductAssociatorAction::ChainWitness, available, lease, guard, n));
}
inline NativeProductAssociatorCellWitness PreparedNativeProductAssociator::cell_witness(algebra::LeftAssociatedCell c,
    NativeProductAssociatorAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_associator_detail; const auto pin = state_; detail::admit(*pin, available, lease);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.cell_witness(c, guard); lease.check_cancelled();
    return NativeProductAssociatorCellWitness(PreparedNativeProductAssociator(pin), std::move(out), detail::report(*pin, NativeProductAssociatorAction::CellWitness, available, lease, guard));
}
} // namespace rfc::inference
