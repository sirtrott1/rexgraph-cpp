#pragma once
#include "rfc/algebra/owned_product_chain_map.hpp"

namespace rfc::algebra {
struct ProductFactorCompositionContribution {
    numeric::ExactMatrix::Key first_position, second_position, position;
    numeric::Rational first_coefficient, second_coefficient, value;
};
struct ProductFactorComposition {
    std::size_t original_factor{}, grade{}, source_size{}, intermediate_size{}, target_size{};
    numeric::ExactMatrix matrix;
    std::set<numeric::ExactMatrix::Key> support;
    std::vector<ProductFactorCompositionContribution> contributions;
};
struct ProductCompositionWork {
    std::size_t begin{}, end{};
    std::size_t steps() const { return end-begin; }
};
struct ProductMapCompositionComparison {
    OwnedAssociatedProductField direct, staged;
    std::set<AssociatedCell> only_direct_support, only_staged_support;
};
namespace product_composition_detail {
using namespace owned_product_detail;
struct State {
    PreparedProductChainMap first, second, direct;
    std::array<std::vector<ProductFactorComposition>, 3> factors;
    std::size_t words{}, envelope{};
};
inline std::pair<std::size_t, std::size_t> bounds(const PreparedProductChainMap& first,
    const PreparedProductChainMap& second) {
    require(first.target().same_owner(second.source()), "product map composition needs its actual intermediate owner");
    std::size_t paths = 0, grades = 0, sites = 1;
    for (std::size_t f = 0; f < 3; ++f) {
        const auto& a = first.declaration().factors[f]; const auto& b = second.declaration().factors[f];
        grades = add(grades, b.matrices.size());
        for (std::size_t k = 0; k < a.support.size(); ++k) paths = add(paths, mul(a.support[k].size(), b.support[k].size()));
        std::size_t most = 0;
        for (const auto& h : {first.source(), first.target(), second.target()}) {
            std::size_t n = 0; const auto& c = h.factor(f);
            for (std::size_t k = 0; k < c.grade_count(); ++k) n = add(n, count(c, k));
            most = std::max(most, n);
        }
        sites = mul(sites, most);
    }
    const auto source = first.source(), target = second.target();
    const auto map_words = add(add(8, add(source.retained_words(), target.retained_words())), add(mul(grades, 2), mul(paths, 6)));
    const auto direct = add(add(source.entry_envelope(), target.entry_envelope()),
        add(mul(map_words, 16), mul(1024, add(sites, mul(sites, paths)))));
    const auto words = add(add(16, add(first.retained_words(), second.retained_words())), add(map_words, add(mul(grades, 16), mul(paths, 64))));
    // Includes both adopted maps, the complete composed declaration and every
    // direct/staged prefix, proof comparison and uncontracted contribution.
    const auto envelope = add(mul(4, add(add(first.entry_envelope(), second.entry_envelope()), direct)),
        add(mul(words, 32), mul(sites, 4096)));
    return {words, envelope};
}
inline std::pair<std::size_t, std::size_t> identity_bounds(const OwnedProductHexagon& h) {
    std::size_t cells = 0, grades = 0, sites = 1;
    for (std::size_t f = 0; f < 3; ++f) {
        const auto& c = h.factor(f); grades = add(grades, c.grade_count()); std::size_t n = 0;
        for (std::size_t k = 0; k < c.grade_count(); ++k) n = add(n, count(c, k));
        cells = add(cells, n); sites = mul(sites, n);
    }
    const auto words = add(add(8, mul(h.retained_words(), 2)), add(mul(grades, 2), mul(cells, 6)));
    return {words, add(mul(h.entry_envelope(), 2), add(mul(words, 16), mul(1024, add(sites, mul(sites, cells)))))};
}
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.first.validate(guard); s.second.validate(guard); s.direct.validate(guard);
    for (const auto& factor : s.factors) for (const auto& grade : factor) {
        guard.matrix(grade.matrix);
        for (const auto& term : grade.contributions) {
            guard.step(); guard.value(term.first_coefficient); guard.value(term.second_coefficient); guard.value(term.value);
        }
    }
}
inline ProductMapCompositionComparison compare(OwnedAssociatedProductField direct,
    OwnedAssociatedProductField staged, Guard& guard) {
    auto c = product_map_detail::compare(std::move(direct), std::move(staged), guard);
    return {std::move(c.mapped_then_operation), std::move(c.operation_then_mapped),
        std::move(c.only_first_support), std::move(c.only_second_support)};
}
} // namespace product_composition_detail
class PreparedProductMapComposition;
class ProductMapCompositionLift {
public:
    PreparedProductMapComposition plan() const;
    const ProductMapLift& first() const & { return first_; }
    const ProductMapLift& first() const && = delete;
    const ProductMapLift& second() const & { return second_; }
    const ProductMapLift& second() const && = delete;
    const ProductMapLift& direct() const & { return direct_; }
    const ProductMapLift& direct() const && = delete;
    const ProductMapCompositionComparison& comparison() const & { return comparison_; }
    const ProductMapCompositionComparison& comparison() const && = delete;
    const std::array<ProductCompositionWork, 2>& staged_work() const & { return work_; }
    const std::array<ProductCompositionWork, 2>& staged_work() const && = delete;
private:
    friend class PreparedProductMapComposition;
    ProductMapCompositionLift(std::shared_ptr<const product_composition_detail::State> s,
        ProductMapLift first, ProductMapLift second, ProductMapLift direct,
        ProductMapCompositionComparison comparison, std::array<ProductCompositionWork, 2> work)
        : state_(std::move(s)), first_(std::move(first)), second_(std::move(second)),
          direct_(std::move(direct)), comparison_(std::move(comparison)), work_(work) {}
    std::shared_ptr<const product_composition_detail::State> state_;
    ProductMapLift first_, second_, direct_;
    ProductMapCompositionComparison comparison_;
    std::array<ProductCompositionWork, 2> work_;
};
struct ProductMapCompositionRoute {
    std::vector<ProductMapCompositionLift> prefixes;
    std::vector<OwnedAssociatedProductField> staged_path;
    std::vector<ProductMapComparison> staged_naturality;
};
struct ProductMapCompositionBoundary {
    ProductMapCompositionLift input, boundary;
    OwnedAssociatedProductField staged_boundary;
    ProductMapComparison staged_naturality;
    ProductMapCompositionComparison image_boundaries;
};
class ProductMapCompositionWitness {
public:
    PreparedProductMapComposition plan() const;
    const ProductMapCoherenceWitness& direct() const & { return direct_; }
    const ProductMapCoherenceWitness& direct() const && = delete;
    const std::array<ProductMapCompositionRoute, 5>& routes() const & { return routes_; }
    const std::array<ProductMapCompositionRoute, 5>& routes() const && = delete;
    const std::optional<ProductMapCompositionBoundary>& boundary() const & { return boundary_; }
    const std::optional<ProductMapCompositionBoundary>& boundary() const && = delete;
    const ProductMapComparison& staged_yang_baxter() const & { return yang_baxter_; }
    const ProductMapComparison& staged_yang_baxter() const && = delete;
private:
    friend class PreparedProductMapComposition;
    ProductMapCompositionWitness(std::shared_ptr<const product_composition_detail::State> s,
        ProductMapCoherenceWitness direct, std::array<ProductMapCompositionRoute, 5> routes,
        std::optional<ProductMapCompositionBoundary> boundary, ProductMapComparison yang_baxter)
        : state_(std::move(s)), direct_(std::move(direct)), routes_(std::move(routes)),
          boundary_(std::move(boundary)), yang_baxter_(std::move(yang_baxter)) {}
    std::shared_ptr<const product_composition_detail::State> state_;
    ProductMapCoherenceWitness direct_;
    std::array<ProductMapCompositionRoute, 5> routes_;
    std::optional<ProductMapCompositionBoundary> boundary_;
    ProductMapComparison yang_baxter_;
};
class PreparedProductMapComposition {
public:
    static PreparedProductMapComposition prepare(PreparedProductChainMap first, PreparedProductChainMap second,
        std::uint64_t key, std::uint64_t revision, numeric::ExactActionGuard& guard) {
        namespace detail = product_composition_detail;
        const auto [words, envelope] = detail::bounds(first, second); guard.entries(envelope);
        detail::require(key && revision, "product map composition needs a declared identity and revision");
        first.validate(guard); second.validate(guard);
        const auto source = first.source();
        ProductChainMapDeclaration declaration{key, revision, {}};
        std::array<std::vector<ProductFactorComposition>, 3> factors;
        for (std::size_t f = 0; f < 3; ++f) {
            const auto& a = first.declaration().factors[f]; const auto& b = second.declaration().factors[f];
            for (std::size_t k = 0; k < b.matrices.size(); ++k) {
                const auto rows = b.matrices[k].rows(), middle = b.matrices[k].cols(), cols = detail::count(source.factor(f), k);
                ProductFactorComposition out{f, k, cols, middle, rows, numeric::ExactMatrix(rows, cols), {}, {}};
                if (k < a.matrices.size()) for (const auto& x : a.support[k]) for (const auto& y : b.support[k]) {
                    guard.step(); if (x.first != y.second) continue;
                    const numeric::ExactMatrix::Key position{y.first, x.second};
                    const auto fq = a.matrices[k].get(x.first, x.second), gq = b.matrices[k].get(y.first, y.second);
                    const auto value = guard.mul(gq, fq);
                    out.contributions.push_back({x, y, position, fq, gq, value}); out.support.insert(position);
                    out.matrix.set(position.first, position.second, guard.add(out.matrix.get(position.first, position.second), value));
                }
                declaration.factors[f].matrices.push_back(out.matrix);
                declaration.factors[f].support.push_back(out.support); factors[f].push_back(std::move(out));
            }
        }
        auto direct = PreparedProductChainMap::prepare(first.source(), second.target(), std::move(declaration), guard);
        return PreparedProductMapComposition(std::make_shared<const detail::State>(detail::State{
            std::move(first), std::move(second), std::move(direct), std::move(factors), words, envelope}));
    }
    static PreparedProductChainMap identity(OwnedProductHexagon h, std::uint64_t key, std::uint64_t revision,
        numeric::ExactActionGuard& guard) {
        namespace detail = product_composition_detail;
        detail::require(key && revision, "product identity map needs a declared identity and revision");
        guard.entries(detail::identity_bounds(h).second);
        ProductChainMapDeclaration d{key, revision, {}};
        for (std::size_t f = 0; f < 3; ++f) for (std::size_t k = 0; k < h.factor(f).grade_count(); ++k) {
            const auto n = detail::count(h.factor(f), k); guard.dimension(n);
            numeric::ExactMatrix matrix(n, n); std::set<numeric::ExactMatrix::Key> support;
            for (std::size_t i = 0; i < n; ++i) { guard.step(); matrix.set(i, i, numeric::Rational{1}); support.insert({i, i}); }
            d.factors[f].matrices.push_back(std::move(matrix)); d.factors[f].support.push_back(std::move(support));
        }
        return PreparedProductChainMap::prepare(h, h, std::move(d), guard);
    }
    PreparedProductChainMap first() const { return state_->first; }
    PreparedProductChainMap second() const { return state_->second; }
    PreparedProductChainMap direct() const { return state_->direct; }
    const std::array<std::vector<ProductFactorComposition>, 3>& factors() const & { return state_->factors; }
    const std::array<std::vector<ProductFactorComposition>, 3>& factors() const && = delete;
    std::size_t retained_words() const { return state_->words; }
    std::size_t entry_envelope() const { return state_->envelope; }
    bool same_owner(const PreparedProductMapComposition& p) const noexcept { return state_ == p.state_; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; product_composition_detail::validate(*pin, guard); }
    ProductMapCompositionLift apply(OwnedAssociatedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; product_composition_detail::validate(*pin, guard);
        auto direct = pin->direct.apply(std::move(input), guard);
        return staged(pin, std::move(direct), guard);
    }
    ProductMapCompositionWitness witness(OwnedAssociatedProductField input, numeric::ExactActionGuard& guard) const {
        namespace detail = product_composition_detail; const auto pin = state_; detail::validate(*pin, guard);
        auto direct = pin->direct.witness(std::move(input), guard);
        std::array<ProductMapCompositionRoute, 5> routes;
        for (std::size_t r = 0; r < routes.size(); ++r) {
            auto& route = routes[r]; const auto& original = direct.routes()[r];
            for (const auto& lift : original.prefix_maps) route.prefixes.push_back(staged(pin, lift, guard));
            route.staged_path.push_back(route.prefixes.front().second().image());
            for (std::size_t j = 0; j < original.operations.size(); ++j) {
                route.staged_path.push_back(pin->second.target().step(route.staged_path.back(), original.operations[j], guard));
                route.staged_naturality.push_back(product_map_detail::compare(route.staged_path.back(), route.prefixes[j+1].second().image(), guard));
            }
        }
        std::optional<ProductMapCompositionBoundary> boundary;
        if (direct.boundary()) {
            auto root = staged(pin, direct.boundary()->input_map, guard);
            auto db = staged(pin, direct.boundary()->boundary_map, guard);
            auto bd = pin->second.target().boundary(root.second().image(), guard);
            auto comparison = product_map_detail::compare(bd, db.second().image(), guard);
            auto images = detail::compare(direct.boundary()->comparison.mapped_then_operation, bd, guard);
            boundary.emplace(ProductMapCompositionBoundary{std::move(root), std::move(db), std::move(bd), std::move(comparison), std::move(images)});
        }
        auto yang_baxter = product_map_detail::compare(routes[3].staged_path.back(), routes[4].staged_path.back(), guard);
        return ProductMapCompositionWitness(pin, std::move(direct), std::move(routes), std::move(boundary), std::move(yang_baxter));
    }
private:
    friend class ProductMapCompositionLift;
    friend class ProductMapCompositionWitness;
    explicit PreparedProductMapComposition(std::shared_ptr<const product_composition_detail::State> s) : state_(std::move(s)) {}
    static ProductMapCompositionLift staged(std::shared_ptr<const product_composition_detail::State> pin,
        ProductMapLift direct, numeric::ExactActionGuard& guard) {
        const auto begin = guard.steps(); auto first = pin->first.apply(direct.input(), guard);
        const auto middle = guard.steps(); auto second = pin->second.apply(first.image(), guard); const auto end = guard.steps();
        auto comparison = product_composition_detail::compare(direct.image(), second.image(), guard);
        return ProductMapCompositionLift(std::move(pin), std::move(first), std::move(second), std::move(direct),
            std::move(comparison), {{{begin, middle}, {middle, end}}});
    }
    std::shared_ptr<const product_composition_detail::State> state_;
};
inline PreparedProductMapComposition ProductMapCompositionLift::plan() const { return PreparedProductMapComposition(state_); }
inline PreparedProductMapComposition ProductMapCompositionWitness::plan() const { return PreparedProductMapComposition(state_); }
} // namespace rfc::algebra
