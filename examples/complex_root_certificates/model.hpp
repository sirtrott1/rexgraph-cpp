#pragma once
#include "rfc/geometry/complex_algebraic_embedding.hpp"

namespace complex_example {
using Q=rfc::numeric::Rational;
using Field=rfc::numeric::CertifiedAlgebraicField;
using Embedding=rfc::geometry::SelectedComplexAlgebraicEmbedding;
struct Case { const char* name; Field field; Embedding embedding; };
inline std::vector<Case> cases() {
    const auto quadratic=Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3});
    const auto cubic=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto quartic=Field::declare({1,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    const auto quintic=Field::declare({-2,0,0,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto sextic=Field::declare({-2,0,0,0,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto octic=Field::declare({1,0,0,0,0,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    std::vector<Case> result;
    const auto add=[&](const char* name,const Field& field,Q x,Q y,Q radius) {
        const auto upper=Embedding::isolate(field,{std::move(x),std::move(y)},std::move(radius));
        result.push_back({name,field,upper}); result.push_back({name,field,upper.conjugated()});
    };
    add("quadratic",quadratic,0,1,Q{1,4});
    add("cubic",cubic,Q{-5,8},Q{35,32},Q{1,8});
    add("quartic",quartic,Q{11,16},Q{11,16},Q{1,8});
    add("quintic",quintic,Q{23,64},Q{35,32},Q{1,16});
    add("sextic",sextic,Q{9,16},Q{31,32},Q{1,32});
    add("octic",octic,Q{59,64},Q{49,128},Q{1,32});
    return result;
}
} // namespace complex_example
