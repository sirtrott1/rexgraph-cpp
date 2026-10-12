#pragma once

#include "rfc/inference/relational_finite_source.hpp"
#include "rfc/learning/relational_field_evidence.hpp"
#include <variant>

namespace rfc::learning {
using RelationalFiniteFieldTarget = std::variant<inference::OwnedRelationalFieldProgram, PreparedRelationalFieldEvidence>;
using RelationalFiniteFieldValue = std::variant<inference::OwnedAccessionFieldResult, RelationalAffineProgramResult>;
struct RelationalFiniteFieldRule {
    std::uint64_t key{};
    std::vector<std::uint64_t> bindings, symbols;
    std::optional<std::size_t> target;
    std::vector<inference::RelationalAccessionInput> inputs;
    std::string unsupported;
};
struct RelationalFiniteFieldMapDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<RelationalFiniteFieldRule> rules;
};
enum class RelationalFiniteFieldStatus : std::uint8_t { Unmapped, Unsupported, Interpreted };
namespace relational_finite_field_detail {
inline inference::OwnedRelationalFieldProgram program(const RelationalFiniteFieldTarget& target) {
    return std::visit([](const auto& t) -> inference::OwnedRelationalFieldProgram {
        if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::OwnedRelationalFieldProgram>) return t;
        else return t.program();
    }, target);
}
} // namespace relational_finite_field_detail
class RelationalFiniteFieldResult;
class PreparedRelationalFiniteFieldMap {
    struct State {
        inference::OwnedRelationalFiniteSource source;
        std::vector<RelationalFiniteFieldTarget> targets;
        RelationalFiniteFieldMapDeclaration declaration;
    };
public:
    PreparedRelationalFiniteFieldMap(inference::OwnedRelationalFiniteSource source,
        std::vector<RelationalFiniteFieldTarget> targets, RelationalFiniteFieldMapDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace inference::field_interface_detail; Guard guard(limits, std::move(check));
        const auto frozen = declaration; const auto target_pins = targets; require(frozen.key && frozen.revision, "finite field map identity/revision");
        guard.dimension(target_pins.size()); std::set<std::uint64_t> rules; std::size_t retained = add(frozen.rules.size(), target_pins.size());
        for (const auto& r : frozen.rules) {
            guard.step(); require(r.key && rules.insert(r.key).second, "finite interpretation rule identity is zero or duplicated");
            require(!r.bindings.empty() && r.bindings.size() == r.symbols.size(), "finite rule needs explicit binding/symbol dependencies");
            retained = add(retained, add(add(r.bindings.size(), r.symbols.size()), add(r.inputs.size(), r.unsupported.size())));
            guard.entries(retained); std::set<std::uint64_t> bindings, uses;
            for (std::size_t i = 0; i < r.bindings.size(); ++i) {
                guard.step(); require(bindings.insert(r.bindings[i]).second, "finite rule repeats a use binding");
                const auto& b = source.binding(r.bindings[i]); uses.insert(b.use);
                const auto& domain = source.declaration().variables[source.variable_position(b.cell)].domain;
                require(std::find(domain.begin(), domain.end(), r.symbols[i]) != domain.end(), "finite rule symbol outside declared domain");
            }
            if (!r.target) { require(r.inputs.empty() && !r.unsupported.empty(), "unsupported finite rule requires reason and no numerical inputs"); continue; }
            require(r.unsupported.empty() && *r.target < target_pins.size(), "finite rule target or unsupported state");
            const auto p = relational_finite_field_detail::program(target_pins[*r.target]); std::set<std::uint64_t> inputs;
            for (const auto& input : r.inputs) {
                guard.step(); const auto a = input.accession(); const auto actual = a.program();
                require(a.asset().same_owner(source.asset()) && p.fibers().same_owner(actual.fibers()), "finite interpretation input uses a foreign actual asset/program owner");
                require(uses.contains(input.use()), "finite numerical input use is absent from declared dependencies");
                const auto& n = node(p, a.declaration().node);
                require(n.kind == inference::FieldProgramNodeKind::Input && inputs.insert(n.key).second, "finite rule repeats an input node");
                admit_field(input.source_field(), a.declaration().accession.cols(), guard);
                const auto fibers = p.fibers();
                admit_field(input.represented(), fibers.fibers().stalks().total_dimension(n.output_grade), guard);
            }
            for (const auto& n : p.declaration().nodes) {
                if (n.kind == inference::FieldProgramNodeKind::Input) require(inputs.contains(n.key), "finite rule omits a complete program input");
            }
        }
        guard.step(); state_ = std::make_shared<const State>(State{std::move(source), target_pins, frozen});
    }
    inference::OwnedRelationalFiniteSource source() const { return state_->source; }
    const std::vector<RelationalFiniteFieldTarget>& targets() const & { return state_->targets; }
    const std::vector<RelationalFiniteFieldTarget>& targets() const && = delete;
    const RelationalFiniteFieldMapDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFiniteFieldMapDeclaration& declaration() const && = delete;
    bool same_owner(const PreparedRelationalFiniteFieldMap& other) const noexcept { return state_ == other.state_; }
    RelationalFiniteFieldResult execute(numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
struct RelationalFiniteFieldBranch {
    PreparedRelationalFiniteFieldMap map;
    inference::RelationalFiniteSections finite;
    std::size_t ordinal{}, section{};
    std::optional<std::uint64_t> rule;
    RelationalFiniteFieldStatus status{RelationalFiniteFieldStatus::Unmapped};
    std::optional<RelationalFiniteFieldValue> value;
};
class RelationalFiniteFieldResult {
    struct State {
        PreparedRelationalFiniteFieldMap map;
        inference::RelationalFiniteSections sections;
        std::vector<RelationalFiniteFieldBranch> branches;
    };
public:
    PreparedRelationalFiniteFieldMap map() const { return state_->map; }
    inference::RelationalFiniteSections sections() const { return state_->sections; }
    std::size_t size() const noexcept { return state_->branches.size(); }
    const RelationalFiniteFieldBranch& branch(std::size_t ordinal) const & { return state_->branches.at(ordinal); }
    const RelationalFiniteFieldBranch& branch(std::size_t) const && = delete;
    std::vector<RelationalFiniteFieldBranch> select(std::size_t begin, std::size_t count,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        using namespace inference::field_program_detail; const auto pin = state_; Guard guard(limits, std::move(check));
        const auto end = add(begin, count); require(end <= pin->branches.size(), "finite branch selection exceeds full result");
        guard.entries(count); std::vector<RelationalFiniteFieldBranch> out;
        for (auto i = begin; i < end; ++i) { guard.step(); out.push_back(pin->branches[i]); } return out;
    }
private:
    friend class PreparedRelationalFiniteFieldMap;
    explicit RelationalFiniteFieldResult(State s) : state_(std::make_shared<const State>(std::move(s))) {}
    std::shared_ptr<const State> state_;
};
inline RelationalFiniteFieldResult PreparedRelationalFiniteFieldMap::execute(numeric::ExactActionLimits limits,
    std::function<void()> check) const {
    using namespace inference::field_program_detail; const auto pin = *this; Guard guard(limits, check);
    const auto source = pin.source(); const auto sections = source.sections(limits, check); const auto& d = pin.declaration();
    guard.entries(machine::checked_size_mul(sections.size(), std::max(std::size_t{1}, d.rules.size())));
    std::vector<RelationalFiniteFieldBranch> branches;
    for (std::size_t section = 0; section < sections.size(); ++section) {
        bool matched{}; const auto& assignment = sections.assignment(section);
        for (const auto& r : d.rules) {
            guard.step(); bool match{true};
            for (std::size_t i = 0; i < r.bindings.size(); ++i) {
                guard.step(); if (assignment[source.variable_position(source.binding(r.bindings[i]).cell)] != r.symbols[i]) match = false;
            }
            if (!match) continue;
            matched = true;
            if (!r.target) { branches.push_back({pin, sections, branches.size(), section, r.key, RelationalFiniteFieldStatus::Unsupported, {}}); continue; }
            const auto& target = pin.targets()[*r.target]; const auto p = relational_finite_field_detail::program(target);
            auto input = inference::execute_relational_inputs(p, r.inputs, limits, check);
            auto value = std::visit([&](const auto& t) -> RelationalFiniteFieldValue {
                if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::OwnedRelationalFieldProgram>) return input;
                else return t.execute(input, limits, check);
            }, target);
            branches.push_back({pin, sections, branches.size(), section, r.key, RelationalFiniteFieldStatus::Interpreted, std::move(value)});
        }
        if (!matched) branches.push_back({pin, sections, branches.size(), section, {}, RelationalFiniteFieldStatus::Unmapped, {}});
    }
    guard.step(); return RelationalFiniteFieldResult({pin, sections, std::move(branches)});
}
} // namespace rfc::learning
