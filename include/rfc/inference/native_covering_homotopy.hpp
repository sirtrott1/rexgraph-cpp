#pragma once
#include "rfc/inference/native_covering.hpp"
#include "rfc/inference/native_relational_homotopy.hpp"
#include "rfc/topology/covering_homotopy.hpp"

namespace rfc::inference {
struct NativeCoveringHomotopyRequest {
    std::uint64_t key{}, revision{};
    NativeGluingSourceMap base_map, initial;
};
struct NativeCoveringHomotopyAvailability {
    NativeRelationalHomotopyAvailability original;
    NativeCoveringAvailability covering;
};
struct NativeCoveringHomotopyProvenance {
    NativeRelationalHomotopyAction action;
    std::uint64_t declaration{}, revision{}, covering_declaration{}, covering_revision{};
    // X, I, C, Y, E, B. Aliasing roles still have independent cutoffs.
    std::array<std::uint64_t, 6> sources, namespaces, sequences;
    std::array<std::uint64_t, 2> maps;
    NativeCoveringHomotopyAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
namespace native_covering_homotopy_detail {
using native_relational_homotopy_detail::Pin;
using native_relational_homotopy_detail::Guard;
using native_relational_homotopy_detail::add;
using native_relational_homotopy_detail::mul;
using native_relational_homotopy_detail::require;
using native_product_detail::requirements;
inline std::array<Pin, 6> pins(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCovering& cover) {
    const auto p = native_relational_homotopy_detail::pins(h.cylinder(), h.declaration());
    return {p[0], p[1], p[2], p[3], cover.anchor(local::CoveringChannel::Cover), cover.anchor(local::CoveringChannel::Base)};
}
inline std::array<std::uint64_t, 6> sources(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCovering& cover) {
    const auto cylinder = h.cylinder(); const auto& p = cover.declaration().projection;
    return {cylinder.declaration().left_factor, cylinder.declaration().right_factor,
        h.declaration().carrier_source, h.declaration().target_source, p.from, p.to};
}
inline void available(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCovering& cover,
    NativeCoveringHomotopyAvailability a) {
    native_relational_homotopy_detail::available(h.cylinder(), h.declaration(), h.preparation_availability());
    native_relational_homotopy_detail::available(h.cylinder(), h.declaration(), a.original);
    for (const auto& w : {cover.preparation_availability(), a.covering}) {
        runtime::admit_work_availability(cover.anchor(local::CoveringChannel::Cover).source_namespace(), cover.anchor(local::CoveringChannel::Cover).sequence(), w.cover);
        runtime::admit_work_availability(cover.anchor(local::CoveringChannel::Base).source_namespace(), cover.anchor(local::CoveringChannel::Base).sequence(), w.base);
    }
}
inline std::size_t words(const PreparedNativeRelationalHomotopy& h, const PreparedNativeCovering& cover, const NativeCoveringHomotopyRequest& r) {
    auto n = add(native_relational_homotopy_detail::words(h.cylinder(), h.declaration()), cover.numeric_plan().retained_words());
    const auto& p = cover.declaration().projection;
    n = add(n, add(10, add(mul(p.cells.size(), 4), mul(p.occurrences.size(), 4))));
    for (const auto* m : {&r.base_map, &r.initial}) n = add(n, add(3, add(mul(m->cells.size(), 4), mul(m->occurrences.size(), 4))));
    return n;
}
struct State {
    PreparedNativeRelationalHomotopy original;
    PreparedNativeCovering covering;
    NativeCoveringHomotopyRequest request;
    topology::OwnedCoveringHomotopy lift;
    NativeCoveringHomotopyAvailability preparation;
    std::size_t steps{}, retained{}, envelope{};
    std::string policy;
    bool fallback{};
};
inline numeric::ExactActionLimits limits(const State& s, const runtime::ExecutionLease& lease) {
    const auto cylinder = s.original.cylinder();
    return native_product_detail::limits(lease, pins(s.original, s.covering),
        std::min(cylinder.declaration().factor_dimension_limit, s.covering.declaration().dimension_limit));
}
inline NativeCoveringHomotopyProvenance report(const State& s, NativeRelationalHomotopyAction action,
    NativeCoveringHomotopyAvailability a, const runtime::ExecutionLease& lease, const Guard& guard, runtime::ExecutionRequirements required) {
    const auto p = pins(s.original, s.covering); std::array<std::uint64_t, 6> ns{}, seq{};
    for (std::size_t i = 0; i < p.size(); ++i) { ns[i] = p[i].source_namespace(); seq[i] = p[i].sequence(); }
    return {action, s.request.key, s.request.revision, s.covering.declaration().key, s.covering.declaration().revision,
        sources(s.original, s.covering), ns, seq, {s.request.base_map.key, s.request.initial.key}, s.preparation, a, required,
        guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name, s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
} // namespace native_covering_homotopy_detail
class PreparedNativeCoveringHomotopy;
class NativeCoveringHomotopyWitness;
class DerivedNativeLiftedHomotopyChain {
public:
    PreparedNativeCoveringHomotopy plan() const;
    const topology::LiftedHomotopyChain& chain() const & { return chain_; }
    const topology::LiftedHomotopyChain& chain() const && = delete;
    const NativeCoveringHomotopyProvenance& provenance() const & { return report_; }
    const NativeCoveringHomotopyProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeCylinderChain> source_input() const { return source_; }
    std::shared_ptr<const DerivedNativeCylinderField> product_input() const { return product_; }
    std::shared_ptr<const DerivedNativeLiftedHomotopyChain> input() const { return input_; }
private:
    friend class PreparedNativeCoveringHomotopy;
    DerivedNativeLiftedHomotopyChain(std::shared_ptr<const native_covering_homotopy_detail::State> owner,
        topology::LiftedHomotopyChain chain, NativeCoveringHomotopyProvenance report,
        std::shared_ptr<const DerivedNativeCylinderChain> source = {}, std::shared_ptr<const DerivedNativeCylinderField> product = {},
        std::shared_ptr<const DerivedNativeLiftedHomotopyChain> input = {})
        : owner_(std::move(owner)), chain_(std::move(chain)), report_(std::move(report)), source_(std::move(source)), product_(std::move(product)), input_(std::move(input)) {}
    std::shared_ptr<const native_covering_homotopy_detail::State> owner_; topology::LiftedHomotopyChain chain_;
    NativeCoveringHomotopyProvenance report_; std::shared_ptr<const DerivedNativeCylinderChain> source_;
    std::shared_ptr<const DerivedNativeCylinderField> product_; std::shared_ptr<const DerivedNativeLiftedHomotopyChain> input_;
};
class PreparedNativeCoveringHomotopy {
public:
    static PreparedNativeCoveringHomotopy prepare(PreparedNativeRelationalHomotopy original, PreparedNativeCovering covering,
        NativeCoveringHomotopyRequest input, NativeCoveringHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_covering_homotopy_detail; auto request = input; lease.check_cancelled(); available(original, covering, a);
        require(request.key && request.revision, "native covering homotopy needs its constructor identity and revision");
        const auto p = pins(original, covering); const auto ids = sources(original, covering);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            require(ids[i], "native covering homotopy needs complete source identities");
            for (std::size_t j = 0; j < i; ++j) if (ids[i] == ids[j])
                require(p[i].endpoint() == p[j].endpoint() && p[i].same_execution_authority(p[j]),
                    "one lifting source identity names distinct native publications or authorities");
        }
        require(request.base_map.key && request.initial.key && request.base_map.key != request.initial.key,
            "covering homotopy maps need distinct declared identities");
        for (const auto key : {request.base_map.key, request.initial.key})
            for (const auto old : {original.declaration().cylinder_target.key, original.declaration().first.key,
                original.declaration().second.key, covering.declaration().projection.key})
                require(key != old, "covering homotopy reuses an existing map identity");
        require(request.base_map.from == ids[3] && request.base_map.to == ids[5] && request.initial.from == ids[0] && request.initial.to == ids[4],
            "covering homotopy maps name a foreign original source or target");
        const auto n = words(original, covering, request), envelope = add(1, mul(n, 128));
        runtime::admit_execution(requirements(n, envelope), lease.policy()); const auto cylinder = original.cylinder();
        Guard guard(native_product_detail::limits(lease, p, std::min(cylinder.declaration().factor_dimension_limit, covering.declaration().dimension_limit)), [&lease]{ lease.check_cancelled(); });
        guard.entries(envelope); original.numeric_plan().validate(guard); covering.numeric_plan().validate(guard);
        auto base = native_gluing_detail::indexed(p[3].endpoint()->source, p[5].endpoint()->source, request.base_map.cells, request.base_map.occurrences, guard);
        auto initial = native_gluing_detail::indexed(p[0].endpoint()->source, p[4].endpoint()->source, request.initial.cells, request.initial.occurrences, guard);
        auto lifted = topology::OwnedCoveringHomotopy::prepare(original.numeric_plan(), covering.numeric_plan(),
            {request.key, request.revision, std::move(base), std::move(initial)}, guard);
        const auto retained = add(n, lifted.retained_words()), complete = add(envelope, lifted.entry_envelope());
        guard.entries(complete); runtime::admit_execution(requirements(retained, complete), lease.policy()); lease.check_cancelled();
        return PreparedNativeCoveringHomotopy(std::make_shared<const State>(State{std::move(original), std::move(covering), std::move(request),
            std::move(lifted), a, guard.steps(), retained, complete, lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeRelationalHomotopy original() const { return state_->original; }
    PreparedNativeCovering covering() const { return state_->covering; }
    PreparedNativeCylinder domain() const { return state_->original.cylinder(); }
    topology::OwnedCoveringHomotopy numeric_plan() const { return state_->lift; }
    const NativeCoveringHomotopyRequest& declaration() const & { return state_->request; }
    const NativeCoveringHomotopyRequest& declaration() const && = delete;
    NativeCoveringHomotopyAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeCoveringHomotopy& other) const { return state_ == other.state_; }
    DerivedNativeLiftedHomotopyChain push(DerivedNativeCylinderField input, NativeCoveringHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Push, {}, std::move(input), {}, a, std::move(lease));
    }
    DerivedNativeLiftedHomotopyChain endpoint(DerivedNativeCylinderChain input, topology::CylinderEndpoint end,
        NativeCoveringHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        native_covering_homotopy_detail::require(end == topology::CylinderEndpoint::Start || end == topology::CylinderEndpoint::Finish, "unknown lifted homotopy endpoint");
        return action(end == topology::CylinderEndpoint::Start ? NativeRelationalHomotopyAction::Start : NativeRelationalHomotopyAction::Finish, std::move(input), {}, {}, a, std::move(lease));
    }
    DerivedNativeLiftedHomotopyChain homotopy(DerivedNativeCylinderChain input, NativeCoveringHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Homotopy, std::move(input), {}, {}, a, std::move(lease));
    }
    DerivedNativeLiftedHomotopyChain boundary(DerivedNativeLiftedHomotopyChain input, NativeCoveringHomotopyAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Boundary, {}, {}, std::move(input), a, std::move(lease));
    }
    NativeCoveringHomotopyWitness witness(DerivedNativeCylinderChain, NativeCoveringHomotopyAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeLiftedHomotopyChain;
    explicit PreparedNativeCoveringHomotopy(std::shared_ptr<const native_covering_homotopy_detail::State> s) : state_(std::move(s)) {}
    DerivedNativeLiftedHomotopyChain action(NativeRelationalHomotopyAction, std::optional<DerivedNativeCylinderChain>,
        std::optional<DerivedNativeCylinderField>, std::optional<DerivedNativeLiftedHomotopyChain>, NativeCoveringHomotopyAvailability, runtime::ExecutionLease) const;
    std::shared_ptr<const native_covering_homotopy_detail::State> state_;
};
inline PreparedNativeCoveringHomotopy DerivedNativeLiftedHomotopyChain::plan() const { return PreparedNativeCoveringHomotopy(owner_); }
inline DerivedNativeLiftedHomotopyChain PreparedNativeCoveringHomotopy::action(NativeRelationalHomotopyAction which,
    std::optional<DerivedNativeCylinderChain> source, std::optional<DerivedNativeCylinderField> product,
    std::optional<DerivedNativeLiftedHomotopyChain> input, NativeCoveringHomotopyAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_covering_homotopy_detail; const auto pin = state_; lease.check_cancelled();
    available(pin->original, pin->covering, pin->preparation); available(pin->original, pin->covering, a); const auto domain = pin->original.cylinder();
    if (source) require(domain.same_owner(source->plan()), "covering homotopy needs its actual domain chain");
    if (product) require(domain.same_owner(product->plan()), "covering homotopy needs its actual domain product field");
    if (input) require(input->owner_ == pin, "covering homotopy needs its actual native target chain");
    const auto size = source ? source->chain().values().size() : product ? product->field().values().size() : input->chain().chain().values().size();
    const auto required = requirements(pin->retained, add(pin->envelope, mul(size, 8))); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); guard.entries(required.rational_slots);
    for (std::size_t n = 0; n < pin->retained; ++n) guard.step();
    auto out = [&] {
        if (which == NativeRelationalHomotopyAction::Push) return pin->lift.push(product->field(), guard);
        if (which == NativeRelationalHomotopyAction::Boundary) return pin->lift.boundary(input->chain(), guard);
        if (which == NativeRelationalHomotopyAction::Homotopy) return pin->lift.homotopy(source->chain(), guard);
        require(which == NativeRelationalHomotopyAction::Start || which == NativeRelationalHomotopyAction::Finish, "unknown covering homotopy action");
        return pin->lift.endpoint(source->chain(), which == NativeRelationalHomotopyAction::Start ? topology::CylinderEndpoint::Start : topology::CylinderEndpoint::Finish, guard);
    }(); lease.check_cancelled();
    return {pin, std::move(out), report(*pin, which, a, lease, guard, required),
        source ? std::make_shared<const DerivedNativeCylinderChain>(std::move(*source)) : nullptr,
        product ? std::make_shared<const DerivedNativeCylinderField>(std::move(*product)) : nullptr,
        input ? std::make_shared<const DerivedNativeLiftedHomotopyChain>(std::move(*input)) : nullptr};
}
class NativeCoveringHomotopyWitness {
public:
    PreparedNativeCoveringHomotopy plan() const { return plan_; }
    std::shared_ptr<const DerivedNativeCylinderChain> input() const { return input_; }
    const topology::CoveringHomotopyWitness& witness() const & { return witness_; }
    const topology::CoveringHomotopyWitness& witness() const && = delete;
    const NativeCoveringHomotopyProvenance& provenance() const & { return report_; }
    const NativeCoveringHomotopyProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeCoveringHomotopy;
    NativeCoveringHomotopyWitness(PreparedNativeCoveringHomotopy plan, std::shared_ptr<const DerivedNativeCylinderChain> input,
        topology::CoveringHomotopyWitness witness, NativeCoveringHomotopyProvenance report)
        : plan_(std::move(plan)), input_(std::move(input)), witness_(std::move(witness)), report_(std::move(report)) {}
    PreparedNativeCoveringHomotopy plan_; std::shared_ptr<const DerivedNativeCylinderChain> input_;
    topology::CoveringHomotopyWitness witness_; NativeCoveringHomotopyProvenance report_;
};
inline NativeCoveringHomotopyWitness PreparedNativeCoveringHomotopy::witness(DerivedNativeCylinderChain input,
    NativeCoveringHomotopyAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_covering_homotopy_detail; const auto pin = state_; lease.check_cancelled();
    available(pin->original, pin->covering, pin->preparation); available(pin->original, pin->covering, a); const auto domain = pin->original.cylinder();
    require(domain.same_owner(input.plan()), "covering homotopy witness needs its actual domain chain"); const auto lifted = pin->lift.numeric_plan();
    const auto extra = add(lifted.entry_envelope(), mul(add(topology::cylinder_carrier_detail::population(domain.numeric_plan()),
        algebra::owned_gluing_detail::words(lifted.target())), 32));
    const auto required = requirements(pin->retained, add(pin->envelope, add(extra, mul(input.chain().values().size(), 8))));
    runtime::admit_execution(required, lease.policy()); Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); guard.entries(required.rational_slots);
    for (std::size_t n = 0; n < pin->retained; ++n) guard.step();
    auto out = pin->lift.witness(input.chain(), guard); lease.check_cancelled();
    return {PreparedNativeCoveringHomotopy(pin), std::make_shared<const DerivedNativeCylinderChain>(std::move(input)), std::move(out),
        report(*pin, NativeRelationalHomotopyAction::Witness, a, lease, guard, required)};
}
} // namespace rfc::inference
