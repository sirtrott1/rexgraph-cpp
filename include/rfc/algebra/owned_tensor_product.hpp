#pragma once
#include "rfc/algebra/product_field.hpp"
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <map>
#include <memory>
#include <optional>
#include <set>

namespace rfc::algebra {
class OwnedRelationalTensorProduct;
struct RetainedProductOccurrence;
namespace owned_product_detail {
struct State;
std::vector<RetainedProductOccurrence> routes(std::shared_ptr<const State>, ProductCell, numeric::ExactActionGuard&);
}

// Grade-zero entities have no primary relation type. Their null type token is
// distinct from every positive-grade relation type, including type number zero.
struct ExternalProductType {
    std::optional<core::RelationTypeId> left, right;
    std::uint64_t key{};
    bool operator==(const ExternalProductType&) const = default;
};
struct RelationalProductDeclaration {
    std::uint64_t key{}, revision{};
    // One declared output type for every pair in the two complete signatures,
    // including the entity token. Original declaration order remains retained.
    std::vector<ExternalProductType> types;
};
struct RetainedProductCell {
    ProductCell cell;
    ProductCellIdentity source_identity;
    std::uint64_t type{};
    OwnedRelationalTensorProduct product() const;
private:
    friend class OwnedRelationalTensorProduct;
    RetainedProductCell(ProductCell id, ProductCellIdentity identity, std::uint64_t declared_type,
                        std::shared_ptr<const owned_product_detail::State> owner)
        : cell(id), source_identity(identity), type(declared_type), owner_(std::move(owner)) {}
    std::shared_ptr<const owned_product_detail::State> owner_;
};
struct RetainedProductOccurrence {
    ProductCell upper, lower;
    ProductBoundarySide side;
    core::OccurrenceKey factor_occurrence;
    std::size_t attaching_position{};
    bool koszul_flip{}, exists{};
    // The full original aspect word, including orientation, remains separate
    // from the additional product parity bit and its realized coefficient.
    std::vector<bool> factor_aspects;
    numeric::Rational factor_coefficient{0}, coefficient{0};
    OwnedRelationalTensorProduct product() const;
private:
    friend std::vector<RetainedProductOccurrence> owned_product_detail::routes(
        std::shared_ptr<const owned_product_detail::State>, ProductCell, numeric::ExactActionGuard&);
    RetainedProductOccurrence(ProductCell u, ProductCell l, ProductBoundarySide s, core::OccurrenceKey occurrence,
                              std::size_t position, bool flip, bool active, std::vector<bool> aspects,
                              numeric::Rational original, numeric::Rational realized,
                              std::shared_ptr<const owned_product_detail::State> owner)
        : upper(u), lower(l), side(s), factor_occurrence(occurrence), attaching_position(position), koszul_flip(flip),
          exists(active), factor_aspects(std::move(aspects)), factor_coefficient(std::move(original)),
          coefficient(std::move(realized)), owner_(std::move(owner)) {}
    std::shared_ptr<const owned_product_detail::State> owner_;
};

namespace owned_product_detail {
using Q = numeric::Rational;
using Matrix = numeric::ExactMatrix;
using Guard = numeric::ExactActionGuard;
using Values = std::map<ProductCell, Q>;
using Types = std::map<std::pair<std::optional<core::RelationTypeId>,
                                 std::optional<core::RelationTypeId>>, std::uint64_t>;
inline void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
inline std::size_t add(std::size_t a, std::size_t b) { return machine::checked_size_add(a, b); }
inline std::size_t mul(std::size_t a, std::size_t b) { return machine::checked_size_mul(a, b); }
inline core::Grade grade(std::size_t k) {
    if (k > std::numeric_limits<std::uint16_t>::max()) throw std::overflow_error("owned product grade overflow");
    return core::Grade{static_cast<std::uint16_t>(k)};
}
inline std::size_t count(const core::RelationalComplex& c, std::size_t k) {
    return k < c.grade_count() ? c.grade(grade(k)).cell_count() : 0;
}
inline std::size_t source_words(const core::RelationalComplex& c) {
    std::size_t n = c.grade_count();
    const auto& signature = c.signature();
    for (std::size_t j = 0; j < signature.aspects().size(); ++j)
        n = add(n, add(1, signature.aspects().at(static_cast<composite::AspectId>(j)).name.size()));
    for (std::size_t t = 0; t < signature.type_count(); ++t) {
        const auto& type = signature.type(static_cast<core::RelationTypeId>(t));
        n = add(n, add(add(1, type.name.size()), type.constraints.rules.size()));
    }
    for (std::size_t k = 0; k < c.grade_count(); ++k) {
        const auto& a = c.grade(grade(k));
        n = add(n, add(mul(a.cell_count(), 4), mul(a.incidence_count(), add(7, signature.aspects().size()))));
    }
    return n;
}
inline void cell(const core::RelationalComplex& c, core::CellId id) {
    if (id.grade.value >= c.grade_count() || id.index >= c.grade(id.grade).cell_count())
        throw std::out_of_range("owned product factor cell");
}
inline ProductFactorIdentity identity(const core::RelationalComplex& c, core::CellId id) {
    cell(c, id);
    const auto& a = c.grade(id.grade);
    return {id.grade, id.grade.value ? a.relation_identities.at(id.index).value : a.identities.at(id.index)};
}
inline std::optional<core::RelationTypeId> type(const core::RelationalComplex& c, core::CellId id) {
    cell(c, id);
    return id.grade.value ? std::optional{c.grade(id.grade).relation_types.at(id.index)} : std::nullopt;
}
inline Q coefficient(const core::RelationalComplex& c, core::Grade g, std::size_t p, Guard& guard) {
    const auto& a = c.grade(g);
    const auto& lower = c.grade(grade(g.value - 1u));
    Q lattice{a.lattice_coefficient.at(p)};
    guard.value(lattice);
    return guard.div(guard.mul(lower.cell_scales.at(a.lower_index.at(p)), lattice),
                     a.cell_scales.at(a.upper_index.at(p)));
}
inline void validate_source(const core::RelationalComplex& c, Guard& guard) {
    guard.dimension(c.grade_count());
    guard.dimension(c.signature().type_count());
    guard.dimension(c.signature().aspects().size());
    for (std::size_t t = 0; t < c.signature().type_count(); ++t) {
        const auto& d = c.signature().type(static_cast<core::RelationTypeId>(t));
        for (const auto& rule : d.constraints.rules) { guard.step(); guard.value(rule.value); }
    }
    for (std::size_t k = 0; k < c.grade_count(); ++k) {
        const auto& a = c.grade(grade(k));
        guard.dimension(a.cell_count());
        for (const auto& q : a.cell_scales) { guard.step(); guard.value(q); }
        for (std::size_t p = 0; p < a.incidence_count(); ++p) {
            guard.step(); (void)coefficient(c, grade(k), p, guard);
            for (std::size_t j = 0; j < c.signature().aspects().size(); ++j) {
                guard.step(); (void)a.aspects.get(static_cast<composite::AspectId>(j), p);
            }
        }
    }
}
inline Matrix boundary(const core::RelationalComplex& c, std::size_t k, Guard& guard) {
    require(k > 0, "owned product factor boundary at grade zero");
    Matrix b(count(c, k - 1), count(c, k));
    if (k >= c.grade_count()) return b;
    const auto& a = c.grade(grade(k));
    const auto exists = c.signature().existence_aspect();
    for (std::size_t p = 0; p < a.incidence_count(); ++p) {
        guard.step();
        if (a.aspects.get(exists, p)) {
            const auto i = a.lower_index[p], j = a.upper_index[p];
            b.set(i, j, guard.add(b.get(i, j), coefficient(c, grade(k), p, guard)));
            guard.entries(b.nonzeros());
        }
    }
    return b;
}
inline Matrix multiply(const Matrix& a, const Matrix& b, Guard& guard, std::size_t resident) {
    require(a.cols() == b.rows(), "owned product matrix contraction dimensions");
    guard.matrix(a); guard.matrix(b);
    const auto fixed = add(resident, add(a.nonzeros(), b.nonzeros()));
    guard.entries(fixed);
    Matrix out(a.rows(), b.cols());
    // Only factor realization maps are contracted here. The product carrier
    // is never flattened. Every scanned sparse pair pays the work guard.
    for (const auto& [ij, x] : a.entries()) for (const auto& [kl, y] : b.entries()) {
        guard.step(); if (ij.second != kl.first) continue;
        guard.entries(add(fixed, add(out.nonzeros(), 1)));
        out.set(ij.first, kl.second, guard.add(out.get(ij.first, kl.second), guard.mul(x, y)));
    }
    return out;
}
inline void chain(const core::RelationalComplex& c, Guard& guard, std::size_t resident) {
    for (std::size_t k = 2; k < c.grade_count(); ++k) {
        const auto lower = boundary(c, k - 1, guard), upper = boundary(c, k, guard);
        require(multiply(lower, upper, guard, resident).nonzeros() == 0,
                "owned product requires the complete strict factor chain family");
    }
}
struct State {
    core::RelationalComplex left, right;
    RelationalProductDeclaration declaration;
    Types types;
    std::size_t words{}, envelope{};
    core::Grade maximum;
};
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope);
    guard.value(Q{1}); guard.value(Q{0});
    for (const auto& t : s.declaration.types) { (void)t; guard.step(); }
    validate_source(s.left, guard); validate_source(s.right, guard);
}
inline void cell(const State& s, ProductCell id) {
    cell(s.left, id.left); cell(s.right, id.right); (void)id.grade();
}
inline void values(const State& s, core::Grade g, const Values& v, Guard& guard) {
    if (g > s.maximum) throw std::out_of_range("owned product field grade");
    guard.entries(add(s.envelope, v.size()));
    for (const auto& [id, q] : v) {
        guard.step(); cell(s, id); require(id.grade() == g, "owned product field mixes grades"); guard.value(q);
    }
}
inline std::vector<RetainedProductOccurrence> routes(std::shared_ptr<const State> owner, ProductCell upper, Guard& guard) {
    const auto& s = *owner;
    cell(s, upper); require(upper.grade().value > 0, "owned product grade zero has no boundary");
    std::vector<RetainedProductOccurrence> out;
    for (const bool right : {false, true}) {
        const auto& c = right ? s.right : s.left;
        const auto id = right ? upper.right : upper.left;
        if (id.grade.value == 0) continue;
        const auto& a = c.grade(id.grade);
        const auto [begin, end] = a.incidence_range(id.index);
        const bool flip = right && (upper.left.grade.value % 2u != 0);
        for (auto p = begin; p < end; ++p) {
            guard.step();
            guard.entries(add(s.envelope, mul(add(out.size(), 1), add(3, c.signature().aspects().size()))));
            auto lower = upper;
            const core::CellId participant{grade(id.grade.value - 1u), a.lower_index[p]};
            if (right) lower.right = participant; else lower.left = participant;
            auto q = coefficient(c, id.grade, p, guard);
            auto realized = flip ? guard.sub(Q{0}, q) : q;
            std::vector<bool> aspects;
            aspects.reserve(c.signature().aspects().size());
            for (std::size_t j = 0; j < c.signature().aspects().size(); ++j) {
                guard.step(); aspects.push_back(a.aspects.get(static_cast<composite::AspectId>(j), p));
            }
            out.push_back({upper, lower, right ? ProductBoundarySide::Right : ProductBoundarySide::Left,
                           a.occurrence_identities[p], p - begin, flip,
                           a.aspects.get(c.signature().existence_aspect(), p), std::move(aspects),
                           std::move(q), std::move(realized), owner});
        }
    }
    return out;
}
} // namespace owned_product_detail

class OwnedRelationalTensorProduct;
class PreparedTensorChainMap;
// A supported zero stays present. The immutable owner binds numeric cell
// positions to their actual two retained sources; equal keys do not bind owners.
class OwnedProductField {
public:
    core::Grade grade() const noexcept { return grade_; }
    const owned_product_detail::Values& values() const & noexcept { return values_; }
    const owned_product_detail::Values& values() const && = delete;
    OwnedRelationalTensorProduct product() const;
private:
    friend class OwnedRelationalTensorProduct;
    friend class PreparedTensorChainMap;
    OwnedProductField(std::shared_ptr<const owned_product_detail::State> owner, core::Grade g,
                      owned_product_detail::Values values)
        : owner_(std::move(owner)), grade_(g), values_(std::move(values)) {}
    std::shared_ptr<const owned_product_detail::State> owner_;
    core::Grade grade_;
    owned_product_detail::Values values_;
};

// Constructor-native binary source. Its complete cell carrier consists of
// ordered identity pairs. It never promotes derived linear-map axes to primary
// assertions or silently recodes this source into a scalar-key complex.
class OwnedRelationalTensorProduct {
public:
    using Values = owned_product_detail::Values;
    static OwnedRelationalTensorProduct prepare(core::RelationalComplex left, core::RelationalComplex right,
                                                RelationalProductDeclaration declaration,
                                                numeric::ExactActionGuard& guard) {
        using namespace owned_product_detail;
        const auto maximum = grade(add(left.max_grade().value, right.max_grade().value));
        require(declaration.key != 0, "owned product needs a declared constructor identity");
        const auto expected = mul(add(left.signature().type_count(), 1), add(right.signature().type_count(), 1));
        require(declaration.types.size() == expected, "owned product type declaration is not complete");
        const auto words = add(add(source_words(left), source_words(right)), mul(expected, 3));
        // Conservative named source/index/validation containers, not GMP/RSS.
        const auto envelope = add(1, mul(words, 8));
        guard.entries(envelope);
        Types types;
        std::set<std::uint64_t> keys;
        for (const auto& t : declaration.types) {
            guard.step();
            require(t.key && (!t.left || *t.left < left.signature().type_count()) &&
                    (!t.right || *t.right < right.signature().type_count()), "invalid declared product type");
            require(types.emplace(std::pair{t.left, t.right}, t.key).second && keys.insert(t.key).second,
                    "duplicate product type pair or output identity");
        }
        State state{std::move(left), std::move(right), std::move(declaration), std::move(types), words, envelope, maximum};
        owned_product_detail::validate(state, guard); chain(state.left, guard, envelope); chain(state.right, guard, envelope);
        return OwnedRelationalTensorProduct(std::make_shared<const State>(std::move(state)));
    }
    core::Grade max_grade() const noexcept { return state_->maximum; }
    std::size_t retained_words() const noexcept { return state_->words; }
    std::size_t entry_envelope() const noexcept { return state_->envelope; }
    bool same_owner(const OwnedRelationalTensorProduct& other) const noexcept { return state_ == other.state_; }
    const core::RelationalComplex& factor(bool right) const & { return right ? state_->right : state_->left; }
    const core::RelationalComplex& factor(bool) const && = delete;
    const RelationalProductDeclaration& declaration() const & { return state_->declaration; }
    const RelationalProductDeclaration& declaration() const && = delete;
    void validate(numeric::ExactActionGuard& guard) const {
        const auto pin = state_; owned_product_detail::validate(*pin, guard);
    }
    RetainedProductCell cell(ProductCell id) const {
        using namespace owned_product_detail;
        owned_product_detail::cell(*state_, id);
        return {id, {identity(state_->left, id.left), identity(state_->right, id.right)},
                state_->types.at({type(state_->left, id.left), type(state_->right, id.right)}), state_};
    }
    // Logical cardinality is an explicit observation, not an allocation.
    std::size_t cell_count(core::Grade g) const {
        using namespace owned_product_detail;
        if (g > max_grade()) throw std::out_of_range("owned product carrier grade");
        std::size_t n{};
        for (std::size_t p = 0; p <= g.value; ++p) n = add(n, mul(count(state_->left, p), count(state_->right, g.value - p)));
        return n;
    }
    std::vector<RetainedProductOccurrence> retained_boundary(ProductCell upper, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; owned_product_detail::validate(*pin, guard);
        return owned_product_detail::routes(pin, upper, guard);
    }
    OwnedProductField field(core::Grade g, Values values, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; owned_product_detail::validate(*pin, guard);
        owned_product_detail::values(*pin, g, values, guard);
        for (auto& [id, q] : values) { (void)id; guard.step(); q = numeric::canonical_rational(q); guard.value(q); }
        return OwnedProductField(pin, g, std::move(values));
    }
    OwnedProductField boundary(OwnedProductField input, numeric::ExactActionGuard& guard) const {
        using namespace owned_product_detail;
        const auto pin = state_;
        require(pin == input.owner_, "owned product boundary requires the actual field carrier");
        owned_product_detail::validate(*pin, guard); values(*pin, input.grade_, input.values_, guard);
        require(input.grade_.value > 0, "owned product grade zero has no boundary");
        Values out;
        for (const auto& [id, q] : input.values_) {
            const auto terms = routes(pin, id, guard);
            for (const auto& t : terms) {
                guard.step();
                guard.entries(add(pin->envelope, add(add(mul(input.values_.size(), 2), add(out.size(), 1)),
                              mul(terms.size(), add(3, std::max(pin->left.signature().aspects().size(), pin->right.signature().aspects().size()))))));
                auto& value = out.try_emplace(t.lower, Q{0}).first->second;
                if (t.exists) value = guard.add(value, guard.mul(q, t.coefficient));
            }
        }
        return OwnedProductField(pin, grade(input.grade_.value - 1u), std::move(out));
    }
private:
    friend class OwnedProductField;
    friend class PreparedTensorChainMap;
    friend struct RetainedProductCell;
    friend struct RetainedProductOccurrence;
    explicit OwnedRelationalTensorProduct(std::shared_ptr<const owned_product_detail::State> state) : state_(std::move(state)) {}
    std::shared_ptr<const owned_product_detail::State> state_;
};
inline OwnedRelationalTensorProduct OwnedProductField::product() const { return OwnedRelationalTensorProduct(owner_); }
inline OwnedRelationalTensorProduct RetainedProductCell::product() const { return OwnedRelationalTensorProduct(owner_); }
inline OwnedRelationalTensorProduct RetainedProductOccurrence::product() const { return OwnedRelationalTensorProduct(owner_); }

// Complete degree-zero rational chain maps on the actual factor sources.
// Linear maps do not assert preservation of relation types or occurrence fibers.
class PreparedTensorChainMap {
    using Matrix = numeric::ExactMatrix;
    using Guard = numeric::ExactActionGuard;
    struct State {
        OwnedRelationalTensorProduct source, target;
        std::vector<Matrix> left, right;
        std::size_t map_words{};
    };
    static std::size_t envelope(const State& s) {
        using namespace owned_product_detail;
        return add(add(s.source.entry_envelope(), s.target.entry_envelope()), mul(s.map_words, 8));
    }
public:
    static PreparedTensorChainMap prepare(OwnedRelationalTensorProduct source, OwnedRelationalTensorProduct target,
                                          std::vector<Matrix> left, std::vector<Matrix> right, Guard& guard) {
        using namespace owned_product_detail;
        source.validate(guard); target.validate(guard);
        std::size_t words = add(left.size(), right.size());
        for (const auto* maps : {&left, &right}) for (const auto& m : *maps) words = add(words, m.nonzeros());
        const auto resident = add(add(source.entry_envelope(), target.entry_envelope()), mul(words, 8));
        guard.entries(resident);
        const auto check = [&](const std::vector<Matrix>& maps, bool side) {
            const auto& a = source.factor(side); const auto& b = target.factor(side);
            require(b.grade_count() >= a.grade_count(), "tensor map target must explicitly declare every source factor grade, including empty grades");
            const auto grades = std::max(a.grade_count(), b.grade_count());
            require(maps.size() == grades, "tensor factor map must include every source and target grade");
            for (std::size_t k = 0; k < grades; ++k) {
                require(maps[k].cols() == count(a, k) && maps[k].rows() == count(b, k), "tensor factor map carrier dimensions");
                guard.matrix(maps[k]);
                if (k) {
                    const auto bt = boundary(b, k, guard), bs = boundary(a, k, guard);
                    const auto lhs = multiply(bt, maps[k], guard, add(resident, bs.nonzeros()));
                    const auto rhs = multiply(maps[k - 1], bs, guard, add(resident, add(bt.nonzeros(), lhs.nonzeros())));
                    require(lhs == rhs, "tensor factor map fails the complete chain identity");
                }
            }
        };
        check(left, false); check(right, true);
        return PreparedTensorChainMap(std::make_shared<const State>(State{std::move(source), std::move(target),
                                       std::move(left), std::move(right), words}));
    }
    OwnedRelationalTensorProduct source() const { return state_->source; }
    OwnedRelationalTensorProduct target() const { return state_->target; }
    const std::vector<Matrix>& factor_maps(bool right) const & { return right ? state_->right : state_->left; }
    const std::vector<Matrix>& factor_maps(bool) const && = delete;
    std::size_t entry_envelope() const {
        return envelope(*state_);
    }
    void validate(Guard& guard) const {
        const auto pin = state_; guard.entries(envelope(*pin)); pin->source.validate(guard); pin->target.validate(guard);
        for (const auto* maps : {&pin->left, &pin->right}) for (const auto& m : *maps) guard.matrix(m);
    }
    OwnedProductField apply(OwnedProductField input, Guard& guard) const {
        using namespace owned_product_detail;
        const auto pin = state_;
        require(pin->source.state_ == input.owner_, "tensor map requires its actual product input owner");
        // Pin before any caller-supplied checkpoint can replace this handle.
        const auto captured = PreparedTensorChainMap(pin);
        captured.validate(guard); values(*input.owner_, input.grade_, input.values_, guard);
        Values out;
        for (const auto& [id, q] : input.values_) {
            const auto& a = pin->left.at(id.left.grade.value); const auto& b = pin->right.at(id.right.grade.value);
            for (const auto& [ij, x] : a.entries()) {
                guard.step(); if (ij.second != id.left.index) continue;
                for (const auto& [kl, y] : b.entries()) {
                    guard.step(); if (kl.second != id.right.index) continue;
                    guard.entries(add(envelope(*pin), add(mul(input.values_.size(), 2), add(out.size(), 1))));
                    const ProductCell target{{id.left.grade, static_cast<std::uint32_t>(ij.first)},
                                             {id.right.grade, static_cast<std::uint32_t>(kl.first)}};
                    auto& value = out.try_emplace(target, Q{0}).first->second;
                    value = guard.add(value, guard.mul(q, guard.mul(x, y)));
                }
            }
        }
        return OwnedProductField(pin->target.state_, input.grade_, std::move(out));
    }
private:
    explicit PreparedTensorChainMap(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::algebra
