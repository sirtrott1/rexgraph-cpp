#pragma once

#include "rfc/inference/relational_field_interface.hpp"

namespace rfc::inference {
struct RelationalStalkExtensionMap {
    core::CellId cell;
    std::uint64_t source_basis{}, target_basis{};
    numeric::ExactMatrix inclusion, reconstruction;
};
struct RelationalFieldExtensionDeclaration {
    std::uint64_t key{}, revision{}, free_carrier{}, free_basis{};
    std::vector<std::uint64_t> free_directions;
    algebra::RetainedSourceMap source_map;
    std::vector<RelationalStalkExtensionMap> stalks;
    bool isometric{};
};
struct RelationalExtensionAffineField {
    numeric::SparseRationalTensor particular;
    numeric::ExactMatrix directions;
};
namespace field_extension_detail {
using namespace field_interface_detail;
inline void preserve_source(const core::RelationalComplex& source, const core::RelationalComplex& target,
    const algebra::RetainedSourceMap& map, Guard& guard) {
    algebra::owned_gluing_detail::map(source, target, map, true, guard);
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        const auto g = field_program_detail::grade(k); const auto& a = source.grade(g); const auto& b = target.grade(g);
        for (std::size_t i = 0; i < a.cell_count(); ++i) {
            guard.step(); const auto j = map.cells[k][i]; require(a.cell_scales[i] == b.cell_scales[j], "extension changes original cell scale");
            require(k ? a.relation_identities[i] == b.relation_identities[j] : a.identities[i] == b.identities[j], "extension changes original primary identity");
            if (!k) continue;
            const auto [begin, end] = a.incidence_range(i); const auto target_begin = b.incidence_range(j).first;
            for (auto p = begin; p < end; ++p) {
                guard.step(); const auto q = map.occurrences[k][p];
                require(q == target_begin + p - begin, "extension changes original ordered flag fiber");
                require(a.occurrence_identities[p] == b.occurrence_identities[q] && a.lattice_coefficient[p] == b.lattice_coefficient[q],
                    "extension changes original flag identity or lattice magnitude");
            }
        }
    }
}
inline Matrix boundary(const OwnedRelationalFieldProgram& p, core::Grade k, Guard& guard) {
    if (!k.value) return Matrix(0, p.fibers().fibers().stalks().total_dimension(k));
    guard.step(); const local::PreparedGradeFieldAction action(p.fibers().fibers(), k);
    Matrix out(action.statistics().target_dimension, action.statistics().source_dimension);
    for (const auto& entry : action.entries()) {
        guard.step(); out.set(entry.target, entry.source, guard.add(out.get(entry.target, entry.source), entry.coefficient)); guard.entries(out.nonzeros());
    }
    return out;
}
inline Tensor affine_value(const RelationalExtensionAffineField& field, const numeric::RationalVector& free, Guard& guard) {
    admit_field(field.particular, field.directions.rows(), guard);
    const auto shift = numeric::checked_apply(field.directions, free, guard); auto out = field.particular.values().dense(); guard.vector(out);
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = guard.add(out[i], shift[i]);
    return tensor(out, guard);
}
struct Field {
    std::uint64_t node{}; core::Grade grade;
    RelationalExtensionAffineField value;
    std::optional<RelationalExtensionAffineField> occurrences;
};
} // namespace field_extension_detail
class RelationalExtensionFieldFamily;
class RelationalExtensionEvaluation;
class OwnedRelationalFieldExtension {
    struct State {
        OwnedRelationalFieldProgram source, target;
        RelationalFieldExtensionDeclaration declaration;
        std::shared_ptr<const State> parent;
        std::vector<numeric::ExactMatrix> inclusion, reconstruction, kernel;
        std::size_t retained{};
        State(OwnedRelationalFieldProgram a, OwnedRelationalFieldProgram b, RelationalFieldExtensionDeclaration d,
            std::shared_ptr<const State> previous, numeric::ExactActionGuard& guard)
            : source(std::move(a)), target(std::move(b)), declaration(std::move(d)), parent(std::move(previous)) {
            using namespace field_extension_detail;
            const auto ap = source.publication(), bp = target.publication(); const auto& old = ap.endpoint().source; const auto& next = bp.endpoint().source;
            require(declaration.key && declaration.revision && declaration.free_carrier && declaration.free_basis,
                "extension requires versioned identity and named completion carrier");
            require(source.declaration().differential && target.declaration().differential && !source.fibers().same_owner(target.fibers()),
                "extension requires distinct actual differential program owners");
            require(target.declaration().key == source.declaration().key && target.declaration().revision > source.declaration().revision,
                "extension requires a newer explicit target program revision");
            if (parent) require(source.fibers().same_owner(parent->target.fibers()) && declaration.key == parent->declaration.key &&
                declaration.revision == machine::checked_size_add(parent->declaration.revision, 1), "extension actual parent lineage/version mismatch");
            preserve_source(old, next, declaration.source_map, guard);
            require(declaration.stalks.size() == source.declaration().stalks.size(), "extension omits original stalk maps");
            for (std::size_t k = 0; k < next.grade_count(); ++k) {
                const auto g = field_program_detail::grade(k); const auto n = target.fibers().fibers().stalks().total_dimension(g);
                const auto m = k < old.grade_count() ? source.fibers().fibers().stalks().total_dimension(g) : 0;
                guard.dimension(n); guard.dimension(m); inclusion.emplace_back(n, m); reconstruction.emplace_back(m, n);
            }
            for (std::size_t i = 0; i < declaration.stalks.size(); ++i) {
                guard.step(); const auto& s = source.declaration().stalks[i]; const auto& map = declaration.stalks[i];
                require(map.cell == s.cell && map.source_basis == s.basis, "extension original stalk/basis order mismatch");
                const core::CellId image{s.cell.grade, declaration.source_map.cells[s.cell.grade.value][s.cell.index]};
                const auto& t = field_interface_detail::stalk(target, image);
                require(map.target_basis == t.basis && (s.basis != t.basis || s.directions == t.directions), "extension target basis identity mismatch");
                require(map.inclusion.cols() == s.directions.size() && map.inclusion.rows() == t.directions.size(), "extension stalk inclusion dimensions");
                faithful(map.inclusion, map.reconstruction, guard);
                const auto a_offsets = source.fibers().fibers().stalks().offsets(s.cell.grade);
                const auto b_offsets = target.fibers().fibers().stalks().offsets(s.cell.grade);
                for (const auto& [ij, q] : map.inclusion.entries()) { guard.step(); inclusion[s.cell.grade.value].set(add(b_offsets[image.index], ij.first), add(a_offsets[s.cell.index], ij.second), q); }
                for (const auto& [ij, q] : map.reconstruction.entries()) { guard.step(); reconstruction[s.cell.grade.value].set(add(a_offsets[s.cell.index], ij.first), add(b_offsets[image.index], ij.second), q); }
            }
            for (std::size_t k = 0; k < next.grade_count(); ++k) {
                const auto g = field_program_detail::grade(k); faithful(inclusion[k], reconstruction[k], guard);
                if (declaration.isometric) {
                    const auto old_metric = k < old.grade_count() ? grade_metric(source, g, guard) : Matrix(0, 0);
                    require(numeric::checked_multiply(inclusion[k].transpose(), numeric::checked_multiply(grade_metric(target, g, guard), inclusion[k], guard), guard) == old_metric,
                        "extension false metric pullback claim");
                }
                const numeric::PreparedExactFrames reduction(reconstruction[k], guard); kernel.push_back(reduction.kernel());
                require(kernel.back().cols() == inclusion[k].rows() - inclusion[k].cols(), "extension incomplete completion kernel");
                if (k) {
                    const auto before = k < old.grade_count() ? boundary(source, g, guard) : Matrix(inclusion[k - 1].cols(), 0);
                    require(numeric::checked_multiply(boundary(target, g, guard), inclusion[k], guard) == numeric::checked_multiply(inclusion[k - 1], before, guard),
                        "extension violates a complete grade boundary law");
                }
            }
            for (const auto& flag : source.declaration().occurrences) {
                guard.step(); const auto& arena = old.grade(flag.grade); const auto slot = declaration.source_map.occurrences[flag.grade.value][flag.occurrence];
                const auto found = std::find_if(target.declaration().occurrences.begin(), target.declaration().occurrences.end(),
                    [&](const auto& f) { return f.grade == flag.grade && f.occurrence == slot; });
                require(found != target.declaration().occurrences.end() && found->role == flag.role, "extension changes original declared flag map role");
                const auto upper = core::CellId{flag.grade, arena.upper_index[flag.occurrence]};
                const auto lower = core::CellId{field_program_detail::grade(flag.grade.value - 1u), arena.lower_index[flag.occurrence]};
                const auto find_map = [&](core::CellId cell) -> const RelationalStalkExtensionMap& {
                    const auto it = std::find_if(declaration.stalks.begin(), declaration.stalks.end(), [&](const auto& m) { return m.cell == cell; });
                    require(it != declaration.stalks.end(), "extension absent original map stalk"); return *it;
                };
                require(numeric::checked_multiply(found->map, find_map(upper).inclusion, guard) ==
                    numeric::checked_multiply(find_map(lower).inclusion, flag.map, guard), "extension loses original intrinsic flag map");
            }
            require(target.declaration().nodes.size() >= source.declaration().nodes.size(), "extension loses original program nodes");
            std::size_t free{};
            for (std::size_t i = 0; i < target.declaration().nodes.size(); ++i) {
                guard.step(); const auto& n = target.declaration().nodes[i];
                if (i >= source.declaration().nodes.size()) { require(n.kind != FieldProgramNodeKind::Input, "extension new input nodes require a separate input contract"); continue; }
                const auto& previous_node = source.declaration().nodes[i];
                require(n.key == previous_node.key && n.kind == previous_node.kind && n.output_grade == previous_node.output_grade && n.input == previous_node.input,
                    "extension changes original node identity or operation order");
                if (n.kind == FieldProgramNodeKind::Input) free = add(free, kernel[n.output_grade.value].cols());
                else {
                    const auto input_grade = field_interface_detail::node(source, *n.input).output_grade;
                    const auto old_action = source.node_action(n.key), new_action = target.node_action(n.key);
                    require(numeric::checked_multiply(new_action->matrix(), inclusion[input_grade.value], guard) ==
                        numeric::checked_multiply(inclusion[n.output_grade.value], old_action->matrix(), guard), "extension changes original program action on admitted image");
                }
            }
            require(declaration.free_directions.size() == free, "extension completion carrier dimension");
            std::set<std::uint64_t> names;
            for (const auto key : declaration.free_directions) { guard.step(); require(key && names.insert(key).second, "extension duplicate or zero completion direction"); }
            for (std::size_t k = 0; k < inclusion.size(); ++k)
                retained = add(retained, add(add(inclusion[k].nonzeros(), reconstruction[k].nonzeros()), kernel[k].nonzeros()));
            retained = add(retained, add(algebra::owned_gluing_detail::words(declaration.source_map), free)); guard.entries(retained);
        }
    };
public:
    OwnedRelationalFieldExtension(OwnedRelationalFieldProgram source, OwnedRelationalFieldProgram target,
        RelationalFieldExtensionDeclaration declaration, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        const auto frozen = declaration; numeric::ExactActionGuard guard(limits, std::move(check));
        state_ = std::make_shared<const State>(std::move(source), std::move(target), frozen, nullptr, guard);
    }
    OwnedRelationalFieldProgram source() const { return state_->source; }
    OwnedRelationalFieldProgram target() const { return state_->target; }
    std::optional<OwnedRelationalFieldExtension> parent() const { return state_->parent ? std::optional<OwnedRelationalFieldExtension>{OwnedRelationalFieldExtension(state_->parent)} : std::nullopt; }
    const RelationalFieldExtensionDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFieldExtensionDeclaration& declaration() const && = delete;
    const numeric::ExactMatrix& inclusion(core::Grade k) const & { return state_->inclusion.at(k.value); }
    const numeric::ExactMatrix& reconstruction(core::Grade k) const & { return state_->reconstruction.at(k.value); }
    const numeric::ExactMatrix& kernel(core::Grade k) const & { return state_->kernel.at(k.value); }
    const numeric::ExactMatrix& inclusion(core::Grade) const && = delete;
    const numeric::ExactMatrix& reconstruction(core::Grade) const && = delete;
    const numeric::ExactMatrix& kernel(core::Grade) const && = delete;
    OwnedRelationalFieldExtension extend(OwnedRelationalFieldProgram target, RelationalFieldExtensionDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        const auto pin = state_; const auto frozen = declaration; numeric::ExactActionGuard guard(limits, std::move(check));
        return OwnedRelationalFieldExtension(std::make_shared<const State>(pin->target, std::move(target), frozen, pin, guard));
    }
    RelationalInputAccessionDeclaration transport_accession(const OwnedRelationalInputAccession& original, std::uint64_t key,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        using namespace field_extension_detail;
        const auto pin = state_; const auto actual = original; const auto source_program = actual.program(); Guard guard(limits, std::move(check)); guard.step();
        require(source_program.fibers().same_owner(pin->source.fibers()) && key, "extension accession requires actual source program and new identity");
        auto d = actual.declaration(); const auto g = field_interface_detail::node(pin->source, d.node).output_grade; d.key = key;
        for (std::size_t k = 0; k < d.instances.cells.size(); ++k) {
            for (auto& i : d.instances.cells[k]) { guard.step(); i = pin->declaration.source_map.cells[k].at(i); }
            for (auto& i : d.instances.occurrences[k]) { guard.step(); i = pin->declaration.source_map.occurrences[k].at(i); }
        }
        d.accession = numeric::checked_multiply(pin->inclusion[g.value], d.accession, guard);
        d.reconstruction = numeric::checked_multiply(d.reconstruction, pin->reconstruction[g.value], guard);
        d.isometric = d.isometric && pin->declaration.isometric; return d;
    }
    RelationalExtensionFieldFamily execute(OwnedAccessionFieldResult source_input,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    explicit OwnedRelationalFieldExtension(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
class RelationalExtensionFieldFamily {
    struct State {
        OwnedRelationalFieldExtension extension;
        OwnedAccessionFieldResult input;
        std::vector<field_extension_detail::Field> fields;
    };
public:
    OwnedRelationalFieldExtension extension() const { return state_->extension; }
    OwnedAccessionFieldResult input() const { return state_->input; }
    const std::vector<field_extension_detail::Field>& fields() const & { return state_->fields; }
    const std::vector<field_extension_detail::Field>& fields() const && = delete;
    const RelationalExtensionAffineField& field(std::uint64_t key) const & {
        const auto it = std::find_if(state_->fields.begin(), state_->fields.end(), [&](const auto& f) { return f.node == key; });
        if (it == state_->fields.end()) throw std::out_of_range("extension result node");
        return it->value;
    }
    const RelationalExtensionAffineField& field(std::uint64_t) const && = delete;
    RelationalExtensionEvaluation evaluate(numeric::RationalVector free, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class OwnedRelationalFieldExtension;
    explicit RelationalExtensionFieldFamily(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
class RelationalExtensionEvaluation {
public:
    RelationalExtensionFieldFamily family() const { return family_; }
    OwnedRelationalFieldResult result() const { return result_; }
    const numeric::RationalVector& free_values() const & { return free_; }
    const numeric::RationalVector& free_values() const && = delete;
private:
    friend class RelationalExtensionFieldFamily;
    RelationalExtensionEvaluation(RelationalExtensionFieldFamily family, OwnedRelationalFieldResult result, numeric::RationalVector free)
        : family_(std::move(family)), result_(std::move(result)), free_(std::move(free)) {}
    RelationalExtensionFieldFamily family_; OwnedRelationalFieldResult result_; numeric::RationalVector free_;
};
inline RelationalExtensionFieldFamily OwnedRelationalFieldExtension::execute(OwnedAccessionFieldResult source_input,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace field_extension_detail;
    const auto parent = *this; const auto pin = parent.state_; const auto original = std::move(source_input);
    Guard guard(limits, std::move(check)); guard.step(); const auto old_result = original.result(); const auto actual = old_result.program();
    require(actual.fibers().same_owner(pin->source.fibers()), "extension field family requires actual source program owner");
    const auto free = pin->declaration.free_directions.size(); guard.dimension(free); std::size_t offset{}, retained = pin->retained;
    std::vector<Field> output; std::map<std::uint64_t, std::size_t> positions;
    for (const auto& node : pin->target.declaration().nodes) {
        guard.step(); Field value{node.key, node.output_grade, {tensor({}, guard), Matrix(0, free)}, {}};
        if (node.kind == FieldProgramNodeKind::Input) {
            const auto k = node.output_grade.value; value.value.particular = act(pin->inclusion[k], old_result.field(node.key), guard);
            value.value.directions = Matrix(pin->inclusion[k].rows(), free);
            for (const auto& [ij, q] : pin->kernel[k].entries()) { guard.step(); value.value.directions.set(ij.first, add(offset, ij.second), q); }
            offset = add(offset, pin->kernel[k].cols());
            const auto projected = act(pin->reconstruction[k], value.value.particular, guard);
            require(equal(projected, old_result.field(node.key)) && numeric::checked_multiply(pin->reconstruction[k], value.value.directions, guard).nonzeros() == 0,
                "extension input family does not preserve original observed projection");
        } else {
            const auto& input = output.at(positions.at(*node.input)).value; const auto action = pin->target.node_action(node.key);
            value.value = {field_program_detail::apply(*action, input.particular, guard), numeric::checked_multiply(action->matrix(), input.directions, guard)};
            if (const auto flags = pin->target.occurrence_action(node.key))
                value.occurrences = RelationalExtensionAffineField{field_program_detail::apply(*flags, input.particular, guard), numeric::checked_multiply(flags->matrix(), input.directions, guard)};
        }
        admit_field(value.value.particular, value.value.directions.rows(), guard); guard.matrix(value.value.directions);
        retained = add(retained, add(value.value.particular.values().nonzeros(), value.value.directions.nonzeros()));
        if (value.occurrences) retained = add(retained, add(value.occurrences->particular.values().nonzeros(), value.occurrences->directions.nonzeros()));
        guard.entries(retained);
        positions.emplace(node.key, output.size()); output.push_back(std::move(value));
    }
    require(offset == free, "extension whole completion carrier not consumed"); guard.step(); return RelationalExtensionFieldFamily({parent, original, std::move(output)});
}
inline RelationalExtensionEvaluation RelationalExtensionFieldFamily::evaluate(numeric::RationalVector free,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace field_extension_detail;
    const auto parent = *this; const auto pin = parent.state_; const auto frozen = free; Guard guard(limits, std::move(check)); guard.step();
    const auto extension = pin->extension; require(frozen.size() == extension.declaration().free_directions.size(), "every completion direction must be explicitly supplied");
    guard.vector(frozen); const auto old_result = pin->input.result(); std::vector<RelationalFieldInput> inputs;
    for (const auto& input : old_result.inputs()) inputs.push_back({input.node, input.use, affine_value(parent.field(input.node), frozen, guard)});
    auto result = extension.target().execute(std::move(inputs), limits, [&] { guard.step(); });
    for (const auto& field : pin->fields) {
        require(equal(affine_value(field.value, frozen, guard), result.field(field.node)), "extension complete affine field replay mismatch");
        if (field.occurrences) require(equal(affine_value(*field.occurrences, frozen, guard), result.occurrence_field(field.node)), "extension original occurrence family replay mismatch");
    }
    guard.step(); return {parent, std::move(result), guard.canonical_input(frozen)};
}
} // namespace rfc::inference
