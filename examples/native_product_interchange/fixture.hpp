#pragma once
#include "rfc/inference/native_product_interchange.hpp"
#include "../native_tensor_product/fixture.hpp"

namespace native_interchange_example {
using Q = rfc::numeric::Rational;
using Plan = rfc::inference::PreparedNativeProductInterchange;
using Numeric = rfc::algebra::OwnedProductInterchange;
using Order = rfc::algebra::ProductFactorOrder;
using Cell = rfc::algebra::ProductCell;
using Guard = rfc::numeric::ExactActionGuard;
inline rfc::algebra::ProductInterchangeDeclaration declaration(const std::array<rfc::core::RelationalComplex,2>& factors) {
    rfc::algebra::ProductInterchangeDeclaration d{1400,5,
        native_product_example::declaration(factors[0],factors[1]),native_product_example::declaration(factors[1],factors[0]),{}};
    d.ab.key=1401; d.ba.key=1402;
    for (auto& t : d.ba.types) t.key += 200;
    for (const auto& a : d.ab.types) for (const auto& b : d.ba.types)
        if (a.left == b.right && a.right == b.left) d.types.push_back({a.key,b.key});
    return d;
}
inline rfc::inference::NativeProductInterchangeRequest request(const std::array<rfc::storage::PinnedNativeAccession,2>& pins) {
    return {pins,{41,42},declaration({pins[0].endpoint()->source,pins[1].endpoint()->source}),128};
}
inline constexpr rfc::inference::NativeProductInterchangeAvailability available{{{910,7},{911,11}}};
inline rfc::core::RelationalComplex tower(Q weight) {
    using namespace rfc;
    core::RelationSignature sig; const auto tag=sig.aspects().add("tag",composite::AspectRole::Provenance,false);
    const auto t=sig.add_type("exact-tower"); (void)sig.add_type("unused-tower-a"); (void)sig.add_type("unused-tower-b");
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(core::RelationKey{100},t,{{10,true,true,weight,{{tag,true}},core::OccurrenceKey{101}},
        {20,true,false,weight,{{tag,false}},core::OccurrenceKey{102}}});
    for (std::uint16_t g=2;g<=4;++g) {
        std::vector<core::HigherIncidenceInput> word;
        if (g == 3) word.push_back({0,true,false,weight,{{tag,true}},core::OccurrenceKey{301}});
        word.push_back({0,true,false,Q{2},{{tag,true}},core::OccurrenceKey{static_cast<std::uint64_t>(100*g+2)}});
        word.push_back({0,true,true,Q{2},{{tag,false}},core::OccurrenceKey{static_cast<std::uint64_t>(100*g+3)}});
        word.push_back({0,false,true,Q{0},{{tag,true}},core::OccurrenceKey{static_cast<std::uint64_t>(100*g+4)}});
        word.push_back({0,true,false,Q{0},{{tag,false}},core::OccurrenceKey{static_cast<std::uint64_t>(100*g+5)}});
        b.add_higher({g},core::RelationKey{100},t,word);
    }
    return b.freeze();
}
inline std::array<rfc::core::RelationalComplex,2> sources(unsigned sample) {
    if (sample < 9) {
        const std::array<Q,3> scales{Q{1},Q{2,3},Q{3,7}};
        return {native_product_example::interval(scales[sample/3],true),native_product_example::interval(scales[sample%3],true)};
    }
    if (sample == 9) return {native_product_example::unit(false,3),native_product_example::interval(Q{5,2},true)};
    if (sample == 10) return {native_product_example::interval(Q{2,3},true),native_product_example::unit(true,3)};
    if (sample == 11) return {native_product_example::unit(true,3),native_product_example::unit(false,2)};
    if (sample == 12) return {native_product_example::interval(Q{5,2},true,3),native_product_example::interval(Q{3,7},true)};
    if (sample == 14) return {tower(Q{3,7}),native_product_example::interval(Q{2,3},true)};
    if (sample == 15) return {tower(Q{2,3}),tower(Q{5,2})};
    rfc::core::RelationSignature signature; const auto t=signature.add_type("empty-assertion");
    (void)signature.add_type("unused-empty-type"); rfc::core::RelationalComplexBuilder builder(signature);
    builder.declare_witness_identity(10); builder.add_grade_one(rfc::core::RelationKey{100},t,{}); builder.declare_grade({3});
    return {builder.freeze(),native_product_example::interval(Q{2,3},true)};
}
inline Plan plan(unsigned sample=0) {
    const auto f=sources(sample);
    rfc::storage::NativeAccessionDirectory a(rfc::storage::TemporalEndpoint(f[0],7),910),b(rfc::storage::TemporalEndpoint(f[1],11),911);
    return Plan::prepare(request({native_product_example::anchor(a),native_product_example::anchor(b)}),available);
}
} // namespace native_interchange_example
