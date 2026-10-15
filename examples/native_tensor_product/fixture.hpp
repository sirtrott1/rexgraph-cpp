#pragma once
#include "rfc/inference/native_tensor_product.hpp"
#include "rfc/core/builder.hpp"

namespace native_product_example {
using Q = rfc::numeric::Rational;
using Matrix = rfc::numeric::ExactMatrix;
using Cell = rfc::algebra::ProductCell;
using Product = rfc::algebra::OwnedRelationalTensorProduct;
using Plan = rfc::inference::PreparedNativeTensorProduct;
using Map = rfc::inference::PreparedNativeTensorChainMap;
using Guard = rfc::numeric::ExactActionGuard;
inline Cell cell(std::uint16_t p, std::uint32_t a, std::uint16_t q, std::uint32_t b) {
    return {{rfc::core::Grade{p}, a}, {rfc::core::Grade{q}, b}};
}
inline rfc::core::RelationalComplex interval(Q scale = 1, bool face = false, std::size_t witnesses = 0) {
    using namespace rfc;
    core::RelationSignature signature;
    const auto tag = signature.aspects().add("tag", composite::AspectRole::Provenance, false);
    const auto edge = signature.add_type("exact-interval");
    (void)signature.add_type("unused-declared-type");
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, edge, {
        {10, true, true, scale, {{tag, true}}, core::OccurrenceKey{101}},
        {20, true, false, scale, {{tag, false}}, core::OccurrenceKey{102}},
        {10, true, false, Q{2}, {{tag, true}}, core::OccurrenceKey{103}},
        {10, true, true, Q{2}, {{tag, false}}, core::OccurrenceKey{104}},
        {20, false, true, Q{0}, {{tag, true}}, core::OccurrenceKey{105}},
        {20, true, false, Q{0}, {{tag, false}}, core::OccurrenceKey{106}}});
    if (face) builder.add_higher(core::Grade{2}, core::RelationKey{100}, edge, {
        {0, true, false, Q{1}, {{tag, true}}, core::OccurrenceKey{211}},
        {0, true, true, Q{1}, {{tag, false}}, core::OccurrenceKey{212}},
        {0, false, true, Q{0}, {{tag, true}}, core::OccurrenceKey{213}},
        {0, true, false, Q{0}, {{tag, false}}, core::OccurrenceKey{214}}});
    for (std::size_t i = 0; i < witnesses; ++i) builder.declare_witness_identity(1000 + i);
    return builder.freeze();
}
inline rfc::core::RelationalComplex unit(bool empty = false, std::uint16_t maximum = 0) {
    rfc::core::RelationalComplexBuilder builder{rfc::core::RelationSignature{}};
    if (!empty) builder.declare_witness_identity(10);
    builder.declare_grade(rfc::core::Grade{maximum});
    return builder.freeze();
}
inline rfc::algebra::RelationalProductDeclaration declaration(const rfc::core::RelationalComplex& a,
                                                             const rfc::core::RelationalComplex& b) {
    rfc::algebra::RelationalProductDeclaration result{500, 3, {}};
    for (std::size_t i = 0; i <= a.signature().type_count(); ++i)
        for (std::size_t j = 0; j <= b.signature().type_count(); ++j)
            result.types.push_back({i ? std::optional{static_cast<rfc::core::RelationTypeId>(i - 1)} : std::nullopt,
                                    j ? std::optional{static_cast<rfc::core::RelationTypeId>(j - 1)} : std::nullopt,
                                    501 + result.types.size()});
    return result;
}
inline rfc::storage::PinnedNativeAccession anchor(const rfc::storage::NativeAccessionDirectory& d) {
    return d.pin(rfc::storage::NativeAccessionRequest::sparse_chain(rfc::core::Grade{0}, {}, {}));
}
inline rfc::inference::NativeTensorProductRequest request(const rfc::storage::NativeAccessionDirectory& a,
                                                        const rfc::storage::NativeAccessionDirectory& b) {
    const auto left = anchor(a), right = anchor(b);
    return {left, right, 41, 42, declaration(left.endpoint()->source, right.endpoint()->source)};
}
inline std::vector<Matrix> factor_map(Q from_scale, Q to_scale, Q t, Q u, Q v, Q face_value = 1) {
    Q h = to_scale * t / from_scale; h.canonicalize();
    return {Matrix::from_rows({{u + h, u}, {v, v + h}}), Matrix::from_rows({{t}}), Matrix::from_rows({{face_value}})};
}
inline constexpr rfc::inference::NativeProductAvailability available{{900, 7}, {901, 8}};
inline constexpr rfc::inference::NativeProductAvailability target_available{{902, 9}, {903, 10}};
} // namespace native_product_example
