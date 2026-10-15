#pragma once
#include "rfc/algebra/owned_product_associator.hpp"
#include "rfc/algebra/owned_product_interchange.hpp"

namespace rfc::algebra {
enum class ProductPermutation { ABC, ACB, BAC, BCA, CAB, CBA };
struct ProductHexagonCarrier {
    ProductPermutation order;
    ProductAssociation association;
    bool operator==(const ProductHexagonCarrier&) const = default;
};
enum class ProductHexagonAction { Reassociate, InnerInterchange, BlockInterchange };
struct ProductHexagonTypeImage {
    std::uint64_t source{}, target{};
    bool operator==(const ProductHexagonTypeImage&) const = default;
};
struct ProductHexagonSwapDeclaration {
    std::vector<ProductHexagonTypeImage> inner, block;
};
struct ProductHexagonDeclaration {
    std::uint64_t key{}, revision{};
    // Original unordered pairs AB, AC, BC; each retains both ordered products.
    std::array<ProductInterchangeDeclaration,3> pairs;
    // ABC, ACB, BAC, BCA, CAB, CBA, each with distinct actual left/right carriers.
    std::array<ProductAssociatorDeclaration,6> associators;
    // Carrier number 2*order + (association == Right). No block becomes an entity.
    std::array<ProductHexagonSwapDeclaration,12> swaps;
};
namespace product_hexagon_detail {
using product_associator_detail::Q;
using product_associator_detail::Guard;
using product_associator_detail::Values;
using product_associator_detail::add;
using product_associator_detail::mul;
using product_associator_detail::grade;
using product_associator_detail::require;
inline constexpr std::array<std::array<std::size_t,3>,6> orders{{{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}}};
inline std::size_t index(ProductPermutation order) {
    const auto i=static_cast<std::size_t>(order); if (i>=orders.size()) throw std::out_of_range("unknown hexagon factor order"); return i;
}
inline std::size_t index(ProductHexagonCarrier c) {
    product_associator_detail::side(c.association); return add(mul(index(c.order),2),c.association == ProductAssociation::Right ? 1u : 0u);
}
inline ProductHexagonCarrier carrier(std::size_t i) {
    if (i>=12) throw std::out_of_range("unknown hexagon carrier");
    return {static_cast<ProductPermutation>(i/2),i%2 ? ProductAssociation::Right : ProductAssociation::Left};
}
inline std::size_t pair_index(std::size_t a,std::size_t b) {
    require(a<3 && b<3 && a!=b,"hexagon pair must name two different original roles");
    if (a>b) std::swap(a,b);
    return a==0 ? b-1 : 2u;
}
inline const RelationalProductDeclaration& pair(const ProductHexagonDeclaration& d,std::size_t a,std::size_t b) {
    const auto& p=d.pairs[pair_index(a,b)]; return a<b ? p.ab : p.ba;
}
inline bool same(const RelationalProductDeclaration& a,const RelationalProductDeclaration& b) {
    return a.key==b.key && a.revision==b.revision && a.types==b.types;
}
inline ProductHexagonCarrier target(ProductHexagonCarrier c,ProductHexagonAction action) {
    (void)index(c); auto order=orders[index(c.order)];
    if (action == ProductHexagonAction::InnerInterchange) {
        const auto p=c.association == ProductAssociation::Left ? 0u : 1u; std::swap(order[p],order[p+1]);
    } else if (action == ProductHexagonAction::BlockInterchange) {
        order=c.association == ProductAssociation::Left ? std::array{order[2],order[0],order[1]} : std::array{order[1],order[2],order[0]};
    } else require(action == ProductHexagonAction::Reassociate,"unknown hexagon action");
    const auto it=std::find(orders.begin(),orders.end(),order); require(it!=orders.end(),"hexagon target order");
    return {static_cast<ProductPermutation>(it-orders.begin()),action == ProductHexagonAction::InnerInterchange ? c.association :
        c.association == ProductAssociation::Left ? ProductAssociation::Right : ProductAssociation::Left};
}
inline std::array<core::CellId,3> global(ProductHexagonCarrier c,const AssociatedCell& cell) {
    (void)index(c); require(product_associator_detail::association(cell)==c.association,"hexagon cell has another association");
    const auto ids=product_associator_detail::factors(cell); std::array<core::CellId,3> out{};
    for (std::size_t i=0;i<3;++i) out[orders[index(c.order)][i]]=ids[i];
    return out;
}
inline AssociatedCell cell(ProductHexagonCarrier c,const std::array<core::CellId,3>& global_ids) {
    (void)index(c); const auto o=orders[index(c.order)]; return product_associator_detail::cell(c.association,{global_ids[o[0]],global_ids[o[1]],global_ids[o[2]]});
}
inline bool flip(ProductHexagonCarrier c,ProductHexagonAction action,const AssociatedCell& cell) {
    (void)target(c,action); const auto ids=product_associator_detail::factors(cell);
    const bool p=ids[0].grade.value%2!=0,q=ids[1].grade.value%2!=0,r=ids[2].grade.value%2!=0;
    if (action == ProductHexagonAction::Reassociate) return false;
    if (action == ProductHexagonAction::InnerInterchange) return c.association == ProductAssociation::Left ? p&&q : q&&r;
    return c.association == ProductAssociation::Left ? (p!=q)&&r : p&&(q!=r);
}
using TypeTuple=std::array<std::optional<core::RelationTypeId>,3>;
using TypeTable=std::map<TypeTuple,std::array<std::uint64_t,12>>;
inline std::size_t words(const std::array<const core::RelationalComplex*,3>& sources,const ProductHexagonDeclaration& d) {
    auto n=std::size_t{1};
    for (std::size_t i=0;i<6;++i) { const auto o=orders[i]; n=add(n,product_associator_detail::words({sources[o[0]],sources[o[1]],sources[o[2]]},d.associators[i])); }
    for (const auto ab : {std::array<std::size_t,2>{0,1},std::array<std::size_t,2>{0,2},std::array<std::size_t,2>{1,2}})
        n=add(n,product_interchange_detail::words({sources[ab[0]],sources[ab[1]]},d.pairs[pair_index(ab[0],ab[1])]));
    for (const auto& s : d.swaps) n=add(n,mul(add(s.inner.size(),s.block.size()),2));
    std::array<std::size_t,3> cells{},flags{};
    for (std::size_t f=0;f<3;++f) for (std::size_t k=0;k<sources[f]->grade_count();++k) {
        const auto& a=sources[f]->grade(grade(k)); cells[f]=add(cells[f],a.cell_count());
        flags[f]=add(flags[f],mul(a.incidence_count(),add(64,mul(8,sources[f]->signature().aspects().size()))));
    }
    n=add(n,mul(128,mul(cells[0],mul(cells[1],cells[2]))));
    for (std::size_t f=0;f<3;++f) n=add(n,mul(flags[f],mul(cells[(f+1)%3],cells[(f+2)%3])));
    return n;
}
struct State {
    ProductHexagonDeclaration declaration;
    std::vector<OwnedProductAssociator> associators;
    std::vector<OwnedProductInterchange> pairs;
    TypeTable types;
    std::array<std::array<std::map<std::uint64_t,std::uint64_t>,2>,12> swaps;
    std::vector<std::array<core::CellId,3>> cells;
    std::size_t words{},envelope{};
};
inline void validate(const State& s,Guard& guard) {
    guard.entries(s.envelope);
    for (const auto& a : s.associators) a.validate(guard);
    for (const auto& p : s.pairs) for (const auto o : {ProductFactorOrder::AB,ProductFactorOrder::BA}) p.product(o).validate(guard);
    for (const auto& t : s.types) { (void)t; guard.step(); }
    for (const auto& s_ : s.declaration.swaps) for (const auto* ts : {&s_.inner,&s_.block}) for (const auto& t : *ts) { (void)t; guard.step(); }
}
inline TypeTable type_table(const std::array<core::RelationalComplex,3>& f,const ProductHexagonDeclaration& d,Guard& guard) {
    std::array<owned_product_detail::Types,12> pairs;
    std::array<product_associator_detail::Types,12> nested;
    for (std::size_t i=0;i<6;++i) {
        const auto& a=d.associators[i];
        for (const auto& t : a.ab.types) { guard.step(); pairs[2*i].emplace(std::pair{t.left,t.right},t.key); }
        for (const auto& t : a.bc.types) { guard.step(); pairs[2*i+1].emplace(std::pair{t.left,t.right},t.key); }
        for (const auto& t : a.left.types) { guard.step(); nested[2*i].emplace(std::pair{t.inner_type,t.factor_type},t.key); }
        for (const auto& t : a.right.types) { guard.step(); nested[2*i+1].emplace(std::pair{t.inner_type,t.factor_type},t.key); }
    }
    TypeTable out;
    const auto token=[](std::size_t i)->std::optional<core::RelationTypeId>{return i ? std::optional{static_cast<core::RelationTypeId>(i-1)} : std::nullopt;};
    for (std::size_t a=0;a<=f[0].signature().type_count();++a) for (std::size_t b=0;b<=f[1].signature().type_count();++b)
        for (std::size_t c=0;c<=f[2].signature().type_count();++c) {
            guard.step(); TypeTuple t{token(a),token(b),token(c)}; std::array<std::uint64_t,12> roots{};
            for (std::size_t i=0;i<6;++i) {
                const auto o=orders[i]; const auto x=t[o[0]],y=t[o[1]],z=t[o[2]];
                roots[2*i]=nested[2*i].at({pairs[2*i].at({x,y}),z}); roots[2*i+1]=nested[2*i+1].at({pairs[2*i+1].at({y,z}),x});
            }
            out.emplace(t,roots);
        }
    return out;
}
} // namespace product_hexagon_detail
struct ProductHexagonCarrierCell {
    ProductHexagonCarrier carrier;
    OwnedProductAssociator owner;
    AssociatedCell cell;
    std::array<ProductFactorIdentity,3> identities;
    std::uint64_t type{};
    std::vector<NestedProductOccurrence> flags;
};
struct ProductHexagonFlag {
    NestedProductOccurrence source,target;
    std::size_t original_factor{},source_word_position{},target_word_position{};
    bool upper_interchange_flip{},lower_interchange_flip{};
    numeric::Rational mapped_boundary_coefficient,image_boundary_coefficient;
};
struct ProductHexagonInnerReceipt {
    RetainedProductCell source_inner,target_inner;
    ProductInterchangeCellWitness interchange;
};
struct ProductHexagonEdgeCertificate {
    ProductHexagonCarrier source,target;
    ProductHexagonAction action;
    OwnedProductAssociator source_owner,target_owner;
    std::vector<ProductHexagonFlag> flags;
    std::optional<ProductHexagonInnerReceipt> inner_receipt;
};
namespace product_hexagon_detail {
struct Certificate { std::vector<ProductHexagonCarrierCell> carriers; std::vector<ProductHexagonEdgeCertificate> edges; };
inline Certificate certificate(const State& s,const std::array<core::CellId,3>& ids,Guard& guard) {
    Certificate out; std::array<ProductFactorIdentity,3> identities; TypeTuple tuple;
    for (std::size_t f=0;f<3;++f) {
        const auto& source=s.associators[0].factor(f); identities[f]=owned_product_detail::identity(source,ids[f]); tuple[f]=owned_product_detail::type(source,ids[f]);
    }
    const auto& types=s.types.at(tuple);
    for (std::size_t i=0;i<6;++i) {
        const auto c=carrier(2*i); const auto left=std::get<LeftAssociatedCell>(cell(c,ids));
        const auto w=s.associators[i].cell_witness(left,guard);
        for (std::size_t side=0;side<2;++side) {
            std::vector<NestedProductOccurrence> flags;
            for (const auto& o : w.flags()) { guard.step(); flags.push_back(side ? o.right : o.left); }
            require(types[2*i+side] == (side ? w.cell().right_type : w.cell().left_type),"hexagon cell type");
            out.carriers.push_back({carrier(2*i+side),w.plan(),cell(carrier(2*i+side),ids),identities,types[2*i+side],std::move(flags)});
        }
    }
    const auto n=out.carriers[0].flags.size(); std::size_t aspects{};
    for (std::size_t f=0;f<3;++f) aspects=std::max(aspects,s.associators[0].factor(f).signature().aspects().size());
    guard.entries(add(s.envelope,mul(n,add(2048,mul(aspects,128)))));
    for (const auto& from : out.carriers) for (const auto action : {ProductHexagonAction::Reassociate,ProductHexagonAction::InnerInterchange,ProductHexagonAction::BlockInterchange}) {
        guard.step(); const auto to=target(from.carrier,action); const auto& dest=out.carriers[index(to)];
        require(dest.flags.size()==n,"hexagon flag population");
        if (action != ProductHexagonAction::Reassociate) require(s.swaps[index(from.carrier)][action == ProductHexagonAction::InnerInterchange ? 0u : 1u].at(from.type)==dest.type,"hexagon edge type");
        std::map<std::pair<std::size_t,std::size_t>,std::size_t> positions;
        for (std::size_t j=0;j<n;++j) { guard.step(); const auto& b=dest.flags[j];
            require(positions.emplace(std::pair{orders[index(to.order)][b.factor],b.attaching_position},j).second,"hexagon original flag position"); }
        ProductHexagonEdgeCertificate edge{from.carrier,to,action,from.owner,dest.owner,{},{}};
        if (action == ProductHexagonAction::InnerInterchange) {
            const auto local=product_associator_detail::factors(from.cell); const auto o=orders[index(from.carrier.order)];
            const auto side=from.carrier.association == ProductAssociation::Left ? 0u : 1u;
            const auto a=o[side],b=o[side+1]; const auto order=a<b ? ProductFactorOrder::AB : ProductFactorOrder::BA;
            const ProductCell pair_cell{local[side],local[side+1]};
            auto receipt=s.pairs[pair_index(a,b)].cell_witness(order,pair_cell,guard);
            const auto source_inner=from.owner.inner_product(side==1).cell(pair_cell);
            const auto target_inner=dest.owner.inner_product(side==1).cell(product_interchange_detail::image(pair_cell));
            require(source_inner.type==receipt.source().type && source_inner.source_identity==receipt.source().source_identity &&
                target_inner.type==receipt.target().type && target_inner.source_identity==receipt.target().source_identity,
                "hexagon inner lift retains its actual nested and interchange carriers");
            for (const auto& word : {std::cref(receipt.source_flags()),std::cref(receipt.target_flags())}) {
                const auto& nested= &word.get()==&receipt.source_flags() ? from : dest;
                for (const auto& flag : word.get()) {
                    guard.step(); const auto f=side+(flag.side==ProductBoundarySide::Right ? 1u : 0u);
                    const auto it=std::find_if(nested.flags.begin(),nested.flags.end(),[&](const auto& x){return x.factor==f && x.attaching_position==flag.attaching_position;});
                    require(it!=nested.flags.end() && it->factor_occurrence==flag.factor_occurrence && it->factor_aspects==flag.factor_aspects &&
                        it->factor_coefficient==flag.factor_coefficient && it->exists==flag.exists && it->inner_koszul_flip==flag.koszul_flip,
                        "hexagon inner lift must use the canonical owning interchange flag");
                }
            }
            edge.inner_receipt=ProductHexagonInnerReceipt{source_inner,target_inner,std::move(receipt)};
        }
        for (std::size_t j=0;j<n;++j) {
            guard.step(); const auto& a=from.flags[j]; const auto original=orders[index(from.carrier.order)][a.factor];
            const auto k=positions.at({original,a.attaching_position}); const auto& b=dest.flags[k];
            require(global(from.carrier,a.upper)==global(to,b.upper) && global(from.carrier,a.lower)==global(to,b.lower) &&
                a.factor_occurrence==b.factor_occurrence && a.factor_aspects==b.factor_aspects && a.factor_coefficient==b.factor_coefficient && a.exists==b.exists,
                "hexagon edge must preserve the original factor flag");
            const auto upper=flip(from.carrier,action,a.upper),lower=flip(from.carrier,action,a.lower);
            auto mapped=lower ? guard.sub(Q{0},a.coefficient) : a.coefficient;
            auto direct=upper ? guard.sub(Q{0},b.coefficient) : b.coefficient;
            require(mapped==direct,"hexagon signed boundary flag identity");
            edge.flags.push_back({a,b,original,j,k,upper,lower,std::move(mapped),std::move(direct)});
        }
        out.edges.push_back(std::move(edge));
    }
    return out;
}
} // namespace product_hexagon_detail
class ProductHexagonCellWitness;
class ProductHexagonChainWitness;
struct ProductHexagonEdgeWitness;
class OwnedProductHexagon {
public:
    using Values=product_hexagon_detail::Values;
    static OwnedProductHexagon prepare(std::array<core::RelationalComplex,3> factors,ProductHexagonDeclaration input,product_hexagon_detail::Guard& guard) {
        namespace detail=product_hexagon_detail; auto d=input;
        const auto words=detail::words({&factors[0],&factors[1],&factors[2]},d),envelope=detail::add(1,detail::mul(words,16));
        guard.entries(detail::mul(envelope,2)); detail::require(d.key && d.revision,"hexagon requires an identity and revision");
        std::vector<OwnedProductInterchange> pairs;
        for (const auto ab : {std::array<std::size_t,2>{0,1},std::array<std::size_t,2>{0,2},std::array<std::size_t,2>{1,2}})
            pairs.push_back(OwnedProductInterchange::prepare({factors[ab[0]],factors[ab[1]]},d.pairs[detail::pair_index(ab[0],ab[1])],guard));
        std::vector<OwnedProductAssociator> associators;
        for (std::size_t i=0;i<6;++i) {
            guard.step(); const auto o=detail::orders[i]; const auto& a=d.associators[i];
            detail::require(detail::same(a.ab,detail::pair(d,o[0],o[1])) && detail::same(a.bc,detail::pair(d,o[1],o[2])),
                "hexagon associators require the complete corresponding pair declarations");
            associators.push_back(OwnedProductAssociator::prepare({factors[o[0]],factors[o[1]],factors[o[2]]},a,guard));
        }
        auto types=detail::type_table(factors,d,guard);
        std::array<std::array<std::map<std::uint64_t,std::uint64_t>,2>,12> swaps;
        for (std::size_t i=0;i<12;++i) for (std::size_t k=0;k<2;++k) {
            const auto& declared=k ? d.swaps[i].block : d.swaps[i].inner;
            detail::require(declared.size()==types.size(),"hexagon swaps need complete unused type directions"); std::set<std::uint64_t> targets;
            for (const auto& t : declared) { guard.step(); detail::require(swaps[i][k].emplace(t.source,t.target).second && targets.insert(t.target).second,"hexagon swap type bijection"); }
            const auto j=detail::index(detail::target(detail::carrier(i),k ? ProductHexagonAction::BlockInterchange : ProductHexagonAction::InnerInterchange));
            for (const auto& [tuple,roots] : types) { (void)tuple; guard.step();
                detail::require(swaps[i][k].contains(roots[i]) && swaps[i][k].at(roots[i])==roots[j],"hexagon swap changes an original constructor triple"); }
        }
        std::vector<std::array<core::CellId,3>> cells;
        for (const auto& c : associators[0].cells()) { guard.step(); cells.push_back({c.left.inner.left,c.left.inner.right,c.left.outer}); }
        auto s=std::make_shared<const detail::State>(detail::State{std::move(d),std::move(associators),std::move(pairs),std::move(types),std::move(swaps),std::move(cells),words,envelope});
        detail::validate(*s,guard); for (const auto& ids : s->cells) { guard.step(); (void)detail::certificate(*s,ids,guard); }
        return OwnedProductHexagon(std::move(s));
    }
    bool same_owner(const OwnedProductHexagon& other) const noexcept { return state_==other.state_; }
    const ProductHexagonDeclaration& declaration() const & { return state_->declaration; }
    const ProductHexagonDeclaration& declaration() const && = delete;
    const std::vector<std::array<core::CellId,3>>& cells() const & { return state_->cells; }
    const std::vector<std::array<core::CellId,3>>& cells() const && = delete;
    OwnedProductAssociator associator(ProductPermutation p) const { return state_->associators.at(product_hexagon_detail::index(p)); }
    OwnedProductInterchange pair(std::size_t a,std::size_t b) const { return state_->pairs.at(product_hexagon_detail::pair_index(a,b)); }
    const core::RelationalComplex& factor(std::size_t f) const & { return state_->associators[0].factor(f); }
    const core::RelationalComplex& factor(std::size_t) const && = delete;
    core::Grade max_grade() const noexcept { return state_->associators[0].max_grade(); }
    std::size_t retained_words() const noexcept { return state_->words; }
    std::size_t entry_envelope() const noexcept { return state_->envelope; }
    ProductHexagonCarrier carrier(const OwnedAssociatedProductField& f) const {
        for (std::size_t i=0;i<6;++i) if (f.plan().same_owner(state_->associators[i])) return {static_cast<ProductPermutation>(i),f.association()};
        throw std::invalid_argument("hexagon requires its actual owned associated field");
    }
    OwnedAssociatedProductField field(ProductHexagonCarrier c,core::Grade g,Values input,product_hexagon_detail::Guard& guard) const {
        const auto pin=state_; auto frozen=input; (void)product_hexagon_detail::index(c); product_hexagon_detail::validate(*pin,guard);
        return pin->associators[product_hexagon_detail::index(c.order)].field(c.association,g,std::move(frozen),guard);
    }
    OwnedAssociatedProductField boundary(OwnedAssociatedProductField input,product_hexagon_detail::Guard& guard) const {
        const auto pin=state_; const OwnedProductHexagon owner(pin); const auto c=owner.carrier(input); product_hexagon_detail::validate(*pin,guard);
        return pin->associators[product_hexagon_detail::index(c.order)].boundary(std::move(input),guard);
    }
    OwnedAssociatedProductField step(OwnedAssociatedProductField input,ProductHexagonAction action,product_hexagon_detail::Guard& guard) const {
        namespace detail=product_hexagon_detail; const auto pin=state_; const OwnedProductHexagon owner(pin); const auto from=owner.carrier(input),to=detail::target(from,action);
        detail::validate(*pin,guard); guard.entries(detail::add(pin->envelope,detail::mul(input.values().size(),16)));
        if (action == ProductHexagonAction::Reassociate) return pin->associators[detail::index(from.order)].reassociate(std::move(input),guard);
        Values out;
        for (const auto& [c,q] : input.values()) { guard.step(); guard.value(q);
            out.emplace(detail::cell(to,detail::global(from,c)),detail::flip(from,action,c) ? guard.sub(detail::Q{0},q) : q); }
        return pin->associators[detail::index(to.order)].field(to.association,input.grade(),std::move(out),guard);
    }
    ProductHexagonCellWitness cell_witness(std::array<core::CellId,3>,product_hexagon_detail::Guard&) const;
    ProductHexagonEdgeWitness edge_witness(OwnedAssociatedProductField,ProductHexagonAction,product_hexagon_detail::Guard&) const;
    ProductHexagonChainWitness witness(OwnedAssociatedProductField,product_hexagon_detail::Guard&) const;
private:
    explicit OwnedProductHexagon(std::shared_ptr<const product_hexagon_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const product_hexagon_detail::State> state_;
};
class ProductHexagonCellWitness {
public:
    OwnedProductHexagon plan() const { return plan_; }
    const std::vector<ProductHexagonCarrierCell>& carriers() const & { return data_.carriers; }
    const std::vector<ProductHexagonCarrierCell>& carriers() const && = delete;
    const std::vector<ProductHexagonEdgeCertificate>& edges() const & { return data_.edges; }
    const std::vector<ProductHexagonEdgeCertificate>& edges() const && = delete;
private:
    friend class OwnedProductHexagon;
    ProductHexagonCellWitness(OwnedProductHexagon p,product_hexagon_detail::Certificate data) : plan_(std::move(p)),data_(std::move(data)) {}
    OwnedProductHexagon plan_;
    product_hexagon_detail::Certificate data_;
};
struct ProductHexagonEdgeWitness {
    ProductHexagonAction action;
    OwnedAssociatedProductField input,image,round_trip;
    std::optional<OwnedAssociatedProductField> mapped_boundary,image_boundary;
};
struct ProductHexagonPathWitness {
    std::vector<OwnedAssociatedProductField> long_path;
    ProductHexagonEdgeWitness direct;
    std::vector<ProductHexagonEdgeWitness> edges;
};
class ProductHexagonChainWitness {
public:
    OwnedProductHexagon plan() const { return plan_; }
    const std::array<ProductHexagonPathWitness,2>& hexagons() const & { return hexagons_; }
    const std::array<ProductHexagonPathWitness,2>& hexagons() const && = delete;
private:
    friend class OwnedProductHexagon;
    ProductHexagonChainWitness(OwnedProductHexagon p,std::array<ProductHexagonPathWitness,2> h) : plan_(std::move(p)),hexagons_(std::move(h)) {}
    OwnedProductHexagon plan_;
    std::array<ProductHexagonPathWitness,2> hexagons_;
};
inline ProductHexagonCellWitness OwnedProductHexagon::cell_witness(std::array<core::CellId,3> ids,product_hexagon_detail::Guard& guard) const {
    const auto pin=state_; product_hexagon_detail::validate(*pin,guard);
    return ProductHexagonCellWitness(OwnedProductHexagon(pin),product_hexagon_detail::certificate(*pin,ids,guard));
}
inline ProductHexagonEdgeWitness OwnedProductHexagon::edge_witness(OwnedAssociatedProductField input,ProductHexagonAction action,product_hexagon_detail::Guard& guard) const {
    const auto pin=state_; const OwnedProductHexagon owner(pin); auto image=owner.step(input,action,guard),back=owner.step(image,action,guard);
    product_hexagon_detail::require(back.plan().same_owner(input.plan()) && back.association()==input.association() && back.values()==input.values(),"hexagon edge round trip");
    std::optional<OwnedAssociatedProductField> before,after;
    if (input.grade().value) { before=owner.step(owner.boundary(input,guard),action,guard); after=owner.boundary(image,guard);
        product_hexagon_detail::require(before->plan().same_owner(after->plan()) && before->association()==after->association() && before->values()==after->values(),"hexagon edge boundary naturality"); }
    return {action,std::move(input),std::move(image),std::move(back),std::move(before),std::move(after)};
}
inline ProductHexagonChainWitness OwnedProductHexagon::witness(OwnedAssociatedProductField input,product_hexagon_detail::Guard& guard) const {
    const auto pin=state_; const OwnedProductHexagon owner(pin); (void)owner.carrier(input);
    const auto path=[&](OwnedAssociatedProductField source) {
        ProductHexagonPathWitness out{{source},owner.edge_witness(source,ProductHexagonAction::BlockInterchange,guard),{}};
        for (const auto action : {ProductHexagonAction::Reassociate,ProductHexagonAction::InnerInterchange,ProductHexagonAction::Reassociate,
            ProductHexagonAction::InnerInterchange,ProductHexagonAction::Reassociate}) {
            auto w=owner.edge_witness(out.long_path.back(),action,guard); out.long_path.push_back(w.image); out.edges.push_back(std::move(w));
        }
        product_hexagon_detail::require(owner.carrier(out.long_path.back())==owner.carrier(out.direct.image) && out.long_path.back().values()==out.direct.image.values(),"hexagon route equality");
        return out;
    };
    auto first=path(input),second=path(owner.step(input,ProductHexagonAction::Reassociate,guard));
    return ProductHexagonChainWitness(owner,{std::move(first),std::move(second)});
}
} // namespace rfc::algebra
