#pragma once
#include "rfc/inference/native_product_map_associativity.hpp"
#include "../native_product_map_composition/fixture.hpp"

namespace native_associativity_example {
using namespace native_composition_example;
using Associativity = rfc::inference::PreparedNativeProductMapAssociativity;
using PureAssociativity = rfc::algebra::PreparedProductMapAssociativity;
inline constexpr rfc::inference::NativeProductAssociativityAvailability available_associativity{
    available_composition.source, available_composition.intermediate, available_composition.target,
    {{{940,47}, {941,53}, {942,59}}}};
inline constexpr rfc::algebra::ProductMapAssociativityDeclaration associativity_declaration{4100,5,4200,4300,4400};
inline rfc::runtime::ExecutionHostPolicy associativity_policy() {
    auto p = composition_policy(); p.name = "three-map-associativity-example";
    p.limits.records = 16000000;
    p.limits.rational_slots = p.limits.sparse_entries = 8000000000ULL; return p;
}
inline rfc::runtime::ExecutionLease associativity_lease() { return rfc::runtime::ExecutionLease(associativity_policy()); }
inline rfc::numeric::ExactActionLimits associativity_limits() {
    auto p = composition_limits(); p.entries = 8000000000ULL; return p;
}
struct Fixture {
    native_composition_example::Fixture prefix;
    std::array<rfc::core::RelationalComplex, 3> target;
    Declaration last;
};
inline Fixture fixture(unsigned sample = 0) {
    auto p = native_composition_example::fixture(sample);
    Fixture out{p, p.target, {}};
    if (sample == 0) out.target[0] = native_map_example::subdivision();
    if (sample == 1) out.target = {native_product_example::interval(Q{7,5},true),
        native_product_example::interval(Q{11,3},true), native_product_example::interval(Q{13,7},true)};
    if (sample == 2 || sample == 6 || sample == 7) out.target[0] = points(2,2);
    if (sample == 3) out.target[0] = points(1,1);
    if (sample == 4) out.target[0] = points(2,3);
    if (sample == 5) out.target[0] = native_product_example::unit(true,5);
    out.last = identity_between(p.target,out.target,4500);
    if (sample == 0) out.last.factors[0] = native_map_example::fixture(0).maps.factors[0];
    if (sample == 1) {
        const std::array<Q,3> from{Q{3,7},Q{2,3},Q{5,2}}, to{Q{7,5},Q{11,3},Q{13,7}};
        for (std::size_t f = 0; f < 3; ++f) out.last.factors[f] = FactorMap::nonzero_support(
            native_product_example::factor_map(from[f],to[f],Q{2,7},Q{3,5},Q{-5,11},Q{7,3}));
    }
    if (sample == 2 || sample == 4 || sample == 6 || sample == 7) {
        out.last.factors[0].matrices[0] = Matrix::from_rows({{sample == 7 ? Q{0} : Q{2}}, {Q{-3}}});
        out.last.factors[0].support[0] = {{0,0},{1,0}};
    }
    if (sample == 3) out.last.factors[0].matrices[0] = Matrix::from_rows({{Q{1},Q{1}}});
    if (sample == 3) out.last.factors[0].support[0] = {{0,0},{0,1}};
    if (sample == 8) {
        out.last.factors[0].matrices[0] = Matrix::from_rows({{Q{2},Q{-1}}, {Q{3},Q{1}}});
        out.last.factors[0].support[0] = {{0,0},{0,1},{1,0},{1,1}};
        for (std::size_t f = 1; f < 3; ++f) {
            const auto scale = f == 1 ? Q{2,3} : Q{5,7};
            out.last.factors[f] = FactorMap::nonzero_support(native_product_example::factor_map(scale,scale,Q{2},Q{-1},Q{3},Q{4}));
        }
    }
    return out;
}
struct Plans { Composition prefix; Map last; Associativity associativity; };
inline Plan associativity_endpoint_plan(const std::array<rfc::core::RelationalComplex,3>& factors,
    rfc::inference::NativeProductHexagonAvailability a) {
    rfc::storage::NativeAccessionLimits limits; limits.max_action_work = 64000000;
    return endpoint_plan(factors,a,limits);
}
inline Plans plans(unsigned sample = 0) {
    auto f = fixture(sample); const auto a = available_associativity;
    auto x = associativity_endpoint_plan(f.prefix.source,a[0]), y = associativity_endpoint_plan(f.prefix.intermediate,a[1]),
        z = associativity_endpoint_plan(f.prefix.target,a[2]), w = associativity_endpoint_plan(f.target,a[3]);
    auto first = Map::prepare(x,y,std::move(f.prefix.first),{a[0],a[1]},associativity_lease());
    auto second = Map::prepare(y,z,std::move(f.prefix.second),{a[1],a[2]},associativity_lease());
    auto prefix = Composition::prepare(first,second,3300,4,{a[0],a[1],a[2]},associativity_lease());
    auto last = Map::prepare(z,w,std::move(f.last),{a[2],a[3]},associativity_lease());
    auto p = Associativity::prepare(prefix,last,associativity_declaration,a,associativity_lease());
    return {std::move(prefix),std::move(last),std::move(p)};
}
} // namespace native_associativity_example
