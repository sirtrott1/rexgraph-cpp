#pragma once

#include "rfc/inference/relational_finite_source.hpp"
#include <limits>
#include <span>

namespace rfc::inference {

struct RelationalFiniteProjectionDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<core::CellId> cells;
    // Absent means the complete original coordinate carriers. An explicitly
    // empty coordinate window denotes an empty image, not missing coverage.
    std::optional<std::vector<std::vector<std::uint64_t>>> window;
    OwnedRelationalFiniteSource::Assignment fixed;
    std::vector<core::RelationId> distinct_relations;
};

struct RelationalFiniteInterfaceBijection {
    std::uint64_t key{}, revision{};
    // Output port i is compared with right port right_positions[i]. Symbols
    // list its right values in the complete original left-carrier order.
    std::vector<std::size_t> right_positions;
    std::vector<std::vector<std::uint64_t>> right_symbols;
};

struct RelationalFiniteFamilyLimits {
    std::size_t max_variables{65536}, max_steps{64000000}, max_entries{1u << 24};
    std::size_t max_candidates{65536}, max_rows{65536};
};

struct RelationalFiniteFamilyWork {
    std::size_t steps{}, peak_entries{}, candidates{}, completed_queries{};
    std::size_t preparations{}, source_queries{}, rows{}, witness_values{};
    std::size_t reused_preparations{}, prior_preparation_steps{};
    std::size_t reused_propagations{}, prior_propagation_steps{}, continued_source_queries{};
    std::size_t propagations{}, pruned_values{};
    local::FiniteSearchReceipt native;
};

enum class RelationalFiniteImageStatus { Complete, BudgetExceeded, Cancelled };

struct RelationalFiniteImagePropagation {
    std::size_t leaf{};
    local::FinitePropagationStatus status{local::FinitePropagationStatus::BudgetExceeded};
    std::optional<OwnedRelationalFinitePropagation> snapshot;
    // Executed work here; an incoming or shared root has no new propagation work.
    local::FiniteSearchReceipt work;
    bool reused{};
};

// Exclusions only narrow enumeration, never the retained family declaration.
// A fixed point preserves all global sections; its domains are not independent
// witnesses. Each surviving tuple still needs a complete source query.
struct RelationalFiniteImagePruning {
    std::vector<std::vector<std::uint64_t>> window;
    std::vector<RelationalFiniteImagePropagation> propagations;
    std::optional<std::size_t> unsatisfiable_leaf;
    bool complete{};
};

namespace finite_family_detail {
using Id = std::uint64_t;
using Assignment = OwnedRelationalFiniteSource::Assignment;
inline std::size_t add(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a)
        throw std::length_error("finite family size overflow");
    return a + b;
}
inline bool less(core::CellId a, core::CellId b) {
    return a.grade.value < b.grade.value || (a.grade == b.grade && a.index < b.index);
}
inline const std::vector<Id> &domain(const OwnedRelationalFiniteSource &s, core::CellId c,
                                     numeric::ExactActionGuard &guard) {
    for (const auto &v : s.declaration().variables) {
        guard.step();
        if (v.cell == c)
            return v.domain;
    }
    throw std::invalid_argument("finite interface cell is not a source variable");
}
inline std::size_t find(const std::vector<Id> &values, Id value, numeric::ExactActionGuard &guard) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        guard.step();
        if (values[i] == value)
            return i;
    }
    throw std::invalid_argument("finite interface symbol is outside its original carrier");
}
struct Leaf {
    OwnedRelationalFiniteSource source;
    RelationalFiniteProjectionDeclaration declaration;
    std::vector<core::CellId> cells;
    std::vector<std::vector<Id>> symbols;
    std::vector<std::pair<core::CellId, Id>> fixed;
    std::optional<PreparedRelationalFiniteSearch> prepared;
    std::optional<OwnedRelationalFinitePropagation> propagation;
};
struct State {
    std::uint64_t key{}, revision{};
    OwnedRelationalFiniteSource anchor;
    std::vector<core::CellId> cells;
    std::vector<std::vector<Id>> domains, window;
    std::vector<Leaf> leaves;
    std::vector<std::shared_ptr<const State>> parents;
    std::optional<RelationalFiniteInterfaceBijection> comparison;
    std::size_t retained{};
    explicit State(OwnedRelationalFiniteSource s) : anchor(std::move(s)) {}
};
inline void account(State &s, std::size_t n, numeric::ExactActionGuard &guard) {
    s.retained = add(s.retained, n);
    guard.entries(s.retained);
    guard.step();
}
inline std::size_t leaf_words(const Leaf &leaf, numeric::ExactActionGuard &guard) {
    auto n = add(32, add(leaf.cells.size(), leaf.declaration.cells.size()));
    if (leaf.propagation)
        n = add(n, 4);
    n = add(n, leaf.declaration.distinct_relations.size());
    n = add(n, add(add(leaf.fixed.size(), leaf.fixed.size()),
                   add(leaf.declaration.fixed.size(), leaf.declaration.fixed.size())));
    for (const auto &values : leaf.symbols) {
        guard.step();
        n = add(n, add(1, values.size()));
    }
    if (leaf.declaration.window)
        for (const auto &values : *leaf.declaration.window) {
            guard.step();
            n = add(n, add(1, values.size()));
        }
    return n;
}
struct Interrupted {
    local::FiniteSearchStatus status;
};

// Entries are a conservative logical envelope for request metadata, prepared
// indexes, action scratch and output evidence. Already owned source bytes/Value
// storage and allocator/RSS overhead are separate, as in the native solver.
class Action {
  public:
    Action(const State &state, RelationalFiniteFamilyLimits limits, std::stop_token stop)
        : state_(state), limits_(limits), stop_(stop) {}
    RelationalFiniteFamilyWork work;
    void initialize() {
        keep(state_.retained);
    }
    void step(std::size_t n = 1) {
        if (stop_.stop_requested())
            throw Interrupted{local::FiniteSearchStatus::Cancelled};
        if (n > limits_.max_steps - work.steps)
            throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        work.steps += n;
    }
    void observe(std::size_t temporary = 0) {
        if (temporary > limits_.max_entries - retained_)
            throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        work.peak_entries = std::max(work.peak_entries, retained_ + temporary);
    }
    void keep(std::size_t n) {
        step();
        observe(n);
        retained_ += n;
    }
    void drop(std::size_t n) noexcept {
        retained_ -= n;
    }
    struct Hold {
        Action &action;
        std::size_t entries{};
        explicit Hold(Action &a) : action(a) {}
        Hold(const Hold &) = delete;
        Hold &operator=(const Hold &) = delete;
        void keep(std::size_t n) {
            action.keep(n);
            entries = add(entries, n);
        }
        std::size_t disown() noexcept {
            const auto n = entries;
            entries = 0;
            return n;
        }
        ~Hold() {
            action.drop(entries);
        }
    };
    void prepare() {
        keep(add(state_.leaves.size(), add(state_.leaves.size(), state_.leaves.size())));
        prepared_.reserve(state_.leaves.size());
        roots_.reserve(state_.leaves.size());
        for (const auto &leaf : state_.leaves) {
            step();
            const auto li = prepared_.size();
            auto selected = leaf.prepared;
            if (!selected) {
                for (std::size_t i = 0; i < prepared_.size(); ++i) {
                    step();
                    if (leaf.source.same_owner(state_.leaves[i].source) &&
                        leaf.declaration.distinct_relations.size() ==
                            state_.leaves[i].declaration.distinct_relations.size()) {
                        step(leaf.declaration.distinct_relations.size());
                        if (leaf.declaration.distinct_relations ==
                            state_.leaves[i].declaration.distinct_relations) {
                            selected = prepared_[i];
                            break;
                        }
                    }
                }
            }
            if (selected) {
                ++work.reused_preparations;
                bool already_retained = false;
                for (const auto &previous : prepared_) {
                    step();
                    already_retained = already_retained || selected->same_owner(previous);
                }
                if (!already_retained) {
                    if (leaf.prepared)
                        work.prior_preparation_steps =
                            add(work.prior_preparation_steps, selected->preparation_work().steps);
                    keep(selected->retained_entries());
                }
                prepared_.push_back(std::move(*selected));
                roots_.push_back(leaf.propagation);
                retain_propagation(li);
                continue;
            }
            ++work.preparations;
            auto p = leaf.source.prepare_global_section(leaf.declaration.distinct_relations, native_limits(),
                                                        stop_);
            native(p.work);
            if (!p.prepared)
                throw Interrupted{p.status == local::FinitePreparationStatus::Cancelled
                                      ? local::FiniteSearchStatus::Cancelled
                                      : local::FiniteSearchStatus::BudgetExceeded};
            keep(p.prepared->retained_entries());
            prepared_.push_back(std::move(*p.prepared));
            roots_.push_back(leaf.propagation);
            retain_propagation(li);
        }
    }
    void begin_enumeration() {
        keep(8);
        pruning = std::make_shared<RelationalFiniteImagePruning>();
        for (const auto &values : state_.window) {
            keep(add(values.size(), 3));
            pruning->window.push_back(values);
        }
    }
    bool prune_enumeration() {
        prepare();
        for (std::size_t li = 0; li < state_.leaves.size(); ++li) {
            const auto &leaf = state_.leaves[li];
            keep(32);
            RelationalFiniteImagePropagation evidence;
            evidence.leaf = li;
            if (!roots_[li]) {
                for (std::size_t previous = 0; previous < li; ++previous) {
                    step();
                    if (prepared_[li].same_owner(prepared_[previous]) &&
                        leaf.fixed.size() == state_.leaves[previous].fixed.size()) {
                        step(leaf.fixed.size());
                        if (leaf.fixed == state_.leaves[previous].fixed) {
                            roots_[li] = roots_[previous];
                            ++work.reused_propagations;
                            break;
                        }
                    }
                }
            }
            evidence.reused = roots_[li].has_value();
            if (roots_[li]) {
                evidence.status = local::FinitePropagationStatus::FixedPoint;
                evidence.snapshot = roots_[li];
            } else {
                ++work.propagations;
                auto p = prepared_[li].propagate(leaf.declaration.fixed, native_limits(), stop_);
                native(p.work);
                evidence.status = p.status;
                evidence.work = p.work;
                if (p.snapshot) {
                    keep(p.snapshot->retained_entries());
                    roots_[li] = std::move(p.snapshot);
                    evidence.snapshot = roots_[li];
                }
            }
            pruning->propagations.push_back(std::move(evidence));
            const auto status = pruning->propagations.back().status;
            if (status == local::FinitePropagationStatus::Cancelled ||
                status == local::FinitePropagationStatus::BudgetExceeded)
                throw Interrupted{status == local::FinitePropagationStatus::Cancelled
                                      ? local::FiniteSearchStatus::Cancelled
                                      : local::FiniteSearchStatus::BudgetExceeded};
            if (status == local::FinitePropagationStatus::Unsatisfiable) {
                pruning->unsatisfiable_leaf = li;
                return false;
            }
        }
        for (std::size_t i = 0; i < pruning->window.size(); ++i) {
            auto &values = pruning->window[i];
            std::size_t kept{};
            for (const auto value : values) {
                const auto at = position(state_.domains[i], value);
                bool admitted = true;
                for (std::size_t li = 0; li < state_.leaves.size(); ++li) {
                    step();
                    const auto &leaf = state_.leaves[li];
                    const auto &domain_values = roots_[li]->domains().at(leaf.cells[i]);
                    bool present = false;
                    for (const auto symbol : domain_values) {
                        step();
                        present = present || symbol == leaf.symbols[i][at];
                    }
                    admitted = admitted && present;
                }
                if (admitted)
                    values[kept++] = value;
                else
                    ++work.pruned_values;
            }
            values.resize(kept);
        }
        return true;
    }
    std::shared_ptr<RelationalFiniteImagePruning> pruning;
    struct Query {
        local::FiniteSearchStatus status;
        std::vector<Assignment> fields;
        std::size_t retained{}, source_queries{}, native_steps{};
        std::optional<std::size_t> decisive_leaf;
    };
    Query query(std::span<const Id> tuple) {
        if (tuple.size() != state_.cells.size())
            throw std::invalid_argument("finite interface tuple arity");
        step();
        if (work.candidates == limits_.max_candidates)
            throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        ++work.candidates;
        const auto calls = work.source_queries, native_steps = work.native.steps;
        Hold positions_held(*this);
        positions_held.keep(tuple.size());
        std::vector<std::size_t> positions;
        positions.reserve(tuple.size());
        for (std::size_t i = 0; i < tuple.size(); ++i) {
            positions.push_back(position(state_.domains[i], tuple[i]));
            bool admitted = false;
            for (const auto v : state_.window[i]) {
                step();
                admitted = admitted || v == tuple[i];
            }
            if (!admitted) {
                ++work.completed_queries;
                return {local::FiniteSearchStatus::Unsatisfiable, {}, 0, 0, 0, {}};
            }
        }
        if (prepared_.empty())
            prepare();
        Hold held(*this);
        held.keep(state_.leaves.size());
        std::vector<Assignment> fields;
        fields.reserve(state_.leaves.size());
        for (std::size_t li = 0; li < state_.leaves.size(); ++li) {
            const auto &leaf = state_.leaves[li];
            Hold scratch(*this);
            scratch.keep(add(add(leaf.fixed.size(), tuple.size()), add(leaf.fixed.size(), tuple.size())));
            Assignment fixed;
            fixed.reserve(add(leaf.fixed.size(), tuple.size()));
            for (const auto &[cell, value] : leaf.fixed) {
                step();
                fixed.emplace(cell, value);
            }
            for (std::size_t i = 0; i < tuple.size(); ++i) {
                step();
                const auto value = leaf.symbols[i][positions[i]];
                const auto [at, inserted] = fixed.emplace(leaf.cells[i], value);
                if (!inserted && at->second != value) {
                    ++work.completed_queries;
                    return {local::FiniteSearchStatus::Unsatisfiable, {}, 0, work.source_queries - calls,
                            work.native.steps - native_steps,         li};
                }
            }
            step();
            auto result = [&] {
                if (!roots_[li]) {
                    ++work.source_queries;
                    return prepared_[li].global_section(fixed, native_limits(), stop_);
                }
                // The checkpoint already includes this exact common request.
                // Keep tuple conflicts above, then submit only new observations.
                for (const auto &[cell, value] : leaf.fixed) {
                    (void)value;
                    step();
                    fixed.erase(cell);
                }
                ++work.source_queries;
                ++work.continued_source_queries;
                return roots_[li]->continue_global_section(fixed, native_limits(), stop_);
            }();
            native(result.work);
            if (result.status == local::FiniteSearchStatus::BudgetExceeded ||
                result.status == local::FiniteSearchStatus::Cancelled)
                throw Interrupted{result.status};
            if (!result.witness) {
                ++work.completed_queries;
                return {local::FiniteSearchStatus::Unsatisfiable, {}, 0, work.source_queries - calls,
                        work.native.steps - native_steps,         li};
            }
            held.keep(add(result.witness->size(), result.witness->size()));
            fields.push_back(std::move(*result.witness));
        }
        ++work.completed_queries;
        return {local::FiniteSearchStatus::Satisfiable,
                std::move(fields),
                held.disown(),
                work.source_queries - calls,
                work.native.steps - native_steps,
                {}};
    }
    const RelationalFiniteFamilyLimits &limits() const noexcept {
        return limits_;
    }
    local::FiniteSearchLimits native_bounds() const {
        return native_limits();
    }
    void record_native(const local::FiniteSearchReceipt &receipt) {
        native(receipt);
    }

  private:
    const State &state_;
    RelationalFiniteFamilyLimits limits_;
    std::stop_token stop_;
    std::size_t retained_{};
    std::vector<PreparedRelationalFiniteSearch> prepared_;
    std::vector<std::optional<OwnedRelationalFinitePropagation>> roots_;
    void retain_propagation(std::size_t li) {
        const auto &root = state_.leaves[li].propagation;
        if (!root)
            return;
        step();
        ++work.reused_propagations;
        for (std::size_t previous = 0; previous < li; ++previous) {
            step();
            const auto &other = state_.leaves[previous].propagation;
            if (other && root->same_owner(*other))
                return;
        }
        work.prior_propagation_steps = add(work.prior_propagation_steps, root->propagation_work().steps);
        keep(root->retained_entries());
    }
    local::FiniteSearchLimits native_limits() const {
        return {limits_.max_variables, limits_.max_steps - work.steps, limits_.max_entries - retained_};
    }
    std::size_t position(const std::vector<Id> &domain_values, Id value) {
        for (std::size_t i = 0; i < domain_values.size(); ++i) {
            step();
            if (domain_values[i] == value)
                return i;
        }
        throw std::invalid_argument("finite interface symbol is outside its original carrier");
    }
    void native(const local::FiniteSearchReceipt &r) {
        // Account even an interrupted native action, without an extra checkpoint
        // that could discard its final receipt when the stop token changes.
        if (r.steps > limits_.max_steps - work.steps)
            throw std::logic_error("native finite work exceeded admission");
        work.steps += r.steps;
        observe(r.peak_entries);
        auto &n = work.native;
        n.steps = add(n.steps, r.steps);
        n.value_trials = add(n.value_trials, r.value_trials);
        n.chart_tests = add(n.chart_tests, r.chart_tests);
        n.tuple_tests = add(n.tuple_tests, r.tuple_tests);
        n.coordinate_tests = add(n.coordinate_tests, r.coordinate_tests);
        n.variables = std::max(n.variables, r.variables);
        n.peak_assigned = std::max(n.peak_assigned, r.peak_assigned);
        n.peak_entries = std::max(n.peak_entries, r.peak_entries);
        n.domain_reductions = add(n.domain_reductions, r.domain_reductions);
        n.search_nodes = add(n.search_nodes, r.search_nodes);
        n.distinct_groups = std::max(n.distinct_groups, r.distinct_groups);
        n.matching_tests = add(n.matching_tests, r.matching_tests);
    }
};
} // namespace finite_family_detail

class RelationalFiniteFamilyExtension;
class OwnedRelationalFiniteImage;
struct RelationalFiniteFamilyExtensionResult;
struct RelationalFiniteImageResult;

class OwnedRelationalFiniteFamily {
    using State = finite_family_detail::State;

  public:
    static OwnedRelationalFiniteFamily projection(OwnedRelationalFiniteSource source,
                                                  RelationalFiniteProjectionDeclaration declaration,
                                                  numeric::ExactActionLimits limits = {},
                                                  std::function<void()> check = {}) {
        return projection_impl(std::move(source), {}, {}, std::move(declaration), limits, std::move(check));
    }
    static OwnedRelationalFiniteFamily projection(PreparedRelationalFiniteSearch prepared,
                                                  RelationalFiniteProjectionDeclaration declaration,
                                                  numeric::ExactActionLimits limits = {},
                                                  std::function<void()> check = {}) {
        field_program_detail::require(prepared.relations() == declaration.distinct_relations,
                                      "finite projection request differs from the prepared group order");
        const auto source = prepared.source();
        return projection_impl(source, std::move(prepared), {}, std::move(declaration), limits,
                               std::move(check));
    }
    static OwnedRelationalFiniteFamily projection(OwnedRelationalFinitePropagation propagation,
                                                  RelationalFiniteProjectionDeclaration declaration,
                                                  numeric::ExactActionLimits limits = {},
                                                  std::function<void()> check = {}) {
        field_program_detail::require(propagation.prepared().relations() == declaration.distinct_relations &&
                                          propagation.fixed() == declaration.fixed,
                                      "finite projection request differs from its propagation checkpoint");
        const auto source = propagation.source();
        const auto prepared = propagation.prepared();
        return projection_impl(source, prepared, std::move(propagation), std::move(declaration), limits,
                               std::move(check));
    }

  private:
    static OwnedRelationalFiniteFamily
    projection_impl(OwnedRelationalFiniteSource source,
                    std::optional<PreparedRelationalFiniteSearch> prepared,
                    std::optional<OwnedRelationalFinitePropagation> propagation,
                    RelationalFiniteProjectionDeclaration declaration, numeric::ExactActionLimits limits,
                    std::function<void()> check) {
        using namespace finite_family_detail;
        auto s = std::make_shared<State>(std::move(source));
        auto d = declaration; // Moved input storage may still have caller-held aliases.
        numeric::ExactActionGuard guard(limits, std::move(check));
        field_program_detail::require(d.key && d.revision, "finite projection identity/revision");
        guard.dimension(d.cells.size());
        field_program_detail::require(!d.window || d.window->size() == d.cells.size(),
                                      "finite projection window arity");
        s->key = d.key;
        s->revision = d.revision;
        account(*s, 8, guard);
        std::set<core::CellId, decltype(&less)> seen(&less);
        for (std::size_t i = 0; i < d.cells.size(); ++i) {
            guard.step();
            field_program_detail::require(seen.insert(d.cells[i]).second,
                                          "finite interface repeats a source cell");
            const auto &values = domain(s->anchor, d.cells[i], guard);
            const auto &window = d.window ? (*d.window)[i] : values;
            account(*s, add(add(values.size(), values.size()), add(window.size(), 4)), guard);
            std::set<Id> unique;
            for (const auto value : window) {
                (void)find(values, value, guard);
                field_program_detail::require(unique.insert(value).second,
                                              "finite interface window repeats a symbol");
            }
            s->cells.push_back(d.cells[i]);
            s->domains.push_back(values);
            s->window.push_back(window);
        }
        Leaf leaf{s->anchor,           std::move(d),          s->cells, s->domains, {},
                  std::move(prepared), std::move(propagation)};
        account(*s,
                add(add(leaf.declaration.cells.size(), leaf.declaration.distinct_relations.size()),
                    add(leaf.declaration.fixed.size(), leaf.declaration.fixed.size())),
                guard);
        if (leaf.declaration.window)
            for (const auto &w : *leaf.declaration.window)
                account(*s, w.size(), guard);
        for (const auto &[cell, value] : leaf.declaration.fixed) {
            (void)find(domain(leaf.source, cell, guard), value, guard);
            leaf.fixed.emplace_back(cell, value);
        }
        std::sort(leaf.fixed.begin(), leaf.fixed.end(),
                  [](const auto &a, const auto &b) { return less(a.first, b.first); });
        account(*s, add(leaf.fixed.size(), leaf.fixed.size()), guard);
        account(*s, leaf_words(leaf, guard), guard);
        s->leaves.push_back(std::move(leaf));
        return OwnedRelationalFiniteFamily(std::move(s));
    }

  public:
    static OwnedRelationalFiniteFamily intersect_images(OwnedRelationalFiniteFamily left,
                                                        OwnedRelationalFiniteFamily right,
                                                        RelationalFiniteInterfaceBijection comparison,
                                                        numeric::ExactActionLimits limits = {},
                                                        std::function<void()> check = {}) {
        using namespace finite_family_detail;
        auto map = comparison;
        const auto &a = *left.state_;
        const auto &b = *right.state_;
        auto s = std::make_shared<State>(a.anchor);
        numeric::ExactActionGuard guard(limits, std::move(check));
        const auto n = a.cells.size();
        guard.dimension(n);
        guard.dimension(add(a.leaves.size(), b.leaves.size()));
        field_program_detail::require(map.key && map.revision && b.cells.size() == n &&
                                          map.right_positions.size() == n && map.right_symbols.size() == n,
                                      "finite interface comparison shape/identity");
        s->key = map.key;
        s->revision = map.revision;
        account(*s, add(add(a.retained, b.retained), 8), guard);
        std::set<std::size_t> used;
        std::vector<std::vector<std::size_t>> positions;
        for (std::size_t i = 0; i < n; ++i) {
            guard.step();
            const auto j = map.right_positions[i];
            field_program_detail::require(
                j < n && used.insert(j).second && map.right_symbols[i].size() == a.domains[i].size() &&
                    a.domains[i].size() == b.domains[j].size(),
                "finite interface comparison is not a complete port/carrier bijection");
            std::vector<std::size_t> indices;
            std::set<Id> unique;
            for (const auto v : map.right_symbols[i]) {
                indices.push_back(find(b.domains[j], v, guard));
                field_program_detail::require(unique.insert(v).second,
                                              "finite interface symbol map is not injective");
            }
            positions.push_back(indices);
            std::vector<Id> window;
            for (const auto v : a.window[i]) {
                const auto k = find(a.domains[i], v, guard);
                bool present = false;
                for (const auto candidate : b.window[j]) {
                    guard.step();
                    present = present || candidate == map.right_symbols[i][k];
                }
                if (present)
                    window.push_back(v);
            }
            s->window.push_back(std::move(window));
            account(*s, add(add(a.domains[i].size(), s->window.back().size()), 3), guard);
        }
        s->cells = a.cells;
        s->domains = a.domains;
        for (const auto &leaf : a.leaves) {
            account(*s, leaf_words(leaf, guard), guard);
            s->leaves.push_back(leaf);
        }
        for (const auto &leaf : b.leaves) {
            account(*s, leaf_words(leaf, guard), guard);
            Leaf next{leaf.source, leaf.declaration, {}, {}, leaf.fixed, leaf.prepared, leaf.propagation};
            for (std::size_t i = 0; i < n; ++i) {
                const auto j = map.right_positions[i];
                next.cells.push_back(leaf.cells[j]);
                std::vector<Id> values;
                for (const auto k : positions[i]) {
                    guard.step();
                    values.push_back(leaf.symbols[j][k]);
                }
                next.symbols.push_back(std::move(values));
            }
            s->leaves.push_back(std::move(next));
        }
        // Flatten execution only; retain both actual parent owners and the
        // complete declared comparison. No equal-source interning is performed.
        s->parents = {left.state_, right.state_};
        s->comparison = std::move(map);
        return OwnedRelationalFiniteFamily(std::move(s));
    }
    const OwnedRelationalFiniteSource &coordinate_source() const & noexcept {
        return state_->anchor;
    }
    const OwnedRelationalFiniteSource &coordinate_source() const && = delete;
    const std::vector<core::CellId> &cells() const & noexcept {
        return state_->cells;
    }
    const std::vector<core::CellId> &cells() const && = delete;
    const std::vector<std::vector<std::uint64_t>> &carriers() const & noexcept {
        return state_->domains;
    }
    const std::vector<std::vector<std::uint64_t>> &carriers() const && = delete;
    const std::vector<std::vector<std::uint64_t>> &window() const & noexcept {
        return state_->window;
    }
    const std::vector<std::vector<std::uint64_t>> &window() const && = delete;
    const OwnedRelationalFiniteSource &source(std::size_t leaf) const & {
        return state_->leaves.at(leaf).source;
    }
    const OwnedRelationalFiniteSource &source(std::size_t) const && = delete;
    const RelationalFiniteProjectionDeclaration &declaration(std::size_t leaf) const & {
        return state_->leaves.at(leaf).declaration;
    }
    const RelationalFiniteProjectionDeclaration &declaration(std::size_t) const && = delete;
    const std::optional<OwnedRelationalFinitePropagation> &propagation(std::size_t leaf) const & {
        return state_->leaves.at(leaf).propagation;
    }
    const std::optional<OwnedRelationalFinitePropagation> &propagation(std::size_t) const && = delete;
    const std::vector<core::CellId> &source_cells(std::size_t leaf) const & {
        return state_->leaves.at(leaf).cells;
    }
    const std::vector<core::CellId> &source_cells(std::size_t) const && = delete;
    const std::vector<std::vector<std::uint64_t>> &source_symbols(std::size_t leaf) const & {
        return state_->leaves.at(leaf).symbols;
    }
    const std::vector<std::vector<std::uint64_t>> &source_symbols(std::size_t) const && = delete;
    const std::optional<RelationalFiniteInterfaceBijection> &comparison() const & {
        return state_->comparison;
    }
    const std::optional<RelationalFiniteInterfaceBijection> &comparison() const && = delete;
    OwnedRelationalFiniteFamily parent(bool right) const {
        return OwnedRelationalFiniteFamily(state_->parents.at(right ? 1 : 0));
    }
    std::size_t leaf_count() const noexcept {
        return state_->leaves.size();
    }
    std::size_t retained_entries() const noexcept {
        return state_->retained;
    }
    std::uint64_t key() const noexcept {
        return state_->key;
    }
    std::uint64_t revision() const noexcept {
        return state_->revision;
    }
    bool same_owner(const OwnedRelationalFiniteFamily &other) const noexcept {
        return state_ == other.state_;
    }
    RelationalFiniteFamilyExtensionResult extend(std::span<const std::uint64_t> tuple,
                                                 RelationalFiniteFamilyLimits limits = {},
                                                 std::stop_token stop = {}) const;
    RelationalFiniteImageResult materialize(RelationalFiniteFamilyLimits limits = {},
                                            std::stop_token stop = {}) const;

  private:
    explicit OwnedRelationalFiniteFamily(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

class RelationalFiniteFamilyExtension {
  public:
    const OwnedRelationalFiniteFamily &family() const & noexcept {
        return state_->family;
    }
    const OwnedRelationalFiniteFamily &family() const && = delete;
    const std::vector<std::uint64_t> &tuple() const & noexcept {
        return state_->tuple;
    }
    const std::vector<std::uint64_t> &tuple() const && = delete;
    const OwnedRelationalFiniteSource::Assignment &witness(std::size_t leaf) const & {
        return state_->fields.at(leaf);
    }
    const OwnedRelationalFiniteSource::Assignment &witness(std::size_t) const && = delete;
    std::size_t leaf_count() const noexcept {
        return state_->fields.size();
    }

  private:
    friend class OwnedRelationalFiniteFamily;
    struct State {
        OwnedRelationalFiniteFamily family;
        std::vector<std::uint64_t> tuple;
        std::vector<OwnedRelationalFiniteSource::Assignment> fields;
    };
    RelationalFiniteFamilyExtension(OwnedRelationalFiniteFamily family, std::vector<std::uint64_t> tuple,
                                    std::vector<OwnedRelationalFiniteSource::Assignment> fields)
        : state_(
              std::make_shared<const State>(State{std::move(family), std::move(tuple), std::move(fields)})) {}
    std::shared_ptr<const State> state_;
};

struct RelationalFiniteFamilyExtensionResult {
    local::FiniteSearchStatus status;
    std::optional<RelationalFiniteFamilyExtension> extension;
    RelationalFiniteFamilyWork work;
    std::optional<std::size_t> decisive_leaf;
};

class OwnedRelationalFiniteImage {
  public:
    const OwnedRelationalFiniteFamily &family() const & noexcept {
        return state_->family;
    }
    const OwnedRelationalFiniteFamily &family() const && = delete;
    std::size_t size() const noexcept {
        return state_->rows.size();
    }
    const RelationalFiniteFamilyExtension &row(std::size_t i) const & {
        return state_->rows.at(i);
    }
    const RelationalFiniteFamilyExtension &row(std::size_t) const && = delete;
    const RelationalFiniteImagePruning &pruning() const & noexcept {
        return *state_->pruning;
    }
    const RelationalFiniteImagePruning &pruning() const && = delete;

  private:
    friend class OwnedRelationalFiniteFamily;
    struct State {
        OwnedRelationalFiniteFamily family;
        std::vector<RelationalFiniteFamilyExtension> rows;
        std::shared_ptr<const RelationalFiniteImagePruning> pruning;
    };
    OwnedRelationalFiniteImage(OwnedRelationalFiniteFamily family,
                               std::vector<RelationalFiniteFamilyExtension> rows,
                               std::shared_ptr<const RelationalFiniteImagePruning> pruning)
        : state_(std::make_shared<const State>(
              State{std::move(family), std::move(rows), std::move(pruning)})) {}
    std::shared_ptr<const State> state_;
};

struct RelationalFiniteImageQuery {
    std::vector<std::uint64_t> tuple;
    local::FiniteSearchStatus status{local::FiniteSearchStatus::BudgetExceeded};
    std::size_t source_queries{}, native_steps{};
    std::optional<std::size_t> decisive_leaf;
};
struct RelationalFiniteImageResult {
    RelationalFiniteImageStatus status{RelationalFiniteImageStatus::BudgetExceeded};
    std::optional<OwnedRelationalFiniteImage> image;
    std::vector<RelationalFiniteImageQuery> queries;
    RelationalFiniteFamilyWork work;
    // An interrupted action can retain a partial recipe, never a complete image.
    std::shared_ptr<const RelationalFiniteImagePruning> pruning;
};

inline RelationalFiniteFamilyExtensionResult
OwnedRelationalFiniteFamily::extend(std::span<const std::uint64_t> tuple, RelationalFiniteFamilyLimits limits,
                                    std::stop_token stop) const {
    using namespace finite_family_detail;
    if (tuple.size() != state_->cells.size())
        throw std::invalid_argument("finite interface tuple arity");
    // The actual owner is pinned before any stop checkpoint. Inputs are borrowed
    // only for this synchronous call, never retained in a result.
    const auto pin = *this;
    std::optional<Action> action;
    try {
        action.emplace(*pin.state_, limits, stop);
        action->initialize();
        action->keep(add(tuple.size(), 32));
        std::vector<Id> frozen(tuple.begin(), tuple.end());
        auto q = action->query(frozen);
        if (q.status != local::FiniteSearchStatus::Satisfiable)
            return {q.status, {}, action->work, q.decisive_leaf};
        if (!limits.max_rows)
            throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        action->work.rows = 1;
        for (const auto &field : q.fields)
            action->work.witness_values = add(action->work.witness_values, field.size());
        return {q.status,
                RelationalFiniteFamilyExtension(pin, std::move(frozen), std::move(q.fields)),
                action->work,
                {}};
    } catch (const Interrupted &e) {
        return {e.status, {}, action ? action->work : RelationalFiniteFamilyWork{}, {}};
    }
}

inline RelationalFiniteImageResult
OwnedRelationalFiniteFamily::materialize(RelationalFiniteFamilyLimits limits, std::stop_token stop) const {
    using namespace finite_family_detail;
    const auto pin = *this;
    RelationalFiniteImageResult result;
    std::optional<Action> action;
    std::optional<std::pair<std::size_t, std::size_t>> pending_query;
    try {
        action.emplace(*pin.state_, limits, stop);
        action->initialize();
        action->keep(add(state_->cells.size(), add(state_->cells.size(), 4)));
        action->begin_enumeration();
        std::vector<RelationalFiniteFamilyExtension> rows;
        const auto finish = [&] {
            action->step();
            action->pruning->complete = true;
            result.status = RelationalFiniteImageStatus::Complete;
            result.pruning = action->pruning;
            result.image = OwnedRelationalFiniteImage(pin, std::move(rows), result.pruning);
            result.work = action->work;
        };
        for (const auto &values : state_->window) {
            action->step();
            if (values.empty()) {
                finish();
                return result;
            }
        }
        if (!action->prune_enumeration()) {
            finish();
            return result;
        }
        const auto &window = action->pruning->window;
        for (const auto &values : window) {
            action->step();
            if (values.empty()) {
                finish();
                return result;
            }
        }
        std::vector<std::size_t> positions(state_->cells.size());
        std::vector<Id> tuple(state_->cells.size());
        bool done = false;
        while (!done) {
            action->step();
            if (action->work.candidates == limits.max_candidates)
                throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
            for (std::size_t i = 0; i < tuple.size(); ++i) {
                action->step();
                tuple[i] = window[i][positions[i]];
            }
            action->keep(add(tuple.size(), 20));
            result.queries.push_back({tuple, local::FiniteSearchStatus::BudgetExceeded, 0, 0, {}});
            pending_query = {{action->work.source_queries, action->work.native.steps}};
            auto q = action->query(tuple);
            auto &record = result.queries.back();
            record.status = q.status;
            record.source_queries = q.source_queries;
            record.native_steps = q.native_steps;
            record.decisive_leaf = q.decisive_leaf;
            pending_query.reset();
            if (q.status == local::FiniteSearchStatus::Satisfiable) {
                if (rows.size() == limits.max_rows)
                    throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
                action->keep(add(tuple.size(), 32));
                for (const auto &field : q.fields)
                    action->work.witness_values = add(action->work.witness_values, field.size());
                rows.push_back(RelationalFiniteFamilyExtension(pin, tuple, std::move(q.fields)));
                ++action->work.rows;
            }
            done = true;
            for (auto i = positions.size(); i > 0; --i) {
                action->step();
                const auto j = i - 1;
                if (++positions[j] < window[j].size()) {
                    done = false;
                    break;
                }
                positions[j] = 0;
            }
        }
        finish();
        return result;
    } catch (const Interrupted &e) {
        result.status = e.status == local::FiniteSearchStatus::Cancelled
                            ? RelationalFiniteImageStatus::Cancelled
                            : RelationalFiniteImageStatus::BudgetExceeded;
        if (!result.queries.empty() &&
            result.queries.back().status == local::FiniteSearchStatus::BudgetExceeded)
            result.queries.back().status = e.status;
        if (pending_query && action) {
            result.queries.back().source_queries = action->work.source_queries - pending_query->first;
            result.queries.back().native_steps = action->work.native.steps - pending_query->second;
        }
        result.work = action ? action->work : RelationalFiniteFamilyWork{};
        result.pruning = action ? action->pruning : nullptr;
        return result;
    }
}

} // namespace rfc::inference
