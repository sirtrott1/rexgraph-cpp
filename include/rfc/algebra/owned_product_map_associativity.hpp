#pragma once
#include "rfc/algebra/owned_product_map_composition.hpp"

namespace rfc::algebra {
// Each derived composition has its own declared identity. The original maps
// and the adopted F-then-G composition keep theirs.
struct ProductMapAssociativityDeclaration {
    std::uint64_t key{}, revision{}, tail_key{}, left_key{}, right_key{};
};
struct ProductFactorTripleContribution {
    std::array<numeric::ExactMatrix::Key, 3> positions;
    numeric::ExactMatrix::Key position;
    std::array<numeric::Rational, 3> coefficients;
    numeric::Rational first_pair, last_pair, left_value, right_value;
};
struct ProductFactorAssociativity {
    std::size_t original_factor{}, grade{};
    std::array<std::size_t, 4> sizes;
    numeric::ExactMatrix matrix;
    std::set<numeric::ExactMatrix::Key> support;
    std::vector<ProductFactorTripleContribution> contributions;
};
namespace product_associativity_detail {
using namespace owned_product_detail;
// A conservative composition bound with a supplied upper path count. The
// right parenthesization is admitted before its intermediate map is built.
inline std::pair<std::size_t, std::size_t> composition_bound(const OwnedProductHexagon& x,
    const OwnedProductHexagon& z, std::size_t w1, std::size_t e1, std::size_t w2,
    std::size_t e2, std::size_t grades, std::size_t paths, std::size_t sites) {
    const auto mw = add(add(8, add(x.retained_words(), z.retained_words())), add(mul(grades, 2), mul(paths, 6)));
    const auto me = add(add(x.entry_envelope(), z.entry_envelope()), add(mul(mw, 16), mul(1024, add(sites, mul(sites, paths)))));
    const auto w = add(add(16, add(w1, w2)), add(mw, add(mul(grades, 16), mul(paths, 64))));
    return {w, add(mul(4, add(add(e1, e2), me)), add(mul(w, 32), mul(sites, 4096)))};
}
inline std::pair<std::size_t, std::size_t> bounds(const PreparedProductMapComposition& p,
    const PreparedProductChainMap& h) {
    require(p.second().target().same_owner(h.source()), "three-map composition needs its actual second intermediate owner");
    const auto f = p.first(), g = p.second();
    std::size_t sites = 1, triples = 0, grades = 0;
    for (std::size_t j = 0; j < 3; ++j) {
        std::size_t most = 0;
        for (const auto& c : {f.source(), f.target(), g.target(), h.target()}) {
            std::size_t n = 0;
            for (std::size_t k = 0; k < c.factor(j).grade_count(); ++k) n = add(n, count(c.factor(j), k));
            most = std::max(most, n);
        }
        sites = mul(sites, most); grades = add(grades, h.declaration().factors[j].matrices.size());
        const auto& a = f.declaration().factors[j]; const auto& b = g.declaration().factors[j]; const auto& c = h.declaration().factors[j];
        for (std::size_t k = 0; k < a.support.size(); ++k)
            triples = add(triples, mul(mul(a.support[k].size(), b.support[k].size()), c.support[k].size()));
    }
    const auto tail = product_composition_detail::bounds(g, h);
    const auto left = product_composition_detail::bounds(p.direct(), h);
    const auto right = composition_bound(f.source(), h.target(), f.retained_words(), f.entry_envelope(),
        tail.first, tail.second, grades, triples, sites);
    const auto words = add(16, add(add(p.retained_words(), h.retained_words()),
        add(add(tail.first, left.first), add(right.first, mul(triples, 64)))));
    const auto envelope = add(mul(2, add(add(p.entry_envelope(), h.entry_envelope()),
        add(add(tail.second, left.second), right.second))), add(mul(words, 32), mul(sites, 4096)));
    return {words, envelope};
}
struct State {
    PreparedProductMapComposition prefix, tail, left, right;
    PreparedProductChainMap last;
    ProductMapAssociativityDeclaration declaration;
    std::array<std::vector<ProductFactorAssociativity>, 3> factors;
    std::size_t words{}, envelope{};
};
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.prefix.validate(guard); s.last.validate(guard);
    s.tail.validate(guard); s.left.validate(guard); s.right.validate(guard);
    for (const auto& factor : s.factors) for (const auto& k : factor) {
        guard.matrix(k.matrix);
        for (const auto& t : k.contributions) {
            guard.step(); for (const auto& q : t.coefficients) guard.value(q);
            for (const auto* q : {&t.first_pair, &t.last_pair, &t.left_value, &t.right_value}) guard.value(*q);
        }
    }
}
} // namespace product_associativity_detail
class PreparedProductMapAssociativity;
class ProductMapAssociativityLift {
public:
    PreparedProductMapAssociativity plan() const;
    const ProductMapCompositionLift& prefix() const & { return prefix_; }
    const ProductMapCompositionLift& prefix() const && = delete;
    const ProductMapLift& last() const & { return last_; }
    const ProductMapLift& last() const && = delete;
    const ProductMapCompositionLift& left() const & { return left_; }
    const ProductMapCompositionLift& left() const && = delete;
    const ProductMapCompositionLift& right() const & { return right_; }
    const ProductMapCompositionLift& right() const && = delete;
    const ProductMapCompositionComparison& parenthesizations() const & { return parents_; }
    const ProductMapCompositionComparison& parenthesizations() const && = delete;
    const ProductMapCompositionComparison& staged_comparison() const & { return staged_; }
    const ProductMapCompositionComparison& staged_comparison() const && = delete;
    const std::array<ProductCompositionWork, 2>& staged_work() const & { return work_; }
    const std::array<ProductCompositionWork, 2>& staged_work() const && = delete;
private:
    friend class PreparedProductMapAssociativity;
    ProductMapAssociativityLift(std::shared_ptr<const product_associativity_detail::State> s,
        ProductMapCompositionLift prefix, ProductMapLift last, ProductMapCompositionLift left,
        ProductMapCompositionLift right, ProductMapCompositionComparison parents,
        ProductMapCompositionComparison staged, std::array<ProductCompositionWork, 2> work)
        : state_(std::move(s)), prefix_(std::move(prefix)), last_(std::move(last)), left_(std::move(left)),
          right_(std::move(right)), parents_(std::move(parents)), staged_(std::move(staged)), work_(work) {}
    std::shared_ptr<const product_associativity_detail::State> state_;
    ProductMapCompositionLift prefix_;
    ProductMapLift last_;
    ProductMapCompositionLift left_, right_;
    ProductMapCompositionComparison parents_, staged_;
    std::array<ProductCompositionWork, 2> work_;
};
struct ProductMapAssociativityRoute {
    std::vector<ProductMapAssociativityLift> prefixes;
    std::vector<OwnedAssociatedProductField> staged_path;
    std::vector<ProductMapComparison> staged_naturality;
};
struct ProductMapAssociativityBoundary {
    ProductMapAssociativityLift input, boundary;
    OwnedAssociatedProductField staged_boundary;
    ProductMapComparison staged_naturality;
};
struct ProductMapAssociativityWitness {
    ProductMapCompositionWitness left, right;
    std::array<ProductMapAssociativityRoute, 5> routes;
    std::optional<ProductMapAssociativityBoundary> boundary;
    ProductMapComparison staged_yang_baxter;
};
class PreparedProductMapAssociativity {
public:
    static PreparedProductMapAssociativity prepare(PreparedProductMapComposition prefix,
        PreparedProductChainMap last, ProductMapAssociativityDeclaration d, numeric::ExactActionGuard& guard) {
        namespace detail = product_associativity_detail;
        const auto [words, envelope] = detail::bounds(prefix, last); guard.entries(envelope);
        const std::set ids{d.key, d.tail_key, d.left_key, d.right_key};
        detail::require(d.revision && !ids.contains(0) && ids.size() == 4,
            "associativity needs four distinct positive operation identities and a revision");
        prefix.validate(guard); last.validate(guard);
        auto tail = PreparedProductMapComposition::prepare(prefix.second(), last, d.tail_key, d.revision, guard);
        auto left = PreparedProductMapComposition::prepare(prefix.direct(), last, d.left_key, d.revision, guard);
        auto right = PreparedProductMapComposition::prepare(prefix.first(), tail.direct(), d.right_key, d.revision, guard);
        std::array<std::vector<ProductFactorAssociativity>, 3> factors;
        const auto first = prefix.first(), second = prefix.second();
        const auto x_owner = first.source(), y_owner = first.target(), z_owner = second.target();
        for (std::size_t f = 0; f < 3; ++f) {
            const auto& a = first.declaration().factors[f]; const auto& b = second.declaration().factors[f]; const auto& c = last.declaration().factors[f];
            for (std::size_t k = 0; k < c.matrices.size(); ++k) {
                const auto cols = detail::count(x_owner.factor(f), k), rows = c.matrices[k].rows();
                ProductFactorAssociativity out{f, k, {cols, detail::count(y_owner.factor(f), k),
                    detail::count(z_owner.factor(f), k), rows}, numeric::ExactMatrix(rows, cols), {}, {}};
                if (k < a.matrices.size()) for (const auto& x : a.support[k]) for (const auto& y : b.support[k]) {
                    guard.step(); if (x.first != y.second) continue;
                    for (const auto& z : c.support[k]) {
                        guard.step(); if (y.first != z.second) continue;
                        const auto fq = a.matrices[k].get(x.first, x.second), gq = b.matrices[k].get(y.first, y.second), hq = c.matrices[k].get(z.first, z.second);
                        const auto gf = guard.mul(gq, fq), hg = guard.mul(hq, gq), lq = guard.mul(hq, gf), rq = guard.mul(hg, fq);
                        detail::require(lq == rq, "rational factor composition associativity failed");
                        const numeric::ExactMatrix::Key position{z.first, x.second};
                        out.contributions.push_back({{x, y, z}, position, {fq, gq, hq}, gf, hg, lq, rq});
                        out.support.insert(position); out.matrix.set(position.first, position.second, guard.add(out.matrix.get(position.first, position.second), lq));
                    }
                }
                for (const auto& p : {left.direct(), right.direct()}) {
                    const auto& map = p.declaration().factors[f];
                    detail::require(out.matrix == map.matrices[k] && out.support == map.support[k],
                        "triple paths disagree with a composed factor map or its support");
                }
                factors[f].push_back(std::move(out));
            }
        }
        return PreparedProductMapAssociativity(std::make_shared<const detail::State>(detail::State{
            std::move(prefix), std::move(tail), std::move(left), std::move(right), std::move(last), d, std::move(factors), words, envelope}));
    }
    PreparedProductMapComposition prefix() const { return state_->prefix; }
    PreparedProductMapComposition tail() const { return state_->tail; }
    PreparedProductMapComposition left() const { return state_->left; }
    PreparedProductMapComposition right() const { return state_->right; }
    PreparedProductChainMap last() const { return state_->last; }
    ProductMapAssociativityDeclaration declaration() const { return state_->declaration; }
    const std::array<std::vector<ProductFactorAssociativity>, 3>& factors() const & { return state_->factors; }
    const std::array<std::vector<ProductFactorAssociativity>, 3>& factors() const && = delete;
    std::size_t retained_words() const { return state_->words; }
    std::size_t entry_envelope() const { return state_->envelope; }
    bool same_owner(const PreparedProductMapAssociativity& p) const noexcept { return state_ == p.state_; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; product_associativity_detail::validate(*pin, guard); }
    ProductMapAssociativityLift apply(OwnedAssociatedProductField input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; product_associativity_detail::validate(*pin, guard);
        const auto begin = guard.steps(); auto prefix = pin->prefix.apply(input, guard); const auto end = guard.steps();
        auto left = pin->left.apply(input, guard), right = pin->right.apply(std::move(input), guard);
        return finish(pin, std::move(prefix), std::move(left), std::move(right), {begin, end}, guard);
    }
    ProductMapAssociativityLift continue_from(const ProductMapCompositionLift& input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_;
        product_associativity_detail::require(pin->prefix.same_owner(input.plan()), "continuation requires its actual prefix composition receipt");
        guard.entries(pin->envelope);
        // Freeze the caller's receipt after preflight and before any checkpoint
        // could replace that lvalue. The retained native receipt is immutable.
        auto prior = input;
        product_associativity_detail::validate(*pin, guard);
        auto left = pin->left.apply(prior.first().input(), guard), right = pin->right.apply(prior.first().input(), guard);
        // Numerical diagnostics may reevaluate the parenthesizations. The
        // original staged prefix receipt is adopted without reevaluating it.
        return finish(pin, std::move(prior), std::move(left), std::move(right), {guard.steps(), guard.steps()}, guard);
    }
    ProductMapAssociativityWitness witness(OwnedAssociatedProductField input, numeric::ExactActionGuard& guard) const {
        namespace detail = product_associativity_detail; const auto pin = state_; detail::validate(*pin, guard);
        auto left = pin->left.witness(input, guard), right = pin->right.witness(std::move(input), guard);
        std::array<ProductMapAssociativityRoute, 5> routes;
        for (std::size_t r = 0; r < routes.size(); ++r) {
            auto& route = routes[r]; const auto& original = left.direct().routes()[r];
            for (std::size_t j = 0; j < original.prefix_maps.size(); ++j) {
                const auto begin = guard.steps(); auto prefix = pin->prefix.apply(original.prefix_maps[j].input(), guard); const auto end = guard.steps();
                route.prefixes.push_back(finish(pin, std::move(prefix), left.routes()[r].prefixes[j], right.routes()[r].prefixes[j], {begin, end}, guard));
            }
            route.staged_path.push_back(route.prefixes.front().last().image());
            for (std::size_t j = 0; j < original.operations.size(); ++j) {
                route.staged_path.push_back(pin->last.target().step(route.staged_path.back(), original.operations[j], guard));
                route.staged_naturality.push_back(product_map_detail::compare(route.staged_path.back(), route.prefixes[j+1].last().image(), guard));
            }
        }
        const auto bound = [&](const ProductMapCompositionLift& l, const ProductMapCompositionLift& r) {
            const auto begin = guard.steps(); auto prefix = pin->prefix.apply(l.first().input(), guard); const auto end = guard.steps();
            return finish(pin, std::move(prefix), l, r, {begin, end}, guard);
        };
        std::optional<ProductMapAssociativityBoundary> boundary;
        if (left.boundary()) {
            auto root = bound(left.boundary()->input, right.boundary()->input), db = bound(left.boundary()->boundary, right.boundary()->boundary);
            auto bd = pin->last.target().boundary(root.last().image(), guard);
            auto comparison = product_map_detail::compare(bd, db.last().image(), guard);
            boundary.emplace(ProductMapAssociativityBoundary{std::move(root), std::move(db), std::move(bd), std::move(comparison)});
        }
        auto yang = product_map_detail::compare(routes[3].staged_path.back(), routes[4].staged_path.back(), guard);
        return {std::move(left), std::move(right), std::move(routes), std::move(boundary), std::move(yang)};
    }
private:
    friend class ProductMapAssociativityLift;
    explicit PreparedProductMapAssociativity(std::shared_ptr<const product_associativity_detail::State> s) : state_(std::move(s)) {}
    static ProductMapAssociativityLift finish(std::shared_ptr<const product_associativity_detail::State> pin,
        ProductMapCompositionLift prefix, ProductMapCompositionLift left, ProductMapCompositionLift right,
        ProductCompositionWork prefix_work, numeric::ExactActionGuard& guard) {
        const auto begin = guard.steps(); auto last = pin->last.apply(prefix.second().image(), guard); const auto end = guard.steps();
        auto parents = product_composition_detail::compare(left.direct().image(), right.direct().image(), guard);
        auto staged = product_composition_detail::compare(left.direct().image(), last.image(), guard);
        return ProductMapAssociativityLift(std::move(pin), std::move(prefix), std::move(last), std::move(left), std::move(right),
            std::move(parents), std::move(staged), {{prefix_work, {begin, end}}});
    }
    std::shared_ptr<const product_associativity_detail::State> state_;
};
inline PreparedProductMapAssociativity ProductMapAssociativityLift::plan() const { return PreparedProductMapAssociativity(state_); }
} // namespace rfc::algebra
