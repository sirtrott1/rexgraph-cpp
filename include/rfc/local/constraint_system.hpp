#pragma once

#include "rfc/core/boundary_fiber.hpp"
#include "rfc/local/section.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rfc::local {

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

    [[nodiscard]] bool chart_satisfied(const Chart& chart,
                                       const Assignment& assignment,
                                       bool require_complete) const {
        for (const auto& tuple : chart.allowed) {
            bool matches = true;
            for (std::size_t i = 0; i < chart.support.size(); ++i) {
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
