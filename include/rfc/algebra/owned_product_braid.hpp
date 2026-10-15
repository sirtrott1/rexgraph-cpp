#pragma once
#include "rfc/algebra/owned_product_hexagon.hpp"

namespace rfc::algebra {
// Adjacent signed interchanges in the declared symmetric rational realization.
enum class ProductBraidGenerator { FirstPair, SecondPair };
struct ProductBraidDeclaration { std::uint64_t key{}, revision{}; };
struct ProductBraidTypeNode {
    ProductHexagonCarrier carrier;
    std::uint64_t type{};
    bool operator==(const ProductBraidTypeNode&) const = default;
};
struct ProductBraidTypeWitness {
    std::array<std::optional<core::RelationTypeId>,3> original_types;
    ProductHexagonCarrier source;
    std::array<std::vector<ProductBraidTypeNode>,2> routes;
};
struct ProductBraidFlagTrace {
    std::size_t original_factor{};
    std::vector<std::size_t> word_positions;
    bool upper_interchange_flip{}, lower_interchange_flip{};
    numeric::Rational mapped_boundary_coefficient, image_boundary_coefficient;
};
struct ProductBraidCellRoute {
    std::vector<ProductHexagonCarrier> carriers;
    // Indices into the owning hexagon certificate, including reassociations.
    std::vector<std::size_t> edges;
    std::vector<ProductBraidFlagTrace> flags;
};
namespace product_braid_detail {
using namespace product_hexagon_detail;
inline constexpr std::array<std::array<ProductBraidGenerator,3>,2> routes{{
    {ProductBraidGenerator::FirstPair,ProductBraidGenerator::SecondPair,ProductBraidGenerator::FirstPair},
    {ProductBraidGenerator::SecondPair,ProductBraidGenerator::FirstPair,ProductBraidGenerator::SecondPair}}};
inline std::vector<ProductHexagonAction> actions(ProductHexagonCarrier c,ProductBraidGenerator g) {
    (void)index(c);
    require(g==ProductBraidGenerator::FirstPair || g==ProductBraidGenerator::SecondPair,"unknown adjacent product generator");
    if ((g==ProductBraidGenerator::FirstPair)==(c.association==ProductAssociation::Left)) return {ProductHexagonAction::InnerInterchange};
    return {ProductHexagonAction::Reassociate,ProductHexagonAction::InnerInterchange,ProductHexagonAction::Reassociate};
}
inline ProductHexagonCarrier target(ProductHexagonCarrier c,ProductBraidGenerator g) {
    for (const auto a : actions(c,g)) c=product_hexagon_detail::target(c,a);
    return c;
}
inline std::vector<ProductHexagonAction> word(ProductHexagonCarrier c,std::size_t route) {
    require(route<routes.size(),"unknown product braid route"); std::vector<ProductHexagonAction> out;
    for (const auto g : routes[route]) for (const auto a : actions(c,g)) { out.push_back(a); c=product_hexagon_detail::target(c,a); }
    return out;
}
inline std::size_t triple_count(const OwnedProductHexagon& h) {
    std::size_t n=1;
    for (std::size_t i=0;i<3;++i) n=mul(n,add(h.factor(i).signature().type_count(),1));
    return n;
}
inline std::size_t words(const OwnedProductHexagon& h) { return add(h.retained_words(),add(64,mul(1920,triple_count(h)))); }
inline std::size_t envelope(const OwnedProductHexagon& h) { return add(h.entry_envelope(),mul(16,add(64,mul(1920,triple_count(h))))); }
inline std::size_t entries(const OwnedProductHexagon& h,std::size_t support=0) { return add(mul(envelope(h),4),mul(support,2048)); }
inline std::vector<ProductBraidTypeWitness> types(const OwnedProductHexagon& h,Guard& guard) {
    const auto& d=h.declaration();
    std::array<owned_product_detail::Types,12> pairs;
    std::array<product_associator_detail::Types,12> nested;
    std::array<std::map<std::uint64_t,std::uint64_t>,12> edges;
    for (std::size_t i=0;i<6;++i) {
        const auto& a=d.associators[i];
        for (const auto& t : a.ab.types) { guard.step(); pairs[2*i].emplace(std::pair{t.left,t.right},t.key); }
        for (const auto& t : a.bc.types) { guard.step(); pairs[2*i+1].emplace(std::pair{t.left,t.right},t.key); }
        for (const auto& t : a.left.types) { guard.step(); nested[2*i].emplace(std::pair{t.inner_type,t.factor_type},t.key); }
        for (const auto& t : a.right.types) { guard.step(); nested[2*i+1].emplace(std::pair{t.inner_type,t.factor_type},t.key); }
    }
    for (std::size_t i=0;i<12;++i) for (const auto& t : d.swaps[i].inner) { guard.step(); edges[i].emplace(t.source,t.target); }
    const auto token=[](std::size_t i)->std::optional<core::RelationTypeId>{return i ? std::optional{static_cast<core::RelationTypeId>(i-1)} : std::nullopt;};
    std::vector<ProductBraidTypeWitness> out;
    for (std::size_t a=0;a<=h.factor(0).signature().type_count();++a) for (std::size_t b=0;b<=h.factor(1).signature().type_count();++b)
        for (std::size_t c=0;c<=h.factor(2).signature().type_count();++c) {
            TypeTuple tuple{token(a),token(b),token(c)}; std::array<std::uint64_t,12> roots{};
            for (std::size_t i=0;i<6;++i) { guard.step(); const auto o=orders[i];
                roots[2*i]=nested[2*i].at({pairs[2*i].at({tuple[o[0]],tuple[o[1]]}),tuple[o[2]]});
                roots[2*i+1]=nested[2*i+1].at({pairs[2*i+1].at({tuple[o[1]],tuple[o[2]]}),tuple[o[0]]}); }
            for (std::size_t i=0;i<12;++i) {
                ProductBraidTypeWitness w{tuple,carrier(i),{}};
                for (std::size_t r=0;r<2;++r) {
                    ProductBraidTypeNode node{carrier(i),roots[i]}; w.routes[r].push_back(node);
                    for (const auto action : word(node.carrier,r)) {
                        guard.step(); const auto from=node.carrier;
                        if (action==ProductHexagonAction::Reassociate) {
                            const auto& ts=d.associators[index(from.order)].types;
                            const auto it=std::find_if(ts.begin(),ts.end(),[&](const auto& t){return (from.association==ProductAssociation::Left ? t.left : t.right)==node.type;});
                            require(it!=ts.end(),"braid reassociation needs its complete declared type direction");
                            node.type=from.association==ProductAssociation::Left ? it->right : it->left;
                        } else node.type=edges[index(from)].at(node.type);
                        node.carrier=product_hexagon_detail::target(from,action);
                        require(node.type==roots[index(node.carrier)],"braid changes an original constructor triple");
                        w.routes[r].push_back(node);
                    }
                }
                require(w.routes[0].back()==w.routes[1].back(),"complete Yang-Baxter type identity"); out.push_back(std::move(w));
            }
        }
    return out;
}
inline bool parity(const std::array<core::CellId,3>& ids) {
    const bool p=ids[0].grade.value%2!=0,q=ids[1].grade.value%2!=0,r=ids[2].grade.value%2!=0;
    return ((p&&q)!=(p&&r))!=(q&&r);
}
inline std::array<std::array<ProductBraidCellRoute,2>,12> certificate(const ProductHexagonCellWitness& h,Guard& guard) {
    std::array<std::array<ProductBraidCellRoute,2>,12> out;
    for (std::size_t i=0;i<12;++i) for (std::size_t r=0;r<2;++r) {
        auto& route=out[i][r]; auto from=carrier(i); route.carriers.push_back(from);
        for (const auto action : word(from,r)) {
            guard.step(); const auto edge=add(mul(index(from),3),static_cast<std::size_t>(action)); const auto& e=h.edges().at(edge);
            require(e.source==from && e.action==action,"braid must use the actual hexagon edge");
            route.edges.push_back(edge); from=e.target; route.carriers.push_back(from);
        }
        const auto& source=h.carriers().at(i);
        for (std::size_t j=0;j<source.flags.size();++j) {
            const auto& flag=source.flags[j]; ProductBraidFlagTrace trace{orders[i/2][flag.factor],{j},false,false,flag.coefficient,Q{0}};
            auto position=j;
            for (const auto e : route.edges) {
                guard.step(); const auto& edge=h.edges()[e]; const auto& f=edge.flags.at(position);
                require(f.original_factor==trace.original_factor && f.source_word_position==position,"braid preserves original flag role and position");
                trace.upper_interchange_flip=trace.upper_interchange_flip!=f.upper_interchange_flip;
                trace.lower_interchange_flip=trace.lower_interchange_flip!=f.lower_interchange_flip;
                if (f.lower_interchange_flip) trace.mapped_boundary_coefficient=guard.sub(Q{0},trace.mapped_boundary_coefficient);
                position=f.target_word_position; trace.word_positions.push_back(position);
            }
            const auto& last=h.carriers().at(index(from)).flags.at(position);
            trace.image_boundary_coefficient=trace.upper_interchange_flip ? guard.sub(Q{0},last.coefficient) : last.coefficient;
            require(trace.upper_interchange_flip==parity(global(source.carrier,flag.upper)) && trace.lower_interchange_flip==parity(global(source.carrier,flag.lower)),"three-factor signed parity");
            require(trace.mapped_boundary_coefficient==trace.image_boundary_coefficient,"Yang-Baxter flag boundary identity"); route.flags.push_back(std::move(trace));
        }
    }
    for (std::size_t i=0;i<12;++i) {
        require(out[i][0].carriers.back()==out[i][1].carriers.back(),"Yang-Baxter actual target carrier");
        for (std::size_t j=0;j<out[i][0].flags.size();++j) { guard.step(); const auto& a=out[i][0].flags[j]; const auto& b=out[i][1].flags[j];
            require(a.original_factor==b.original_factor && a.word_positions.back()==b.word_positions.back() && a.upper_interchange_flip==b.upper_interchange_flip &&
                a.lower_interchange_flip==b.lower_interchange_flip && a.mapped_boundary_coefficient==b.mapped_boundary_coefficient && a.image_boundary_coefficient==b.image_boundary_coefficient,
                "Yang-Baxter routes preserve the same original flag"); }
    }
    return out;
}
struct State {
    OwnedProductHexagon hexagon;
    ProductBraidDeclaration declaration;
    std::vector<ProductBraidTypeWitness> types;
    std::size_t words{},envelope{};
};
inline void validate(const State& s,Guard& guard,std::size_t support=0) {
    guard.entries(entries(s.hexagon,support));
    // This validates every original source, strict chain family and unused scalar.
    (void)s.hexagon.field(carrier(0),{0},{},guard);
    for (const auto& t : s.types) { (void)t; guard.step(); }
}
} // namespace product_braid_detail
class ProductBraidCellWitness;
class ProductBraidStepWitness;
class ProductBraidWitness;
class OwnedProductBraid {
public:
    static OwnedProductBraid adopt(OwnedProductHexagon h,ProductBraidDeclaration input,product_braid_detail::Guard& guard) {
        namespace detail=product_braid_detail; const auto d=input;
        const auto words=detail::words(h),envelope=detail::envelope(h); guard.entries(detail::entries(h));
        detail::require(d.key && d.revision,"product braid requires an identity and revision");
        (void)h.field(detail::carrier(0),{0},{},guard); auto types=detail::types(h,guard);
        for (const auto& ids : h.cells()) { guard.step(); const auto w=h.cell_witness(ids,guard); (void)detail::certificate(w,guard); }
        return OwnedProductBraid(std::make_shared<const detail::State>(detail::State{std::move(h),d,std::move(types),words,envelope}));
    }
    bool same_owner(const OwnedProductBraid& other) const noexcept { return state_==other.state_; }
    OwnedProductHexagon hexagon() const { return state_->hexagon; }
    const ProductBraidDeclaration& declaration() const & { return state_->declaration; }
    const ProductBraidDeclaration& declaration() const && = delete;
    const std::vector<ProductBraidTypeWitness>& type_witnesses() const & { return state_->types; }
    const std::vector<ProductBraidTypeWitness>& type_witnesses() const && = delete;
    std::size_t retained_words() const noexcept { return state_->words; }
    std::size_t entry_envelope() const noexcept { return state_->envelope; }
    OwnedAssociatedProductField adjacent(OwnedAssociatedProductField,ProductBraidGenerator,product_braid_detail::Guard&) const;
    ProductBraidCellWitness cell_witness(std::array<core::CellId,3>,product_braid_detail::Guard&) const;
    ProductBraidStepWitness step_witness(OwnedAssociatedProductField,ProductBraidGenerator,product_braid_detail::Guard&) const;
    ProductBraidWitness witness(OwnedAssociatedProductField,product_braid_detail::Guard&) const;
private:
    explicit OwnedProductBraid(std::shared_ptr<const product_braid_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const product_braid_detail::State> state_;
};
class ProductBraidCellWitness {
public:
    OwnedProductBraid plan() const { return plan_; }
    const ProductHexagonCellWitness& hexagon() const & { return hexagon_; }
    const ProductHexagonCellWitness& hexagon() const && = delete;
    const std::array<std::array<ProductBraidCellRoute,2>,12>& routes() const & { return routes_; }
    const std::array<std::array<ProductBraidCellRoute,2>,12>& routes() const && = delete;
private:
    friend class OwnedProductBraid;
    ProductBraidCellWitness(OwnedProductBraid p,ProductHexagonCellWitness h,std::array<std::array<ProductBraidCellRoute,2>,12> r)
        : plan_(std::move(p)),hexagon_(std::move(h)),routes_(std::move(r)) {}
    OwnedProductBraid plan_;
    ProductHexagonCellWitness hexagon_;
    std::array<std::array<ProductBraidCellRoute,2>,12> routes_;
};
struct ProductBraidStepData {
    ProductBraidGenerator generator;
    OwnedAssociatedProductField input,image,round_trip;
    std::vector<ProductHexagonEdgeWitness> edges;
    std::vector<OwnedAssociatedProductField> inverse_path,boundary_path;
    std::optional<OwnedAssociatedProductField> image_boundary;
};
class ProductBraidStepWitness {
public:
    OwnedProductBraid plan() const { return plan_; }
    const ProductBraidStepData& data() const & { return data_; }
    const ProductBraidStepData& data() const && = delete;
private:
    friend class OwnedProductBraid;
    ProductBraidStepWitness(OwnedProductBraid p,ProductBraidStepData d) : plan_(std::move(p)),data_(std::move(d)) {}
    OwnedProductBraid plan_;
    ProductBraidStepData data_;
};
struct ProductBraidPathWitness {
    std::array<ProductBraidGenerator,3> generators;
    std::vector<OwnedAssociatedProductField> path;
    std::vector<ProductBraidStepWitness> steps;
};
class ProductBraidWitness {
public:
    OwnedProductBraid plan() const { return plan_; }
    const std::array<ProductBraidPathWitness,2>& routes() const & { return routes_; }
    const std::array<ProductBraidPathWitness,2>& routes() const && = delete;
private:
    friend class OwnedProductBraid;
    ProductBraidWitness(OwnedProductBraid p,std::array<ProductBraidPathWitness,2> r) : plan_(std::move(p)),routes_(std::move(r)) {}
    OwnedProductBraid plan_;
    std::array<ProductBraidPathWitness,2> routes_;
};
inline OwnedAssociatedProductField OwnedProductBraid::adjacent(OwnedAssociatedProductField input,ProductBraidGenerator g,product_braid_detail::Guard& guard) const {
    namespace detail=product_braid_detail; const auto pin=state_; const auto actions=detail::actions(pin->hexagon.carrier(input),g);
    detail::validate(*pin,guard,input.values().size());
    for (const auto action : actions) input=pin->hexagon.step(std::move(input),action,guard);
    return input;
}
inline ProductBraidCellWitness OwnedProductBraid::cell_witness(std::array<core::CellId,3> ids,product_braid_detail::Guard& guard) const {
    const auto pin=state_; product_braid_detail::validate(*pin,guard);
    auto h=pin->hexagon.cell_witness(ids,guard); auto r=product_braid_detail::certificate(h,guard);
    return ProductBraidCellWitness(OwnedProductBraid(pin),std::move(h),std::move(r));
}
inline ProductBraidStepWitness OwnedProductBraid::step_witness(OwnedAssociatedProductField input,ProductBraidGenerator g,product_braid_detail::Guard& guard) const {
    namespace detail=product_braid_detail; const auto pin=state_; const auto& h=pin->hexagon; const auto actions=detail::actions(h.carrier(input),g);
    detail::validate(*pin,guard,input.values().size());
    ProductBraidStepData out{g,input,input,input,{},{},{},{}};
    if (input.grade().value) out.boundary_path.push_back(h.boundary(input,guard));
    for (const auto a : actions) {
        auto e=h.edge_witness(out.image,a,guard); out.image=e.image;
        if (e.mapped_boundary) out.boundary_path.push_back(*e.mapped_boundary);
        out.image_boundary=e.image_boundary; out.edges.push_back(std::move(e));
    }
    out.inverse_path.push_back(out.image); auto back=out.image;
    for (auto it=actions.rbegin();it!=actions.rend();++it) { back=h.step(std::move(back),*it,guard); out.inverse_path.push_back(back); }
    detail::require(h.carrier(back)==h.carrier(input) && back.values()==input.values(),"adjacent signed interchange round trip"); out.round_trip=std::move(back);
    return ProductBraidStepWitness(OwnedProductBraid(pin),std::move(out));
}
inline ProductBraidWitness OwnedProductBraid::witness(OwnedAssociatedProductField input,product_braid_detail::Guard& guard) const {
    namespace detail=product_braid_detail; const auto pin=state_; const OwnedProductBraid owner(pin); (void)pin->hexagon.carrier(input);
    const auto path=[&](std::size_t r) {
        ProductBraidPathWitness out{detail::routes[r],{input},{}};
        for (const auto g : out.generators) { auto step=owner.step_witness(out.path.back(),g,guard); out.path.push_back(step.data().image); out.steps.push_back(std::move(step)); }
        return out;
    };
    auto first=path(0),second=path(1);
    detail::require(pin->hexagon.carrier(first.path.back())==pin->hexagon.carrier(second.path.back()) && first.path.back().values()==second.path.back().values(),"exact Yang-Baxter field identity");
    return ProductBraidWitness(owner,{std::move(first),std::move(second)});
}
} // namespace rfc::algebra
