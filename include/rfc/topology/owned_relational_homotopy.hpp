#pragma once
#include "rfc/topology/cylinder_carrier.hpp"

namespace rfc::topology {
struct RelationalHomotopyDeclaration {
    std::uint64_t key{}, revision{};
    algebra::RetainedSourceMap cylinder_target, first, second;
};
namespace owned_homotopy_detail {
using owned_cylinder_detail::Guard;
using owned_cylinder_detail::Q;
using owned_cylinder_detail::Values;
using owned_cylinder_detail::add;
using owned_cylinder_detail::mul;
using owned_cylinder_detail::grade;
using owned_cylinder_detail::require;
struct State {
    OwnedCylinderCarrier carrier;
    core::RelationalComplex target;
    RelationalHomotopyDeclaration declaration;
    std::size_t retained{}, envelope{};
};
inline std::size_t words(const RelationalHomotopyDeclaration& d) {
    return add(2, add(algebra::owned_gluing_detail::words(d.cylinder_target),
        add(algebra::owned_gluing_detail::words(d.first), algebra::owned_gluing_detail::words(d.second))));
}
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.carrier.validate(guard); algebra::owned_gluing_detail::source(s.target, guard);
    for (std::size_t n = 0; n < words(s.declaration); ++n) guard.step();
}
inline void values(const State& s, core::Grade g, const Values& v, Guard& guard) {
    guard.dimension(s.target.grade(g).cell_count());
    for (const auto& [i, q] : v) { guard.step(); require(i < s.target.grade(g).cell_count(), "homotopy target coordinate out of range"); guard.value(q); }
}
inline Values combine(const Values& a, const Values& b, Q sign, Guard& guard, std::size_t resident) {
    Values out;
    for (const auto& [i, q] : a) { guard.step(); guard.entries(add(resident, add(out.size(), 1))); guard.value(q); out.emplace(i, q); }
    for (const auto& [i, q] : b) {
        guard.step(); guard.entries(add(resident, add(out.size(), 1))); auto& value = out.try_emplace(i, Q{0}).first->second;
        value = guard.add(value, guard.mul(sign, q));
    }
    return out;
}
} // namespace owned_homotopy_detail
class OwnedRelationalHomotopy;
class RelationalHomotopyWitness;
class OwnedHomotopyChain {
public:
    OwnedRelationalHomotopy plan() const;
    core::Grade grade() const { return grade_; }
    const owned_homotopy_detail::Values& values() const & { return values_; }
    const owned_homotopy_detail::Values& values() const && = delete;
    const std::optional<algebra::OwnedProductField>& product_input() const & { return product_; }
    const std::optional<algebra::OwnedProductField>& product_input() const && = delete;
    std::shared_ptr<const OwnedHomotopyChain> input() const { return input_; }
private:
    friend class OwnedRelationalHomotopy;
    OwnedHomotopyChain(std::shared_ptr<const owned_homotopy_detail::State> owner, core::Grade g, owned_homotopy_detail::Values v,
        std::optional<algebra::OwnedProductField> product = {}, std::shared_ptr<const OwnedHomotopyChain> input = {})
        : owner_(std::move(owner)), grade_(g), values_(std::move(v)), product_(std::move(product)), input_(std::move(input)) {}
    std::shared_ptr<const owned_homotopy_detail::State> owner_; core::Grade grade_; owned_homotopy_detail::Values values_;
    std::optional<algebra::OwnedProductField> product_; std::shared_ptr<const OwnedHomotopyChain> input_;
};
// H is the complete supplied strict source morphism C -> Y, where C is a
// certified declared realization of X boxtimes I. Its chain homotopy is derived.
class OwnedRelationalHomotopy {
public:
    static OwnedRelationalHomotopy prepare(OwnedCylinderCarrier carrier, core::RelationalComplex target,
        RelationalHomotopyDeclaration input, numeric::ExactActionGuard& guard) {
        using namespace owned_homotopy_detail; auto declaration = input;
        require(declaration.key && declaration.revision, "relational homotopy needs its identity and revision");
        const auto retained = add(carrier.retained_words(), add(algebra::owned_gluing_detail::words(target), words(declaration)));
        const auto envelope = add(carrier.entry_envelope(), add(1, mul(retained, 32)));
        State s{std::move(carrier), std::move(target), std::move(declaration), retained, envelope};
        owned_homotopy_detail::validate(s, guard); algebra::owned_gluing_detail::chain(s.target, guard, envelope);
        const auto cylinder = s.carrier.cylinder(); const auto& source = cylinder.source();
        algebra::owned_gluing_detail::map(s.carrier.carrier(), s.target, s.declaration.cylinder_target, false, guard);
        algebra::owned_gluing_detail::map(source, s.target, s.declaration.first, false, guard);
        algebra::owned_gluing_detail::map(source, s.target, s.declaration.second, false, guard);
        for (std::size_t end = 0; end < 2; ++end) {
            const auto& inclusion = s.carrier.endpoint(end ? CylinderEndpoint::Finish : CylinderEndpoint::Start);
            const auto& endpoint = end ? s.declaration.second : s.declaration.first; const auto& h = s.declaration.cylinder_target;
            for (std::size_t k = 0; k < source.grade_count(); ++k) {
                for (std::size_t i = 0; i < inclusion.cells[k].size(); ++i) {
                    guard.step(); require(h.cells[k].at(inclusion.cells[k][i]) == endpoint.cells[k][i], "homotopy restriction disagrees on an original cell");
                }
                for (std::size_t q = 0; q < inclusion.occurrences[k].size(); ++q) {
                    guard.step(); require(h.occurrences[k].at(inclusion.occurrences[k][q]) == endpoint.occurrences[k][q],
                        "homotopy restriction disagrees on an original occurrence");
                }
            }
        }
        return OwnedRelationalHomotopy(std::make_shared<const State>(std::move(s)));
    }
    OwnedCylinderCarrier carrier() const { return state_->carrier; }
    const core::RelationalComplex& target() const & { return state_->target; }
    const core::RelationalComplex& target() const && = delete;
    const RelationalHomotopyDeclaration& declaration() const & { return state_->declaration; }
    const RelationalHomotopyDeclaration& declaration() const && = delete;
    bool same_owner(const OwnedRelationalHomotopy& other) const { return state_ == other.state_; }
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; owned_homotopy_detail::validate(*pin, guard); }
    OwnedHomotopyChain push(algebra::OwnedProductField input, numeric::ExactActionGuard& guard) const {
        using namespace owned_homotopy_detail; const auto pin = state_;
        require(input.product().same_owner(pin->carrier.cylinder().product()), "homotopy push needs its actual cylinder field");
        owned_homotopy_detail::validate(*pin, guard); Values out;
        for (const auto& [id, q] : input.values()) {
            guard.step(); guard.value(q); guard.entries(add(pin->envelope, add(mul(input.values().size(), 3), add(out.size(), 1))));
            const auto i = pin->declaration.cylinder_target.cells.at(id.grade().value).at(pin->carrier.image(id));
            auto& value = out.try_emplace(i, Q{0}).first->second; value = guard.add(value, q);
        }
        return {pin, input.grade(), std::move(out), std::move(input)};
    }
    OwnedHomotopyChain endpoint(OwnedCylinderSourceChain input, CylinderEndpoint end, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; const auto cylinder = pin->carrier.cylinder();
        return OwnedRelationalHomotopy(pin).push(cylinder.endpoint(std::move(input), end, guard), guard);
    }
    OwnedHomotopyChain homotopy(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; const auto cylinder = pin->carrier.cylinder();
        return OwnedRelationalHomotopy(pin).push(cylinder.homotopy(std::move(input), guard), guard);
    }
    OwnedHomotopyChain boundary(OwnedHomotopyChain input, numeric::ExactActionGuard& guard) const {
        using namespace owned_homotopy_detail; const auto pin = state_; require(input.owner_ == pin, "homotopy boundary needs its actual target chain");
        require(input.grade_.value > 0, "homotopy target grade zero has no boundary"); owned_homotopy_detail::validate(*pin, guard);
        values(*pin, input.grade_, input.values_, guard); const auto& a = pin->target.grade(input.grade_); Values out;
        for (const auto& [i, q] : input.values_) {
            const auto [b, e] = a.incidence_range(i);
            for (auto p = b; p < e; ++p) {
                guard.step(); guard.entries(add(pin->envelope, add(mul(input.values_.size(), 3), add(out.size(), 1))));
                auto& value = out.try_emplace(a.lower_index[p], Q{0}).first->second;
                if (a.aspects.get(pin->target.signature().existence_aspect(), p))
                    value = guard.add(value, guard.mul(q, algebra::owned_gluing_detail::coefficient(pin->target, input.grade_, p, guard)));
            }
        }
        return {pin, grade(input.grade_.value - 1u), std::move(out), {}, std::make_shared<const OwnedHomotopyChain>(std::move(input))};
    }
    RelationalHomotopyWitness witness(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
private:
    friend class OwnedHomotopyChain;
    explicit OwnedRelationalHomotopy(std::shared_ptr<const owned_homotopy_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const owned_homotopy_detail::State> state_;
};
inline OwnedRelationalHomotopy OwnedHomotopyChain::plan() const { return OwnedRelationalHomotopy(owner_); }
class RelationalHomotopyWitness {
public:
    const CylinderHomotopyWitness& cylinder_witness() const & { return cylinder_; }
    const CylinderHomotopyWitness& cylinder_witness() const && = delete;
    const OwnedHomotopyChain& first() const & { return first_; }
    const OwnedHomotopyChain& first() const && = delete;
    const OwnedHomotopyChain& second() const & { return second_; }
    const OwnedHomotopyChain& second() const && = delete;
    const OwnedHomotopyChain& homotopy() const & { return h_; }
    const OwnedHomotopyChain& homotopy() const && = delete;
    const OwnedHomotopyChain& boundary_homotopy() const & { return dh_; }
    const OwnedHomotopyChain& boundary_homotopy() const && = delete;
    const OwnedHomotopyChain& homotopy_boundary() const & { return hd_; }
    const OwnedHomotopyChain& homotopy_boundary() const && = delete;
    const OwnedHomotopyChain& endpoint_difference() const & { return difference_; }
    const OwnedHomotopyChain& endpoint_difference() const && = delete;
    const OwnedHomotopyChain& residual() const & { return residual_; }
    const OwnedHomotopyChain& residual() const && = delete;
private:
    friend class OwnedRelationalHomotopy;
    RelationalHomotopyWitness(CylinderHomotopyWitness cylinder, OwnedHomotopyChain first, OwnedHomotopyChain second, OwnedHomotopyChain h,
        OwnedHomotopyChain dh, OwnedHomotopyChain hd, OwnedHomotopyChain difference, OwnedHomotopyChain residual)
        : cylinder_(std::move(cylinder)), first_(std::move(first)), second_(std::move(second)), h_(std::move(h)),
          dh_(std::move(dh)), hd_(std::move(hd)), difference_(std::move(difference)), residual_(std::move(residual)) {}
    CylinderHomotopyWitness cylinder_; OwnedHomotopyChain first_, second_, h_, dh_, hd_, difference_, residual_;
};
inline RelationalHomotopyWitness OwnedRelationalHomotopy::witness(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    using namespace owned_homotopy_detail; const auto pin = state_; const auto fixed = OwnedRelationalHomotopy(pin);
    const auto cylinder = pin->carrier.cylinder();
    const auto resident = add(pin->envelope, mul(add(cylinder_carrier_detail::population(cylinder), algebra::owned_gluing_detail::words(pin->target)), 32));
    guard.entries(resident); auto cw = cylinder.homotopy_witness(std::move(input), guard);
    auto first = fixed.push(cw.start(), guard), second = fixed.push(cw.finish(), guard), h = fixed.push(cw.homotopy(), guard);
    auto dh = fixed.boundary(h, guard), hd = fixed.push(cw.homotopy_boundary(), guard);
    OwnedHomotopyChain difference{pin, first.grade(), combine(second.values(), first.values(), Q{-1}, guard, resident)};
    const auto sum = combine(dh.values(), hd.values(), Q{1}, guard, resident);
    OwnedHomotopyChain residual{pin, first.grade(), combine(difference.values(), sum, Q{-1}, guard, resident)};
    for (const auto& [i, q] : residual.values()) { (void)i; guard.step(); require(q == 0, "relational homotopy chain identity failed"); }
    return {std::move(cw), std::move(first), std::move(second), std::move(h), std::move(dh), std::move(hd), std::move(difference), std::move(residual)};
}
} // namespace rfc::topology
