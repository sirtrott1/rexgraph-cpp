#pragma once
#include <limits>

#include "rfc/composite/declaration.hpp"
#include "rfc/numeric/sparse_tensor.hpp"
#include <climits>
#include <memory>
#include <optional>
#include <variant>

namespace rfc::composite {

// V1 remains the default. V2 changes only RationalTensor support encoding;
// declarations and endpoint archives retain this explicit discriminator.
// Canonical bytes never contain host pointers or padding.
enum class BooleanLayoutProfile : std::uint8_t { Lsb0OctetsV1 = 1, Lsb0OctetsV2 = 2 };
inline void validate_layout_profile(BooleanLayoutProfile profile) {
    declaration_detail::require((profile == BooleanLayoutProfile::Lsb0OctetsV1 ||
                                 profile == BooleanLayoutProfile::Lsb0OctetsV2) && CHAR_BIT == 8,
                                "unsupported Boolean layout profile/host octet");
}

struct DeclaredValue {
    struct Symbol { std::uint64_t key{}; bool operator==(const Symbol&) const = default; };
    struct Product { std::vector<DeclaredValue> elements; };
    struct Alternative { std::uint64_t key{}; std::vector<DeclaredValue> element; };
    struct Optional { std::vector<DeclaredValue> element; }; // zero or one; no ambiguous null sentinel
    struct Repeated { std::vector<DeclaredValue> elements; };
    using Data = std::variant<bool, Symbol, numeric::Integer, numeric::Rational,
                              Product, Alternative, Optional, Repeated, numeric::SparseRationalTensor>;
    Data data{false};

    [[nodiscard]] static DeclaredValue boolean(bool x) { return {Data{x}}; }
    [[nodiscard]] static DeclaredValue symbol(std::uint64_t x) { return {Data{Symbol{x}}}; }
    [[nodiscard]] static DeclaredValue integer(numeric::Integer x) { return {Data{std::move(x)}}; }
    [[nodiscard]] static DeclaredValue rational(numeric::Rational x) { return {Data{std::move(x)}}; }
    [[nodiscard]] static DeclaredValue product(std::vector<DeclaredValue> x) { return {Data{Product{std::move(x)}}}; }
    [[nodiscard]] static DeclaredValue alternative(std::uint64_t key, DeclaredValue x) {
        return {Data{Alternative{key, {std::move(x)}}}};
    }
    [[nodiscard]] static DeclaredValue absent() { return {Data{Optional{}}}; }
    [[nodiscard]] static DeclaredValue present(DeclaredValue x) { return {Data{Optional{{std::move(x)}}}}; }
    [[nodiscard]] static DeclaredValue repeated(std::vector<DeclaredValue> x) { return {Data{Repeated{std::move(x)}}}; }
    [[nodiscard]] static DeclaredValue tensor(numeric::SparseRationalTensor x) { return {Data{std::move(x)}}; }
};

inline bool declared_value_equal(const DeclaredValue& a, const DeclaredValue& b) {
    if (a.data.index() != b.data.index()) return false;
    auto sequence_equal = [](const auto& x, const auto& y) {
        if (x.size() != y.size()) return false;
        for (std::size_t i = 0; i < x.size(); ++i) if (!declared_value_equal(x[i], y[i])) return false;
        return true;
    };
    switch (a.data.index()) {
        case 0: return std::get<0>(a.data) == std::get<0>(b.data);
        case 1: return std::get<1>(a.data) == std::get<1>(b.data);
        case 2: return std::get<2>(a.data) == std::get<2>(b.data);
        case 3: return std::get<3>(a.data) == std::get<3>(b.data);
        case 4: return sequence_equal(std::get<4>(a.data).elements, std::get<4>(b.data).elements);
        case 5: return std::get<5>(a.data).key == std::get<5>(b.data).key &&
                       sequence_equal(std::get<5>(a.data).element, std::get<5>(b.data).element);
        case 6: return sequence_equal(std::get<6>(a.data).element, std::get<6>(b.data).element);
        case 7: return sequence_equal(std::get<7>(a.data).elements, std::get<7>(b.data).elements);
        case 8: {
            const auto& x = std::get<8>(a.data); const auto& y = std::get<8>(b.data);
            if (x.domain() != y.domain() || x.support().count() != y.support().count() ||
                x.values().nonzeros() != y.values().nonzeros()) return false;
            bool same = true;
            // Equal full membership needs no logical-coordinate traversal. This
            // includes full sparse/packed variants and the empty domain. Temporal
            // recoding checks use this equality before classifying replacements.
            if (x.support().count() != x.domain().logical_size())
                x.support().for_each_active([&](auto i) { if (!y.support().contains(i)) same = false; });
            const auto& xs = x.values().entries(); const auto& ys = y.values().entries();
            for (std::size_t i = 0; i < xs.size(); ++i)
                if (xs[i].index != ys[i].index || xs[i].value != ys[i].value) return false;
            return same;
        }
        default: throw std::invalid_argument("valueless declared value");
    }
}

struct LayoutBounds {
    std::size_t minimum_bits{};
    // nullopt means variable exact magnitude, not unbounded allocation permission.
    std::optional<std::size_t> maximum_bits;
};
struct LayoutAllocation {
    std::size_t bits{}, bytes{}, words{}, value_nodes{}, materialized_bytes{};
};

namespace declaration_detail {
inline std::size_t integer_bits(const numeric::Integer& x) {
    return x == 0 ? 0 : mpz_sizeinbase(x.get_mpz_t(), 2);
}
inline std::size_t magnitude_bits(const numeric::Integer& x, const DeclarationLimits& limits) {
    const auto n = integer_bits(x);
    budget(n, limits.max_integer_bits, "exact integer magnitude budget");
    const auto bytes = machine::ceil_div(n, 8);
    return machine::checked_size_mul(machine::checked_size_add(varuint_bytes(bytes), bytes), 8);
}
inline std::size_t signed_integer_bits(const numeric::Integer& x, const DeclarationLimits& limits) {
    return machine::checked_size_add(1, magnitude_bits(x, limits));
}
inline std::size_t rational_bits(const numeric::Rational& x, const DeclarationLimits& limits) {
    const numeric::Integer n = x.get_num(), d = x.get_den();
    const auto bits = machine::checked_size_add(signed_integer_bits(n, limits), magnitude_bits(d, limits));
    require(d > 0, "nonpositive rational denominator");
    numeric::Integer gcd;
    mpz_gcd(gcd.get_mpz_t(), n.get_mpz_t(), d.get_mpz_t());
    require(gcd == 1, "noncanonical rational value");
    return bits;
}
} // namespace declaration_detail

// Layout views share this registry after validation under one resource policy.
class PreparedLayoutRegistry {
public:
    explicit PreparedLayoutRegistry(DeclarationRegistry declarations, DeclarationLimits limits = {})
        : limits_(limits), declarations_(std::make_shared<const DeclarationRegistry>(declarations.definitions(), limits_)) {}
    PreparedLayoutRegistry(const PreparedLayoutRegistry&) = default;
    PreparedLayoutRegistry& operator=(const PreparedLayoutRegistry&) = default;
    [[nodiscard]] const DeclarationRegistry& declarations() const noexcept { return *declarations_; }
    [[nodiscard]] const DeclarationLimits& limits() const noexcept { return limits_; }
private:
    DeclarationLimits limits_;
    std::shared_ptr<const DeclarationRegistry> declarations_;
};

// Owning compilation: no borrowed registry that can change under an executing
// codec. Meaningful factor/symbol order is retained; no Cartesian product is formed.
class CompiledLayout {
public:
    CompiledLayout(DeclarationRegistry declarations, DeclarationId root,
                   BooleanLayoutProfile profile = BooleanLayoutProfile::Lsb0OctetsV1,
                   DeclarationLimits limits = {})
        : CompiledLayout(PreparedLayoutRegistry(std::move(declarations), limits), root, profile) {}
    CompiledLayout(PreparedLayoutRegistry declarations, DeclarationId root,
                   BooleanLayoutProfile profile = BooleanLayoutProfile::Lsb0OctetsV1)
        : declarations_(std::move(declarations)), root_(root), profile_(profile), limits_(declarations_.limits()) {
        validate_layout_profile(profile_); limits_.validate();
        (void)declarations_.declarations().at(root_);
        bounds_ = compute_bounds(root_);
    }
    [[nodiscard]] const DeclarationRegistry& declarations() const noexcept { return declarations_.declarations(); }
    [[nodiscard]] DeclarationId root() const noexcept { return root_; }
    [[nodiscard]] BooleanLayoutProfile profile() const noexcept { return profile_; }
    [[nodiscard]] const DeclarationLimits& limits() const noexcept { return limits_; }
    [[nodiscard]] LayoutBounds bounds() const noexcept { return bounds_; }
    [[nodiscard]] LayoutAllocation allocation(const DeclaredValue& value) const {
        std::size_t nodes = 0, materialized = 0;
        const auto bits = inspect(root_, value, 1, nodes, materialized);
        const auto bytes = machine::ceil_div(bits, 8);
        declaration_detail::budget(bytes, limits_.max_encoded_bytes, "encoded value budget");
        return {bits, bytes, machine::ceil_div(bits, 64), nodes, materialized};
    }
    void validate(const DeclaredValue& value) const { (void)allocation(value); }

    // Exact scalar growth, not machine-lane wrap. This small action reuses GMP;
    // schema/layout migration and a general operation compiler belong to later batches.
    [[nodiscard]] DeclaredValue add_exact(const DeclaredValue& a, const DeclaredValue& b) const {
        validate(a); validate(b); DeclaredValue result;
        switch (declarations_.declarations().at(root_).kind) {
            case DeclarationKind::Integer:
                result = DeclaredValue::integer(numeric::Integer(std::get<numeric::Integer>(a.data) +
                                                                 std::get<numeric::Integer>(b.data))); break;
            case DeclarationKind::Rational:
                result = DeclaredValue::rational(numeric::Rational(std::get<numeric::Rational>(a.data) +
                                                                   std::get<numeric::Rational>(b.data))); break;
            default: throw std::invalid_argument("exact addition requires an integer/rational declaration");
        }
        validate(result); return result;
    }
private:
    LayoutBounds compute_bounds(DeclarationId id) {
        using namespace declaration_detail;
        if (const auto it = bound_cache_.find(id); it != bound_cache_.end()) return it->second;
        const auto& d = declarations_.declarations().at(id); LayoutBounds b;
        auto add = machine::checked_size_add; auto mul = machine::checked_size_mul;
        switch (d.kind) {
            case DeclarationKind::Boolean: b = {1, 1}; break;
            case DeclarationKind::Finite: b = {tag_bits(d.symbols.size()), tag_bits(d.symbols.size())}; break;
            case DeclarationKind::Integer: b = {9, std::nullopt}; break;
            case DeclarationKind::Rational: b = {25, std::nullopt}; break;
            case DeclarationKind::Product:
                b = {0, 0};
                for (const auto& f : d.factors) {
                    const auto child = compute_bounds(f.type); b.minimum_bits = add(b.minimum_bits, child.minimum_bits);
                    if (b.maximum_bits && child.maximum_bits) b.maximum_bits = add(*b.maximum_bits, *child.maximum_bits);
                    else b.maximum_bits.reset();
                } break;
            case DeclarationKind::Alternative: {
                const auto tag = tag_bits(d.factors.size());
                auto first = compute_bounds(d.factors.front().type); b = first;
                for (const auto& f : d.factors) {
                    const auto child = compute_bounds(f.type); b.minimum_bits = std::min(b.minimum_bits, child.minimum_bits);
                    if (b.maximum_bits && child.maximum_bits) b.maximum_bits = std::max(*b.maximum_bits, *child.maximum_bits);
                    else b.maximum_bits.reset();
                }
                b.minimum_bits = add(tag, b.minimum_bits);
                if (b.maximum_bits) b.maximum_bits = add(tag, *b.maximum_bits);
                break;
            }
            case DeclarationKind::Optional: {
                const auto child = compute_bounds(d.element); b = {1, std::nullopt};
                if (child.maximum_bits) b.maximum_bits = add(1, *child.maximum_bits);
                break;
            }
            case DeclarationKind::Repeated: {
                const auto child = compute_bounds(d.element);
                b = {add(mul(varuint_bytes(d.min_count), 8), mul(d.min_count, child.minimum_bits)), std::nullopt};
                if (d.max_count == 0) b.maximum_bits = 8;
                else if (child.maximum_bits) b.maximum_bits = add(mul(varuint_bytes(d.max_count), 8), mul(d.max_count, *child.maximum_bits));
                break;
            }
            case DeclarationKind::RationalTensor:
                b = {16, DeclarationRegistry::tensor_domain(d).logical_size() == 0 ? std::optional<std::size_t>{16} : std::nullopt};
                break;
            default: throw std::invalid_argument("unknown layout constructor");
        }
        bound_cache_.emplace(id, b); return b;
    }
    std::size_t inspect(DeclarationId id, const DeclaredValue& value, std::size_t depth,
                        std::size_t& nodes, std::size_t& materialized) const {
        using namespace declaration_detail;
        budget(depth, limits_.max_depth, "value nesting budget");
        nodes = machine::checked_size_add(nodes, 1); budget(nodes, limits_.max_value_nodes, "value work budget");
        const auto& d = declarations_.declarations().at(id); std::size_t bits = 0;
        auto accumulate = [&](std::size_t n) {
            bits = machine::checked_size_add(bits, n);
            budget(machine::ceil_div(bits, 8), limits_.max_encoded_bytes, "encoded value budget");
        };
        auto allocate = [&](std::size_t count, std::size_t size) {
            materialized = machine::checked_size_add(materialized, machine::checked_size_mul(count, size));
            budget(materialized, limits_.max_materialized_bytes, "value materialization budget");
        };
        auto magnitude_allocation = [&](const numeric::Integer& x) { allocate(machine::ceil_div(integer_bits(x), 8), 1); };
        auto rational_allocation = [&](const numeric::Rational& q) { magnitude_allocation(q.get_num()); magnitude_allocation(q.get_den()); };
        auto child = [&](DeclarationId type, const DeclaredValue& v) { accumulate(inspect(type, v, depth + 1, nodes, materialized)); };
        switch (d.kind) {
            case DeclarationKind::Boolean:
                require(std::holds_alternative<bool>(value.data), "expected Boolean value"); bits = 1; break;
            case DeclarationKind::Finite: {
                const auto* p = std::get_if<DeclaredValue::Symbol>(&value.data);
                require(p, "expected finite symbol");
                (void)declarations_.declarations().symbol_ordinal(d.id, p->key);
                bits = tag_bits(d.symbols.size()); break;
            }
            case DeclarationKind::Integer: {
                const auto* p = std::get_if<numeric::Integer>(&value.data); require(p, "expected integer value");
                bits = signed_integer_bits(*p, limits_); magnitude_allocation(*p); break;
            }
            case DeclarationKind::Rational: {
                const auto* p = std::get_if<numeric::Rational>(&value.data); require(p, "expected rational value");
                bits = rational_bits(*p, limits_); rational_allocation(*p); break;
            }
            case DeclarationKind::Product: {
                const auto* p = std::get_if<DeclaredValue::Product>(&value.data);
                require(p && p->elements.size() == d.factors.size(), "product factor count/type");
                allocate(p->elements.size(), sizeof(DeclaredValue));
                for (std::size_t i = 0; i < d.factors.size(); ++i) child(d.factors[i].type, p->elements[i]);
                break;
            }
            case DeclarationKind::Alternative: {
                const auto* p = std::get_if<DeclaredValue::Alternative>(&value.data);
                require(p && p->element.size() == 1, "alternative payload count/type");
                const auto& factor = d.factors.at(declarations_.declarations().alternative_ordinal(d.id, p->key));
                bits = tag_bits(d.factors.size());
                allocate(1, sizeof(DeclaredValue)); child(factor.type, p->element.front()); break;
            }
            case DeclarationKind::Optional: {
                const auto* p = std::get_if<DeclaredValue::Optional>(&value.data);
                require(p && p->element.size() <= 1, "optional payload count/type"); bits = 1;
                if (!p->element.empty()) { allocate(1, sizeof(DeclaredValue)); child(d.element, p->element.front()); }
                break;
            }
            case DeclarationKind::Repeated: {
                const auto* p = std::get_if<DeclaredValue::Repeated>(&value.data);
                require(p && p->elements.size() >= d.min_count && p->elements.size() <= d.max_count, "repetition count/type");
                budget(p->elements.size(), limits_.max_value_nodes - nodes, "repetition work budget");
                allocate(p->elements.size(), sizeof(DeclaredValue));
                bits = machine::checked_size_mul(varuint_bytes(p->elements.size()), 8);
                for (const auto& v : p->elements) child(d.element, v);
                break;
            }
            case DeclarationKind::RationalTensor: {
                const auto* p = std::get_if<numeric::SparseRationalTensor>(&value.data);
                require(p && p->domain() == DeclarationRegistry::tensor_domain(d), "tensor value domain/type");
                const auto count = p->support().count();
                const bool compact = profile_ == BooleanLayoutProfile::Lsb0OctetsV2;
                // Canonicalize full support by membership, not its storage variant.
                const bool symbolic_full = compact && count == p->domain().logical_size();
                const auto records = symbolic_full ? 0 : count;
                nodes = machine::checked_size_add(nodes, machine::checked_size_add(records, p->stored_nonzeros()));
                budget(nodes, limits_.max_value_nodes, "tensor support/value work budget");
                allocate(records, sizeof(std::size_t));
                allocate(p->stored_nonzeros(), sizeof(numeric::SparseRationalEntry));
                bits = compact ? 8 : 0; // V2 support tag, even for the empty domain.
                if (!symbolic_full) {
                    accumulate(machine::checked_size_mul(varuint_bytes(count), 8));
                    if (p->support().storage() == numeric::StructuralSupport::Storage::Full) {
                        // V1 still writes every index. Count its widths symbolically
                        // without changing its historical allocation/admission policy.
                        std::size_t index_bytes = count;
                        for (std::size_t threshold = 128; threshold < count;) {
                            index_bytes = machine::checked_size_add(index_bytes, count - threshold);
                            if (threshold > std::numeric_limits<std::size_t>::max() / 128) break;
                            threshold *= 128;
                        }
                        accumulate(machine::checked_size_mul(index_bytes, 8));
                    } else {
                        p->support().for_each_active([&](auto i) { accumulate(machine::checked_size_mul(varuint_bytes(i), 8)); });
                    }
                }
                accumulate(machine::checked_size_mul(varuint_bytes(p->stored_nonzeros()), 8));
                for (const auto& e : p->values().entries()) {
                    accumulate(machine::checked_size_mul(varuint_bytes(e.index), 8));
                    accumulate(rational_bits(e.value, limits_)); rational_allocation(e.value);
                }
                break;
            }
            default: throw std::invalid_argument("unknown value constructor");
        }
        budget(machine::ceil_div(bits, 8), limits_.max_encoded_bytes, "encoded value budget");
        return bits;
    }
    PreparedLayoutRegistry declarations_;
    DeclarationId root_;
    BooleanLayoutProfile profile_;
    DeclarationLimits limits_;
    LayoutBounds bounds_;
    std::map<DeclarationId, LayoutBounds> bound_cache_;
};

} // namespace rfc::composite
