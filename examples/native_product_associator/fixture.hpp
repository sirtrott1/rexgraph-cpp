#pragma once
#include "rfc/inference/native_product_associator.hpp"
#include "../native_tensor_product/fixture.hpp"

namespace native_associator_example {
using Q = rfc::numeric::Rational;
using Plan = rfc::inference::PreparedNativeProductAssociator;
using Numeric = rfc::algebra::OwnedProductAssociator;
using Association = rfc::algebra::ProductAssociation;
using Cell = rfc::algebra::AssociatedCell;
using Left = rfc::algebra::LeftAssociatedCell;
using Right = rfc::algebra::RightAssociatedCell;
using Guard = rfc::numeric::ExactActionGuard;
inline rfc::algebra::ProductAssociatorDeclaration declaration(const std::array<rfc::core::RelationalComplex, 3>& factors) {
    using namespace rfc;
    algebra::ProductAssociatorDeclaration d{1200, 4,
        native_product_example::declaration(factors[0], factors[1]), native_product_example::declaration(factors[1], factors[2]),
        {1201, 7, {}}, {1202, 8, {}}, {}};
    d.ab.key = 1203; d.bc.key = 1204;
    for (const auto& t : d.ab.types) for (std::size_t k = 0; k <= factors[2].signature().type_count(); ++k)
        d.left.types.push_back({t.key, k ? std::optional{static_cast<core::RelationTypeId>(k-1)} : std::nullopt, 2000+d.left.types.size()});
    for (std::size_t k = 0; k <= factors[0].signature().type_count(); ++k) for (const auto& t : d.bc.types)
        d.right.types.push_back({t.key, k ? std::optional{static_cast<core::RelationTypeId>(k-1)} : std::nullopt, 3000+d.right.types.size()});
    for (const auto& x : d.ab.types) for (const auto& y : d.bc.types) if (x.right == y.left) {
        const auto l = std::find_if(d.left.types.begin(), d.left.types.end(), [&](const auto& t){return t.inner_type == x.key && t.factor_type == y.right;});
        const auto r = std::find_if(d.right.types.begin(), d.right.types.end(), [&](const auto& t){return t.inner_type == y.key && t.factor_type == x.left;});
        d.types.push_back({l->key, r->key});
    }
    return d;
}
inline rfc::inference::NativeProductAssociatorRequest request(const rfc::storage::NativeAccessionDirectory& a,
    const rfc::storage::NativeAccessionDirectory& b, const rfc::storage::NativeAccessionDirectory& c) {
    const std::array pins{native_product_example::anchor(a), native_product_example::anchor(b), native_product_example::anchor(c)};
    return {pins, {41,42,43}, declaration({pins[0].endpoint()->source, pins[1].endpoint()->source, pins[2].endpoint()->source}), 128};
}
inline constexpr rfc::inference::NativeProductAssociatorAvailability available{{{910,7}, {911,11}, {912,19}}};
inline std::array<rfc::core::RelationalComplex, 3> sources(unsigned sample) {
    if (sample < 9) {
        const std::array<Q, 3> scales{Q{1}, Q{2,3}, Q{5,2}};
        return {native_product_example::interval(scales[sample/3], true), native_product_example::interval(scales[sample%3], true),
            native_product_example::interval(Q{3,7}, true)};
    }
    if (sample == 9) return {native_product_example::unit(false,2), native_product_example::interval(Q{2,3}, true), native_product_example::unit()};
    if (sample == 10) return {native_product_example::interval(Q{2,3}, true), native_product_example::unit(true,2), native_product_example::interval(Q{1}, true)};
    rfc::core::RelationSignature signature; const auto t = signature.add_type("empty-assertion");
    (void)signature.add_type("unused-empty-type"); rfc::core::RelationalComplexBuilder builder(signature);
    builder.declare_witness_identity(10); builder.add_grade_one(rfc::core::RelationKey{100}, t, {}); builder.declare_grade(rfc::core::Grade{2});
    return {builder.freeze(), native_product_example::interval(Q{2,3}, true), native_product_example::unit(false,1)};
}
inline Plan plan(unsigned sample = 0) {
    const auto f = sources(sample);
    rfc::storage::NativeAccessionDirectory a(rfc::storage::TemporalEndpoint(f[0],7),910),
        b(rfc::storage::TemporalEndpoint(f[1],11),911), c(rfc::storage::TemporalEndpoint(f[2],19),912);
    return Plan::prepare(request(a,b,c), available);
}
} // namespace native_associator_example
