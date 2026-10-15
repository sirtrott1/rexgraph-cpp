#pragma once
#include "rfc/inference/native_covering_homotopy.hpp"
#include "rfc/topology/covering_homotopy_base_change.hpp"

namespace rfc::inference {
struct NativeCoveringHomotopyBaseChangeRequest {
    std::uint64_t key{}, revision{};
    NativeGluingSourceMap comparison;
    // Explicit identities for C->E/C->D, start X->E/X->D, finish X->E/X->D.
    std::array<std::uint64_t, 6> cone_maps;
};
struct NativeCoveringHomotopyBaseChangeAvailability {
    NativeCoveringHomotopyAvailability original, changed;
    NativeCoveringPullbackAvailability pullback;
};
struct NativeCoveringHomotopyBaseChangeProvenance {
    std::uint64_t declaration{}, revision{};
    std::array<std::uint64_t, 3> constructors, revisions;
    // Original six roles, changed six roles, actual pullback E/B/P/D roles.
    // Aliasing keeps all sixteen independent obligations.
    std::array<std::uint64_t, 16> sources, namespaces, sequences;
    std::array<std::uint64_t, 7> maps;
    NativeCoveringHomotopyBaseChangeAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
// Owning native cone receipt. Numeric mediation uses the actual prepared
// pullback; this view never creates a native source or a synthetic publication.
struct NativeCoveringHomotopyBaseChangeCone {
    NativeCoveringCone request;
    runtime::WorkAvailability source_availability;
};
namespace native_covering_base_change_detail {
using native_covering_homotopy_detail::Pin;
using native_covering_homotopy_detail::Guard;
using native_covering_homotopy_detail::add;
using native_covering_homotopy_detail::mul;
using native_covering_homotopy_detail::require;
using native_product_detail::requirements;
inline std::array<Pin, 16> pins(const PreparedNativeCoveringHomotopy& a, const PreparedNativeCoveringHomotopy& b,
    const PreparedNativeCoveringPullback& square) {
    const auto x = native_covering_homotopy_detail::pins(a.original(), a.covering());
    const auto y = native_covering_homotopy_detail::pins(b.original(), b.covering());
    const auto p = square.original(), q = square.pulled();
    return {x[0], x[1], x[2], x[3], x[4], x[5], y[0], y[1], y[2], y[3], y[4], y[5],
        p.anchor(local::CoveringChannel::Cover), p.anchor(local::CoveringChannel::Base),
        q.anchor(local::CoveringChannel::Cover), q.anchor(local::CoveringChannel::Base)};
}
inline std::array<std::uint64_t, 16> sources(const PreparedNativeCoveringHomotopy& a, const PreparedNativeCoveringHomotopy& b,
    const PreparedNativeCoveringPullback& square) {
    const auto x = native_covering_homotopy_detail::sources(a.original(), a.covering());
    const auto y = native_covering_homotopy_detail::sources(b.original(), b.covering());
    const auto p = square.original(), q = square.pulled();
    return {x[0], x[1], x[2], x[3], x[4], x[5], y[0], y[1], y[2], y[3], y[4], y[5],
        p.declaration().projection.from, p.declaration().projection.to, q.declaration().projection.from, q.declaration().projection.to};
}
inline void available(const PreparedNativeCoveringHomotopy& a, const PreparedNativeCoveringHomotopy& b,
    const PreparedNativeCoveringPullback& square, NativeCoveringHomotopyBaseChangeAvailability current) {
    for (const auto& item : {std::pair{a, current.original}, std::pair{b, current.changed}}) {
        native_covering_homotopy_detail::available(item.first.original(), item.first.covering(), item.first.preparation_availability());
        native_covering_homotopy_detail::available(item.first.original(), item.first.covering(), item.second);
    }
    for (const auto cut : {square.preparation_availability(), current.pullback}) {
        const auto p = square.original(), q = square.pulled();
        for (const auto& item : {std::pair{p, cut.original}, std::pair{q, cut.pulled}})
            for (const auto a0 : {item.first.preparation_availability(), item.second}) {
                runtime::admit_work_availability(item.first.anchor(local::CoveringChannel::Cover).source_namespace(), item.first.anchor(local::CoveringChannel::Cover).sequence(), a0.cover);
                runtime::admit_work_availability(item.first.anchor(local::CoveringChannel::Base).source_namespace(), item.first.anchor(local::CoveringChannel::Base).sequence(), a0.base);
            }
    }
}
inline std::size_t map_words(const NativeGluingSourceMap& m) { return add(3, add(mul(m.cells.size(), 4), mul(m.occurrences.size(), 4))); }
inline NativeGluingSourceMap named(const core::RelationalComplex& a, const core::RelationalComplex& b,
    const algebra::RetainedSourceMap& m, std::uint64_t key, std::uint64_t from, std::uint64_t to, Guard& guard) {
    NativeGluingSourceMap out{key, from, to, {}, {}};
    for (std::size_t k = 0; k < m.cells.size(); ++k) {
        const auto g = topology::owned_cylinder_detail::grade(k); const auto& x = a.grade(g); const auto& y = b.grade(g);
        for (std::size_t i = 0; i < m.cells[k].size(); ++i) { guard.step(); out.cells.push_back({storage::stable_cell_key(a, {g, static_cast<std::uint32_t>(i)}), storage::stable_cell_key(b, {g, m.cells[k][i]})}); }
        for (std::size_t i = 0; i < m.occurrences[k].size(); ++i) { guard.step(); out.occurrences.push_back({{g, x.occurrence_identities[i]}, {g, y.occurrence_identities.at(m.occurrences[k][i])}}); }
    }
    return out;
}
struct State {
    PreparedNativeCoveringHomotopy original, changed;
    PreparedNativeCoveringPullback pullback;
    NativeCoveringHomotopyBaseChangeRequest request;
    topology::OwnedCoveringHomotopyBaseChange numeric;
    std::array<NativeCoveringHomotopyBaseChangeCone, 3> cones;
    NativeCoveringHomotopyBaseChangeAvailability preparation;
    std::size_t steps{}, retained{}, envelope{};
    std::string policy;
    bool fallback{};
};
inline numeric::ExactActionLimits limits(const State& s, const runtime::ExecutionLease& lease) {
    const auto d = s.original.domain(); const auto p = s.pullback.original(), q = s.pullback.pulled();
    return native_product_detail::limits(lease, pins(s.original, s.changed, s.pullback),
        std::min({d.declaration().factor_dimension_limit, p.declaration().dimension_limit, q.declaration().dimension_limit}));
}
inline NativeCoveringHomotopyBaseChangeProvenance report(const State& s, NativeCoveringHomotopyBaseChangeAvailability a,
    const runtime::ExecutionLease& lease, const Guard& guard, runtime::ExecutionRequirements required) {
    const auto p = pins(s.original, s.changed, s.pullback); std::array<std::uint64_t, 16> ns{}, seq{};
    for (std::size_t i = 0; i < p.size(); ++i) { ns[i] = p[i].source_namespace(); seq[i] = p[i].sequence(); }
    return {s.request.key, s.request.revision,
        {s.original.declaration().key, s.changed.declaration().key, s.pullback.declaration().key},
        {s.original.declaration().revision, s.changed.declaration().revision, s.pullback.declaration().revision},
        sources(s.original, s.changed, s.pullback), ns, seq,
        {s.request.comparison.key, s.request.cone_maps[0], s.request.cone_maps[1], s.request.cone_maps[2], s.request.cone_maps[3], s.request.cone_maps[4], s.request.cone_maps[5]},
        s.preparation, a, required, guard.limits(), s.steps, guard.steps(), s.policy, lease.policy().name,
        s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
} // namespace native_covering_base_change_detail
class PreparedNativeCoveringHomotopyBaseChange;
class NativeCoveringHomotopyBaseChangeWitness;
class PreparedNativeCoveringHomotopyBaseChange {
public:
    static PreparedNativeCoveringHomotopyBaseChange prepare(PreparedNativeCoveringHomotopy original,
        PreparedNativeCoveringHomotopy changed, PreparedNativeCoveringPullback pullback,
        NativeCoveringHomotopyBaseChangeRequest input, NativeCoveringHomotopyBaseChangeAvailability a,
        runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_covering_base_change_detail; auto request = input; lease.check_cancelled(); available(original, changed, pullback, a);
        require(request.key && request.revision, "native covering base change needs its identity and revision");
        require(original.covering().same_owner(pullback.original()) && changed.covering().same_owner(pullback.pulled()), "native base change needs its actual covering owners");
        require(original.domain().same_owner(changed.domain()), "native base change needs its actual common domain");
        const auto p = pins(original, changed, pullback); const auto ids = sources(original, changed, pullback);
        require(ids[2] == ids[8] && p[2].endpoint() == p[8].endpoint() && p[2].same_execution_authority(p[8]),
            "native base change needs its actual common scalar carrier publication");
        for (std::size_t i = 0; i < ids.size(); ++i) {
            require(ids[i], "base change omits a source identity");
            for (std::size_t j = 0; j < i; ++j) if (ids[i] == ids[j]) require(p[i].endpoint() == p[j].endpoint() && p[i].same_execution_authority(p[j]), "base change aliases distinct native publications or authorities");
        }
        require(request.comparison.from == ids[3] && request.comparison.to == ids[9], "base-change comparison names a foreign target");
        std::set<std::uint64_t> keys{request.comparison.key}; require(request.comparison.key, "base change omits its comparison identity");
        for (const auto key : request.cone_maps) require(key && keys.insert(key).second, "base-change cone maps need distinct identities");
        const auto old_h = original.original(), new_h = changed.original(); const auto op = original.covering(), np = changed.covering();
        for (const auto key : keys)
            for (const auto old : {old_h.declaration().cylinder_target.key, old_h.declaration().first.key, old_h.declaration().second.key,
                new_h.declaration().cylinder_target.key, new_h.declaration().first.key, new_h.declaration().second.key,
                original.declaration().base_map.key, original.declaration().initial.key, changed.declaration().base_map.key, changed.declaration().initial.key,
                op.declaration().projection.key, np.declaration().projection.key, pullback.declaration().into_cover.key, pullback.declaration().base_map.key})
                require(key != old, "base change reuses an existing map identity");
        auto n = add(native_covering_homotopy_detail::words(old_h, op, original.declaration()),
            native_covering_homotopy_detail::words(new_h, np, changed.declaration()));
        n = add(n, add(8, add(map_words(request.comparison), add(map_words(pullback.declaration().into_cover), map_words(pullback.declaration().base_map)))));
        const auto envelope = add(1, mul(n, 128)); runtime::admit_execution(requirements(n, envelope), lease.policy());
        const auto domain = original.domain(); Guard guard(native_product_detail::limits(lease, p,
            std::min({domain.declaration().factor_dimension_limit, op.declaration().dimension_limit, np.declaration().dimension_limit})), [&lease]{ lease.check_cancelled(); });
        guard.entries(envelope);
        const std::array<const NativeGluingSourceMap*, 14> inherited{
            &old_h.declaration().cylinder_target, &old_h.declaration().first, &old_h.declaration().second,
            &new_h.declaration().cylinder_target, &new_h.declaration().first, &new_h.declaration().second,
            &original.declaration().base_map, &original.declaration().initial, &changed.declaration().base_map, &changed.declaration().initial,
            &op.declaration().projection, &np.declaration().projection, &pullback.declaration().into_cover, &pullback.declaration().base_map};
        for (std::size_t i = 0; i < inherited.size(); ++i) for (std::size_t j = 0; j < i; ++j) {
            guard.step(); const auto& x = *inherited[i]; const auto& y = *inherited[j];
            if (x.key != y.key) continue;
            require(x.from == y.from && x.to == y.to, "base change aliases distinct inherited map domains");
            const auto from = static_cast<std::size_t>(std::find(ids.begin(), ids.end(), x.from) - ids.begin());
            const auto to = static_cast<std::size_t>(std::find(ids.begin(), ids.end(), x.to) - ids.begin());
            require(from < ids.size() && to < ids.size(), "base change omits an inherited map carrier");
            const auto a0 = native_gluing_detail::indexed(p[from].endpoint()->source, p[to].endpoint()->source, x.cells, x.occurrences, guard);
            const auto b0 = native_gluing_detail::indexed(p[from].endpoint()->source, p[to].endpoint()->source, y.cells, y.occurrences, guard);
            topology::covering_homotopy_detail::equal(a0, b0, guard);
        }
        auto comparison = native_gluing_detail::indexed(p[3].endpoint()->source, p[9].endpoint()->source, request.comparison.cells, request.comparison.occurrences, guard);
        auto numeric = topology::OwnedCoveringHomotopyBaseChange::prepare(original.numeric_plan(), changed.numeric_plan(), pullback.numeric_plan(),
            {request.key, request.revision, std::move(comparison)}, guard);
        const auto cone = [&](std::size_t j) {
            const auto pin = j ? p[0] : p[2]; const auto source = j ? ids[0] : ids[2];
            const auto& mediator = numeric.certificate().mediators[j];
            return NativeCoveringHomotopyBaseChangeCone{{pin, source,
                named(pin.endpoint()->source, p[4].endpoint()->source, mediator.cone_map(), request.cone_maps[2*j], source, ids[4], guard),
                named(pin.endpoint()->source, p[11].endpoint()->source, mediator.cone_map(true), request.cone_maps[2*j+1], source, ids[11], guard)},
                j ? a.original.original.cylinder.left : a.original.original.carrier};
        };
        std::array<NativeCoveringHomotopyBaseChangeCone, 3> cones{cone(0), cone(1), cone(2)};
        for (const auto& c : cones) n = add(n, add(2, add(map_words(c.request.into_cover), map_words(c.request.into_base))));
        const auto retained = add(n, numeric.retained_words()), complete = add(add(envelope, mul(n, 32)), numeric.entry_envelope());
        guard.entries(complete); runtime::admit_execution(requirements(retained, complete), lease.policy()); lease.check_cancelled();
        return PreparedNativeCoveringHomotopyBaseChange(std::make_shared<const State>(State{std::move(original), std::move(changed), std::move(pullback),
            std::move(request), std::move(numeric), std::move(cones), a, guard.steps(), retained, complete, lease.policy().name,
            lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeCoveringHomotopy original() const { return state_->original; }
    PreparedNativeCoveringHomotopy changed() const { return state_->changed; }
    PreparedNativeCoveringPullback pullback() const { return state_->pullback; }
    PreparedNativeCylinder domain() const { return state_->original.domain(); }
    topology::OwnedCoveringHomotopyBaseChange numeric_plan() const { return state_->numeric; }
    const NativeCoveringHomotopyBaseChangeRequest& declaration() const & { return state_->request; }
    const NativeCoveringHomotopyBaseChangeRequest& declaration() const && = delete;
    const std::array<NativeCoveringHomotopyBaseChangeCone, 3>& cones() const & { return state_->cones; }
    const std::array<NativeCoveringHomotopyBaseChangeCone, 3>& cones() const && = delete;
    NativeCoveringHomotopyBaseChangeAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeCoveringHomotopyBaseChange& other) const { return state_ == other.state_; }
    NativeCoveringHomotopyBaseChangeWitness witness(DerivedNativeCylinderChain, NativeCoveringHomotopyBaseChangeAvailability,
        runtime::ExecutionLease = runtime::ExecutionLease{}) const;
private:
    explicit PreparedNativeCoveringHomotopyBaseChange(std::shared_ptr<const native_covering_base_change_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_covering_base_change_detail::State> state_;
};
class NativeCoveringHomotopyBaseChangeWitness {
public:
    PreparedNativeCoveringHomotopyBaseChange plan() const { return plan_; }
    std::shared_ptr<const DerivedNativeCylinderChain> input() const { return input_; }
    const topology::CoveringHomotopyBaseChangeWitness& witness() const & { return witness_; }
    const topology::CoveringHomotopyBaseChangeWitness& witness() const && = delete;
    const NativeCoveringHomotopyBaseChangeProvenance& provenance() const & { return report_; }
    const NativeCoveringHomotopyBaseChangeProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeCoveringHomotopyBaseChange;
    NativeCoveringHomotopyBaseChangeWitness(PreparedNativeCoveringHomotopyBaseChange plan, std::shared_ptr<const DerivedNativeCylinderChain> input,
        topology::CoveringHomotopyBaseChangeWitness witness, NativeCoveringHomotopyBaseChangeProvenance report)
        : plan_(std::move(plan)), input_(std::move(input)), witness_(std::move(witness)), report_(std::move(report)) {}
    PreparedNativeCoveringHomotopyBaseChange plan_; std::shared_ptr<const DerivedNativeCylinderChain> input_;
    topology::CoveringHomotopyBaseChangeWitness witness_; NativeCoveringHomotopyBaseChangeProvenance report_;
};
inline NativeCoveringHomotopyBaseChangeWitness PreparedNativeCoveringHomotopyBaseChange::witness(DerivedNativeCylinderChain input,
    NativeCoveringHomotopyBaseChangeAvailability a, runtime::ExecutionLease lease) const {
    using namespace native_covering_base_change_detail; const auto pin = state_; lease.check_cancelled();
    available(pin->original, pin->changed, pin->pullback, pin->preparation); available(pin->original, pin->changed, pin->pullback, a);
    require(pin->original.domain().same_owner(input.plan()), "base-change witness needs its actual cylinder input");
    const auto n = pin->numeric.original().numeric_plan(), np = pin->numeric.changed().numeric_plan();
    const auto extra = mul(add(topology::cylinder_carrier_detail::population(pin->original.domain().numeric_plan()),
        add(algebra::owned_gluing_detail::words(n.target()), algebra::owned_gluing_detail::words(np.target()))), 128);
    const auto required = requirements(pin->retained, add(pin->envelope, add(extra, mul(input.chain().values().size(), 32))));
    runtime::admit_execution(required, lease.policy()); Guard guard(limits(*pin, lease), [&lease]{ lease.check_cancelled(); }); guard.entries(required.rational_slots);
    for (std::size_t i = 0; i < pin->retained; ++i) guard.step();
    auto out = pin->numeric.witness(input.chain(), guard); lease.check_cancelled();
    return {PreparedNativeCoveringHomotopyBaseChange(pin), std::make_shared<const DerivedNativeCylinderChain>(std::move(input)), std::move(out), report(*pin, a, lease, guard, required)};
}
} // namespace rfc::inference
