#pragma once
#include "rfc/algebra/owned_product_braid.hpp"

namespace rfc::algebra {
// Matrix rows are target positions; columns are source positions. Support is
// declared independently, because ExactMatrix deliberately normalizes zeros.
struct ProductFactorChainMap {
    std::vector<numeric::ExactMatrix> matrices;
    std::vector<std::set<numeric::ExactMatrix::Key>> support;
    static ProductFactorChainMap nonzero_support(std::vector<numeric::ExactMatrix> maps) {
        ProductFactorChainMap out{std::move(maps), {}};
        for (const auto& m : out.matrices) {
            std::set<numeric::ExactMatrix::Key> sites;
            for (const auto& [site, q] : m.entries()) { (void)q; sites.insert(site); }
            out.support.push_back(std::move(sites));
        }
        return out;
    }
};
struct ProductChainMapDeclaration {
    std::uint64_t key{}, revision{};
    std::array<ProductFactorChainMap, 3> factors;
};
struct ProductMapContribution {
    std::array<core::CellId, 3> input, image;
    numeric::ExactMatrix::Key matrix_position;
    numeric::Rational input_value, coefficient, value;
};
struct ProductMapStage {
    std::size_t original_factor{};
    // These are mixed source/target coordinates, not an undeclared product.
    std::array<bool, 3> input_target{}, image_target{};
    std::map<std::array<core::CellId, 3>, numeric::Rational> input, image;
    std::vector<ProductMapContribution> contributions;
};
struct ProductMapComparison {
    OwnedAssociatedProductField mapped_then_operation, operation_then_mapped;
    std::set<AssociatedCell> only_first_support, only_second_support;
};
namespace product_map_detail {
using namespace owned_product_detail;
struct State {
    OwnedProductHexagon source, target;
    ProductChainMapDeclaration declaration;
    std::size_t words{}, envelope{};
};
inline void validate_hexagon(const OwnedProductHexagon& h, Guard& guard) {
    for (std::size_t i = 0; i < 6; ++i) h.associator(static_cast<ProductPermutation>(i)).validate(guard);
    for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = i+1; j < 3; ++j)
        for (const auto order : {ProductFactorOrder::AB, ProductFactorOrder::BA}) h.pair(i, j).product(order).validate(guard);
}
inline std::pair<std::size_t, std::size_t> bounds(const OwnedProductHexagon& source,
    const OwnedProductHexagon& target, const ProductChainMapDeclaration& d) {
    auto words = add(8, add(source.retained_words(), target.retained_words()));
    std::size_t sites = 1, map_sites = 0;
    for (std::size_t f = 0; f < 3; ++f) {
        std::array<std::size_t, 2> counts{};
        for (std::size_t j = 0; j < 2; ++j) {
            const auto& c = j ? target.factor(f) : source.factor(f);
            for (std::size_t k = 0; k < c.grade_count(); ++k) counts[j] = add(counts[j], count(c, k));
        }
        sites = mul(sites, std::max(counts[0], counts[1]));
        const auto& m = d.factors[f];
        words = add(words, add(m.matrices.size(), m.support.size()));
        for (const auto& matrix : m.matrices) words = add(words, mul(matrix.nonzeros(), 4));
        for (const auto& support : m.support) {
            map_sites = add(map_sites, support.size()); words = add(words, mul(support.size(), 2));
        }
    }
    // Covers all twenty prefix lifts, their three stages, boundary lifts,
    // contributions, primitive histories and both final comparisons together.
    const auto scratch = mul(1024, add(sites, mul(sites, map_sites)));
    return {words, add(add(source.entry_envelope(), target.entry_envelope()), add(mul(words, 16), scratch))};
}
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); validate_hexagon(s.source, guard); validate_hexagon(s.target, guard);
    for (const auto& f : s.declaration.factors) {
        for (const auto& m : f.matrices) guard.matrix(m);
        for (const auto& support : f.support) for (const auto& site : support) { (void)site; guard.step(); }
    }
}
inline ProductMapComparison compare(OwnedAssociatedProductField a, OwnedAssociatedProductField b, Guard& guard) {
    require(a.plan().same_owner(b.plan()) && a.association() == b.association() && a.grade() == b.grade(),
        "product map comparison requires the actual target carrier and grade");
    ProductMapComparison out{std::move(a), std::move(b), {}, {}};
    const auto& first = out.mapped_then_operation.values(); const auto& second = out.operation_then_mapped.values();
    for (const auto& [id, q] : first) {
        guard.step(); guard.value(q); const auto it = second.find(id);
        require(q == (it == second.end() ? Q{0} : it->second), "product map naturality has a nonzero residual");
        if (it == second.end()) out.only_first_support.insert(id);
    }
    for (const auto& [id, q] : second) {
        guard.step(); guard.value(q);
        if (!first.contains(id)) { require(q == 0, "product map naturality has a nonzero residual"); out.only_second_support.insert(id); }
    }
    return out;
}
} // namespace product_map_detail
class PreparedProductChainMap;
class ProductMapLift {
public:
    PreparedProductChainMap plan() const;
    const OwnedAssociatedProductField& input() const & { return input_; }
    const OwnedAssociatedProductField& input() const && = delete;
    const OwnedAssociatedProductField& image() const & { return image_; }
    const OwnedAssociatedProductField& image() const && = delete;
    const std::array<ProductMapStage, 3>& stages() const & { return stages_; }
    const std::array<ProductMapStage, 3>& stages() const && = delete;
private:
    friend class PreparedProductChainMap;
    ProductMapLift(std::shared_ptr<const product_map_detail::State> p, OwnedAssociatedProductField input,
        OwnedAssociatedProductField image, std::array<ProductMapStage, 3> stages)
        : plan_(std::move(p)), input_(std::move(input)), image_(std::move(image)), stages_(std::move(stages)) {}
    std::shared_ptr<const product_map_detail::State> plan_;
    OwnedAssociatedProductField input_, image_;
    std::array<ProductMapStage, 3> stages_;
};
struct ProductMapRoute {
    std::vector<ProductHexagonAction> operations;
    std::vector<OwnedAssociatedProductField> source_path, target_path;
    std::vector<ProductMapLift> prefix_maps;
    std::vector<ProductMapComparison> comparisons;
};
struct ProductMapBoundary {
    ProductMapLift input_map, boundary_map;
    OwnedAssociatedProductField source_boundary;
    ProductMapComparison comparison;
};
class ProductMapCoherenceWitness {
public:
    PreparedProductChainMap plan() const;
    const std::array<ProductMapRoute, 5>& routes() const & { return routes_; }
    const std::array<ProductMapRoute, 5>& routes() const && = delete;
    const std::optional<ProductMapBoundary>& boundary() const & { return boundary_; }
    const std::optional<ProductMapBoundary>& boundary() const && = delete;
    const ProductMapComparison& yang_baxter() const & { return yang_baxter_; }
    const ProductMapComparison& yang_baxter() const && = delete;
private:
    friend class PreparedProductChainMap;
    ProductMapCoherenceWitness(std::shared_ptr<const product_map_detail::State> p, std::array<ProductMapRoute, 5> r,
        std::optional<ProductMapBoundary> b, ProductMapComparison y)
        : plan_(std::move(p)), routes_(std::move(r)), boundary_(std::move(b)), yang_baxter_(std::move(y)) {}
    std::shared_ptr<const product_map_detail::State> plan_;
    std::array<ProductMapRoute, 5> routes_;
    std::optional<ProductMapBoundary> boundary_;
    ProductMapComparison yang_baxter_;
};
class PreparedProductChainMap {
public:
    static PreparedProductChainMap prepare(OwnedProductHexagon source, OwnedProductHexagon target,
        ProductChainMapDeclaration input, numeric::ExactActionGuard& guard) {
        namespace detail = product_map_detail;
        // Preflight without callbacks, then freeze elements before the first
        // checkpoint can mutate aliases retained by the caller.
        const auto [words, envelope] = detail::bounds(source, target, input); guard.entries(envelope);
        auto d = input;
        detail::require(d.key && d.revision, "product chain map requires a declared identity and revision");
        detail::validate_hexagon(source, guard); detail::validate_hexagon(target, guard);
        for (std::size_t f = 0; f < 3; ++f) {
            const auto& a = source.factor(f); const auto& b = target.factor(f); const auto& map = d.factors[f];
            detail::require(b.grade_count() >= a.grade_count(), "product map target must declare every source grade, including empty grades");
            const auto grades = b.grade_count();
            detail::require(map.matrices.size() == grades && map.support.size() == grades, "product factor map needs every grade and its declared support");
            for (std::size_t k = 0; k < grades; ++k) {
                const auto& m = map.matrices[k]; const auto& support = map.support[k];
                detail::require(m.rows() == detail::count(b, k) && m.cols() == detail::count(a, k), "product factor map carrier dimensions");
                guard.matrix(m);
                for (const auto& site : support) { guard.step(); detail::require(site.first < m.rows() && site.second < m.cols(), "product factor map support outside its carrier"); }
                for (const auto& [site, q] : m.entries()) { (void)q; guard.step(); detail::require(support.contains(site), "product factor map omits a nonzero support direction"); }
                if (k) {
                    const auto bt = detail::boundary(b, k, guard), bs = detail::boundary(a, k, guard);
                    const auto lhs = detail::multiply(bt, m, guard, detail::add(envelope, bs.nonzeros()));
                    const auto rhs = detail::multiply(map.matrices[k-1], bs, guard, detail::add(envelope, detail::add(bt.nonzeros(), lhs.nonzeros())));
                    detail::require(lhs == rhs, "product factor map fails a complete chain identity");
                }
            }
        }
        return PreparedProductChainMap(std::make_shared<const detail::State>(detail::State{
            std::move(source), std::move(target), std::move(d), words, envelope}));
    }
    OwnedProductHexagon source() const { return state_->source; }
    OwnedProductHexagon target() const { return state_->target; }
    const ProductChainMapDeclaration& declaration() const & { return state_->declaration; }
    const ProductChainMapDeclaration& declaration() const && = delete;
    std::size_t retained_words() const { return state_->words; }
    std::size_t entry_envelope() const { return state_->envelope; }
    bool same_owner(const PreparedProductChainMap& p) const noexcept { return state_ == p.state_; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; product_map_detail::validate(*pin, guard); }
    ProductMapLift apply(OwnedAssociatedProductField input, numeric::ExactActionGuard& guard) const {
        namespace detail = product_map_detail; const auto pin = state_;
        const auto carrier = pin->source.carrier(input); detail::validate(*pin, guard);
        // Canonical validation retains the input's supported zeros and owner.
        (void)pin->source.field(carrier, input.grade(), input.values(), guard);
        std::map<std::array<core::CellId,3>, numeric::Rational> current;
        for (const auto& [id, q] : input.values()) { guard.step(); current.emplace(product_hexagon_detail::global(carrier, id), q); }
        std::array<ProductMapStage, 3> stages;
        for (std::size_t f = 0; f < 3; ++f) {
            auto& stage = stages[f]; stage.original_factor = f;
            for (std::size_t j = 0; j < 3; ++j) { stage.input_target[j] = j < f; stage.image_target[j] = j <= f; }
            stage.input = std::move(current);
            for (const auto& [ids, q] : stage.input) {
                const auto g = ids[f].grade.value; const auto& map = pin->declaration.factors[f]; const auto& m = map.matrices[g];
                for (const auto& site : map.support[g]) {
                    guard.step(); if (site.second != ids[f].index) continue;
                    auto image = ids; image[f].index = static_cast<std::uint32_t>(site.first);
                    const auto coefficient = m.get(site.first, site.second), value = guard.mul(q, coefficient);
                    stage.contributions.push_back({ids, image, site, q, coefficient, value});
                    auto& out = stage.image.try_emplace(image, numeric::Rational{0}).first->second; out = guard.add(out, value);
                }
            }
            current = stage.image;
        }
        OwnedProductHexagon::Values out;
        for (const auto& [ids, q] : current) { guard.step(); out.emplace(product_hexagon_detail::cell(carrier, ids), q); }
        auto image = pin->target.field(carrier, input.grade(), std::move(out), guard);
        return ProductMapLift(pin, std::move(input), std::move(image), std::move(stages));
    }
    ProductMapCoherenceWitness witness(OwnedAssociatedProductField input, numeric::ExactActionGuard& guard) const {
        namespace detail = product_map_detail; const auto pin = state_; const auto captured = PreparedProductChainMap(pin);
        const auto carrier = pin->source.carrier(input); detail::validate(*pin, guard);
        std::array<ProductMapRoute, 5> routes;
        for (std::size_t r = 0; r < routes.size(); ++r) {
            auto& route = routes[r];
            route.operations = r < 3 ? std::vector{static_cast<ProductHexagonAction>(r)} : product_braid_detail::word(carrier, r-3);
            route.source_path.push_back(input); route.prefix_maps.push_back(captured.apply(input, guard));
            route.target_path.push_back(route.prefix_maps.back().image());
            for (const auto action : route.operations) {
                route.source_path.push_back(pin->source.step(route.source_path.back(), action, guard));
                route.target_path.push_back(pin->target.step(route.target_path.back(), action, guard));
                route.prefix_maps.push_back(captured.apply(route.source_path.back(), guard));
                route.comparisons.push_back(detail::compare(route.target_path.back(), route.prefix_maps.back().image(), guard));
            }
        }
        std::optional<ProductMapBoundary> boundary;
        if (input.grade().value) {
            auto mapped = captured.apply(input, guard); auto db = pin->source.boundary(input, guard);
            auto mapped_db = captured.apply(db, guard); auto bd = pin->target.boundary(mapped.image(), guard);
            auto comparison = detail::compare(std::move(bd), mapped_db.image(), guard);
            boundary.emplace(ProductMapBoundary{std::move(mapped), std::move(mapped_db), std::move(db), std::move(comparison)});
        }
        auto yang_baxter = detail::compare(routes[3].target_path.back(), routes[4].target_path.back(), guard);
        return ProductMapCoherenceWitness(pin, std::move(routes), std::move(boundary), std::move(yang_baxter));
    }
private:
    friend class ProductMapLift;
    friend class ProductMapCoherenceWitness;
    explicit PreparedProductChainMap(std::shared_ptr<const product_map_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const product_map_detail::State> state_;
};
inline PreparedProductChainMap ProductMapLift::plan() const { return PreparedProductChainMap(plan_); }
inline PreparedProductChainMap ProductMapCoherenceWitness::plan() const { return PreparedProductChainMap(plan_); }
} // namespace rfc::algebra
