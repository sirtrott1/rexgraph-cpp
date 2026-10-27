#pragma once

#include "rfc/algebra/owned_gluing.hpp"
#include "rfc/inference/relational_finite_family.hpp"
#include <map>

namespace rfc::inference {

struct RelationalFiniteGluingSymbols {
    std::uint64_t key{}, revision{};
    // One total injective map per original source variable, in its original
    // domain order, into that variable's declared glued carrier.
    std::vector<std::vector<std::uint64_t>> left, right;
};
enum class RelationalFiniteGluingStatus {
    Compatible,
    Incompatible,
    RefutedByGlobalSource,
    BudgetExceeded,
    Cancelled
};

namespace finite_family_detail {
struct GluingState {
    OwnedRelationalFiniteSource left, right, global;
    algebra::OwnedRelationalGluing structural;
    RelationalFiniteGluingSymbols symbols;
    std::vector<core::CellId> left_cells, right_cells;
    std::size_t retained{}, preparation_steps{};
};
} // namespace finite_family_detail

class OwnedRelationalFiniteGluedSection;
struct RelationalFiniteGluingResult;

class OwnedRelationalFiniteGluing {
    using State = finite_family_detail::GluingState;

  public:
    static OwnedRelationalFiniteGluing
    prepare(core::RelationalComplex common, OwnedRelationalFiniteSource left,
            OwnedRelationalFiniteSource right, OwnedRelationalFiniteSource global,
            algebra::RelationalGluingDeclaration source_maps, RelationalFiniteGluingSymbols symbol_maps,
            numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace finite_family_detail;
        // Copy map words before invoking a caller checkpoint. A moved vector
        // can still have caller-held references to its transferred storage.
        auto maps = source_maps;
        auto symbols = symbol_maps;
        const auto lp = left.publication(), rp = right.publication(), gp = global.publication();
        numeric::ExactActionGuard guard(limits, std::move(check));
        auto structural = algebra::OwnedRelationalGluing::prepare(
            std::move(common), lp.endpoint().source, rp.endpoint().source, gp.endpoint().source, maps, guard);
        field_program_detail::require(symbols.key && symbols.revision,
                                      "finite gluing symbol-map identity/revision");
        auto s = std::make_shared<State>(State{std::move(left),
                                               std::move(right),
                                               std::move(global),
                                               std::move(structural),
                                               std::move(symbols),
                                               {},
                                               {},
                                               0,
                                               0});
        s->retained = add(s->structural.retained_words(), 16);
        guard.entries(s->retained);
        std::map<core::CellId, unsigned, decltype(&less)> covered(&less);
        for (const bool r : {false, true}) {
            const auto &source = r ? s->right : s->left;
            const auto &values = r ? s->symbols.right : s->symbols.left;
            auto &cells = r ? s->right_cells : s->left_cells;
            const auto &inclusion =
                r ? s->structural.declaration().right_glued : s->structural.declaration().left_glued;
            field_program_detail::require(values.size() == source.declaration().variables.size(),
                                          "finite gluing omits a variable symbol map");
            guard.dimension(values.size());
            s->retained = add(s->retained, values.size());
            guard.entries(s->retained);
            for (std::size_t i = 0; i < values.size(); ++i) {
                guard.step();
                const auto &v = source.declaration().variables[i];
                const core::CellId target{v.cell.grade,
                                          inclusion.cells.at(v.cell.grade.value).at(v.cell.index)};
                const auto &target_domain = domain(s->global, target, guard);
                field_program_detail::require(values[i].size() == v.domain.size(),
                                              "finite gluing symbol map is not total");
                s->retained = add(s->retained, add(values[i].size(), 2));
                guard.entries(s->retained);
                std::set<Id> unique;
                for (const auto value : values[i]) {
                    (void)find(target_domain, value, guard);
                    field_program_detail::require(unique.insert(value).second,
                                                  "finite gluing symbol map is not injective");
                }
                cells.push_back(target);
                covered[target] |= r ? 2u : 1u;
            }
        }
        field_program_detail::require(covered.size() == s->global.declaration().variables.size(),
                                      "finite gluing does not cover all global variable fields");
        const auto &structural_maps = s->structural.declaration();
        for (std::size_t g = 0; g < structural_maps.common_left.cells.size(); ++g) {
            for (const auto local : structural_maps.common_left.cells[g]) {
                guard.step();
                const core::CellId shared{{static_cast<std::uint16_t>(g)},
                                          structural_maps.left_glued.cells[g][local]};
                const auto at = covered.find(shared);
                field_program_detail::require(
                    at == covered.end() || at->second == 3u,
                    "finite gluing requires both parent fields on every shared variable cell");
            }
        }
        s->preparation_steps = guard.steps();
        return OwnedRelationalFiniteGluing(std::move(s));
    }
    const OwnedRelationalFiniteSource &left() const & noexcept {
        return state_->left;
    }
    const OwnedRelationalFiniteSource &left() const && = delete;
    const OwnedRelationalFiniteSource &right() const & noexcept {
        return state_->right;
    }
    const OwnedRelationalFiniteSource &right() const && = delete;
    const OwnedRelationalFiniteSource &global() const & noexcept {
        return state_->global;
    }
    const OwnedRelationalFiniteSource &global() const && = delete;
    const algebra::OwnedRelationalGluing &structural() const & noexcept {
        return state_->structural;
    }
    const algebra::OwnedRelationalGluing &structural() const && = delete;
    const RelationalFiniteGluingSymbols &symbols() const & noexcept {
        return state_->symbols;
    }
    const RelationalFiniteGluingSymbols &symbols() const && = delete;
    std::size_t retained_entries() const noexcept {
        return state_->retained;
    }
    std::size_t preparation_steps() const noexcept {
        return state_->preparation_steps;
    }
    bool same_owner(const OwnedRelationalFiniteGluing &other) const noexcept {
        return state_ == other.state_;
    }
    RelationalFiniteGluingResult glue(RelationalFiniteFamilyExtension left, std::size_t left_leaf,
                                      RelationalFiniteFamilyExtension right, std::size_t right_leaf,
                                      RelationalFiniteFamilyLimits limits = {},
                                      std::stop_token stop = {}) const;

  private:
    explicit OwnedRelationalFiniteGluing(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

class OwnedRelationalFiniteGluedSection {
    struct State {
        OwnedRelationalFiniteGluing gluing;
        RelationalFiniteFamilyExtension left, right;
        std::size_t left_leaf{}, right_leaf{};
        OwnedRelationalFiniteSource::Assignment field;
    };

  public:
    const OwnedRelationalFiniteGluing &gluing() const & noexcept {
        return state_->gluing;
    }
    const OwnedRelationalFiniteGluing &gluing() const && = delete;
    const RelationalFiniteFamilyExtension &left() const & noexcept {
        return state_->left;
    }
    const RelationalFiniteFamilyExtension &left() const && = delete;
    const RelationalFiniteFamilyExtension &right() const & noexcept {
        return state_->right;
    }
    const RelationalFiniteFamilyExtension &right() const && = delete;
    const OwnedRelationalFiniteSource::Assignment &field() const & noexcept {
        return state_->field;
    }
    const OwnedRelationalFiniteSource::Assignment &field() const && = delete;
    std::size_t left_leaf() const noexcept {
        return state_->left_leaf;
    }
    std::size_t right_leaf() const noexcept {
        return state_->right_leaf;
    }

  private:
    friend class OwnedRelationalFiniteGluing;
    OwnedRelationalFiniteGluedSection(OwnedRelationalFiniteGluing gluing,
                                      RelationalFiniteFamilyExtension left, std::size_t li,
                                      RelationalFiniteFamilyExtension right, std::size_t ri,
                                      OwnedRelationalFiniteSource::Assignment field)
        : state_(std::make_shared<const State>(
              State{std::move(gluing), std::move(left), std::move(right), li, ri, std::move(field)})) {}
    std::shared_ptr<const State> state_;
};
struct RelationalFiniteGluingResult {
    RelationalFiniteGluingStatus status;
    std::optional<OwnedRelationalFiniteGluedSection> section;
    RelationalFiniteFamilyWork work;
};

inline RelationalFiniteGluingResult
OwnedRelationalFiniteGluing::glue(RelationalFiniteFamilyExtension left, std::size_t left_leaf,
                                  RelationalFiniteFamilyExtension right, std::size_t right_leaf,
                                  RelationalFiniteFamilyLimits limits, std::stop_token stop) const {
    using namespace finite_family_detail;
    const auto pin = *this;
    field_program_detail::require(left.family().source(left_leaf).same_owner(pin.state_->left) &&
                                      right.family().source(right_leaf).same_owner(pin.state_->right),
                                  "finite gluing extension has a foreign actual source owner");
    State const &s = *pin.state_;
    // Reuse the exact action budget/counter policy. This descriptor owns no
    // additional solver and never treats an incompatible pair as global UNSAT.
    finite_family_detail::State budget_state(s.global);
    budget_state.retained = s.retained;
    Action action(budget_state, limits, stop);
    try {
        action.initialize();
        const auto n = s.global.declaration().variables.size();
        if (n > limits.max_variables)
            throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        action.keep(add(add(n, n), 32));
        Assignment fixed;
        fixed.reserve(n);
        for (const bool r : {false, true}) {
            const auto &source = r ? s.right : s.left;
            const auto &maps = r ? s.symbols.right : s.symbols.left;
            const auto &cells = r ? s.right_cells : s.left_cells;
            const auto &field = r ? right.witness(right_leaf) : left.witness(left_leaf);
            for (std::size_t i = 0; i < cells.size(); ++i) {
                action.step();
                const auto &variable = source.declaration().variables[i];
                const auto value = field.at(variable.cell);
                std::size_t position = 0;
                for (; position < variable.domain.size(); ++position) {
                    action.step();
                    if (variable.domain[position] == value)
                        break;
                }
                if (position == variable.domain.size())
                    throw std::logic_error("owned finite extension has an invalid whole field");
                const auto [at, inserted] = fixed.emplace(cells[i], maps[i][position]);
                if (!inserted && at->second != maps[i][position])
                    return {RelationalFiniteGluingStatus::Incompatible, {}, action.work};
            }
        }
        action.step();
        ++action.work.preparations;
        auto prepared = s.global.prepare_global_section({}, action.native_bounds(), stop);
        action.record_native(prepared.work);
        if (!prepared.prepared)
            throw Interrupted{prepared.status == local::FinitePreparationStatus::Cancelled
                                  ? local::FiniteSearchStatus::Cancelled
                                  : local::FiniteSearchStatus::BudgetExceeded};
        action.keep(prepared.prepared->retained_entries());
        if (!limits.max_candidates)
            throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        action.step();
        ++action.work.candidates;
        ++action.work.source_queries;
        auto result = prepared.prepared->global_section(fixed, action.native_bounds(), stop);
        action.record_native(result.work);
        if (result.status == local::FiniteSearchStatus::Cancelled ||
            result.status == local::FiniteSearchStatus::BudgetExceeded)
            throw Interrupted{result.status};
        ++action.work.completed_queries;
        if (!result.witness)
            return {RelationalFiniteGluingStatus::RefutedByGlobalSource, {}, action.work};
        if (!limits.max_rows)
            throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        action.work.rows = 1;
        action.work.witness_values = result.witness->size();
        return {RelationalFiniteGluingStatus::Compatible,
                OwnedRelationalFiniteGluedSection(pin, std::move(left), left_leaf, std::move(right),
                                                  right_leaf, std::move(*result.witness)),
                action.work};
    } catch (const Interrupted &e) {
        return {e.status == local::FiniteSearchStatus::Cancelled
                    ? RelationalFiniteGluingStatus::Cancelled
                    : RelationalFiniteGluingStatus::BudgetExceeded,
                {},
                action.work};
    }
}

} // namespace rfc::inference
