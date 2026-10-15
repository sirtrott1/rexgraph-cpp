#pragma once
#include "rfc/inference/native_product_map_composition.hpp"
#include "../native_product_chain_map/fixture.hpp"

namespace native_composition_example {
using namespace native_map_example;
using Composition = rfc::inference::PreparedNativeProductMapComposition;
using PureComposition = rfc::algebra::PreparedProductMapComposition;
inline constexpr rfc::inference::NativeProductCompositionAvailability available_composition{
    map_available.source, map_available.target, {{{930,37}, {931,41}, {932,43}}}};
inline rfc::runtime::ExecutionHostPolicy composition_policy() {
    auto p = map_policy(); p.name = "three-factor-composition-example";
    p.limits.rational_slots = p.limits.sparse_entries = 512000000; return p;
}
inline rfc::runtime::ExecutionLease composition_lease() { return rfc::runtime::ExecutionLease(composition_policy()); }
inline rfc::numeric::ExactActionLimits composition_limits() {
    auto out = pure_limits(); out.entries = 512000000; return out;
}
inline rfc::core::RelationalComplex points(std::size_t n, std::uint16_t empty_grades = 0) {
    using namespace rfc;
    core::RelationSignature signature; (void)signature.add_type("unused-point-type");
    core::RelationalComplexBuilder b(signature);
    for (std::size_t i = 0; i < n; ++i) b.declare_witness_identity(10+static_cast<std::uint64_t>(i));
    b.declare_grade(core::Grade{empty_grades});
    return b.freeze();
}
inline Declaration identity_between(const std::array<rfc::core::RelationalComplex,3>& a,
    const std::array<rfc::core::RelationalComplex,3>& b, std::uint64_t key) {
    Declaration d{key, 2, {}};
    for (std::size_t f = 0; f < 3; ++f) {
        std::vector<Matrix> matrices;
        for (std::size_t k = 0; k < b[f].grade_count(); ++k) {
            const auto rows = rfc::algebra::owned_product_detail::count(b[f], k), cols = rfc::algebra::owned_product_detail::count(a[f], k);
            Matrix m(rows, cols); for (std::size_t i = 0; i < std::min(rows, cols); ++i) m.set(i,i,Q{1});
            matrices.push_back(std::move(m));
        }
        d.factors[f] = FactorMap::nonzero_support(std::move(matrices));
    }
    return d;
}
struct Fixture {
    std::array<rfc::core::RelationalComplex,3> source, intermediate, target;
    Declaration first, second;
};
inline Fixture fixture(unsigned sample = 0) {
    const auto f = native_map_example::fixture(sample < 6 ? sample : 0);
    Fixture out{f.source, f.target, f.target, f.maps, {}}; out.first.key = 3100;
    if (sample == 0) out.target[0] = native_product_example::interval();
    if (sample == 1) out.target = {native_product_example::interval(Q{3,7},true),
        native_product_example::interval(Q{2,3},true), native_product_example::interval(Q{5,2},true)};
    if (sample == 2) out.target[0] = native_product_example::unit(false,2);
    if (sample == 3) out.target[0] = points(2,1);
    if (sample == 4) out.target[0] = native_product_example::unit(false,1);
    if (sample == 5) out.target[0] = native_product_example::unit(true,4);
    if (sample >= 6) {
        out.source = {points(sample == 8 ? 2u : 1u,2), native_product_example::unit(false,2), native_product_example::unit(false,2)};
        out.intermediate = {points(2,2), out.source[1], out.source[2]};
        out.target = {points(sample == 8 ? 2u : 1u,2), out.source[1], out.source[2]};
        if (sample == 8) {
            out.source[1] = out.intermediate[1] = out.target[1] = native_product_example::interval(Q{2,3},true);
            out.source[2] = out.intermediate[2] = out.target[2] = native_product_example::interval(Q{5,7},true);
        }
        out.first = identity_between(out.source, out.intermediate, 3100);
        out.first.factors[0].matrices[0] = sample == 8 ? Matrix::from_rows({{Q{1},Q{1}}, {Q{0},Q{1}}})
            : Matrix::from_rows({{Q{1}}, {sample == 7 ? Q{0} : Q{1}}});
        out.first.factors[0].support[0] = sample == 8 ? std::set<Matrix::Key>{{0,0},{0,1},{1,1}} : std::set<Matrix::Key>{{0,0},{1,0}};
    }
    out.second = identity_between(out.intermediate, out.target, 3200);
    if (sample == 0) {
        out.second.factors[0] = FactorMap::nonzero_support({Matrix::from_rows({{Q{1},Q{0},Q{1}}, {Q{0},Q{1},Q{0}}}), Matrix::from_rows({{Q{0},Q{1}}})});
        out.second.factors[0].support[1].insert({0,0});
    }
    if (sample == 1) {
        const std::array<Q,3> mid{Q{5,2},Q{3,7},Q{2,3}}, end{Q{3,7},Q{2,3},Q{5,2}};
        for (std::size_t j = 0; j < 3; ++j) out.second.factors[j] = FactorMap::nonzero_support(
            native_product_example::factor_map(mid[j],end[j],Q{-5,3},Q{7,11},Q{2,5},Q{-3,2}));
    }
    if (sample == 2) out.second.factors[0] = FactorMap::nonzero_support({Matrix::from_rows({{Q{1},Q{1},Q{1},Q{1}}}), Matrix(0,5), Matrix(0,2)});
    if (sample == 3) out.second.factors[0] = FactorMap::nonzero_support({Matrix::from_rows({{Q{1}}, {Q{-1}}}), Matrix(0,0)});
    if (sample == 4) out.second.factors[0] = FactorMap::nonzero_support({Matrix::from_rows({{Q{1},Q{1}}}), Matrix(0,1)});
    if (sample >= 6) {
        out.second.factors[0].matrices[0] = sample == 8 ? Matrix::from_rows({{Q{1},Q{0}}, {Q{1},Q{1}}})
            : Matrix::from_rows({{sample == 7 ? Q{0} : Q{1}, sample == 7 ? Q{3} : Q{-1}}});
        out.second.factors[0].support[0] = sample == 8 ? std::set<Matrix::Key>{{0,0},{1,0},{1,1}} : std::set<Matrix::Key>{{0,0},{0,1}};
    }
    return out;
}
struct Plans { Map first, second; Composition composition; };
inline Plans plans(unsigned sample = 0) {
    auto f = fixture(sample);
    auto source = endpoint_plan(f.source, available_composition.source), middle = endpoint_plan(f.intermediate, available_composition.intermediate),
        target_plan = endpoint_plan(f.target, available_composition.target);
    auto first = Map::prepare(source,middle,std::move(f.first),{available_composition.source,available_composition.intermediate},composition_lease());
    auto second = Map::prepare(middle,target_plan,std::move(f.second),{available_composition.intermediate,available_composition.target},composition_lease());
    auto composed = Composition::prepare(first,second,3300,4,available_composition,composition_lease());
    return {std::move(first),std::move(second),std::move(composed)};
}
} // namespace native_composition_example
