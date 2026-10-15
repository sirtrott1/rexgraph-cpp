#pragma once

#include "rfc/local/owned_cellular_sheaf.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace rfc::local {

using SparseSheafValues = std::map<std::uint32_t, numeric::SparseRationalVector>;

// Counts for this layer, not allocator bytes or GMP internal storage. Index
// slots comprise one flag bucket, one represented-column bucket, and two slots
// (target coordinate and coefficient reference) per explicit nonzero. Retained
// source cells/occurrences and their dimension metadata are counted separately.
struct SparseSheafPreparationWork {
    std::size_t cells{}, occurrences{}, restriction_nonzeros{};
    std::size_t indexed_flags{}, indexed_columns{}, indexed_terms{};
    std::size_t retained_index_entries{}, checked_steps{};
    bool operator==(const SparseSheafPreparationWork&) const = default;
};

struct SparseSheafActionWork {
    std::size_t active_stalks{}, incidences{}, inactive_incidences{}, zero_coeff_incidences{};
    std::size_t identity_products{}, restriction_products{}, scalar_products{};
    std::size_t peak_numeric_nonzeros{}, peak_live_numeric_entries{}, checked_steps{};
    bool operator==(const SparseSheafActionWork&) const = default;
};

class SparseSheafActions;

// A total cochain over the owner's retained stalks. Missing numerical entries
// mean exact zero, not missing observations or absent cells. No set of unknown
// observations is represented by this value type. Source support remains owned.
class SparseSheafCochain {
    friend class SparseSheafActions;
    OwnedLinearCellularSheaf owner_;
    core::Grade grade_{};
    SparseSheafValues values_;
    std::size_t nonzeros_{};
    SparseSheafCochain(OwnedLinearCellularSheaf owner, core::Grade grade, SparseSheafValues values)
        : owner_(std::move(owner)), grade_(grade), values_(std::move(values)) {
        for (const auto& [cell, value] : values_) {
            (void)cell;
            nonzeros_ = machine::checked_size_add(nonzeros_, value.nonzeros());
        }
    }
public:
    const OwnedLinearCellularSheaf& owner() const & noexcept { return owner_; }
    const OwnedLinearCellularSheaf& owner() const && = delete;
    core::Grade grade() const noexcept { return grade_; }
    const SparseSheafValues& values() const & noexcept { return values_; }
    const SparseSheafValues& values() const && = delete;
    std::size_t nonzeros() const noexcept { return nonzeros_; }
    bool zero() const noexcept { return values_.empty(); }
};

struct SparseSheafActionResult {
    SparseSheafCochain field;
    SparseSheafActionWork work;
};

// Native upward delta^q actions. This is NOT the adjoint of a downward fiber
// map. Each rho_{lower -> upper} is multiplied exactly once by its retained
// signed rational incidence coefficient, matching LinearCellularSheaf.
//
// No whole coboundary, stalk identity, grade vector, kernel or projector is
// assembled. Prepared indices use only represented columns of explicit rho.
// Equal-dimensional default restrictions are symbolic identities. An explicit
// zero restriction stays zero. Inactive flags stay in the immutable source.
//
// Every flag declaration is admitted at preparation, including inactive and
// zero-coefficient flags. A missing restriction between unequal stalks is
// rejected even for zero input. Cochain flatness is not assumed or certified by
// preparation: two_step_defect measures delta^(q+1) delta^q on the supplied field.
// Whole-space cohomology/local-system certification remains a separate request.
class SparseSheafActions {
    using Q = numeric::Rational;
    using S = numeric::SparseRationalVector;
    using E = numeric::SparseRationalEntry;
    using Guard = numeric::ExactActionGuard;
    using Coordinates = std::map<std::size_t, Q>;
    using Accumulator = std::map<std::uint32_t, Coordinates>;
    using Flag = std::pair<std::uint16_t, std::size_t>;
    struct Term { std::size_t target; const Q* coefficient; };
    using Columns = std::map<std::size_t, std::vector<Term>>;
    struct State {
        OwnedLinearCellularSheaf owner;
        numeric::ExactActionLimits limits;
        std::map<Flag, Columns> indices;
        SparseSheafPreparationWork work;
    };
    std::shared_ptr<const State> state_;

    static void add_count(std::size_t& value, std::size_t n = 1u) {
        value = machine::checked_size_add(value, n);
    }
    void require_grade(core::Grade grade) const {
        if (grade.value >= state_->owner.sheaf().complex().grade_count())
            throw std::out_of_range("sparse sheaf cochain grade is not retained");
    }
    void require_field(const SparseSheafCochain& field) const {
        if (!state_->owner.same_owner(field.owner()))
            throw std::invalid_argument("sparse sheaf action requires the actual immutable sheaf owner");
        require_grade(field.grade());
    }
    static void live(Guard& guard, SparseSheafActionWork& work, std::size_t entries) {
        guard.entries(entries);
        work.peak_live_numeric_entries = std::max(work.peak_live_numeric_entries, entries);
    }
    static void peak(SparseSheafActionWork& work, std::size_t entries) {
        work.peak_numeric_nonzeros = std::max(work.peak_numeric_nonzeros, entries);
    }
    static void accumulate(Accumulator& output, std::uint32_t cell, std::size_t coordinate,
                           const Q& contribution, std::size_t& stored, std::size_t base,
                           Guard& guard, SparseSheafActionWork& work) {
        if (contribution == 0) return;
        auto at = output.find(cell);
        if (at == output.end()) at = output.emplace(cell, Coordinates{}).first;
        auto& values = at->second;
        const auto old = values.find(coordinate);
        Q sum = guard.add(old == values.end() ? Q{0} : old->second, contribution);
        if (sum == 0) {
            if (old != values.end()) { values.erase(old); --stored; }
            if (values.empty()) output.erase(at);
        } else {
            if (old == values.end()) {
                const auto next = machine::checked_size_add(stored, 1u);
                live(guard, work, machine::checked_size_add(base, next));
                stored = next;
            }
            values[coordinate] = std::move(sum);
        }
        peak(work, stored);
    }

    SparseSheafCochain apply(const SparseSheafCochain& input, Guard& guard,
                            SparseSheafActionWork& work, std::size_t held) const {
        const auto q = input.grade();
        const auto& sheaf = state_->owner.sheaf();
        const auto& complex = sheaf.complex();
        if (static_cast<std::size_t>(q.value) + 1u >= complex.grade_count())
            throw std::out_of_range("sparse sheaf coboundary requires a retained target grade");
        const core::Grade upper{static_cast<std::uint16_t>(q.value + 1u)};
        const auto& arena = complex.grade(upper);
        const auto exists = complex.signature().existence_aspect();
        const auto base = machine::checked_size_add(held, input.nonzeros());
        live(guard, work, base); peak(work, input.nonzeros());
        Accumulator output;
        std::size_t stored{};
        for (const auto& [cell, values] : input.values()) {
            guard.step(); add_count(work.active_stalks); guard.sparse_vector(values);
            const auto [begin, end] = arena.coface_incidence_range(cell);
            for (auto position = begin; position < end; ++position) {
                guard.step(); add_count(work.incidences);
                const auto slot = arena.incidence_by_lower[position];
                if (!arena.aspects.get(exists, slot)) { add_count(work.inactive_incidences); continue; }
                const Q coefficient = complex.incidence_coefficient(upper, slot);
                guard.value(coefficient);
                if (coefficient == 0) { add_count(work.zero_coeff_incidences); continue; }
                const auto target = arena.upper_index[slot];
                if (!sheaf.restriction_override(upper, slot)) {
                    for (const auto& entry : values.entries()) {
                        add_count(work.identity_products); add_count(work.scalar_products);
                        accumulate(output, target, entry.index, guard.mul(coefficient, entry.value),
                                   stored, base, guard, work);
                    }
                    continue;
                }
                const auto flag = state_->indices.find({upper.value, slot});
                // An explicitly zero restriction must not become an identity.
                if (flag == state_->indices.end()) continue;
                for (const auto& entry : values.entries()) {
                    guard.step();
                    const auto column = flag->second.find(entry.index);
                    if (column == flag->second.end()) continue;
                    for (const auto& term : column->second) {
                        add_count(work.restriction_products); add_count(work.scalar_products, 2u);
                        const Q mapped = guard.mul(*term.coefficient, entry.value);
                        accumulate(output, target, term.target, guard.mul(coefficient, mapped),
                                   stored, base, guard, work);
                    }
                }
            }
        }
        // The scalar map accumulator and the result can coexist at finish.
        // Admit both, conservatively, before result-vector allocation.
        live(guard, work, machine::checked_size_add(base, machine::checked_size_mul(stored, 2u)));
        SparseSheafValues result;
        for (auto& [cell, coordinates] : output) {
            std::vector<E> entries; entries.reserve(coordinates.size());
            for (auto& [coordinate, value] : coordinates) {
                guard.step(); entries.push_back({coordinate, std::move(value)});
            }
            result.emplace(cell, S(sheaf.stalks().dimension({upper, cell}), std::move(entries)));
        }
        return SparseSheafCochain(state_->owner, upper, std::move(result));
    }

public:
    static numeric::ExactActionLimits default_limits() {
        numeric::ExactActionLimits limits;
        limits.dimension = std::numeric_limits<std::size_t>::max();
        return limits;
    }
    explicit SparseSheafActions(OwnedLinearCellularSheaf owner,
                                numeric::ExactActionLimits limits = default_limits()) {
        auto state = std::make_shared<State>(State{std::move(owner), limits, {}, {}});
        auto& work = state->work;
        const auto& sheaf = state->owner.sheaf();
        const auto& complex = sheaf.complex();
        Guard guard(limits);
        for (std::size_t k = 0; k < complex.grade_count(); ++k) {
            const core::Grade grade{static_cast<std::uint16_t>(k)};
            const auto& arena = complex.grade(grade);
            for (std::size_t i = 0; i < arena.cell_count(); ++i) {
                guard.step(); add_count(work.cells);
                guard.dimension(sheaf.stalks().dimension({grade, static_cast<std::uint32_t>(i)}));
            }
            if (!k) continue;
            const core::Grade lower{static_cast<std::uint16_t>(k - 1u)};
            for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
                guard.step(); add_count(work.occurrences);
                guard.value(complex.incidence_coefficient(grade, slot));
                const auto n = sheaf.stalks().dimension({lower, arena.lower_index[slot]});
                const auto m = sheaf.stalks().dimension({grade, arena.upper_index[slot]});
                const auto& map = sheaf.restriction_override(grade, slot);
                if (!map) {
                    if (n != m) throw std::invalid_argument("unequal sheaf stalks require an explicit upward restriction");
                    continue;
                }
                if (map->rows() != m || map->cols() != n)
                    throw std::invalid_argument("frozen sheaf restriction has wrong stalk dimensions");
                guard.matrix(*map);
                for (const auto& [coordinate, value] : map->entries()) {
                    guard.step(); add_count(work.restriction_nonzeros);
                    const Flag key{grade.value, slot};
                    if (!state->indices.contains(key)) {
                        guard.entries(machine::checked_size_add(work.retained_index_entries, 1u));
                        state->indices.emplace(key, Columns{});
                        add_count(work.indexed_flags); add_count(work.retained_index_entries);
                    }
                    auto& columns = state->indices.at(key);
                    if (!columns.contains(coordinate.second)) {
                        guard.entries(machine::checked_size_add(work.retained_index_entries, 1u));
                        columns.emplace(coordinate.second, std::vector<Term>{});
                        add_count(work.indexed_columns); add_count(work.retained_index_entries);
                    }
                    guard.entries(machine::checked_size_add(work.retained_index_entries, 2u));
                    columns.at(coordinate.second).push_back({coordinate.first, &value});
                    add_count(work.indexed_terms); add_count(work.retained_index_entries, 2u);
                }
            }
        }
        work.checked_steps = guard.steps();
        state_ = std::move(state);
    }

    numeric::ExactActionLimits limits() const noexcept { return state_->limits; }
    bool same_action_owner(const SparseSheafActions& other) const noexcept { return state_ == other.state_; }

    const OwnedLinearCellularSheaf& owner() const & noexcept { return state_->owner; }
    const OwnedLinearCellularSheaf& owner() const && = delete;
    const SparseSheafPreparationWork& preparation() const & noexcept { return state_->work; }
    const SparseSheafPreparationWork& preparation() const && = delete;

    SparseSheafCochain bind(core::Grade grade, SparseSheafValues values = {}) const {
        require_grade(grade); Guard guard(state_->limits); std::size_t stored{};
        for (auto it = values.begin(); it != values.end();) {
            const auto n = state_->owner.sheaf().stalks().dimension({grade, it->first});
            if (it->second.size() != n) throw std::invalid_argument("sparse sheaf input stalk dimension");
            guard.sparse_vector(it->second);
            if (it->second.empty()) it = values.erase(it);
            else {
                stored = machine::checked_size_add(stored, it->second.nonzeros());
                guard.entries(stored); ++it;
            }
        }
        return SparseSheafCochain(state_->owner, grade, std::move(values));
    }

    SparseSheafActionResult coboundary(const SparseSheafCochain& input) const {
        require_field(input); Guard guard(state_->limits); SparseSheafActionWork work;
        auto result = apply(input, guard, work, 0u);
        work.checked_steps = guard.steps();
        return {std::move(result), work};
    }

    SparseSheafActionResult two_step_defect(const SparseSheafCochain& input) const {
        require_field(input);
        if (static_cast<std::size_t>(input.grade().value) + 2u >= state_->owner.sheaf().complex().grade_count())
            throw std::out_of_range("sparse sheaf defect requires two retained higher grades");
        Guard guard(state_->limits); SparseSheafActionWork work;
        auto intermediate = apply(input, guard, work, 0u);
        auto result = apply(intermediate, guard, work, input.nonzeros());
        work.checked_steps = guard.steps();
        return {std::move(result), work};
    }
};

} // namespace rfc::local
