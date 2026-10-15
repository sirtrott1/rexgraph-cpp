#pragma once
#include "rfc/topology/cylinder.hpp"
#include "rfc/algebra/owned_tensor_product.hpp"
#include "rfc/algebra/owned_gluing.hpp"

namespace rfc::topology {
namespace owned_cylinder_detail {
using Guard = numeric::ExactActionGuard;
using Q = numeric::Rational;
using Values = std::map<std::uint32_t, Q>;
using Product = algebra::OwnedRelationalTensorProduct;
using ProductValues = Product::Values;
using algebra::owned_gluing_detail::add;
using algebra::owned_gluing_detail::mul;
using algebra::owned_gluing_detail::grade;
using algebra::owned_gluing_detail::require;

// This profile names the existing canonical interval. Matching its realized
// matrix alone cannot replace its identities, occurrence order or declarations.
inline void canonical_interval(const core::RelationalComplex& actual, Guard& guard) {
    RelationalInterval canonical;
    const auto& expected = canonical.complex();
    require(actual.grade_count() == expected.grade_count(), "cylinder interval changes declared grades");
    algebra::RetainedSourceMap identity;
    identity.cells.resize(expected.grade_count()); identity.occurrences.resize(expected.grade_count());
    for (std::size_t k = 0; k < expected.grade_count(); ++k) {
        const auto& a = actual.grade(grade(k)); const auto& b = expected.grade(grade(k));
        guard.step();
        require(a.cell_count() == b.cell_count() && a.incidence_count() == b.incidence_count(), "cylinder interval changes retained population");
        require(a.identities == b.identities && a.relation_identities == b.relation_identities &&
                a.occurrence_identities == b.occurrence_identities && a.relation_types == b.relation_types &&
                a.cell_scales == b.cell_scales && a.lattice_coefficient == b.lattice_coefficient &&
                a.lower_index == b.lower_index && a.upper_index == b.upper_index && a.offsets == b.offsets,
                "cylinder interval is not the complete canonical declaration");
        for (std::size_t i = 0; i < b.cell_count(); ++i) { guard.step(); identity.cells[k].push_back(static_cast<std::uint32_t>(i)); }
        for (std::size_t p = 0; p < b.incidence_count(); ++p) { guard.step(); identity.occurrences[k].push_back(p); }
    }
    algebra::owned_gluing_detail::map(expected, actual, identity, true, guard);
}
inline std::size_t count(const Product& product) {
    std::size_t n{};
    for (std::size_t k = 0; k < product.factor(false).grade_count(); ++k)
        n = add(n, product.factor(false).grade(grade(k)).cell_count());
    return n;
}
struct State {
    Product product;
    RelationalCylinder reference;
    explicit State(Product p) : product(std::move(p)), reference(product.factor(false)) {}
};
inline void validate(const State& s, Guard& guard) { s.product.validate(guard); }
inline ProductValues combine(const ProductValues& a, const ProductValues& b, Q sign, Guard& guard, std::size_t resident) {
    ProductValues out;
    for (const auto& [id, q] : a) { guard.step(); guard.value(q); guard.entries(add(resident, add(out.size(), 1))); out.emplace(id, q); }
    for (const auto& [id, q] : b) {
        guard.step(); guard.value(q); guard.entries(add(resident, add(out.size(), 1)));
        auto& value = out.try_emplace(id, Q{0}).first->second; value = guard.add(value, guard.mul(sign, q));
    }
    return out;
}
} // namespace owned_cylinder_detail

class OwnedCylinderSourceChain;
class CylinderHomotopyWitness;
class OwnedRelationalCylinder {
public:
    using Values = owned_cylinder_detail::Values;
    static OwnedRelationalCylinder prepare(algebra::OwnedRelationalTensorProduct product, numeric::ExactActionGuard& guard) {
        product.validate(guard); owned_cylinder_detail::canonical_interval(product.factor(true), guard);
        return OwnedRelationalCylinder(std::make_shared<const owned_cylinder_detail::State>(std::move(product)));
    }
    bool same_owner(const OwnedRelationalCylinder& other) const { return state_ == other.state_; }
    algebra::OwnedRelationalTensorProduct product() const { return state_->product; }
    const core::RelationalComplex& source() const & { return state_->product.factor(false); }
    const core::RelationalComplex& source() const && = delete;
    const core::RelationalComplex& interval() const & { return state_->product.factor(true); }
    const core::RelationalComplex& interval() const && = delete;
    const RelationalCylinder& reference() const & { return state_->reference; }
    const RelationalCylinder& reference() const && = delete;
    std::size_t retained_words() const { return state_->product.retained_words(); }
    std::size_t entry_envelope() const { return state_->product.entry_envelope(); }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; owned_cylinder_detail::validate(*pin, guard); }
    algebra::RetainedProductCell endpoint_cell(core::CellId id, CylinderEndpoint endpoint) const {
        const auto pin = state_; owned_cylinder_detail::require(endpoint == CylinderEndpoint::Start || endpoint == CylinderEndpoint::Finish, "unknown cylinder endpoint");
        return pin->product.cell(pin->reference.endpoint_cell(id, endpoint));
    }
    algebra::RetainedProductCell prism_cell(core::CellId id) const {
        const auto pin = state_; return pin->product.cell(pin->reference.prism_cell(id));
    }
    std::vector<algebra::RetainedProductOccurrence> retained_prism_boundary(core::CellId id, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; return pin->product.retained_boundary(pin->reference.prism_cell(id), guard);
    }
    OwnedCylinderSourceChain chain(core::Grade, Values, numeric::ExactActionGuard&) const;
    OwnedCylinderSourceChain source_boundary(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
    algebra::OwnedProductField endpoint(OwnedCylinderSourceChain, CylinderEndpoint, numeric::ExactActionGuard&) const;
    algebra::OwnedProductField prism(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
    algebra::OwnedProductField homotopy(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
    algebra::OwnedProductField boundary(algebra::OwnedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; return pin->product.boundary(std::move(input), guard);
    }
    CylinderHomotopyWitness homotopy_witness(OwnedCylinderSourceChain, numeric::ExactActionGuard&) const;
private:
    friend class OwnedCylinderSourceChain;
    explicit OwnedRelationalCylinder(std::shared_ptr<const owned_cylinder_detail::State> s) : state_(std::move(s)) {}
    algebra::OwnedProductField embed(OwnedCylinderSourceChain, bool, bool, CylinderEndpoint, numeric::ExactActionGuard&) const;
    std::shared_ptr<const owned_cylinder_detail::State> state_;
};
class OwnedCylinderSourceChain {
public:
    OwnedRelationalCylinder cylinder() const { return OwnedRelationalCylinder(owner_); }
    core::Grade grade() const { return grade_; }
    const owned_cylinder_detail::Values& values() const & { return values_; }
    const owned_cylinder_detail::Values& values() const && = delete;
private:
    friend class OwnedRelationalCylinder;
    OwnedCylinderSourceChain(std::shared_ptr<const owned_cylinder_detail::State> owner, core::Grade g, owned_cylinder_detail::Values values)
        : owner_(std::move(owner)), grade_(g), values_(std::move(values)) {}
    std::shared_ptr<const owned_cylinder_detail::State> owner_; core::Grade grade_; owned_cylinder_detail::Values values_;
};
inline OwnedCylinderSourceChain OwnedRelationalCylinder::chain(core::Grade g, Values input, numeric::ExactActionGuard& guard) const {
    using namespace owned_cylinder_detail; const auto pin = state_; auto captured = input;
    owned_cylinder_detail::validate(*pin, guard); guard.entries(add(pin->product.entry_envelope(), mul(captured.size(), 3)));
    const auto& arena = pin->product.factor(false).grade(g); guard.dimension(arena.cell_count());
    for (auto& [i, q] : captured) {
        guard.step(); require(i < arena.cell_count(), "cylinder source coordinate out of range"); guard.value(q);
        q = numeric::canonical_rational(q); guard.value(q);
    }
    return {pin, g, std::move(captured)};
}
inline OwnedCylinderSourceChain OwnedRelationalCylinder::source_boundary(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    using namespace owned_cylinder_detail; const auto pin = state_; require(input.owner_ == pin, "cylinder source boundary needs its actual owner");
    require(input.grade_.value > 0, "cylinder source grade zero has no boundary"); owned_cylinder_detail::validate(*pin, guard);
    const auto& source = pin->product.factor(false); const auto& arena = source.grade(input.grade_); Values out;
    for (const auto& [i, q] : input.values_) {
        guard.value(q); const auto [b, e] = arena.incidence_range(i);
        for (auto p = b; p < e; ++p) {
            guard.step(); guard.entries(add(pin->product.entry_envelope(), add(mul(input.values_.size(), 2), add(out.size(), 1))));
            auto& value = out.try_emplace(arena.lower_index[p], Q{0}).first->second;
            if (arena.aspects.get(source.signature().existence_aspect(), p))
                value = guard.add(value, guard.mul(q, algebra::owned_product_detail::coefficient(source, input.grade_, p, guard)));
        }
    }
    return {pin, grade(input.grade_.value - 1u), std::move(out)};
}
inline algebra::OwnedProductField OwnedRelationalCylinder::embed(OwnedCylinderSourceChain input, bool swept, bool signed_prism,
    CylinderEndpoint endpoint, numeric::ExactActionGuard& guard) const {
    using namespace owned_cylinder_detail; const auto pin = state_; require(input.owner_ == pin, "cylinder embedding needs its actual source chain");
    owned_cylinder_detail::validate(*pin, guard);
    require(endpoint == CylinderEndpoint::Start || endpoint == CylinderEndpoint::Finish, "unknown cylinder endpoint");
    ProductValues out;
    for (const auto& [i, q] : input.values_) {
        guard.step(); guard.value(q); guard.entries(add(pin->product.entry_envelope(), add(mul(input.values_.size(), 2), add(out.size(), 1))));
        const core::CellId source{input.grade_, i};
        const auto id = swept ? pin->reference.prism_cell(source) : pin->reference.endpoint_cell(source, endpoint);
        out.emplace(id, signed_prism && input.grade_.value % 2u ? guard.sub(Q{0}, q) : q);
    }
    return pin->product.field(grade(add(input.grade_.value, swept ? 1u : 0u)), std::move(out), guard);
}
inline algebra::OwnedProductField OwnedRelationalCylinder::endpoint(OwnedCylinderSourceChain input, CylinderEndpoint endpoint, numeric::ExactActionGuard& guard) const {
    return embed(std::move(input), false, false, endpoint, guard);
}
inline algebra::OwnedProductField OwnedRelationalCylinder::prism(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    return embed(std::move(input), true, false, CylinderEndpoint::Start, guard);
}
inline algebra::OwnedProductField OwnedRelationalCylinder::homotopy(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    return embed(std::move(input), true, true, CylinderEndpoint::Start, guard);
}
class CylinderHomotopyWitness {
public:
    const OwnedCylinderSourceChain& input() const & { return input_; }
    const OwnedCylinderSourceChain& input() const && = delete;
    const std::optional<OwnedCylinderSourceChain>& source_boundary() const & { return source_boundary_; }
    const std::optional<OwnedCylinderSourceChain>& source_boundary() const && = delete;
    const algebra::OwnedProductField& start() const & { return start_; }
    const algebra::OwnedProductField& start() const && = delete;
    const algebra::OwnedProductField& finish() const & { return finish_; }
    const algebra::OwnedProductField& finish() const && = delete;
    const algebra::OwnedProductField& homotopy() const & { return homotopy_; }
    const algebra::OwnedProductField& homotopy() const && = delete;
    const algebra::OwnedProductField& boundary_homotopy() const & { return boundary_homotopy_; }
    const algebra::OwnedProductField& boundary_homotopy() const && = delete;
    const algebra::OwnedProductField& homotopy_boundary() const & { return homotopy_boundary_; }
    const algebra::OwnedProductField& homotopy_boundary() const && = delete;
    const algebra::OwnedProductField& endpoint_difference() const & { return difference_; }
    const algebra::OwnedProductField& endpoint_difference() const && = delete;
    const algebra::OwnedProductField& residual() const & { return residual_; }
    const algebra::OwnedProductField& residual() const && = delete;
private:
    friend class OwnedRelationalCylinder;
    CylinderHomotopyWitness(OwnedCylinderSourceChain input, std::optional<OwnedCylinderSourceChain> d,
        algebra::OwnedProductField start, algebra::OwnedProductField finish, algebra::OwnedProductField h,
        algebra::OwnedProductField dh, algebra::OwnedProductField hd, algebra::OwnedProductField difference, algebra::OwnedProductField residual)
        : input_(std::move(input)), source_boundary_(std::move(d)), start_(std::move(start)), finish_(std::move(finish)),
          homotopy_(std::move(h)), boundary_homotopy_(std::move(dh)), homotopy_boundary_(std::move(hd)), difference_(std::move(difference)), residual_(std::move(residual)) {}
    OwnedCylinderSourceChain input_; std::optional<OwnedCylinderSourceChain> source_boundary_;
    algebra::OwnedProductField start_, finish_, homotopy_, boundary_homotopy_, homotopy_boundary_, difference_, residual_;
};
inline CylinderHomotopyWitness OwnedRelationalCylinder::homotopy_witness(OwnedCylinderSourceChain input, numeric::ExactActionGuard& guard) const {
    using namespace owned_cylinder_detail; const auto pin = state_; require(input.owner_ == pin, "cylinder witness needs its actual source chain");
    const auto resident = add(pin->product.entry_envelope(), mul(count(pin->product), 64)); guard.entries(resident);
    const auto fixed = OwnedRelationalCylinder(pin);
    auto start = fixed.endpoint(input, CylinderEndpoint::Start, guard), finish = fixed.endpoint(input, CylinderEndpoint::Finish, guard);
    auto h = fixed.homotopy(input, guard), dh = fixed.boundary(h, guard);
    std::optional<OwnedCylinderSourceChain> d;
    if (input.grade_.value) d = fixed.source_boundary(input, guard);
    auto hd = d ? fixed.homotopy(*d, guard) : pin->product.field(input.grade_, {}, guard);
    auto difference = pin->product.field(input.grade_, combine(finish.values(), start.values(), Q{-1}, guard, resident), guard);
    const auto sum = combine(dh.values(), hd.values(), Q{1}, guard, resident);
    auto residual = pin->product.field(input.grade_, combine(difference.values(), sum, Q{-1}, guard, resident), guard);
    for (const auto& [id, q] : residual.values()) { (void)id; guard.step(); require(q == 0, "cylinder homotopy identity failed"); }
    return {std::move(input), std::move(d), std::move(start), std::move(finish), std::move(h), std::move(dh), std::move(hd), std::move(difference), std::move(residual)};
}

// Full strict factor maps induce a cylinder chain map. This is not a declared
// morphism X x I -> Y, nor a geometric homotopy certificate for arbitrary maps.
class OwnedRelationalCylinderMap {
    struct State {
        OwnedRelationalCylinder source, target;
        algebra::RetainedSourceMap source_map, interval_map;
        algebra::PreparedTensorChainMap chain_map;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> types;
        std::size_t retained, envelope;
    };
public:
    static OwnedRelationalCylinderMap prepare(OwnedRelationalCylinder source, OwnedRelationalCylinder target,
        algebra::RetainedSourceMap source_map, algebra::RetainedSourceMap interval_map, numeric::ExactActionGuard& guard) {
        using namespace owned_cylinder_detail; auto a = source_map, b = interval_map;
        source.validate(guard); target.validate(guard);
        const auto words = add(source.retained_words(), add(target.retained_words(), add(algebra::owned_gluing_detail::words(a), algebra::owned_gluing_detail::words(b))));
        const auto envelope = add(1, mul(words, 32)); guard.entries(envelope);
        algebra::owned_gluing_detail::map(source.source(), target.source(), a, false, guard);
        algebra::owned_gluing_detail::map(source.interval(), target.interval(), b, true, guard);
        for (std::size_t k = 0; k < b.cells.size(); ++k) for (std::size_t i = 0; i < b.cells[k].size(); ++i) {
            guard.step(); require(b.cells[k][i] == i, "cylinder map changes declared interval endpoints");
        }
        const auto matrices = [&](const core::RelationalComplex& from, const core::RelationalComplex& to, const algebra::RetainedSourceMap& map) {
            std::vector<numeric::ExactMatrix> out;
            for (std::size_t k = 0; k < to.grade_count(); ++k) {
                const auto cols = algebra::owned_product_detail::count(from, k), rows = to.grade(grade(k)).cell_count();
                numeric::ExactMatrix m(rows, cols);
                for (std::size_t i = 0; i < cols; ++i) { guard.step(); guard.entries(envelope); m.set(map.cells[k][i], i, Q{1}); }
                out.push_back(std::move(m));
            }
            return out;
        };
        auto left = matrices(source.source(), target.source(), a), right = matrices(source.interval(), target.interval(), b);
        auto map = algebra::PreparedTensorChainMap::prepare(source.product(), target.product(), std::move(left), std::move(right), guard);
        std::vector<std::pair<std::uint64_t, std::uint64_t>> types;
        const auto source_product = source.product(), target_product = target.product();
        for (const auto& t : source_product.declaration().types) {
            bool found = false;
            for (const auto& u : target_product.declaration().types) {
                guard.step(); if (t.left == u.left && t.right == u.right) { types.emplace_back(t.key, u.key); found = true; break; }
            }
            require(found, "cylinder factor map has no declared target product type");
        }
        const auto complete_envelope = add(envelope, map.entry_envelope()); guard.entries(complete_envelope);
        return OwnedRelationalCylinderMap(std::make_shared<const State>(State{std::move(source), std::move(target), std::move(a), std::move(b), std::move(map), std::move(types), words, complete_envelope}));
    }
    OwnedRelationalCylinder source() const { return state_->source; }
    OwnedRelationalCylinder target() const { return state_->target; }
    const algebra::RetainedSourceMap& factor_map(bool interval) const & { return interval ? state_->interval_map : state_->source_map; }
    const algebra::RetainedSourceMap& factor_map(bool) const && = delete;
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& product_types() const & { return state_->types; }
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& product_types() const && = delete;
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    algebra::OwnedProductField apply(algebra::OwnedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; guard.entries(pin->envelope); pin->source.validate(guard); pin->target.validate(guard);
        for (std::size_t n = 0; n < owned_cylinder_detail::add(algebra::owned_gluing_detail::words(pin->source_map), algebra::owned_gluing_detail::words(pin->interval_map)); ++n) guard.step();
        return pin->chain_map.apply(std::move(input), guard);
    }
private:
    explicit OwnedRelationalCylinderMap(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::topology
