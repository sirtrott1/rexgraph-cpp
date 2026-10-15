#pragma once
#include "rfc/algebra/owned_tensor_product.hpp"
#include <array>
#include <variant>

namespace rfc::algebra {
enum class ProductAssociation { Left, Right };
struct LeftAssociatedCell {
    ProductCell inner;
    core::CellId outer;
    core::Grade grade() const { return owned_product_detail::grade(owned_product_detail::add(inner.grade().value, outer.grade.value)); }
    auto operator<=>(const LeftAssociatedCell&) const = default;
};
struct RightAssociatedCell {
    core::CellId outer;
    ProductCell inner;
    core::Grade grade() const { return owned_product_detail::grade(owned_product_detail::add(outer.grade.value, inner.grade().value)); }
    auto operator<=>(const RightAssociatedCell&) const = default;
};
using AssociatedCell = std::variant<LeftAssociatedCell, RightAssociatedCell>;
inline core::Grade associated_grade(const AssociatedCell& c) { return std::visit([](const auto& x){ return x.grade(); }, c); }
// The inner constructor key includes its declared entity/entity pair. It is
// never replaced by an outer null token. The factor token is null at grade zero.
struct NestedProductType {
    std::uint64_t inner_type{};
    std::optional<core::RelationTypeId> factor_type;
    std::uint64_t key{};
    bool operator==(const NestedProductType&) const = default;
};
struct NestedProductDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<NestedProductType> types;
};
struct ProductAssociatorTypeImage {
    std::uint64_t left{}, right{};
    bool operator==(const ProductAssociatorTypeImage&) const = default;
};
struct ProductAssociatorDeclaration {
    std::uint64_t key{}, revision{};
    RelationalProductDeclaration ab, bc;
    // left: (A product B) product C; right: A product (B product C).
    NestedProductDeclaration left, right;
    std::vector<ProductAssociatorTypeImage> types;
};
struct ProductAssociationCell {
    LeftAssociatedCell left;
    RightAssociatedCell right;
    ProductCellIdentity left_inner_identity, right_inner_identity;
    ProductFactorIdentity left_outer_identity, right_outer_identity;
    std::uint64_t left_type{}, right_type{};
    bool operator==(const ProductAssociationCell&) const = default;
};
struct NestedProductOccurrence {
    AssociatedCell upper, lower;
    // An explicit path through the nested pair, from outermost to innermost.
    std::array<ProductBoundarySide, 2> path{};
    std::size_t path_length{}, factor{}, attaching_position{}, word_position{};
    core::OccurrenceKey factor_occurrence;
    bool inner_koszul_flip{}, outer_koszul_flip{}, koszul_flip{}, exists{};
    std::vector<bool> factor_aspects;
    numeric::Rational factor_coefficient, coefficient;
    bool operator==(const NestedProductOccurrence&) const = default;
};
struct ProductAssociationFlag { NestedProductOccurrence left, right; };
namespace product_associator_detail {
using owned_product_detail::Q;
using owned_product_detail::Guard;
using owned_product_detail::add;
using owned_product_detail::mul;
using owned_product_detail::grade;
using owned_product_detail::require;
using Values = std::map<AssociatedCell, Q>;
using Types = std::map<std::pair<std::uint64_t, std::optional<core::RelationTypeId>>, std::uint64_t>;
inline ProductAssociation association(const AssociatedCell& c) {
    return std::holds_alternative<LeftAssociatedCell>(c) ? ProductAssociation::Left : ProductAssociation::Right;
}
inline void side(ProductAssociation s) {
    require(s == ProductAssociation::Left || s == ProductAssociation::Right, "unknown product association");
}
inline AssociatedCell image(const AssociatedCell& c) {
    if (const auto* x = std::get_if<LeftAssociatedCell>(&c)) return RightAssociatedCell{x->inner.left, {x->inner.right, x->outer}};
    const auto& x = std::get<RightAssociatedCell>(c); return LeftAssociatedCell{{x.outer, x.inner.left}, x.inner.right};
}
inline std::size_t words(const std::array<const core::RelationalComplex*, 3>& factors, const ProductAssociatorDeclaration& d) {
    auto n = add(10, add(mul(add(d.ab.types.size(), d.bc.types.size()), 3),
        add(mul(add(d.left.types.size(), d.right.types.size()), 3), mul(d.types.size(), 2))));
    std::array<std::size_t, 3> cells{}, flags{};
    for (std::size_t f = 0; f < 3; ++f) {
        const auto& c = *factors[f]; n = add(n, owned_product_detail::source_words(c));
        for (std::size_t k = 0; k < c.grade_count(); ++k) {
            cells[f] = add(cells[f], c.grade(grade(k)).cell_count());
            flags[f] = add(flags[f], mul(c.grade(grade(k)).incidence_count(), add(16, c.signature().aspects().size())));
        }
    }
    n = add(n, mul(18, mul(cells[0], mul(cells[1], cells[2]))));
    for (std::size_t f = 0; f < 3; ++f) n = add(n, mul(flags[f], mul(cells[(f+1)%3], cells[(f+2)%3])));
    return n;
}
inline Types types(const NestedProductDeclaration& d, const RelationalProductDeclaration& inner,
                   const core::RelationalComplex& factor, Guard& guard) {
    require(d.key && d.revision, "nested product requires a constructor identity and revision");
    require(d.types.size() == mul(inner.types.size(), add(factor.signature().type_count(), 1)),
        "nested product needs all constructor pairs, including unused pairs");
    std::set<std::uint64_t> inner_keys, keys;
    for (const auto& t : inner.types) { guard.step(); inner_keys.insert(t.key); }
    Types out;
    for (const auto& t : d.types) {
        guard.step(); require(inner_keys.contains(t.inner_type) &&
            (!t.factor_type || *t.factor_type < factor.signature().type_count()), "undeclared nested product factor type");
        require(t.key && keys.insert(t.key).second && out.emplace(std::pair{t.inner_type, t.factor_type}, t.key).second,
            "nested product type keys and pairs must be unique");
    }
    return out;
}
struct State {
    OwnedRelationalTensorProduct ab, bc;
    ProductAssociatorDeclaration declaration;
    Types left_types, right_types;
    std::vector<ProductAssociationCell> cells;
    std::vector<std::size_t> grade_counts;
    std::size_t words{}, envelope{};
    core::Grade maximum;
};
inline const core::RelationalComplex& factor(const State& s, std::size_t f) {
    if (f > 2) throw std::out_of_range("product associator factor");
    return f < 2 ? s.ab.factor(f == 1) : s.bc.factor(true);
}
inline std::array<core::CellId, 3> factors(const AssociatedCell& c) {
    if (const auto* x = std::get_if<LeftAssociatedCell>(&c)) return {x->inner.left, x->inner.right, x->outer};
    const auto& x = std::get<RightAssociatedCell>(c); return {x.outer, x.inner.left, x.inner.right};
}
inline AssociatedCell cell(ProductAssociation a, const std::array<core::CellId, 3>& ids) {
    side(a); if (a == ProductAssociation::Left) return LeftAssociatedCell{{ids[0], ids[1]}, ids[2]};
    return RightAssociatedCell{ids[0], {ids[1], ids[2]}};
}
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.ab.validate(guard); s.bc.validate(guard);
    guard.dimension(s.grade_counts.size());
    for (const auto n : s.grade_counts) { guard.step(); guard.dimension(n); }
    for (const auto* d : {&s.declaration.left, &s.declaration.right}) for (const auto& t : d->types) { (void)t; guard.step(); }
    for (const auto& t : s.declaration.types) { (void)t; guard.step(); }
}
inline ProductAssociationCell certificate(const State& s, LeftAssociatedCell c) {
    const auto ab = s.ab.cell(c.inner); owned_product_detail::cell(factor(s, 2), c.outer);
    const RightAssociatedCell r{c.inner.left, {c.inner.right, c.outer}};
    const auto bc = s.bc.cell(r.inner);
    return {c, r, ab.source_identity, bc.source_identity, owned_product_detail::identity(factor(s, 2), c.outer),
        owned_product_detail::identity(factor(s, 0), r.outer),
        s.left_types.at({ab.type, owned_product_detail::type(factor(s, 2), c.outer)}),
        s.right_types.at({bc.type, owned_product_detail::type(factor(s, 0), r.outer)})};
}
inline std::vector<NestedProductOccurrence> routes(const State& s, AssociatedCell upper, Guard& guard) {
    const auto ids = factors(upper); const auto a = association(upper);
    for (std::size_t f = 0; f < 3; ++f) owned_product_detail::cell(factor(s, f), ids[f]);
    require(associated_grade(upper).value > 0, "nested product grade zero has no boundary");
    std::vector<NestedProductOccurrence> out;
    // Use the canonical inner owning products for their full original fibers.
    const auto inner = a == ProductAssociation::Left ? ProductCell{ids[0], ids[1]} : ProductCell{ids[1], ids[2]};
    const auto inner_grade = inner.grade();
    const auto append = [&](std::size_t f, std::size_t position, core::OccurrenceKey occurrence, bool exists,
                            std::vector<bool> aspects, Q q, core::CellId lower_factor) {
        guard.step(); auto lower = ids; lower[f] = lower_factor;
        const bool inner_flip = a == ProductAssociation::Left ? f == 1 && ids[0].grade.value % 2u != 0
            : f == 2 && ids[1].grade.value % 2u != 0;
        const bool outer_flip = a == ProductAssociation::Left ? f == 2 && inner_grade.value % 2u != 0
            : f > 0 && ids[0].grade.value % 2u != 0;
        const bool flip = inner_flip != outer_flip;
        const auto l = ProductBoundarySide::Left, r = ProductBoundarySide::Right;
        const auto path = a == ProductAssociation::Left ? (f == 0 ? std::array{l,l} : f == 1 ? std::array{l,r} : std::array{r,l})
            : (f == 0 ? std::array{l,l} : f == 1 ? std::array{r,l} : std::array{r,r});
        auto realized = flip ? guard.sub(Q{0}, q) : q;
        guard.entries(add(s.envelope, mul(add(out.size(), 1), add(16, aspects.size()))));
        out.push_back({upper, cell(a, lower), path, (a == ProductAssociation::Left ? f == 2 : f == 0) ? 1u : 2u,
            f, position, out.size(), occurrence, inner_flip, outer_flip, flip, exists, std::move(aspects), std::move(q), std::move(realized)});
    };
    const auto outer = a == ProductAssociation::Left ? 2u : 0u;
    const auto emit_outer = [&] {
        const auto id = ids[outer]; if (id.grade.value == 0) return;
        const auto& c = factor(s, outer); const auto& arena = c.grade(id.grade);
        const auto [begin, end] = arena.incidence_range(id.index);
        for (auto p = begin; p < end; ++p) {
            guard.step(); std::vector<bool> aspects; aspects.reserve(c.signature().aspects().size());
            for (std::size_t j = 0; j < c.signature().aspects().size(); ++j) { guard.step(); aspects.push_back(arena.aspects.get(static_cast<composite::AspectId>(j), p)); }
            append(outer, p-begin, arena.occurrence_identities[p], arena.aspects.get(c.signature().existence_aspect(), p),
                std::move(aspects), owned_product_detail::coefficient(c, id.grade, p, guard),
                {grade(id.grade.value-1u), arena.lower_index[p]});
        }
    };
    if (a == ProductAssociation::Right) emit_outer();
    if (inner_grade.value) {
        const auto word = (a == ProductAssociation::Left ? s.ab : s.bc).retained_boundary(inner, guard);
        for (const auto& o : word) {
            const bool right = o.side == ProductBoundarySide::Right;
            const auto f = (a == ProductAssociation::Left ? 0u : 1u) + (right ? 1u : 0u);
            append(f, o.attaching_position, o.factor_occurrence, o.exists, o.factor_aspects, o.factor_coefficient,
                right ? o.lower.right : o.lower.left);
        }
    }
    if (a == ProductAssociation::Left) emit_outer();
    return out;
}
inline std::vector<ProductAssociationFlag> flags(const State& s, LeftAssociatedCell c, Guard& guard) {
    if (c.grade().value == 0) return {};
    const auto left = routes(s, c, guard), right = routes(s, image(c), guard);
    require(left.size() == right.size(), "associator changes a retained occurrence population");
    std::vector<ProductAssociationFlag> out; out.reserve(left.size());
    for (std::size_t i = 0; i < left.size(); ++i) {
        guard.step(); const auto& x = left[i]; const auto& y = right[i];
        require(image(x.upper) == y.upper && image(x.lower) == y.lower && x.factor == y.factor &&
            x.attaching_position == y.attaching_position && x.word_position == y.word_position &&
            x.factor_occurrence == y.factor_occurrence && x.exists == y.exists && x.factor_aspects == y.factor_aspects &&
            x.factor_coefficient == y.factor_coefficient && x.koszul_flip == y.koszul_flip && x.coefficient == y.coefficient,
            "associator changes an original flag or its exact realized coefficient");
        guard.value(x.factor_coefficient); guard.value(x.coefficient); guard.value(y.factor_coefficient); guard.value(y.coefficient);
        out.push_back({x, y});
    }
    return out;
}
inline void values(const State& s, ProductAssociation a, core::Grade g, const Values& v, Guard& guard) {
    side(a); if (g > s.maximum) throw std::out_of_range("nested product field grade");
    guard.entries(add(s.envelope, mul(v.size(), 8)));
    for (const auto& [c, q] : v) {
        guard.step(); require(association(c) == a && associated_grade(c) == g, "nested product field has a foreign association or grade");
        const auto ids = factors(c); for (std::size_t f = 0; f < 3; ++f) owned_product_detail::cell(factor(s, f), ids[f]);
        guard.value(q);
    }
}
} // namespace product_associator_detail
class OwnedProductAssociator;
class ProductAssociatorCellWitness;
class ProductAssociatorChainWitness;
class OwnedAssociatedProductField {
public:
    ProductAssociation association() const noexcept { return association_; }
    core::Grade grade() const noexcept { return grade_; }
    const product_associator_detail::Values& values() const & { return values_; }
    const product_associator_detail::Values& values() const && = delete;
    OwnedProductAssociator plan() const;
private:
    friend class OwnedProductAssociator;
    OwnedAssociatedProductField(std::shared_ptr<const product_associator_detail::State> owner, ProductAssociation a,
        core::Grade g, product_associator_detail::Values v) : owner_(std::move(owner)), association_(a), grade_(g), values_(std::move(v)) {}
    std::shared_ptr<const product_associator_detail::State> owner_;
    ProductAssociation association_;
    core::Grade grade_;
    product_associator_detail::Values values_;
};
class OwnedProductAssociator {
public:
    using Values = product_associator_detail::Values;
    static OwnedProductAssociator prepare(std::array<core::RelationalComplex, 3> factors, ProductAssociatorDeclaration input,
                                          product_associator_detail::Guard& guard) {
        auto d = input; // Moving a vector does not revoke the caller's element aliases.
        namespace detail = product_associator_detail;
        using detail::add; using detail::mul; using detail::require;
        const auto words = detail::words({&factors[0], &factors[1], &factors[2]}, d), envelope = add(1, mul(words, 16));
        guard.entries(envelope); guard.step(); require(d.key && d.revision, "associator needs a declared identity and revision");
        const auto maximum = detail::grade(add(add(factors[0].max_grade().value, factors[1].max_grade().value), factors[2].max_grade().value));
        auto ab = OwnedRelationalTensorProduct::prepare(factors[0], factors[1], d.ab, guard);
        auto bc = OwnedRelationalTensorProduct::prepare(factors[1], factors[2], d.bc, guard);
        auto lt = detail::types(d.left, d.ab, factors[2], guard), rt = detail::types(d.right, d.bc, factors[0], guard);
        require(d.types.size() == d.left.types.size() && d.types.size() == d.right.types.size(), "associator needs its complete type correspondence");
        std::map<std::uint64_t, std::uint64_t> images; std::set<std::uint64_t> outputs;
        for (const auto& t : d.types) { guard.step(); require(images.emplace(t.left, t.right).second && outputs.insert(t.right).second, "associator type correspondence is not a bijection"); }
        for (const auto& a : d.ab.types) for (const auto& b : d.bc.types) {
            guard.step(); if (a.right != b.left) continue;
            require(images.contains(lt.at({a.key, b.right})) && images.at(lt.at({a.key, b.right})) == rt.at({b.key, a.left}),
                "associator type correspondence changes an ordered constructor triple");
        }
        guard.dimension(add(maximum.value, 1));
        auto s = std::make_shared<detail::State>(detail::State{std::move(ab), std::move(bc), std::move(d), std::move(lt), std::move(rt), {},
            std::vector<std::size_t>(add(maximum.value, 1)), words, envelope, maximum});
        for (std::size_t p = 0; p < factors[0].grade_count(); ++p) for (std::size_t q = 0; q < factors[1].grade_count(); ++q)
            for (std::size_t r = 0; r < factors[2].grade_count(); ++r) {
                guard.step(); const auto count = mul(owned_product_detail::count(factors[0], p), mul(owned_product_detail::count(factors[1], q), owned_product_detail::count(factors[2], r)));
                auto& n = s->grade_counts.at(add(add(p,q),r)); n = add(n, count); guard.dimension(n);
                for (std::size_t i = 0; i < owned_product_detail::count(factors[0], p); ++i)
                    for (std::size_t j = 0; j < owned_product_detail::count(factors[1], q); ++j)
                        for (std::size_t k = 0; k < owned_product_detail::count(factors[2], r); ++k) {
                            guard.step(); const LeftAssociatedCell c{{{detail::grade(p), static_cast<std::uint32_t>(i)}, {detail::grade(q), static_cast<std::uint32_t>(j)}}, {detail::grade(r), static_cast<std::uint32_t>(k)}};
                            s->cells.push_back(detail::certificate(*s, c)); (void)detail::flags(*s, c, guard);
                        }
            }
        guard.step(); return OwnedProductAssociator(std::move(s));
    }
    core::Grade max_grade() const noexcept { return state_->maximum; }
    std::size_t retained_words() const noexcept { return state_->words; }
    std::size_t entry_envelope() const noexcept { return state_->envelope; }
    bool same_owner(const OwnedProductAssociator& other) const noexcept { return state_ == other.state_; }
    const core::RelationalComplex& factor(std::size_t f) const & { return product_associator_detail::factor(*state_, f); }
    const core::RelationalComplex& factor(std::size_t) const && = delete;
    OwnedRelationalTensorProduct inner_product(bool bc) const { return bc ? state_->bc : state_->ab; }
    const ProductAssociatorDeclaration& declaration() const & { return state_->declaration; }
    const ProductAssociatorDeclaration& declaration() const && = delete;
    const std::vector<ProductAssociationCell>& cells() const & { return state_->cells; }
    const std::vector<ProductAssociationCell>& cells() const && = delete;
    std::size_t cell_count(core::Grade g) const { if (g > state_->maximum) throw std::out_of_range("associated product grade"); return state_->grade_counts[g.value]; }
    void validate(product_associator_detail::Guard& guard) const { const auto pin = state_; guard.step(); product_associator_detail::validate(*pin, guard); }
    OwnedAssociatedProductField field(ProductAssociation a, core::Grade g, Values input, product_associator_detail::Guard& guard) const {
        const auto pin = state_; auto v = input; guard.step(); product_associator_detail::validate(*pin, guard); product_associator_detail::values(*pin, a, g, v, guard);
        for (auto& [c, q] : v) { (void)c; guard.step(); q.canonicalize(); }
        return OwnedAssociatedProductField(pin, a, g, std::move(v));
    }
    OwnedAssociatedProductField boundary(OwnedAssociatedProductField input, product_associator_detail::Guard& guard) const {
        namespace detail = product_associator_detail; const auto pin = state_; guard.step();
        detail::require(pin == input.owner_, "associated boundary requires its actual owner"); detail::validate(*pin, guard);
        detail::values(*pin, input.association_, input.grade_, input.values_, guard);
        detail::require(input.grade_.value > 0, "associated boundary at grade zero"); Values out;
        for (const auto& [c, q] : input.values_) for (const auto& o : detail::routes(*pin, c, guard)) {
            guard.step(); guard.entries(detail::add(pin->envelope, detail::mul(detail::add(input.values_.size(), detail::add(out.size(), 1)), 8)));
            auto [it, inserted] = out.try_emplace(o.lower, Q{0}); (void)inserted;
            if (o.exists) it->second = guard.add(it->second, guard.mul(q, o.coefficient));
        }
        return OwnedAssociatedProductField(pin, input.association_, detail::grade(input.grade_.value-1u), std::move(out));
    }
    OwnedAssociatedProductField reassociate(OwnedAssociatedProductField input, product_associator_detail::Guard& guard) const {
        namespace detail = product_associator_detail; const auto pin = state_; guard.step();
        detail::require(pin == input.owner_, "associator requires its actual field owner"); detail::validate(*pin, guard);
        detail::values(*pin, input.association_, input.grade_, input.values_, guard); Values out;
        for (const auto& [c, q] : input.values_) { guard.step(); out.emplace(detail::image(c), q); }
        return OwnedAssociatedProductField(pin, input.association_ == ProductAssociation::Left ? ProductAssociation::Right : ProductAssociation::Left,
            input.grade_, std::move(out));
    }
    ProductAssociatorCellWitness cell_witness(LeftAssociatedCell, product_associator_detail::Guard&) const;
    ProductAssociatorChainWitness witness(OwnedAssociatedProductField, product_associator_detail::Guard&) const;
private:
    friend class OwnedAssociatedProductField;
    explicit OwnedProductAssociator(std::shared_ptr<const product_associator_detail::State> s) : state_(std::move(s)) {}
    using Q = product_associator_detail::Q;
    std::shared_ptr<const product_associator_detail::State> state_;
};
inline OwnedProductAssociator OwnedAssociatedProductField::plan() const { return OwnedProductAssociator(owner_); }
class ProductAssociatorCellWitness {
public:
    OwnedProductAssociator plan() const { return plan_; }
    const ProductAssociationCell& cell() const & { return cell_; }
    const ProductAssociationCell& cell() const && = delete;
    const std::vector<ProductAssociationFlag>& flags() const & { return flags_; }
    const std::vector<ProductAssociationFlag>& flags() const && = delete;
private:
    friend class OwnedProductAssociator;
    ProductAssociatorCellWitness(OwnedProductAssociator p, ProductAssociationCell c, std::vector<ProductAssociationFlag> f)
        : plan_(std::move(p)), cell_(std::move(c)), flags_(std::move(f)) {}
    OwnedProductAssociator plan_;
    ProductAssociationCell cell_;
    std::vector<ProductAssociationFlag> flags_;
};
inline ProductAssociatorCellWitness OwnedProductAssociator::cell_witness(LeftAssociatedCell c, product_associator_detail::Guard& guard) const {
    const auto pin = state_; guard.step(); product_associator_detail::validate(*pin, guard);
    return ProductAssociatorCellWitness(OwnedProductAssociator(pin), product_associator_detail::certificate(*pin, c), product_associator_detail::flags(*pin, c, guard));
}
class ProductAssociatorChainWitness {
public:
    const OwnedAssociatedProductField& input() const & { return input_; }
    const OwnedAssociatedProductField& input() const && = delete;
    const OwnedAssociatedProductField& image() const & { return image_; }
    const OwnedAssociatedProductField& image() const && = delete;
    const OwnedAssociatedProductField& round_trip() const & { return round_trip_; }
    const OwnedAssociatedProductField& round_trip() const && = delete;
    const std::optional<OwnedAssociatedProductField>& mapped_boundary() const & { return mapped_boundary_; }
    const std::optional<OwnedAssociatedProductField>& mapped_boundary() const && = delete;
    const std::optional<OwnedAssociatedProductField>& image_boundary() const & { return image_boundary_; }
    const std::optional<OwnedAssociatedProductField>& image_boundary() const && = delete;
    OwnedProductAssociator plan() const { return input_.plan(); }
private:
    friend class OwnedProductAssociator;
    ProductAssociatorChainWitness(OwnedAssociatedProductField a, OwnedAssociatedProductField b, OwnedAssociatedProductField c,
        std::optional<OwnedAssociatedProductField> d, std::optional<OwnedAssociatedProductField> e)
        : input_(std::move(a)), image_(std::move(b)), round_trip_(std::move(c)), mapped_boundary_(std::move(d)), image_boundary_(std::move(e)) {}
    OwnedAssociatedProductField input_, image_, round_trip_;
    std::optional<OwnedAssociatedProductField> mapped_boundary_, image_boundary_;
};
inline ProductAssociatorChainWitness OwnedProductAssociator::witness(OwnedAssociatedProductField input, product_associator_detail::Guard& guard) const {
    const auto owner = *this; auto mapped = owner.reassociate(input, guard), round_trip = owner.reassociate(mapped, guard);
    product_associator_detail::require(round_trip.values() == input.values(), "associator round trip changes structural support");
    std::optional<OwnedAssociatedProductField> before, after;
    if (input.grade().value) {
        before = owner.reassociate(owner.boundary(input, guard), guard); after = owner.boundary(mapped, guard);
        product_associator_detail::require(before->values() == after->values(), "associator has a nonzero chain naturality residual");
    }
    return ProductAssociatorChainWitness(std::move(input), std::move(mapped), std::move(round_trip), std::move(before), std::move(after));
}
} // namespace rfc::algebra
