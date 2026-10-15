#pragma once
#include "rfc/inference/native_product_chain_map.hpp"
#include "../native_product_hexagon/fixture.hpp"

namespace native_map_example {
using namespace native_hexagon_example;
using Matrix = rfc::numeric::ExactMatrix;
using Map = rfc::inference::PreparedNativeProductChainMap;
using Declaration = rfc::algebra::ProductChainMapDeclaration;
using FactorMap = rfc::algebra::ProductFactorChainMap;
inline constexpr rfc::inference::NativeProductMapAvailability map_available{
    available, {{{920,23}, {921,29}, {922,31}}}};
// Two owning hexagons and all proof prefixes have an explicit logical budget.
inline rfc::runtime::ExecutionHostPolicy map_policy() {
    rfc::runtime::ExecutionHostPolicy p;
    p.name="three-factor-map-example";
    p.limits.rational_slots=p.limits.sparse_entries=64000000;
    return p;
}
inline rfc::runtime::ExecutionLease map_lease() { return rfc::runtime::ExecutionLease(map_policy()); }
inline rfc::numeric::ExactActionLimits pure_limits() {
    rfc::numeric::ExactActionLimits out;out.entries=64000000;return out;
}
struct Fixture {
    std::array<rfc::core::RelationalComplex, 3> source, target;
    Declaration maps;
};
inline rfc::core::RelationalComplex subdivision() {
    using namespace rfc;
    core::RelationSignature signature; const auto edge = signature.add_type("subdivided-interval");
    (void)signature.add_type("unused-subdivision-type"); core::RelationalComplexBuilder b(signature);
    b.declare_witness_identity(10); b.declare_witness_identity(20); b.declare_witness_identity(30);
    b.add_grade_one(core::RelationKey{201}, edge, {{10,true,true,Q{1},{}}, {30,true,false,Q{1},{}}});
    b.add_grade_one(core::RelationKey{202}, edge, {{30,true,true,Q{1},{}}, {20,true,false,Q{1},{}}});
    return b.freeze();
}
inline rfc::core::RelationalComplex square(bool micro) {
    using namespace rfc;
    core::RelationSignature signature; const auto type = signature.add_type("square-realization");
    (void)signature.add_type("unused-square-type"); core::RelationalComplexBuilder b(signature);
    for (std::uint64_t v = 10; v <= 40; v += 10) b.declare_witness_identity(v);
    for (std::uint64_t i = 0; i < 4; ++i) b.add_grade_one(core::RelationKey{100+i}, type,
        {{10+10*i,true,true,Q{1},{}}, {10+10*((i+1)%4),true,false,Q{1},{}}});
    if (micro) {
        b.add_grade_one(core::RelationKey{104}, type, {{10,true,true,Q{1},{}}, {30,true,false,Q{1},{}}});
        b.add_higher(core::Grade{2}, core::RelationKey{200}, type, {{0,true,false,Q{1},{}}, {1,true,false,Q{1},{}}, {4,true,true,Q{1},{}}});
        b.add_higher(core::Grade{2}, core::RelationKey{201}, type, {{4,true,false,Q{1},{}}, {2,true,false,Q{1},{}}, {3,true,false,Q{1},{}}});
    } else b.add_higher(core::Grade{2}, core::RelationKey{200}, type,
        {{0,true,false,Q{1},{}}, {1,true,false,Q{1},{}}, {2,true,false,Q{1},{}}, {3,true,false,Q{1},{}}});
    return b.freeze();
}
inline Fixture fixture(unsigned sample = 0) {
    using namespace rfc;
    Fixture out{{native_product_example::interval(), native_product_example::interval(Q{2,3},true), native_product_example::unit(false,2)},
        {subdivision(), native_product_example::interval(Q{2,3},true), native_product_example::unit(false,2)}, {2100,1,{}}};
    if (sample == 1) {
        out.source = {native_product_example::interval(Q{2,3},true), native_product_example::interval(Q{5,2},true), native_product_example::interval(Q{3,7},true)};
        out.target = {native_product_example::interval(Q{5,2},true), native_product_example::interval(Q{3,7},true), native_product_example::interval(Q{2,3},true)};
    } else if (sample == 2) {
        out.source = {square(false), native_product_example::unit(), native_product_example::unit()};
        out.target = {square(true), native_product_example::unit(), native_product_example::unit()};
    } else if (sample == 3) {
        out.source = {native_product_example::interval(), native_product_example::unit(), native_product_example::unit()};
        out.target = {native_product_example::unit(false,1), native_product_example::unit(), native_product_example::unit()};
    } else if (sample == 4) {
        out.source = {subdivision(), native_product_example::interval(), native_product_example::unit()};
        out.target = {native_product_example::interval(), native_product_example::interval(), native_product_example::unit()};
    } else if (sample == 5) {
        out.source = {native_product_example::unit(true,2), native_product_example::interval(), native_product_example::unit()};
        out.target = {native_product_example::unit(true,3), native_product_example::interval(), native_product_example::unit()};
    }
    for (std::size_t f = 0; f < 3; ++f) {
        std::vector<Matrix> maps;
        for (std::size_t k = 0; k < out.target[f].grade_count(); ++k) {
            const auto rows = algebra::owned_product_detail::count(out.target[f], k), cols = algebra::owned_product_detail::count(out.source[f], k);
            Matrix m(rows,cols); for (std::size_t i = 0; i < std::min(rows,cols); ++i) m.set(i,i,Q{1}); maps.push_back(std::move(m));
        }
        if (sample == 1) {
            const std::array<Q,3> from{Q{2,3},Q{5,2},Q{3,7}}, to{Q{5,2},Q{3,7},Q{2,3}};
            maps = native_product_example::factor_map(from[f],to[f],Q{2,3},Q{-1,5},Q{3,7},Q{-5,2});
        }
        out.maps.factors[f] = FactorMap::nonzero_support(std::move(maps));
    }
    if (sample == 0) out.maps.factors[0] = FactorMap::nonzero_support({Matrix::from_rows({{Q{1},Q{0}}, {Q{0},Q{1}}, {Q{0},Q{0}}}), Matrix::from_rows({{Q{1}}, {Q{1}}})});
    if (sample == 2) {
        Matrix edges(5,4); for (std::size_t i = 0; i < 4; ++i) edges.set(i,i,Q{1});
        out.maps.factors[0] = FactorMap::nonzero_support({Matrix::identity(4),edges,Matrix::from_rows({{Q{1}}, {Q{1}}})});
    }
    if (sample == 3) out.maps.factors[0] = FactorMap::nonzero_support({Matrix::from_rows({{Q{1},Q{1}}}), Matrix(0,1)});
    if (sample == 4) {
        out.maps.factors[0] = FactorMap::nonzero_support({Matrix::from_rows({{Q{1},Q{0},Q{1}}, {Q{0},Q{1},Q{0}}}), Matrix::from_rows({{Q{0},Q{1}}})});
        out.maps.factors[0].support[1].insert({0,0}); // A declared zero map direction.
    }
    return out;
}
inline Plan endpoint_plan(const std::array<rfc::core::RelationalComplex,3>& factors, rfc::inference::NativeProductHexagonAvailability a,
    rfc::storage::NativeAccessionLimits limits = {}) {
    using namespace rfc;
    storage::NativeAccessionDirectory x(storage::TemporalEndpoint(factors[0],a[0].max_sequence),a[0].source_namespace,{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},limits),
        y(storage::TemporalEndpoint(factors[1],a[1].max_sequence),a[1].source_namespace,{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},limits),
        z(storage::TemporalEndpoint(factors[2],a[2].max_sequence),a[2].source_namespace,{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},limits);
    auto r = request({native_product_example::anchor(x),native_product_example::anchor(y),native_product_example::anchor(z)});
    return Plan::prepare(std::move(r),a);
}
inline Map plan(unsigned sample = 0) {
    auto f = fixture(sample); return Map::prepare(endpoint_plan(f.source,map_available.source), endpoint_plan(f.target,map_available.target),std::move(f.maps),map_available,map_lease());
}
} // namespace native_map_example
