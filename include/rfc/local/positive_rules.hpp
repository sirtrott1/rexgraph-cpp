#pragma once

#include "rfc/local/carrier.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace rfc::local {

// Finite ground positive rules are a declared logical view of retained cells.
// Neither coefficients nor orientation bits implicitly define logical negation.
// No grounding, new source cell, function symbol, or closed-world false is added.
struct PositiveRule {
    core::CellId identity;                  // actual retained primary relation
    std::vector<core::CellId> premises;      // original ordered occurrences
    core::CellId conclusion;
};
struct PositiveCompileLimits {
    std::size_t max_facts{65536}, max_rules{65536}, max_premises{1u << 20};
    std::size_t max_steps{1u << 22};
};
struct PositiveCompileReceipt {
    std::size_t steps{}, facts{}, rules{}, premise_records{}, unique_premises{};
    std::size_t watch_entries{};
};
struct PositiveRunLimits {
    std::size_t max_seed_records{65536}, max_derived_facts{65536};
    std::size_t max_justifications{65536}, max_steps{1u << 22};
};
struct PositiveRunReceipt {
    std::size_t steps{}, seed_records{}, unique_seeds{}, processed_facts{};
    std::size_t watcher_visits{}, rule_firings{}, derived_facts{};
    std::size_t verified_derivations{}, verified_rules{}, verified_premises{};
    std::size_t peak_queue{}, generated_global_tables{};
};
enum class PositiveRunStatus { Complete, BudgetExceeded, Cancelled };
struct PositivePreparationCancelled {};
struct PositiveDerivation {
    core::CellId fact;
    // No rule means this fact is an explicitly supplied seed. Repeated/cyclic
    // rules are recorded separately; this first derivation is acyclic.
    std::optional<core::CellId> rule;
};

namespace positive_detail {
struct Interrupted { PositiveRunStatus status; };
struct ProgramState {
    std::shared_ptr<const core::RelationalComplex> owner;
    std::vector<core::CellId> facts;
    std::vector<PositiveRule> rules;
    std::map<core::CellId, std::size_t> positions;
    std::vector<std::vector<std::size_t>> unique, watchers;
    std::vector<std::size_t> heads;
    PositiveCompileReceipt receipt;
};
struct ClosureState {
    std::shared_ptr<const ProgramState> program;
    std::vector<core::CellId> seeds;
    std::vector<std::uint8_t> truth;
    std::vector<PositiveDerivation> derivations;
    std::vector<core::CellId> enabled_rules;
};
} // namespace positive_detail

class FinitePositiveProgram;
class PositiveClosure {
    std::shared_ptr<const positive_detail::ClosureState> state_;
    explicit PositiveClosure(std::shared_ptr<const positive_detail::ClosureState> state)
        : state_(std::move(state)) {}
    friend class FinitePositiveProgram;
public:
    [[nodiscard]] bool contains(core::CellId fact) const {
        const auto it = state_->program->positions.find(fact);
        if (it == state_->program->positions.end())
            throw std::invalid_argument("fact is outside the declared positive universe");
        return state_->truth[it->second] != 0;
    }
    [[nodiscard]] std::size_t size() const noexcept { return state_->derivations.size(); }
    [[nodiscard]] const auto& seeds() const & noexcept { return state_->seeds; }
    const std::vector<core::CellId>& seeds() const && = delete;
    [[nodiscard]] const auto& derivations() const & noexcept { return state_->derivations; }
    const std::vector<PositiveDerivation>& derivations() const && = delete;
    [[nodiscard]] const auto& enabled_rules() const & noexcept { return state_->enabled_rules; }
    const std::vector<core::CellId>& enabled_rules() const && = delete;
    [[nodiscard]] const core::RelationalComplex& source() const & noexcept { return *state_->program->owner; }
    const core::RelationalComplex& source() const && = delete;
    void require_for(const FinitePositiveProgram&) const;
};
class PositiveClosureRun {
    PositiveRunStatus status_;
    PositiveRunReceipt receipt_;
    std::optional<PositiveClosure> closure_;
    PositiveClosureRun(PositiveRunStatus s, PositiveRunReceipt r, std::optional<PositiveClosure> c)
        : status_(s), receipt_(r), closure_(std::move(c)) {}
    friend class FinitePositiveProgram;
public:
    [[nodiscard]] PositiveRunStatus status() const noexcept { return status_; }
    [[nodiscard]] PositiveRunReceipt receipt() const noexcept { return receipt_; }
    [[nodiscard]] const std::optional<PositiveClosure>& closure() const & noexcept { return closure_; }
    const std::optional<PositiveClosure>& closure() const && = delete;
};

class FinitePositiveProgram {
    std::shared_ptr<const positive_detail::ProgramState> state_;
    explicit FinitePositiveProgram(std::shared_ptr<const positive_detail::ProgramState> state)
        : state_(std::move(state)) {}
    friend class PositiveClosure;
public:
    // One index entry per explicitly declared fact, never per ambient source
    // coordinate or possible assignment. Equal-content independent programs
    // retain different identities. The caller supplies actual immutable ownership.
    static FinitePositiveProgram compile(std::shared_ptr<const core::RelationalComplex> owner,
            std::vector<core::CellId> facts, std::vector<PositiveRule> rules,
            PositiveCompileLimits limits = {}, std::stop_token stop = {}) {
        PositiveCompileReceipt receipt;
        const auto step = [&] {
            if (stop.stop_requested()) throw PositivePreparationCancelled{};
            if (receipt.steps == limits.max_steps) throw std::length_error("positive preparation step limit");
            ++receipt.steps;
        };
        step();
        if (!owner) throw std::invalid_argument("positive program requires an owned source");
        if (facts.size() > limits.max_facts || rules.size() > limits.max_rules)
            throw std::length_error("positive preparation record limit");
        for (const auto& rule : rules) {
            step();
            if (rule.premises.size() > limits.max_premises - receipt.premise_records)
                throw std::length_error("positive premise record limit");
            receipt.premise_records += rule.premises.size();
        }
        auto state = std::make_shared<positive_detail::ProgramState>();
        state->owner = std::move(owner);
        const RelationalLocalCarrier carrier(*state->owner);
        for (std::size_t i = 0; i < facts.size(); ++i) {
            step(); carrier.validate_cell(facts[i]);
            if (!state->positions.emplace(facts[i], i).second)
                throw std::invalid_argument("duplicate positive fact identity");
        }
        state->facts = std::move(facts);
        state->watchers.resize(state->facts.size());
        state->unique.reserve(rules.size()); state->heads.reserve(rules.size());
        std::set<core::CellId> identities;
        const auto position = [&](core::CellId c) {
            const auto it = state->positions.find(c);
            if (it == state->positions.end()) throw std::invalid_argument("rule fact outside declared universe");
            return it->second;
        };
        for (std::size_t ri = 0; ri < rules.size(); ++ri) {
            const auto& rule = rules[ri]; step(); carrier.validate_cell(rule.identity);
            if (rule.identity.grade.value == 0 || !identities.insert(rule.identity).second)
                throw std::invalid_argument("rule requires a distinct retained primary identity");
            state->heads.push_back(position(rule.conclusion));
            std::set<std::size_t> unique;
            std::vector<std::size_t> body;
            for (const auto p : rule.premises) {
                step(); const auto pi = position(p);
                if (unique.insert(pi).second) {
                    body.push_back(pi); state->watchers[pi].push_back(ri);
                    ++receipt.unique_premises;
                }
            }
            state->unique.push_back(std::move(body));
        }
        state->rules = std::move(rules);
        receipt.facts = state->facts.size(); receipt.rules = state->rules.size();
        receipt.watch_entries = receipt.unique_premises;
        step(); state->receipt = receipt;
        return FinitePositiveProgram(std::move(state));
    }
    [[nodiscard]] PositiveCompileReceipt receipt() const noexcept { return state_->receipt; }
    [[nodiscard]] bool same_program(const FinitePositiveProgram& p) const noexcept { return state_ == p.state_; }
    [[nodiscard]] const auto& facts() const & noexcept { return state_->facts; }
    const std::vector<core::CellId>& facts() const && = delete;
    [[nodiscard]] const auto& rules() const & noexcept { return state_->rules; }
    const std::vector<PositiveRule>& rules() const && = delete;
    [[nodiscard]] const core::RelationalComplex& source() const & noexcept { return *state_->owner; }
    const core::RelationalComplex& source() const && = delete;

    [[nodiscard]] PositiveClosureRun run(std::span<const core::CellId> seeds,
            PositiveRunLimits limits = {}, std::stop_token stop = {}) const {
        PositiveRunReceipt receipt;
        const auto step = [&] {
            if (stop.stop_requested()) throw positive_detail::Interrupted{PositiveRunStatus::Cancelled};
            if (receipt.steps == limits.max_steps)
                throw positive_detail::Interrupted{PositiveRunStatus::BudgetExceeded};
            ++receipt.steps;
        };
        const auto budget = [&](bool b) {
            if (!b) throw positive_detail::Interrupted{PositiveRunStatus::BudgetExceeded};
        };
        try {
            step(); budget(seeds.size() <= limits.max_seed_records);
            // Storage is bounded by the explicitly admitted finite facts/rules.
            auto data = std::make_shared<positive_detail::ClosureState>();
            data->program = state_;
            const auto n = state_->facts.size(), m = state_->rules.size();
            constexpr auto unseen = std::numeric_limits<std::size_t>::max();
            data->truth.assign(n, 0);
            std::vector<std::uint8_t> seeded(n, 0), fired(m, 0);
            std::vector<std::size_t> birth(n, unseen), reason(n, unseen), remaining(m), queue;
            std::size_t cursor = 0;
            const auto insert = [&](std::size_t p, std::size_t why) {
                step();
                if (data->truth[p]) return;
                budget(data->derivations.size() < limits.max_derived_facts);
                birth[p] = data->derivations.size(); reason[p] = why; data->truth[p] = 1;
                data->derivations.push_back({state_->facts[p], why == unseen ? std::nullopt :
                    std::optional<core::CellId>{state_->rules[why].identity}});
                queue.push_back(p); ++receipt.derived_facts;
                receipt.peak_queue = std::max(receipt.peak_queue, queue.size() - cursor);
            };
            for (const auto seed : seeds) {
                step(); ++receipt.seed_records;
                const auto it = state_->positions.find(seed);
                if (it == state_->positions.end()) throw std::invalid_argument("seed outside declared positive universe");
                data->seeds.push_back(seed);
                if (!seeded[it->second]) { seeded[it->second] = 1; ++receipt.unique_seeds; }
                insert(it->second, unseen);
            }
            const auto fire = [&](std::size_t r) {
                step();
                if (fired[r]) throw std::logic_error("positive rule fired twice");
                budget(data->enabled_rules.size() < limits.max_justifications);
                fired[r] = 1; ++receipt.rule_firings;
                data->enabled_rules.push_back(state_->rules[r].identity);
                insert(state_->heads[r], r);
            };
            for (std::size_t r = 0; r < m; ++r) {
                step(); remaining[r] = state_->unique[r].size();
                if (remaining[r] == 0) fire(r);
            }
            while (cursor < queue.size()) {
                step(); const auto p = queue[cursor++]; ++receipt.processed_facts;
                for (const auto r : state_->watchers[p]) {
                    step(); ++receipt.watcher_visits;
                    if (remaining[r] == 0) throw std::logic_error("positive watch processed twice");
                    if (--remaining[r] == 0) fire(r);
                }
            }
            // Verify derivability in insertion order, separately from closure.
            // Derivability places the result inside every seed-containing model;
            // closure makes it a model, hence it is the least such model.
            for (std::size_t p = 0; p < n; ++p) {
                step();
                if (!data->truth[p]) continue;
                ++receipt.verified_derivations;
                if (reason[p] == unseen) {
                    if (!seeded[p]) throw std::logic_error("unjustified positive seed");
                } else {
                    const auto r = reason[p];
                    if (state_->heads[r] != p || !fired[r]) throw std::logic_error("bad positive derivation");
                    for (const auto q : state_->unique[r]) {
                        step(); ++receipt.verified_premises;
                        if (!data->truth[q] || birth[q] >= birth[p])
                            throw std::logic_error("cyclic or ungrounded first derivation");
                    }
                }
            }
            for (std::size_t r = 0; r < m; ++r) {
                step(); ++receipt.verified_rules;
                bool enabled = true;
                for (const auto p : state_->unique[r]) {
                    step(); ++receipt.verified_premises;
                    enabled = enabled && data->truth[p] != 0;
                }
                if (enabled != (fired[r] != 0) || (enabled && !data->truth[state_->heads[r]]))
                    throw std::logic_error("positive result is not closed");
            }
            step();
            const PositiveClosure closed{std::move(data)};
            return PositiveClosureRun(PositiveRunStatus::Complete, receipt, closed);
        } catch (const positive_detail::Interrupted& interrupted) {
            return PositiveClosureRun(interrupted.status, receipt, std::nullopt);
        }
    }
};
inline void PositiveClosure::require_for(const FinitePositiveProgram& program) const {
    if (state_->program != program.state_) throw std::invalid_argument("positive certificate program owners differ");
}

} // namespace rfc::local
