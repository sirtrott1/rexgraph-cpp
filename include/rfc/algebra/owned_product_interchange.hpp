#pragma once
#include "rfc/algebra/owned_tensor_product.hpp"
#include <array>

namespace rfc::algebra {
enum class ProductFactorOrder { AB, BA };
struct ProductInterchangeTypeImage {
    std::uint64_t ab{}, ba{};
    bool operator==(const ProductInterchangeTypeImage&) const = default;
};
struct ProductInterchangeDeclaration {
    std::uint64_t key{}, revision{};
    RelationalProductDeclaration ab, ba;
    // An additional signature declaration, including every unused pair.
    std::vector<ProductInterchangeTypeImage> types;
};
struct ProductInterchangeFlag {
    RetainedProductOccurrence source, target;
    std::size_t source_word_position{}, target_word_position{};
    bool upper_interchange_flip{}, lower_interchange_flip{};
    numeric::Rational mapped_boundary_coefficient, image_boundary_coefficient;
};
namespace product_interchange_detail {
using owned_product_detail::Q;
using owned_product_detail::Guard;
using owned_product_detail::Values;
using owned_product_detail::add;
using owned_product_detail::mul;
using owned_product_detail::grade;
using owned_product_detail::require;
inline std::size_t index(ProductFactorOrder order) {
    require(order == ProductFactorOrder::AB || order == ProductFactorOrder::BA, "unknown product factor order");
    return order == ProductFactorOrder::AB ? 0u : 1u;
}
inline ProductFactorOrder opposite(ProductFactorOrder order) {
    return index(order) == 0 ? ProductFactorOrder::BA : ProductFactorOrder::AB;
}
inline ProductCell image(ProductCell c) { return {c.right, c.left}; }
inline bool flip(ProductCell c) { return c.left.grade.value % 2u != 0 && c.right.grade.value % 2u != 0; }
inline std::size_t words(const std::array<const core::RelationalComplex*, 2>& factors, const ProductInterchangeDeclaration& d) {
    auto n = add(8, add(mul(add(d.ab.types.size(), d.ba.types.size()), 3), mul(d.types.size(), 2)));
    std::array<std::size_t, 2> cells{}, flags{};
    for (std::size_t f = 0; f < 2; ++f) {
        const auto& c = *factors[f]; n = add(n, mul(2, owned_product_detail::source_words(c)));
        for (std::size_t k = 0; k < c.grade_count(); ++k) {
            cells[f] = add(cells[f], c.grade(grade(k)).cell_count());
            flags[f] = add(flags[f], mul(c.grade(grade(k)).incidence_count(), add(20, c.signature().aspects().size())));
        }
    }
    n = add(n, mul(16, mul(cells[0], cells[1])));
    for (std::size_t f = 0; f < 2; ++f) n = add(n, mul(2, mul(flags[f], cells[1-f])));
    return n;
}
struct State {
    std::array<OwnedRelationalTensorProduct, 2> products;
    ProductInterchangeDeclaration declaration;
    std::map<std::uint64_t, std::uint64_t> types;
    std::vector<ProductCell> cells;
    std::vector<std::size_t> grade_counts;
    std::size_t words{}, envelope{};
};
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope);
    for (const auto& p : s.products) p.validate(guard);
    guard.dimension(s.grade_counts.size());
    for (const auto count : s.grade_counts) { guard.step(); guard.dimension(count); }
    for (const auto& t : s.declaration.types) { (void)t; guard.step(); }
}
inline std::vector<RetainedProductOccurrence> routes(const OwnedRelationalTensorProduct& p, ProductCell c, Guard& guard) {
    return c.grade().value ? p.retained_boundary(c, guard) : std::vector<RetainedProductOccurrence>{};
}
struct Certificate {
    RetainedProductCell source, target;
    std::vector<RetainedProductOccurrence> source_flags, target_flags;
    std::vector<ProductInterchangeFlag> flags;
    bool interchange_flip{};
};
inline Certificate certificate(const State& s, ProductFactorOrder order, ProductCell c, Guard& guard) {
    const auto i = index(order); const auto& source = s.products[i]; const auto& target = s.products[1-i];
    Certificate out{source.cell(c), target.cell(image(c)), routes(source, c, guard), routes(target, image(c), guard), {}, flip(c)};
    require((i == 0 ? s.types.at(out.source.type) == out.target.type : s.types.at(out.target.type) == out.source.type),
        "interchange cell violates its complete type declaration");
    require(out.source.source_identity.left == out.target.source_identity.right &&
        out.source.source_identity.right == out.target.source_identity.left, "interchange must retain the original primary identities");
    const auto n = out.source_flags.size(); require(n == out.target_flags.size(), "interchange flag cardinality");
    guard.entries(add(s.envelope, mul(n, 64)));
    std::size_t left{};
    for (const auto& flag : out.source_flags) if (flag.side == ProductBoundarySide::Left) ++left;
    const auto right = n-left;
    out.flags.reserve(n);
    for (std::size_t p = 0; p < n; ++p) {
        guard.step(); const auto q = p < left ? add(right,p) : p-left;
        const auto& a = out.source_flags[p]; const auto& b = out.target_flags[q];
        require(b.upper == image(a.upper) && b.lower == image(a.lower) && a.side != b.side &&
            a.factor_occurrence == b.factor_occurrence && a.attaching_position == b.attaching_position &&
            a.exists == b.exists && a.factor_aspects == b.factor_aspects && a.factor_coefficient == b.factor_coefficient,
            "interchange must preserve each original factor flag and its attaching order");
        const auto lower_flip = flip(a.lower);
        auto mapped = lower_flip ? guard.sub(Q{0},a.coefficient) : a.coefficient;
        auto direct = out.interchange_flip ? guard.sub(Q{0},b.coefficient) : b.coefficient;
        require(mapped == direct, "graded interchange boundary flag identity");
        out.flags.push_back({a,b,p,q,out.interchange_flip,lower_flip,std::move(mapped),std::move(direct)});
    }
    return out;
}
} // namespace product_interchange_detail
class ProductInterchangeCellWitness;
class ProductInterchangeChainWitness;
class OwnedProductInterchange {
public:
    using Values = OwnedRelationalTensorProduct::Values;
    static OwnedProductInterchange prepare(std::array<core::RelationalComplex, 2> factors,
        ProductInterchangeDeclaration input, numeric::ExactActionGuard& guard) {
        namespace detail = product_interchange_detail;
        // Freeze movable declarations before the first user checkpoint.
        auto d = input;
        const auto words = detail::words({&factors[0],&factors[1]},d), envelope = detail::add(1,detail::mul(words,16));
        guard.entries(detail::mul(envelope,2));
        detail::require(d.key && d.revision && d.ab.key && d.ab.revision && d.ba.key && d.ba.revision,
            "interchange and both products require declared identities and revisions");
        auto ab = OwnedRelationalTensorProduct::prepare(factors[0],factors[1],d.ab,guard);
        auto ba = OwnedRelationalTensorProduct::prepare(factors[1],factors[0],d.ba,guard);
        detail::require(d.types.size() == d.ab.types.size() && d.types.size() == d.ba.types.size(),
            "interchange must declare all product type pairs, including unused directions");
        std::map<std::uint64_t,ExternalProductType> left, right;
        for (const auto& t : d.ab.types) { guard.step(); left.emplace(t.key,t); }
        for (const auto& t : d.ba.types) { guard.step(); right.emplace(t.key,t); }
        std::map<std::uint64_t,std::uint64_t> types; std::set<std::uint64_t> targets;
        for (const auto& t : d.types) {
            guard.step(); detail::require(left.contains(t.ab) && right.contains(t.ba), "interchange names an undeclared product type");
            const auto& a = left.at(t.ab); const auto& b = right.at(t.ba);
            detail::require(a.left == b.right && a.right == b.left && types.emplace(t.ab,t.ba).second && targets.insert(t.ba).second,
                "interchange needs a complete bijection of the swapped original type pairs");
        }
        const auto maximum = ab.max_grade();
        guard.dimension(detail::add(maximum.value,1));
        std::vector<std::size_t> counts(detail::add(maximum.value,1)); std::vector<ProductCell> cells;
        for (std::size_t k = 0; k < counts.size(); ++k) {
            guard.step(); counts[k] = ab.cell_count(detail::grade(k)); guard.dimension(counts[k]);
        }
        for (std::size_t p = 0; p < factors[0].grade_count(); ++p) for (std::size_t q = 0; q < factors[1].grade_count(); ++q) {
            const auto n = owned_product_detail::count(factors[0],p), m = owned_product_detail::count(factors[1],q);
            detail::require(n <= std::numeric_limits<std::uint32_t>::max() && m <= std::numeric_limits<std::uint32_t>::max(),
                "interchange factor exceeds the declared cell-index representation");
            for (std::size_t a = 0; a < n; ++a) for (std::size_t b = 0; b < m; ++b) {
                guard.step(); cells.push_back({{detail::grade(p),static_cast<std::uint32_t>(a)}, {detail::grade(q),static_cast<std::uint32_t>(b)}});
            }
        }
        auto state = std::make_shared<const product_interchange_detail::State>(product_interchange_detail::State{
            {std::move(ab),std::move(ba)},std::move(d),std::move(types),std::move(cells),std::move(counts),words,envelope});
        detail::validate(*state,guard);
        for (const auto c : state->cells) { guard.step(); (void)detail::certificate(*state,ProductFactorOrder::AB,c,guard); }
        return OwnedProductInterchange(std::move(state));
    }
    bool same_owner(const OwnedProductInterchange& other) const noexcept { return state_ == other.state_; }
    OwnedRelationalTensorProduct product(ProductFactorOrder order) const { return state_->products[product_interchange_detail::index(order)]; }
    const ProductInterchangeDeclaration& declaration() const & { return state_->declaration; }
    const ProductInterchangeDeclaration& declaration() const && = delete;
    const std::vector<ProductCell>& cells() const & { return state_->cells; }
    const std::vector<ProductCell>& cells() const && = delete;
    core::Grade max_grade() const { return state_->products[0].max_grade(); }
    std::size_t retained_words() const noexcept { return state_->words; }
    std::size_t entry_envelope() const noexcept { return state_->envelope; }
    ProductFactorOrder order(const OwnedProductField& input) const {
        for (const auto a : {ProductFactorOrder::AB,ProductFactorOrder::BA}) if (input.product().same_owner(product(a))) return a;
        throw std::invalid_argument("interchange requires its actual owned product field");
    }
    OwnedProductField field(ProductFactorOrder order, core::Grade g, Values values, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; auto frozen = values;
        const auto i = product_interchange_detail::index(order); product_interchange_detail::validate(*pin,guard);
        return pin->products[i].field(g,std::move(frozen),guard);
    }
    OwnedProductField boundary(OwnedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; const auto a = OwnedProductInterchange(pin).order(input);
        product_interchange_detail::validate(*pin,guard);
        return pin->products[product_interchange_detail::index(a)].boundary(std::move(input),guard);
    }
    OwnedProductField interchange(OwnedProductField input, numeric::ExactActionGuard& guard) const {
        namespace detail = product_interchange_detail; const auto pin = state_; const auto a = OwnedProductInterchange(pin).order(input);
        detail::validate(*pin,guard); guard.entries(detail::add(pin->envelope,detail::mul(input.values().size(),2))); Values out;
        for (const auto& [c,q] : input.values()) {
            guard.step(); guard.value(q); out.emplace(detail::image(c),detail::flip(c) ? guard.sub(numeric::Rational{0},q) : q);
        }
        return pin->products[1-detail::index(a)].field(input.grade(),std::move(out),guard);
    }
    ProductInterchangeCellWitness cell_witness(ProductFactorOrder, ProductCell, numeric::ExactActionGuard&) const;
    ProductInterchangeChainWitness witness(OwnedProductField, numeric::ExactActionGuard&) const;
private:
    explicit OwnedProductInterchange(std::shared_ptr<const product_interchange_detail::State> state) : state_(std::move(state)) {}
    std::shared_ptr<const product_interchange_detail::State> state_;
};
class ProductInterchangeCellWitness {
public:
    OwnedProductInterchange plan() const { return plan_; }
    ProductFactorOrder order() const noexcept { return order_; }
    const RetainedProductCell& source() const & { return certificate_.source; }
    const RetainedProductCell& source() const && = delete;
    const RetainedProductCell& target() const & { return certificate_.target; }
    const RetainedProductCell& target() const && = delete;
    const std::vector<RetainedProductOccurrence>& source_flags() const & { return certificate_.source_flags; }
    const std::vector<RetainedProductOccurrence>& source_flags() const && = delete;
    const std::vector<RetainedProductOccurrence>& target_flags() const & { return certificate_.target_flags; }
    const std::vector<RetainedProductOccurrence>& target_flags() const && = delete;
    const std::vector<ProductInterchangeFlag>& flags() const & { return certificate_.flags; }
    const std::vector<ProductInterchangeFlag>& flags() const && = delete;
    bool interchange_flip() const noexcept { return certificate_.interchange_flip; }
private:
    friend class OwnedProductInterchange;
    ProductInterchangeCellWitness(OwnedProductInterchange p, ProductFactorOrder o, product_interchange_detail::Certificate c)
        : plan_(std::move(p)),order_(o),certificate_(std::move(c)) {}
    OwnedProductInterchange plan_;
    ProductFactorOrder order_;
    product_interchange_detail::Certificate certificate_;
};
class ProductInterchangeChainWitness {
public:
    OwnedProductInterchange plan() const { return plan_; }
    const OwnedProductField& input() const & { return input_; }
    const OwnedProductField& input() const && = delete;
    const OwnedProductField& image() const & { return image_; }
    const OwnedProductField& image() const && = delete;
    const OwnedProductField& round_trip() const & { return round_trip_; }
    const OwnedProductField& round_trip() const && = delete;
    const std::optional<OwnedProductField>& mapped_boundary() const & { return mapped_; }
    const std::optional<OwnedProductField>& mapped_boundary() const && = delete;
    const std::optional<OwnedProductField>& image_boundary() const & { return direct_; }
    const std::optional<OwnedProductField>& image_boundary() const && = delete;
private:
    friend class OwnedProductInterchange;
    ProductInterchangeChainWitness(OwnedProductInterchange p, OwnedProductField input, OwnedProductField image,
        OwnedProductField round_trip, std::optional<OwnedProductField> mapped, std::optional<OwnedProductField> direct)
        : plan_(std::move(p)),input_(std::move(input)),image_(std::move(image)),round_trip_(std::move(round_trip)),mapped_(std::move(mapped)),direct_(std::move(direct)) {}
    OwnedProductInterchange plan_;
    OwnedProductField input_,image_,round_trip_;
    std::optional<OwnedProductField> mapped_,direct_;
};
inline ProductInterchangeCellWitness OwnedProductInterchange::cell_witness(ProductFactorOrder order, ProductCell c, numeric::ExactActionGuard& guard) const {
    const auto pin = state_; product_interchange_detail::validate(*pin,guard);
    return ProductInterchangeCellWitness(OwnedProductInterchange(pin),order,product_interchange_detail::certificate(*pin,order,c,guard));
}
inline ProductInterchangeChainWitness OwnedProductInterchange::witness(OwnedProductField input, numeric::ExactActionGuard& guard) const {
    namespace detail = product_interchange_detail; const auto pin = state_; const OwnedProductInterchange owner(pin);
    (void)owner.order(input); detail::validate(*pin,guard);
    auto image = owner.interchange(input,guard), round_trip = owner.interchange(image,guard);
    detail::require(round_trip.product().same_owner(input.product()) && round_trip.values() == input.values(), "interchange round trip");
    std::optional<OwnedProductField> mapped, direct;
    if (input.grade().value) {
        mapped = owner.interchange(owner.boundary(input,guard),guard); direct = owner.boundary(image,guard);
        detail::require(mapped->product().same_owner(direct->product()) && mapped->values() == direct->values(), "interchange boundary naturality");
    }
    return ProductInterchangeChainWitness(owner,std::move(input),std::move(image),std::move(round_trip),std::move(mapped),std::move(direct));
}
} // namespace rfc::algebra
