#pragma once
#include "rfc/topology/owned_relational_homotopy.hpp"

namespace rfc::topology {
struct HomotopyCompositionDeclaration {
    std::uint64_t key{}, revision{};
    // W -> X, the endpoint-fixing interval map, and Y -> Z. Each includes
    // every original occurrence, independently of its realized coefficient.
    algebra::RetainedSourceMap before, interval, after;
};
namespace homotopy_composition_detail {
using owned_homotopy_detail::Guard;
using owned_homotopy_detail::add;
using owned_homotopy_detail::mul;
using owned_homotopy_detail::grade;
using owned_homotopy_detail::require;
using Map = algebra::RetainedSourceMap;
inline std::size_t words(const HomotopyCompositionDeclaration& d) {
    return add(2, add(algebra::owned_gluing_detail::words(d.before),
        add(algebra::owned_gluing_detail::words(d.interval), algebra::owned_gluing_detail::words(d.after))));
}
inline Map compose(const Map& a, const Map& b, Guard& guard) {
    Map out; out.cells.resize(a.cells.size()); out.occurrences.resize(a.occurrences.size());
    for (std::size_t k = 0; k < a.cells.size(); ++k) {
        for (const auto i : a.cells[k]) { guard.step(); out.cells[k].push_back(b.cells.at(k).at(i)); }
        for (const auto p : a.occurrences[k]) { guard.step(); out.occurrences[k].push_back(b.occurrences.at(k).at(p)); }
    }
    return out;
}
inline algebra::ProductCell image(algebra::ProductCell id, const HomotopyCompositionDeclaration& d) {
    return {{id.left.grade, d.before.cells.at(id.left.grade.value).at(id.left.index)},
        {id.right.grade, d.interval.cells.at(id.right.grade.value).at(id.right.index)}};
}
inline Map transport(const OwnedCylinderCarrier& from, const OwnedCylinderCarrier& to,
    const HomotopyCompositionDeclaration& d, Guard& guard) {
    const auto a = from.cylinder(), b = to.cylinder(); const auto ap = a.product(), bp = b.product();
    // Compare complete constructor interpretations, even for unoccupied pairs.
    std::map<std::uint64_t, std::optional<core::RelationTypeId>> at, bt;
    for (const auto& t : from.declaration().types) { guard.step(); at.emplace(t.product_type, t.carrier_type); }
    for (const auto& t : to.declaration().types) { guard.step(); bt.emplace(t.product_type, t.carrier_type); }
    for (const auto& t : ap.declaration().types) {
        bool found = false;
        for (const auto& u : bp.declaration().types) {
            guard.step(); if (t.left != u.left || t.right != u.right) continue;
            require(at.at(t.key) == bt.at(u.key), "homotopy composition changes an unused cylinder type interpretation");
            found = true; break;
        }
        require(found, "homotopy composition omits a declared product type pair");
    }
    std::array<std::vector<std::map<core::OccurrenceKey, std::size_t>>, 2> slots;
    for (std::size_t side = 0; side < 2; ++side) {
        const auto& source = side ? a.interval() : a.source(); slots[side].resize(source.grade_count());
        for (std::size_t k = 0; k < source.grade_count(); ++k) {
            const auto& arena = source.grade(grade(k));
            for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
                guard.step(); slots[side][k].emplace(arena.occurrence_identities[p], p);
            }
        }
    }
    Map out; out.cells.resize(from.carrier().grade_count()); out.occurrences.resize(out.cells.size());
    for (std::size_t k = 0; k < out.cells.size(); ++k) {
        out.cells[k].resize(from.carrier().grade(grade(k)).cell_count());
        out.occurrences[k].resize(from.carrier().grade(grade(k)).incidence_count());
    }
    for (const auto& cell : from.declaration().cells) {
        guard.step(); out.cells[cell.from.grade().value].at(cell.to) = to.image(image(cell.from, d));
    }
    for (const auto& flag : from.declaration().occurrences) {
        guard.step(); const bool right = flag.from.side == algebra::ProductBoundarySide::Right;
        const auto k = right ? flag.from.upper.right.grade.value : flag.from.upper.left.grade.value;
        const auto p = slots[right ? 1 : 0].at(k).at(flag.from.occurrence);
        const auto q = (right ? d.interval : d.before).occurrences.at(k).at(p);
        const auto& target = right ? b.interval() : b.source();
        const CylinderFlag mapped{image(flag.from.upper, d), flag.from.side, target.grade(grade(k)).occurrence_identities.at(q)};
        out.occurrences[flag.from.upper.grade().value].at(flag.to) = to.image(mapped);
    }
    algebra::owned_gluing_detail::map(from.carrier(), to.carrier(), out, false, guard);
    return out;
}
struct State {
    OwnedRelationalHomotopy original;
    OwnedCylinderCarrier domain;
    core::RelationalComplex target;
    HomotopyCompositionDeclaration declaration;
    Map transport;
    OwnedRelationalHomotopy composed;
    std::size_t retained{}, envelope{};
};
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.original.validate(guard); s.domain.validate(guard);
    algebra::owned_gluing_detail::source(s.target, guard); s.composed.validate(guard);
    for (std::size_t n = 0; n < add(words(s.declaration), algebra::owned_gluing_detail::words(s.transport)); ++n) guard.step();
}
} // namespace homotopy_composition_detail
class OwnedHomotopyComposition;
class ComposedHomotopyWitness;
class ComposedHomotopyChain {
public:
    OwnedHomotopyComposition plan() const;
    const OwnedHomotopyChain& chain() const & { return chain_; }
    const OwnedHomotopyChain& chain() const && = delete;
    std::shared_ptr<const ComposedHomotopyChain> input() const { return input_; }
private:
    friend class OwnedHomotopyComposition;
    ComposedHomotopyChain(std::shared_ptr<const homotopy_composition_detail::State> owner, OwnedHomotopyChain chain,
        std::shared_ptr<const ComposedHomotopyChain> input = {}) : owner_(std::move(owner)), chain_(std::move(chain)), input_(std::move(input)) {}
    std::shared_ptr<const homotopy_composition_detail::State> owner_; OwnedHomotopyChain chain_;
    std::shared_ptr<const ComposedHomotopyChain> input_;
};
// For supplied W-cylinder and maps u:W->X, v:Y->Z this constructs v H (u x id).
// Identity pre/postcomposition also requires the full explicit identity maps.
class OwnedHomotopyComposition {
public:
    static OwnedHomotopyComposition prepare(OwnedRelationalHomotopy original, OwnedCylinderCarrier domain,
        core::RelationalComplex target, HomotopyCompositionDeclaration input, numeric::ExactActionGuard& guard) {
        using namespace homotopy_composition_detail; auto declaration = input;
        require(declaration.key && declaration.revision, "homotopy composition needs its identity and revision");
        const auto n = add(original.retained_words(), add(domain.retained_words(),
            add(algebra::owned_gluing_detail::words(target), words(declaration))));
        const auto envelope = add(original.entry_envelope(), add(domain.entry_envelope(), add(1, mul(n, 128))));
        guard.entries(envelope); original.validate(guard); domain.validate(guard); algebra::owned_gluing_detail::source(target, guard);
        algebra::owned_gluing_detail::chain(target, guard, envelope);
        const auto old = original.carrier(); const auto from = domain.cylinder(), to = old.cylinder();
        algebra::owned_gluing_detail::map(from.source(), to.source(), declaration.before, false, guard);
        algebra::owned_gluing_detail::map(from.interval(), to.interval(), declaration.interval, true, guard);
        for (std::size_t k = 0; k < declaration.interval.cells.size(); ++k)
            for (std::size_t i = 0; i < declaration.interval.cells[k].size(); ++i) {
                guard.step(); require(declaration.interval.cells[k][i] == i, "homotopy composition reverses an interval endpoint");
            }
        algebra::owned_gluing_detail::map(original.target(), target, declaration.after, false, guard);
        auto bridge = transport(domain, old, declaration, guard);
        auto h = compose(compose(bridge, original.declaration().cylinder_target, guard), declaration.after, guard);
        auto first = compose(compose(declaration.before, original.declaration().first, guard), declaration.after, guard);
        auto second = compose(compose(declaration.before, original.declaration().second, guard), declaration.after, guard);
        auto composed = OwnedRelationalHomotopy::prepare(domain, target,
            {declaration.key, declaration.revision, std::move(h), std::move(first), std::move(second)}, guard);
        const auto retained = add(n, add(algebra::owned_gluing_detail::words(bridge), composed.retained_words()));
        const auto complete = add(envelope, composed.entry_envelope()); guard.entries(complete);
        return OwnedHomotopyComposition(std::make_shared<const State>(State{std::move(original), std::move(domain), std::move(target),
            std::move(declaration), std::move(bridge), std::move(composed), retained, complete}));
    }
    OwnedRelationalHomotopy original() const { return state_->original; }
    OwnedCylinderCarrier domain() const { return state_->domain; }
    OwnedRelationalHomotopy numeric_plan() const { return state_->composed; }
    const HomotopyCompositionDeclaration& declaration() const & { return state_->declaration; }
    const HomotopyCompositionDeclaration& declaration() const && = delete;
    const algebra::RetainedSourceMap& carrier_map() const & { return state_->transport; }
    const algebra::RetainedSourceMap& carrier_map() const && = delete;
    bool same_owner(const OwnedHomotopyComposition& other) const { return state_ == other.state_; }
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; homotopy_composition_detail::validate(*pin, guard); }
    ComposedHomotopyChain push(algebra::OwnedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_composition_detail::validate(*pin, guard); return {pin, pin->composed.push(std::move(input), guard)};
    }
    ComposedHomotopyChain endpoint(OwnedCylinderSourceChain input, CylinderEndpoint end, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_composition_detail::validate(*pin, guard); return {pin, pin->composed.endpoint(std::move(input), end, guard)};
    }
    ComposedHomotopyChain homotopy(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_composition_detail::validate(*pin, guard); return {pin, pin->composed.homotopy(std::move(input), guard)};
    }
    ComposedHomotopyChain boundary(ComposedHomotopyChain input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_composition_detail::require(pin == input.owner_, "homotopy composition needs its actual output owner");
        homotopy_composition_detail::validate(*pin, guard); auto out = pin->composed.boundary(input.chain_, guard);
        return {pin, std::move(out), std::make_shared<const ComposedHomotopyChain>(std::move(input))};
    }
    ComposedHomotopyWitness witness(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
private:
    friend class ComposedHomotopyChain;
    explicit OwnedHomotopyComposition(std::shared_ptr<const homotopy_composition_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const homotopy_composition_detail::State> state_;
};
inline OwnedHomotopyComposition ComposedHomotopyChain::plan() const { return OwnedHomotopyComposition(owner_); }
class ComposedHomotopyWitness {
public:
    OwnedHomotopyComposition plan() const { return plan_; }
    const RelationalHomotopyWitness& witness() const & { return witness_; }
    const RelationalHomotopyWitness& witness() const && = delete;
private:
    friend class OwnedHomotopyComposition;
    ComposedHomotopyWitness(OwnedHomotopyComposition plan, RelationalHomotopyWitness witness)
        : plan_(std::move(plan)), witness_(std::move(witness)) {}
    OwnedHomotopyComposition plan_; RelationalHomotopyWitness witness_;
};
inline ComposedHomotopyWitness OwnedHomotopyComposition::witness(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    const auto pin = state_; homotopy_composition_detail::validate(*pin, guard);
    return {OwnedHomotopyComposition(pin), pin->composed.witness(std::move(input), guard)};
}
} // namespace rfc::topology
