#pragma once
#include "rfc/inference/native_relational_homotopy.hpp"
#include "rfc/topology/homotopy_composition.hpp"

namespace rfc::inference {
struct NativeHomotopyCompositionRequest {
    std::uint64_t key{}, revision{}, carrier_source{}, target_source{};
    storage::PinnedNativeAccession carrier, target;
    NativeCylinderCarrierDeclaration realization;
    NativeGluingSourceMap before, interval, after;
};
struct NativeHomotopyCompositionAvailability {
    NativeRelationalHomotopyAvailability original;
    NativeCylinderAvailability domain;
    runtime::WorkAvailability carrier, target;
};
struct NativeHomotopyCompositionProvenance {
    NativeRelationalHomotopyAction action;
    std::uint64_t declaration{}, revision{}, carrier_declaration{}, carrier_revision{};
    // X, I_X, C_X, Y, W, I_W, C_W, Z. Equal namespaces do not merge roles.
    std::array<std::uint64_t, 8> sources, namespaces, sequences;
    std::array<std::uint64_t, 3> maps;
    NativeHomotopyCompositionAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
namespace native_homotopy_composition_detail {
using native_relational_homotopy_detail::Pin;
using native_relational_homotopy_detail::Guard;
using native_relational_homotopy_detail::add;
using native_relational_homotopy_detail::mul;
using native_relational_homotopy_detail::require;
using native_product_detail::requirements;
inline std::array<Pin, 8> pins(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCylinder& c,
    const NativeHomotopyCompositionRequest& r) {
    const auto p = native_relational_homotopy_detail::pins(h.cylinder(), h.declaration());
    return {p[0], p[1], p[2], p[3], c.factor(false), c.factor(true), r.carrier, r.target};
}
inline std::array<std::uint64_t, 8> sources(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCylinder& c,
    const NativeHomotopyCompositionRequest& r) {
    const auto old = h.cylinder();
    return {old.declaration().left_factor, old.declaration().right_factor, h.declaration().carrier_source,
        h.declaration().target_source, c.declaration().left_factor, c.declaration().right_factor, r.carrier_source, r.target_source};
}
inline void available(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCylinder& c,
    const NativeHomotopyCompositionRequest& r, NativeHomotopyCompositionAvailability a) {
    native_relational_homotopy_detail::available(h.cylinder(), h.declaration(), h.preparation_availability());
    native_relational_homotopy_detail::available(h.cylinder(), h.declaration(), a.original);
    native_product_detail::available(c.declaration(), c.preparation_availability());
    native_product_detail::available(c.declaration(), a.domain);
    runtime::admit_work_availability(r.carrier.source_namespace(), r.carrier.sequence(), a.carrier);
    runtime::admit_work_availability(r.target.source_namespace(), r.target.sequence(), a.target);
}
inline std::size_t words(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCylinder& c,
    const NativeHomotopyCompositionRequest& r) {
    auto n = add(native_relational_homotopy_detail::words(h.cylinder(), h.declaration()), c.numeric_plan().retained_words());
    n = add(n, add(algebra::owned_gluing_detail::words(r.carrier.endpoint()->source), algebra::owned_gluing_detail::words(r.target.endpoint()->source)));
    n = add(n, add(12, add(mul(r.realization.types.size(), 2), add(mul(r.realization.cells.size(), 6), mul(r.realization.occurrences.size(), 8)))));
    for (const auto* m : {&r.before, &r.interval, &r.after}) n = add(n, add(3, add(mul(m->cells.size(), 4), mul(m->occurrences.size(), 4))));
    return n;
}
struct State {
    PreparedNativeRelationalHomotopy original;
    PreparedNativeCylinder domain;
    NativeHomotopyCompositionRequest request;
    topology::OwnedHomotopyComposition composition;
    NativeHomotopyCompositionAvailability preparation;
    std::size_t steps{}, retained{}, envelope{};
    std::string policy;
    bool fallback{};
};
inline numeric::ExactActionLimits limits(const State& s, const runtime::ExecutionLease& lease) {
    const auto p = pins(s.original, s.domain, s.request); const auto cylinder = s.original.cylinder();
    return native_product_detail::limits(lease, p,
        std::min(cylinder.declaration().factor_dimension_limit, s.domain.declaration().factor_dimension_limit));
}
inline NativeHomotopyCompositionProvenance report(const State& s, NativeRelationalHomotopyAction action,
    NativeHomotopyCompositionAvailability a, const runtime::ExecutionLease& lease, const Guard& guard, runtime::ExecutionRequirements required) {
    const auto p = pins(s.original, s.domain, s.request); std::array<std::uint64_t, 8> ns{}, seq{};
    for (std::size_t i = 0; i < p.size(); ++i) { ns[i] = p[i].source_namespace(); seq[i] = p[i].sequence(); }
    const auto& r = s.request;
    return {action, r.key, r.revision, r.realization.key, r.realization.revision, sources(s.original, s.domain, r), ns, seq,
        {r.before.key, r.interval.key, r.after.key}, s.preparation, a, required, guard.limits(), s.steps, guard.steps(),
        s.policy, lease.policy().name, s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
} // namespace native_homotopy_composition_detail
class PreparedNativeHomotopyComposition;
class NativeComposedHomotopyWitness;
class DerivedNativeComposedHomotopyChain {
public:
    PreparedNativeHomotopyComposition plan() const;
    const topology::ComposedHomotopyChain& chain() const & { return chain_; }
    const topology::ComposedHomotopyChain& chain() const && = delete;
    const NativeHomotopyCompositionProvenance& provenance() const & { return report_; }
    const NativeHomotopyCompositionProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeCylinderChain> source_input() const { return source_; }
    std::shared_ptr<const DerivedNativeCylinderField> product_input() const { return product_; }
    std::shared_ptr<const DerivedNativeComposedHomotopyChain> input() const { return input_; }
private:
    friend class PreparedNativeHomotopyComposition;
    DerivedNativeComposedHomotopyChain(std::shared_ptr<const native_homotopy_composition_detail::State> owner,
        topology::ComposedHomotopyChain chain, NativeHomotopyCompositionProvenance report,
        std::shared_ptr<const DerivedNativeCylinderChain> source = {}, std::shared_ptr<const DerivedNativeCylinderField> product = {},
        std::shared_ptr<const DerivedNativeComposedHomotopyChain> input = {})
        : owner_(std::move(owner)), chain_(std::move(chain)), report_(std::move(report)), source_(std::move(source)), product_(std::move(product)), input_(std::move(input)) {}
    std::shared_ptr<const native_homotopy_composition_detail::State> owner_; topology::ComposedHomotopyChain chain_;
    NativeHomotopyCompositionProvenance report_; std::shared_ptr<const DerivedNativeCylinderChain> source_;
    std::shared_ptr<const DerivedNativeCylinderField> product_; std::shared_ptr<const DerivedNativeComposedHomotopyChain> input_;
};
class PreparedNativeHomotopyComposition {
public:
    static PreparedNativeHomotopyComposition prepare(PreparedNativeRelationalHomotopy original, PreparedNativeCylinder domain,
        NativeHomotopyCompositionRequest input, NativeHomotopyCompositionAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_homotopy_composition_detail; auto request = input;
        lease.check_cancelled(); available(original, domain, request, a);
        require(request.key && request.revision && request.realization.key && request.realization.revision,
            "native homotopy composition needs its constructor identities and revisions");
        const auto p = pins(original, domain, request); const auto ids = sources(original, domain, request);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            require(ids[i], "native homotopy composition needs complete source identities");
            for (std::size_t j = 0; j < i; ++j) if (ids[i] == ids[j])
                require(p[i].endpoint() == p[j].endpoint() && p[i].same_execution_authority(p[j]),
                    "one composition source identity names distinct native publications or authorities");
        }
        for (const auto* anchor : {&request.carrier, &request.target})
            require(anchor->kind() == storage::NativeAccessionKind::Chain && !anchor->support_count(), "composition carriers must be empty owning Chain anchors");
        require(request.before.key && request.interval.key && request.after.key && request.before.key != request.interval.key &&
            request.before.key != request.after.key && request.interval.key != request.after.key, "composition maps need distinct declared identities");
        require(request.before.from == ids[4] && request.before.to == ids[0] && request.interval.from == ids[5] &&
            request.interval.to == ids[1] && request.after.from == ids[3] && request.after.to == ids[7], "composition maps name a foreign source or target");
        const auto n = words(original, domain, request), envelope = add(1, mul(n, 128));
        runtime::admit_execution(requirements(n, envelope), lease.policy());
        const auto original_cylinder = original.cylinder();
        Guard guard(native_product_detail::limits(lease, p,
            std::min(original_cylinder.declaration().factor_dimension_limit, domain.declaration().factor_dimension_limit)), [&lease]{ lease.check_cancelled(); });
        guard.entries(envelope); original.numeric_plan().validate(guard); domain.numeric_plan().validate(guard);
        for (const auto* anchor : {&request.carrier, &request.target}) algebra::owned_gluing_detail::source(anchor->endpoint()->source, guard);
        // Reuse the stable cylinder interpretation reader, without constructing H
        // or substituting a numerical-only product map for a source morphism.
        const NativeRelationalHomotopyRequest realization{request.key, request.revision, request.carrier_source, request.target_source,
            request.carrier, request.target, request.realization, {}, {}, {}};
        auto carrier = topology::OwnedCylinderCarrier::prepare(domain.numeric_plan(), request.carrier.endpoint()->source,
            native_relational_homotopy_detail::indexed(domain, realization, guard), guard);
        const auto indexed = [&](const NativeGluingSourceMap& m, std::size_t from, std::size_t to) {
            return native_gluing_detail::indexed(p[from].endpoint()->source, p[to].endpoint()->source, m.cells, m.occurrences, guard);
        };
        auto before = indexed(request.before, 4, 0), interval = indexed(request.interval, 5, 1), after = indexed(request.after, 3, 7);
        auto composition = topology::OwnedHomotopyComposition::prepare(original.numeric_plan(), std::move(carrier), request.target.endpoint()->source,
            {request.key, request.revision, std::move(before), std::move(interval), std::move(after)}, guard);
        const auto retained = add(n, composition.retained_words()), complete = add(envelope, composition.entry_envelope());
        guard.entries(complete); runtime::admit_execution(requirements(retained, complete), lease.policy()); lease.check_cancelled();
        return PreparedNativeHomotopyComposition(std::make_shared<const State>(State{std::move(original), std::move(domain), std::move(request),
            std::move(composition), a, guard.steps(), retained, complete, lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeRelationalHomotopy original() const { return state_->original; }
    PreparedNativeCylinder domain() const { return state_->domain; }
    topology::OwnedHomotopyComposition numeric_plan() const { return state_->composition; }
    const NativeHomotopyCompositionRequest& declaration() const & { return state_->request; }
    const NativeHomotopyCompositionRequest& declaration() const && = delete;
    NativeHomotopyCompositionAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeHomotopyComposition& other) const { return state_ == other.state_; }
    DerivedNativeComposedHomotopyChain push(DerivedNativeCylinderField input, NativeHomotopyCompositionAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Push, {}, std::move(input), {}, a, std::move(lease));
    }
    DerivedNativeComposedHomotopyChain endpoint(DerivedNativeCylinderChain input, topology::CylinderEndpoint end,
        NativeHomotopyCompositionAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        native_homotopy_composition_detail::require(end == topology::CylinderEndpoint::Start || end == topology::CylinderEndpoint::Finish, "unknown composition endpoint");
        return action(end == topology::CylinderEndpoint::Start ? NativeRelationalHomotopyAction::Start : NativeRelationalHomotopyAction::Finish, std::move(input), {}, {}, a, std::move(lease));
    }
    DerivedNativeComposedHomotopyChain homotopy(DerivedNativeCylinderChain input, NativeHomotopyCompositionAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Homotopy, std::move(input), {}, {}, a, std::move(lease));
    }
    DerivedNativeComposedHomotopyChain boundary(DerivedNativeComposedHomotopyChain input, NativeHomotopyCompositionAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Boundary, {}, {}, std::move(input), a, std::move(lease));
    }
    NativeComposedHomotopyWitness witness(DerivedNativeCylinderChain, NativeHomotopyCompositionAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeComposedHomotopyChain;
    explicit PreparedNativeHomotopyComposition(std::shared_ptr<const native_homotopy_composition_detail::State> s) : state_(std::move(s)) {}
    DerivedNativeComposedHomotopyChain action(NativeRelationalHomotopyAction, std::optional<DerivedNativeCylinderChain>,
        std::optional<DerivedNativeCylinderField>, std::optional<DerivedNativeComposedHomotopyChain>, NativeHomotopyCompositionAvailability, runtime::ExecutionLease) const;
    std::shared_ptr<const native_homotopy_composition_detail::State> state_;
};
inline PreparedNativeHomotopyComposition DerivedNativeComposedHomotopyChain::plan() const { return PreparedNativeHomotopyComposition(owner_); }
inline DerivedNativeComposedHomotopyChain PreparedNativeHomotopyComposition::action(NativeRelationalHomotopyAction which,
    std::optional<DerivedNativeCylinderChain> source, std::optional<DerivedNativeCylinderField> product,
    std::optional<DerivedNativeComposedHomotopyChain> input, NativeHomotopyCompositionAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_homotopy_composition_detail; const auto pin = state_; lease.check_cancelled();
    available(pin->original, pin->domain, pin->request, pin->preparation); available(pin->original, pin->domain, pin->request, a);
    if (source) require(pin->domain.same_owner(source->plan()), "composition needs its actual domain chain");
    if (product) require(pin->domain.same_owner(product->plan()), "composition needs its actual domain product field");
    if (input) require(input->owner_ == pin, "composition needs its actual native target chain");
    const auto size = source ? source->chain().values().size() : product ? product->field().values().size() : input->chain().chain().values().size();
    const auto required = requirements(pin->retained, add(pin->envelope, mul(size, 8))); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); guard.entries(required.rational_slots);
    for (std::size_t n = 0; n < pin->retained; ++n) guard.step();
    auto out = [&] {
        if (which == NativeRelationalHomotopyAction::Push) return pin->composition.push(product->field(), guard);
        if (which == NativeRelationalHomotopyAction::Boundary) return pin->composition.boundary(input->chain(), guard);
        if (which == NativeRelationalHomotopyAction::Homotopy) return pin->composition.homotopy(source->chain(), guard);
        require(which == NativeRelationalHomotopyAction::Start || which == NativeRelationalHomotopyAction::Finish, "unknown composition action");
        return pin->composition.endpoint(source->chain(), which == NativeRelationalHomotopyAction::Start ? topology::CylinderEndpoint::Start : topology::CylinderEndpoint::Finish, guard);
    }(); lease.check_cancelled();
    return {pin, std::move(out), report(*pin, which, a, lease, guard, required),
        source ? std::make_shared<const DerivedNativeCylinderChain>(std::move(*source)) : nullptr,
        product ? std::make_shared<const DerivedNativeCylinderField>(std::move(*product)) : nullptr,
        input ? std::make_shared<const DerivedNativeComposedHomotopyChain>(std::move(*input)) : nullptr};
}
class NativeComposedHomotopyWitness {
public:
    PreparedNativeHomotopyComposition plan() const { return plan_; }
    std::shared_ptr<const DerivedNativeCylinderChain> input() const { return input_; }
    const topology::ComposedHomotopyWitness& witness() const & { return witness_; }
    const topology::ComposedHomotopyWitness& witness() const && = delete;
    const NativeHomotopyCompositionProvenance& provenance() const & { return report_; }
    const NativeHomotopyCompositionProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeHomotopyComposition;
    NativeComposedHomotopyWitness(PreparedNativeHomotopyComposition plan, std::shared_ptr<const DerivedNativeCylinderChain> input,
        topology::ComposedHomotopyWitness witness, NativeHomotopyCompositionProvenance report)
        : plan_(std::move(plan)), input_(std::move(input)), witness_(std::move(witness)), report_(std::move(report)) {}
    PreparedNativeHomotopyComposition plan_; std::shared_ptr<const DerivedNativeCylinderChain> input_;
    topology::ComposedHomotopyWitness witness_; NativeHomotopyCompositionProvenance report_;
};
inline NativeComposedHomotopyWitness PreparedNativeHomotopyComposition::witness(DerivedNativeCylinderChain input,
    NativeHomotopyCompositionAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_homotopy_composition_detail; const auto pin = state_; lease.check_cancelled();
    available(pin->original, pin->domain, pin->request, pin->preparation); available(pin->original, pin->domain, pin->request, a);
    require(pin->domain.same_owner(input.plan()), "composition witness needs its actual domain chain");
    const auto composed = pin->composition.numeric_plan();
    const auto extra = add(composed.entry_envelope(), mul(add(topology::cylinder_carrier_detail::population(pin->domain.numeric_plan()),
        algebra::owned_gluing_detail::words(composed.target())), 32));
    const auto required = requirements(pin->retained, add(pin->envelope, add(extra, mul(input.chain().values().size(), 8))));
    runtime::admit_execution(required, lease.policy()); Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); guard.entries(required.rational_slots);
    for (std::size_t n = 0; n < pin->retained; ++n) guard.step();
    auto out = pin->composition.witness(input.chain(), guard); lease.check_cancelled();
    return {PreparedNativeHomotopyComposition(pin), std::make_shared<const DerivedNativeCylinderChain>(std::move(input)), std::move(out),
        report(*pin, NativeRelationalHomotopyAction::Witness, a, lease, guard, required)};
}
} // namespace rfc::inference
