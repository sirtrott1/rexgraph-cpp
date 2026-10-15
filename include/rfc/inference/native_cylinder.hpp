#pragma once
#include "rfc/topology/owned_cylinder.hpp"
#include "rfc/inference/native_tensor_product.hpp"
#include "rfc/inference/native_gluing.hpp"

namespace rfc::inference {
using NativeCylinderAvailability = NativeProductAvailability;
struct NativeCylinderMapAvailability { NativeCylinderAvailability source, target; };
struct NativeCylinderMapRequest { std::uint64_t key{}; NativeGluingSourceMap source, interval; };
enum class NativeCylinderAction { DeclaredChain, NativeChain, SourceBoundary, DeclaredField, Start, Finish, Prism, Homotopy, Boundary, Witness, CylinderMap };
struct NativeCylinderProvenance {
    NativeCylinderAction action;
    std::uint64_t declaration{}, revision{}, source_factor{}, interval_factor{};
    std::uint64_t source_namespace{}, interval_namespace{}, source_sequence{}, interval_sequence{};
    NativeCylinderAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
namespace native_cylinder_detail {
using native_gluing_detail::Pin;
using Guard = numeric::ExactActionGuard;
using native_product_detail::requirements;
using topology::owned_cylinder_detail::add;
using topology::owned_cylinder_detail::mul;
using topology::owned_cylinder_detail::require;
struct State {
    NativeTensorProductRequest request;
    topology::OwnedRelationalCylinder cylinder;
    NativeCylinderAvailability preparation;
    std::size_t steps;
    std::string policy;
    bool fallback;
};
inline void available(const State& s, NativeCylinderAvailability a) {
    native_product_detail::available(s.request, s.preparation); native_product_detail::available(s.request, a);
}
inline numeric::ExactActionLimits limits(const State& s, const runtime::ExecutionLease& lease) {
    const std::array<Pin, 2> pins{s.request.left, s.request.right};
    return native_product_detail::limits(lease, pins, s.request.factor_dimension_limit);
}
inline NativeCylinderProvenance report(const State& s, NativeCylinderAction action, NativeCylinderAvailability a,
    const runtime::ExecutionLease& lease, const Guard& guard, runtime::ExecutionRequirements required) {
    const auto& r = s.request;
    return {action, r.declaration.key, r.declaration.revision, r.left_factor, r.right_factor,
        r.left.source_namespace(), r.right.source_namespace(), r.left.sequence(), r.right.sequence(), s.preparation, a,
        required, guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
inline runtime::ExecutionRequirements chain_requirements(const State& s, std::size_t values) {
    return requirements(s.cylinder.retained_words(), add(s.cylinder.entry_envelope(), mul(values, 4)));
}
}
class DerivedNativeCylinderChain;
class DerivedNativeCylinderField;
class NativeCylinderHomotopyWitness;
class PreparedNativeCylinderMap;
class PreparedNativeCylinder {
public:
    using Values = topology::OwnedRelationalCylinder::Values;
    using ProductValues = algebra::OwnedRelationalTensorProduct::Values;
    static PreparedNativeCylinder prepare(NativeTensorProductRequest input, NativeCylinderAvailability a,
        runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_cylinder_detail; auto request = input; lease.check_cancelled();
        native_product_detail::available(request, a);
        require(request.declaration.key && request.declaration.revision && request.factor_dimension_limit &&
            request.left_factor && request.right_factor && request.left_factor != request.right_factor, "native cylinder needs complete constructor and distinct factor identities");
        for (const auto* p : {&request.left, &request.right})
            require(p->kind() == storage::NativeAccessionKind::Chain && p->support_count() == 0, "native cylinder factors must be empty owning Chain anchors");
        const auto words = add(add(algebra::owned_product_detail::source_words(request.left.endpoint()->source),
            algebra::owned_product_detail::source_words(request.right.endpoint()->source)), mul(request.declaration.types.size(), 3));
        const auto envelope = add(1, mul(words, 32)); runtime::admit_execution(requirements(words, envelope), lease.policy());
        const std::array<Pin, 2> pins{request.left, request.right};
        Guard guard(native_product_detail::limits(lease, pins, request.factor_dimension_limit), [&lease]{ lease.check_cancelled(); }); guard.entries(envelope);
        auto product = algebra::OwnedRelationalTensorProduct::prepare(request.left.endpoint()->source, request.right.endpoint()->source, request.declaration, guard);
        auto cylinder = topology::OwnedRelationalCylinder::prepare(std::move(product), guard); lease.check_cancelled();
        const auto steps = guard.steps();
        return PreparedNativeCylinder(std::make_shared<const State>(State{std::move(request), std::move(cylinder), a,
            steps, lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    topology::OwnedRelationalCylinder numeric_plan() const { return state_->cylinder; }
    algebra::OwnedRelationalTensorProduct tensor_product() const { return state_->cylinder.product(); }
    const NativeTensorProductRequest& declaration() const & { return state_->request; }
    const NativeTensorProductRequest& declaration() const && = delete;
    storage::PinnedNativeAccession factor(bool interval) const { return interval ? state_->request.right : state_->request.left; }
    NativeCylinderAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeCylinder& other) const { return state_ == other.state_; }
    DerivedNativeCylinderChain chain(core::Grade, Values, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeCylinderChain chain(storage::PinnedNativeAccession, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeCylinderChain source_boundary(DerivedNativeCylinderChain, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeCylinderField field(core::Grade, ProductValues, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeCylinderField endpoint(DerivedNativeCylinderChain, topology::CylinderEndpoint, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeCylinderField prism(DerivedNativeCylinderChain, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeCylinderField homotopy(DerivedNativeCylinderChain, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    DerivedNativeCylinderField boundary(DerivedNativeCylinderField, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
    NativeCylinderHomotopyWitness homotopy_witness(DerivedNativeCylinderChain, NativeCylinderAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeCylinderChain; friend class DerivedNativeCylinderField; friend class PreparedNativeCylinderMap;
    explicit PreparedNativeCylinder(std::shared_ptr<const native_cylinder_detail::State> s) : state_(std::move(s)) {}
    DerivedNativeCylinderField embed(DerivedNativeCylinderChain, NativeCylinderAction, NativeCylinderAvailability, runtime::ExecutionLease) const;
    std::shared_ptr<const native_cylinder_detail::State> state_;
};
class DerivedNativeCylinderChain {
public:
    PreparedNativeCylinder plan() const { return owner_; }
    const topology::OwnedCylinderSourceChain& chain() const & { return chain_; }
    const topology::OwnedCylinderSourceChain& chain() const && = delete;
    const NativeCylinderProvenance& provenance() const & { return report_; }
    const NativeCylinderProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeCylinderChain> input() const { return input_; }
    std::optional<storage::PinnedNativeAccession> native_input() const { return native_; }
private:
    friend class PreparedNativeCylinder;
    DerivedNativeCylinderChain(PreparedNativeCylinder owner, topology::OwnedCylinderSourceChain chain, NativeCylinderProvenance report,
        std::shared_ptr<const DerivedNativeCylinderChain> input = {}, std::optional<storage::PinnedNativeAccession> native = {})
        : owner_(std::move(owner)), chain_(std::move(chain)), report_(std::move(report)), input_(std::move(input)), native_(std::move(native)) {}
    PreparedNativeCylinder owner_; topology::OwnedCylinderSourceChain chain_; NativeCylinderProvenance report_;
    std::shared_ptr<const DerivedNativeCylinderChain> input_; std::optional<storage::PinnedNativeAccession> native_;
};
class DerivedNativeCylinderField {
public:
    PreparedNativeCylinder plan() const { return owner_; }
    const algebra::OwnedProductField& field() const & { return field_; }
    const algebra::OwnedProductField& field() const && = delete;
    const NativeCylinderProvenance& provenance() const & { return report_; }
    const NativeCylinderProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeCylinderChain> source_input() const { return source_; }
    std::shared_ptr<const DerivedNativeCylinderField> input() const { return input_; }
private:
    friend class PreparedNativeCylinder; friend class PreparedNativeCylinderMap;
    DerivedNativeCylinderField(PreparedNativeCylinder owner, algebra::OwnedProductField field, NativeCylinderProvenance report,
        std::shared_ptr<const DerivedNativeCylinderChain> source = {}, std::shared_ptr<const DerivedNativeCylinderField> input = {})
        : owner_(std::move(owner)), field_(std::move(field)), report_(std::move(report)), source_(std::move(source)), input_(std::move(input)) {}
    PreparedNativeCylinder owner_; algebra::OwnedProductField field_; NativeCylinderProvenance report_;
    std::shared_ptr<const DerivedNativeCylinderChain> source_; std::shared_ptr<const DerivedNativeCylinderField> input_;
};
inline DerivedNativeCylinderChain PreparedNativeCylinder::chain(core::Grade g, Values input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_cylinder_detail; const auto pin = state_; auto captured = input; lease.check_cancelled(); available(*pin, a);
    const auto required = chain_requirements(*pin, captured.size()); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); auto output = pin->cylinder.chain(g, std::move(captured), guard); lease.check_cancelled();
    return {PreparedNativeCylinder(pin), std::move(output), report(*pin, NativeCylinderAction::DeclaredChain, a, lease, guard, required)};
}
inline DerivedNativeCylinderChain PreparedNativeCylinder::chain(storage::PinnedNativeAccession input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_cylinder_detail; const auto pin = state_; lease.check_cancelled(); available(*pin, a);
    require(input.kind() == storage::NativeAccessionKind::Chain && input.endpoint() == pin->request.left.endpoint() && input.same_execution_authority(pin->request.left),
        "cylinder Chain needs its actual source publication and execution authority");
    const auto required = chain_requirements(*pin, input.support_count()); runtime::admit_execution(required, lease.policy());
    const std::array<Pin, 3> pins{pin->request.left, pin->request.right, input};
    Guard guard(native_product_detail::limits(lease, pins, pin->request.factor_dimension_limit), [&lease]{ lease.check_cancelled(); });
    pin->cylinder.validate(guard); const auto descriptor = input.descriptor(); const auto g = descriptor.request().grade;
    storage::StableSourceIndex index(pin->request.left.endpoint()->source); Values values;
    input.for_each_cell([&](auto key) {
        guard.step(); const auto cell = index.cell({g, key}); const auto q = input.state(key); require(cell && q, "cylinder native support changed");
        guard.value(*q); values.emplace(cell->index, *q);
    });
    auto out = pin->cylinder.chain(g, std::move(values), guard); lease.check_cancelled();
    return {PreparedNativeCylinder(pin), std::move(out), report(*pin, NativeCylinderAction::NativeChain, a, lease, guard, required), {}, std::move(input)};
}
inline DerivedNativeCylinderChain PreparedNativeCylinder::source_boundary(DerivedNativeCylinderChain input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_cylinder_detail; const auto pin = state_; lease.check_cancelled(); available(*pin, a);
    require(input.owner_.state_ == pin, "native cylinder source boundary needs its actual owner");
    const auto required = chain_requirements(*pin, input.chain_.values().size()); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); auto out = pin->cylinder.source_boundary(input.chain_, guard); lease.check_cancelled();
    return {PreparedNativeCylinder(pin), std::move(out), report(*pin, NativeCylinderAction::SourceBoundary, a, lease, guard, required), std::make_shared<const DerivedNativeCylinderChain>(std::move(input))};
}
inline DerivedNativeCylinderField PreparedNativeCylinder::field(core::Grade g, ProductValues input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_cylinder_detail; const auto pin = state_; auto captured = input; lease.check_cancelled(); available(*pin, a);
    const auto required = chain_requirements(*pin, captured.size()); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); auto out = pin->cylinder.product().field(g, std::move(captured), guard); lease.check_cancelled();
    return {PreparedNativeCylinder(pin), std::move(out), report(*pin, NativeCylinderAction::DeclaredField, a, lease, guard, required)};
}
inline DerivedNativeCylinderField PreparedNativeCylinder::embed(DerivedNativeCylinderChain input, NativeCylinderAction action, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_cylinder_detail; const auto pin = state_; lease.check_cancelled(); available(*pin, a); require(input.owner_.state_ == pin, "native cylinder embedding needs its actual source chain");
    const auto required = chain_requirements(*pin, input.chain_.values().size()); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); });
    auto out = [&] {
        if (action == NativeCylinderAction::Start || action == NativeCylinderAction::Finish)
            return pin->cylinder.endpoint(input.chain_, action == NativeCylinderAction::Start ? topology::CylinderEndpoint::Start : topology::CylinderEndpoint::Finish, guard);
        if (action == NativeCylinderAction::Prism) return pin->cylinder.prism(input.chain_, guard);
        require(action == NativeCylinderAction::Homotopy, "unknown cylinder embedding"); return pin->cylinder.homotopy(input.chain_, guard);
    }(); lease.check_cancelled();
    return {PreparedNativeCylinder(pin), std::move(out), report(*pin, action, a, lease, guard, required), std::make_shared<const DerivedNativeCylinderChain>(std::move(input))};
}
inline DerivedNativeCylinderField PreparedNativeCylinder::endpoint(DerivedNativeCylinderChain input, topology::CylinderEndpoint endpoint, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    native_cylinder_detail::require(endpoint == topology::CylinderEndpoint::Start || endpoint == topology::CylinderEndpoint::Finish, "unknown native cylinder endpoint");
    return embed(std::move(input), endpoint == topology::CylinderEndpoint::Start ? NativeCylinderAction::Start : NativeCylinderAction::Finish, a, std::move(lease));
}
inline DerivedNativeCylinderField PreparedNativeCylinder::prism(DerivedNativeCylinderChain input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const { return embed(std::move(input), NativeCylinderAction::Prism, a, std::move(lease)); }
inline DerivedNativeCylinderField PreparedNativeCylinder::homotopy(DerivedNativeCylinderChain input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const { return embed(std::move(input), NativeCylinderAction::Homotopy, a, std::move(lease)); }
inline DerivedNativeCylinderField PreparedNativeCylinder::boundary(DerivedNativeCylinderField input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_cylinder_detail; const auto pin = state_; lease.check_cancelled(); available(*pin, a); require(input.owner_.state_ == pin, "native cylinder boundary needs its actual field owner");
    const auto required = chain_requirements(*pin, input.field_.values().size()); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); auto out = pin->cylinder.boundary(input.field_, guard); lease.check_cancelled();
    return {PreparedNativeCylinder(pin), std::move(out), report(*pin, NativeCylinderAction::Boundary, a, lease, guard, required), {}, std::make_shared<const DerivedNativeCylinderField>(std::move(input))};
}
class NativeCylinderHomotopyWitness {
public:
    std::shared_ptr<const DerivedNativeCylinderChain> input() const { return input_; }
    const topology::CylinderHomotopyWitness& witness() const & { return witness_; }
    const topology::CylinderHomotopyWitness& witness() const && = delete;
    const NativeCylinderProvenance& provenance() const & { return report_; }
    const NativeCylinderProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeCylinder;
    NativeCylinderHomotopyWitness(std::shared_ptr<const DerivedNativeCylinderChain> input, topology::CylinderHomotopyWitness witness, NativeCylinderProvenance report)
        : input_(std::move(input)), witness_(std::move(witness)), report_(std::move(report)) {}
    std::shared_ptr<const DerivedNativeCylinderChain> input_; topology::CylinderHomotopyWitness witness_; NativeCylinderProvenance report_;
};
inline NativeCylinderHomotopyWitness PreparedNativeCylinder::homotopy_witness(DerivedNativeCylinderChain input, NativeCylinderAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_cylinder_detail; const auto pin = state_; lease.check_cancelled(); available(*pin, a); require(input.owner_.state_ == pin, "native cylinder witness needs its actual source chain");
    const auto required = requirements(pin->cylinder.retained_words(), add(pin->cylinder.entry_envelope(), mul(topology::owned_cylinder_detail::count(pin->cylinder.product()), 64)));
    runtime::admit_execution(required, lease.policy()); Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); });
    auto witness = pin->cylinder.homotopy_witness(input.chain_, guard); lease.check_cancelled();
    return {std::make_shared<const DerivedNativeCylinderChain>(std::move(input)), std::move(witness), report(*pin, NativeCylinderAction::Witness, a, lease, guard, required)};
}
namespace native_cylinder_detail {
struct MapState {
    PreparedNativeCylinder source, target;
    NativeCylinderMapRequest request;
    topology::OwnedRelationalCylinderMap map;
    NativeCylinderMapAvailability preparation;
    std::size_t steps;
    std::string policy;
    bool fallback;
};
}
class MappedNativeCylinderField {
public:
    PreparedNativeCylinderMap plan() const;
    const algebra::OwnedProductField& field() const & { return field_; }
    const algebra::OwnedProductField& field() const && = delete;
    std::shared_ptr<const DerivedNativeCylinderField> input() const { return input_; }
    NativeCylinderMapAvailability execution_availability() const { return availability_; }
    const NativeCylinderProvenance& provenance() const & { return report_; }
    const NativeCylinderProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeCylinderMap;
    MappedNativeCylinderField(std::shared_ptr<const native_cylinder_detail::MapState> owner, algebra::OwnedProductField field,
        std::shared_ptr<const DerivedNativeCylinderField> input, NativeCylinderMapAvailability a, NativeCylinderProvenance report)
        : owner_(std::move(owner)), field_(std::move(field)), input_(std::move(input)), availability_(a), report_(std::move(report)) {}
    std::shared_ptr<const native_cylinder_detail::MapState> owner_; algebra::OwnedProductField field_;
    std::shared_ptr<const DerivedNativeCylinderField> input_; NativeCylinderMapAvailability availability_; NativeCylinderProvenance report_;
};
class PreparedNativeCylinderMap {
public:
    static PreparedNativeCylinderMap prepare(PreparedNativeCylinder source, PreparedNativeCylinder target, NativeCylinderMapRequest input,
        NativeCylinderMapAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_cylinder_detail; auto request = input; lease.check_cancelled(); available(*source.state_, a.source); available(*target.state_, a.target);
        require(request.key && request.source.key && request.interval.key && request.source.from == source.declaration().left_factor &&
            request.source.to == target.declaration().left_factor && request.interval.from == source.declaration().right_factor && request.interval.to == target.declaration().right_factor,
            "cylinder map needs all declared factor identities");
        const auto words = add(source.numeric_plan().retained_words(), add(target.numeric_plan().retained_words(),
            add(add(request.source.cells.size(), request.source.occurrences.size()), add(request.interval.cells.size(), request.interval.occurrences.size()))));
        const auto envelope = add(1, mul(words, 64)); runtime::admit_execution(requirements(words, envelope), lease.policy());
        const std::array<Pin, 4> pins{source.factor(false), source.factor(true), target.factor(false), target.factor(true)};
        Guard guard(native_product_detail::limits(lease, pins, std::min(source.declaration().factor_dimension_limit, target.declaration().factor_dimension_limit)), [&lease]{ lease.check_cancelled(); }); guard.entries(envelope);
        source.numeric_plan().validate(guard); target.numeric_plan().validate(guard);
        auto left = native_gluing_detail::indexed(source.factor(false).endpoint()->source, target.factor(false).endpoint()->source, request.source.cells, request.source.occurrences, guard);
        auto right = native_gluing_detail::indexed(source.factor(true).endpoint()->source, target.factor(true).endpoint()->source, request.interval.cells, request.interval.occurrences, guard);
        auto map = topology::OwnedRelationalCylinderMap::prepare(source.numeric_plan(), target.numeric_plan(), std::move(left), std::move(right), guard);
        runtime::admit_execution(requirements(words, add(envelope, map.entry_envelope())), lease.policy()); lease.check_cancelled();
        return PreparedNativeCylinderMap(std::make_shared<const MapState>(MapState{std::move(source), std::move(target), std::move(request), std::move(map), a,
            guard.steps(), lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeCylinder source() const { return state_->source; }
    PreparedNativeCylinder target() const { return state_->target; }
    const NativeCylinderMapRequest& declaration() const & { return state_->request; }
    const NativeCylinderMapRequest& declaration() const && = delete;
    topology::OwnedRelationalCylinderMap numeric_plan() const { return state_->map; }
    NativeCylinderMapAvailability preparation_availability() const { return state_->preparation; }
    MappedNativeCylinderField execute(DerivedNativeCylinderField input, NativeCylinderMapAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        using namespace native_cylinder_detail; const auto pin = state_; lease.check_cancelled(); require(pin->source.same_owner(input.owner_), "native cylinder map needs its actual input owner");
        available(*pin->source.state_, pin->preparation.source); available(*pin->target.state_, pin->preparation.target);
        available(*pin->source.state_, a.source); available(*pin->target.state_, a.target);
        const auto required = requirements(pin->map.retained_words(), add(pin->map.entry_envelope(), mul(input.field_.values().size(), 4))); runtime::admit_execution(required, lease.policy());
        const std::array<Pin, 4> pins{pin->source.factor(false), pin->source.factor(true), pin->target.factor(false), pin->target.factor(true)};
        Guard guard(native_product_detail::limits(lease, pins, std::min(pin->source.declaration().factor_dimension_limit, pin->target.declaration().factor_dimension_limit)), [&lease]{ lease.check_cancelled(); });
        auto out = pin->map.apply(input.field_, guard); lease.check_cancelled();
        auto receipt = report(*pin->source.state_, NativeCylinderAction::CylinderMap, a.source, lease, guard, required);
        receipt.preparation_availability = pin->preparation.source; receipt.preparation_steps = pin->steps; receipt.preparation_policy = pin->policy;
        receipt.fallback = receipt.fallback || pin->fallback || pin->target.state_->fallback;
        return {pin, std::move(out), std::make_shared<const DerivedNativeCylinderField>(std::move(input)), a, std::move(receipt)};
    }
private:
    friend class MappedNativeCylinderField;
    explicit PreparedNativeCylinderMap(std::shared_ptr<const native_cylinder_detail::MapState> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_cylinder_detail::MapState> state_;
};
inline PreparedNativeCylinderMap MappedNativeCylinderField::plan() const { return PreparedNativeCylinderMap(owner_); }
} // namespace rfc::inference
