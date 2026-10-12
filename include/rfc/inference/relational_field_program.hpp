#pragma once

#include "rfc/local/owned_fiber_system.hpp"
#include "rfc/local/prepared_field_action.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/numeric/prepared_sparse_map.hpp"
#include "rfc/storage/native_series.hpp"
#include <map>
#include <set>

namespace rfc::inference {
struct StalkBasisDeclaration {
    core::CellId cell;
    std::uint64_t basis{};
    std::vector<std::uint64_t> directions;
    numeric::ExactMatrix metric;
};
struct OccurrenceMapDeclaration {
    core::Grade grade;
    std::size_t occurrence{};
    storage::TemporalTensorRole role{storage::TemporalTensorRole::RealizedIncidenceMap};
    numeric::ExactMatrix map;
};
enum class FieldProgramNodeKind : std::uint8_t { Input, Boundary, DeclaredMap };
struct FieldProgramNode {
    std::uint64_t key{};
    FieldProgramNodeKind kind{FieldProgramNodeKind::Input};
    core::Grade output_grade;
    std::optional<std::uint64_t> input;
    numeric::ExactMatrix map;
};
struct RelationalFieldProgramDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<StalkBasisDeclaration> stalks;
    std::vector<OccurrenceMapDeclaration> occurrences;
    std::vector<FieldProgramNode> nodes;
    bool differential{};
};
struct RelationalFieldInput {
    std::uint64_t node{}, use{};
    numeric::SparseRationalTensor field = numeric::SparseRationalTensor::zero(
        numeric::IndexDomain({0}), numeric::StructuralSupport::full(0));
};
struct SelectedStalkField {
    core::CellId cell;
    std::uint64_t basis{};
    std::vector<std::uint64_t> directions;
    numeric::SparseRationalTensor field;
};

namespace field_program_detail {
using Guard = numeric::ExactActionGuard;
using Matrix = numeric::ExactMatrix;
using Tensor = numeric::SparseRationalTensor;
inline void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
inline std::size_t add(std::size_t a, std::size_t b) { return machine::checked_size_add(a, b); }
inline core::Grade grade(std::size_t k) {
    if (k > std::numeric_limits<std::uint16_t>::max()) throw std::length_error("field program grade limit");
    return {static_cast<std::uint16_t>(k)};
}
inline std::size_t bits(const numeric::Rational& q) {
    const auto size = [](mpz_srcptr z) { return mpz_sgn(z) ? mpz_sizeinbase(z, 2) : std::size_t{0}; };
    return std::max(size(mpq_numref(q.get_mpq_t())), size(mpq_denref(q.get_mpq_t())));
}
inline void admit_field(const Tensor& field, std::size_t size, Guard& guard) {
    require(field.domain().extents() == std::vector<std::size_t>{size}, "field program grade domain mismatch");
    require(field.support().count() == size, "field program requires complete supported grade fields");
    guard.dimension(size); guard.entries(field.values().nonzeros());
    field.values().for_each_nonzero([&](std::size_t, const auto& q) { guard.step(); guard.value(q); });
}
inline Tensor apply(const numeric::PreparedSparseExactMap& action, const Tensor& input, Guard& guard) {
    const auto& map = action.matrix();
    guard.matrix(map); admit_field(input, map.cols(), guard);
    std::size_t coefficient_bits{1}, input_bits{1};
    for (const auto& [coordinate, q] : map.entries()) {
        (void)coordinate; coefficient_bits = std::max(coefficient_bits, bits(q));
    }
    input.values().for_each_nonzero([&](std::size_t, const auto& q) { input_bits = std::max(input_bits, bits(q)); });
    // Conservative product/sum admission includes cancelled intermediate values.
    const auto terms = add(map.nonzeros(), 1);
    const auto bound = machine::checked_size_mul(terms, add(add(coefficient_bits, input_bits), 1));
    if (bound > guard.limits().integer_bits) throw std::length_error("field program exact growth admission");
    auto output = action.apply(input, [&] { guard.step(); });
    admit_field(output, map.rows(), guard);
    return output;
}
inline void source_values(const core::RelationalComplex& source, Guard& guard) {
    guard.dimension(source.grade_count());
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        const auto& arena = source.grade(grade(k));
        guard.dimension(arena.cell_count()); guard.entries(arena.incidence_count());
        for (const auto& q : arena.cell_scales) { guard.step(); guard.value(q); }
        for (const auto& z : arena.lattice_coefficient) { guard.step(); guard.value(numeric::Rational{z}); }
    }
}
inline local::OwnedLinearFiberSystem make_fibers(const storage::NativeSeriesEndpoint& publication,
        const RelationalFieldProgramDeclaration& declaration, Guard& guard) {
    const auto& source = publication.endpoint().source;
    source_values(source, guard);
    require(declaration.key && !declaration.nodes.empty(), "field program needs identity and ordered nodes");
    local::LinearStalkBundle bundle(source);
    std::map<std::uint64_t, std::vector<std::uint64_t>> bases;
    std::size_t position{};
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        for (std::size_t i = 0; i < source.grade(grade(k)).cell_count(); ++i) {
            guard.step(); require(position < declaration.stalks.size(), "field program omits a stalk basis");
            const auto& stalk = declaration.stalks[position++];
            const core::CellId cell{grade(k), static_cast<std::uint32_t>(i)};
            require(stalk.cell == cell && stalk.basis && !stalk.directions.empty(), "field program basis order or identity");
            std::set<std::uint64_t> directions;
            for (const auto key : stalk.directions) {
                guard.step(); require(key && directions.insert(key).second, "field program duplicate or zero basis direction");
            }
            const auto [it, inserted] = bases.emplace(stalk.basis, stalk.directions);
            require(inserted || it->second == stalk.directions, "field program basis identity changes ordered directions");
            const auto n = stalk.directions.size(); guard.dimension(n); guard.matrix(stalk.metric);
            require(stalk.metric.rows() == n && stalk.metric.cols() == n, "field program stalk metric dimension");
            field::hodge_frame_detail::positive(stalk.metric, guard);
            bundle.set_dimension(cell, n);
        }
        guard.dimension(bundle.total_dimension(grade(k)));
    }
    require(position == declaration.stalks.size(), "field program extra stalk basis");
    local::LinearRelationalFiberSystem fibers(std::move(bundle)); position = 0;
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        const auto& arena = source.grade(grade(k));
        for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
            guard.step(); require(position < declaration.occurrences.size(), "field program omits an original occurrence map");
            const auto& declared = declaration.occurrences[position++];
            require(declared.grade == grade(k) && declared.occurrence == slot, "field program original occurrence map order");
            guard.matrix(declared.map);
            const bool active = arena.aspects.get(source.signature().existence_aspect(), slot);
            const auto coefficient = active ? source.incidence_coefficient(grade(k), slot) : numeric::Rational{0};
            guard.value(coefficient);
            Matrix realized(declared.map.rows(), declared.map.cols());
            if (declared.role == storage::TemporalTensorRole::CoefficientIndependentMap) {
                for (const auto& [coordinate, q] : declared.map.entries())
                    realized.set(coordinate.first, coordinate.second, guard.mul(coefficient, q));
            } else {
                require(declared.role == storage::TemporalTensorRole::RealizedIncidenceMap, "unsupported field program occurrence map role");
                require(coefficient != 0 || declared.map.nonzeros() == 0, "inactive or zero flag has a nonzero realized map");
                realized = declared.map;
            }
            fibers.set_transport(grade(k), slot, std::move(realized));
        }
    }
    require(position == declaration.occurrences.size(), "field program extra original occurrence map");
    if (declaration.differential) {
        std::optional<Matrix> previous;
        for (std::size_t k = 1; k < source.grade_count(); ++k) {
            auto boundary = local::GradeTensorField(fibers, grade(k)).matrix(); guard.matrix(boundary);
            if (previous) require(numeric::checked_multiply(*previous, boundary, guard).nonzeros() == 0,
                                  "field program violates an adjacent realized chain law");
            previous = std::move(boundary);
        }
    }
    return local::OwnedLinearFiberSystem(fibers);
}
struct NodeAction {
    std::optional<numeric::PreparedSparseExactMap> assembled, occurrences;
};
struct ProgramState {
    storage::NativeSeriesEndpoint publication;
    RelationalFieldProgramDeclaration declaration;
    local::OwnedLinearFiberSystem fibers;
    std::vector<NodeAction> actions;
    std::size_t retained{};
    ProgramState(storage::NativeSeriesEndpoint p, RelationalFieldProgramDeclaration d, Guard& guard)
        : publication(std::move(p)), declaration(std::move(d)), fibers(make_fibers(publication, declaration, guard)) {
        std::map<std::uint64_t, core::Grade> outputs;
        const auto& system = fibers.fibers();
        for (const auto& node : declaration.nodes) {
            guard.step(); require(node.key && !outputs.contains(node.key), "field program duplicate or zero node");
            require(node.output_grade.value < system.complex().grade_count(), "field program output grade");
            NodeAction action;
            if (node.kind == FieldProgramNodeKind::Input) {
                require(!node.input && node.map.rows() == 0 && node.map.cols() == 0, "field program input node has an operation");
            } else {
                require(node.input && outputs.contains(*node.input), "field program node needs an earlier input");
                if (node.kind == FieldProgramNodeKind::Boundary) {
                    const auto upper = outputs.at(*node.input);
                    require(upper.value > 0 && node.output_grade.value + 1u == upper.value &&
                            node.map.rows() == 0 && node.map.cols() == 0, "field program boundary grades or extra map");
                    const local::PreparedGradeFieldAction prepared(system, upper);
                    Matrix assembled(prepared.statistics().target_dimension, prepared.statistics().source_dimension);
                    std::vector<std::size_t> offsets{0};
                    for (const auto& block : prepared.blocks()) offsets.push_back(add(offsets.back(), block.rows));
                    guard.dimension(offsets.back());
                    Matrix occurrences(offsets.back(), assembled.cols());
                    for (const auto& entry : prepared.entries()) {
                        guard.step(); const auto& block = prepared.blocks().at(entry.block);
                        occurrences.set(add(offsets.at(entry.block), entry.target - block.target_begin), entry.source, entry.coefficient);
                        assembled.set(entry.target, entry.source, guard.add(assembled.get(entry.target, entry.source), entry.coefficient));
                        guard.entries(add(assembled.nonzeros(), occurrences.nonzeros()));
                    }
                    action.assembled.emplace(std::move(assembled)); action.occurrences.emplace(std::move(occurrences));
                } else {
                    require(node.kind == FieldProgramNodeKind::DeclaredMap, "unsupported field program node kind");
                    require(node.map.cols() == system.stalks().total_dimension(outputs.at(*node.input)) &&
                            node.map.rows() == system.stalks().total_dimension(node.output_grade), "field program declared map carrier dimensions");
                    guard.matrix(node.map); action.assembled.emplace(node.map);
                }
            }
            if (action.assembled) retained = add(retained, action.assembled->matrix().nonzeros());
            if (action.occurrences) retained = add(retained, action.occurrences->matrix().nonzeros());
            actions.push_back(std::move(action)); outputs.emplace(node.key, node.output_grade);
        }
        for (const auto& stalk : declaration.stalks) retained = add(retained, add(stalk.metric.nonzeros(), stalk.directions.size()));
        for (const auto& occurrence : declaration.occurrences) retained = add(retained, add(3, occurrence.map.nonzeros()));
        retained = add(retained, declaration.nodes.size()); guard.entries(retained);
    }
    void validate(Guard& guard) const {
        guard.step(); source_values(publication.endpoint().source, guard); guard.entries(retained);
        for (const auto& stalk : declaration.stalks) guard.matrix(stalk.metric);
        for (const auto& occurrence : declaration.occurrences) guard.matrix(occurrence.map);
        for (const auto& action : actions) {
            if (action.assembled) guard.matrix(action.assembled->matrix());
            if (action.occurrences) guard.matrix(action.occurrences->matrix());
        }
    }
};
struct ResultField {
    std::uint64_t node{};
    core::Grade grade;
    Tensor field = Tensor::zero(numeric::IndexDomain({0}), numeric::StructuralSupport::full(0));
    std::optional<Tensor> occurrences;
};
} // namespace field_program_detail

class OwnedRelationalFieldResult;
class OwnedRelationalFieldProgram {
public:
    OwnedRelationalFieldProgram(storage::NativeSeriesEndpoint publication, RelationalFieldProgramDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        // Copy moved buffers as well: caller-retained mutable references cannot publish into this owner.
        auto frozen = declaration;
        numeric::ExactActionGuard guard(limits, std::move(check));
        state_ = std::make_shared<const field_program_detail::ProgramState>(std::move(publication), std::move(frozen), guard);
    }
    const RelationalFieldProgramDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFieldProgramDeclaration& declaration() const && = delete;
    storage::NativeSeriesEndpoint publication() const { return state_->publication; }
    const local::OwnedLinearFiberSystem& fibers() const & { return state_->fibers; }
    const local::OwnedLinearFiberSystem& fibers() const && = delete;
    OwnedRelationalFieldResult execute(std::vector<RelationalFieldInput> input, numeric::ExactActionLimits limits = {},
                                      std::function<void()> check = {}) const;
private:
    friend class OwnedRelationalFieldResult;
    explicit OwnedRelationalFieldProgram(std::shared_ptr<const field_program_detail::ProgramState> state) : state_(std::move(state)) {}
    std::shared_ptr<const field_program_detail::ProgramState> state_;
};

class OwnedRelationalFieldResult {
    struct State {
        OwnedRelationalFieldProgram program;
        std::vector<RelationalFieldInput> inputs;
        std::vector<field_program_detail::ResultField> fields;
    };
public:
    OwnedRelationalFieldProgram program() const { return state_->program; }
    const numeric::SparseRationalTensor& field(std::uint64_t node) const & { return find(node).field; }
    const numeric::SparseRationalTensor& field(std::uint64_t) const && = delete;
    const numeric::SparseRationalTensor& occurrence_field(std::uint64_t node) const & {
        const auto& value = find(node); field_program_detail::require(value.occurrences.has_value(), "node has no original occurrence field");
        return *value.occurrences;
    }
    const numeric::SparseRationalTensor& occurrence_field(std::uint64_t) const && = delete;
    const std::vector<RelationalFieldInput>& inputs() const & { return state_->inputs; }
    const std::vector<RelationalFieldInput>& inputs() const && = delete;
    std::vector<SelectedStalkField> select(std::uint64_t node, std::vector<core::CellId> cells,
        std::size_t coordinate_limit, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        using namespace field_program_detail;
        const auto pin = state_; const auto requested = cells;
        Guard guard(limits, std::move(check)); pin->program.state_->validate(guard);
        const auto& value = find(*pin, node);
        const auto offsets = pin->program.fibers().fibers().stalks().offsets(value.grade);
        std::set<std::uint32_t> selected; std::size_t coordinates{};
        for (const auto cell : requested) {
            guard.step(); require(cell.grade == value.grade && cell.index + std::size_t{1} < offsets.size() &&
                selected.insert(cell.index).second, "selected field cells must be distinct in the node grade");
            coordinates = add(coordinates, offsets[cell.index + 1u] - offsets[cell.index]);
        }
        if (coordinates > coordinate_limit) throw std::length_error("field program bounded stalk export limit");
        guard.entries(coordinates); std::vector<SelectedStalkField> output;
        const auto& entries = value.field.values().entries();
        for (const auto cell : requested) {
            const auto begin = offsets[cell.index], end = offsets[cell.index + 1u];
            std::vector<numeric::SparseRationalEntry> local;
            for (auto it = std::lower_bound(entries.begin(), entries.end(), begin,
                    [](const auto& entry, std::size_t i) { return entry.index < i; }); it != entries.end() && it->index < end; ++it) {
                guard.step(); guard.value(it->value); local.push_back({it->index - begin, it->value});
            }
            const auto& stalks = pin->program.declaration().stalks;
            const auto basis = std::find_if(stalks.begin(), stalks.end(), [&](const auto& d) { return d.cell == cell; });
            output.push_back({cell, basis->basis, basis->directions, Tensor(numeric::IndexDomain({end - begin}),
                numeric::StructuralSupport::full(end - begin), numeric::SparseRationalVector(end - begin, std::move(local)))});
        }
        guard.step(); return output;
    }
private:
    friend class OwnedRelationalFieldProgram;
    static const field_program_detail::ResultField& find(const State& state, std::uint64_t node) {
        const auto it = std::find_if(state.fields.begin(), state.fields.end(), [&](const auto& value) { return value.node == node; });
        if (it == state.fields.end()) throw std::out_of_range("field program result node");
        return *it;
    }
    const field_program_detail::ResultField& find(std::uint64_t node) const { return find(*state_, node); }
    explicit OwnedRelationalFieldResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};

inline OwnedRelationalFieldResult OwnedRelationalFieldProgram::execute(std::vector<RelationalFieldInput> input,
        numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace field_program_detail;
    const auto pin = state_; const auto frozen = input;
    Guard guard(limits, std::move(check)); pin->validate(guard);
    OwnedRelationalFieldResult::State result{OwnedRelationalFieldProgram(pin), {}, {}};
    std::map<std::uint64_t, const RelationalFieldInput*> supplied;
    for (const auto& value : frozen) {
        guard.step(); require(value.use && supplied.emplace(value.node, &value).second, "field program input use or duplicate node");
    }
    std::map<std::uint64_t, std::size_t> previous;
    std::size_t retained = pin->retained, consumed{};
    for (std::size_t index = 0; index < pin->declaration.nodes.size(); ++index) {
        guard.step(); const auto& node = pin->declaration.nodes[index];
        ResultField output{node.key, node.output_grade,
            Tensor::zero(numeric::IndexDomain({0}), numeric::StructuralSupport::full(0)), {}};
        if (node.kind == FieldProgramNodeKind::Input) {
            require(supplied.contains(node.key), "field program missing complete input");
            const auto& value = *supplied.at(node.key); ++consumed;
            admit_field(value.field, pin->fibers.fibers().stalks().total_dimension(node.output_grade), guard);
            output.field = value.field; result.inputs.push_back(value);
        } else {
            const auto& field = result.fields.at(previous.at(*node.input)).field;
            output.field = apply(*pin->actions[index].assembled, field, guard);
            if (pin->actions[index].occurrences) output.occurrences = apply(*pin->actions[index].occurrences, field, guard);
        }
        retained = add(retained, output.field.values().nonzeros());
        if (output.occurrences) retained = add(retained, output.occurrences->values().nonzeros());
        guard.entries(retained); previous.emplace(node.key, result.fields.size()); result.fields.push_back(std::move(output));
    }
    require(consumed == supplied.size(), "field program supplied input for a non-input node");
    guard.step(); return OwnedRelationalFieldResult(std::move(result));
}
} // namespace rfc::inference
