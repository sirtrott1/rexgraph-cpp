#pragma once
#include "rfc/storage/native_series_codec.hpp"
#include <fstream>

namespace native_series_fixture {
using namespace rfc;
using namespace rfc::storage;
using Q = numeric::Rational;
using M = numeric::ExactMatrix;
using Vec = numeric::RationalVector;
inline core::RelationalComplex path(std::size_t edges) {
    core::RelationSignature sig;
    const auto type = sig.add_type("pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(sig); builder.declare_grade(core::Grade{2});
    for (std::size_t i = 0; i < edges; ++i)
        builder.add_grade_one(core::RelationKey{100 + i}, type, {
            {10 + i, true, true, Q{1}, {}, core::OccurrenceKey{1000 + 2 * i}},
            {11 + i, true, false, Q{1}, {}, core::OccurrenceKey{1001 + 2 * i}}});
    return builder.freeze();
}
inline TemporalTensor field_tensor(std::uint64_t key, std::uint64_t carrier, const Vec& values) {
    TemporalTensor t; t.key = key; t.output = carrier; t.role = TemporalTensorRole::Field;
    std::vector<numeric::SparseRationalEntry> xs;
    for (std::size_t i = 0; i < values.size(); ++i) if (values[i] != 0) xs.push_back({i, values[i]});
    t.tensor = numeric::SparseRationalTensor(numeric::IndexDomain::linear(values.size()),
        numeric::StructuralSupport::full(values.size()), numeric::SparseRationalVector(values.size(), xs));
    return t;
}
inline TemporalTensor metric_tensor(std::uint64_t key, std::uint64_t carrier, const M& matrix) {
    TemporalTensor t; t.key = key; t.input = carrier; t.output = carrier; t.role = TemporalTensorRole::Metric;
    t.tensor = temporal_matrix_tensor(matrix); return t;
}
inline TemporalEndpoint endpoint(std::uint64_t sequence, const Vec& values) {
    TemporalEndpoint e(path(values.size()), sequence);
    e.carriers = {{1, TemporalCarrierKind::SourceGrade, core::Grade{1}, {}, 0, numeric::IndexDomain::linear(values.size())}};
    e.tensors = {field_tensor(10, 1, values), metric_tensor(20, 1, M::identity(values.size()))};
    validate_temporal_endpoint(e); return e;
}
struct PaperHistory {
    TemporalEndpoint a = endpoint(40, {1, 2});
    TemporalEndpoint b = endpoint(41, {3, 1, 6});
    TemporalEndpoint c = endpoint(42, {13, -7});
    M J0 = M::from_rows({{1, 0}, {0, 1}, {1, 1}});
    M J1 = M::from_rows({{1, 0, 1}, {0, 1, -1}});
    TemporalSegment first = make_temporal_segment(a, b, {{1, J0}});
    TemporalSegment second = make_temporal_segment(b, c, {{1, J1}});
};
inline void save(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("series fixture write failed");
}
inline void matrix(std::ostream& out, const M& m) {
    out << m.rows() << ' ' << m.cols();
    for (std::size_t i = 0; i < m.rows(); ++i) for (std::size_t j = 0; j < m.cols(); ++j) out << ' ' << m.get(i, j).get_str();
    out << '\n';
}
inline void vector(std::ostream& out, const Vec& xs) {
    out << xs.size(); for (const auto& x : xs) out << ' ' << x.get_str(); out << '\n';
}
inline void rehash(std::vector<std::byte>& b) {
    const auto hash = rfcx_detail::fnv1a64(std::span<const std::byte>(b).subspan(32));
    for (std::size_t j = 0; j < 8; ++j) b[24 + j] = std::byte((hash >> (8 * j)) & 255);
}
} // namespace native_series_fixture
