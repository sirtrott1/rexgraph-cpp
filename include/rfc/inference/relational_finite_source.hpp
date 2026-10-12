#pragma once

#include "rfc/inference/relational_field_interface.hpp"
#include "rfc/local/constraint_system.hpp"

namespace rfc::inference {
struct RelationalFiniteSymbol {
    std::uint64_t key{};
    std::vector<std::byte> definition;
};
struct RelationalFiniteVariable {
    core::CellId cell;
    std::vector<std::uint64_t> domain;
};
struct RelationalFiniteChart {
    std::uint64_t key{};
    std::string name;
    std::vector<core::CellId> patch, support;
    std::vector<std::vector<std::uint64_t>> allowed;
};
struct RelationalFiniteBinding {
    std::uint64_t key{}, use{};
    core::CellId cell;
};
struct RelationalFiniteSourceDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<RelationalFiniteSymbol> symbols;
    std::vector<RelationalFiniteVariable> variables;
    std::vector<RelationalFiniteChart> charts;
    std::vector<RelationalFiniteBinding> bindings;
};
class RelationalFiniteSections;
class OwnedRelationalFiniteSource {
    using System = local::FiniteConstraintSystem<std::uint64_t>;
    struct State {
        OwnedRelationalInputUseAsset asset;
        storage::NativeSeriesEndpoint publication;
        RelationalFiniteSourceDeclaration declaration;
        System system;
        State(OwnedRelationalInputUseAsset a, RelationalFiniteSourceDeclaration d)
            : asset(std::move(a)), publication(asset.publication()), declaration(std::move(d)),
              system(publication.endpoint().source) {}
    };
public:
    OwnedRelationalFiniteSource(OwnedRelationalInputUseAsset asset, RelationalFiniteSourceDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace field_program_detail;
        auto s = std::make_shared<State>(asset, declaration); Guard guard(limits, std::move(check));
        const auto& d = s->declaration; const auto& source = s->publication.endpoint().source;
        require(d.key && d.revision && !d.variables.empty(), "finite source requires identity, revision and variables");
        source_values(source, guard); guard.dimension(d.variables.size()); guard.dimension(d.symbols.size());
        std::size_t retained = add(add(d.variables.size(), d.symbols.size()), add(d.charts.size(), d.bindings.size()));
        guard.entries(retained); std::set<std::uint64_t> symbols, chart_keys, binding_keys;
        for (const auto& symbol : d.symbols) {
            guard.step(); require(symbol.key && symbols.insert(symbol.key).second, "finite symbol identity is zero or duplicated");
            retained = add(retained, symbol.definition.size()); guard.entries(retained);
        }
        const auto less = [](core::CellId a, core::CellId b) {
            return a.grade.value < b.grade.value || (a.grade == b.grade && a.index < b.index);
        };
        std::optional<core::CellId> previous;
        for (const auto& v : d.variables) {
            guard.step(); require(!previous || less(*previous, v.cell), "finite variables must follow distinct original cell order");
            previous = v.cell; require(!v.domain.empty(), "finite variable has no symbols");
            retained = add(retained, v.domain.size()); guard.entries(retained); std::set<std::uint64_t> unique;
            for (const auto symbol : v.domain) {
                guard.step(); require(symbols.contains(symbol) && unique.insert(symbol).second, "finite domain has unknown or repeated symbols");
            }
            s->system.set_domain(v.cell, v.domain);
        }
        for (const auto& c : d.charts) {
            guard.step(); require(c.key && chart_keys.insert(c.key).second && !c.name.empty(), "finite chart identity/name");
            retained = add(retained, add(add(c.patch.size(), c.support.size()), c.name.size()));
            algebra::SubcomplexMask patch(source); std::vector<core::CellId> seen;
            for (const auto cell : c.patch) {
                guard.step(); s->system.carrier().validate_cell(cell);
                require(std::find(seen.begin(), seen.end(), cell) == seen.end(), "finite chart repeats a patch cell");
                seen.push_back(cell); patch.grade(cell.grade).set(cell.index);
            }
            for (const auto& tuple : c.allowed) { retained = add(retained, add(tuple.size(), 1)); }
            guard.entries(retained);
            s->system.add_chart(c.name, std::move(patch), c.support, c.allowed);
        }
        const storage::StableSourceIndex index(source); std::set<std::size_t> bound;
        for (const auto& b : d.bindings) {
            guard.step(); require(b.key && binding_keys.insert(b.key).second, "finite binding identity is zero or duplicated");
            const auto position = variable_position(d, b.cell); bound.insert(position);
            const auto& use = s->asset.use(b.use); bool actual{};
            for (const auto key : use.cells) { guard.step(); actual = actual || index.cell(key) == b.cell; }
            for (const auto key : use.occurrences) {
                guard.step(); const auto slot = index.occurrence(key); require(slot.has_value(), "finite binding occurrence absent");
                const auto& arena = source.grade(key.upper_grade);
                const auto lower = core::CellId{grade(key.upper_grade.value - 1), arena.lower_index[*slot]};
                const auto upper = core::CellId{key.upper_grade, arena.upper_index[*slot]};
                actual = actual || b.cell == lower || b.cell == upper;
            }
            require(actual, "finite binding use does not contain its actual variable cell");
        }
        require(bound.size() == d.variables.size(), "finite source omits original-use bindings for variables");
        guard.step(); state_ = std::move(s);
    }
    OwnedRelationalInputUseAsset asset() const { return state_->asset; }
    storage::NativeSeriesEndpoint publication() const { return state_->publication; }
    const RelationalFiniteSourceDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFiniteSourceDeclaration& declaration() const && = delete;
    bool same_owner(const OwnedRelationalFiniteSource& other) const noexcept { return state_ == other.state_; }
    const RelationalFiniteBinding& binding(std::uint64_t key) const & {
        const auto& bindings = state_->declaration.bindings;
        const auto it = std::find_if(bindings.begin(), bindings.end(), [&](const auto& b) { return b.key == key; });
        field_program_detail::require(it != bindings.end(), "unknown finite original-use binding"); return *it;
    }
    const RelationalFiniteBinding& binding(std::uint64_t) const && = delete;
    std::size_t variable_position(core::CellId cell) const { return variable_position(state_->declaration, cell); }
    RelationalFiniteSections sections(numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    static std::size_t variable_position(const RelationalFiniteSourceDeclaration& d, core::CellId cell) {
        const auto it = std::find_if(d.variables.begin(), d.variables.end(), [&](const auto& v) { return v.cell == cell; });
        field_program_detail::require(it != d.variables.end(), "finite binding refers to a nonvariable cell");
        return static_cast<std::size_t>(it - d.variables.begin());
    }
    std::shared_ptr<const State> state_;
};
class RelationalFiniteSections {
    struct State {
        OwnedRelationalFiniteSource source;
        std::size_t candidate_count{};
        std::vector<std::vector<std::uint64_t>> assignments;
    };
public:
    OwnedRelationalFiniteSource source() const { return state_->source; }
    std::size_t candidate_count() const noexcept { return state_->candidate_count; }
    std::size_t size() const noexcept { return state_->assignments.size(); }
    const std::vector<std::uint64_t>& assignment(std::size_t ordinal) const & { return state_->assignments.at(ordinal); }
    const std::vector<std::uint64_t>& assignment(std::size_t) const && = delete;
    std::vector<std::vector<std::uint64_t>> select(std::size_t begin, std::size_t count,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        using namespace field_program_detail; const auto pin = state_; Guard guard(limits, std::move(check));
        const auto end = add(begin, count); require(end <= pin->assignments.size(), "finite section selection exceeds complete family");
        guard.entries(machine::checked_size_mul(count, pin->source.declaration().variables.size()));
        std::vector<std::vector<std::uint64_t>> out;
        for (auto i = begin; i < end; ++i) { guard.step(); out.push_back(pin->assignments[i]); } return out;
    }
private:
    friend class OwnedRelationalFiniteSource;
    explicit RelationalFiniteSections(State s) : state_(std::make_shared<const State>(std::move(s))) {}
    std::shared_ptr<const State> state_;
};
inline RelationalFiniteSections OwnedRelationalFiniteSource::sections(numeric::ExactActionLimits limits,
    std::function<void()> check) const {
    using namespace field_program_detail; const auto pin = *this; Guard guard(limits, std::move(check));
    const auto& d = pin.declaration(); guard.dimension(d.variables.size()); std::size_t count{1}, cost{1};
    for (const auto& v : d.variables) { count = machine::checked_size_mul(count, v.domain.size()); cost = add(cost, v.domain.size()); }
    for (const auto& chart : d.charts) {
        cost = add(cost, 1); for (const auto& tuple : chart.allowed) { cost = add(cost, add(tuple.size(), 1)); }
    }
    guard.entries(machine::checked_size_mul(count, d.variables.size()));
    const auto work = machine::checked_size_mul(count, cost);
    if (work > limits.steps) throw std::length_error("finite complete Cartesian search exceeds work admission");
    std::vector<std::size_t> slots(d.variables.size()); std::vector<std::vector<std::uint64_t>> assignments;
    for (std::size_t candidate = 0; candidate < count; ++candidate) {
        // The existing predicate is atomic; precharge its complete comparison bound.
        for (std::size_t i = 0; i < cost; ++i) guard.step();
        System::Assignment a; std::vector<std::uint64_t> values;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto symbol = d.variables[i].domain[slots[i]]; a.emplace(d.variables[i].cell, symbol); values.push_back(symbol);
        }
        if (pin.state_->system.globally_valid(a)) assignments.push_back(std::move(values));
        for (auto i = slots.size(); i > 0; --i) { if (++slots[i - 1] < d.variables[i - 1].domain.size()) break; slots[i - 1] = 0; }
    }
    return RelationalFiniteSections({pin, count, std::move(assignments)});
}
} // namespace rfc::inference
