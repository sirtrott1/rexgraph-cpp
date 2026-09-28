#pragma once

#include "rfc/storage/native_defect_types.hpp"

namespace rfc::storage {
namespace native_defect_detail {

inline void add_entries(State& s, std::size_t n) {
    s.exact_entries = machine::checked_size_add(s.exact_entries, n);
    bound(s.exact_entries, s.limits.max_total_nonzeros, "aggregate defect exact-entry budget");
}
inline void remove_entries(State& s, std::size_t n) {
    require(n <= s.exact_entries, "defect exact-entry accounting invariant"); s.exact_entries -= n;
}
inline NativeDefectRealization canonical_realization(NativeDefectRealization r, const NativeDefectLimits& limits) {
    require(r.kind == NativeDefectRealizationKind::SourceBoundary || r.kind == NativeDefectRealizationKind::LinearStalks,
            "unknown defect realization");
    bound(r.stalk_carriers.size(), limits.max_cells, "defect stalk selection budget");
    std::sort(r.stalk_carriers.begin(), r.stalk_carriers.end());
    require(std::adjacent_find(r.stalk_carriers.begin(), r.stalk_carriers.end()) == r.stalk_carriers.end(),
            "duplicate selected defect carrier");
    require(r.kind != NativeDefectRealizationKind::SourceBoundary || r.stalk_carriers.empty(),
            "scalar boundary realization cannot carry stalk selections");
    return r;
}
inline QueryBinding bind_query(const State& s, NativeDefectQuery q, Work& work) {
    require(q.key != 0, "defect query key required");
    require(q.upper_grade.value >= 2 && q.upper_grade.value < s.endpoint->source.grade_count(), "defect query grade absent");
    if (q.endpoints) {
        require(q.endpoints->upper.grade == q.upper_grade &&
                std::size_t(q.endpoints->lower.grade.value) + 2 == q.upper_grade.value,
                "defect endpoint scope grade mismatch");
        require(s.cells.contains(q.endpoints->upper) && s.cells.contains(q.endpoints->lower), "defect endpoint scope absent");
    }
    require(q.selection == NativeDefectSelection::AllRoutes || q.selection == NativeDefectSelection::Occurrences ||
            q.selection == NativeDefectSelection::LocalBlock, "unknown defect query selection");
    bound(q.occurrences.size(), s.limits.max_occurrences, "defect query occurrence budget");
    require(q.selection == NativeDefectSelection::Occurrences || q.occurrences.empty(), "extraneous defect occurrences");
    require(q.selection == NativeDefectSelection::LocalBlock || q.block == 0, "extraneous defect block key");
    std::sort(q.occurrences.begin(), q.occurrences.end());
    require(std::adjacent_find(q.occurrences.begin(), q.occurrences.end()) == q.occurrences.end(), "duplicate chart occurrence");
    QueryBinding out{std::move(q), {}};
    if (out.query.selection == NativeDefectSelection::Occurrences) {
        for (const auto key : out.query.occurrences) {
            work.take(); require(s.legs.contains(key), "chart occurrence absent");
            require(key.upper_grade == out.query.upper_grade || std::size_t(key.upper_grade.value) + 1 == out.query.upper_grade.value,
                    "chart occurrence outside adjacent interface");
            out.selected.insert(key);
        }
    } else if (out.query.selection == NativeDefectSelection::LocalBlock) {
        const LocalBlockDefinition* found = nullptr;
        for (const auto& b : s.endpoint->local_blocks) { work.take(); if (b.block_key == out.query.block) found = &b; }
        require(found, "defect local block absent");
        const auto add = [&](StableOccurrenceKey key) {
            work.take();
            if (key.upper_grade == out.query.upper_grade || std::size_t(key.upper_grade.value) + 1 == out.query.upper_grade.value)
                out.selected.insert(key);
        };
        if (found->kind == LocalBlockKind::StrictSubcomplex) {
            for (auto key : found->cells) {
                work.take();
                if (const auto it = s.outgoing.find(key); it != s.outgoing.end()) for (auto occ : it->second) add(occ);
            }
        } else for (auto occ : found->occurrences) add(occ);
    }
    return out;
}

inline void bind_source(State& next, const State* old, std::set<StableOccurrenceKey>& changed, Work& work) {
    const auto& e = *next.endpoint;
    const auto cs = temporal_endpoint_detail::carriers(e);
    std::map<StableCellKey, const TemporalCarrier*> stalks;
    for (auto key : next.realization.stalk_carriers) {
        work.take(); require(cs.contains(key) && cs.at(key)->kind == TemporalCarrierKind::CellStalk, "selected defect carrier is not a stalk");
        require(stalks.emplace(*cs.at(key)->cell, cs.at(key)).second, "two selected stalks on one retained cell");
    }
    for (std::size_t k = 0; k < e.source.grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        const auto& arena = e.source.grade(grade);
        for (std::size_t i = 0; i < arena.cell_count(); ++i) {
            work.take(); bound(next.cells.size() + 1, next.limits.max_cells, "defect cell count budget");
            const auto key = stable_cell_key(e.source, {grade, static_cast<std::uint32_t>(i)});
            Cell c;
            if (next.realization.kind == NativeDefectRealizationKind::LinearStalks) {
                require(stalks.contains(key), "defect realization needs a selected stalk on every retained cell");
                c.carrier = stalks.at(key); c.dimension = c.carrier->domain.logical_size();
                require(c.dimension != 0, "native linear stalk dimension must be positive");
            }
            bound(c.dimension, next.limits.max_dimension, "defect stalk dimension budget");
            next.cells.emplace(key, c);
        }
    }
    if (next.realization.kind == NativeDefectRealizationKind::LinearStalks)
        require(stalks.size() == next.cells.size(), "extraneous selected defect stalk");
    std::map<StableOccurrenceKey, const TemporalTensor*> maps;
    if (next.realization.kind == NativeDefectRealizationKind::LinearStalks) {
        for (const auto& t : e.tensors) {
            work.take();
            if (t.role == TemporalTensorRole::RealizedIncidenceMap || t.role == TemporalTensorRole::CoefficientIndependentMap)
                require(maps.emplace(t.owner.occurrence, &t).second, "duplicate local incidence map");
        }
    }
    for (std::size_t k = 1; k < e.source.grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)}, below{static_cast<std::uint16_t>(k - 1)};
        const auto& arena = e.source.grade(grade);
        for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
            work.take(); ++next.stats.scanned_occurrences;
            bound(next.legs.size() + 1, next.limits.max_occurrences, "defect occurrence budget");
            const StableOccurrenceKey key{grade, arena.occurrence_identities[p]};
            Leg leg;
            leg.upper = stable_cell_key(e.source, {grade, arena.upper_index[p]});
            leg.lower = stable_cell_key(e.source, {below, arena.lower_index[p]});
            leg.slot = p; leg.active = arena.aspects.get(e.source.signature().existence_aspect(), p);
            leg.coefficient = e.source.incidence_coefficient(grade, p); rational(leg.coefficient, next.limits);
            if (const auto it = maps.find(key); it != maps.end()) leg.tensor = it->second;
            const auto& upper = next.cells.at(leg.upper); const auto& lower = next.cells.at(leg.lower);
            if (leg.tensor) {
                require(leg.tensor->input == upper.carrier->key && leg.tensor->output == lower.carrier->key,
                        "local map belongs to a different selected stalk realization");
            } else require(upper.dimension == lower.dimension, "rectangular defect stalk requires an explicit local map");
            const auto previous = old ? old->legs.find(key) : next.legs.end();
            const bool same = old && previous != old->legs.end() && old->realization.kind == next.realization.kind &&
                              same_leg(*old, previous->second, next, leg);
            if (same) leg.map = previous->second.map;
            else {
                changed.insert(key);
                Matrix m(lower.dimension, upper.dimension);
                if (leg.tensor) {
                    // Full target tensor, not a defect-only reconstruction. A
                    // realized override is never multiplied by c a second time.
                    m = temporal_tensor_matrix(*leg.tensor);
                    if (leg.tensor->role == TemporalTensorRole::CoefficientIndependentMap) m = m.scale(leg.coefficient);
                } else if (leg.coefficient != 0) {
                    work.take(upper.dimension);
                    for (std::size_t d = 0; d < upper.dimension; ++d) m.set(d, d, leg.coefficient);
                }
                matrix(m, next.limits); ++next.stats.maps_materialized;
                leg.map = std::make_shared<const Matrix>(std::move(m));
            }
            add_entries(next, leg.map->nonzeros());
            next.outgoing[leg.upper].push_back(key); next.incoming[leg.lower].push_back(key);
            require(next.legs.emplace(key, std::move(leg)).second, "duplicate retained occurrence identity");
        }
    }
    if (old) for (const auto& [key, leg] : old->legs) { (void)leg; work.take(); if (!next.legs.contains(key)) changed.insert(key); }
    next.stats.changed_legs = changed.size(); next.stats.cells = next.cells.size(); next.stats.occurrences = next.legs.size();
}
inline void candidate(std::set<NativeRouteKey>& keys, NativeRouteKey key, std::size_t limit, Work& work) {
    work.take();
    if (keys.contains(key)) return;
    bound(machine::checked_size_add(keys.size(), 1), limit, "defect candidate route budget");
    keys.insert(key);
}
inline void add_candidates(const State& s, StableOccurrenceKey key, std::set<NativeRouteKey>& candidates,
                           std::size_t limit, Work& work) {
    const auto found = s.legs.find(key); if (found == s.legs.end()) return;
    const auto& leg = found->second;
    if (key.upper_grade.value >= 2) {
        if (const auto it = s.outgoing.find(leg.lower); it != s.outgoing.end())
            for (auto lower : it->second) candidate(candidates, {key, lower}, limit, work);
    }
    if (const auto it = s.incoming.find(leg.upper); it != s.incoming.end())
        for (auto upper : it->second) candidate(candidates, {upper, key}, limit, work);
}
inline bool valid_route(const State& s, NativeRouteKey key) {
    const auto a = s.legs.find(key.upper), b = s.legs.find(key.lower);
    return a != s.legs.end() && b != s.legs.end() && a->second.lower == b->second.upper &&
           key.upper.upper_grade.value >= 2 && std::size_t(key.lower.upper_grade.value) + 1 == key.upper.upper_grade.value;
}
using MutableBlocks = std::map<NativeDefectBlockKey, std::shared_ptr<Block>>;
inline std::shared_ptr<Block> edit_block(State& s, MutableBlocks& edits, NativeDefectBlockKey key,
                                        std::size_t rows, std::size_t cols) {
    if (const auto it = edits.find(key); it != edits.end()) return it->second;
    const auto old = s.blocks.find(key);
    auto b = old == s.blocks.end() ? std::make_shared<Block>(Block{{}, Matrix(rows, cols)}) : std::make_shared<Block>(*old->second);
    if (old != s.blocks.end()) ++s.stats.copied_blocks;
    edits.emplace(key, b); return b;
}
inline void remove_route(State& s, MutableBlocks& edits, NativeRouteKey key, Work& work) {
    const auto found = s.routes.find(key); if (found == s.routes.end()) return;
    const auto& r = *found->second; const NativeDefectBlockKey bk{r.lower, r.upper};
    auto b = edit_block(s, edits, bk, r.contribution.rows(), r.contribution.cols());
    require(b->routes.erase(key) == 1, "defect block membership invariant");
    remove_entries(s, r.contribution.nonzeros()); remove_entries(s, b->sum.nonzeros());
    accumulate(b->sum, r.contribution, -1, work); add_entries(s, b->sum.nonzeros()); ++s.stats.block_accumulations;
    if (b->routes.empty()) s.blocks.erase(bk); else s.blocks[bk] = std::move(b);
    for (auto leg : {key.upper, key.lower}) {
        auto& refs = s.reverse.at(leg); require(refs.erase(key) == 1, "defect reverse membership invariant");
        if (refs.empty()) s.reverse.erase(leg);
    }
    auto& count = s.grade_routes.at(key.upper.upper_grade); require(count != 0, "defect route count invariant"); --count;
    s.routes.erase(found); ++s.stats.routes_removed;
}
inline void insert_route(State& s, MutableBlocks& edits, NativeRouteKey key, Work& work) {
    bound(s.routes.size() + 1, s.limits.max_routes, "defect route count budget");
    const auto& upper = s.legs.at(key.upper); const auto& lower = s.legs.at(key.lower);
    auto r = std::make_shared<NativeRouteContribution>();
    r->key = key; r->upper = upper.upper; r->middle = upper.lower; r->lower = lower.lower;
    r->active = upper.active && lower.active;
    // Retain inactive occurrences but do not execute them in the assembled
    // native relation tensor. Their declared overrides remain in the endpoint.
    r->contribution = r->active ? multiply(*lower.map, *upper.map, work) : Matrix(lower.map->rows(), upper.map->cols());
    if (r->active) ++s.stats.route_products;
    add_entries(s, r->contribution.nonzeros());
    const NativeDefectBlockKey bk{r->lower, r->upper};
    auto b = edit_block(s, edits, bk, r->contribution.rows(), r->contribution.cols());
    if (b->routes.empty()) b->sum = Matrix(r->contribution.rows(), r->contribution.cols());
    require(b->routes.insert(key).second, "duplicate defect route");
    remove_entries(s, b->sum.nonzeros());
    accumulate(b->sum, r->contribution, 1, work); add_entries(s, b->sum.nonzeros()); ++s.stats.block_accumulations;
    s.blocks[bk] = std::move(b); s.reverse[key.upper].insert(key); s.reverse[key.lower].insert(key);
    ++s.grade_routes[key.upper.upper_grade]; s.routes.emplace(key, std::move(r)); ++s.stats.routes_refreshed;
}
inline void adjust_certificate(NativeDefectCertificate& c, const NativeRouteContribution& r, int sign, State& s, Work& work) {
    const NativeDefectBlockKey bk{r.lower, r.upper};
    if (sign == 1) {
        bound(c.selected_routes.size() + 1, s.limits.max_query_memberships, "defect chart membership budget");
        require(c.selected_routes.insert(r.key).second, "duplicate chart route");
        if (!c.block_routes.contains(bk)) c.sums.emplace(bk, Matrix(r.contribution.rows(), r.contribution.cols()));
        ++c.block_routes[bk]; c.active_routes += r.active; c.nonzero_routes += r.contribution.nonzeros() != 0;
        accumulate(c.sums.at(bk), r.contribution, 1, work);
    } else {
        require(c.selected_routes.erase(r.key) == 1, "missing chart route");
        accumulate(c.sums.at(bk), r.contribution, -1, work);
        if (--c.block_routes.at(bk) == 0) { c.block_routes.erase(bk); c.sums.erase(bk); }
        c.active_routes -= r.active; c.nonzero_routes -= r.contribution.nonzeros() != 0;
    }
    ++s.stats.query_accumulations;
}
inline void update_queries(State& s, const State* old, const std::vector<NativeDefectQuery>& queries,
                           const std::set<NativeRouteKey>& affected, Work& work) {
    bound(queries.size(), s.limits.max_queries, "defect query count budget");
    std::size_t memberships = 0;
    for (const auto& q : queries) {
        auto binding = bind_query(s, q, work);
        require(s.queries.emplace(q.key, binding).second, "duplicate defect query key");
        const auto total = scope_count(s, binding.query);
        bool reuse = old && old->queries.contains(q.key) && old->queries.at(q.key) == binding;
        std::shared_ptr<NativeDefectCertificate> c;
        if (reuse) {
            const auto& prior = *old->certificates.at(q.key);
            bool dirty = prior.total_routes != total;
            for (const auto key : affected) {
                work.take();
                if (prior.selected_routes.contains(key) || (s.routes.contains(key) && selected(binding, *s.routes.at(key)))) dirty = true;
            }
            if (!dirty) s.certificates[q.key] = old->certificates.at(q.key);
            else {
                c = std::make_shared<NativeDefectCertificate>(prior);
                // Remove ALL old values before adding new ones, including when
                // a changing stalk alters the shape of a whole endpoint block.
                for (const auto key : affected) if (c->selected_routes.contains(key)) adjust_certificate(*c, *old->routes.at(key), -1, s, work);
                for (const auto key : affected) if (s.routes.contains(key) && selected(binding, *s.routes.at(key)))
                    adjust_certificate(*c, *s.routes.at(key), 1, s, work);
            }
        } else {
            c = std::make_shared<NativeDefectCertificate>(); c->query = binding.query; ++s.stats.query_rebuilds;
            for (const auto& [key, r] : s.routes) {
                (void)key; work.take(); if (selected(binding, *r)) adjust_certificate(*c, *r, 1, s, work);
            }
        }
        if (c) { c->total_routes = total; s.certificates[q.key] = std::move(c); }
        for (const auto& [bk, m] : s.certificates.at(q.key)->sums) { (void)bk; add_entries(s, m.nonzeros()); }
        memberships = machine::checked_size_add(memberships, s.certificates.at(q.key)->selected_routes.size());
        bound(memberships, s.limits.max_query_memberships, "aggregate defect chart memberships");
    }
}
inline std::shared_ptr<const State> build(std::shared_ptr<const TemporalEndpoint> endpoint, std::uint64_t name_space,
    NativeDefectRealization realization, const std::vector<NativeDefectQuery>& queries,
    const NativeDefectLimits& limits, const State* old) {
    auto s = std::make_shared<State>(); s->endpoint = std::move(endpoint); s->name_space = name_space; s->limits = limits;
    s->realization = canonical_realization(std::move(realization), limits);
    Work work{limits}; std::set<StableOccurrenceKey> changed;
    bind_source(*s, old, changed, work);
    std::set<NativeRouteKey> affected;
    const auto candidate_limit = old ? limits.max_candidate_routes : std::min(limits.max_routes, limits.max_candidate_routes);
    if (old) {
        // Candidate maps/sets are physical indexes. Copying their topology is
        // currently whole-index work; expensive exact route products are shared.
        work.take(old->routes.size());
        s->routes = old->routes; s->reverse = old->reverse; s->blocks = old->blocks; s->grade_routes = old->grade_routes;
        for (const auto& [key, r] : s->routes) { (void)key; add_entries(*s, r->contribution.nonzeros()); }
        for (const auto& [key, b] : s->blocks) { (void)key; add_entries(*s, b->sum.nonzeros()); }
        for (auto key : changed) {
            if (const auto it = old->reverse.find(key); it != old->reverse.end())
                for (auto r : it->second) candidate(affected, r, candidate_limit, work);
            add_candidates(*s, key, affected, candidate_limit, work);
        }
    } else {
        for (const auto& [key, leg] : s->legs) {
            (void)leg;
            if (key.upper_grade.value >= 2) {
                const auto it = s->outgoing.find(s->legs.at(key).lower);
                if (it != s->outgoing.end()) for (auto lower : it->second) candidate(affected, {key, lower}, candidate_limit, work);
            }
        }
    }
    s->stats.candidate_routes = affected.size();
    MutableBlocks edits;
    for (auto key : affected) remove_route(*s, edits, key, work);
    const auto reused = s->routes.size();
    for (auto key : affected) if (valid_route(*s, key)) insert_route(*s, edits, key, work);
    s->stats.reused_routes = reused; s->stats.routes = s->routes.size();
    update_queries(*s, old, queries, affected, work);
    std::size_t entries = 0;
    const auto count_entries = [&](std::size_t n) {
        entries = machine::checked_size_add(entries, n); bound(entries, limits.max_total_nonzeros, "aggregate defect exact-entry budget");
    };
    for (const auto& [key, leg] : s->legs) { (void)key; count_entries(leg.map->nonzeros()); }
    for (const auto& [key, route] : s->routes) { (void)key; count_entries(route->contribution.nonzeros()); }
    for (const auto& [key, block] : s->blocks) { (void)key; count_entries(block->sum.nonzeros()); }
    for (const auto& [key, c] : s->certificates) { (void)key; for (const auto& [bk, m] : c->sums) { (void)bk; count_entries(m.nonzeros()); } }
    require(entries == s->exact_entries, "defect cache accounting mismatch");
    s->stats.work = work.used; return s;
}
} // namespace native_defect_detail

// A pin is an immutable publication. It survives advances and owner destruction.
// References returned by lvalue accessors borrow this pin, not a mutable store.
class NativeDefectView {
public:
    std::uint64_t sequence() const { return state_->endpoint->sequence; }
    std::uint64_t name_space() const { return state_->name_space; }
    std::shared_ptr<const TemporalEndpoint> endpoint() const { return state_->endpoint; }
    NativeDefectStats stats() const { return state_->stats; }
    std::size_t route_count() const { return state_->routes.size(); }
    bool contains(NativeRouteKey key) const { return state_->routes.contains(key); }
    const NativeRouteContribution& route(NativeRouteKey key) const & { return *state_->routes.at(key); }
    const NativeRouteContribution& route(NativeRouteKey) const && = delete;
    const NativeDefectCertificate& certificate(std::uint64_t key) const & { return *state_->certificates.at(key); }
    const NativeDefectCertificate& certificate(std::uint64_t) const && = delete;
    std::vector<NativeRouteKey> route_keys() const {
        std::vector<NativeRouteKey> out; out.reserve(state_->routes.size());
        for (const auto& [key, r] : state_->routes) { (void)r; out.push_back(key); } return out;
    }
    // Numeric zero and a missing route/block remain different. An endpoint block
    // with no retained two-step routes is represented by an empty optional.
    std::optional<numeric::ExactMatrix> block(NativeDefectBlockKey key) const {
        const auto it = state_->blocks.find(key); if (it == state_->blocks.end()) return std::nullopt;
        return it->second->sum;
    }
    bool flat(core::Grade grade) const {
        native_defect_detail::require(grade.value >= 2 && grade.value < state_->endpoint->source.grade_count(), "defect interface grade absent");
        for (const auto& [key, b] : state_->blocks) if (key.upper.grade == grade && b->sum.nonzeros()) return false;
        return true;
    }
    // Complete current word maps, not just their value on the current field.
    // Persistent topology is required for the same route to be compared.
    temporal::OperationWordDefect word_defect_to(const NativeDefectView& next, NativeRouteKey key,
        const std::vector<TemporalCorrespondence>& maps = {}) const {
        using namespace native_defect_detail;
        require(name_space() == next.name_space(), "route comparison source namespace mismatch");
        require(state_->realization.kind == next.state_->realization.kind, "route comparison realization kinds differ");
        const auto& old_route = *state_->routes.at(key); const auto& new_route = *next.state_->routes.at(key);
        require(old_route.upper == new_route.upper && old_route.middle == new_route.middle && old_route.lower == new_route.lower,
                "route reattachment is not a persistent local word");
        temporal_endpoint_detail::CarrierCorrespondenceResolver resolver(*state_->endpoint, *next.state_->endpoint, maps);
        const auto comparison = [&](StableCellKey c) {
            const auto& a = state_->cells.at(c); const auto& b = next.state_->cells.at(c);
            if (!a.carrier) return Matrix::identity(1);
            require(a.carrier->key == b.carrier->key, "route comparison requires persistent selected carrier identity");
            return resolver.get(b.carrier->key);
        };
        auto Ju = comparison(old_route.upper), Jm = comparison(old_route.middle), Jl = comparison(old_route.lower);
        Work work{next.state_->limits};
        const auto A = effective_map(state_->legs.at(key.upper)), B = effective_map(state_->legs.at(key.lower));
        const auto Ap = effective_map(next.state_->legs.at(key.upper)), Bp = effective_map(next.state_->legs.at(key.lower));
        const auto subtract = [&](Matrix a, const Matrix& b) { accumulate(a, b, -1, work); return a; };
        auto Ea = subtract(multiply(Ap, Ju, work), multiply(Jm, A, work));
        auto Eb = subtract(multiply(Bp, Jm, work), multiply(Jl, B, work));
        auto first = multiply(Bp, Ea, work), second = multiply(Eb, A, work);
        auto expanded = first; accumulate(expanded, second, 1, work);
        auto direct = subtract(multiply(new_route.contribution, Ju, work), multiply(Jl, old_route.contribution, work));
        return {std::move(direct), std::move(expanded), {std::move(Ea), std::move(Eb)}, {std::move(first), std::move(second)}};
    }
private:
    friend class NativeDefectStore;
    explicit NativeDefectView(std::shared_ptr<const native_defect_detail::State> state) : state_(std::move(state)) {}
    std::shared_ptr<const native_defect_detail::State> state_;
};

// Downstream accelerator over the paper's retained source and existing endpoint
// realizations. E owns observations/replay; both publications are staged together.
// No history, schema language, matrix-only source or persistence grammar is added.
class NativeDefectStore {
public:
    NativeDefectStore(const TemporalEndpoint& endpoint, std::uint64_t name_space,
        NativeDefectRealization realization = {}, std::vector<NativeDefectQuery> queries = {},
        NativeDefectLimits limits = {}, std::vector<RelationalCodeObservationQuery> observations = {},
        RelationalCodeObservationLimits observation_limits = {}) : limits_(limits) {
        native_defect_detail::require(name_space != 0, "defect source namespace required");
        auto store = std::make_unique<RelationalCodeObservationStore>(endpoint, std::move(observations),
            [](auto bytes) { return rfcx_detail::fnv1a64(bytes); }, observation_limits);
        auto state = native_defect_detail::build(store->pin_snapshot(), name_space, std::move(realization), queries, limits, nullptr);
        observations_ = std::move(store); state_ = std::move(state);
    }
    NativeDefectStore(const NativeDefectStore&) = delete;
    NativeDefectStore& operator=(const NativeDefectStore&) = delete;
    NativeDefectStore(NativeDefectStore&&) = delete;
    NativeDefectStore& operator=(NativeDefectStore&&) = delete;
    NativeDefectView pin() const { return NativeDefectView(state_); }
    NativeDefectStats stats() const { return state_->stats; }
    RelationalCodeObservationStats observation_stats() const { return observations_->stats(); }
    std::shared_ptr<const RelationalCodeObservation> observation(RelationalCodeIdentity id) const { return observations_->pin_observation(id); }
    NativeDefectStats advance(const TemporalSegment& segment,
        std::optional<NativeDefectRealization> realization = {},
        std::optional<std::vector<NativeDefectQuery>> queries = {},
        std::optional<std::vector<RelationalCodeObservationQuery>> observations = {}) {
        auto candidate = observations_->fork(); candidate->advance(segment, std::move(observations));
        auto state = native_defect_detail::build(candidate->pin_snapshot(), state_->name_space,
            realization ? *realization : state_->realization, queries ? *queries : current_queries(), limits_, state_.get());
        observations_.swap(candidate); state_.swap(state); return state_->stats;
    }
    NativeDefectStats reconfigure(std::vector<NativeDefectQuery> queries,
                                 std::optional<NativeDefectRealization> realization = {}) {
        auto state = native_defect_detail::build(observations_->pin_snapshot(), state_->name_space,
            realization ? *realization : state_->realization, queries, limits_, state_.get());
        state_.swap(state); return state_->stats;
    }
private:
    std::vector<NativeDefectQuery> current_queries() const {
        std::vector<NativeDefectQuery> out; out.reserve(state_->queries.size());
        for (const auto& [key, q] : state_->queries) { (void)key; out.push_back(q.query); } return out;
    }
    NativeDefectLimits limits_;
    std::unique_ptr<RelationalCodeObservationStore> observations_;
    std::shared_ptr<const native_defect_detail::State> state_;
};
} // namespace rfc::storage
