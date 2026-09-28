#pragma once

#include "rfc/storage/relational_code_observations.hpp"
#include "rfc/temporal/operation_word.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::storage {

// A route is a PAIR of retained occurrences, not merely a triple of cells or
// an entry of an already contracted boundary matrix. Upper is applied first.
struct NativeRouteKey {
    StableOccurrenceKey upper, lower;
    auto operator<=>(const NativeRouteKey&) const = default;
};
struct NativeDefectBlockKey {
    StableCellKey lower, upper;
    auto operator<=>(const NativeDefectBlockKey&) const = default;
};
enum class NativeDefectRealizationKind : std::uint8_t { SourceBoundary, LinearStalks };
struct NativeDefectRealization {
    NativeDefectRealizationKind kind{NativeDefectRealizationKind::SourceBoundary};
    // LinearStalks selects exactly one declared carrier for every retained cell.
    // A different carrier identity is not inferred from equal dimensions.
    std::vector<std::uint64_t> stalk_carriers;
    bool operator==(const NativeDefectRealization&) const = default;
};
enum class NativeDefectSelection : std::uint8_t { AllRoutes, Occurrences, LocalBlock };
struct NativeDefectQuery {
    std::uint64_t key{};
    core::Grade upper_grade{};
    // Absent: the complete grade interface. Present: this endpoint block only.
    std::optional<NativeDefectBlockKey> endpoints;
    NativeDefectSelection selection{NativeDefectSelection::AllRoutes};
    std::vector<StableOccurrenceKey> occurrences;
    std::uint64_t block{};
    bool operator==(const NativeDefectQuery&) const = default;
};
enum class NativeDefectStatus : std::uint8_t {
    PartialZero, PartialNonzero, CompleteZero, CompleteNonzero
};
struct NativeDefectLimits {
    std::size_t max_cells{1000000}, max_occurrences{2000000}, max_routes{4000000};
    std::size_t max_dimension{100000}, max_matrix_nonzeros{4000000};
    std::size_t max_candidate_routes{8000000};
    std::size_t max_queries{10000}, max_query_memberships{8000000};
    std::size_t max_total_nonzeros{16000000};
    std::size_t max_work{64000000}, max_integer_bytes{1u << 20};
};
struct NativeDefectStats {
    std::size_t cells{}, occurrences{}, routes{};
    std::size_t scanned_occurrences{}, changed_legs{}, maps_materialized{};
    std::size_t candidate_routes{}, routes_removed{}, routes_refreshed{};
    std::size_t route_products{}, reused_routes{}, block_accumulations{}, copied_blocks{};
    std::size_t query_rebuilds{}, query_accumulations{}, work{};
};
struct NativeRouteContribution {
    NativeRouteKey key;
    StableCellKey upper, middle, lower;
    bool active{};
    numeric::ExactMatrix contribution;
};
struct NativeDefectCertificate {
    NativeDefectQuery query;
    std::size_t total_routes{}, active_routes{}, nonzero_routes{};
    std::set<NativeRouteKey> selected_routes;
    // Zero blocks with retained routes remain present; cancelling occurrences
    // are never erased by contraction. No Cartesian zero matrix is allocated.
    std::map<NativeDefectBlockKey, numeric::ExactMatrix> sums;
    std::map<NativeDefectBlockKey, std::size_t> block_routes;
    bool complete() const noexcept { return selected_routes.size() == total_routes; }
    bool assembled_zero() const noexcept {
        for (const auto& [key, value] : sums) { (void)key; if (value.nonzeros()) return false; }
        return true;
    }
    NativeDefectStatus status() const noexcept {
        if (complete()) return assembled_zero() ? NativeDefectStatus::CompleteZero : NativeDefectStatus::CompleteNonzero;
        return assembled_zero() ? NativeDefectStatus::PartialZero : NativeDefectStatus::PartialNonzero;
    }
    // A statement about the declared scope, NOT about all other grades/blocks.
    bool curved_in_scope() const noexcept { return complete() && !assembled_zero(); }
    bool local_channels_cancel() const noexcept { return complete() && assembled_zero() && nonzero_routes != 0; }
};

namespace native_defect_detail {
using Q = numeric::Rational;
using Matrix = numeric::ExactMatrix;
using temporal_source_detail::require;
inline void bound(std::size_t n, std::size_t limit, const char* message) {
    if (n > limit) throw std::length_error(message);
}
struct Work {
    const NativeDefectLimits& limits;
    std::size_t used{};
    void take(std::size_t n = 1) {
        used = machine::checked_size_add(used, n);
        bound(used, limits.max_work, "defect work budget");
    }
};
inline void rational(const Q& q, const NativeDefectLimits& limits) {
    require(mpz_sgn(mpq_denref(q.get_mpq_t())) > 0, "defect rational denominator");
    const auto bytes = [](mpz_srcptr x) { const auto b = mpz_sizeinbase(x, 2); return b / 8 + (b % 8 != 0); };
    bound(bytes(mpq_numref(q.get_mpq_t())), limits.max_integer_bytes, "defect numerator budget");
    bound(bytes(mpq_denref(q.get_mpq_t())), limits.max_integer_bytes, "defect denominator budget");
}
inline void matrix(const Matrix& m, const NativeDefectLimits& limits) {
    bound(m.rows(), limits.max_dimension, "defect matrix row budget");
    bound(m.cols(), limits.max_dimension, "defect matrix column budget");
    bound(m.nonzeros(), limits.max_matrix_nonzeros, "defect matrix entry budget");
    for (const auto& [ij, q] : m.entries()) { (void)ij; rational(q, limits); }
}
inline Matrix multiply(const Matrix& a, const Matrix& b, Work& work) {
    require(a.cols() == b.rows(), "defect local map composition mismatch");
    matrix(a, work.limits); matrix(b, work.limits);
    // Preflight the reference kernel's sparse work, including its row index.
    // The declared dimension limit bounds the reference kernel's row scratch.
    work.take(b.rows()); work.take(b.nonzeros());
    std::map<std::size_t, std::size_t> rows;
    for (const auto& [ij, q] : b.entries()) { (void)q; ++rows[ij.first]; }
    for (const auto& [ij, q] : a.entries()) {
        (void)q; work.take();
        if (const auto it = rows.find(ij.second); it != rows.end()) work.take(it->second);
    }
    auto out = a.multiply(b); matrix(out, work.limits); return out;
}
inline void accumulate(Matrix& target, const Matrix& value, int sign, Work& work) {
    require(target.rows() == value.rows() && target.cols() == value.cols(), "defect accumulation carrier mismatch");
    work.take(value.nonzeros());
    for (const auto& [ij, q] : value.entries()) {
        Q v = target.get(ij.first, ij.second) + sign * q;
        rational(v, work.limits); target.set(ij.first, ij.second, std::move(v));
        bound(target.nonzeros(), work.limits.max_matrix_nonzeros, "defect sum entry budget");
    }
}
struct Cell {
    std::size_t dimension{1};
    const TemporalCarrier* carrier{}; // owned by State::endpoint
};
struct Leg {
    StableCellKey upper, lower;
    std::size_t slot{};
    bool active{};
    Q coefficient{0};
    const TemporalTensor* tensor{}; // owned by State::endpoint
    std::shared_ptr<const Matrix> map;
};
struct Block {
    std::set<NativeRouteKey> routes;
    Matrix sum;
};
struct QueryBinding {
    NativeDefectQuery query;
    std::set<StableOccurrenceKey> selected;
    bool operator==(const QueryBinding&) const = default;
};
struct State {
    std::uint64_t name_space{};
    std::shared_ptr<const TemporalEndpoint> endpoint;
    NativeDefectRealization realization;
    NativeDefectLimits limits;
    std::map<StableCellKey, Cell> cells;
    std::map<StableOccurrenceKey, Leg> legs;
    std::map<StableCellKey, std::vector<StableOccurrenceKey>> outgoing, incoming;
    std::map<NativeRouteKey, std::shared_ptr<const NativeRouteContribution>> routes;
    std::map<StableOccurrenceKey, std::set<NativeRouteKey>> reverse;
    std::map<NativeDefectBlockKey, std::shared_ptr<const Block>> blocks;
    std::map<core::Grade, std::size_t> grade_routes;
    std::map<std::uint64_t, QueryBinding> queries;
    std::map<std::uint64_t, std::shared_ptr<const NativeDefectCertificate>> certificates;
    NativeDefectStats stats;
    std::size_t exact_entries{};
};
inline bool same_cell(const Cell& a, const Cell& b) {
    if (a.dimension != b.dimension || bool(a.carrier) != bool(b.carrier)) return false;
    return !a.carrier || *a.carrier == *b.carrier;
}
inline bool same_leg(const State& old, const Leg& a, const State& next, const Leg& b) {
    if (a.upper != b.upper || a.lower != b.lower || a.active != b.active || a.coefficient != b.coefficient ||
        bool(a.tensor) != bool(b.tensor)) return false;
    if (!same_cell(old.cells.at(a.upper), next.cells.at(b.upper)) ||
        !same_cell(old.cells.at(a.lower), next.cells.at(b.lower))) return false;
    return !a.tensor || temporal_endpoint_detail::tensor_equal(*a.tensor, *b.tensor);
}
inline Matrix effective_map(const Leg& leg) {
    return leg.active ? *leg.map : Matrix(leg.map->rows(), leg.map->cols());
}
inline bool in_scope(const NativeDefectQuery& q, const NativeRouteContribution& r) {
    return q.upper_grade == r.key.upper.upper_grade &&
        (!q.endpoints || *q.endpoints == NativeDefectBlockKey{r.lower, r.upper});
}
inline bool selected(const QueryBinding& q, const NativeRouteContribution& r) {
    return in_scope(q.query, r) && (q.query.selection == NativeDefectSelection::AllRoutes ||
        (q.selected.contains(r.key.upper) && q.selected.contains(r.key.lower)));
}
inline std::size_t scope_count(const State& s, const NativeDefectQuery& q) {
    if (q.endpoints) {
        const auto b = s.blocks.find(*q.endpoints); return b == s.blocks.end() ? 0 : b->second->routes.size();
    }
    const auto g = s.grade_routes.find(q.upper_grade); return g == s.grade_routes.end() ? 0 : g->second;
}
} // namespace native_defect_detail
} // namespace rfc::storage
