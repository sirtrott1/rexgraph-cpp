#pragma once
#include "rfc/inference/native_homotopy_composition.hpp"
#include "rfc/topology/homotopy_sequence.hpp"

namespace rfc::inference {
struct NativeHomotopyStageAvailability {
    NativeCylinderAvailability domain;
    runtime::WorkAvailability carrier, target;
};
struct NativeHomotopySequenceAvailability {
    NativeRelationalHomotopyAvailability root;
    std::vector<NativeHomotopyStageAvailability> stages;
};
struct NativeHomotopyStageProvenance {
    std::uint64_t declaration{}, revision{}, carrier_declaration{}, carrier_revision{};
    std::array<std::uint64_t, 3> maps;
    NativeHomotopySequenceAvailability preparation_availability;
    std::size_t preparation_steps{};
    std::string preparation_policy;
    bool fallback{};
};
struct NativeHomotopySequenceProvenance {
    NativeRelationalHomotopyAction action;
    topology::HomotopySequenceDeclaration declaration;
    // Root X,I,C,Y followed by each stage's W,I_W,C_W,Z, in append order.
    // Repeated actual owners/namespaces still occupy independent roles.
    std::vector<std::uint64_t> sources, namespaces, sequences;
    std::array<std::uint64_t, 3> root_maps;
    std::vector<NativeHomotopyStageProvenance> stages;
    NativeHomotopySequenceAvailability preparation_availability, execution_availability;
    NativeRelationalHomotopyAvailability root_preparation_availability;
    std::size_t root_preparation_steps{};
    std::string root_preparation_policy;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
namespace native_homotopy_sequence_detail {
using native_homotopy_composition_detail::Pin;
using native_homotopy_composition_detail::Guard;
using native_homotopy_composition_detail::add;
using native_homotopy_composition_detail::mul;
using native_homotopy_composition_detail::require;
using native_product_detail::requirements;
struct Stage {
    PreparedNativeCylinder domain;
    NativeHomotopyCompositionRequest request;
    NativeHomotopyStageProvenance provenance;
};
struct State {
    PreparedNativeRelationalHomotopy root;
    topology::OwnedHomotopySequence sequence;
    std::vector<Stage> stages;
    NativeHomotopySequenceAvailability preparation;
    NativeRelationalHomotopyAvailability root_preparation;
    std::size_t root_steps{};
    std::string root_policy;
    std::size_t steps{}, retained{}, envelope{};
    std::string policy;
    bool fallback{};
};
inline PreparedNativeCylinder domain(const State& s) { return s.stages.empty() ? s.root.cylinder() : s.stages.back().domain; }
inline std::array<Pin, 4> pins(const Stage& s) { return {s.domain.factor(false), s.domain.factor(true), s.request.carrier, s.request.target}; }
inline std::array<std::uint64_t, 4> sources(const Stage& s) {
    return {s.domain.declaration().left_factor, s.domain.declaration().right_factor, s.request.carrier_source, s.request.target_source};
}
inline std::vector<Pin> pins(const State& s) {
    const auto root = native_relational_homotopy_detail::pins(s.root.cylinder(), s.root.declaration());
    std::vector<Pin> out(root.begin(), root.end());
    for (const auto& stage : s.stages) { const auto p = pins(stage); out.insert(out.end(), p.begin(), p.end()); }
    return out;
}
inline std::vector<std::uint64_t> sources(const State& s) {
    const auto c = s.root.cylinder(); const auto& r = s.root.declaration();
    std::vector<std::uint64_t> out{c.declaration().left_factor, c.declaration().right_factor, r.carrier_source, r.target_source};
    for (const auto& stage : s.stages) { const auto ids = sources(stage); out.insert(out.end(), ids.begin(), ids.end()); }
    return out;
}
inline void available(const Stage& s, NativeHomotopyStageAvailability a) {
    native_product_detail::available(s.domain.declaration(), s.domain.preparation_availability());
    native_product_detail::available(s.domain.declaration(), a.domain);
    runtime::admit_work_availability(s.request.carrier.source_namespace(), s.request.carrier.sequence(), a.carrier);
    runtime::admit_work_availability(s.request.target.source_namespace(), s.request.target.sequence(), a.target);
}
inline void prefix_available(const State& s, const NativeHomotopySequenceAvailability& a, std::size_t count) {
    require(a.stages.size() == count, "homotopy sequence needs exactly one availability per retained stage");
    native_relational_homotopy_detail::available(s.root.cylinder(), s.root.declaration(), a.root);
    for (std::size_t i = 0; i < count; ++i) available(s.stages.at(i), a.stages[i]);
}
inline void available(const State& s, const NativeHomotopySequenceAvailability& a) {
    native_relational_homotopy_detail::available(s.root.cylinder(), s.root.declaration(), s.root.preparation_availability());
    native_relational_homotopy_detail::available(s.root.cylinder(), s.root.declaration(), s.root_preparation);
    prefix_available(s, s.preparation, s.stages.size()); prefix_available(s, a, s.stages.size());
    for (std::size_t i = 0; i < s.stages.size(); ++i) prefix_available(s, s.stages[i].provenance.preparation_availability, i + 1);
}
inline std::size_t words(const Stage& s) {
    const auto& r = s.request;
    auto n = add(s.domain.numeric_plan().retained_words(), add(algebra::owned_gluing_detail::words(r.carrier.endpoint()->source), algebra::owned_gluing_detail::words(r.target.endpoint()->source)));
    n = add(n, add(12, add(mul(r.realization.types.size(), 2), add(mul(r.realization.cells.size(), 6), mul(r.realization.occurrences.size(), 8)))));
    for (const auto* m : {&r.before, &r.interval, &r.after}) n = add(n, add(3, add(mul(m->cells.size(), 4), mul(m->occurrences.size(), 4))));
    return add(n, add(8, mul(s.provenance.preparation_availability.stages.size(), 8)));
}
inline std::size_t dimension(const State& s) {
    const auto c = s.root.cylinder(); auto n = c.declaration().factor_dimension_limit;
    for (const auto& stage : s.stages) n = std::min(n, stage.domain.declaration().factor_dimension_limit);
    return n;
}
inline numeric::ExactActionLimits limits(const State& s, const runtime::ExecutionLease& lease) {
    const auto p = pins(s); return native_product_detail::limits(lease, p, dimension(s));
}
inline void aliases(const State& s, Guard& guard) {
    const auto p = pins(s); const auto ids = sources(s);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        guard.step(); require(ids[i], "homotopy sequence needs every source identity");
        for (std::size_t j = 0; j < i; ++j) { guard.step(); if (ids[i] == ids[j])
            require(p[i].endpoint() == p[j].endpoint() && p[i].same_execution_authority(p[j]), "one sequence source identity names distinct publications or execution authorities"); }
    }
}
inline NativeHomotopySequenceProvenance report(const State& s, NativeRelationalHomotopyAction action,
    const NativeHomotopySequenceAvailability& a, const runtime::ExecutionLease& lease, const Guard& guard, runtime::ExecutionRequirements required) {
    const auto p = pins(s); std::vector<std::uint64_t> ns, seq; std::vector<NativeHomotopyStageProvenance> stages;
    for (const auto& pin : p) { ns.push_back(pin.source_namespace()); seq.push_back(pin.sequence()); }
    for (const auto& stage : s.stages) stages.push_back(stage.provenance);
    const auto& r = s.root.declaration();
    return {action, s.sequence.declaration(), sources(s), std::move(ns), std::move(seq), {r.cylinder_target.key, r.first.key, r.second.key},
        std::move(stages), s.preparation, a, s.root_preparation, s.root_steps, s.root_policy, required, guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
} // namespace native_homotopy_sequence_detail
class PreparedNativeHomotopySequence;
class NativeSequencedHomotopyWitness;
class DerivedNativeSequencedHomotopyChain {
public:
    PreparedNativeHomotopySequence plan() const;
    const topology::SequencedHomotopyChain& chain() const & { return chain_; }
    const topology::SequencedHomotopyChain& chain() const && = delete;
    const NativeHomotopySequenceProvenance& provenance() const & { return report_; }
    const NativeHomotopySequenceProvenance& provenance() const && = delete;
    std::shared_ptr<const DerivedNativeCylinderChain> source_input() const { return source_; }
    std::shared_ptr<const DerivedNativeCylinderField> product_input() const { return product_; }
    std::shared_ptr<const DerivedNativeSequencedHomotopyChain> input() const { return input_; }
private:
    friend class PreparedNativeHomotopySequence;
    DerivedNativeSequencedHomotopyChain(std::shared_ptr<const native_homotopy_sequence_detail::State> owner,
        topology::SequencedHomotopyChain chain, NativeHomotopySequenceProvenance report,
        std::shared_ptr<const DerivedNativeCylinderChain> source = {}, std::shared_ptr<const DerivedNativeCylinderField> product = {},
        std::shared_ptr<const DerivedNativeSequencedHomotopyChain> input = {})
        : owner_(std::move(owner)), chain_(std::move(chain)), report_(std::move(report)), source_(std::move(source)), product_(std::move(product)), input_(std::move(input)) {}
    std::shared_ptr<const native_homotopy_sequence_detail::State> owner_; topology::SequencedHomotopyChain chain_;
    NativeHomotopySequenceProvenance report_; std::shared_ptr<const DerivedNativeCylinderChain> source_;
    std::shared_ptr<const DerivedNativeCylinderField> product_; std::shared_ptr<const DerivedNativeSequencedHomotopyChain> input_;
};
class PreparedNativeHomotopySequence {
public:
    static PreparedNativeHomotopySequence prepare(PreparedNativeRelationalHomotopy root, topology::HomotopySequenceDeclaration d,
        NativeHomotopySequenceAvailability input, runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_homotopy_sequence_detail; auto a = input; lease.check_cancelled();
        require(a.stages.empty(), "root homotopy sequence cannot have later stage availability");
        native_relational_homotopy_detail::available(root.cylinder(), root.declaration(), root.preparation_availability());
        native_relational_homotopy_detail::available(root.cylinder(), root.declaration(), a.root);
        require(d.key != root.declaration().key, "homotopy sequence reuses its root identity");
        const auto n = add(11, native_relational_homotopy_detail::words(root.cylinder(), root.declaration())), e = add(1, mul(n, 256));
        runtime::admit_execution(requirements(n, e), lease.policy()); const auto p = native_relational_homotopy_detail::pins(root.cylinder(), root.declaration());
        const auto c = root.cylinder(); Guard guard(native_product_detail::limits(lease, p, c.declaration().factor_dimension_limit), [&lease]{ lease.check_cancelled(); });
        guard.entries(e); auto sequence = topology::OwnedHomotopySequence::prepare(root.numeric_plan(), d, guard);
        const auto retained = add(n, sequence.retained_words()), envelope = add(add(1,mul(n,128)), sequence.entry_envelope());
        guard.entries(envelope); runtime::admit_execution(requirements(retained, envelope), lease.policy()); lease.check_cancelled();
        const auto original_availability = a.root;
        State s{std::move(root), std::move(sequence), {}, std::move(a), original_availability, 0, lease.policy().name,
            guard.steps(), retained, envelope, lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
        aliases(s, guard); s.steps = guard.steps(); s.root_steps = guard.steps();
        return PreparedNativeHomotopySequence(std::make_shared<const State>(std::move(s)));
    }
    PreparedNativeHomotopySequence append(PreparedNativeCylinder new_domain, NativeHomotopyCompositionRequest input,
        NativeHomotopySequenceAvailability availability, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        using namespace native_homotopy_sequence_detail; const auto pin = state_; auto request = input; auto a = availability;
        lease.check_cancelled(); require(pin->stages.size() < pin->sequence.declaration().max_stages, "native homotopy sequence stage bound exceeded");
        require(a.stages.size() == add(pin->stages.size(), 1), "append needs the full prefix and new stage availability");
        auto previous = a; previous.stages.pop_back(); available(*pin, previous);
        Stage next{std::move(new_domain), std::move(request), {0,0,0,0,{},a,0,lease.policy().name,lease.policy().backend != runtime::ExecutionBackend::ExactCpu}};
        available(next, a.stages.back()); const auto& r = next.request;
        require(r.key && r.revision && r.realization.key && r.realization.revision, "sequence stage needs its constructor identities and revisions");
        for (const auto* anchor : {&r.carrier, &r.target}) require(anchor->kind() == storage::NativeAccessionKind::Chain && !anchor->support_count(), "sequence carriers must be empty owning Chain anchors");
        const std::array<std::uint64_t, 3> keys{r.before.key, r.interval.key, r.after.key};
        const auto& root = pin->root.declaration(); const std::array<std::uint64_t, 3> original_keys{root.cylinder_target.key, root.first.key, root.second.key};
        for (std::size_t i = 0; i < keys.size(); ++i) {
            require(keys[i], "sequence maps need declared identities");
            for (std::size_t j = 0; j < i; ++j) require(keys[i] != keys[j], "stage map identities must be distinct");
            for (const auto key : original_keys) require(keys[i] != key, "stage map reuses an original operation identity");
            for (const auto& old : pin->stages) for (const auto key : old.provenance.maps) require(keys[i] != key, "stage map identity already retained");
        }
        const auto old_domain = native_homotopy_sequence_detail::domain(*pin);
        const auto old_target = pin->stages.empty() ? root.target_source : pin->stages.back().request.target_source;
        require(r.before.from == next.domain.declaration().left_factor && r.before.to == old_domain.declaration().left_factor &&
            r.interval.from == next.domain.declaration().right_factor && r.interval.to == old_domain.declaration().right_factor &&
            r.after.from == old_target && r.after.to == r.target_source, "sequence maps name a foreign preceding source or target");
        // The new immutable state contains the prefix spine once. Its previous
        // cumulative native envelope is not another retained prefix allocation.
        const auto n = add(pin->retained - pin->sequence.retained_words(), add(words(next),8));
        const auto e = add(pin->sequence.entry_envelope(), add(1,mul(add(n,pin->sequence.retained_words()),256)));
        runtime::admit_execution(requirements(n, e), lease.policy()); auto p = pins(*pin); const auto extra = pins(next); p.insert(p.end(), extra.begin(), extra.end());
        Guard guard(native_product_detail::limits(lease, p, std::min(dimension(*pin), next.domain.declaration().factor_dimension_limit)), [&lease]{ lease.check_cancelled(); });
        guard.entries(e); pin->sequence.validate(guard); next.domain.numeric_plan().validate(guard);
        for (const auto* anchor : {&r.carrier, &r.target}) algebra::owned_gluing_detail::source(anchor->endpoint()->source, guard);
        const NativeRelationalHomotopyRequest realization{r.key,r.revision,r.carrier_source,r.target_source,r.carrier,r.target,r.realization,{},{},{}};
        auto carrier = topology::OwnedCylinderCarrier::prepare(next.domain.numeric_plan(), r.carrier.endpoint()->source,
            native_relational_homotopy_detail::indexed(next.domain, realization, guard), guard);
        const auto old_pins = pin->stages.empty() ? native_relational_homotopy_detail::pins(pin->root.cylinder(), root) : pins(pin->stages.back());
        const auto index = [&](const NativeGluingSourceMap& m, const Pin& from, const Pin& to) {
            return native_gluing_detail::indexed(from.endpoint()->source, to.endpoint()->source, m.cells, m.occurrences, guard);
        };
        auto before = index(r.before, extra[0], old_pins[0]), interval = index(r.interval, extra[1], old_pins[1]), after = index(r.after, old_pins[3], extra[3]);
        auto sequence = pin->sequence.append(std::move(carrier), r.target.endpoint()->source, {r.key,r.revision,std::move(before),std::move(interval),std::move(after)}, guard);
        const auto retained = add(n, sequence.retained_words()), envelope = add(add(1,mul(n,128)), sequence.entry_envelope()); guard.entries(envelope);
        runtime::admit_execution(requirements(retained, envelope), lease.policy());
        next.provenance.declaration = r.key; next.provenance.revision = r.revision;
        next.provenance.carrier_declaration = r.realization.key; next.provenance.carrier_revision = r.realization.revision; next.provenance.maps = keys;
        auto stages = pin->stages; stages.push_back(std::move(next));
        State s{pin->root,std::move(sequence),std::move(stages),std::move(a),pin->root_preparation,pin->root_steps,pin->root_policy,0,retained,envelope,lease.policy().name,
            pin->fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
        aliases(s, guard); lease.check_cancelled(); s.steps = add(pin->steps, guard.steps()); s.stages.back().provenance.preparation_steps = guard.steps();
        return PreparedNativeHomotopySequence(std::make_shared<const State>(std::move(s)));
    }
    PreparedNativeRelationalHomotopy root() const { return state_->root; }
    PreparedNativeCylinder domain() const { return native_homotopy_sequence_detail::domain(*state_); }
    topology::OwnedHomotopySequence numeric_plan() const { return state_->sequence; }
    std::size_t stage_count() const { return state_->stages.size(); }
    PreparedNativeCylinder stage_domain(std::size_t i) const { return state_->stages.at(i).domain; }
    const NativeHomotopyCompositionRequest& stage_declaration(std::size_t i) const & { return state_->stages.at(i).request; }
    const NativeHomotopyCompositionRequest& stage_declaration(std::size_t) const && = delete;
    const topology::HomotopySequenceDeclaration& declaration() const & { return state_->sequence.declaration(); }
    const topology::HomotopySequenceDeclaration& declaration() const && = delete;
    NativeHomotopySequenceAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeHomotopySequence& other) const { return state_ == other.state_; }
    DerivedNativeSequencedHomotopyChain push(DerivedNativeCylinderField input, NativeHomotopySequenceAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Push, {}, std::move(input), {}, std::move(a), std::move(lease));
    }
    DerivedNativeSequencedHomotopyChain endpoint(DerivedNativeCylinderChain input, topology::CylinderEndpoint end,
        NativeHomotopySequenceAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        native_homotopy_sequence_detail::require(end == topology::CylinderEndpoint::Start || end == topology::CylinderEndpoint::Finish, "unknown sequence endpoint");
        return action(end == topology::CylinderEndpoint::Start ? NativeRelationalHomotopyAction::Start : NativeRelationalHomotopyAction::Finish, std::move(input), {}, {}, std::move(a), std::move(lease));
    }
    DerivedNativeSequencedHomotopyChain homotopy(DerivedNativeCylinderChain input, NativeHomotopySequenceAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Homotopy, std::move(input), {}, {}, std::move(a), std::move(lease));
    }
    DerivedNativeSequencedHomotopyChain boundary(DerivedNativeSequencedHomotopyChain input, NativeHomotopySequenceAvailability a, runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        return action(NativeRelationalHomotopyAction::Boundary, {}, {}, std::move(input), std::move(a), std::move(lease));
    }
    NativeSequencedHomotopyWitness witness(DerivedNativeCylinderChain, NativeHomotopySequenceAvailability, runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeSequencedHomotopyChain;
    explicit PreparedNativeHomotopySequence(std::shared_ptr<const native_homotopy_sequence_detail::State> s) : state_(std::move(s)) {}
    DerivedNativeSequencedHomotopyChain action(NativeRelationalHomotopyAction, std::optional<DerivedNativeCylinderChain>,
        std::optional<DerivedNativeCylinderField>, std::optional<DerivedNativeSequencedHomotopyChain>, NativeHomotopySequenceAvailability, runtime::ExecutionLease) const;
    std::shared_ptr<const native_homotopy_sequence_detail::State> state_;
};
inline PreparedNativeHomotopySequence DerivedNativeSequencedHomotopyChain::plan() const { return PreparedNativeHomotopySequence(owner_); }
inline DerivedNativeSequencedHomotopyChain PreparedNativeHomotopySequence::action(NativeRelationalHomotopyAction which,
    std::optional<DerivedNativeCylinderChain> source, std::optional<DerivedNativeCylinderField> product,
    std::optional<DerivedNativeSequencedHomotopyChain> input, NativeHomotopySequenceAvailability availability, runtime::ExecutionLease lease) const {
    using namespace native_homotopy_sequence_detail; const auto pin = state_; auto a = availability; lease.check_cancelled(); available(*pin, a);
    const auto current_domain = native_homotopy_sequence_detail::domain(*pin);
    if (source) require(current_domain.same_owner(source->plan()), "sequence needs its actual current domain chain");
    if (product) require(current_domain.same_owner(product->plan()), "sequence needs its actual current product field");
    if (input) require(input->owner_ == pin, "sequence needs its actual native output owner");
    const auto size = source ? source->chain().values().size() : product ? product->field().values().size() : input->chain().chain().values().size();
    const auto required = requirements(pin->retained, add(pin->envelope, mul(size, 8))); runtime::admit_execution(required, lease.policy());
    Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); guard.entries(required.rational_slots);
    for (std::size_t i = 0; i < pin->retained; ++i) guard.step();
    auto out = [&] {
        if (which == NativeRelationalHomotopyAction::Push) return pin->sequence.push(product->field(), guard);
        if (which == NativeRelationalHomotopyAction::Boundary) return pin->sequence.boundary(input->chain(), guard);
        if (which == NativeRelationalHomotopyAction::Homotopy) return pin->sequence.homotopy(source->chain(), guard);
        require(which == NativeRelationalHomotopyAction::Start || which == NativeRelationalHomotopyAction::Finish, "unknown sequence action");
        return pin->sequence.endpoint(source->chain(), which == NativeRelationalHomotopyAction::Start ? topology::CylinderEndpoint::Start : topology::CylinderEndpoint::Finish, guard);
    }(); lease.check_cancelled();
    return {pin,std::move(out),report(*pin,which,a,lease,guard,required),
        source ? std::make_shared<const DerivedNativeCylinderChain>(std::move(*source)) : nullptr,
        product ? std::make_shared<const DerivedNativeCylinderField>(std::move(*product)) : nullptr,
        input ? std::make_shared<const DerivedNativeSequencedHomotopyChain>(std::move(*input)) : nullptr};
}
class NativeSequencedHomotopyWitness {
public:
    PreparedNativeHomotopySequence plan() const { return plan_; }
    std::shared_ptr<const DerivedNativeCylinderChain> input() const { return input_; }
    const topology::SequencedHomotopyWitness& witness() const & { return witness_; }
    const topology::SequencedHomotopyWitness& witness() const && = delete;
    const NativeHomotopySequenceProvenance& provenance() const & { return report_; }
    const NativeHomotopySequenceProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeHomotopySequence;
    NativeSequencedHomotopyWitness(PreparedNativeHomotopySequence p, std::shared_ptr<const DerivedNativeCylinderChain> input,
        topology::SequencedHomotopyWitness w, NativeHomotopySequenceProvenance report)
        : plan_(std::move(p)), input_(std::move(input)), witness_(std::move(w)), report_(std::move(report)) {}
    PreparedNativeHomotopySequence plan_; std::shared_ptr<const DerivedNativeCylinderChain> input_;
    topology::SequencedHomotopyWitness witness_; NativeHomotopySequenceProvenance report_;
};
inline NativeSequencedHomotopyWitness PreparedNativeHomotopySequence::witness(DerivedNativeCylinderChain input,
    NativeHomotopySequenceAvailability availability, runtime::ExecutionLease lease) const {
    using namespace native_homotopy_sequence_detail; const auto pin = state_; auto a = availability; lease.check_cancelled(); available(*pin,a);
    require(native_homotopy_sequence_detail::domain(*pin).same_owner(input.plan()), "sequence witness needs its actual current domain chain");
    const auto h = pin->sequence.current(); const auto carrier = h.carrier();
    const auto extra = add(h.entry_envelope(), mul(add(topology::cylinder_carrier_detail::population(carrier.cylinder()), algebra::owned_gluing_detail::words(h.target())), 32));
    const auto required = requirements(pin->retained, add(pin->envelope, add(extra,mul(input.chain().values().size(),8))));
    runtime::admit_execution(required,lease.policy()); Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();}); guard.entries(required.rational_slots);
    for (std::size_t i = 0; i < pin->retained; ++i) guard.step();
    auto out = pin->sequence.witness(input.chain(),guard); lease.check_cancelled();
    return {PreparedNativeHomotopySequence(pin),std::make_shared<const DerivedNativeCylinderChain>(std::move(input)),std::move(out),
        report(*pin,NativeRelationalHomotopyAction::Witness,a,lease,guard,required)};
}
} // namespace rfc::inference
