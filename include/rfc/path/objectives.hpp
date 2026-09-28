#pragma once
#include "rfc/path/realization.hpp"
#include "rfc/view/graph_projection.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::path {

struct RouteResult {
    bool reachable{};
    numeric::Rational distance{0};
    std::vector<std::uint32_t> projected_edges;
    std::vector<std::uint32_t> relations; // retained source grade-one relation indices
    numeric::SparseRationalVector source_field;

    [[nodiscard]] numeric::RationalVector materialize_source_field() const {
        numeric::RationalVector dense(source_field.size(), numeric::Rational{0});
        for (const auto& entry : source_field.entries()) dense.at(entry.index) = entry.value;
        return dense;
    }
};

inline RouteResult bfs_min_hops(const view::DirectedGraphProjection& graph,
                                std::uint32_t source,
                                std::uint32_t target) {
    if (source >= graph.object_count() || target >= graph.object_count()) throw std::out_of_range("BFS object index");
    const auto n = graph.object_count();
    const auto no_index = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> queue(n);
    std::vector<bool> seen(n, false);
    std::vector<std::uint32_t> parent_vertex(n, no_index), parent_edge(n, no_index);
    std::size_t head = 0, tail = 0;
    queue[tail++] = source;
    seen[source] = true;
    while (head < tail) {
        const auto u = queue[head++];
        if (u == target) break;
        for (const auto edge_index : graph.outgoing(u)) {
            const auto& edge = graph.edge(edge_index);
            if (seen[edge.target_object]) continue;
            seen[edge.target_object] = true;
            parent_vertex[edge.target_object] = u;
            parent_edge[edge.target_object] = static_cast<std::uint32_t>(edge_index);
            queue[tail++] = edge.target_object;
        }
    }
    if (!seen[target]) return RouteResult{false, 0, {}, {}, numeric::SparseRationalVector(graph.source_relation_count())};

    std::vector<std::uint32_t> reversed_edges;
    for (auto v = target; v != source; v = parent_vertex[v]) reversed_edges.push_back(parent_edge[v]);
    std::vector<std::uint32_t> projected(reversed_edges.rbegin(), reversed_edges.rend());
    std::vector<std::uint32_t> relations;
    std::vector<numeric::SparseRationalEntry> field_entries;
    relations.reserve(projected.size());
    field_entries.reserve(projected.size());
    for (const auto edge_index : projected) {
        const auto source_relation = graph.edge(edge_index).source_relation;
        relations.push_back(source_relation);
        field_entries.push_back({source_relation, numeric::Rational{1}});
    }
    return RouteResult{true,
                       numeric::Rational{static_cast<long>(projected.size())},
                       std::move(projected),
                       std::move(relations),
                       numeric::SparseRationalVector(graph.source_relation_count(), std::move(field_entries))};
}

inline RouteResult dijkstra_exact(const view::DirectedGraphProjection& graph,
                                  std::uint32_t source,
                                  std::uint32_t target) {
    if (source >= graph.object_count() || target >= graph.object_count()) throw std::out_of_range("Dijkstra object index");

    const auto n = graph.object_count();
    std::vector<std::optional<numeric::Rational>> dist(n);
    const auto no_index = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> parent_vertex(n, no_index), parent_edge(n, no_index);
    using QueueItem = std::pair<numeric::Rational, std::uint32_t>;
    std::set<QueueItem> frontier;
    dist[source] = numeric::Rational{0};
    frontier.insert({numeric::Rational{0}, source});
    while (!frontier.empty()) {
        const auto [du, u] = *frontier.begin();
        frontier.erase(frontier.begin());
        if (!dist[u] || du != *dist[u]) continue;
        if (u == target) break;
        for (const auto edge_index : graph.outgoing(u)) {
            const auto& edge = graph.edge(edge_index);
            numeric::Rational candidate = du + edge.measure;
            candidate.canonicalize();
            if (!dist[edge.target_object] || candidate < *dist[edge.target_object]) {
                if (dist[edge.target_object]) frontier.erase({*dist[edge.target_object], edge.target_object});
                dist[edge.target_object] = candidate;
                parent_vertex[edge.target_object] = u;
                parent_edge[edge.target_object] = static_cast<std::uint32_t>(edge_index);
                frontier.insert({candidate, edge.target_object});
            }
        }
    }
    if (!dist[target]) return RouteResult{false, 0, {}, {}, numeric::SparseRationalVector(graph.source_relation_count())};

    std::vector<std::uint32_t> reversed_edges;
    for (auto v = target; v != source; v = parent_vertex[v]) reversed_edges.push_back(parent_edge[v]);
    std::vector<std::uint32_t> projected(reversed_edges.rbegin(), reversed_edges.rend());
    std::vector<std::uint32_t> relations;
    std::vector<numeric::SparseRationalEntry> field_entries;
    relations.reserve(projected.size());
    field_entries.reserve(projected.size());
    for (const auto edge_index : projected) {
        const auto source_relation = graph.edge(edge_index).source_relation;
        relations.push_back(source_relation);
        field_entries.push_back({source_relation, numeric::Rational{1}});
    }
    return RouteResult{true,
                       *dist[target],
                       std::move(projected),
                       std::move(relations),
                       numeric::SparseRationalVector(graph.source_relation_count(), std::move(field_entries))};
}

inline bool shortest_path_certificate(const numeric::ExactMatrix& boundary,
                                      const numeric::RationalVector& lengths,
                                      const numeric::RationalVector& path_field,
                                      const numeric::RationalVector& demand,
                                      const numeric::RationalVector& potential) {
    if (boundary.cols() != lengths.size() || path_field.size() != lengths.size() ||
        boundary.rows() != demand.size() || potential.size() != demand.size()) {
        throw std::invalid_argument("shortest-path certificate shape mismatch");
    }
    for (const auto& x : path_field) if (x < 0) return false;
    if (boundary.apply(path_field) != demand) return false;
    const auto dual = boundary.transpose().apply(potential);
    for (std::size_t i = 0; i < dual.size(); ++i) if (dual[i] > lengths[i]) return false;
    return numeric::dot(lengths, path_field) == numeric::dot(potential, demand);
}

inline numeric::RationalVector least_quadrance_feasible(const numeric::ExactMatrix& boundary,
                                                         const field::ExactMetric& metric,
                                                         const numeric::RationalVector& feasible) {
    if (boundary.cols() != metric.size() || feasible.size() != metric.size()) throw std::invalid_argument("least-quadrance dimensions");
    const auto demand = boundary.apply(feasible);
    if (boundary.apply(feasible) != demand) throw std::logic_error("unreachable least-quadrance guard");
    const auto Z = boundary.kernel_frame();
    if (Z.cols() == 0) return feasible;
    const auto gram = field::metric_gram(Z,metric,Z);
    const auto rhs = field::metric_frame_dual_apply(Z,metric,feasible);
    const auto coeff = gram.solve(rhs);
    return numeric::sub(feasible, Z.apply(coeff));
}

} // namespace rfc::path
