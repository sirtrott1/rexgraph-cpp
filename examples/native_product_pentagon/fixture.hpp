#pragma once
#include "rfc/inference/native_product_pentagon.hpp"
#include "../native_product_associator/fixture.hpp"

namespace native_pentagon_example {
using Q = rfc::numeric::Rational;
using Plan = rfc::inference::PreparedNativeProductPentagon;
using Numeric = rfc::algebra::OwnedProductPentagon;
using Vertex = rfc::algebra::ProductParenthesization;
using Cell = rfc::algebra::PentagonCell;
using Guard = rfc::numeric::ExactActionGuard;
inline rfc::algebra::ProductPentagonDeclaration declaration(const std::array<rfc::core::RelationalComplex,4>& f) {
    using namespace rfc; algebra::ProductPentagonDeclaration d;
    d.key = 2400; d.revision = 9;
    d.abc = native_associator_example::declaration({f[0],f[1],f[2]});
    d.bcd = native_associator_example::declaration({f[1],f[2],f[3]});
    d.bcd.key = 2401; d.bcd.ab = d.abc.bc;
    const std::array<const algebra::NestedProductDeclaration*,4> inner{&d.abc.left,&d.abc.right,&d.bcd.left,&d.bcd.right};
    for (std::size_t v = 0; v < 4; ++v) {
        auto& outer = d.nested[v]; outer.key = 2410+v; outer.revision = 7+v;
        for (const auto& t : inner[v]->types) for (std::size_t k = 0; k <= f[v < 2 ? 3 : 0].signature().type_count(); ++k)
            outer.types.push_back({t.key,k ? std::optional{static_cast<core::RelationTypeId>(k-1)} : std::nullopt,10000+v*200+outer.types.size()});
    }
    d.middle = {2420,11,{}};
    for (const auto& ab : d.abc.ab.types) for (const auto& cd : d.bcd.bc.types)
        d.middle.types.push_back({ab.key,cd.key,10800+d.middle.types.size()});
    algebra::product_pentagon_detail::Tokens tokens{};
    const auto visit = [&](auto&& self, std::size_t j) -> void {
        if (j < 4) {
            for (std::size_t k = 0; k <= f[j].signature().type_count(); ++k) {
                tokens[j] = k ? std::optional{static_cast<core::RelationTypeId>(k-1)} : std::nullopt; self(self,j+1);
            } return;
        }
        const auto word = algebra::product_pentagon_detail::type_word(d,tokens);
        for (std::size_t e = 0; e < 5; ++e) { const auto [a,b] = algebra::product_pentagon_detail::endpoints[e]; d.edges[e].push_back({word[a],word[b]}); }
    }; visit(visit,0); return d;
}
inline constexpr rfc::inference::NativeProductPentagonAvailability available{{{910,7},{911,11},{912,19},{913,23}}};
inline std::array<rfc::core::RelationalComplex,4> sources(unsigned sample) {
    if (sample >= 12) throw std::out_of_range("pentagon example source sample");
    const std::array<Q,4> scales{Q{1},Q{2,3},Q{5,2},Q{3,7}};
    std::array<rfc::core::RelationalComplex,4> out{
        native_product_example::interval(scales[sample%4],true), native_product_example::interval(scales[(sample+1)%4],true),
        native_product_example::interval(scales[(sample+2)%4],true), native_product_example::interval(scales[(sample+3)%4],true)};
    if (sample >= 4 && sample < 8) out[sample-4] = native_product_example::unit(true,2);
    if (sample >= 8) out[sample-8] = native_product_example::unit(false,2);
    return out;
}
inline rfc::inference::NativeProductPentagonRequest request(const std::array<rfc::storage::PinnedNativeAccession,4>& pins) {
    return {pins,{41,42,43,44},declaration({pins[0].endpoint()->source,pins[1].endpoint()->source,pins[2].endpoint()->source,pins[3].endpoint()->source}),128};
}
inline Plan plan(unsigned sample = 0) {
    const auto f = sources(sample);
    rfc::storage::NativeAccessionDirectory a(rfc::storage::TemporalEndpoint(f[0],7),910), b(rfc::storage::TemporalEndpoint(f[1],11),911),
        c(rfc::storage::TemporalEndpoint(f[2],19),912), d(rfc::storage::TemporalEndpoint(f[3],23),913);
    return Plan::prepare(request({native_product_example::anchor(a),native_product_example::anchor(b),native_product_example::anchor(c),native_product_example::anchor(d)}),available);
}
} // namespace native_pentagon_example
