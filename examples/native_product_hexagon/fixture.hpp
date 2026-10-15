#pragma once
#include "rfc/inference/native_product_hexagon.hpp"
#include "../native_product_interchange/fixture.hpp"

namespace native_hexagon_example {
using Q = rfc::numeric::Rational;
using Plan = rfc::inference::PreparedNativeProductHexagon;
using Numeric = rfc::algebra::OwnedProductHexagon;
using Association = rfc::algebra::ProductAssociation;
using Permutation = rfc::algebra::ProductPermutation;
using Carrier = rfc::algebra::ProductHexagonCarrier;
using Action = rfc::algebra::ProductHexagonAction;
using Cell = rfc::algebra::AssociatedCell;
using Guard = rfc::numeric::ExactActionGuard;
inline constexpr std::array<std::array<std::size_t,3>,6> orders{{{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}}};
inline Carrier carrier(std::size_t i) {
    return {static_cast<Permutation>(i/2),i%2 ? Association::Right : Association::Left};
}
inline Cell cell(Carrier c,const std::array<rfc::core::CellId,3>& ids) {
    const auto o=orders.at(static_cast<std::size_t>(c.order));
    if (c.association == Association::Left) return rfc::algebra::LeftAssociatedCell{{ids[o[0]],ids[o[1]]},ids[o[2]]};
    return rfc::algebra::RightAssociatedCell{ids[o[0]],{ids[o[1]],ids[o[2]]}};
}
inline std::size_t target(std::size_t i,Action action) {
    auto o=orders.at(i/2);
    if (action == Action::InnerInterchange) std::swap(o[i%2 ? 1u : 0u],o[i%2 ? 2u : 1u]);
    if (action == Action::BlockInterchange) std::rotate(o.begin(),o.begin()+(i%2 ? 1 : 2),o.end());
    const auto p=static_cast<std::size_t>(std::find(orders.begin(),orders.end(),o)-orders.begin());
    return 2*p+(action == Action::InnerInterchange ? i%2 : 1-i%2);
}
inline rfc::algebra::ProductHexagonDeclaration declaration(const std::array<rfc::core::RelationalComplex,3>& factors) {
    using namespace rfc;
    algebra::ProductHexagonDeclaration d; d.key=1600; d.revision=6;
    const std::array<std::array<std::size_t,2>,3> original_pairs{{{0,1},{0,2},{1,2}}};
    for (std::size_t i=0;i<3;++i) {
        const auto roles=original_pairs[i]; d.pairs[i]=native_interchange_example::declaration({factors[roles[0]],factors[roles[1]]});
        d.pairs[i].key=1700+10*i; d.pairs[i].ab.key=1701+10*i; d.pairs[i].ba.key=1702+10*i;
    }
    const auto pair=[&](std::size_t a,std::size_t b)->const algebra::RelationalProductDeclaration& {
        for (std::size_t i=0;i<3;++i) {
            if (original_pairs[i] == std::array{a,b}) return d.pairs[i].ab;
            if (original_pairs[i] == std::array{b,a}) return d.pairs[i].ba;
        }
        throw std::logic_error("fixture pair");
    };
    for (std::size_t i=0;i<6;++i) {
        auto& a=d.associators[i]; const auto o=orders[i];
        a.key=1800+10*i; a.revision=4; a.ab=pair(o[0],o[1]); a.bc=pair(o[1],o[2]);
        a.left={1801+10*i,7,{}}; a.right={1802+10*i,8,{}};
        const auto token=[](std::size_t j)->std::optional<core::RelationTypeId>{return j ? std::optional{static_cast<core::RelationTypeId>(j-1)} : std::nullopt;};
        for (const auto& t : a.ab.types) for (std::size_t j=0;j<=factors[o[2]].signature().type_count();++j)
            a.left.types.push_back({t.key,token(j),10000+2000*i+a.left.types.size()});
        for (std::size_t j=0;j<=factors[o[0]].signature().type_count();++j) for (const auto& t : a.bc.types)
            a.right.types.push_back({t.key,token(j),11000+2000*i+a.right.types.size()});
        for (const auto& x : a.ab.types) for (const auto& y : a.bc.types) if (x.right == y.left) {
            const auto l=std::find_if(a.left.types.begin(),a.left.types.end(),[&](const auto& t){return t.inner_type == x.key && t.factor_type == y.right;});
            const auto r=std::find_if(a.right.types.begin(),a.right.types.end(),[&](const auto& t){return t.inner_type == y.key && t.factor_type == x.left;});
            a.types.push_back({l->key,r->key});
        }
    }
    const auto root=[&](std::size_t i,const std::array<std::optional<core::RelationTypeId>,3>& types) {
        const auto o=orders[i/2]; const auto& a=d.associators[i/2]; const auto& p=i%2 ? a.bc : a.ab;
        const auto left=types[o[i%2 ? 1u : 0u]],right=types[o[i%2 ? 2u : 1u]],outer=types[o[i%2 ? 0u : 2u]];
        const auto inner=std::find_if(p.types.begin(),p.types.end(),[&](const auto& t){return t.left == left && t.right == right;});
        const auto& nested=i%2 ? a.right.types : a.left.types;
        return std::find_if(nested.begin(),nested.end(),[&](const auto& t){return t.inner_type == inner->key && t.factor_type == outer;})->key;
    };
    for (std::size_t a=0;a<=factors[0].signature().type_count();++a) for (std::size_t b=0;b<=factors[1].signature().type_count();++b)
        for (std::size_t c=0;c<=factors[2].signature().type_count();++c) {
            const auto token=[](std::size_t i)->std::optional<core::RelationTypeId>{return i ? std::optional{static_cast<core::RelationTypeId>(i-1)} : std::nullopt;};
            const std::array types{token(a),token(b),token(c)};
            for (std::size_t i=0;i<12;++i) {
                d.swaps[i].inner.push_back({root(i,types),root(target(i,Action::InnerInterchange),types)});
                d.swaps[i].block.push_back({root(i,types),root(target(i,Action::BlockInterchange),types)});
            }
        }
    return d;
}
inline constexpr rfc::inference::NativeProductHexagonAvailability available{{{910,7},{911,11},{912,19}}};
inline rfc::inference::NativeProductHexagonRequest request(const std::array<rfc::storage::PinnedNativeAccession,3>& pins) {
    return {pins,{41,42,43},declaration({pins[0].endpoint()->source,pins[1].endpoint()->source,pins[2].endpoint()->source}),128};
}
inline std::array<rfc::core::RelationalComplex,3> sources(unsigned sample) {
    const auto pair=native_interchange_example::sources(sample);
    if (sample == 9 || sample == 11 || sample == 15) return {pair[0],pair[1],native_product_example::unit(false,2)};
    return {pair[0],pair[1],native_product_example::interval(Q{3,7},true)};
}
inline Plan plan(unsigned sample=0) {
    const auto f=sources(sample);
    rfc::storage::NativeAccessionDirectory a(rfc::storage::TemporalEndpoint(f[0],7),910),b(rfc::storage::TemporalEndpoint(f[1],11),911),
        c(rfc::storage::TemporalEndpoint(f[2],19),912);
    return Plan::prepare(request({native_product_example::anchor(a),native_product_example::anchor(b),native_product_example::anchor(c)}),available);
}
} // namespace native_hexagon_example
