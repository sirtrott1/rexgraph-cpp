#pragma once

#include "rfc/core/boundary_fiber.hpp"
#include "rfc/local/section.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <stop_token>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rfc::local {

// Bounded native search is an explicit request. Existing unbounded APIs retain
// their original meaning. No partially explored branch is an unsat certificate.
enum class FiniteSearchStatus { Satisfiable, Unsatisfiable, BudgetExceeded, Cancelled };
enum class FinitePreparationStatus { Ready, BudgetExceeded, Cancelled };
struct FiniteSearchLimits {
    std::size_t max_variables{65536};
    std::size_t max_steps{1u << 22};
    // Propagated search admits its dynamic index/flag/reference slots before
    // allocation. This is separate from the retained source and Value storage.
    // Preparation includes its index and temporary scratch; a prepared solve
    // admits only its new workspace. Its index reports retained slots separately.
    std::size_t max_entries{1u << 24};
};
struct FiniteSearchReceipt {
    std::size_t steps{}, value_trials{}, chart_tests{}, tuple_tests{}, coordinate_tests{};
    std::size_t variables{}, peak_assigned{};
    std::size_t peak_entries{}, domain_reductions{}, search_nodes{};
    std::size_t distinct_groups{}, matching_tests{};
};

// Exact finite/set-valued local constraint system over the retained relational
// subcomplex topology.  Value is deliberately unconstrained by linear algebra:
// it may be an integer, a retained relation identity, an enum/state token, or
// another equality-comparable finite value type.
template <class ValueT>
class FiniteConstraintSystem {
public:
    using Value = ValueT;
    using Assignment = BasicSection<Value>;
    using ObstructionKind = LocalObstructionKind;

    struct Chart {
        std::string name;
        algebra::SubcomplexMask patch;
        std::vector<core::CellId> support;
        std::vector<std::vector<Value>> allowed;
    };

    // Exact subset-minimal conflict certificate.  "Subset-minimal" means
    // removing any one retained chart from chart_indices makes the selected
    // constraint family satisfiable.  It is deliberately not advertised as a
    // minimum-cardinality core: finding the smallest possible core would add a
    // second combinatorial optimization problem to exact finite-domain
    // satisfiability.
    struct UnsatCore {
        std::vector<std::size_t> chart_indices;
        std::vector<core::CellId> cells;
        bool subset_minimal{false};
    };

    struct LocalUnsatCore {
        std::size_t patch_index{};
        UnsatCore core;
    };

    struct CoverReport {
        bool every_patch_locally_satisfiable{true};
        bool constraint_complete{true};
        bool global_section_checked{false};
        bool global_section_exists{false};
        ObstructionKind obstruction{ObstructionKind::None};
        LocalObstructionCertificate certificate{};
        std::vector<LocalUnsatCore> local_unsat_cores;
        std::optional<UnsatCore> global_unsat_core;
    };

    explicit FiniteConstraintSystem(const core::RelationalComplex& complex)
        : carrier_(complex) {}

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return carrier_.complex(); }
    [[nodiscard]] const RelationalLocalCarrier& carrier() const noexcept { return carrier_; }

    void set_domain(core::CellId cell, std::vector<Value> values) {
        carrier_.validate_cell(cell);
        if (values.empty()) throw std::invalid_argument("constraint domain must be nonempty");
        std::vector<Value> unique;
        unique.reserve(values.size());
        for (auto& value : values) {
            if (std::find(unique.begin(), unique.end(), value) == unique.end()) unique.push_back(std::move(value));
        }
        invalidate_preparation();
        domains_[cell] = std::move(unique);
    }

    template <class T = Value>
        requires std::is_integral_v<T>
    void set_boolean_domain(core::CellId cell) {
        set_domain(cell, {static_cast<Value>(0), static_cast<Value>(1)});
    }

    [[nodiscard]] const std::vector<Value>& domain(core::CellId cell) const {
        const auto it = domains_.find(cell);
        if (it == domains_.end()) throw std::logic_error("constraint cell has no declared finite domain");
        return it->second;
    }

    [[nodiscard]] const std::unordered_map<core::CellId, std::vector<Value>, CellIdHash>& domains() const noexcept {
        return domains_;
    }

    std::size_t add_chart(std::string name,
                          algebra::SubcomplexMask patch,
                          std::vector<core::CellId> support,
                          std::vector<std::vector<Value>> allowed) {
        carrier_.require_closed_patch(patch);
        if (support.empty()) throw std::invalid_argument("constraint chart support must be nonempty");
        std::unordered_set<core::CellId, CellIdHash> unique;
        for (const auto cell : support) {
            carrier_.validate_cell(cell);
            if (!patch.contains(cell)) throw std::invalid_argument("constraint support lies outside its local patch");
            (void)domain(cell);
            if (!unique.insert(cell).second) throw std::invalid_argument("constraint chart support contains a duplicate cell");
        }
        if (allowed.empty()) throw std::invalid_argument("constraint chart must allow at least one local tuple");
        for (const auto& tuple : allowed) {
            if (tuple.size() != support.size()) throw std::invalid_argument("constraint tuple arity mismatch");
            for (std::size_t i = 0; i < tuple.size(); ++i) {
                if (!domain_contains(support[i], tuple[i])) {
                    throw std::invalid_argument("constraint tuple value is outside the declared local domain");
                }
            }
        }
        invalidate_preparation();
        charts_.push_back(Chart{std::move(name), std::move(patch), std::move(support), std::move(allowed)});
        const auto chart_index = charts_.size() - 1u;
        for (const auto cell : charts_.back().support) charts_by_cell_[cell].push_back(chart_index);
        return chart_index;
    }

    // Packed Boolean predicate: bit index is the little-endian assignment of
    // support bits.  This helper exists only for integral finite values; it is
    // not part of the value-neutral local substrate.
    template <class T = Value>
        requires std::is_integral_v<T>
    std::size_t add_boolean_truth_chart(std::string name,
                                        algebra::SubcomplexMask patch,
                                        std::vector<core::CellId> support,
                                        std::uint64_t truth_table) {
        if (support.size() > 6u) throw std::invalid_argument("uint64 truth-table chart supports at most six Boolean variables");
        for (const auto cell : support) {
            const auto& d = domain(cell);
            if (d.size() != 2u || d[0] != static_cast<Value>(0) || d[1] != static_cast<Value>(1)) {
                throw std::invalid_argument("Boolean truth-table chart requires ordered {0,1} domains");
            }
        }
        std::vector<std::vector<Value>> allowed;
        const auto count = std::size_t{1} << support.size();
        for (std::size_t state = 0; state < count; ++state) {
            if (((truth_table >> state) & std::uint64_t{1}) == 0u) continue;
            std::vector<Value> tuple(support.size(), static_cast<Value>(0));
            for (std::size_t i = 0; i < support.size(); ++i) tuple[i] = static_cast<Value>((state >> i) & 1u);
            allowed.push_back(std::move(tuple));
        }
        return add_chart(std::move(name), std::move(patch), std::move(support), std::move(allowed));
    }

    [[nodiscard]] const std::vector<Chart>& charts() const noexcept { return charts_; }

    [[nodiscard]] Assignment restrict_to(const Assignment& assignment,
                                         const algebra::SubcomplexMask& patch) const {
        return restrict_section(carrier_, assignment, patch);
    }

    [[nodiscard]] bool compatible_on_overlap(const Assignment& left,
                                             const algebra::SubcomplexMask& left_patch,
                                             const Assignment& right,
                                             const algebra::SubcomplexMask& right_patch) const {
        return sections_compatible_on_overlap(carrier_, left, left_patch, right, right_patch,
                                              OverlapCoverage::CompareShared);
    }

    [[nodiscard]] Assignment glue(const Assignment& left,
                                  const algebra::SubcomplexMask& left_patch,
                                  const Assignment& right,
                                  const algebra::SubcomplexMask& right_patch) const {
        return glue_sections(carrier_, left, left_patch, right, right_patch,
                             OverlapCoverage::CompareShared);
    }

    // Boundary participants selected directly from retained occurrence aspects.
    // The aspect is used structurally to select cells; it is not converted into
    // a numerical weight or a pairwise edge view.
    [[nodiscard]] std::vector<core::CellId> relation_aspect_support(core::RelationId relation,
                                                                    composite::AspectId aspect,
                                                                    bool bit_value = true) const {
        validate_relation(relation);
        (void)complex().signature().aspects().at(aspect);
        const auto& arena = complex().grade(relation.grade);
        const auto exists_id = complex().signature().existence_aspect();
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(relation.grade.value - 1u)};
        std::vector<core::CellId> result;
        const auto [begin, end] = arena.incidence_range(relation.index);
        for (std::size_t slot = begin; slot < end; ++slot) {
            if (!arena.aspects.get(exists_id, slot) || arena.aspects.get(aspect, slot) != bit_value) continue;
            const core::CellId lower{lower_grade, arena.lower_index[slot]};
            if (std::find(result.begin(), result.end(), lower) == result.end()) result.push_back(lower);
        }
        return result;
    }

    // Support for one primary relation and its entire active boundary fiber.
    // Shared lower identities remain shared variables; occurrence-local roles
    // are used to define the chart but are not duplicated into fake cells.
    [[nodiscard]] std::vector<core::CellId> relation_fiber_support(core::RelationId relation,
                                                                   bool include_relation = true) const {
        validate_relation(relation);
        std::vector<core::CellId> result;
        if (include_relation) result.push_back(static_cast<core::CellId>(relation));
        const auto& arena = complex().grade(relation.grade);
        const auto exists_id = complex().signature().existence_aspect();
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(relation.grade.value - 1u)};
        const auto [begin, end] = arena.incidence_range(relation.index);
        for (std::size_t slot = begin; slot < end; ++slot) {
            if (!arena.aspects.get(exists_id, slot)) continue;
            const core::CellId lower{lower_grade, arena.lower_index[slot]};
            if (std::find(result.begin(), result.end(), lower) == result.end()) result.push_back(lower);
        }
        return result;
    }

    std::size_t add_relation_fiber_chart(std::string name,
                                         core::RelationId relation,
                                         std::vector<std::vector<Value>> allowed,
                                         bool include_relation = true) {
        const auto support = relation_fiber_support(relation, include_relation);
        auto patch = carrier_.cell_patch(static_cast<core::CellId>(relation));
        return add_chart(std::move(name), std::move(patch), support, std::move(allowed));
    }

    std::size_t add_branch_fiber_chart(std::string name,
                                       core::RelationId relation,
                                       std::vector<std::vector<Value>> allowed,
                                       bool include_relation = true) {
        validate_relation(relation);
        const auto& arena = complex().grade(relation.grade);
        if (!complex().signature().type(arena.relation_types[relation.index]).is_branching()) {
            throw std::invalid_argument("branch-fiber chart requires a branching primary relation");
        }
        return add_relation_fiber_chart(std::move(name), relation, std::move(allowed), include_relation);
    }

    std::size_t add_relation_aspect_chart(std::string name,
                                          core::RelationId relation,
                                          composite::AspectId aspect,
                                          bool bit_value,
                                          std::vector<std::vector<Value>> allowed) {
        const auto support = relation_aspect_support(relation, aspect, bit_value);
        if (support.empty()) throw std::invalid_argument("relation-aspect chart selected no boundary cells");
        auto patch = carrier_.cell_patch(static_cast<core::CellId>(relation));
        return add_chart(std::move(name), std::move(patch), support, std::move(allowed));
    }

    [[nodiscard]] bool valid_on_patch(const Assignment& assignment,
                                      const algebra::SubcomplexMask& patch) const {
        carrier_.require_closed_patch(patch);
        for (const auto& chart : charts_) {
            if (!support_inside(chart.support, patch)) continue;
            if (!chart_satisfied(chart, assignment, true)) return false;
        }
        return true;
    }

    [[nodiscard]] bool globally_valid(const Assignment& assignment) const {
        for (const auto& [cell, values] : domains_) {
            const auto it = assignment.find(cell);
            if (it == assignment.end()) return false;
            if (std::find(values.begin(), values.end(), it->second) == values.end()) return false;
        }
        for (const auto& chart : charts_) if (!chart_satisfied(chart, assignment, true)) return false;
        return true;
    }

    [[nodiscard]] std::optional<Assignment> global_section() const {
        std::vector<core::CellId> variables;
        variables.reserve(domains_.size());
        for (const auto& [cell, _] : domains_) variables.push_back(cell);
        std::sort(variables.begin(), variables.end(), [](const auto a, const auto b) {
            if (a.grade.value != b.grade.value) return a.grade.value < b.grade.value;
            return a.index < b.index;
        });
        std::stable_sort(variables.begin(), variables.end(), [&](const auto a, const auto b) {
            const auto ad = domain(a).size();
            const auto bd = domain(b).size();
            if (ad != bd) return ad < bd;
            const auto ac = chart_degree(a);
            const auto bc = chart_degree(b);
            return ac > bc;
        });
        Assignment current;
        if (search(variables, 0u, current)) return current;
        return std::nullopt;
    }

    struct BoundedSearchResult {
        FiniteSearchStatus status;
        std::optional<Assignment> witness;
        FiniteSearchReceipt work;
    };

private:
    struct CompiledChart { std::vector<std::size_t> support, tuples; };
    struct PropagationIndexData {
        std::shared_ptr<const unsigned char> generation;
        std::vector<core::CellId> variables;
        std::vector<std::size_t> offsets, symbol;
        std::vector<CompiledChart> compiled;
        std::vector<std::vector<std::size_t>> groups, groups_by_variable;
        std::vector<core::RelationId> relations;
        std::size_t symbol_count{}, retained_entries{};
        FiniteSearchReceipt preparation;
    };
public:
    // Compiled positions refer to this system generation, including its actual
    // source, domains, charts and requested group order. As with the local
    // carrier, the borrowed complex must remain unchanged and outlive the
    // system. No Value or source pointers escape in the index. A mutable
    // system rejects stale indexes. Value equality must remain stable;
    // owning source consumers separately retain the complete immutable source.
    class PreparedIndex {
    public:
        const std::vector<core::RelationId>& relations() const & noexcept { return data_->relations; }
        const std::vector<core::RelationId>& relations() const && = delete;
        const FiniteSearchReceipt& preparation_work() const & noexcept { return data_->preparation; }
        const FiniteSearchReceipt& preparation_work() const && = delete;
        std::size_t retained_entries() const noexcept { return data_->retained_entries; }
        bool same_index(const PreparedIndex& other) const noexcept { return data_ == other.data_; }
    private:
        friend class FiniteConstraintSystem;
        explicit PreparedIndex(std::shared_ptr<const PropagationIndexData> data) : data_(std::move(data)) {}
        std::shared_ptr<const PropagationIndexData> data_;
    };
    struct PreparationResult {
        FinitePreparationStatus status;
        std::optional<PreparedIndex> prepared;
        FiniteSearchReceipt work;
    };

    // Preparation proves the requested whole-group specialization and compiles
    // exact domain positions once. Ready asserts no satisfiability result.
    // Its admission includes temporary preparation storage; retained_entries
    // reports the index that survives. Later solve receipts count only their
    // private workspace and executed work, not this previously completed work.
    [[nodiscard]] PreparationResult prepare_global_section(
        const std::vector<core::RelationId>& relations = {}, FiniteSearchLimits limits = {},
        std::stop_token stop = {}) const {
        std::shared_ptr<const PropagationIndexData> data;
        auto result = propagated_section({}, limits, stop, relations, nullptr, &data);
        if (data) return {FinitePreparationStatus::Ready, PreparedIndex(std::move(data)), result.work};
        if (result.status == FiniteSearchStatus::Cancelled)
            return {FinitePreparationStatus::Cancelled, std::nullopt, result.work};
        if (result.status == FiniteSearchStatus::BudgetExceeded)
            return {FinitePreparationStatus::BudgetExceeded, std::nullopt, result.work};
        throw std::logic_error("finite preparation completed without its index");
    }
    [[nodiscard]] BoundedSearchResult prepared_global_section(const PreparedIndex& index,
        const Assignment& fixed = {}, FiniteSearchLimits limits = {}, std::stop_token stop = {}) const {
        return propagated_section(fixed, limits, stop, index.data_->relations, index.data_.get());
    }

    // Iterative depth-first search through the SAME retained domains and exact
    // chart predicate as global_section(). All domains and charts participate,
    // even if a caller later projects the witness onto one selected cell.
    // Work counts retained record visits, not CPU instructions or Value costs.
    // The returned witness is absent on interruption. Unexpected exceptions
    // propagate; they never become an unsatisfiability result.
    [[nodiscard]] BoundedSearchResult bounded_global_section(
        const Assignment& fixed = {}, FiniteSearchLimits limits = {},
        std::stop_token stop = {}) const {
        struct Interrupted { FiniteSearchStatus status; };
        FiniteSearchReceipt work;
        const auto tick = [&] {
            if (stop.stop_requested()) throw Interrupted{FiniteSearchStatus::Cancelled};
            if (work.steps >= limits.max_steps) throw Interrupted{FiniteSearchStatus::BudgetExceeded};
            ++work.steps;
        };
        try {
            tick();
            if (domains_.size() > limits.max_variables)
                throw Interrupted{FiniteSearchStatus::BudgetExceeded};
            work.variables = domains_.size();
            std::vector<core::CellId> variables;
            variables.reserve(domains_.size());
            for (const auto& [cell, _] : domains_) { tick(); variables.push_back(cell); }
            std::sort(variables.begin(), variables.end(), [&](auto a, auto b) {
                tick();
                if (domain(a).size() != domain(b).size()) return domain(a).size() < domain(b).size();
                if (chart_degree(a) != chart_degree(b)) return chart_degree(a) > chart_degree(b);
                return a < b;
            });
            for (const auto& [cell, value] : fixed) {
                tick();
                const auto& values = domain(cell);
                bool found = false;
                for (const auto& v : values) { tick(); if (v == value) { found = true; break; } }
                if (!found) throw std::invalid_argument("bounded search fixed value is outside its native domain");
            }
            const auto test_chart = [&](const Chart& chart, const Assignment& a, bool complete) {
                tick(); ++work.chart_tests;
                return chart_satisfied_work(chart, a, complete, [&](bool tuple) {
                    tick(); if (tuple) ++work.tuple_tests; else ++work.coordinate_tests;
                });
            };
            Assignment current = fixed;
            work.peak_assigned = current.size();
            for (const auto& chart : charts_) {
                if (!test_chart(chart, current, false))
                    return {FiniteSearchStatus::Unsatisfiable, std::nullopt, work};
            }
            variables.erase(std::remove_if(variables.begin(), variables.end(), [&](auto cell) {
                tick(); return fixed.contains(cell);
            }), variables.end());
            std::vector<std::size_t> next(variables.size(), 0);
            std::size_t position = 0;
            while (true) {
                tick();
                if (position == variables.size()) {
                    bool valid = true;
                    for (const auto& chart : charts_)
                        if (!test_chart(chart, current, true)) { valid = false; break; }
                    if (valid) { tick(); return {FiniteSearchStatus::Satisfiable, std::move(current), work}; }
                    if (!position) break;
                    --position; current.erase(variables[position]);
                    continue;
                }
                const auto cell = variables[position];
                const auto& values = domain(cell);
                if (next[position] == values.size()) {
                    next[position] = 0; current.erase(cell);
                    if (!position) break;
                    --position; current.erase(variables[position]);
                    continue;
                }
                tick(); ++work.value_trials;
                current.insert_or_assign(cell, values[next[position]++]);
                work.peak_assigned = std::max(work.peak_assigned, current.size());
                bool extendable = true;
                const auto adjacent = charts_by_cell_.find(cell);
                if (adjacent != charts_by_cell_.end())
                    for (const auto ci : adjacent->second)
                        if (!test_chart(charts_[ci], current, false)) { extendable = false; break; }
                if (extendable) ++position;
                else current.erase(cell);
            }
            tick(); return {FiniteSearchStatus::Unsatisfiable, std::nullopt, work};
        } catch (const Interrupted& e) { return {e.status, std::nullopt, work}; }
    }

    // Restrict each variable to values supported by an actual allowed tuple on
    // every adjacent chart, repeating after each restriction. A global section
    // survives every such step. Local consistency alone need not give a global
    // section: iterative search still explores the remaining alternatives.
    // Scratch uses original domain positions, never numerical encodings of Value.
    // Domains, whole charts, patches and the source remain unchanged. Work counts
    // preparation, tuple/coordinate visits, restrictions and rollback; it is not
    // a CPU-instruction or Value-comparison cost bound. An interrupted request
    // returns neither a partial witness nor an unsatisfiability claim.
    [[nodiscard]] BoundedSearchResult propagated_global_section(
        const Assignment& fixed = {}, FiniteSearchLimits limits = {},
        std::stop_token stop = {}) const {
        return propagated_section(fixed, limits, stop, {});
    }
private:
    [[nodiscard]] BoundedSearchResult propagated_section(const Assignment& fixed, FiniteSearchLimits limits,
        std::stop_token stop, const std::vector<core::RelationId>& distinct_relations,
        const PropagationIndexData* cached = nullptr,
        std::shared_ptr<const PropagationIndexData>* capture = nullptr) const {
        struct Interrupted { FiniteSearchStatus status; };
        struct Decision { std::size_t variable, next, mark; };
        FiniteSearchReceipt work;
        const auto tick = [&] {
            if (stop.stop_requested()) throw Interrupted{FiniteSearchStatus::Cancelled};
            if (work.steps >= limits.max_steps) throw Interrupted{FiniteSearchStatus::BudgetExceeded};
            ++work.steps;
        };
        const auto admit = [&](std::size_t count) {
            tick();
            if (count > limits.max_entries - work.peak_entries)
                throw Interrupted{FiniteSearchStatus::BudgetExceeded};
            work.peak_entries += count;
        };
        try {
            tick();
            if (domains_.size() > limits.max_variables)
                throw Interrupted{FiniteSearchStatus::BudgetExceeded};
            if (cached && cached->generation != generation_)
                throw std::invalid_argument("prepared finite index belongs to another system generation");
            const auto n = domains_.size(), m = charts_.size();
            const auto group_count = distinct_relations.size();
            work.variables = n;
            if (cached) {
                // Value references, counts, decisions and the complete witness.
                for (unsigned i = 0; i < 7; ++i) admit(n);
                for (unsigned i = 0; i < 2; ++i) { admit(m); admit(group_count); }
                work.distinct_groups = group_count;
            } else {
                // Variables, domain references, offsets, counts, decision frames
                // and the owning witness's cell/value entries.
                for (unsigned i = 0; i < 9; ++i) admit(n);
                admit(1);
                for (unsigned i = 0; i < 4; ++i) admit(m);
                if (group_count) {
                    for (unsigned i = 0; i < 3; ++i) admit(group_count);
                    admit(n); admit(m);
                }
            }
            PropagationIndexData building;
            const auto& variables = cached ? cached->variables : building.variables;
            const auto& offsets = cached ? cached->offsets : building.offsets;
            const auto& compiled = cached ? cached->compiled : building.compiled;
            const auto& groups = cached ? cached->groups : building.groups;
            const auto& groups_by_variable = cached ? cached->groups_by_variable : building.groups_by_variable;
            const auto& symbol = cached ? cached->symbol : building.symbol;
            if (!cached) {
                building.variables.reserve(n);
                for (const auto& [cell, _] : domains_) { tick(); building.variables.push_back(cell); }
                std::sort(building.variables.begin(), building.variables.end(), [&](auto a, auto b) { tick(); return a < b; });
            }
            const auto position = [&](core::CellId cell) {
                const auto it = std::lower_bound(variables.begin(), variables.end(), cell,
                    [&](auto a, auto b) { tick(); return a < b; });
                if (it == variables.end() || *it != cell)
                    throw std::invalid_argument("propagated search cell has no native domain");
                return static_cast<std::size_t>(it - variables.begin());
            };
            std::vector<const std::vector<Value>*> values;
            std::vector<std::size_t> counts;
            values.reserve(n); counts.reserve(n);
            if (!cached) { building.offsets.push_back(0); building.offsets.reserve(n + 1); }
            for (const auto cell : variables) {
                tick(); const auto& d = domain(cell);
                // Active/support flags and a two-index rollback entry per value.
                for (unsigned i = 0; i < 4; ++i) admit(d.size());
                values.push_back(&d); counts.push_back(d.size());
                if (!cached) building.offsets.push_back(offsets.back() + d.size());
            }
            const auto total = offsets.back();
            const auto queue_size = m + group_count;
            std::vector<unsigned char> active(total, 1), supported(total, 0), queued(queue_size, 0);
            std::vector<std::pair<std::size_t, std::size_t>> trail;
            trail.reserve(total);
            std::vector<Decision> decisions; decisions.reserve(n);
            std::vector<std::size_t> queue(queue_size);
            if (group_count && !cached) {
                building.groups.reserve(group_count); building.groups_by_variable.resize(n);
                for (auto& membership : building.groups_by_variable) { admit(group_count); membership.reserve(group_count); }
            }
            std::size_t head = 0, pending = 0;
            const auto enqueue = [&](std::size_t ci) {
                tick();
                if (!queued[ci]) {
                    queue[(head + pending) % queue_size] = ci; ++pending; queued[ci] = 1;
                }
            };
            const auto adjacent = [&](std::size_t vi, std::size_t except) {
                const auto it = charts_by_cell_.find(variables[vi]);
                if (it != charts_by_cell_.end())
                    for (const auto ci : it->second) { tick(); if (ci != except) enqueue(ci); }
                if (group_count) for (const auto gi : groups_by_variable[vi]) {
                    tick(); if (m + gi != except) enqueue(m + gi);
                }
            };
            const auto remove = [&](std::size_t vi, std::size_t di) {
                tick();
                if (active[di]) {
                    active[di] = 0; --counts[vi]; trail.emplace_back(vi, di); ++work.domain_reductions;
                }
            };
            for (const auto& [cell, value] : fixed) {
                tick(); const auto vi = position(cell); const auto& d = *values[vi];
                std::optional<std::size_t> selected;
                for (std::size_t k = 0; k < d.size(); ++k) {
                    tick(); if (d[k] == value) { selected = k; break; }
                }
                if (!selected) throw std::invalid_argument("propagated search fixed value is outside its native domain");
                for (std::size_t k = 0; k < d.size(); ++k) {
                    tick(); if (k != *selected) remove(vi, offsets[vi] + k);
                }
            }
            if (!cached) {
                building.compiled.reserve(m);
                for (const auto& chart : charts_) {
                    tick(); CompiledChart c;
                    admit(chart.support.size()); c.support.reserve(chart.support.size());
                    for (const auto cell : chart.support) { tick(); c.support.push_back(position(cell)); }
                    // Pre-admit the complete table, including duplicate tuples.
                    std::size_t entries = 0;
                    for (const auto& tuple : chart.allowed) { tick(); admit(tuple.size()); entries += tuple.size(); }
                    c.tuples.reserve(entries);
                    for (const auto& tuple : chart.allowed) {
                        tick();
                        for (std::size_t k = 0; k < c.support.size(); ++k) {
                            tick(); const auto vi = c.support[k]; const auto& d = *values[vi];
                            auto di = total;
                            for (std::size_t j = 0; j < d.size(); ++j) {
                                tick(); ++work.coordinate_tests;
                                if (d[j] == tuple[k]) { di = offsets[vi] + j; break; }
                            }
                            // set_domain may have removed a formerly allowed value.
                            c.tuples.push_back(di);
                        }
                    }
                    building.compiled.push_back(std::move(c));
                }
            }
            std::vector<std::size_t> left_match, right_match, parent, frontier;
            if (group_count) {
                if (!cached) {
                    // Equality here is exactly Value equality from the retained
                    // chart predicate. Equal symbol definitions do not enter it.
                    admit(total); admit(total);
                    building.symbol.resize(total); std::vector<const Value*> representatives; representatives.reserve(total);
                    for (std::size_t vi = 0; vi < n; ++vi) for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                        tick(); const auto& value = (*values[vi])[di - offsets[vi]]; auto si = representatives.size();
                        for (std::size_t j = 0; j < representatives.size(); ++j) {
                            tick(); ++work.coordinate_tests;
                            if (value == *representatives[j]) { si = j; break; }
                        }
                        if (si == representatives.size()) representatives.push_back(&value);
                        building.symbol[di] = si;
                    }
                    building.symbol_count = representatives.size();
                }
                const auto symbols = cached ? cached->symbol_count : building.symbol_count;
                admit(n); admit(n); for (unsigned i = 0; i < 3; ++i) admit(symbols);
                left_match.resize(n); right_match.resize(symbols); parent.resize(symbols);
                frontier.resize(std::max(n, symbols));
                if (!cached) {
                    std::size_t largest_table = 0;
                    for (const auto& chart : charts_) { tick(); if (chart.support.size() == 2) largest_table = std::max(largest_table, chart.allowed.size()); }
                    admit(largest_table); admit(largest_table);
                    std::vector<std::pair<std::size_t, std::size_t>> rows; rows.reserve(largest_table);
                    std::vector<signed char> verified(m, -1);
                    const auto inequality = [&](std::size_t ci) {
                        tick(); if (verified[ci] >= 0) return verified[ci] != 0;
                        const auto& c = compiled[ci]; rows.clear();
                        const auto a = c.support[0], b = c.support[1];
                        for (std::size_t k = 0; k < c.tuples.size(); k += 2) {
                            tick(); ++work.tuple_tests; const auto x = c.tuples[k], y = c.tuples[k + 1];
                            if (x == total || y == total) continue;
                            if (symbol[x] == symbol[y]) { verified[ci] = 0; return false; }
                            rows.emplace_back(x, y);
                        }
                        std::sort(rows.begin(), rows.end(), [&](const auto& x, const auto& y) { tick(); return x < y; });
                        std::size_t unique = 0;
                        for (std::size_t k = 0; k < rows.size(); ++k) { tick(); if (!k || rows[k] != rows[k - 1]) ++unique; }
                        std::size_t expected = 0;
                        for (auto x = offsets[a]; x < offsets[a + 1]; ++x)
                            for (auto y = offsets[b]; y < offsets[b + 1]; ++y) { tick(); ++work.coordinate_tests; if (symbol[x] != symbol[y]) ++expected; }
                        verified[ci] = unique == expected ? 1 : 0; return verified[ci] != 0;
                    };
                    for (std::size_t gi = 0; gi < group_count; ++gi) {
                        tick(); const auto relation = distinct_relations[gi]; validate_relation(relation);
                        for (std::size_t j = 0; j < gi; ++j) { tick(); if (distinct_relations[j] == relation) throw std::invalid_argument("duplicate distinct relation request"); }
                        const auto& arena = complex().grade(relation.grade);
                        const auto [begin, end] = arena.incidence_range(relation.index);
                        // Retain the original support order; membership storage was
                        // admitted independently before preparing any group.
                        admit(end - begin);
                        std::vector<std::size_t> support; support.reserve(end - begin);
                        const auto exists = complex().signature().existence_aspect();
                        for (auto slot = begin; slot < end; ++slot) {
                            tick(); if (!arena.aspects.get(exists, slot)) continue;
                            const core::CellId cell{{static_cast<std::uint16_t>(relation.grade.value - 1)}, arena.lower_index[slot]};
                            const auto vi = position(cell); bool seen = false;
                            for (const auto previous : support) { tick(); if (previous == vi) { seen = true; break; } }
                            if (!seen) support.push_back(vi);
                        }
                        if (support.size() < 2) throw std::invalid_argument("distinct relation needs at least two active boundary identities");
                        for (std::size_t a = 0; a < support.size(); ++a) for (auto b = a + 1; b < support.size(); ++b) {
                            tick(); bool found = false;
                            const auto adjacent_charts = charts_by_cell_.find(variables[support[a]]);
                            if (adjacent_charts != charts_by_cell_.end()) for (const auto ci : adjacent_charts->second) {
                                tick(); const auto& c = compiled[ci];
                                if (c.support.size() != 2 || !charts_[ci].patch.contains(static_cast<core::CellId>(relation))) continue;
                                if (!((c.support[0] == support[a] && c.support[1] == support[b]) ||
                                      (c.support[1] == support[a] && c.support[0] == support[b]))) continue;
                                if (inequality(ci)) { found = true; break; }
                            }
                            if (!found) throw std::invalid_argument("distinct relation lacks original complete inequality charts on its whole patch");
                        }
                        for (const auto vi : support) { tick(); building.groups_by_variable[vi].push_back(gi); }
                        building.groups.push_back(std::move(support)); ++work.distinct_groups;
                    }
                }
            }
            if (capture) {
                admit(group_count); building.relations.reserve(group_count);
                for (const auto relation : distinct_relations) { tick(); building.relations.push_back(relation); }
                building.generation = generation_;
                // Logical index/record slots; original source/Value storage and
                // allocator overhead remain separate, as in solve admission.
                const auto retain = [&](std::size_t count) {
                    tick();
                    if (count > limits.max_entries - building.retained_entries)
                        throw Interrupted{FiniteSearchStatus::BudgetExceeded};
                    building.retained_entries += count;
                };
                retain(variables.size()); retain(offsets.size()); retain(symbol.size());
                retain(m); retain(m); retain(groups.size()); retain(groups_by_variable.size()); retain(group_count);
                for (const auto& chart : compiled) { retain(chart.support.capacity()); retain(chart.tuples.capacity()); }
                for (const auto& group : groups) retain(group.capacity());
                for (const auto& membership : groups_by_variable) retain(membership.capacity());
                tick(); building.preparation = work;
                *capture = std::make_shared<const PropagationIndexData>(std::move(building));
                return {FiniteSearchStatus::Satisfiable, std::nullopt, work};
            }
            const auto match_group = [&](std::size_t gi) {
                tick(); ++work.matching_tests; const auto& group = groups[gi]; const auto none = total;
                for (const auto vi : group) { tick(); left_match[vi] = none; }
                for (auto& owner : right_match) { tick(); owner = none; }
                // A covering matching is a local injective section. Breadth-first
                // augmenting paths keep cancellation and stack depth explicit.
                for (const auto root : group) {
                    for (auto& p : parent) { tick(); p = none; }
                    std::size_t read = 0, end = 1; frontier[0] = root; auto free = none;
                    while (read < end && free == none) {
                        tick(); const auto vi = frontier[read++];
                        for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                            tick(); if (!active[di]) continue; const auto si = symbol[di];
                            if (parent[si] != none) continue;
                            parent[si] = vi;
                            if (right_match[si] == none) { free = si; break; }
                            frontier[end++] = right_match[si];
                        }
                    }
                    if (free == none) return false;
                    while (free != none) {
                        tick(); const auto vi = parent[free], previous = left_match[vi];
                        right_match[free] = vi; left_match[vi] = free; free = previous;
                    }
                }
                for (const auto vi : group)
                    for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) { tick(); supported[di] = 0; }
                // An edge belongs to a covering matching exactly when it is
                // matched, lies on an alternating cycle, or reaches a free value.
                // Reachability visits actual domain values; no tuple universe is
                // generated and no local fixed point is called a global solve.
                for (std::size_t start = 0; start < right_match.size(); ++start) {
                    for (auto& p : parent) { tick(); p = none; }
                    parent[start] = start; std::size_t read = 0, end = 1; frontier[0] = start; bool reaches_free = false;
                    while (read < end) {
                        tick(); const auto si = frontier[read++], vi = right_match[si];
                        if (vi == none) { reaches_free = true; continue; }
                        for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                            tick(); if (!active[di]) continue; const auto next = symbol[di];
                            if (parent[next] == none) { parent[next] = si; frontier[end++] = next; }
                        }
                    }
                    for (const auto vi : group) for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                        tick();
                        if (active[di] && symbol[di] == start && (reaches_free || parent[left_match[vi]] != none)) supported[di] = 1;
                    }
                }
                for (const auto vi : group) {
                    bool changed = false;
                    for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                        tick(); if (active[di] && !supported[di]) { remove(vi, di); changed = true; }
                    }
                    if (!counts[vi]) return false;
                    if (changed) adjacent(vi, m + gi);
                }
                return true;
            };
            const auto propagate = [&] {
                while (pending) {
                    tick(); const auto ci = queue[head]; head = (head + 1) % queue_size;
                    --pending; queued[ci] = 0;
                    if (ci >= m) { if (!match_group(ci - m)) return false; continue; }
                    ++work.chart_tests;
                    const auto& c = compiled[ci]; const auto arity = c.support.size();
                    for (const auto vi : c.support)
                        for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) { tick(); supported[di] = 0; }
                    bool any = false;
                    for (std::size_t t = 0; t < c.tuples.size(); t += arity) {
                        tick(); ++work.tuple_tests; bool valid = true;
                        for (std::size_t k = 0; k < arity; ++k) {
                            tick(); ++work.coordinate_tests; const auto di = c.tuples[t + k];
                            if (di == total || !active[di]) { valid = false; break; }
                        }
                        if (valid) {
                            any = true;
                            for (std::size_t k = 0; k < arity; ++k) { tick(); supported[c.tuples[t + k]] = 1; }
                        }
                    }
                    if (!any) return false;
                    for (const auto vi : c.support) {
                        bool changed = false;
                        for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                            tick(); if (active[di] && !supported[di]) { remove(vi, di); changed = true; }
                        }
                        if (!counts[vi]) return false;
                        if (changed) adjacent(vi, ci);
                    }
                }
                return true;
            };
            for (std::size_t ci = 0; ci < queue_size; ++ci) enqueue(ci);
            if (!propagate()) return {FiniteSearchStatus::Unsatisfiable, std::nullopt, work};
            bool descend = true;
            while (true) {
                tick();
                if (descend) {
                    std::optional<std::size_t> chosen; std::size_t assigned = 0;
                    for (std::size_t vi = 0; vi < n; ++vi) {
                        tick();
                        if (counts[vi] == 1) { ++assigned; continue; }
                        if (!chosen || counts[vi] < counts[*chosen] ||
                            (counts[vi] == counts[*chosen] && chart_degree(variables[vi]) > chart_degree(variables[*chosen])))
                            chosen = vi;
                    }
                    work.peak_assigned = std::max(work.peak_assigned, assigned);
                    if (!chosen) {
                        Assignment witness;
                        for (std::size_t vi = 0; vi < n; ++vi)
                            for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                                tick(); if (active[di]) witness.emplace(variables[vi], (*values[vi])[di - offsets[vi]]);
                            }
                        // Validate against the original Value predicate as well.
                        for (const auto& chart : charts_) {
                            tick(); ++work.chart_tests;
                            if (!chart_satisfied_work(chart, witness, true, [&](bool tuple) {
                                tick(); if (tuple) ++work.tuple_tests; else ++work.coordinate_tests;
                            })) throw std::logic_error("propagated witness violates its original chart");
                        }
                        tick(); return {FiniteSearchStatus::Satisfiable, std::move(witness), work};
                    }
                    decisions.push_back({*chosen, offsets[*chosen], trail.size()}); ++work.search_nodes;
                }
                if (decisions.empty()) { tick(); return {FiniteSearchStatus::Unsatisfiable, std::nullopt, work}; }
                auto& decision = decisions.back();
                // Restore only this branch; ancestors and fixed values survive.
                while (trail.size() > decision.mark) {
                    tick(); const auto [vi, di] = trail.back(); trail.pop_back(); active[di] = 1; ++counts[vi];
                }
                while (pending) {
                    tick(); queued[queue[head]] = 0; head = (head + 1) % queue_size; --pending;
                }
                const auto vi = decision.variable;
                while (decision.next < offsets[vi + 1] && !active[decision.next]) { tick(); ++decision.next; }
                if (decision.next == offsets[vi + 1]) { decisions.pop_back(); descend = false; continue; }
                tick(); const auto selected = decision.next++; ++work.value_trials;
                for (auto di = offsets[vi]; di < offsets[vi + 1]; ++di) {
                    tick(); if (di != selected) remove(vi, di);
                }
                adjacent(vi, queue_size);
                descend = propagate();
            }
        } catch (const Interrupted& e) { return {e.status, std::nullopt, work}; }
    }

public:
    // A lossless execution request: each named primary relation must already
    // have complete pair-inequality charts over its whole active boundary.
    // Verification refuses missing or weaker charts instead of adding a rule.
    [[nodiscard]] BoundedSearchResult distinct_global_section(const std::vector<core::RelationId>& relations,
        const Assignment& fixed = {}, FiniteSearchLimits limits = {}, std::stop_token stop = {}) const {
        return propagated_section(fixed, limits, stop, relations);
    }

    // Exact deterministic subset-minimal unsatisfiable core for the complete
    // finite constraint family.  Returns nullopt when the family has a global
    // section.  The reduction performs at most one exact satisfiability check
    // per retained candidate chart after the initial unsatisfiability is known.
    [[nodiscard]] std::optional<UnsatCore> exact_global_unsat_core() const {
        auto candidates = all_chart_indices();
        if (chart_subset_has_section(candidates, nullptr)) return std::nullopt;
        return minimize_unsat_core(std::move(candidates), nullptr);
    }

    // Exact subset-minimal core using only constraints fully visible on the
    // supplied boundary-closed patch.  Constraints crossing the patch
    // boundary are not silently truncated.
    [[nodiscard]] std::optional<UnsatCore> exact_patch_unsat_core(const algebra::SubcomplexMask& patch) const {
        carrier_.require_closed_patch(patch);
        auto candidates = visible_chart_indices(patch);
        if (chart_subset_has_section(candidates, &patch)) return std::nullopt;
        return minimize_unsat_core(std::move(candidates), &patch);
    }

    [[nodiscard]] bool cover_is_constraint_complete(const std::vector<algebra::SubcomplexMask>& cover) const {
        for (const auto& patch : cover) carrier_.require_closed_patch(patch);
        for (const auto& chart : charts_) {
            bool seen = false;
            for (const auto& patch : cover) {
                if (support_inside(chart.support, patch)) { seen = true; break; }
            }
            if (!seen) return false;
        }
        return true;
    }

    [[nodiscard]] CoverReport diagnose_cover(const std::vector<algebra::SubcomplexMask>& cover) const {
        CoverReport report;
        for (std::size_t i = 0; i < cover.size(); ++i) {
            carrier_.require_closed_patch(cover[i]);
            if (!patch_has_section(cover[i])) {
                report.every_patch_locally_satisfiable = false;
                report.certificate.patch_indices.push_back(i);
                // patch_has_section already proved unsatisfiability, so do not
                // repeat that exact solve before core reduction.
                auto candidates = visible_chart_indices(cover[i]);
                report.local_unsat_cores.push_back(
                    LocalUnsatCore{i, minimize_unsat_core(std::move(candidates), &cover[i])});
            }
        }
        report.constraint_complete = cover_is_constraint_complete(cover);
        if (!report.every_patch_locally_satisfiable) {
            report.obstruction = ObstructionKind::LocalPatchUnsatisfiable;
            report.certificate.kind = report.obstruction;
            // Preserve the older value-neutral certificate while exposing the
            // richer per-patch cores above.  The compatibility certificate is
            // the deterministic union of all exact local cores.
            std::unordered_set<std::size_t> charts_seen;
            std::unordered_set<core::CellId, CellIdHash> cells_seen;
            for (const auto& local_core : report.local_unsat_cores) {
                for (const auto chart : local_core.core.chart_indices) {
                    if (charts_seen.insert(chart).second) report.certificate.chart_indices.push_back(chart);
                }
                for (const auto cell : local_core.core.cells) {
                    if (cells_seen.insert(cell).second) report.certificate.cells.push_back(cell);
                }
            }
            sort_cells(report.certificate.cells);
        } else if (!report.constraint_complete) {
            report.obstruction = ObstructionKind::CoverOmitsConstraint;
            report.certificate.kind = report.obstruction;
            std::unordered_set<core::CellId, CellIdHash> cells_seen;
            for (std::size_t chart_index = 0; chart_index < charts_.size(); ++chart_index) {
                bool seen = false;
                for (const auto& patch : cover) {
                    if (support_inside(charts_[chart_index].support, patch)) { seen = true; break; }
                }
                if (!seen) {
                    report.certificate.chart_indices.push_back(chart_index);
                    for (const auto cell : charts_[chart_index].support) {
                        if (cells_seen.insert(cell).second) report.certificate.cells.push_back(cell);
                    }
                }
            }
            sort_cells(report.certificate.cells);
        } else {
            report.global_section_checked = true;
            report.global_section_exists = global_section().has_value();
            if (!report.global_section_exists) {
                report.obstruction = ObstructionKind::NoCompatibleGlobalSection;
                report.certificate.kind = report.obstruction;
                // global_section already proved unsatisfiability.  Reduce that
                // exact conflict directly instead of rerunning the initial
                // satisfiability check.
                report.global_unsat_core = minimize_unsat_core(all_chart_indices(), nullptr);
                report.certificate.chart_indices = report.global_unsat_core->chart_indices;
                report.certificate.cells = report.global_unsat_core->cells;
            }
        }
        return report;
    }

private:
    [[nodiscard]] bool domain_contains(core::CellId cell, const Value& value) const {
        const auto& values = domain(cell);
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    void validate_relation(core::RelationId relation) const {
        if (relation.grade.value == 0u) throw std::invalid_argument("local relation chart requires positive grade");
        carrier_.validate_cell(static_cast<core::CellId>(relation));
    }

    [[nodiscard]] static bool support_inside(const std::vector<core::CellId>& support,
                                             const algebra::SubcomplexMask& patch) {
        return std::all_of(support.begin(), support.end(), [&](const auto cell) { return patch.contains(cell); });
    }

    template<class Visit>
    [[nodiscard]] bool chart_satisfied_work(const Chart& chart,
                                            const Assignment& assignment,
                                            bool require_complete, Visit&& visit) const {
        for (const auto& tuple : chart.allowed) {
            visit(true);
            bool matches = true;
            for (std::size_t i = 0; i < chart.support.size(); ++i) {
                visit(false);
                const auto it = assignment.find(chart.support[i]);
                if (it == assignment.end()) {
                    if (require_complete) { matches = false; break; }
                    continue;
                }
                if (!(it->second == tuple[i])) { matches = false; break; }
            }
            if (matches) return true;
        }
        return false;
    }
    [[nodiscard]] bool chart_satisfied(const Chart& chart,
                                       const Assignment& assignment,
                                       bool require_complete) const {
        return chart_satisfied_work(chart, assignment, require_complete, [](bool) {});
    }

    [[nodiscard]] std::size_t chart_degree(core::CellId cell) const noexcept {
        const auto it = charts_by_cell_.find(cell);
        return it == charts_by_cell_.end() ? 0u : it->second.size();
    }

    static void sort_cells(std::vector<core::CellId>& cells) {
        std::sort(cells.begin(), cells.end(), [](const auto a, const auto b) {
            if (a.grade.value != b.grade.value) return a.grade.value < b.grade.value;
            return a.index < b.index;
        });
    }

    [[nodiscard]] std::vector<std::size_t> all_chart_indices() const {
        std::vector<std::size_t> result(charts_.size());
        for (std::size_t i = 0; i < charts_.size(); ++i) result[i] = i;
        return result;
    }

    [[nodiscard]] std::vector<std::size_t> visible_chart_indices(const algebra::SubcomplexMask& patch) const {
        std::vector<std::size_t> result;
        result.reserve(charts_.size());
        for (std::size_t i = 0; i < charts_.size(); ++i) {
            if (support_inside(charts_[i].support, patch)) result.push_back(i);
        }
        return result;
    }

    [[nodiscard]] bool chart_subset_has_section(const std::vector<std::size_t>& chart_indices,
                                                const algebra::SubcomplexMask* patch) const {
        std::vector<std::uint8_t> enabled(charts_.size(), std::uint8_t{0});
        std::vector<core::CellId> variables;
        std::unordered_set<core::CellId, CellIdHash> variable_set;

        for (const auto chart_index : chart_indices) {
            if (chart_index >= charts_.size()) throw std::out_of_range("constraint chart index out of range");
            const auto& chart = charts_[chart_index];
            if (patch != nullptr && !support_inside(chart.support, *patch)) {
                throw std::invalid_argument("constraint-core candidate is not visible on the requested patch");
            }
            enabled[chart_index] = 1u;
            for (const auto cell : chart.support) {
                if (variable_set.insert(cell).second) variables.push_back(cell);
            }
        }

        std::sort(variables.begin(), variables.end(), [](const auto a, const auto b) {
            if (a.grade.value != b.grade.value) return a.grade.value < b.grade.value;
            return a.index < b.index;
        });
        std::unordered_map<core::CellId, std::size_t, CellIdHash> active_degrees;
        active_degrees.reserve(variables.size());
        for (const auto cell : variables) {
            std::size_t count = 0;
            const auto it = charts_by_cell_.find(cell);
            if (it != charts_by_cell_.end()) {
                for (const auto ci : it->second) count += enabled[ci] != 0u ? 1u : 0u;
            }
            active_degrees.emplace(cell, count);
        }
        std::stable_sort(variables.begin(), variables.end(), [&](const auto a, const auto b) {
            const auto ad = domain(a).size();
            const auto bd = domain(b).size();
            if (ad != bd) return ad < bd;
            const auto ac = active_degrees.at(a);
            const auto bc = active_degrees.at(b);
            return ac > bc;
        });

        Assignment current;
        const auto extendable = [&](core::CellId cell) {
            const auto it = charts_by_cell_.find(cell);
            if (it == charts_by_cell_.end()) return true;
            for (const auto chart_index : it->second) {
                if (enabled[chart_index] == 0u) continue;
                if (!chart_satisfied(charts_[chart_index], current, false)) return false;
            }
            return true;
        };
        const auto complete = [&]() {
            for (const auto chart_index : chart_indices) {
                if (!chart_satisfied(charts_[chart_index], current, true)) return false;
            }
            return true;
        };
        const auto recurse = [&](auto&& self, std::size_t position) -> bool {
            if (position == variables.size()) return complete();
            const auto cell = variables[position];
            for (const auto& value : domain(cell)) {
                current[cell] = value;
                if (extendable(cell) && self(self, position + 1u)) return true;
            }
            current.erase(cell);
            return false;
        };
        return recurse(recurse, 0u);
    }

    [[nodiscard]] UnsatCore minimize_unsat_core(std::vector<std::size_t> candidates,
                                                const algebra::SubcomplexMask* patch) const {
        // Deterministic deletion reduction.  On exit, every retained chart is
        // individually necessary for unsatisfiability with all other retained
        // charts fixed.  This is an exact subset-minimal core.
        std::size_t position = 0u;
        while (position < candidates.size()) {
            auto trial = candidates;
            trial.erase(trial.begin() + static_cast<std::ptrdiff_t>(position));
            if (!chart_subset_has_section(trial, patch)) {
                candidates = std::move(trial);
            } else {
                ++position;
            }
        }

        UnsatCore result;
        result.chart_indices = std::move(candidates);
        result.subset_minimal = true;
        std::unordered_set<core::CellId, CellIdHash> cells_seen;
        for (const auto chart_index : result.chart_indices) {
            for (const auto cell : charts_[chart_index].support) {
                if (cells_seen.insert(cell).second) result.cells.push_back(cell);
            }
        }
        sort_cells(result.cells);
        return result;
    }

    [[nodiscard]] bool incident_extendable(core::CellId cell,
                                           const Assignment& assignment,
                                           const algebra::SubcomplexMask* patch = nullptr) const {
        const auto it = charts_by_cell_.find(cell);
        if (it == charts_by_cell_.end()) return true;
        for (const auto chart_index : it->second) {
            const auto& chart = charts_[chart_index];
            if (patch != nullptr && !support_inside(chart.support, *patch)) continue;
            if (!chart_satisfied(chart, assignment, false)) return false;
        }
        return true;
    }

    bool search(const std::vector<core::CellId>& variables,
                std::size_t position,
                Assignment& current) const {
        if (position == variables.size()) return globally_valid(current);
        const auto cell = variables[position];
        for (const auto& value : domain(cell)) {
            current[cell] = value;
            if (incident_extendable(cell, current) && search(variables, position + 1u, current)) return true;
        }
        current.erase(cell);
        return false;
    }

    [[nodiscard]] bool patch_has_section(const algebra::SubcomplexMask& patch) const {
        std::vector<core::CellId> variables;
        for (const auto& [cell, _] : domains_) if (patch.contains(cell)) variables.push_back(cell);
        std::sort(variables.begin(), variables.end(), [](const auto a, const auto b) {
            if (a.grade.value != b.grade.value) return a.grade.value < b.grade.value;
            return a.index < b.index;
        });
        std::stable_sort(variables.begin(), variables.end(), [&](const auto a, const auto b) {
            const auto ad = domain(a).size();
            const auto bd = domain(b).size();
            if (ad != bd) return ad < bd;
            const auto ac = chart_degree(a);
            const auto bc = chart_degree(b);
            return ac > bc;
        });
        Assignment current;
        const auto recurse = [&](auto&& self, std::size_t pos) -> bool {
            if (pos == variables.size()) return valid_on_patch(current, patch);
            const auto cell = variables[pos];
            for (const auto& value : domain(cell)) {
                current[cell] = value;
                if (incident_extendable(cell, current, &patch) && self(self, pos + 1u)) return true;
            }
            current.erase(cell);
            return false;
        };
        return recurse(recurse, 0u);
    }

    void invalidate_preparation() {
        if (generation_.use_count() != 1) generation_ = std::make_shared<const unsigned char>(0);
    }
    std::shared_ptr<const unsigned char> generation_ = std::make_shared<const unsigned char>(0);
    RelationalLocalCarrier carrier_;
    std::unordered_map<core::CellId, std::vector<Value>, CellIdHash> domains_;
    std::vector<Chart> charts_;
    std::unordered_map<core::CellId, std::vector<std::size_t>, CellIdHash> charts_by_cell_;
};

using FiniteIntegerConstraintSystem = FiniteConstraintSystem<std::int64_t>;
using FiniteRelationConstraintSystem = FiniteConstraintSystem<core::RelationIdentity>;

// Compatibility aliases retained for source written before the local-carrier
// realignment.  They are aliases only; the canonical abstraction is a finite
// constraint system and no generic sheaf axiom is implied.
using FiniteConstraintSheaf = FiniteIntegerConstraintSystem;
using FiniteRelationConstraintSheaf = FiniteRelationConstraintSystem;

} // namespace rfc::local
