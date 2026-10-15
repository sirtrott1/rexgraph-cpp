#pragma once
#include "rfc/topology/owned_relational_homotopy.hpp"
#include "rfc/inference/native_cylinder.hpp"

namespace rfc::inference {
struct NativeCylinderCellImage { algebra::ProductCellIdentity from; storage::StableCellKey to; };
struct NativeCylinderFlagImage {
    algebra::ProductCellIdentity upper;
    algebra::ProductBoundarySide side;
    core::OccurrenceKey occurrence;
    storage::StableOccurrenceKey to;
};
struct NativeCylinderCarrierDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<topology::CylinderTypeInterpretation> types;
    std::vector<NativeCylinderCellImage> cells;
    std::vector<NativeCylinderFlagImage> occurrences;
};
struct NativeRelationalHomotopyRequest {
    std::uint64_t key{}, revision{}, carrier_source{}, target_source{};
    storage::PinnedNativeAccession carrier, target;
    NativeCylinderCarrierDeclaration realization;
    NativeGluingSourceMap cylinder_target, first, second;
};
struct NativeRelationalHomotopyAvailability {
    NativeCylinderAvailability cylinder;
    runtime::WorkAvailability carrier, target;
};
enum class NativeRelationalHomotopyAction { Push, Start, Finish, Homotopy, Boundary, Witness };
struct NativeRelationalHomotopyProvenance {
    NativeRelationalHomotopyAction action;
    std::uint64_t declaration{}, revision{}, carrier_declaration{}, carrier_revision{};
    std::array<std::uint64_t, 4> sources, namespaces, sequences;
    NativeRelationalHomotopyAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
namespace native_relational_homotopy_detail {
using native_cylinder_detail::Pin;
using native_cylinder_detail::Guard;
using native_cylinder_detail::add;
using native_cylinder_detail::mul;
using native_cylinder_detail::require;
using native_product_detail::requirements;
struct State {
    PreparedNativeCylinder cylinder;
    NativeRelationalHomotopyRequest request;
    topology::OwnedRelationalHomotopy homotopy;
    NativeRelationalHomotopyAvailability preparation;
    std::size_t steps{}, retained{}, envelope{};
    std::string policy;
    bool fallback{};
};
inline std::array<Pin, 4> pins(const PreparedNativeCylinder& cylinder, const NativeRelationalHomotopyRequest& r) {
    return {cylinder.factor(false), cylinder.factor(true), r.carrier, r.target};
}
inline void available(const PreparedNativeCylinder& cylinder, const NativeRelationalHomotopyRequest& r,
    NativeRelationalHomotopyAvailability a) {
    native_product_detail::available(cylinder.declaration(), cylinder.preparation_availability());
    native_product_detail::available(cylinder.declaration(), a.cylinder);
    runtime::admit_work_availability(r.carrier.source_namespace(), r.carrier.sequence(), a.carrier);
    runtime::admit_work_availability(r.target.source_namespace(), r.target.sequence(), a.target);
}
inline std::size_t words(const PreparedNativeCylinder& cylinder, const NativeRelationalHomotopyRequest& r) {
    auto n = add(cylinder.numeric_plan().retained_words(), add(algebra::owned_gluing_detail::words(r.carrier.endpoint()->source),
        algebra::owned_gluing_detail::words(r.target.endpoint()->source)));
    n = add(n, add(12, add(mul(r.realization.types.size(), 2), add(mul(r.realization.cells.size(), 6), mul(r.realization.occurrences.size(), 8)))));
    for (const auto* m : {&r.cylinder_target, &r.first, &r.second}) n = add(n, add(3, add(mul(m->cells.size(), 4), mul(m->occurrences.size(), 4))));
    return n;
}
inline algebra::ProductCell product_cell(const storage::StableSourceIndex& left, const storage::StableSourceIndex& right,
    algebra::ProductCellIdentity id) {
    const auto a = left.cell({id.left.grade, id.left.key}), b = right.cell({id.right.grade, id.right.key});
    require(a && b, "native cylinder carrier names a foreign factor identity"); return {*a, *b};
}
inline topology::CylinderCarrierDeclaration indexed(const PreparedNativeCylinder& cylinder, const NativeRelationalHomotopyRequest& r, Guard& guard) {
    const auto left = cylinder.factor(false), right = cylinder.factor(true);
    storage::StableSourceIndex a(left.endpoint()->source), b(right.endpoint()->source), c(r.carrier.endpoint()->source);
    topology::CylinderCarrierDeclaration out{r.realization.key, r.realization.revision, r.realization.types, {}, {}};
    for (const auto& x : r.realization.cells) {
        guard.step(); const auto from = product_cell(a, b, x.from); const auto to = c.cell(x.to);
        require(to && to->grade == from.grade(), "native cylinder carrier cell changes grade or names absent data"); out.cells.push_back({from, to->index});
    }
    for (const auto& x : r.realization.occurrences) {
        guard.step(); const auto from = product_cell(a, b, x.upper); const auto to = c.occurrence(x.to);
        require(to && x.to.upper_grade == from.grade(), "native cylinder carrier flag changes grade or names absent data");
        out.occurrences.push_back({{from, x.side, x.occurrence}, *to});
    }
    return out;
}
inline void validate(const State& s, NativeRelationalHomotopyAvailability a) {
    available(s.cylinder, s.request, s.preparation); available(s.cylinder, s.request, a);
}
inline NativeRelationalHomotopyProvenance report(const State& s, NativeRelationalHomotopyAction action,
    NativeRelationalHomotopyAvailability a, const runtime::ExecutionLease& lease, const Guard& guard, runtime::ExecutionRequirements required) {
    const auto p = pins(s.cylinder, s.request); const auto& r = s.request;
    return {action, r.key, r.revision, r.realization.key, r.realization.revision,
        {s.cylinder.declaration().left_factor, s.cylinder.declaration().right_factor, r.carrier_source, r.target_source},
        {p[0].source_namespace(), p[1].source_namespace(), p[2].source_namespace(), p[3].source_namespace()},
        {p[0].sequence(), p[1].sequence(), p[2].sequence(), p[3].sequence()}, s.preparation, a, required, guard.limits(),
        s.steps, guard.steps(), s.policy, lease.policy().name, s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
} // namespace native_relational_homotopy_detail
class PreparedNativeRelationalHomotopy;
class NativeRelationalHomotopyWitness;
class DerivedNativeHomotopyChain {
public:
    PreparedNativeRelationalHomotopy plan() const;
    const topology::OwnedHomotopyChain& chain() const & { return chain_; }
    const topology::OwnedHomotopyChain& chain() const && = delete;
    const NativeRelationalHomotopyProvenance& provenance() const & { return report_; }
    const NativeRelationalHomotopyProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeCylinderChain> source_input() const { return source_; }
    std::shared_ptr<const DerivedNativeCylinderField> product_input() const { return product_; }
    std::shared_ptr<const DerivedNativeHomotopyChain> input() const { return input_; }
private:
    friend class PreparedNativeRelationalHomotopy;
    DerivedNativeHomotopyChain(std::shared_ptr<const native_relational_homotopy_detail::State> owner, topology::OwnedHomotopyChain chain,
        NativeRelationalHomotopyProvenance report, std::shared_ptr<const DerivedNativeCylinderChain> source = {},
        std::shared_ptr<const DerivedNativeCylinderField> product = {}, std::shared_ptr<const DerivedNativeHomotopyChain> input = {})
        : owner_(std::move(owner)), chain_(std::move(chain)), report_(std::move(report)), source_(std::move(source)), product_(std::move(product)), input_(std::move(input)) {}
    std::shared_ptr<const native_relational_homotopy_detail::State> owner_; topology::OwnedHomotopyChain chain_; NativeRelationalHomotopyProvenance report_;
    std::shared_ptr<const DerivedNativeCylinderChain> source_; std::shared_ptr<const DerivedNativeCylinderField> product_; std::shared_ptr<const DerivedNativeHomotopyChain> input_;
};
class PreparedNativeRelationalHomotopy {
public:
    static PreparedNativeRelationalHomotopy prepare(PreparedNativeCylinder cylinder, NativeRelationalHomotopyRequest input,
        NativeRelationalHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_relational_homotopy_detail; auto request = input; lease.check_cancelled(); available(cylinder, request, a);
        require(request.key && request.revision && request.realization.key && request.realization.revision && request.carrier_source && request.target_source,
            "native homotopy needs complete declaration identities and revisions");
        const std::array<std::uint64_t, 4> identities{cylinder.declaration().left_factor, cylinder.declaration().right_factor, request.carrier_source, request.target_source};
        require(std::set<std::uint64_t>(identities.begin(), identities.end()).size() == identities.size(), "native homotopy requires separately named actual carriers");
        for (const auto* p : {&request.carrier, &request.target}) require(p->kind() == storage::NativeAccessionKind::Chain && !p->support_count(), "native homotopy carriers must be empty owning Chain anchors");
        for (const auto* m : {&request.cylinder_target, &request.first, &request.second}) require(m->key && m->to == request.target_source, "native homotopy maps need complete declared target identities");
        require(request.cylinder_target.from == request.carrier_source && request.first.from == identities[0] && request.second.from == identities[0], "native homotopy map names the wrong original source");
        require(request.cylinder_target.key != request.first.key && request.cylinder_target.key != request.second.key && request.first.key != request.second.key, "native homotopy maps need distinct identities");
        const auto n = words(cylinder, request), envelope = add(1, mul(n, 128)); runtime::admit_execution(requirements(n, envelope), lease.policy());
        const auto p = pins(cylinder, request);
        Guard guard(native_product_detail::limits(lease, p, cylinder.declaration().factor_dimension_limit), [&lease]{ lease.check_cancelled(); }); guard.entries(envelope);
        cylinder.numeric_plan().validate(guard);
        for (const auto* pin : {&request.carrier, &request.target}) algebra::owned_gluing_detail::source(pin->endpoint()->source, guard);
        auto realization = indexed(cylinder, request, guard);
        const auto map = [&](const NativeGluingSourceMap& m, const core::RelationalComplex& from) {
            return native_gluing_detail::indexed(from, request.target.endpoint()->source, m.cells, m.occurrences, guard);
        };
        auto h = map(request.cylinder_target, request.carrier.endpoint()->source);
        auto first = map(request.first, cylinder.factor(false).endpoint()->source), second = map(request.second, cylinder.factor(false).endpoint()->source);
        auto carrier = topology::OwnedCylinderCarrier::prepare(cylinder.numeric_plan(), request.carrier.endpoint()->source, std::move(realization), guard);
        auto homotopy = topology::OwnedRelationalHomotopy::prepare(std::move(carrier), request.target.endpoint()->source,
            {request.key, request.revision, std::move(h), std::move(first), std::move(second)}, guard);
        const auto complete_envelope = add(envelope, homotopy.entry_envelope());
        runtime::admit_execution(requirements(n, complete_envelope), lease.policy()); lease.check_cancelled();
        return PreparedNativeRelationalHomotopy(std::make_shared<const native_relational_homotopy_detail::State>(native_relational_homotopy_detail::State{
            std::move(cylinder), std::move(request), std::move(homotopy), a, guard.steps(), n, complete_envelope,
            lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeCylinder cylinder() const { return state_->cylinder; }
    topology::OwnedRelationalHomotopy numeric_plan() const { return state_->homotopy; }
    const NativeRelationalHomotopyRequest& declaration() const & { return state_->request; }
    const NativeRelationalHomotopyRequest& declaration() const && = delete;
    NativeRelationalHomotopyAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeRelationalHomotopy& other) const { return state_ == other.state_; }
    DerivedNativeHomotopyChain push(DerivedNativeCylinderField input, NativeRelationalHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        const auto pin = state_; native_relational_homotopy_detail::require(pin->cylinder.same_owner(input.plan()), "native homotopy push needs its actual cylinder field");
        return action(NativeRelationalHomotopyAction::Push, {}, std::move(input), {}, a, std::move(lease));
    }
    DerivedNativeHomotopyChain endpoint(DerivedNativeCylinderChain input, topology::CylinderEndpoint end,
        NativeRelationalHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        native_relational_homotopy_detail::require(end == topology::CylinderEndpoint::Start || end == topology::CylinderEndpoint::Finish, "unknown native homotopy endpoint");
        return action(end == topology::CylinderEndpoint::Start ? NativeRelationalHomotopyAction::Start : NativeRelationalHomotopyAction::Finish,
            std::move(input), {}, {}, a, std::move(lease));
    }
    DerivedNativeHomotopyChain homotopy(DerivedNativeCylinderChain input, NativeRelationalHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Homotopy, std::move(input), {}, {}, a, std::move(lease));
    }
    DerivedNativeHomotopyChain boundary(DerivedNativeHomotopyChain input, NativeRelationalHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Boundary, {}, {}, std::move(input), a, std::move(lease));
    }
    NativeRelationalHomotopyWitness witness(DerivedNativeCylinderChain, NativeRelationalHomotopyAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeHomotopyChain;
    explicit PreparedNativeRelationalHomotopy(std::shared_ptr<const native_relational_homotopy_detail::State> s) : state_(std::move(s)) {}
    DerivedNativeHomotopyChain action(NativeRelationalHomotopyAction, std::optional<DerivedNativeCylinderChain>,
        std::optional<DerivedNativeCylinderField>, std::optional<DerivedNativeHomotopyChain>, NativeRelationalHomotopyAvailability, runtime::ExecutionLease) const;
    std::shared_ptr<const native_relational_homotopy_detail::State> state_;
};
inline PreparedNativeRelationalHomotopy DerivedNativeHomotopyChain::plan() const { return PreparedNativeRelationalHomotopy(owner_); }
inline DerivedNativeHomotopyChain PreparedNativeRelationalHomotopy::action(NativeRelationalHomotopyAction which,
    std::optional<DerivedNativeCylinderChain> source, std::optional<DerivedNativeCylinderField> product,
    std::optional<DerivedNativeHomotopyChain> input, NativeRelationalHomotopyAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_relational_homotopy_detail; const auto pin = state_; lease.check_cancelled(); validate(*pin, a);
    if (source) require(pin->cylinder.same_owner(source->plan()), "native homotopy needs its actual source chain");
    if (product) require(pin->cylinder.same_owner(product->plan()), "native homotopy needs its actual product field");
    if (input) require(pin->homotopy.same_owner(input->chain().plan()), "native homotopy needs its actual target chain");
    const auto size = source ? source->chain().values().size() : product ? product->field().values().size() : input->chain().values().size();
    const auto required = requirements(pin->retained, add(pin->envelope, mul(size, 8))); runtime::admit_execution(required, lease.policy());
    const auto p = pins(pin->cylinder, pin->request); Guard guard(native_product_detail::limits(lease, p, pin->cylinder.declaration().factor_dimension_limit), [&lease]{ lease.check_cancelled(); });
    guard.entries(required.rational_slots);
    for (std::size_t n = 0; n < pin->retained; ++n) guard.step();
    auto out = [&] {
        if (which == NativeRelationalHomotopyAction::Push) return pin->homotopy.push(product->field(), guard);
        if (which == NativeRelationalHomotopyAction::Boundary) return pin->homotopy.boundary(input->chain(), guard);
        if (which == NativeRelationalHomotopyAction::Homotopy) return pin->homotopy.homotopy(source->chain(), guard);
        require(which == NativeRelationalHomotopyAction::Start || which == NativeRelationalHomotopyAction::Finish, "unknown homotopy action");
        return pin->homotopy.endpoint(source->chain(), which == NativeRelationalHomotopyAction::Start ? topology::CylinderEndpoint::Start : topology::CylinderEndpoint::Finish, guard);
    }(); lease.check_cancelled();
    return {pin, std::move(out), report(*pin, which, a, lease, guard, required),
        source ? std::make_shared<const DerivedNativeCylinderChain>(std::move(*source)) : nullptr,
        product ? std::make_shared<const DerivedNativeCylinderField>(std::move(*product)) : nullptr,
        input ? std::make_shared<const DerivedNativeHomotopyChain>(std::move(*input)) : nullptr};
}
class NativeRelationalHomotopyWitness {
public:
    PreparedNativeRelationalHomotopy plan() const { return input_plan_; }
    std::shared_ptr<const DerivedNativeCylinderChain> input() const { return input_; }
    const topology::RelationalHomotopyWitness& witness() const & { return witness_; }
    const topology::RelationalHomotopyWitness& witness() const && = delete;
    const NativeRelationalHomotopyProvenance& provenance() const & { return report_; }
    const NativeRelationalHomotopyProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeRelationalHomotopy;
    NativeRelationalHomotopyWitness(PreparedNativeRelationalHomotopy plan, std::shared_ptr<const DerivedNativeCylinderChain> input,
        topology::RelationalHomotopyWitness witness, NativeRelationalHomotopyProvenance report)
        : input_plan_(std::move(plan)), input_(std::move(input)), witness_(std::move(witness)), report_(std::move(report)) {}
    PreparedNativeRelationalHomotopy input_plan_; std::shared_ptr<const DerivedNativeCylinderChain> input_; topology::RelationalHomotopyWitness witness_; NativeRelationalHomotopyProvenance report_;
};
inline NativeRelationalHomotopyWitness PreparedNativeRelationalHomotopy::witness(DerivedNativeCylinderChain input,
    NativeRelationalHomotopyAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_relational_homotopy_detail; const auto pin = state_; lease.check_cancelled(); validate(*pin, a);
    require(pin->cylinder.same_owner(input.plan()), "native relational homotopy witness needs its actual source chain");
    const auto numeric = pin->homotopy; const auto carrier = numeric.carrier();
    const auto envelope = add(numeric.entry_envelope(), mul(add(topology::cylinder_carrier_detail::population(carrier.cylinder()), algebra::owned_gluing_detail::words(numeric.target())), 32));
    const auto required = requirements(pin->retained, add(pin->envelope, add(envelope, mul(input.chain().values().size(), 8)))); runtime::admit_execution(required, lease.policy());
    const auto p = pins(pin->cylinder, pin->request); Guard guard(native_product_detail::limits(lease, p, pin->cylinder.declaration().factor_dimension_limit), [&lease]{ lease.check_cancelled(); });
    guard.entries(required.rational_slots);
    for (std::size_t n = 0; n < pin->retained; ++n) guard.step();
    auto out = numeric.witness(input.chain(), guard); lease.check_cancelled();
    return {PreparedNativeRelationalHomotopy(pin), std::make_shared<const DerivedNativeCylinderChain>(std::move(input)), std::move(out), report(*pin, NativeRelationalHomotopyAction::Witness, a, lease, guard, required)};
}
} // namespace rfc::inference
