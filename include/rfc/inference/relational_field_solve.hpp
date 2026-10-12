#pragma once

#include "rfc/inference/relational_field_interface.hpp"
#include "rfc/local/interior_interface_solve.hpp"

namespace rfc::inference {
enum class RelationalFieldSolveKind : std::uint8_t { Green, HarmonicInverse };
struct RelationalFieldSolveDeclaration {
    std::uint64_t key{}, revision{}, coordinate_carrier{}, coordinate_basis{};
    core::Grade grade;
    RelationalFieldSolveKind kind{RelationalFieldSolveKind::Green};
    numeric::Rational lambda{};
    // Absent means derive the complete exact kernel. A supplied frame is verified.
    std::optional<numeric::ExactMatrix> harmonic_frame;
    std::vector<std::size_t> interior;
};
namespace relational_solve_detail {
using namespace field_interface_detail;
inline Matrix boundary(const OwnedRelationalFieldProgram& program, core::Grade g, Guard& guard) {
    guard.step();
    const local::PreparedGradeFieldAction action(program.fibers().fibers(), g);
    Matrix result(action.statistics().target_dimension, action.statistics().source_dimension);
    for (const auto& entry : action.entries()) {
        guard.step(); result.set(entry.target, entry.source, guard.add(result.get(entry.target, entry.source), entry.coefficient));
        guard.entries(result.nonzeros());
    }
    return result;
}
struct Layout {
    std::size_t a{}, b{}, c{}, d{}, f{}, g{}, e{}, t{}, x{}, size{};
};
struct Operator {
    Matrix lower, upper, metric, below_metric, above_metric, frame, augmented;
    Layout layout;
    std::size_t preparation_steps{};
};
inline Operator assemble(const OwnedRelationalFieldProgram& program, RelationalFieldSolveDeclaration& declaration, Guard& guard) {
    const auto start = guard.steps(); guard.step(); const auto& fibers = program.fibers().fibers();
    require(declaration.key && declaration.coordinate_carrier && declaration.coordinate_basis && program.declaration().differential,
        "relational solve requires a differential program and declared coordinate identities");
    require(declaration.grade.value < fibers.complex().grade_count(), "relational solve grade");
    guard.value(declaration.lambda); declaration.lambda = numeric::canonical_rational(declaration.lambda);
    const bool inverse = declaration.kind == RelationalFieldSolveKind::HarmonicInverse;
    require(inverse || declaration.kind == RelationalFieldSolveKind::Green, "relational solve kind");
    require(inverse ? declaration.lambda == 0 : declaration.lambda >= 0, "relational solve rational parameter");
    require(inverse || !declaration.harmonic_frame, "Green solve has no harmonic gauge declaration");
    const auto k = declaration.grade.value;
    const auto n = fibers.stalks().total_dimension(declaration.grade); guard.dimension(n);
    Operator op; op.metric = grade_metric(program, declaration.grade, guard);
    op.lower = k ? boundary(program, declaration.grade, guard) : Matrix(0, n);
    op.upper = k + 1u < fibers.complex().grade_count() ? boundary(program, field_program_detail::grade(k + 1u), guard) : Matrix(n, 0);
    op.below_metric = k ? grade_metric(program, field_program_detail::grade(k - 1u), guard) : Matrix(0, 0);
    op.above_metric = k + 1u < fibers.complex().grade_count() ? grade_metric(program, field_program_detail::grade(k + 1u), guard) : Matrix(0, 0);
    op.frame = Matrix(n, 0);
    if (inverse) {
        const auto constraints = Matrix::vertical_concat(op.lower, numeric::checked_multiply(op.upper.transpose(), op.metric, guard));
        const numeric::PreparedExactFrames kernel(constraints, guard);
        op.frame = declaration.harmonic_frame ? *declaration.harmonic_frame : kernel.kernel(); guard.matrix(op.frame);
        require(op.frame.rows() == n && op.frame.cols() == n - kernel.rank(), "relational solve gauge omits the complete harmonic kernel");
        require(numeric::checked_multiply(constraints, op.frame, guard).nonzeros() == 0, "relational solve gauge is not harmonic");
        const numeric::PreparedExactFrames frame_rank(op.frame, guard);
        require(frame_rank.rank() == op.frame.cols(), "relational solve gauge is not independent");
    }
    const auto l = op.lower.rows(), u = op.upper.cols(), h = op.frame.cols();
    const std::size_t sizes[]{n, l, l, u, u ? n : 0, u ? n : 0, u, h, n};
    std::size_t offsets[9]{}, size{};
    for (std::size_t i = 0; i < 9; ++i) { offsets[i] = size; size = add(size, sizes[i]); }
    guard.dimension(size);
    op.layout = {offsets[0], offsets[1], offsets[2], offsets[3], offsets[4], offsets[5], offsets[6], offsets[7], offsets[8], size};
    const auto& p = op.layout; op.augmented = Matrix(size, size);
    const auto put = [&](std::size_t row, std::size_t column, const Matrix& block, const numeric::Rational& scale = numeric::Rational{1}) {
        guard.matrix(block);
        require(add(row, block.rows()) <= size && add(column, block.cols()) <= size, "relational solve augmented block shape");
        for (const auto& [ij, q] : block.entries()) {
            guard.step(); op.augmented.set(add(row, ij.first), add(column, ij.second),
                guard.add(op.augmented.get(add(row, ij.first), add(column, ij.second)), guard.mul(scale, q)));
            guard.entries(op.augmented.nonzeros());
        }
    };
    put(p.a, p.a, Matrix::identity(n)); put(p.a, p.x, op.metric, -1);
    if (l) { put(p.b, p.b, Matrix::identity(l)); put(p.b, p.x, op.lower, -1);
        put(p.c, p.c, Matrix::identity(l)); put(p.c, p.b, op.below_metric, -1); }
    if (u) { put(p.d, p.d, Matrix::identity(u)); put(p.d, p.a, op.upper.transpose(), -1);
        put(p.f, p.f, Matrix::identity(n)); put(p.f, p.e, op.upper, -1);
        put(p.g, p.g, Matrix::identity(n)); put(p.g, p.f, op.metric, -1);
        put(p.e, p.e, op.above_metric); put(p.e, p.d, Matrix::identity(u), -1); }
    if (inverse) {
        if (h) { put(p.t, p.a, op.frame.transpose()); put(p.x, p.t, numeric::checked_multiply(op.metric, op.frame, guard)); }
        if (l) put(p.x, p.c, op.lower.transpose());
        if (u) put(p.x, p.g, Matrix::identity(n));
    } else {
        put(p.x, p.a, Matrix::identity(n));
        if (l) put(p.x, p.c, op.lower.transpose(), declaration.lambda);
        if (u) put(p.x, p.g, Matrix::identity(n), declaration.lambda);
    }
    op.preparation_steps = guard.steps() - start; return op;
}
inline numeric::RationalVector dense(const Tensor& input, std::size_t n, Guard& guard) {
    admit_field(input, n, guard); numeric::RationalVector out(n);
    input.values().for_each_nonzero([&](std::size_t i, const auto& q) { guard.step(); out[i] = numeric::canonical_rational(q); }); return out;
}
} // namespace relational_solve_detail

class RelationalFieldSolveResult;
class PreparedRelationalFieldSolve {
    struct State {
        OwnedRelationalFieldProgram program;
        RelationalFieldSolveDeclaration declaration;
        relational_solve_detail::Operator op;
        local::PreparedInteriorInterfaceSolve solve;
        State(OwnedRelationalFieldProgram owner, RelationalFieldSolveDeclaration d, numeric::ExactActionLimits limits,
            const std::function<void()>& check)
            : program(std::move(owner)), declaration(std::move(d)), op(make_operator(program, declaration, limits, check)),
              solve({declaration.coordinate_carrier, declaration.coordinate_carrier, declaration.coordinate_basis, declaration.coordinate_basis,
                     numeric::IndexDomain({op.layout.size}), numeric::IndexDomain({op.layout.size})},
                    op.augmented, declaration.interior, limits, check) {}
        static relational_solve_detail::Operator make_operator(const OwnedRelationalFieldProgram& p, RelationalFieldSolveDeclaration& d,
            numeric::ExactActionLimits limits, const std::function<void()>& check) {
            numeric::ExactActionGuard guard(limits, check); return relational_solve_detail::assemble(p, d, guard);
        }
        std::size_t retained() const {
            using relational_solve_detail::add;
            std::size_t n = solve.retained_entries();
            for (const auto* m : {&op.lower, &op.upper, &op.metric, &op.below_metric, &op.above_metric, &op.frame, &op.augmented}) n = add(n, m->nonzeros());
            return n;
        }
    };
public:
    PreparedRelationalFieldSolve(OwnedRelationalFieldProgram program, RelationalFieldSolveDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        const auto frozen = declaration;
        state_ = std::make_shared<const State>(std::move(program), frozen, limits, check);
        numeric::ExactActionGuard(limits).entries(state_->retained());
    }
    OwnedRelationalFieldProgram program() const { return state_->program; }
    const RelationalFieldSolveDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFieldSolveDeclaration& declaration() const && = delete;
    const relational_solve_detail::Operator& operators() const & { return state_->op; }
    const relational_solve_detail::Operator& operators() const && = delete;
    const local::PreparedInteriorInterfaceSolve& partition() const & { return state_->solve; }
    const local::PreparedInteriorInterfaceSolve& partition() const && = delete;
    std::size_t retained_entries() const { return state_->retained(); }
    RelationalFieldSolveResult apply(OwnedAccessionFieldResult rhs, std::uint64_t node,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
class RelationalFieldSolveResult {
    struct State {
        PreparedRelationalFieldSolve problem;
        OwnedAccessionFieldResult rhs;
        std::uint64_t node{};
        numeric::RationalVector augmented_rhs, reconstructed, interface_response, interface_rhs, residual;
        numeric::SparseRationalTensor field;
    };
public:
    PreparedRelationalFieldSolve problem() const { return state_->problem; }
    OwnedAccessionFieldResult rhs() const { return state_->rhs; }
    std::uint64_t node() const { return state_->node; }
    const numeric::SparseRationalTensor& field() const & { return state_->field; }
    const numeric::RationalVector& augmented_rhs() const & { return state_->augmented_rhs; }
    const numeric::RationalVector& reconstructed() const & { return state_->reconstructed; }
    const numeric::RationalVector& interface_response() const & { return state_->interface_response; }
    const numeric::RationalVector& interface_rhs() const & { return state_->interface_rhs; }
    const numeric::RationalVector& residual() const & { return state_->residual; }
    const numeric::SparseRationalTensor& field() const && = delete;
    const numeric::RationalVector& augmented_rhs() const && = delete;
    const numeric::RationalVector& reconstructed() const && = delete;
    const numeric::RationalVector& interface_response() const && = delete;
    const numeric::RationalVector& interface_rhs() const && = delete;
    const numeric::RationalVector& residual() const && = delete;
private:
    friend class PreparedRelationalFieldSolve;
    explicit RelationalFieldSolveResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline RelationalFieldSolveResult PreparedRelationalFieldSolve::apply(OwnedAccessionFieldResult rhs, std::uint64_t node,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_solve_detail;
    const auto parent = *this; const auto pin = parent.state_; const auto input = std::move(rhs); Guard guard(limits, std::move(check)); guard.step();
    guard.entries(pin->retained());
    const auto actual_program = input.result().program();
    require(actual_program.fibers().same_owner(pin->program.fibers()), "relational solve RHS requires the actual program owner");
    require(field_interface_detail::node(pin->program, node).output_grade == pin->declaration.grade, "relational solve RHS grade");
    const auto native = input.result(); const auto j = dense(native.field(node), pin->op.metric.cols(), guard);
    const auto mj = numeric::checked_apply(pin->op.metric, j, guard); numeric::RationalVector full_rhs(pin->op.layout.size);
    for (std::size_t i = 0; i < mj.size(); ++i) full_rhs[add(pin->op.layout.x, i)] = mj[i];
    auto full = pin->solve.apply(full_rhs, limits, [&] { guard.step(); });
    auto residual = numeric::checked_apply(pin->op.augmented, full, guard);
    for (std::size_t i = 0; i < residual.size(); ++i) { residual[i] = guard.sub(residual[i], full_rhs[i]); require(residual[i] == 0, "relational solve full reconstruction residual"); }
    numeric::RationalVector response; response.reserve(pin->solve.interface().size());
    for (const auto i : pin->solve.interface()) { guard.step(); response.push_back(full.at(i)); }
    auto reduced_rhs = numeric::checked_apply(pin->solve.schur(), response, guard);
    numeric::RationalVector x(j.size()); for (std::size_t i = 0; i < x.size(); ++i) { guard.step(); x[i] = full.at(add(pin->op.layout.x, i)); }
    auto field = field_interface_detail::tensor(x, guard); guard.step();
    return RelationalFieldSolveResult({parent, input, node, std::move(full_rhs), std::move(full), std::move(response), std::move(reduced_rhs), std::move(residual), std::move(field)});
}
} // namespace rfc::inference
