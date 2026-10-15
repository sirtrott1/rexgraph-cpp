#pragma once
#include "rfc/local/covering_pullback.hpp"
#include "rfc/topology/covering_homotopy.hpp"

namespace rfc::topology {
struct CoveringHomotopyBaseChangeDeclaration {
    std::uint64_t key{}, revision{};
    // Complete Y -> Z comparison; no comparison is inferred from equal values.
    algebra::RetainedSourceMap comparison;
};
struct CoveringHomotopyBaseChangeCertificate {
    // C, start X, finish X. Each mediator owns the actual pullback and cone.
    std::array<local::OwnedCoveringPullbackMediator, 3> mediators;
};
namespace covering_base_change_detail {
using covering_homotopy_detail::Guard;
using covering_homotopy_detail::add;
using covering_homotopy_detail::mul;
using covering_homotopy_detail::grade;
using covering_homotopy_detail::require;
using covering_homotopy_detail::Map;
using covering_homotopy_detail::compose;
using covering_homotopy_detail::equal;
using owned_homotopy_detail::Values;
using owned_homotopy_detail::Q;
inline std::size_t words(const CoveringHomotopyBaseChangeDeclaration& d) {
    return add(2, algebra::owned_gluing_detail::words(d.comparison));
}
inline std::size_t words(const CoveringHomotopyBaseChangeCertificate& c) {
    std::size_t n{};
    for (const auto& m : c.mediators) {
        n = add(n, algebra::owned_gluing_detail::words(m.source()));
        for (const auto* f : {&m.map(), &m.cone_map(), &m.cone_map(true)}) n = add(n, algebra::owned_gluing_detail::words(*f));
    }
    return n;
}
inline void common_carrier(const OwnedCylinderCarrier& a, const OwnedCylinderCarrier& b, Guard& guard) {
    require(a.cylinder().same_owner(b.cylinder()), "base change needs the actual common cylinder");
    const auto& x = a.declaration(); const auto& y = b.declaration();
    require(x.types.size() == y.types.size() && x.cells.size() == y.cells.size() && x.occurrences.size() == y.occurrences.size(),
        "base change changes its complete cylinder interpretation");
    std::map<std::uint64_t, std::optional<core::RelationTypeId>> types;
    for (const auto& t : y.types) { guard.step(); types.emplace(t.product_type, t.carrier_type); }
    for (const auto& t : x.types) { guard.step(); require(types.at(t.product_type) == t.carrier_type, "base change changes an unused type interpretation"); }
    for (const auto& c : x.cells) { guard.step(); require(b.image(c.from) == c.to, "base change changes a product cell interpretation"); }
    for (const auto& f : x.occurrences) { guard.step(); require(b.image(f.from) == f.to, "base change changes an original flag interpretation"); }
}
struct State {
    OwnedCoveringHomotopy original, changed;
    local::OwnedCoveringPullback pullback;
    CoveringHomotopyBaseChangeDeclaration declaration;
    CoveringHomotopyBaseChangeCertificate certificate;
    std::size_t retained{}, envelope{};
};
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.original.validate(guard); s.changed.validate(guard); s.pullback.validate(guard);
    for (std::size_t i = 0; i < add(words(s.declaration), words(s.certificate)); ++i) guard.step();
}
inline Values project(const Values& input, core::Grade g, const Map& map, Guard& guard, std::size_t resident) {
    Values out;
    for (const auto& [i, q] : input) {
        guard.step(); guard.value(q); guard.entries(add(resident, add(input.size(), add(out.size(), 1))));
        auto& v = out.try_emplace(map.cells.at(g.value).at(i), Q{0}).first->second;
        v = guard.add(v, q);
    }
    return out;
}
} // namespace covering_base_change_detail
class OwnedCoveringHomotopyBaseChange;
class CoveringHomotopyBaseChangeWitness;
// Certifies two supplied lifts through a supplied complete covering pullback.
// All equations are full source-map equations before chain consequences.
class OwnedCoveringHomotopyBaseChange {
public:
    static OwnedCoveringHomotopyBaseChange prepare(OwnedCoveringHomotopy original, OwnedCoveringHomotopy changed,
        local::OwnedCoveringPullback pullback, CoveringHomotopyBaseChangeDeclaration input, numeric::ExactActionGuard& guard) {
        using namespace covering_base_change_detail; auto declaration = input;
        require(declaration.key && declaration.revision, "covering base change needs its identity and revision");
        require(original.covering().same_owner(pullback.original()) && changed.covering().same_owner(pullback.pulled()),
            "base change needs the actual original and pulled covering owners");
        const auto n = add(words(declaration), add(original.retained_words(), add(changed.retained_words(), pullback.retained_words())));
        const auto envelope = add(add(original.entry_envelope(), changed.entry_envelope()), add(pullback.entry_envelope(), mul(n, 128)));
        guard.entries(envelope); original.validate(guard); changed.validate(guard); pullback.validate(guard);
        const auto h = original.original(), hp = changed.original(); const auto c = h.carrier(), cp = hp.carrier();
        common_carrier(c, cp, guard);
        algebra::owned_gluing_detail::map(h.target(), hp.target(), declaration.comparison, false, guard);
        const auto& hd = h.declaration(); const auto& hpd = hp.declaration();
        equal(compose(hd.cylinder_target, declaration.comparison, guard), hpd.cylinder_target, guard);
        equal(compose(hd.first, declaration.comparison, guard), hpd.first, guard);
        equal(compose(hd.second, declaration.comparison, guard), hpd.second, guard);
        equal(compose(compose(declaration.comparison, changed.declaration().base_map, guard), pullback.map(true), guard), original.declaration().base_map, guard);
        const auto l = original.numeric_plan(), lp = changed.numeric_plan(); const auto& ld = l.declaration(); const auto& lpd = lp.declaration();
        const auto cylinder = c.cylinder();
        std::array<local::OwnedCoveringPullbackMediator, 3> mediators{
            pullback.mediate(c.carrier(), ld.cylinder_target, compose(hpd.cylinder_target, changed.declaration().base_map, guard), guard),
            pullback.mediate(cylinder.source(), ld.first, compose(hpd.first, changed.declaration().base_map, guard), guard),
            pullback.mediate(cylinder.source(), ld.second, compose(hpd.second, changed.declaration().base_map, guard), guard)};
        equal(mediators[0].map(), lpd.cylinder_target, guard);
        equal(mediators[1].map(), lpd.first, guard); equal(mediators[2].map(), lpd.second, guard);
        CoveringHomotopyBaseChangeCertificate certificate{std::move(mediators)};
        const auto retained = add(n, words(certificate)), complete = add(envelope, mul(words(certificate), 32)); guard.entries(complete);
        return OwnedCoveringHomotopyBaseChange(std::make_shared<const State>(State{std::move(original), std::move(changed),
            std::move(pullback), std::move(declaration), std::move(certificate), retained, complete}));
    }
    OwnedCoveringHomotopy original() const { return state_->original; }
    OwnedCoveringHomotopy changed() const { return state_->changed; }
    local::OwnedCoveringPullback pullback() const { return state_->pullback; }
    const CoveringHomotopyBaseChangeDeclaration& declaration() const & { return state_->declaration; }
    const CoveringHomotopyBaseChangeDeclaration& declaration() const && = delete;
    const CoveringHomotopyBaseChangeCertificate& certificate() const & { return state_->certificate; }
    const CoveringHomotopyBaseChangeCertificate& certificate() const && = delete;
    bool same_owner(const OwnedCoveringHomotopyBaseChange& other) const { return state_ == other.state_; }
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; covering_base_change_detail::validate(*pin, guard); }
    CoveringHomotopyBaseChangeWitness witness(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
private:
    explicit OwnedCoveringHomotopyBaseChange(std::shared_ptr<const covering_base_change_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const covering_base_change_detail::State> state_;
};
class CoveringHomotopyBaseChangeWitness {
public:
    OwnedCoveringHomotopyBaseChange plan() const { return plan_; }
    const CoveringHomotopyWitness& original() const & { return original_; }
    const CoveringHomotopyWitness& original() const && = delete;
    const CoveringHomotopyWitness& changed() const & { return changed_; }
    const CoveringHomotopyWitness& changed() const && = delete;
    // start, finish, K, dK, Kd, finish-start, prism residual.
    const std::array<owned_homotopy_detail::Values, 7>& projected() const & { return projected_; }
    const std::array<owned_homotopy_detail::Values, 7>& projected() const && = delete;
    const std::array<owned_homotopy_detail::Values, 7>& residuals() const & { return residuals_; }
    const std::array<owned_homotopy_detail::Values, 7>& residuals() const && = delete;
private:
    friend class OwnedCoveringHomotopyBaseChange;
    CoveringHomotopyBaseChangeWitness(OwnedCoveringHomotopyBaseChange plan, CoveringHomotopyWitness original,
        CoveringHomotopyWitness changed, std::array<owned_homotopy_detail::Values, 7> projected,
        std::array<owned_homotopy_detail::Values, 7> residuals)
        : plan_(std::move(plan)), original_(std::move(original)), changed_(std::move(changed)),
          projected_(std::move(projected)), residuals_(std::move(residuals)) {}
    OwnedCoveringHomotopyBaseChange plan_; CoveringHomotopyWitness original_, changed_;
    std::array<owned_homotopy_detail::Values, 7> projected_, residuals_;
};
inline CoveringHomotopyBaseChangeWitness OwnedCoveringHomotopyBaseChange::witness(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    using namespace covering_base_change_detail; const auto pin = state_; covering_base_change_detail::validate(*pin, guard);
    auto original = pin->original.witness(input, guard), changed = pin->changed.witness(std::move(input), guard);
    const auto& a = original.witness(); const auto& b = changed.witness();
    const std::array<const OwnedHomotopyChain*, 7> x{&a.first(), &a.second(), &a.homotopy(), &a.boundary_homotopy(),
        &a.homotopy_boundary(), &a.endpoint_difference(), &a.residual()};
    const std::array<const OwnedHomotopyChain*, 7> y{&b.first(), &b.second(), &b.homotopy(), &b.boundary_homotopy(),
        &b.homotopy_boundary(), &b.endpoint_difference(), &b.residual()};
    std::array<Values, 7> projected, residuals; std::size_t resident = pin->envelope;
    for (std::size_t j = 0; j < x.size(); ++j) {
        projected[j] = project(y[j]->values(), y[j]->grade(), pin->pullback.map(), guard, resident);
        resident = add(resident, projected[j].size());
        residuals[j] = owned_homotopy_detail::combine(projected[j], x[j]->values(), Q{-1}, guard, resident);
        resident = add(resident, residuals[j].size());
        for (const auto& [i, q] : residuals[j]) { (void)i; guard.step(); require(q == 0, "covering base-change chain naturality failed"); }
    }
    return {OwnedCoveringHomotopyBaseChange(pin), std::move(original), std::move(changed), std::move(projected), std::move(residuals)};
}
} // namespace rfc::topology
