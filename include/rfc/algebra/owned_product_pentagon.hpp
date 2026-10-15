#pragma once
#include "rfc/algebra/owned_product_associator.hpp"

namespace rfc::algebra {
enum class ProductParenthesization : std::uint8_t { Left, InnerLeft, InnerRight, Right, Middle };
struct LeftPentagonCell {
    LeftAssociatedCell left; core::CellId right;
    constexpr auto operator<=>(const LeftPentagonCell&) const = default;
};
struct InnerLeftPentagonCell {
    RightAssociatedCell left; core::CellId right;
    constexpr auto operator<=>(const InnerLeftPentagonCell&) const = default;
};
struct InnerRightPentagonCell {
    core::CellId left; LeftAssociatedCell right;
    constexpr auto operator<=>(const InnerRightPentagonCell&) const = default;
};
struct RightPentagonCell {
    core::CellId left; RightAssociatedCell right;
    constexpr auto operator<=>(const RightPentagonCell&) const = default;
};
struct MiddlePentagonCell {
    ProductCell left, right;
    constexpr auto operator<=>(const MiddlePentagonCell&) const = default;
};
using PentagonCell = std::variant<LeftPentagonCell, InnerLeftPentagonCell, InnerRightPentagonCell, RightPentagonCell, MiddlePentagonCell>;
struct ProductPairType {
    std::uint64_t left_type{}, right_type{}, key{};
    bool operator==(const ProductPairType&) const = default;
};
struct ProductPairDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<ProductPairType> types;
};
struct ProductPentagonDeclaration {
    std::uint64_t key{}, revision{};
    ProductAssociatorDeclaration abc, bcd;
    // (((AB)C)D), ((A(BC))D), (A((BC)D)), (A(B(CD))).
    std::array<NestedProductDeclaration, 4> nested;
    ProductPairDeclaration middle;
    // 0->1, 1->2, 2->3, 0->4, 4->3, in retained declaration order.
    std::array<std::vector<ProductAssociatorTypeImage>, 5> edges;
};
struct ProductPentagonCell {
    std::array<PentagonCell, 5> cells;
    std::array<ProductFactorIdentity, 4> factors;
    std::array<std::uint64_t, 5> types;
    bool operator==(const ProductPentagonCell&) const = default;
};
struct PentagonOccurrence {
    PentagonCell upper, lower;
    std::array<ProductBoundarySide, 3> path{};
    std::array<bool, 3> koszul_bits{};
    std::size_t path_length{}, factor{}, attaching_position{}, word_position{};
    core::OccurrenceKey factor_occurrence{};
    bool koszul_flip{}, exists{};
    std::vector<bool> factor_aspects;
    numeric::Rational factor_coefficient{0}, coefficient{0};
    bool operator==(const PentagonOccurrence&) const = default;
};
namespace product_pentagon_detail {
using product_associator_detail::add;
using product_associator_detail::mul;
using product_associator_detail::require;
using product_associator_detail::grade;
using Q = numeric::Rational;
using Guard = numeric::ExactActionGuard;
using Vertex = ProductParenthesization;
using Values = std::map<PentagonCell, Q>;
using Tokens = std::array<std::optional<core::RelationTypeId>, 4>;
using Types = std::map<Tokens, std::array<std::uint64_t, 5>>;
inline constexpr std::array<std::pair<std::size_t, std::size_t>, 5> endpoints{{{0,1},{1,2},{2,3},{0,4},{4,3}}};
inline std::size_t index(Vertex v) {
    const auto i = static_cast<std::size_t>(v); require(i < 5, "unknown product parenthesization"); return i;
}
inline std::array<core::CellId, 4> factors(const PentagonCell& c) {
    switch (c.index()) {
    case 0: { const auto& x = std::get<0>(c); return {x.left.inner.left,x.left.inner.right,x.left.outer,x.right}; }
    case 1: { const auto& x = std::get<1>(c); return {x.left.outer,x.left.inner.left,x.left.inner.right,x.right}; }
    case 2: { const auto& x = std::get<2>(c); return {x.left,x.right.inner.left,x.right.inner.right,x.right.outer}; }
    case 3: { const auto& x = std::get<3>(c); return {x.left,x.right.outer,x.right.inner.left,x.right.inner.right}; }
    case 4: { const auto& x = std::get<4>(c); return {x.left.left,x.left.right,x.right.left,x.right.right}; }
    default: throw std::invalid_argument("valueless product carrier");
    }
}
inline PentagonCell cell(std::size_t v, const std::array<core::CellId, 4>& f) {
    switch (v) {
    case 0: return LeftPentagonCell{{{f[0],f[1]},f[2]},f[3]};
    case 1: return InnerLeftPentagonCell{{f[0],{f[1],f[2]}},f[3]};
    case 2: return InnerRightPentagonCell{f[0],{{f[1],f[2]},f[3]}};
    case 3: return RightPentagonCell{f[0],{f[1],{f[2],f[3]}}};
    case 4: return MiddlePentagonCell{{f[0],f[1]},{f[2],f[3]}};
    default: throw std::invalid_argument("unknown product carrier");
    }
}
inline core::Grade degree(const PentagonCell& c) {
    std::size_t n = 0; for (const auto id : factors(c)) n = add(n,id.grade.value); return grade(n);
}
inline std::size_t words(const std::array<const core::RelationalComplex*,4>& f, const ProductPentagonDeclaration& d) {
    std::size_t n = add(product_associator_detail::words({f[0],f[1],f[2]},d.abc),
                        product_associator_detail::words({f[1],f[2],f[3]},d.bcd));
    for (const auto& t : d.nested) n = add(n,add(2,mul(t.types.size(),3)));
    n = add(n,add(4,mul(d.middle.types.size(),3)));
    for (const auto& e : d.edges) n = add(n,mul(e.size(),2));
    std::array<std::size_t,4> populations{};
    for (std::size_t j = 0; j < 4; ++j) for (std::size_t k = 0; k < f[j]->grade_count(); ++k)
        populations[j] = add(populations[j],owned_product_detail::count(*f[j],k));
    std::size_t cells = 1; for (const auto p : populations) cells = mul(cells,p);
    n = add(n,mul(cells,32));
    for (std::size_t j = 0; j < 4; ++j) {
        std::size_t peers = 1; for (std::size_t i = 0; i < 4; ++i) if (i != j) peers = mul(peers,populations[i]);
        for (std::size_t k = 1; k < f[j]->grade_count(); ++k)
            n = add(n,mul(mul(peers,f[j]->grade(grade(k)).incidence_count()),add(32,f[j]->signature().aspects().size())));
    }
    return n;
}
inline std::uint64_t pair_type(const RelationalProductDeclaration& d, std::optional<core::RelationTypeId> a,
    std::optional<core::RelationTypeId> b) {
    for (const auto& t : d.types) if (t.left == a && t.right == b) return t.key;
    throw std::invalid_argument("missing declared original type pair");
}
inline std::uint64_t nested_type(const NestedProductDeclaration& d, std::uint64_t inner, std::optional<core::RelationTypeId> outer) {
    for (const auto& t : d.types) if (t.inner_type == inner && t.factor_type == outer) return t.key;
    throw std::invalid_argument("missing declared nested type pair");
}
inline std::array<std::uint64_t,5> type_word(const ProductPentagonDeclaration& d, const Tokens& t) {
    const auto ab = pair_type(d.abc.ab,t[0],t[1]), bc = pair_type(d.abc.bc,t[1],t[2]), cd = pair_type(d.bcd.bc,t[2],t[3]);
    const auto abc_l = nested_type(d.abc.left,ab,t[2]), abc_r = nested_type(d.abc.right,bc,t[0]);
    const auto bcd_l = nested_type(d.bcd.left,bc,t[3]), bcd_r = nested_type(d.bcd.right,cd,t[1]);
    std::uint64_t middle = 0;
    for (const auto& x : d.middle.types) if (x.left_type == ab && x.right_type == cd) middle = x.key;
    require(middle != 0,"missing declared product-pair type");
    return {nested_type(d.nested[0],abc_l,t[3]), nested_type(d.nested[1],abc_r,t[3]),
            nested_type(d.nested[2],bcd_l,t[0]), nested_type(d.nested[3],bcd_r,t[0]), middle};
}
struct TypeIndex {
    std::array<owned_product_detail::Types,3> pairs;
    std::array<product_associator_detail::Types,4> triples, nested;
    std::map<std::pair<std::uint64_t,std::uint64_t>,std::uint64_t> middle;
};
inline TypeIndex type_index(const ProductPentagonDeclaration& d, Guard& guard) {
    TypeIndex out;
    const std::array<const RelationalProductDeclaration*,3> pairs{&d.abc.ab,&d.abc.bc,&d.bcd.bc};
    const std::array<const NestedProductDeclaration*,4> triples{&d.abc.left,&d.abc.right,&d.bcd.left,&d.bcd.right};
    for (std::size_t j = 0; j < 3; ++j) for (const auto& t : pairs[j]->types) {
        guard.step(); out.pairs[j].emplace(std::pair{t.left,t.right},t.key);
    }
    for (std::size_t j = 0; j < 4; ++j) {
        for (const auto& t : triples[j]->types) { guard.step(); out.triples[j].emplace(std::pair{t.inner_type,t.factor_type},t.key); }
        for (const auto& t : d.nested[j].types) { guard.step(); out.nested[j].emplace(std::pair{t.inner_type,t.factor_type},t.key); }
    }
    for (const auto& t : d.middle.types) { guard.step(); out.middle.emplace(std::pair{t.left_type,t.right_type},t.key); }
    return out;
}
inline std::array<std::uint64_t,5> type_word(const TypeIndex& d, const Tokens& t) {
    const auto ab = d.pairs[0].at({t[0],t[1]}), bc = d.pairs[1].at({t[1],t[2]}), cd = d.pairs[2].at({t[2],t[3]});
    const auto abc_l = d.triples[0].at({ab,t[2]}), abc_r = d.triples[1].at({bc,t[0]});
    const auto bcd_l = d.triples[2].at({bc,t[3]}), bcd_r = d.triples[3].at({cd,t[1]});
    return {d.nested[0].at({abc_l,t[3]}),d.nested[1].at({abc_r,t[3]}),d.nested[2].at({bcd_l,t[0]}),
            d.nested[3].at({bcd_r,t[0]}),d.middle.at({ab,cd})};
}
struct State {
    OwnedProductAssociator abc, bcd;
    ProductPentagonDeclaration declaration;
    Types types;
    std::array<std::map<std::uint64_t,std::uint64_t>,5> edges;
    std::vector<ProductPentagonCell> cells;
    std::vector<std::size_t> counts;
    std::size_t words{}, envelope{};
    core::Grade maximum{};
};
inline const core::RelationalComplex& factor(const State& s, std::size_t f) {
    if (f < 3) return s.abc.factor(f);
    if (f == 3) return s.bcd.factor(2);
    throw std::out_of_range("pentagon factor");
}
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.abc.validate(guard); s.bcd.validate(guard);
    guard.dimension(s.counts.size()); for (const auto n : s.counts) guard.dimension(n);
    for (const auto& [t,word] : s.types) { (void)t; (void)word; guard.step(); }
    for (const auto& e : s.edges) for (const auto& [a,b] : e) { (void)a; (void)b; guard.step(); }
}
inline ProductPentagonCell certificate(const State& s, const PentagonCell& c) {
    const auto ids = factors(c); Tokens tokens{}; std::array<ProductFactorIdentity,4> identities{};
    for (std::size_t j = 0; j < 4; ++j) {
        identities[j] = owned_product_detail::identity(factor(s,j),ids[j]); tokens[j] = owned_product_detail::type(factor(s,j),ids[j]);
    }
    std::array<PentagonCell,5> cells{cell(0,ids),cell(1,ids),cell(2,ids),cell(3,ids),cell(4,ids)};
    return {std::move(cells),identities,s.types.at(tokens)};
}
inline std::vector<PentagonOccurrence> routes(const State& s, const PentagonCell& c, Guard& guard) {
    // Root-to-leaf paths and the left sibling's original-factor mask at each
    // level. These are the five actual binary trees, not a flattened carrier.
    static constexpr unsigned paths[5][4][3]{
        {{0,0,0},{0,0,1},{0,1,0},{1,0,0}}, {{0,0,0},{0,1,0},{0,1,1},{1,0,0}},
        {{0,0,0},{1,0,0},{1,0,1},{1,1,0}}, {{0,0,0},{1,0,0},{1,1,0},{1,1,1}},
        {{0,0,0},{0,1,0},{1,0,0},{1,1,0}}};
    static constexpr std::size_t lengths[5][4]{{3,3,2,1},{2,3,3,1},{1,3,3,2},{1,2,3,3},{2,2,2,2}};
    static constexpr unsigned masks[5][4][3]{
        {{0,0,0},{0,0,1},{0,3,0},{7,0,0}}, {{0,0,0},{0,1,0},{0,1,2},{7,0,0}},
        {{0,0,0},{1,0,0},{1,0,2},{1,6,0}}, {{0,0,0},{1,0,0},{1,2,0},{1,2,4}},
        {{0,0,0},{0,1,0},{3,0,0},{3,4,0}}};
    (void)certificate(s,c); const auto ids = factors(c); const auto v = c.index(); std::vector<PentagonOccurrence> out;
    for (std::size_t f = 0; f < 4; ++f) {
        if (!ids[f].grade.value) continue;
        const auto& source = factor(s,f); const auto& a = source.grade(ids[f].grade); const auto [begin,end] = a.incidence_range(ids[f].index);
        std::array<ProductBoundarySide,3> path{}; std::array<bool,3> bits{}; bool flip = false;
        for (std::size_t j = 0; j < lengths[v][f]; ++j) {
            path[j] = paths[v][f][j] ? ProductBoundarySide::Right : ProductBoundarySide::Left;
            std::size_t left_degree = 0;
            for (std::size_t i = 0; i < 4; ++i) if ((masks[v][f][j] & (1u << i)) != 0) left_degree = add(left_degree,ids[i].grade.value);
            bits[j] = (left_degree % 2) != 0; flip = flip != bits[j];
        }
        for (auto p = begin; p < end; ++p) {
            guard.step(); auto lower = ids; lower[f] = {grade(ids[f].grade.value-1u),a.lower_index[p]};
            const auto original = owned_product_detail::coefficient(source,ids[f].grade,p,guard);
            const auto realized = flip ? guard.sub(Q{0},original) : original; std::vector<bool> aspects;
            for (std::size_t j = 0; j < source.signature().aspects().size(); ++j) {
                guard.step(); aspects.push_back(a.aspects.get(static_cast<composite::AspectId>(j),p));
            }
            guard.entries(add(s.envelope,mul(add(out.size(),1),4)));
            out.push_back({c,cell(v,lower),path,bits,lengths[v][f],f,p-begin,out.size(),a.occurrence_identities[p],flip,
                a.aspects.get(source.signature().existence_aspect(),p),std::move(aspects),original,realized});
        }
    }
    return out;
}
inline std::array<std::vector<PentagonOccurrence>,5> flags(const State& s, const ProductPentagonCell& c, Guard& guard) {
    std::array<std::vector<PentagonOccurrence>,5> words;
    for (std::size_t v = 0; v < 5; ++v) words[v] = routes(s,c.cells[v],guard);
    for (std::size_t v = 1; v < 5; ++v) {
        require(words[v].size() == words[0].size(),"pentagon occurrence fibers differ");
        for (std::size_t i = 0; i < words[0].size(); ++i) {
            guard.step(); const auto& a = words[0][i]; const auto& b = words[v][i];
            require(factors(a.lower) == factors(b.lower) && a.factor == b.factor && a.attaching_position == b.attaching_position &&
                a.word_position == b.word_position && a.factor_occurrence == b.factor_occurrence && a.factor_aspects == b.factor_aspects &&
                a.exists == b.exists && a.factor_coefficient == b.factor_coefficient && a.coefficient == b.coefficient && a.koszul_flip == b.koszul_flip,
                "pentagon changed an original occurrence or total Koszul parity");
        }
    }
    guard.entries(add(s.envelope,mul(words[0].size(),20))); return words;
}
inline void values(const State& s, Vertex v, core::Grade g, const Values& data, Guard& guard) {
    const auto i = index(v); require(g.value <= s.maximum.value,"pentagon field grade outside declared carrier");
    guard.entries(add(s.envelope,mul(data.size(),2)));
    for (const auto& [c,q] : data) {
        guard.step(); require(c.index() == i && degree(c) == g,"pentagon field has a foreign carrier or grade");
        (void)certificate(s,c); guard.value(q);
    }
}
} // namespace product_pentagon_detail
inline core::Grade pentagon_grade(const PentagonCell& c) { return product_pentagon_detail::degree(c); }
class OwnedProductPentagon;
class ProductPentagonCellWitness;
class ProductPentagonChainWitness;
class OwnedPentagonField {
public:
    ProductParenthesization parenthesization() const { return vertex_; }
    core::Grade grade() const { return grade_; }
    const product_pentagon_detail::Values& values() const & { return values_; }
    const product_pentagon_detail::Values& values() const && = delete;
    OwnedProductPentagon plan() const;
private:
    friend class OwnedProductPentagon;
    OwnedPentagonField(std::shared_ptr<const product_pentagon_detail::State> s, ProductParenthesization v, core::Grade g,
        product_pentagon_detail::Values data) : state_(std::move(s)), vertex_(v), grade_(g), values_(std::move(data)) {}
    std::shared_ptr<const product_pentagon_detail::State> state_;
    ProductParenthesization vertex_;
    core::Grade grade_;
    product_pentagon_detail::Values values_;
};
class OwnedProductPentagon {
public:
    using Values = product_pentagon_detail::Values;
    static OwnedProductPentagon prepare(std::array<core::RelationalComplex,4> factors, ProductPentagonDeclaration input,
        numeric::ExactActionGuard& guard) {
        namespace detail = product_pentagon_detail; auto d = input;
        const auto words = detail::words({&factors[0],&factors[1],&factors[2],&factors[3]},d), envelope = detail::add(1,detail::mul(words,16));
        guard.entries(envelope); guard.step(); detail::require(d.key && d.revision,"pentagon requires a named revised declaration");
        detail::require(d.abc.bc.key == d.bcd.ab.key && d.abc.bc.revision == d.bcd.ab.revision && d.abc.bc.types == d.bcd.ab.types,
            "pentagon requires its complete common BC constructor");
        std::size_t max = 0; for (const auto& f : factors) max = detail::add(max,f.grade_count()-1);
        const auto maximum = detail::grade(max); guard.dimension(detail::add(max,1));
        auto abc = OwnedProductAssociator::prepare({factors[0],factors[1],factors[2]},d.abc,guard);
        auto bcd = OwnedProductAssociator::prepare({factors[1],factors[2],factors[3]},d.bcd,guard);
        const std::array<const NestedProductDeclaration*,4> inner{&d.abc.left,&d.abc.right,&d.bcd.left,&d.bcd.right};
        for (std::size_t v = 0; v < 4; ++v) {
            const auto& outer = d.nested[v]; detail::require(outer.key && outer.revision,"unnamed nested pentagon constructor");
            std::set<std::uint64_t> outputs, inputs;
            for (const auto& t : inner[v]->types) inputs.insert(t.key);
            std::set<std::pair<std::uint64_t,std::optional<core::RelationTypeId>>> pairs;
            const auto count = factors[v < 2 ? 3 : 0].signature().type_count();
            detail::require(outer.types.size() == detail::mul(inputs.size(),detail::add(count,1)),"incomplete nested pentagon types");
            for (const auto& t : outer.types) { guard.step(); detail::require(t.key && inputs.contains(t.inner_type) &&
                (!t.factor_type || *t.factor_type < count) && outputs.insert(t.key).second && pairs.emplace(t.inner_type,t.factor_type).second,
                "invalid or duplicate nested pentagon type"); }
        }
        detail::require(d.middle.key && d.middle.revision && d.middle.types.size() == detail::mul(d.abc.ab.types.size(),d.bcd.bc.types.size()),
            "incomplete or unnamed product-pair constructor");
        std::set<std::uint64_t> ab_keys, cd_keys, middle_keys; std::set<std::pair<std::uint64_t,std::uint64_t>> pairs;
        for (const auto& t : d.abc.ab.types) ab_keys.insert(t.key);
        for (const auto& t : d.bcd.bc.types) cd_keys.insert(t.key);
        for (const auto& t : d.middle.types) { guard.step(); detail::require(t.key && ab_keys.contains(t.left_type) && cd_keys.contains(t.right_type) &&
            middle_keys.insert(t.key).second && pairs.emplace(t.left_type,t.right_type).second,"invalid product-pair type"); }
        std::array<std::map<std::uint64_t,std::uint64_t>,5> edges; std::array<std::set<std::uint64_t>,5> targets;
        for (std::size_t e = 0; e < 5; ++e) for (const auto& t : d.edges[e]) {
            guard.step(); detail::require(t.left && t.right && edges[e].emplace(t.left,t.right).second && targets[e].insert(t.right).second,
                "duplicate or invalid pentagon type image");
        }
        const auto type_index = detail::type_index(d,guard); detail::Types types; detail::Tokens tokens{};
        const auto visit = [&](auto&& self, std::size_t f) -> void {
            if (f < 4) {
                for (std::size_t t = 0; t <= factors[f].signature().type_count(); ++t) {
                    tokens[f] = t ? std::optional{static_cast<core::RelationTypeId>(t-1)} : std::nullopt; self(self,f+1);
                } return;
            }
            guard.step(); const auto word = detail::type_word(type_index,tokens);
            for (std::size_t e = 0; e < 5; ++e) { const auto [a,b] = detail::endpoints[e];
                detail::require(edges[e].contains(word[a]) && edges[e].at(word[a]) == word[b],"pentagon type image changes an ordered original factor"); }
            detail::require(edges[2].at(edges[1].at(edges[0].at(word[0]))) == edges[4].at(edges[3].at(word[0])),"type pentagon does not commute");
            types.emplace(tokens,word);
        }; visit(visit,0);
        for (const auto& e : edges) detail::require(e.size() == types.size(),"pentagon type image has foreign directions");
        auto state = std::make_shared<detail::State>(detail::State{std::move(abc),std::move(bcd),std::move(d),std::move(types),std::move(edges),{},
            std::vector<std::size_t>(detail::add(max,1)),words,envelope,maximum});
        std::array<core::CellId,4> ids{};
        const auto cells = [&](auto&& self, std::size_t f) -> void {
            if (f < 4) {
                const auto& source = detail::factor(*state,f);
                for (std::size_t k = 0; k < source.grade_count(); ++k) {
                    for (std::size_t j = 0; j < source.grade(detail::grade(k)).cell_count(); ++j) {
                        if (j > std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("pentagon factor index overflow");
                        ids[f] = {detail::grade(k),static_cast<std::uint32_t>(j)}; self(self,f+1);
                    }
                }
                return;
            }
            guard.step(); auto c = detail::certificate(*state,detail::cell(0,ids)); (void)detail::flags(*state,c,guard);
            auto& count = state->counts[detail::degree(c.cells[0]).value]; count = detail::add(count,1); state->cells.push_back(std::move(c));
        }; cells(cells,0); detail::validate(*state,guard); return OwnedProductPentagon(std::move(state));
    }
    core::Grade max_grade() const { return state_->maximum; }
    std::size_t retained_words() const { return state_->words; }
    std::size_t entry_envelope() const { return state_->envelope; }
    std::size_t cell_count(core::Grade g) const { return state_->counts.at(g.value); }
    bool same_owner(const OwnedProductPentagon& other) const noexcept { return state_ == other.state_; }
    const core::RelationalComplex& factor(std::size_t f) const & { return product_pentagon_detail::factor(*state_,f); }
    const core::RelationalComplex& factor(std::size_t) const && = delete;
    OwnedProductAssociator inner_associator(bool bcd) const { return bcd ? state_->bcd : state_->abc; }
    const ProductPentagonDeclaration& declaration() const & { return state_->declaration; }
    const ProductPentagonDeclaration& declaration() const && = delete;
    const std::vector<ProductPentagonCell>& cells() const & { return state_->cells; }
    const std::vector<ProductPentagonCell>& cells() const && = delete;
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; product_pentagon_detail::validate(*pin,guard); }
    OwnedPentagonField field(ProductParenthesization v, core::Grade g, Values input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_; auto values = input; product_pentagon_detail::validate(*pin,guard);
        product_pentagon_detail::values(*pin,v,g,values,guard);
        for (auto& [c,q] : values) { (void)c; q.canonicalize(); } return OwnedPentagonField(pin,v,g,std::move(values));
    }
    OwnedPentagonField boundary(OwnedPentagonField input, numeric::ExactActionGuard& guard) const {
        namespace detail = product_pentagon_detail; const auto pin = state_;
        detail::require(pin == input.state_,"pentagon boundary requires its actual field owner"); detail::validate(*pin,guard);
        detail::values(*pin,input.vertex_,input.grade_,input.values_,guard); detail::require(input.grade_.value > 0,"pentagon boundary at grade zero");
        Values out;
        for (const auto& [c,q] : input.values_) for (const auto& o : detail::routes(*pin,c,guard)) {
            guard.entries(detail::add(pin->envelope,detail::mul(detail::add(out.size(),1),4))); auto& value = out.try_emplace(o.lower,numeric::Rational{0}).first->second;
            if (o.exists) value = guard.add(value,guard.mul(q,o.coefficient));
        }
        return OwnedPentagonField(pin,input.vertex_,detail::grade(input.grade_.value-1u),std::move(out));
    }
    OwnedPentagonField traverse(OwnedPentagonField input, std::size_t edge, bool reverse, numeric::ExactActionGuard& guard) const {
        namespace detail = product_pentagon_detail; const auto pin = state_;
        detail::require(pin == input.state_,"pentagon edge requires its actual field owner"); detail::validate(*pin,guard);
        detail::values(*pin,input.vertex_,input.grade_,input.values_,guard);
        if (edge >= 5) throw std::out_of_range("pentagon edge");
        const auto [a,b] = detail::endpoints[edge]; const auto from = reverse ? b : a, to = reverse ? a : b;
        detail::require(detail::index(input.vertex_) == from,"pentagon edge has the wrong source carrier"); Values out;
        for (const auto& [c,q] : input.values_) { guard.step(); const auto cert = detail::certificate(*pin,c);
            detail::require(pin->edges[edge].at(cert.types[a]) == cert.types[b],"pentagon edge type mismatch"); out.emplace(cert.cells[to],q); }
        return OwnedPentagonField(pin,static_cast<ProductParenthesization>(to),input.grade_,std::move(out));
    }
    ProductPentagonCellWitness cell_witness(PentagonCell, numeric::ExactActionGuard&) const;
    ProductPentagonChainWitness witness(OwnedPentagonField, numeric::ExactActionGuard&) const;
private:
    friend class OwnedPentagonField;
    explicit OwnedProductPentagon(std::shared_ptr<const product_pentagon_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const product_pentagon_detail::State> state_;
};
inline OwnedProductPentagon OwnedPentagonField::plan() const { return OwnedProductPentagon(state_); }
class ProductPentagonCellWitness {
public:
    OwnedProductPentagon plan() const { return plan_; }
    const ProductPentagonCell& cell() const & { return cell_; }
    const ProductPentagonCell& cell() const && = delete;
    const std::array<std::vector<PentagonOccurrence>,5>& flags() const & { return flags_; }
    const std::array<std::vector<PentagonOccurrence>,5>& flags() const && = delete;
private:
    friend class OwnedProductPentagon;
    ProductPentagonCellWitness(OwnedProductPentagon p, ProductPentagonCell c, std::array<std::vector<PentagonOccurrence>,5> f)
        : plan_(std::move(p)), cell_(std::move(c)), flags_(std::move(f)) {}
    OwnedProductPentagon plan_;
    ProductPentagonCell cell_;
    std::array<std::vector<PentagonOccurrence>,5> flags_;
};
struct ProductPentagonEdgeWitness {
    std::size_t edge{};
    OwnedPentagonField input, image, round_trip;
    std::optional<OwnedPentagonField> mapped_boundary, image_boundary;
};
class ProductPentagonChainWitness {
public:
    OwnedProductPentagon plan() const { return plan_; }
    const std::array<OwnedPentagonField,4>& long_path() const & { return long_; }
    const std::array<OwnedPentagonField,4>& long_path() const && = delete;
    const std::array<OwnedPentagonField,3>& short_path() const & { return short_; }
    const std::array<OwnedPentagonField,3>& short_path() const && = delete;
    const std::array<ProductPentagonEdgeWitness,5>& edges() const & { return edges_; }
    const std::array<ProductPentagonEdgeWitness,5>& edges() const && = delete;
private:
    friend class OwnedProductPentagon;
    ProductPentagonChainWitness(OwnedProductPentagon p, std::array<OwnedPentagonField,4> l, std::array<OwnedPentagonField,3> s,
        std::array<ProductPentagonEdgeWitness,5> e) : plan_(std::move(p)), long_(std::move(l)), short_(std::move(s)), edges_(std::move(e)) {}
    OwnedProductPentagon plan_;
    std::array<OwnedPentagonField,4> long_;
    std::array<OwnedPentagonField,3> short_;
    std::array<ProductPentagonEdgeWitness,5> edges_;
};
inline ProductPentagonCellWitness OwnedProductPentagon::cell_witness(PentagonCell c, numeric::ExactActionGuard& guard) const {
    const auto pin = state_; product_pentagon_detail::validate(*pin,guard); auto cert = product_pentagon_detail::certificate(*pin,c);
    auto flags = product_pentagon_detail::flags(*pin,cert,guard);
    return ProductPentagonCellWitness(OwnedProductPentagon(pin),std::move(cert),std::move(flags));
}
inline ProductPentagonChainWitness OwnedProductPentagon::witness(OwnedPentagonField input, numeric::ExactActionGuard& guard) const {
    namespace detail = product_pentagon_detail; const auto pin = state_; const auto owner = OwnedProductPentagon(pin);
    detail::require(pin == input.state_,"pentagon witness requires its actual field owner");
    detail::require(input.vertex_ == ProductParenthesization::Left,"pentagon paths start at the left nested carrier");
    auto one = owner.traverse(input,0,false,guard), two = owner.traverse(one,1,false,guard), three = owner.traverse(two,2,false,guard);
    auto middle = owner.traverse(input,3,false,guard), right = owner.traverse(middle,4,false,guard);
    detail::require(three.values() == right.values(),"field pentagon does not commute including support");
    const auto edge = [&](OwnedPentagonField from, OwnedPentagonField image, std::size_t e) {
        auto back = owner.traverse(image,e,true,guard); detail::require(back.values() == from.values(),"pentagon edge round trip changes support");
        std::optional<OwnedPentagonField> mapped, direct;
        if (from.grade().value) {
            mapped = owner.traverse(owner.boundary(from,guard),e,false,guard); direct = owner.boundary(image,guard);
            detail::require(mapped->values() == direct->values(),"pentagon edge does not commute with the exact boundary");
        }
        return ProductPentagonEdgeWitness{e,std::move(from),std::move(image),std::move(back),std::move(mapped),std::move(direct)};
    };
    std::array<ProductPentagonEdgeWitness,5> edges{edge(input,one,0),edge(one,two,1),edge(two,three,2),edge(input,middle,3),edge(middle,right,4)};
    return ProductPentagonChainWitness(owner,{input,one,two,three},{input,middle,right},std::move(edges));
}
} // namespace rfc::algebra
