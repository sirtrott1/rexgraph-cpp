#pragma once

#include "rfc/learning/relational_finite_field_codec.hpp"
#include "rfc/inference/relational_value_admission.hpp"

namespace rfc::learning {
struct RelationalFiniteOperatorPort { std::uint64_t key{}, rule{}; std::size_t input{}; };
struct RelationalFiniteOperatorDeclaration { std::uint64_t key{}, revision{}; std::vector<RelationalFiniteOperatorPort> ports; };
struct RelationalFiniteUseCorrespondence { std::uint64_t reference{}, input{}; };
struct RelationalFiniteSourceTransportDeclaration {
    std::uint64_t key{}, revision{};
    algebra::RetainedSourceMap source_map;
    std::vector<RelationalFiniteUseCorrespondence> uses;
};
struct RelationalFiniteOperatorInstanceDeclaration {
    std::uint64_t key{}, revision{};
    RelationalFiniteSourceTransportDeclaration transport;
};
struct RelationalFiniteOperatorInput { std::uint64_t port{}; inference::RelationalInputSubmission input; };
struct RelationalFiniteVariableCorrespondence {
    std::size_t reference{}, input{};
    core::CellId reference_cell, input_cell;
};
namespace finite_operator_detail {
using namespace inference::field_interface_detail;
using Bytes = std::vector<std::byte>;
using Source = inference::OwnedRelationalFiniteSource;
using Asset = inference::OwnedRelationalInputUseAsset;
using Declaration = RelationalFiniteSourceTransportDeclaration;
inline Bytes schema_bytes(const inference::RelationalFiniteSourceDeclaration& d) {
    inference::relational_finite_source_codec_detail::Writer w;
    inference::relational_finite_source_codec_detail::declaration(w, d); return std::move(w).take_bytes();
}
inline Bytes interface_bytes(const inference::RelationalInputAccessionDeclaration& d) {
    inference::relational_finite_source_codec_detail::Writer w;
    inference::relational_finite_source_codec_detail::accession(w, d); return std::move(w).take_bytes();
}
inline std::uint64_t use(const Declaration& d, std::uint64_t reference) {
    const auto it = std::find_if(d.uses.begin(), d.uses.end(), [&](const auto& u) { return u.reference == reference; });
    require(it != d.uses.end(), "finite operator source use is not declared"); return it->input;
}
inline core::CellId cell(const Declaration& d, core::CellId original) {
    return {original.grade, d.source_map.cells.at(original.grade.value).at(original.index)};
}
// Precursor builders check bounded routing, not geometric compatibility or A/R.
inline void routing(const Asset& reference, const Asset& input, const Declaration& d, Guard& guard) {
    const auto old = reference.publication(); const auto next = input.publication();
    const auto& a = old.endpoint().source; const auto& b = next.endpoint().source; const auto& m = d.source_map;
    require(d.key && d.revision, "finite source transport identity/revision");
    require(a.grade_count() == b.grade_count() && m.cells.size() == a.grade_count() && m.occurrences.size() == a.grade_count(), "finite transport must declare every actual grade");
    guard.entries(add(algebra::owned_gluing_detail::words(m), d.uses.size()));
    for (std::size_t k = 0; k < a.grade_count(); ++k) {
        const auto g = grade(k); const auto& x = a.grade(g); const auto& y = b.grade(g);
        require(x.cell_count() == y.cell_count() && x.incidence_count() == y.incidence_count(), "finite source transport requires complete bijective carriers");
        require(m.cells[k].size() == x.cell_count() && m.occurrences[k].size() == x.incidence_count(), "finite source transport omits routing positions");
        std::set<std::uint32_t> cells; std::set<std::size_t> occurrences;
        for (const auto i : m.cells[k]) { guard.step(); require(i < y.cell_count() && cells.insert(i).second, "finite source cell routing is not bijective"); }
        for (const auto i : m.occurrences[k]) { guard.step(); require(i < y.incidence_count() && occurrences.insert(i).second, "finite source occurrence routing is not bijective"); }
    }
    require(d.uses.size() == reference.declaration().uses.size() && d.uses.size() == input.declaration().uses.size(), "finite transport must retain every original use");
    std::set<std::uint64_t> inputs;
    for (std::size_t i = 0; i < d.uses.size(); ++i) {
        guard.step(); require(d.uses[i].reference == reference.declaration().uses[i].key && inputs.insert(d.uses[i].input).second, "finite transport use order or bijection");
        (void)input.use(d.uses[i].input);
    }
}
inline inference::RelationalFiniteSourceDeclaration source_schema(const Source& reference, const Asset& input,
    const Declaration& d, Guard& guard) {
    routing(reference.asset(), input, d, guard); const auto& original = reference.declaration();
    guard.dimension(original.variables.size()); guard.dimension(original.symbols.size());
    auto retained = add(add(original.variables.size(), original.symbols.size()), add(original.charts.size(), original.bindings.size()));
    for (const auto& symbol : original.symbols) { guard.step(); retained = add(retained, symbol.definition.size()); }
    for (const auto& variable : original.variables) { guard.step(); retained = add(retained, variable.domain.size()); }
    for (const auto& chart : original.charts) {
        guard.step(); retained = add(retained, add(add(chart.patch.size(), chart.support.size()), chart.name.size()));
        for (const auto& tuple : chart.allowed) { guard.step(); retained = add(retained, add(tuple.size(), 1)); }
    }
    guard.entries(retained); auto out = original;
    for (auto& v : out.variables) { guard.step(); v.cell = cell(d, v.cell); }
    std::sort(out.variables.begin(), out.variables.end(), [](const auto& a, const auto& b) {
        return a.cell.grade.value < b.cell.grade.value || (a.cell.grade == b.cell.grade && a.cell.index < b.cell.index);
    });
    for (auto& c : out.charts) {
        for (auto& x : c.patch) { guard.step(); x = cell(d, x); }
        for (auto& x : c.support) { guard.step(); x = cell(d, x); }
    }
    for (auto& b : out.bindings) { guard.step(); b.cell = cell(d, b.cell); b.use = use(d, b.use); }
    return out;
}
inline inference::RelationalInputAccessionDeclaration input_schema(const inference::RelationalInputAccessionDeclaration& original,
    const Declaration& d, Guard& guard) {
    guard.dimension(original.source_stalks.size()); auto retained = algebra::owned_gluing_detail::words(original.instances);
    for (const auto& stalk : original.source_stalks) {
        guard.step(); guard.dimension(stalk.directions.size()); guard.matrix(stalk.metric);
        retained = add(retained, add(stalk.directions.size(), stalk.metric.nonzeros()));
    }
    retained = add(retained, add(original.accession.nonzeros(), original.reconstruction.nonzeros())); guard.entries(retained);
    auto out = original; const auto& cells = d.source_map.cells.at(original.source_grade.value);
    require(cells.size() == original.source_stalks.size(), "finite port source stalk routing differs");
    auto stalks = original.source_stalks; std::vector<std::size_t> before(add(cells.size(), 1)), after(add(cells.size(), 1));
    guard.entries(add(before.size(), after.size()));
    for (std::size_t i = 0; i < cells.size(); ++i) { guard.step(); stalks.at(cells[i]) = original.source_stalks[i]; stalks[cells[i]].cell.index = cells[i]; }
    for (std::size_t i = 0; i < cells.size(); ++i) {
        guard.step(); before[i + 1] = add(before[i], original.source_stalks[i].directions.size()); after[i + 1] = add(after[i], stalks[i].directions.size());
    }
    guard.dimension(before.back()); guard.dimension(after.back()); std::vector<std::size_t> coordinates(before.back()); guard.entries(coordinates.size());
    for (std::size_t i = 0; i < cells.size(); ++i) for (std::size_t j = 0; j < original.source_stalks[i].directions.size(); ++j) {
        guard.step(); coordinates[add(before[i], j)] = add(after[cells[i]], j);
    }
    out.source_stalks = std::move(stalks);
    for (std::size_t k = 0; k < d.source_map.cells.size(); ++k) {
        require(original.instances.cells.at(k).size() == d.source_map.cells[k].size() && original.instances.occurrences.at(k).size() == d.source_map.occurrences[k].size(), "finite port instance routing is incomplete");
        for (std::size_t i = 0; i < d.source_map.cells[k].size(); ++i) { guard.step(); out.instances.cells[k][d.source_map.cells[k][i]] = original.instances.cells[k][i]; }
        for (std::size_t i = 0; i < d.source_map.occurrences[k].size(); ++i) { guard.step(); out.instances.occurrences[k][d.source_map.occurrences[k][i]] = original.instances.occurrences[k][i]; }
    }
    require(original.accession.cols() == coordinates.size() && original.reconstruction.rows() == coordinates.size(), "finite port coefficient routing carrier");
    guard.matrix(original.accession); guard.matrix(original.reconstruction);
    out.accession = Matrix(original.accession.rows(), after.back()); out.reconstruction = Matrix(after.back(), original.reconstruction.cols());
    for (const auto& [ij, q] : original.accession.entries()) { guard.step(); out.accession.set(ij.first, coordinates[ij.second], q); }
    for (const auto& [ij, q] : original.reconstruction.entries()) { guard.step(); out.reconstruction.set(coordinates[ij.first], ij.second, q); }
    return out;
}
template<class T> inline std::vector<T> select(const std::vector<T>& rows, std::size_t begin, std::size_t count, Guard& guard) {
    const auto end = add(begin, count); require(end <= rows.size(), "finite operator bounded selection"); guard.entries(count); std::vector<T> out;
    for (auto i = begin; i < end; ++i) { guard.step(); out.push_back(rows[i]); } return out;
}
} // namespace finite_operator_detail
class OwnedRelationalFiniteSourceTransport {
    struct State {
        inference::OwnedRelationalFiniteSource reference, source;
        RelationalFiniteSourceTransportDeclaration declaration;
        std::vector<RelationalFiniteVariableCorrespondence> variables;
    };
public:
    OwnedRelationalFiniteSourceTransport(inference::OwnedRelationalFiniteSource reference,
        inference::OwnedRelationalFiniteSource source, RelationalFiniteSourceTransportDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace finite_operator_detail; const auto old = reference; const auto input = source; const auto d = declaration; Guard guard(limits, std::move(check));
        const auto a = old.asset(); const auto b = input.asset(); routing(a, b, d, guard);
        const auto ap = a.publication(); const auto bp = b.publication(); const auto& from = ap.endpoint().source; const auto& to = bp.endpoint().source;
        source_values(from, guard); source_values(to, guard); algebra::owned_gluing_detail::map(from, to, d.source_map, true, guard);
        for (std::size_t k = 0; k < from.grade_count(); ++k) {
            const auto g = grade(k); const auto& x = from.grade(g); const auto& y = to.grade(g);
            for (std::size_t i = 0; i < x.cell_count(); ++i) {
                guard.step(); const auto j = d.source_map.cells[k][i]; require(x.cell_scales[i] == y.cell_scales[j], "finite transport changes intrinsic cell scale");
                if (!k) continue;
                const auto [begin, end] = x.incidence_range(i); const auto target = y.incidence_range(j).first;
                for (auto p = begin; p < end; ++p) {
                    guard.step(); const auto q = d.source_map.occurrences[k][p];
                    require(q == add(target, p - begin) && x.lattice_coefficient[p] == y.lattice_coefficient[q], "finite transport changes original ordered flag or unreduced magnitude");
                }
            }
        }
        const storage::StableSourceIndex index(from);
        for (const auto& pair : d.uses) {
            const auto& left = a.use(pair.reference); const auto& right = b.use(pair.input);
            require(left.cells.size() == right.cells.size() && left.occurrences.size() == right.occurrences.size(), "finite transport changes complete use bindings");
            for (std::size_t i = 0; i < left.cells.size(); ++i) { guard.step(); const auto c = index.cell(left.cells[i]); require(c && storage::stable_cell_key(to, cell(d, *c)) == right.cells[i], "finite transport changes ordered original-use cells"); }
            for (std::size_t i = 0; i < left.occurrences.size(); ++i) {
                guard.step(); const auto& c = left.occurrences[i]; const auto slot = index.occurrence(c); require(slot.has_value(), "finite reference use occurrence");
                const storage::StableOccurrenceKey mapped{c.upper_grade, to.occurrence_identity(c.upper_grade, d.source_map.occurrences[c.upper_grade.value][*slot])};
                require(mapped == right.occurrences[i], "finite transport changes ordered original-use occurrences");
            }
        }
        require(schema_bytes(source_schema(old, b, d, guard)) == schema_bytes(input.declaration()), "finite operator input changes original symbolic schema");
        std::vector<RelationalFiniteVariableCorrespondence> variables; guard.entries(old.declaration().variables.size());
        for (std::size_t i = 0; i < old.declaration().variables.size(); ++i) {
            guard.step(); const auto c = old.declaration().variables[i].cell; const auto image = cell(d, c); variables.push_back({i, input.variable_position(image), c, image});
        }
        guard.step(); state_ = std::make_shared<const State>(State{old, input, d, std::move(variables)});
    }
    inference::OwnedRelationalFiniteSource reference() const { return state_->reference; }
    inference::OwnedRelationalFiniteSource source() const { return state_->source; }
    const RelationalFiniteSourceTransportDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFiniteSourceTransportDeclaration& declaration() const && = delete;
    bool same_owner(const OwnedRelationalFiniteSourceTransport& other) const noexcept { return state_ == other.state_; }
    std::vector<RelationalFiniteVariableCorrespondence> variable_positions(std::size_t begin, std::size_t count,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        const auto pin = state_; numeric::ExactActionGuard guard(limits, std::move(check)); return finite_operator_detail::select(pin->variables, begin, count, guard);
    }
    std::vector<RelationalFiniteUseCorrespondence> use_correspondences(std::size_t begin, std::size_t count,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        const auto pin = state_; numeric::ExactActionGuard guard(limits, std::move(check)); return finite_operator_detail::select(pin->declaration.uses, begin, count, guard);
    }
private:
    std::shared_ptr<const State> state_;
};
class RelationalFiniteOperatorInstance;
class PreparedRelationalFiniteFieldOperator {
    struct State { PreparedRelationalFiniteFieldMap reference; RelationalFiniteOperatorDeclaration declaration; };
public:
    PreparedRelationalFiniteFieldOperator(PreparedRelationalFiniteFieldMap reference, RelationalFiniteOperatorDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace finite_operator_detail; const auto parent = reference; const auto d = declaration; Guard guard(limits, std::move(check));
        require(d.key && d.revision, "finite operator identity/revision"); guard.entries(d.ports.size());
        std::set<std::uint64_t> keys; std::set<std::pair<std::uint64_t, std::size_t>> covered;
        for (const auto& p : d.ports) {
            guard.step(); require(p.key && keys.insert(p.key).second && covered.insert({p.rule, p.input}).second, "finite operator repeats a port or input position");
            const auto& rules = parent.declaration().rules; const auto r = std::find_if(rules.begin(), rules.end(), [&](const auto& row) { return row.key == p.rule; });
            require(r != rules.end() && p.input < r->inputs.size(), "finite operator port has no actual reference input");
        }
        std::size_t count{}; for (const auto& r : parent.declaration().rules) count = add(count, r.inputs.size());
        require(count == covered.size(), "finite operator omits a complete rule input port"); guard.step(); state_ = std::make_shared<const State>(State{parent, d});
    }
    PreparedRelationalFiniteFieldMap reference() const { return state_->reference; }
    const RelationalFiniteOperatorDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFiniteOperatorDeclaration& declaration() const && = delete;
    bool same_owner(const PreparedRelationalFiniteFieldOperator& other) const noexcept { return state_ == other.state_; }
    std::vector<RelationalFiniteOperatorPort> select_ports(std::size_t begin, std::size_t count,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        const auto pin = state_; numeric::ExactActionGuard guard(limits, std::move(check)); return finite_operator_detail::select(pin->declaration.ports, begin, count, guard);
    }
    RelationalFiniteOperatorInstance instantiate(inference::OwnedRelationalFiniteSource source,
        RelationalFiniteOperatorInstanceDeclaration declaration, std::vector<RelationalFiniteOperatorInput> inputs,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
namespace finite_operator_detail {
inline inference::RelationalAccessionInput original_input(const PreparedRelationalFiniteFieldOperator& op, std::uint64_t key) {
    const auto& ports = op.declaration().ports; const auto p = std::find_if(ports.begin(), ports.end(), [&](const auto& row) { return row.key == key; });
    require(p != ports.end(), "finite operator has no declared port key"); const auto ref = op.reference(); const auto& rules = ref.declaration().rules;
    const auto r = std::find_if(rules.begin(), rules.end(), [&](const auto& row) { return row.key == p->rule; }); return r->inputs.at(p->input);
}
} // namespace finite_operator_detail
inline inference::RelationalFiniteSourceDeclaration relational_finite_operator_source_declaration(
    const PreparedRelationalFiniteFieldOperator& op, const inference::OwnedRelationalInputUseAsset& asset,
    const RelationalFiniteSourceTransportDeclaration& declaration, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    const auto pin = op; const auto input = asset; const auto d = declaration; numeric::ExactActionGuard guard(limits, std::move(check));
    return finite_operator_detail::source_schema(pin.reference().source(), input, d, guard);
}
inline inference::RelationalInputAccessionDeclaration relational_finite_operator_input_schema(
    const PreparedRelationalFiniteFieldOperator& op, const inference::OwnedRelationalInputUseAsset& asset,
    const RelationalFiniteSourceTransportDeclaration& declaration, std::uint64_t port,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    const auto pin = op; const auto input = asset; const auto d = declaration; numeric::ExactActionGuard guard(limits, std::move(check));
    const auto parent = pin.reference(); const auto source = parent.source(); finite_operator_detail::routing(source.asset(), input, d, guard);
    const auto original = finite_operator_detail::original_input(pin, port); const auto accession = original.accession();
    return finite_operator_detail::input_schema(accession.declaration(), d, guard);
}
class RelationalFiniteOperatorResult;
class RelationalFiniteOperatorInstance {
    struct State {
        PreparedRelationalFiniteFieldOperator op; OwnedRelationalFiniteSourceTransport transport;
        RelationalFiniteOperatorInstanceDeclaration declaration; std::vector<RelationalFiniteOperatorInput> inputs;
        PreparedRelationalFiniteFieldMap map;
    };
public:
    PreparedRelationalFiniteFieldOperator operator_program() const { return state_->op; }
    OwnedRelationalFiniteSourceTransport source_transport() const { return state_->transport; }
    inference::OwnedRelationalFiniteSource source() const { return state_->transport.source(); }
    const RelationalFiniteOperatorInstanceDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFiniteOperatorInstanceDeclaration& declaration() const && = delete;
    const std::vector<RelationalFiniteOperatorInput>& inputs() const & { return state_->inputs; }
    const std::vector<RelationalFiniteOperatorInput>& inputs() const && = delete;
    PreparedRelationalFiniteFieldMap map() const { return state_->map; }
    bool same_owner(const RelationalFiniteOperatorInstance& other) const noexcept { return state_ == other.state_; }
    RelationalFiniteOperatorResult execute(numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class PreparedRelationalFiniteFieldOperator;
    explicit RelationalFiniteOperatorInstance(State s) : state_(std::make_shared<const State>(std::move(s))) {}
    std::shared_ptr<const State> state_;
};
inline RelationalFiniteOperatorInstance PreparedRelationalFiniteFieldOperator::instantiate(inference::OwnedRelationalFiniteSource source,
    RelationalFiniteOperatorInstanceDeclaration declaration, std::vector<RelationalFiniteOperatorInput> inputs,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace finite_operator_detail; const auto op = *this; const auto input_source = source; const auto d = declaration; const auto rows = inputs; Guard guard(limits, check);
    require(d.key && d.revision, "finite operator instance identity/revision"); require(rows.size() == op.declaration().ports.size(), "finite instance requires every independent port"); guard.entries(rows.size());
    std::map<std::uint64_t, std::size_t> positions;
    for (std::size_t i = 0; i < rows.size(); ++i) { guard.step(); require(positions.emplace(rows[i].port, i).second, "finite instance repeats independent port"); }
    const auto parent = op.reference(); OwnedRelationalFiniteSourceTransport transport(parent.source(), input_source, d.transport, limits, check);
    const auto actual_asset = input_source.asset(); std::vector<RelationalFiniteOperatorInput> canonical;
    for (const auto& port : op.declaration().ports) {
        guard.step(); require(positions.contains(port.key), "finite instance omits named port"); const auto& row = rows[positions.at(port.key)];
        require(row.input.asset.same_owner(actual_asset), "finite operator port refers to foreign actual independent asset owner");
        const auto original = original_input(op, port.key); const auto accession = original.accession();
        const auto expected = input_schema(accession.declaration(), d.transport, guard);
        require(interface_bytes(row.input.declaration) == interface_bytes(expected) && row.input.use == use(d.transport, original.use()), "finite operator port changes transported interface or use"); canonical.push_back(row);
    }
    auto raw = parent.declaration(); raw.key = d.key; raw.revision = d.revision;
    for (auto& rule : raw.rules) {
        for (std::size_t i = 0; i < rule.inputs.size(); ++i) {
            guard.step(); const auto& ports = op.declaration().ports; const auto p = std::find_if(ports.begin(), ports.end(), [&](const auto& port) { return port.rule == rule.key && port.input == i; });
            const auto original = original_input(op, p->key); const auto original_accession = original.accession(); const auto target = original_accession.program();
            const auto at = static_cast<std::size_t>(p - ports.begin()); const auto& row = canonical[at];
            const inference::OwnedRelationalInputAccession accession(actual_asset, target, row.input.declaration, limits, check);
            rule.inputs[i] = accession.admit(row.input.use, row.input.source_field, limits, check);
        }
    }
    PreparedRelationalFiniteFieldMap map(input_source, parent.targets(), std::move(raw), limits, check); guard.step();
    return RelationalFiniteOperatorInstance({op, transport, d, std::move(canonical), std::move(map)});
}
class RelationalFiniteOperatorBranch;
class RelationalFiniteOperatorEvaluation;
class RelationalFiniteOperatorResult {
public:
    RelationalFiniteOperatorInstance instance() const { return instance_; }
    RelationalFiniteFieldResult result() const { return result_; }
    std::size_t size() const noexcept { return result_.size(); }
    std::vector<RelationalFiniteOperatorBranch> select(std::size_t begin, std::size_t count,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    RelationalFiniteOperatorEvaluation evaluate(std::size_t ordinal, numeric::RationalVector free,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class RelationalFiniteOperatorInstance;
    RelationalFiniteOperatorResult(RelationalFiniteOperatorInstance instance, RelationalFiniteFieldResult result) : instance_(std::move(instance)), result_(std::move(result)) {}
    RelationalFiniteOperatorInstance instance_; RelationalFiniteFieldResult result_;
};
class RelationalFiniteOperatorBranch {
public:
    RelationalFiniteOperatorResult parent() const { return parent_; }
    std::size_t ordinal() const noexcept { return branch_.ordinal; }
    const RelationalFiniteFieldBranch& branch() const & { return branch_; }
    const RelationalFiniteFieldBranch& branch() const && = delete;
private:
    friend class RelationalFiniteOperatorResult;
    RelationalFiniteOperatorBranch(RelationalFiniteOperatorResult parent, RelationalFiniteFieldBranch branch) : parent_(std::move(parent)), branch_(std::move(branch)) {}
    RelationalFiniteOperatorResult parent_; RelationalFiniteFieldBranch branch_;
};
class RelationalFiniteOperatorEvaluation {
public:
    RelationalFiniteOperatorResult family() const { return family_; }
    RelationalFiniteFieldEvaluation result() const { return result_; }
    std::size_t ordinal() const noexcept { return result_.ordinal(); }
private:
    friend class RelationalFiniteOperatorResult;
    RelationalFiniteOperatorEvaluation(RelationalFiniteOperatorResult family, RelationalFiniteFieldEvaluation result) : family_(std::move(family)), result_(std::move(result)) {}
    RelationalFiniteOperatorResult family_; RelationalFiniteFieldEvaluation result_;
};
inline RelationalFiniteOperatorResult RelationalFiniteOperatorInstance::execute(numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this; const auto result = pin.map().execute(limits, check); if (check) { check(); } return {pin, result};
}
inline std::vector<RelationalFiniteOperatorBranch> RelationalFiniteOperatorResult::select(std::size_t begin, std::size_t count,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this; const auto rows = pin.result_.select(begin, count, limits, std::move(check)); std::vector<RelationalFiniteOperatorBranch> out;
    for (const auto& row : rows) { out.push_back(RelationalFiniteOperatorBranch(pin, row)); } return out;
}
inline RelationalFiniteOperatorEvaluation RelationalFiniteOperatorResult::evaluate(std::size_t ordinal, numeric::RationalVector free,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this; const auto frozen = free; return {pin, pin.result_.evaluate(ordinal, frozen, limits, std::move(check))};
}
} // namespace rfc::learning
