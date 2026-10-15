#pragma once
#include "rfc/topology/homotopy_composition.hpp"

namespace rfc::topology {
struct HomotopySequenceDeclaration {
    std::uint64_t key{}, revision{};
    std::size_t max_stages{64};
};
struct HomotopySequenceCertificate {
    // Current domain -> root domain, current interval -> root interval,
    // root target -> current target, and current carrier -> root carrier.
    algebra::RetainedSourceMap before, interval, after, carrier;
};
namespace homotopy_sequence_detail {
using homotopy_composition_detail::Guard;
using homotopy_composition_detail::Map;
using homotopy_composition_detail::add;
using homotopy_composition_detail::mul;
using homotopy_composition_detail::require;
using homotopy_composition_detail::compose;
inline std::size_t words(const HomotopySequenceCertificate& c) {
    auto n = std::size_t{0};
    for (const auto* m : {&c.before, &c.interval, &c.after, &c.carrier}) n = add(n, algebra::owned_gluing_detail::words(*m));
    return n;
}
inline void equal(const Map& a, const Map& b, Guard& guard) {
    require(a.cells.size() == b.cells.size() && a.occurrences.size() == b.occurrences.size(), "homotopy association changes map grade inventory");
    const auto rows = [&](const auto& x, const auto& y) {
        for (std::size_t k = 0; k < x.size(); ++k) {
            guard.step(); require(x[k].size() == y[k].size(), "homotopy association changes map domain");
            for (std::size_t i = 0; i < x[k].size(); ++i) { guard.step(); require(x[k][i] == y[k][i], "homotopy association changes an original source image"); }
        }
    };
    rows(a.cells, b.cells); rows(a.occurrences, b.occurrences);
}
struct State {
    OwnedRelationalHomotopy root;
    HomotopySequenceDeclaration declaration;
    std::vector<OwnedHomotopyComposition> stages;
    std::optional<HomotopySequenceCertificate> certificate;
    std::size_t retained{}, envelope{};
};
inline OwnedRelationalHomotopy current(const State& s) { return s.stages.empty() ? s.root : s.stages.back().numeric_plan(); }
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.root.validate(guard);
    for (const auto& stage : s.stages) stage.validate(guard);
    for (std::size_t i = 0; i < add(3, s.certificate ? words(*s.certificate) : 0); ++i) guard.step();
}
// The certificate compares complete cell AND original occurrence maps with the
// flattened root expression. It is not an equality test on chain matrices.
inline void certify(const State& s, Guard& guard) {
    const auto h = current(s); const auto a = h.carrier(), b = s.root.carrier();
    const auto from = a.cylinder(), to = b.cylinder(); const auto& c = *s.certificate;
    algebra::owned_gluing_detail::map(from.source(), to.source(), c.before, false, guard);
    algebra::owned_gluing_detail::map(from.interval(), to.interval(), c.interval, true, guard);
    algebra::owned_gluing_detail::map(s.root.target(), h.target(), c.after, false, guard);
    const HomotopyCompositionDeclaration d{s.declaration.key, s.declaration.revision, c.before, c.interval, c.after};
    equal(c.carrier, homotopy_composition_detail::transport(a, b, d, guard), guard);
    const auto& root = s.root.declaration(); const auto& now = h.declaration();
    equal(now.cylinder_target, compose(compose(c.carrier, root.cylinder_target, guard), c.after, guard), guard);
    equal(now.first, compose(compose(c.before, root.first, guard), c.after, guard), guard);
    equal(now.second, compose(compose(c.before, root.second, guard), c.after, guard), guard);
}
} // namespace homotopy_sequence_detail
class OwnedHomotopySequence;
class SequencedHomotopyWitness;
class SequencedHomotopyChain {
public:
    OwnedHomotopySequence plan() const;
    const OwnedHomotopyChain& chain() const & { return chain_; }
    const OwnedHomotopyChain& chain() const && = delete;
    std::shared_ptr<const SequencedHomotopyChain> input() const { return input_; }
private:
    friend class OwnedHomotopySequence;
    SequencedHomotopyChain(std::shared_ptr<const homotopy_sequence_detail::State> owner, OwnedHomotopyChain chain,
        std::shared_ptr<const SequencedHomotopyChain> input = {}) : owner_(std::move(owner)), chain_(std::move(chain)), input_(std::move(input)) {}
    std::shared_ptr<const homotopy_sequence_detail::State> owner_; OwnedHomotopyChain chain_;
    std::shared_ptr<const SequencedHomotopyChain> input_;
};
// An immutable finite spine. Appending preserves the root and every complete
// intermediate composition; earlier sequence owners remain independently usable.
class OwnedHomotopySequence {
public:
    static OwnedHomotopySequence prepare(OwnedRelationalHomotopy root, HomotopySequenceDeclaration d, numeric::ExactActionGuard& guard) {
        using namespace homotopy_sequence_detail;
        require(d.key && d.revision && d.max_stages, "homotopy sequence needs an identity, revision and finite stage bound");
        const auto n = add(3, root.retained_words()), e = add(3, root.entry_envelope());
        State s{std::move(root), d, {}, {}, n, e}; homotopy_sequence_detail::validate(s, guard);
        return OwnedHomotopySequence(std::make_shared<const State>(std::move(s)));
    }
    OwnedHomotopySequence append(OwnedCylinderCarrier domain, core::RelationalComplex target,
        HomotopyCompositionDeclaration input, numeric::ExactActionGuard& guard) const {
        using namespace homotopy_sequence_detail; const auto pin = state_; auto d = input;
        require(pin->stages.size() < pin->declaration.max_stages, "homotopy sequence stage bound exceeded");
        require(d.key != pin->declaration.key && d.key != pin->root.declaration().key, "homotopy stage reuses its sequence or root identity");
        for (const auto& old : pin->stages) { guard.step(); require(old.declaration().key != d.key, "homotopy stage identity already retained"); }
        homotopy_sequence_detail::validate(*pin, guard);
        // Admission covers the full prefix plus the new source and maps before
        // allocating/copying the retained spine or cumulative certificate.
        const auto n = add(pin->retained, add(domain.retained_words(), add(algebra::owned_gluing_detail::words(target), homotopy_composition_detail::words(d))));
        const auto e = add(pin->envelope, add(domain.entry_envelope(), add(1, mul(n, 256)))); guard.entries(e);
        auto stage = OwnedHomotopyComposition::prepare(homotopy_sequence_detail::current(*pin), std::move(domain), std::move(target), d, guard);
        HomotopySequenceCertificate c{d.before, d.interval, d.after, stage.carrier_map()};
        if (pin->certificate) {
            const auto& previous = *pin->certificate;
            c.before = compose(d.before, previous.before, guard); c.interval = compose(d.interval, previous.interval, guard);
            c.after = compose(previous.after, d.after, guard); c.carrier = compose(stage.carrier_map(), previous.carrier, guard);
        }
        const auto retained = add(pin->retained, add(stage.retained_words(), words(c)));
        const auto envelope = add(e, stage.entry_envelope()); guard.entries(envelope);
        auto stages = pin->stages; stages.push_back(std::move(stage));
        State s{pin->root, pin->declaration, std::move(stages), std::move(c), retained, envelope};
        certify(s, guard);
        return OwnedHomotopySequence(std::make_shared<const State>(std::move(s)));
    }
    OwnedRelationalHomotopy root() const { return state_->root; }
    OwnedRelationalHomotopy current() const { return homotopy_sequence_detail::current(*state_); }
    std::size_t stage_count() const { return state_->stages.size(); }
    OwnedHomotopyComposition stage(std::size_t i) const { return state_->stages.at(i); }
    const HomotopySequenceDeclaration& declaration() const & { return state_->declaration; }
    const HomotopySequenceDeclaration& declaration() const && = delete;
    const std::optional<HomotopySequenceCertificate>& certificate() const & { return state_->certificate; }
    const std::optional<HomotopySequenceCertificate>& certificate() const && = delete;
    bool same_owner(const OwnedHomotopySequence& other) const { return state_ == other.state_; }
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; homotopy_sequence_detail::validate(*pin, guard); }
    SequencedHomotopyChain push(algebra::OwnedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_sequence_detail::validate(*pin, guard); return {pin, homotopy_sequence_detail::current(*pin).push(std::move(input), guard)};
    }
    SequencedHomotopyChain endpoint(OwnedCylinderSourceChain input, CylinderEndpoint end, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_sequence_detail::validate(*pin, guard); return {pin, homotopy_sequence_detail::current(*pin).endpoint(std::move(input), end, guard)};
    }
    SequencedHomotopyChain homotopy(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_sequence_detail::validate(*pin, guard); return {pin, homotopy_sequence_detail::current(*pin).homotopy(std::move(input), guard)};
    }
    SequencedHomotopyChain boundary(SequencedHomotopyChain input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; homotopy_sequence_detail::require(pin == input.owner_, "homotopy sequence needs its actual output owner");
        homotopy_sequence_detail::validate(*pin, guard); auto out = homotopy_sequence_detail::current(*pin).boundary(input.chain_, guard);
        return {pin, std::move(out), std::make_shared<const SequencedHomotopyChain>(std::move(input))};
    }
    SequencedHomotopyWitness witness(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
private:
    friend class SequencedHomotopyChain;
    explicit OwnedHomotopySequence(std::shared_ptr<const homotopy_sequence_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const homotopy_sequence_detail::State> state_;
};
inline OwnedHomotopySequence SequencedHomotopyChain::plan() const { return OwnedHomotopySequence(owner_); }
class SequencedHomotopyWitness {
public:
    OwnedHomotopySequence plan() const { return plan_; }
    const RelationalHomotopyWitness& witness() const & { return witness_; }
    const RelationalHomotopyWitness& witness() const && = delete;
private:
    friend class OwnedHomotopySequence;
    SequencedHomotopyWitness(OwnedHomotopySequence p, RelationalHomotopyWitness w) : plan_(std::move(p)), witness_(std::move(w)) {}
    OwnedHomotopySequence plan_; RelationalHomotopyWitness witness_;
};
inline SequencedHomotopyWitness OwnedHomotopySequence::witness(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    const auto pin = state_; homotopy_sequence_detail::validate(*pin, guard);
    return {OwnedHomotopySequence(pin), homotopy_sequence_detail::current(*pin).witness(std::move(input), guard)};
}
} // namespace rfc::topology
