#pragma once

#include "rfc/inference/relational_field_interface.hpp"
#include "rfc/learning/graded_transport_fit.hpp"

namespace rfc::learning {
struct RelationalFieldParameterDeclaration {
    std::uint64_t key{}, revision{}, node{};
    std::vector<TransportCoordinate> variables;
    numeric::ExactMatrix basis;
    std::vector<std::uint64_t> parameters;
};
struct RelationalFieldTrainingObservation {
    std::uint64_t key{};
    inference::OwnedAccessionFieldResult input;
    inference::OwnedRelationalInputUseAsset target_asset;
    std::uint64_t target_use{};
    numeric::SparseRationalTensor target;
    std::vector<core::CellId> observed;
};
struct RelationalParameterParentRelation {
    numeric::RationalVector shift;
    numeric::ExactMatrix map;
};
struct RelationalAffineField {
    numeric::SparseRationalTensor particular;
    numeric::ExactMatrix directions;
};
namespace relational_evidence_detail {
using namespace inference::field_interface_detail;
using V = numeric::RationalVector;
using Program = inference::OwnedRelationalFieldProgram;
using Input = inference::OwnedAccessionFieldResult;
inline void program_values(const Program& program, Guard& guard) {
    const auto p = program.publication(); source_values(p.endpoint().source, guard);
    std::size_t retained = program.declaration().nodes.size();
    for (const auto& s : program.declaration().stalks) {
        guard.matrix(s.metric); retained = add(retained, add(s.metric.nonzeros(), s.directions.size()));
    }
    for (const auto& o : program.declaration().occurrences) { guard.matrix(o.map); retained = add(retained, add(3, o.map.nonzeros())); }
    for (const auto& n : program.declaration().nodes) { guard.matrix(n.map); retained = add(retained, n.map.nonzeros()); }
    guard.entries(retained);
}
inline void same_limits(const numeric::ExactActionLimits& a, const numeric::ExactActionLimits& b) {
    require(a.dimension == b.dimension && a.entries == b.entries && a.steps == b.steps && a.integer_bits == b.integer_bits,
        "field evidence append must retain its declared action policy");
}
inline V sum(V a, const V& b, Guard& guard) {
    require(a.size() == b.size(), "affine field addition carrier"); guard.vector(a); guard.vector(b);
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = guard.add(a[i], b[i]);
    return a;
}
inline V column(const Matrix& m, std::size_t j, Guard& guard) {
    guard.matrix(m); require(j < m.cols(), "affine common free axis"); V out(m.rows());
    for (const auto& [ij, q] : m.entries()) { guard.step(); if (ij.second == j) out[ij.first] = q; }
    return out;
}
inline void column(Matrix& m, std::size_t j, const V& values, Guard& guard) {
    guard.vector(values); require(values.size() == m.rows() && j < m.cols(), "affine direction field carrier");
    for (std::size_t i = 0; i < values.size(); ++i) { guard.step(); if (values[i] != 0) m.set(i, j, values[i]); }
    guard.entries(m.nonzeros());
}
struct Fixed {
    Program program;
    RelationalFieldParameterDeclaration declaration;
    numeric::ExactActionLimits limits;
    core::Grade grade;
    PreparedTransportObservations tangent;
    std::map<std::uint16_t, PreparedTransportObservations> actions;

    static core::Grade selected_grade(const Program& program, const RelationalFieldParameterDeclaration& d) {
        const auto& selected = node(program, d.node);
        require(selected.kind == inference::FieldProgramNodeKind::Boundary && selected.input.has_value(),
            "affine parameter declaration requires an actual Boundary input");
        return node(program, *selected.input).output_grade;
    }

    static PreparedTransportObservations prepare(const Program& program, const RelationalFieldParameterDeclaration& d, Guard& guard) {
        program_values(program, guard); const auto& selected = node(program, d.node);
        require(d.key && d.revision && selected.kind == inference::FieldProgramNodeKind::Boundary && program.declaration().differential,
            "affine evidence needs an identified differential Boundary stage");
        const auto g = selected_grade(program, d);
        std::size_t count{};
        for (const auto& n : program.declaration().nodes)
            if (n.kind == inference::FieldProgramNodeKind::Boundary && node(program, *n.input).output_grade == g) ++count;
        require(count == 1, "affine profile rejects repeated uses of a learned boundary grade");
        require(d.basis.rows() == d.variables.size() && d.basis.cols() == d.parameters.size() && !d.parameters.empty(),
            "affine parameter basis must declare every original coordinate and parameter");
        guard.matrix(d.basis); guard.entries(machine::checked_size_mul(d.variables.size(), 3));
        std::set<std::uint64_t> names;
        for (const auto key : d.parameters) { guard.step(); require(key && names.insert(key).second, "affine parameter name identity"); }
        for (const auto& o : program.declaration().occurrences)
            if (o.grade == g) require(o.role == storage::TemporalTensorRole::CoefficientIndependentMap,
                "affine selected-grade Phi must be explicit even for inactive/zero flags");
        const auto& original = program.fibers().fibers(); local::LinearRelationalFiberSystem tangent(original.stalks());
        for (std::size_t k = 1; k < original.complex().grade_count(); ++k) {
            const auto current = inference::field_program_detail::grade(k);
            for (std::size_t p = 0; p < original.complex().grade(current).incidence_count(); ++p) {
                guard.step(); const auto& map = *original.transport_override(current, p);
                tangent.set_transport(current, p, current == g ? Matrix(map.rows(), map.cols()) : map);
            }
        }
        return PreparedTransportObservations::at_grade(local::OwnedLinearFiberSystem(tangent), g, {}, d.variables, guard, d.basis);
    }
    Fixed(Program p, RelationalFieldParameterDeclaration d, Guard& guard)
        : program(std::move(p)), declaration(std::move(d)), limits(guard.limits()),
          grade(selected_grade(program, declaration)), tangent(prepare(program, declaration, guard)) {
        for (const auto& n : program.declaration().nodes) if (n.kind == inference::FieldProgramNodeKind::Boundary) {
            const auto g = node(program, *n.input).output_grade;
            if (g == grade || actions.contains(g.value)) continue;
            const auto& original = program.fibers().fibers(); std::vector<TransportCoordinate> fixed;
            for (std::size_t pslot = 0; pslot < original.complex().grade(g).incidence_count(); ++pslot) {
                for (const auto& [ij, q] : original.transport_override(g, pslot)->entries()) {
                    (void)q; guard.step(); fixed.push_back({pslot, ij.first, ij.second});
                }
            }
            actions.emplace(g.value, PreparedTransportObservations::at_grade(program.fibers(), g, std::move(fixed), {}, guard));
        }
    }
    TransportObservation observation(const RelationalFieldTrainingObservation& o, Guard& guard) const {
        require(o.key && o.target_use && !o.observed.empty(), "whole field observation identity/use/mask");
        const auto target_asset = o.target_asset; (void)target_asset.use(o.target_use);
        const auto result = o.input.result(); const auto actual = result.program();
        require(program.fibers().same_owner(actual.fibers()), "training evidence targets a different actual source program owner");
        const auto& learned = node(program, declaration.node); const auto lower = learned.output_grade;
        const auto dimension = program.fibers().fibers().stalks().total_dimension(lower);
        admit_field(o.target, dimension, guard); const auto target = o.target.values().dense();
        const auto base = result.field(learned.key).values().dense();
        const auto input = result.field(*learned.input).values().dense(); guard.vector(input);
        const auto offsets = program.fibers().fibers().stalks().offsets(lower);
        std::vector<std::size_t> mask(offsets.size() - 1); std::set<std::uint32_t> observed;
        for (const auto cell : o.observed) {
            guard.step(); require(cell.grade == lower && cell.index < mask.size() && observed.insert(cell.index).second,
                "observation mask must name distinct whole original lower stalks"); mask[cell.index] = 1;
        }
        V residual(dimension);
        for (std::size_t cell = 0; cell < mask.size(); ++cell) if (mask[cell])
            for (std::size_t i = offsets[cell]; i < offsets[cell+1]; ++i) residual[i] = guard.sub(target[i], base[i]);
        return {input, std::move(residual), std::move(mask)};
    }
};
struct ResultNode {
    std::uint64_t node{};
    core::Grade grade;
    RelationalAffineField field;
    std::optional<RelationalAffineField> occurrences;
    bool learned_dependency{};
};
} // namespace relational_evidence_detail

class RelationalAffineProgramResult;
class RelationalAffineEvaluation;
class RelationalFieldResidual;
class RelationalAffineOccurrenceMap;
class PreparedRelationalFieldEvidence {
    struct State {
        std::shared_ptr<const relational_evidence_detail::Fixed> fixed;
        RelationalFieldParameterDeclaration declaration;
        PreparedGradedTransportFit fit;
        std::vector<RelationalFieldTrainingObservation> observations;
        std::vector<std::uint64_t> free_names;
        std::shared_ptr<const State> parent;
        std::optional<RelationalParameterParentRelation> relationship;
        State(std::shared_ptr<const relational_evidence_detail::Fixed> f, RelationalFieldParameterDeclaration d,
            PreparedGradedTransportFit value, std::vector<RelationalFieldTrainingObservation> o, std::shared_ptr<const State> previous,
            numeric::ExactActionGuard& guard)
            : fixed(std::move(f)), declaration(std::move(d)), fit(std::move(value)), observations(std::move(o)), parent(std::move(previous)) {
            using namespace relational_evidence_detail;
            for (const auto i : fit.free_columns()) { guard.step(); free_names.push_back(declaration.parameters.at(i)); }
            std::size_t retained = fit.retained_entries();
            for (const auto& sample : observations) retained = add(retained, add(add(sample.target.values().nonzeros(), sample.observed.size()), 4));
            guard.entries(retained);
            if (parent) {
                guard.entries(parent->fit.free_columns().size());
                RelationalParameterParentRelation r{V(parent->fit.free_columns().size()), Matrix(parent->fit.free_columns().size(), fit.kernel().cols())};
                std::map<std::size_t, std::size_t> parent_rows;
                for (std::size_t i = 0; i < parent->fit.free_columns().size(); ++i) {
                    guard.step(); const auto j = parent->fit.free_columns()[i]; r.shift[i] = guard.sub(fit.particular()[j], parent->fit.particular()[j]);
                    parent_rows.emplace(j, i);
                }
                // This is the complete child kernel restricted to the actual
                // parent's free parameter rows, including every zero column.
                // Gather represented entries instead of probing the rectangle.
                for (const auto& [ij, q] : fit.kernel().entries()) {
                    guard.step(); const auto row = parent_rows.find(ij.first);
                    if (row != parent_rows.end()) {
                        r.map.set(row->second, ij.second, q);
                        guard.entries(add(parent_rows.size(), r.map.nonzeros()));
                    }
                }
                require(sum(parent->fit.particular(), numeric::checked_apply(parent->fit.kernel(), r.shift, guard), guard) == fit.particular() &&
                    numeric::checked_multiply(parent->fit.kernel(), r.map, guard) == fit.kernel(), "child affine evidence does not descend from actual parent family");
                relationship = std::move(r);
            }
        }
    };
public:
    PreparedRelationalFieldEvidence(inference::OwnedRelationalFieldProgram program, RelationalFieldParameterDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        const auto frozen = declaration; const auto pin = program; numeric::ExactActionGuard guard(limits, std::move(check));
        auto fixed = std::make_shared<const relational_evidence_detail::Fixed>(pin, frozen, guard);
        PreparedGradedTransportFit fit(fixed->tangent, {}, guard);
        state_ = std::make_shared<const State>(fixed, frozen, std::move(fit), std::vector<RelationalFieldTrainingObservation>{}, nullptr, guard);
    }
    inference::OwnedRelationalFieldProgram program() const { return state_->fixed->program; }
    bool same_owner(const PreparedRelationalFieldEvidence& other) const noexcept { return state_ == other.state_; }
    const RelationalFieldParameterDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFieldParameterDeclaration& declaration() const && = delete;
    const std::vector<RelationalFieldTrainingObservation>& observations() const & { return state_->observations; }
    const std::vector<RelationalFieldTrainingObservation>& observations() const && = delete;
    const numeric::RationalVector& particular() const & { return state_->fit.particular(); }
    const numeric::RationalVector& particular() const && = delete;
    const numeric::ExactMatrix& kernel() const & { return state_->fit.kernel(); }
    const numeric::ExactMatrix& kernel() const && = delete;
    const std::vector<std::uint64_t>& parameters() const & { return state_->declaration.parameters; }
    const std::vector<std::uint64_t>& parameters() const && = delete;
    const std::vector<std::uint64_t>& free_parameters() const & { return state_->free_names; }
    const std::vector<std::uint64_t>& free_parameters() const && = delete;
    const numeric::ExactMatrix& chain_constraints() const & { return state_->fit.chain(); }
    const numeric::ExactMatrix& chain_constraints() const && = delete;
    const std::vector<TransportChainConstraint>& chain_coordinates() const & { return state_->fit.chain_coordinates(); }
    const std::vector<TransportChainConstraint>& chain_coordinates() const && = delete;
    const TransportObservationSystem& observation_system() const & { return state_->fit.evidence(); }
    const TransportObservationSystem& observation_system() const && = delete;
    const std::optional<RelationalParameterParentRelation>& parent_relation() const & { return state_->relationship; }
    const std::optional<RelationalParameterParentRelation>& parent_relation() const && = delete;
    numeric::ExactActionLimits limits() const { return state_->fixed->limits; }
    std::optional<PreparedRelationalFieldEvidence> parent() const {
        if (state_->parent) return PreparedRelationalFieldEvidence(state_->parent);
        return {};
    }
    PreparedRelationalFieldEvidence append(std::vector<RelationalFieldTrainingObservation> suffix,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        using namespace relational_evidence_detail;
        const auto pin = state_; const auto frozen = suffix; Guard guard(limits, std::move(check));
        same_limits(pin->fixed->limits, limits); require(!frozen.empty(), "nonempty source-backed evidence suffix required");
        require(pin->declaration.revision != std::numeric_limits<std::uint64_t>::max(), "field evidence revision overflow");
        std::set<std::uint64_t> keys; for (const auto& o : pin->observations) keys.insert(o.key);
        guard.dimension(add(pin->observations.size(), frozen.size())); std::vector<TransportObservation> observations;
        for (const auto& o : frozen) {
            guard.step(); require(keys.insert(o.key).second, "source-backed evidence key already exists");
            observations.push_back(pin->fixed->observation(o, guard));
        }
        auto fit = pin->fit.append(observations, guard); auto all = pin->observations;
        all.insert(all.end(), frozen.begin(), frozen.end()); auto d = pin->declaration; ++d.revision;
        return PreparedRelationalFieldEvidence(std::make_shared<const State>(pin->fixed, std::move(d), std::move(fit), std::move(all), pin, guard));
    }
    RelationalAffineProgramResult execute(inference::OwnedAccessionFieldResult independent_input,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    RelationalFieldResidual residual(std::size_t observation, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    RelationalAffineOccurrenceMap coefficient_independent_map(core::Grade grade, std::size_t occurrence,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class RelationalAffineProgramResult;
    explicit PreparedRelationalFieldEvidence(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

class RelationalAffineProgramResult {
    struct State {
        PreparedRelationalFieldEvidence evidence;
        inference::OwnedAccessionFieldResult input;
        std::vector<relational_evidence_detail::ResultNode> nodes;
    };
public:
    PreparedRelationalFieldEvidence evidence() const { return state_->evidence; }
    inference::OwnedAccessionFieldResult input() const { return state_->input; }
    const RelationalAffineField& field(std::uint64_t node) const & { return find(node).field; }
    const RelationalAffineField& field(std::uint64_t) const && = delete;
    const RelationalAffineField& occurrence_field(std::uint64_t node) const & {
        const auto& n = find(node); inference::field_program_detail::require(n.occurrences.has_value(), "affine node has no original flag field"); return *n.occurrences;
    }
    const RelationalAffineField& occurrence_field(std::uint64_t) const && = delete;
    bool identified(std::uint64_t node, std::size_t coordinate) const {
        const auto pin = state_; const auto& directions = find(*pin, node).field.directions;
        inference::field_program_detail::require(coordinate < directions.rows(), "affine identification coordinate");
        for (const auto& [ij, q] : directions.entries()) { (void)q; if (ij.first == coordinate) return false; }
        return true;
    }
    RelationalAffineEvaluation evaluate(numeric::RationalVector free,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class PreparedRelationalFieldEvidence;
    const relational_evidence_detail::ResultNode& find(std::uint64_t key) const { return find(*state_, key); }
    static const relational_evidence_detail::ResultNode& find(const State& state, std::uint64_t key) {
        const auto it = std::find_if(state.nodes.begin(), state.nodes.end(), [&](const auto& n) { return n.node == key; });
        inference::field_program_detail::require(it != state.nodes.end(), "unknown complete affine program node"); return *it;
    }
    explicit RelationalAffineProgramResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline RelationalAffineProgramResult PreparedRelationalFieldEvidence::execute(inference::OwnedAccessionFieldResult independent_input,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_evidence_detail;
    const auto pin = state_; const auto input_pin = independent_input; Guard guard(limits, std::move(check));
    const auto base = input_pin.result(); const auto actual = base.program(); const auto& program = pin->fixed->program;
    require(actual.fibers().same_owner(program.fibers()), "held affine input targets a different actual program owner");
    program_values(program, guard); guard.vector(pin->fit.particular()); guard.matrix(pin->fit.kernel());
    std::vector<ResultNode> output; std::map<std::uint64_t, std::size_t> positions;
    const auto free = pin->fit.kernel().cols(); std::size_t retained{};
    for (const auto& n : program.declaration().nodes) {
        guard.step(); const auto size = base.field(n.key).domain().logical_size(); admit_field(base.field(n.key), size, guard);
        ResultNode value{n.key, n.output_grade, {base.field(n.key), Matrix(size, free)}, {}, false};
        if (n.kind == inference::FieldProgramNodeKind::Boundary) {
            const auto& flags = base.occurrence_field(n.key);
            value.occurrences = RelationalAffineField{flags, Matrix(flags.domain().logical_size(), free)};
        }
        if (n.key == pin->declaration.node) {
            const auto& previous = output.at(positions.at(*n.input));
            require(!previous.learned_dependency && previous.field.directions.nonzeros() == 0, "learned stage input cannot contain unknown parameter products");
            const auto x = previous.field.particular.values().dense(); const auto& model = pin->fit.model();
            for (const bool shared : {true, false}) {
                const auto h = model.observation_map(x, shared, guard);
                auto& result = shared ? value.field : *value.occurrences;
                auto p = sum(result.particular.values().dense(), numeric::checked_apply(h, pin->fit.particular(), guard), guard);
                result.particular = tensor(p, guard); result.directions = numeric::checked_multiply(h, pin->fit.kernel(), guard);
            }
            value.learned_dependency = true;
        } else if (n.input && output.at(positions.at(*n.input)).learned_dependency) {
            const auto& previous = output.at(positions.at(*n.input)).field; value.learned_dependency = true;
            if (n.kind == inference::FieldProgramNodeKind::DeclaredMap) {
                value.field.particular = act(n.map, previous.particular, guard);
                value.field.directions = numeric::checked_multiply(n.map, previous.directions, guard);
            } else {
                require(n.kind == inference::FieldProgramNodeKind::Boundary, "unsupported fixed affine stage");
                const auto g = node(program, *n.input).output_grade;
                const auto& model = pin->fixed->actions.at(g.value);
                const auto p = model.fixed_response(previous.particular.values().dense(), guard);
                value.field.particular = tensor(p.boundary, guard); value.occurrences->particular = tensor(p.occurrences, guard);
                for (std::size_t j = 0; j < free; ++j) {
                    const auto direction = model.fixed_response(column(previous.directions, j, guard), guard);
                    column(value.field.directions, j, direction.boundary, guard); column(value.occurrences->directions, j, direction.occurrences, guard);
                }
            }
        }
        retained = add(retained, add(value.field.particular.values().nonzeros(), value.field.directions.nonzeros()));
        if (value.occurrences) retained = add(retained, add(value.occurrences->particular.values().nonzeros(), value.occurrences->directions.nonzeros()));
        guard.entries(retained); positions.emplace(n.key, output.size()); output.push_back(std::move(value));
    }
    guard.step(); return RelationalAffineProgramResult({PreparedRelationalFieldEvidence(pin), input_pin, std::move(output)});
}
class RelationalAffineEvaluation {
public:
    RelationalAffineProgramResult family() const { return family_; }
    inference::OwnedAccessionFieldResult result() const { return result_; }
    const numeric::RationalVector& free_values() const & { return free_; }
    const numeric::RationalVector& free_values() const && = delete;
private:
    friend class RelationalAffineProgramResult;
    RelationalAffineEvaluation(RelationalAffineProgramResult family, inference::OwnedAccessionFieldResult result, numeric::RationalVector free)
        : family_(std::move(family)), result_(std::move(result)), free_(std::move(free)) {}
    RelationalAffineProgramResult family_;
    inference::OwnedAccessionFieldResult result_;
    numeric::RationalVector free_;
};
inline RelationalAffineEvaluation RelationalAffineProgramResult::evaluate(numeric::RationalVector free,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_evidence_detail;
    const auto pin = *this; const auto frozen = free; Guard guard(limits, std::move(check));
    const auto evidence = pin.evidence(); const auto base = evidence.program(); const auto& d = evidence.declaration();
    require(frozen.size() == evidence.kernel().cols(), "every actual common free parameter must be supplied");
    const auto theta = sum(evidence.particular(), numeric::checked_apply(evidence.kernel(), frozen, guard), guard);
    const auto entries = numeric::checked_apply(d.basis, theta, guard); auto program = base.declaration();
    require(program.revision <= std::numeric_limits<std::uint64_t>::max() - d.revision, "prospective affine program revision overflow");
    program.revision += d.revision;
    const auto grade = node(base, *node(base, d.node).input).output_grade;
    for (std::size_t i = 0; i < d.variables.size(); ++i) {
        guard.step(); const auto& variable = d.variables[i];
        const auto map = std::find_if(program.occurrences.begin(), program.occurrences.end(), [&](const auto& o) { return o.grade == grade && o.occurrence == variable.occurrence; });
        require(map != program.occurrences.end(), "affine variable lost its original flag");
        map->map.set(variable.row, variable.column, guard.add(map->map.get(variable.row, variable.column), entries[i]));
    }
    const Program child(base.publication(), program, limits, [&] { guard.step(); });
    const auto input = pin.input(); std::vector<inference::RelationalAccessionInput> inputs;
    for (const auto& original : input.inputs()) {
        const auto a = original.accession();
        const inference::OwnedRelationalInputAccession rebuilt(a.asset(), child, a.declaration(), limits, [&] { guard.step(); });
        inputs.push_back(rebuilt.admit(original.use(), original.source_field(), limits, [&] { guard.step(); }));
    }
    auto result = inference::execute_relational_inputs(child, std::move(inputs), limits, [&] { guard.step(); }); const auto actual = result.result();
    for (const auto& n : program.nodes) {
        guard.step(); const auto verify = [&](const RelationalAffineField& f, const Tensor& field) {
            const auto expected = sum(f.particular.values().dense(), numeric::checked_apply(f.directions, frozen, guard), guard);
            require(equal(tensor(expected, guard), field), "prospective materialization differs from complete correlated field family");
        };
        verify(pin.field(n.key), actual.field(n.key));
        if (n.kind == inference::FieldProgramNodeKind::Boundary) verify(pin.occurrence_field(n.key), actual.occurrence_field(n.key));
    }
    guard.step(); return {pin, std::move(result), frozen};
}
class RelationalFieldResidual {
public:
    RelationalAffineProgramResult response() const { return response_; }
    PreparedRelationalFieldEvidence evidence() const { return response_.evidence(); }
    std::size_t observation() const noexcept { return observation_; }
    const RelationalAffineField& field() const & { return field_; }
    const RelationalAffineField& field() const && = delete;
private:
    friend class PreparedRelationalFieldEvidence;
    RelationalFieldResidual(RelationalAffineProgramResult response, std::size_t observation, RelationalAffineField field)
        : response_(std::move(response)), observation_(observation), field_(std::move(field)) {}
    RelationalAffineProgramResult response_;
    std::size_t observation_{};
    RelationalAffineField field_;
};
inline RelationalFieldResidual PreparedRelationalFieldEvidence::residual(std::size_t observation,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_evidence_detail;
    const auto pin = *this; Guard guard(limits, std::move(check));
    require(observation < pin.observations().size(), "original residual observation index");
    const auto& o = pin.observations()[observation];
    auto response = pin.execute(o.input, limits, [&] { guard.step(); });
    const auto& complete = response.field(pin.declaration().node);
    auto values = complete.particular.values().dense(); const auto measured = o.target.values().dense();
    guard.vector(values); guard.vector(measured);
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = guard.sub(values[i], measured[i]);
    const auto program = pin.program(); const auto lower = node(program, pin.declaration().node).output_grade;
    const auto offsets = program.fibers().fibers().stalks().offsets(lower);
    for (const auto cell : o.observed) for (auto i = offsets[cell.index]; i < offsets[cell.index+1]; ++i) {
        guard.step(); require(values[i] == 0, "native feasible evidence has a nonzero observed residual");
        for (const auto& [ij, q] : complete.directions.entries()) { (void)q; guard.step(); require(ij.first != i, "native feasible family leaves its observed whole stalks"); }
    }
    return {std::move(response), observation, {tensor(values, guard), complete.directions}};
}
class RelationalAffineOccurrenceMap {
public:
    PreparedRelationalFieldEvidence evidence() const { return evidence_; }
    core::Grade grade() const noexcept { return grade_; }
    std::size_t occurrence() const noexcept { return occurrence_; }
    const numeric::ExactMatrix& particular() const & { return particular_; }
    const numeric::ExactMatrix& particular() const && = delete;
    const std::vector<numeric::ExactMatrix>& directions() const & { return directions_; }
    const std::vector<numeric::ExactMatrix>& directions() const && = delete;
private:
    friend class PreparedRelationalFieldEvidence;
    RelationalAffineOccurrenceMap(PreparedRelationalFieldEvidence evidence, core::Grade grade, std::size_t occurrence,
        numeric::ExactMatrix particular, std::vector<numeric::ExactMatrix> directions)
        : evidence_(std::move(evidence)), grade_(grade), occurrence_(occurrence), particular_(std::move(particular)), directions_(std::move(directions)) {}
    PreparedRelationalFieldEvidence evidence_;
    core::Grade grade_;
    std::size_t occurrence_{};
    numeric::ExactMatrix particular_;
    std::vector<numeric::ExactMatrix> directions_;
};
inline RelationalAffineOccurrenceMap PreparedRelationalFieldEvidence::coefficient_independent_map(core::Grade grade, std::size_t occurrence,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_evidence_detail;
    const auto pin = *this; Guard guard(limits, std::move(check)); const auto program = pin.program();
    const auto& maps = program.declaration().occurrences;
    const auto original = std::find_if(maps.begin(), maps.end(), [&](const auto& o) { return o.grade == grade && o.occurrence == occurrence; });
    require(original != maps.end() && original->role == storage::TemporalTensorRole::CoefficientIndependentMap,
        "an actual original Phi declaration is required for map-family inspection");
    auto particular = original->map; guard.matrix(particular); guard.dimension(pin.kernel().cols());
    std::vector<Matrix> directions(pin.kernel().cols(), Matrix(particular.rows(), particular.cols()));
    const auto& d = pin.declaration(); const auto g = node(program, *node(program, d.node).input).output_grade;
    std::size_t retained = add(particular.nonzeros(), directions.size()); guard.entries(retained);
    if (grade == g) {
        const auto p = numeric::checked_apply(d.basis, pin.particular(), guard);
        const auto k = numeric::checked_multiply(d.basis, pin.kernel(), guard);
        for (std::size_t i = 0; i < d.variables.size(); ++i) if (d.variables[i].occurrence == occurrence) {
            const auto& c = d.variables[i]; guard.step();
            particular.set(c.row, c.column, guard.add(particular.get(c.row, c.column), p[i]));
        }
        for (const auto& [ij, q] : k.entries()) {
            guard.step(); const auto& c = d.variables[ij.first];
            if (c.occurrence != occurrence) continue;
            directions[ij.second].set(c.row, c.column, q); retained = add(retained, 1); guard.entries(add(retained, particular.nonzeros()));
        }
    }
    guard.step(); return {pin, grade, occurrence, std::move(particular), std::move(directions)};
}
} // namespace rfc::learning
