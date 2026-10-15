#pragma once
#include "rfc/local/owned_covering.hpp"
#include "rfc/topology/owned_relational_homotopy.hpp"

namespace rfc::topology {
struct CoveringHomotopyDeclaration {
    std::uint64_t key{}, revision{};
    // Y -> B and X -> E. Even an identity comparison is supplied explicitly.
    algebra::RetainedSourceMap base_map, initial;
};
struct CoveringHomotopyRoute {
    core::CellId from;
    core::Grade upper_grade;
    std::size_t occurrence{}, lifted_occurrence{};
    bool upward{};
};
struct CoveringHomotopyCertificate {
    algebra::RetainedSourceMap projected_homotopy, projected_first, projected_second;
    // First-discovery forest rooted at every cell of the supplied start map.
    // The complete lifted morphism additionally checks all remaining flags.
    std::vector<CoveringHomotopyRoute> routes;
    std::size_t seeds{};
};
namespace covering_homotopy_detail {
using owned_homotopy_detail::Guard;
using owned_homotopy_detail::add;
using owned_homotopy_detail::mul;
using owned_homotopy_detail::grade;
using owned_homotopy_detail::require;
using Map = algebra::RetainedSourceMap;
inline std::size_t words(const CoveringHomotopyDeclaration& d) {
    return add(2, add(algebra::owned_gluing_detail::words(d.base_map), algebra::owned_gluing_detail::words(d.initial)));
}
inline std::size_t words(const CoveringHomotopyCertificate& c) {
    return add(1, add(mul(c.routes.size(), 7), add(algebra::owned_gluing_detail::words(c.projected_homotopy),
        add(algebra::owned_gluing_detail::words(c.projected_first), algebra::owned_gluing_detail::words(c.projected_second)))));
}
inline Map compose(const Map& a, const Map& b, Guard& guard) {
    Map out; out.cells.resize(a.cells.size()); out.occurrences.resize(a.occurrences.size());
    for (std::size_t k = 0; k < a.cells.size(); ++k) {
        for (const auto i : a.cells[k]) { guard.step(); out.cells[k].push_back(b.cells.at(k).at(i)); }
        for (const auto p : a.occurrences[k]) { guard.step(); out.occurrences[k].push_back(b.occurrences.at(k).at(p)); }
    }
    return out;
}
inline void equal(const Map& a, const Map& b, Guard& guard) {
    require(a.cells.size() == b.cells.size() && a.occurrences.size() == b.occurrences.size(), "covering lift changes a complete map inventory");
    for (std::size_t k = 0; k < a.cells.size(); ++k) {
        require(a.cells[k].size() == b.cells[k].size() && a.occurrences[k].size() == b.occurrences[k].size(), "covering lift changes a map row");
        for (std::size_t i = 0; i < a.cells[k].size(); ++i) { guard.step(); require(a.cells[k][i] == b.cells[k][i], "covering lift cell square does not commute"); }
        for (std::size_t p = 0; p < a.occurrences[k].size(); ++p) { guard.step(); require(a.occurrences[k][p] == b.occurrences[k][p], "covering lift original occurrence square does not commute"); }
    }
}
struct State {
    OwnedRelationalHomotopy original;
    local::OwnedRelationalCovering covering;
    CoveringHomotopyDeclaration declaration;
    CoveringHomotopyCertificate certificate;
    OwnedRelationalHomotopy lifted;
    std::size_t retained{}, envelope{};
};
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.original.validate(guard); s.covering.validate(guard); s.lifted.validate(guard);
    for (std::size_t i = 0; i < add(words(s.declaration), words(s.certificate)); ++i) guard.step();
}
} // namespace covering_homotopy_detail
class OwnedCoveringHomotopy;
class CoveringHomotopyWitness;
class LiftedHomotopyChain {
public:
    OwnedCoveringHomotopy plan() const;
    const OwnedHomotopyChain& chain() const & { return chain_; }
    const OwnedHomotopyChain& chain() const && = delete;
    std::shared_ptr<const LiftedHomotopyChain> input() const { return input_; }
private:
    friend class OwnedCoveringHomotopy;
    LiftedHomotopyChain(std::shared_ptr<const covering_homotopy_detail::State> owner, OwnedHomotopyChain chain,
        std::shared_ptr<const LiftedHomotopyChain> input = {}) : owner_(std::move(owner)), chain_(std::move(chain)), input_(std::move(input)) {}
    std::shared_ptr<const covering_homotopy_detail::State> owner_; OwnedHomotopyChain chain_;
    std::shared_ptr<const LiftedHomotopyChain> input_;
};
// Exact finite incidence lifting. No homotopy is solved, and no sum-over-sheets
// transfer is used. Obstructed incidence cycles reject before publication.
class OwnedCoveringHomotopy {
public:
    static OwnedCoveringHomotopy prepare(OwnedRelationalHomotopy original, local::OwnedRelationalCovering covering,
        CoveringHomotopyDeclaration input, numeric::ExactActionGuard& guard) {
        using namespace covering_homotopy_detail; auto declaration = input;
        require(declaration.key && declaration.revision, "covering homotopy needs its identity and revision");
        const auto n = add(original.retained_words(), add(covering.retained_words(), words(declaration)));
        const auto envelope = add(original.entry_envelope(), add(covering.entry_envelope(), add(1, mul(n, 128))));
        guard.entries(envelope); original.validate(guard); covering.validate(guard);
        const auto carrier = original.carrier(); const auto cylinder = carrier.cylinder(); const auto& c = carrier.carrier();
        const auto& e = covering.source(); const auto& b = covering.source(local::CoveringChannel::Base);
        algebra::owned_gluing_detail::map(original.target(), b, declaration.base_map, false, guard);
        algebra::owned_gluing_detail::map(cylinder.source(), e, declaration.initial, false, guard);
        CoveringHomotopyCertificate certificate{
            compose(original.declaration().cylinder_target, declaration.base_map, guard),
            compose(original.declaration().first, declaration.base_map, guard),
            compose(original.declaration().second, declaration.base_map, guard), {}, 0};
        equal(compose(declaration.initial, covering.projection(), guard), certificate.projected_first, guard);
        Map lifted; lifted.cells.resize(c.grade_count()); lifted.occurrences.resize(c.grade_count());
        std::vector<std::vector<bool>> seen(c.grade_count()), seen_flags(c.grade_count());
        std::vector<std::vector<std::vector<std::size_t>>> upward(c.grade_count());
        std::vector<core::CellId> queue;
        for (std::size_t k = 0; k < c.grade_count(); ++k) {
            const auto& a = c.grade(grade(k)); guard.dimension(a.cell_count());
            lifted.cells[k].resize(a.cell_count()); lifted.occurrences[k].resize(a.incidence_count());
            seen[k].resize(a.cell_count()); seen_flags[k].resize(a.incidence_count()); upward[k].resize(a.cell_count());
            if (k) for (std::size_t p = 0; p < a.incidence_count(); ++p) { guard.step(); upward[k-1][a.lower_index[p]].push_back(p); }
        }
        const auto assign = [&](core::CellId id, std::uint32_t image) {
            guard.step(); require(image < e.grade(id.grade).cell_count(), "covering homotopy lift cell out of range");
            if (seen[id.grade.value][id.index]) { require(lifted.cells[id.grade.value][id.index] == image, "covering homotopy is obstructed at a shared cell"); return false; }
            seen[id.grade.value][id.index] = true; lifted.cells[id.grade.value][id.index] = image; queue.push_back(id); return true;
        };
        const auto& start = carrier.endpoint(CylinderEndpoint::Start);
        for (std::size_t k = 0; k < start.cells.size(); ++k)
            for (std::size_t i = 0; i < start.cells[k].size(); ++i) {
                require(assign({grade(k), start.cells[k][i]}, declaration.initial.cells[k][i]), "covering lift repeats a cylinder seed"); ++certificate.seeds;
            }
        for (std::size_t position = 0; position < queue.size(); ++position) {
            const auto current = queue[position];
            const auto visit = [&](core::Grade g, std::size_t p, bool up) {
                guard.step(); const auto& a = c.grade(g); const auto& z = e.grade(g);
                const auto q = covering.lift_flag({current.grade, lifted.cells[current.grade.value][current.index]}, g,
                    certificate.projected_homotopy.occurrences[g.value][p], up, guard);
                const core::CellId next{grade(g.value - (up ? 0u : 1u)), up ? a.upper_index[p] : a.lower_index[p]};
                const auto image = up ? z.upper_index[q] : z.lower_index[q];
                // A flag is encountered from both endpoints. Both unique star
                // inverses must agree, even when its realized coefficient is zero.
                if (seen_flags[g.value][p]) require(lifted.occurrences[g.value][p] == q, "covering homotopy is obstructed at an original occurrence");
                seen_flags[g.value][p] = true; lifted.occurrences[g.value][p] = q;
                if (assign(next, image)) certificate.routes.push_back({current, g, p, q, up});
            };
            if (current.grade.value) {
                const auto& a = c.grade(current.grade); const auto [first, last] = a.incidence_range(current.index);
                for (auto p = first; p < last; ++p) visit(current.grade, p, false);
            }
            const auto k = static_cast<std::size_t>(current.grade.value) + 1;
            if (k < c.grade_count()) for (const auto p : upward[current.grade.value][current.index]) visit(grade(k), p, true);
        }
        for (const auto& row : seen) for (const auto reached : row) { guard.step(); require(reached, "covering homotopy has an unseeded cylinder component"); }
        // Full strict-map validation includes unused rules, attaching order,
        // repeated/inactive flags and all applicable chain constraints.
        auto first = compose(start, lifted, guard);
        auto second = compose(carrier.endpoint(CylinderEndpoint::Finish), lifted, guard);
        equal(first, declaration.initial, guard);
        equal(compose(lifted, covering.projection(), guard), certificate.projected_homotopy, guard);
        equal(compose(second, covering.projection(), guard), certificate.projected_second, guard);
        auto h = OwnedRelationalHomotopy::prepare(carrier, e,
            {declaration.key, declaration.revision, std::move(lifted), std::move(first), std::move(second)}, guard);
        const auto retained = add(n, add(words(certificate), h.retained_words()));
        const auto complete = add(envelope, h.entry_envelope()); guard.entries(complete);
        return OwnedCoveringHomotopy(std::make_shared<const State>(State{std::move(original), std::move(covering),
            std::move(declaration), std::move(certificate), std::move(h), retained, complete}));
    }
    OwnedRelationalHomotopy original() const { return state_->original; }
    local::OwnedRelationalCovering covering() const { return state_->covering; }
    OwnedRelationalHomotopy numeric_plan() const { return state_->lifted; }
    const CoveringHomotopyDeclaration& declaration() const & { return state_->declaration; }
    const CoveringHomotopyDeclaration& declaration() const && = delete;
    const CoveringHomotopyCertificate& certificate() const & { return state_->certificate; }
    const CoveringHomotopyCertificate& certificate() const && = delete;
    bool same_owner(const OwnedCoveringHomotopy& other) const { return state_ == other.state_; }
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; covering_homotopy_detail::validate(*pin, guard); }
    LiftedHomotopyChain push(algebra::OwnedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; covering_homotopy_detail::validate(*pin, guard); return {pin, pin->lifted.push(std::move(input), guard)};
    }
    LiftedHomotopyChain endpoint(OwnedCylinderSourceChain input, CylinderEndpoint end, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; covering_homotopy_detail::validate(*pin, guard); return {pin, pin->lifted.endpoint(std::move(input), end, guard)};
    }
    LiftedHomotopyChain homotopy(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; covering_homotopy_detail::validate(*pin, guard); return {pin, pin->lifted.homotopy(std::move(input), guard)};
    }
    LiftedHomotopyChain boundary(LiftedHomotopyChain input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; covering_homotopy_detail::require(pin == input.owner_, "covering homotopy boundary needs its actual output owner");
        covering_homotopy_detail::validate(*pin, guard); auto out = pin->lifted.boundary(input.chain_, guard);
        return {pin, std::move(out), std::make_shared<const LiftedHomotopyChain>(std::move(input))};
    }
    CoveringHomotopyWitness witness(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
private:
    friend class LiftedHomotopyChain;
    explicit OwnedCoveringHomotopy(std::shared_ptr<const covering_homotopy_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const covering_homotopy_detail::State> state_;
};
inline OwnedCoveringHomotopy LiftedHomotopyChain::plan() const { return OwnedCoveringHomotopy(owner_); }
class CoveringHomotopyWitness {
public:
    OwnedCoveringHomotopy plan() const { return plan_; }
    const RelationalHomotopyWitness& witness() const & { return witness_; }
    const RelationalHomotopyWitness& witness() const && = delete;
private:
    friend class OwnedCoveringHomotopy;
    CoveringHomotopyWitness(OwnedCoveringHomotopy plan, RelationalHomotopyWitness witness) : plan_(std::move(plan)), witness_(std::move(witness)) {}
    OwnedCoveringHomotopy plan_; RelationalHomotopyWitness witness_;
};
inline CoveringHomotopyWitness OwnedCoveringHomotopy::witness(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    const auto pin = state_; covering_homotopy_detail::validate(*pin, guard);
    return {OwnedCoveringHomotopy(pin), pin->lifted.witness(std::move(input), guard)};
}
} // namespace rfc::topology
