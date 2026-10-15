#pragma once
#include "rfc/algebra/owned_product_interchange.hpp"
#include "rfc/inference/native_tensor_product.hpp"

namespace rfc::inference {
using NativeProductInterchangeAvailability = std::array<runtime::WorkAvailability, 2>;
struct NativeProductInterchangeRequest {
    std::array<storage::PinnedNativeAccession, 2> factors;
    std::array<std::uint64_t, 2> factor_identities;
    algebra::ProductInterchangeDeclaration declaration;
    std::size_t dimension_limit{64};
};
enum class NativeProductInterchangeAction { DeclaredField, Boundary, Interchange, ChainWitness, CellWitness };
struct NativeProductInterchangeProvenance {
    NativeProductInterchangeAction action{NativeProductInterchangeAction::DeclaredField};
    std::uint64_t declaration{}, revision{};
    std::array<std::uint64_t, 2> factors, namespaces, sequences;
    NativeProductInterchangeAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
    algebra::ProductFactorOrder source_order, target_order;
};
namespace native_interchange_detail {
using algebra::product_interchange_detail::add;
using algebra::product_interchange_detail::mul;
using algebra::product_interchange_detail::require;
using native_product_detail::Guard;
inline void available(const NativeProductInterchangeRequest& r, NativeProductInterchangeAvailability a) {
    for (std::size_t f = 0; f < 2; ++f) runtime::admit_work_availability(r.factors[f].source_namespace(), r.factors[f].sequence(), a[f]);
}
struct State {
    NativeProductInterchangeRequest request;
    algebra::OwnedProductInterchange numeric;
    NativeProductInterchangeAvailability preparation;
    std::size_t steps{};
    std::string policy;
    bool fallback{};
};
inline runtime::ExecutionRequirements requirements(const State& s, std::size_t support = 0) {
    return native_product_detail::requirements(s.numeric.retained_words(), add(mul(s.numeric.entry_envelope(), 2), mul(support, 128)));
}
inline void admit(const State& s, NativeProductInterchangeAvailability a, const runtime::ExecutionLease& lease, std::size_t support = 0) {
    lease.check_cancelled(); available(s.request, s.preparation); available(s.request, a);
    runtime::admit_execution(requirements(s, support), lease.policy());
}
inline NativeProductInterchangeProvenance report(const State& s, NativeProductInterchangeAction action,
    NativeProductInterchangeAvailability a, const runtime::ExecutionLease& lease, Guard& guard, algebra::ProductFactorOrder source_order, algebra::ProductFactorOrder target_order, std::size_t support = 0) {
    std::array<std::uint64_t, 2> namespaces{}, sequences{};
    for (std::size_t f = 0; f < 2; ++f) { namespaces[f] = s.request.factors[f].source_namespace(); sequences[f] = s.request.factors[f].sequence(); }
    return {action, s.request.declaration.key, s.request.declaration.revision, s.request.factor_identities, namespaces, sequences,
        s.preparation, a, requirements(s, support), guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu, source_order, target_order};
}
} // namespace native_interchange_detail
class DerivedNativeInterchangeField;
class NativeProductInterchangeChainWitness;
class NativeProductInterchangeCellWitness;
class PreparedNativeProductInterchange {
public:
    using Values = algebra::OwnedProductInterchange::Values;
    static PreparedNativeProductInterchange prepare(NativeProductInterchangeRequest input, NativeProductInterchangeAvailability a,
        runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        namespace detail = native_interchange_detail;
        auto r = input;
        lease.check_cancelled(); detail::available(r, a);
        detail::require(r.dimension_limit > 0, "native interchange requires a positive dimension limit");
        for (std::size_t f = 0; f < 2; ++f) {
            detail::require(r.factor_identities[f] && r.factors[f].kind() == storage::NativeAccessionKind::Chain && r.factors[f].support_count() == 0,
                "native interchange requires named empty owning Chain anchors");
            for (std::size_t j = 0; j < f; ++j) if (r.factor_identities[j] == r.factor_identities[f])
                detail::require(r.factors[j].endpoint() == r.factors[f].endpoint() && r.factors[j].same_execution_authority(r.factors[f]),
                    "native interchange aliases different publications or execution authorities");
        }
        const std::array<const core::RelationalComplex*, 2> sources{&r.factors[0].endpoint()->source, &r.factors[1].endpoint()->source};
        const auto words = algebra::product_interchange_detail::words(sources, r.declaration), envelope = detail::add(1, detail::mul(words, 16));
        runtime::admit_execution(native_product_detail::requirements(words, detail::mul(envelope,2)), lease.policy());
        detail::Guard guard(native_product_detail::limits(lease, r.factors, r.dimension_limit), [&lease]{ lease.check_cancelled(); });
        auto numeric = algebra::OwnedProductInterchange::prepare({*sources[0], *sources[1]}, r.declaration, guard); lease.check_cancelled();
        return PreparedNativeProductInterchange(std::make_shared<const native_interchange_detail::State>(native_interchange_detail::State{
            std::move(r), std::move(numeric), a, guard.steps(), lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    algebra::OwnedProductInterchange numeric_plan() const { return state_->numeric; }
    storage::PinnedNativeAccession factor(std::size_t f) const { return state_->request.factors.at(f); }
    const NativeProductInterchangeRequest& declaration() const & { return state_->request; }
    const NativeProductInterchangeRequest& declaration() const && = delete;
    NativeProductInterchangeAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeProductInterchange& other) const noexcept { return state_ == other.state_; }
    DerivedNativeInterchangeField field(algebra::ProductFactorOrder, core::Grade, Values, NativeProductInterchangeAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeInterchangeField boundary(DerivedNativeInterchangeField, NativeProductInterchangeAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeInterchangeField interchange(DerivedNativeInterchangeField, NativeProductInterchangeAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductInterchangeChainWitness witness(DerivedNativeInterchangeField, NativeProductInterchangeAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeProductInterchangeCellWitness cell_witness(algebra::ProductFactorOrder, algebra::ProductCell, NativeProductInterchangeAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeInterchangeField;
    explicit PreparedNativeProductInterchange(std::shared_ptr<const native_interchange_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_interchange_detail::State> state_;
};
class DerivedNativeInterchangeField {
public:
    PreparedNativeProductInterchange plan() const { return plan_; }
    const algebra::OwnedProductField& field() const & { return field_; }
    const algebra::OwnedProductField& field() const && = delete;
    const NativeProductInterchangeProvenance& provenance() const & { return report_; }
    const NativeProductInterchangeProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeInterchangeField> input() const { return input_; }
private:
    friend class PreparedNativeProductInterchange;
    DerivedNativeInterchangeField(PreparedNativeProductInterchange p, algebra::OwnedProductField f, NativeProductInterchangeProvenance r,
        std::shared_ptr<const DerivedNativeInterchangeField> input = {}) : plan_(std::move(p)), field_(std::move(f)), report_(std::move(r)), input_(std::move(input)) {}
    PreparedNativeProductInterchange plan_;
    algebra::OwnedProductField field_;
    NativeProductInterchangeProvenance report_;
    std::shared_ptr<const DerivedNativeInterchangeField> input_;
};
class NativeProductInterchangeChainWitness {
public:
    PreparedNativeProductInterchange plan() const { return plan_; }
    std::shared_ptr<const DerivedNativeInterchangeField> input() const { return input_; }
    const algebra::ProductInterchangeChainWitness& witness() const & { return witness_; }
    const algebra::ProductInterchangeChainWitness& witness() const && = delete;
    const NativeProductInterchangeProvenance& provenance() const & { return report_; }
    const NativeProductInterchangeProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductInterchange;
    NativeProductInterchangeChainWitness(PreparedNativeProductInterchange p, DerivedNativeInterchangeField input,
        algebra::ProductInterchangeChainWitness w, NativeProductInterchangeProvenance r)
        : plan_(std::move(p)), input_(std::make_shared<const DerivedNativeInterchangeField>(std::move(input))), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductInterchange plan_;
    std::shared_ptr<const DerivedNativeInterchangeField> input_;
    algebra::ProductInterchangeChainWitness witness_;
    NativeProductInterchangeProvenance report_;
};
class NativeProductInterchangeCellWitness {
public:
    PreparedNativeProductInterchange plan() const { return plan_; }
    const algebra::ProductInterchangeCellWitness& witness() const & { return witness_; }
    const algebra::ProductInterchangeCellWitness& witness() const && = delete;
    const NativeProductInterchangeProvenance& provenance() const & { return report_; }
    const NativeProductInterchangeProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductInterchange;
    NativeProductInterchangeCellWitness(PreparedNativeProductInterchange p, algebra::ProductInterchangeCellWitness w, NativeProductInterchangeProvenance r)
        : plan_(std::move(p)), witness_(std::move(w)), report_(std::move(r)) {}
    PreparedNativeProductInterchange plan_;
    algebra::ProductInterchangeCellWitness witness_;
    NativeProductInterchangeProvenance report_;
};
inline DerivedNativeInterchangeField PreparedNativeProductInterchange::field(algebra::ProductFactorOrder a, core::Grade g, Values values,
    NativeProductInterchangeAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_interchange_detail; const auto pin = state_; auto frozen = values; const auto n = frozen.size(); detail::admit(*pin, available, lease, n);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.field(a, g, std::move(frozen), guard); lease.check_cancelled();
    return DerivedNativeInterchangeField(PreparedNativeProductInterchange(pin), std::move(out), detail::report(*pin, NativeProductInterchangeAction::DeclaredField, available, lease, guard, a, a, n));
}
inline DerivedNativeInterchangeField PreparedNativeProductInterchange::boundary(DerivedNativeInterchangeField input,
    NativeProductInterchangeAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_interchange_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native interchange boundary needs its actual owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.boundary(input.field_, guard); lease.check_cancelled();
    auto report = detail::report(*pin, NativeProductInterchangeAction::Boundary, available, lease, guard, pin->numeric.order(input.field_), pin->numeric.order(input.field_), n);
    return DerivedNativeInterchangeField(PreparedNativeProductInterchange(pin), std::move(out), std::move(report),
        std::make_shared<const DerivedNativeInterchangeField>(std::move(input)));
}
inline DerivedNativeInterchangeField PreparedNativeProductInterchange::interchange(DerivedNativeInterchangeField input,
    NativeProductInterchangeAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_interchange_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native interchange needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.interchange(input.field_, guard); lease.check_cancelled();
    auto report = detail::report(*pin, NativeProductInterchangeAction::Interchange, available, lease, guard, pin->numeric.order(input.field_), algebra::product_interchange_detail::opposite(pin->numeric.order(input.field_)), n);
    return DerivedNativeInterchangeField(PreparedNativeProductInterchange(pin), std::move(out), std::move(report),
        std::make_shared<const DerivedNativeInterchangeField>(std::move(input)));
}
inline NativeProductInterchangeChainWitness PreparedNativeProductInterchange::witness(DerivedNativeInterchangeField input,
    NativeProductInterchangeAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_interchange_detail; const auto pin = state_; const auto n = input.field_.values().size();
    detail::admit(*pin, available, lease, n); detail::require(pin == input.plan_.state_, "native chain witness needs its actual field owner");
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.witness(input.field_, guard); lease.check_cancelled();
    auto report = detail::report(*pin, NativeProductInterchangeAction::ChainWitness, available, lease, guard, pin->numeric.order(input.field_), algebra::product_interchange_detail::opposite(pin->numeric.order(input.field_)), n);
    return NativeProductInterchangeChainWitness(PreparedNativeProductInterchange(pin), std::move(input), std::move(out), std::move(report));
}
inline NativeProductInterchangeCellWitness PreparedNativeProductInterchange::cell_witness(algebra::ProductFactorOrder order, algebra::ProductCell c,
    NativeProductInterchangeAvailability available, runtime::ExecutionLease lease) const {
    namespace detail = native_interchange_detail; const auto pin = state_; detail::admit(*pin, available, lease);
    detail::Guard guard(native_product_detail::limits(lease, pin->request.factors, pin->request.dimension_limit), [&lease]{ lease.check_cancelled(); });
    auto out = pin->numeric.cell_witness(order, c, guard); lease.check_cancelled();
    return NativeProductInterchangeCellWitness(PreparedNativeProductInterchange(pin), std::move(out), detail::report(*pin, NativeProductInterchangeAction::CellWitness, available, lease, guard, order, algebra::product_interchange_detail::opposite(order)));
}
} // namespace rfc::inference
