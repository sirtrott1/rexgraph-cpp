#pragma once

#include "rfc/field/exact_metric.hpp"
#include "rfc/field/operator_contract.hpp"
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <initializer_list>
#include <memory>
#include <optional>

namespace rfc::field {

class PreparedExactExecutionTower;

// One fixed source-grade operation. Factors are immutable; every application
// has independent scratch and rechecks its current limits and cancellation.
class PreparedGlobalAction {
    struct State {
        std::shared_ptr<const core::RelationalComplex> source;
        OperatorRequest operation;
        numeric::ExactMatrix metric, frame, dual;
        std::optional<numeric::PreparedExactSolve> solve;
        std::size_t width{}, output_width{}, output_offset{}, rhs_offset{}, dimension{}, retained_entries{}, preparation_steps{};

        static numeric::ExactMatrix boundary(const core::RelationalComplex& source_value,
                core::Grade grade, numeric::ExactActionGuard& guard) {
            const auto& upper = source_value.grade(grade);
            const auto& lower = source_value.grade(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)});
            guard.dimension(lower.cell_count()); guard.dimension(upper.cell_count());
            numeric::ExactMatrix result(lower.cell_count(), upper.cell_count());
            const auto exists = source_value.signature().existence_aspect();
            for (std::size_t cell = 0; cell < upper.cell_count(); ++cell) {
                const auto [begin, end] = upper.incidence_range(cell);
                for (auto p = begin; p < end; ++p) {
                    guard.step();
                    if (!upper.aspects.get(exists, p)) continue;
                    const auto row = upper.lower_index[p];
                    const numeric::Rational lattice{upper.lattice_coefficient[p]};
                    guard.value(lower.cell_scales[row]); guard.value(upper.cell_scales[cell]); guard.value(lattice);
                    const auto value = guard.div(guard.mul(lower.cell_scales[row], lattice), upper.cell_scales[cell]);
                    result.set(row, cell, guard.add(result.get(row, cell), value));
                    guard.entries(result.nonzeros());
                }
            }
            return result;
        }
        static numeric::ExactMatrix owned_metric(const ExactMetric& value, numeric::ExactActionGuard& guard) {
            // External callbacks do not constitute an immutable coefficient
            // owner. Their existing one-shot action remains available.
            if (value.kind() != ExactMetricKind::Identity && value.kind() != ExactMetricKind::Diagonal &&
                value.kind() != ExactMetricKind::SparseMatrix)
                throw std::invalid_argument("prepared global action requires owned unit, diagonal or sparse metrics");
            guard.dimension(value.size());
            if (value.is_diagonal()) guard.entries(value.size());
            auto result = value.materialize(); guard.matrix(result); return result;
        }
        State(std::shared_ptr<const core::RelationalComplex> owner, const std::vector<ExactMetric>& metrics,
              OperatorRequest request, const numeric::ExactMatrix& harmonic_frame, numeric::ExactActionGuard& guard)
            : source(std::move(owner)), operation(std::move(request)) {
            const auto start = guard.steps();
            guard.step();
            if (!source || operation.grade.value >= source->grade_count() || metrics.size() != source->grade_count())
                throw std::invalid_argument("prepared global action source, grade or metric count");
            for (std::size_t k = 0; k < metrics.size(); ++k)
                if (metrics[k].size() != source->grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count())
                    throw std::invalid_argument("prepared global action metric dimension");
            width = source->grade(operation.grade).cell_count(); output_width = width; guard.dimension(width);
            guard.value(operation.lambda); operation.lambda = numeric::canonical_rational(operation.lambda);
            const bool inverse = operation.kind == OperatorKind::HarmonicInverse || operation.kind == OperatorKind::GradientPotential;
            if (inverse && operation.lambda != 0) throw std::invalid_argument("harmonic inverse and potential have no lambda parameter");
            if (operation.kind == OperatorKind::GradientPotential && operation.grade.value == 0)
                throw std::invalid_argument("grade zero has no lower potential carrier");
            if (operation.kind == OperatorKind::Green && operation.lambda == 0) {
                dimension = width;
            } else if (operation.kind == OperatorKind::Green || inverse) {
                metric = owned_metric(metrics[operation.grade.value], guard);
                const auto below = operation.grade.value ? core::Grade{static_cast<std::uint16_t>(operation.grade.value - 1u)} : core::Grade{0};
                const auto above = core::Grade{static_cast<std::uint16_t>(operation.grade.value + 1u)};
                const auto l = operation.grade.value ? source->grade(below).cell_count() : 0;
                const auto u = operation.grade.value < source->max_grade().value ? source->grade(above).cell_count() : 0;
                const auto lower = operation.grade.value ? boundary(*source, operation.grade, guard) : numeric::ExactMatrix(0, width);
                const auto upper = u ? boundary(*source, above, guard) : numeric::ExactMatrix(width, 0);
                const auto lower_metric = l ? owned_metric(metrics[below.value], guard) : numeric::ExactMatrix(0, 0);
                const auto upper_metric = u ? owned_metric(metrics[above.value], guard) : numeric::ExactMatrix(0, 0);
                // Auxiliary variables first, x last: the paper's equations,
                // with a column permutation that removes identity blocks early.
                if (inverse) {
                    if (harmonic_frame.rows() != width) throw std::invalid_argument("prepared harmonic frame dimension");
                    guard.matrix(harmonic_frame); frame = harmonic_frame;
                }
                const auto h = inverse ? frame.cols() : 0;
                const std::size_t sizes[]{width, l, l, u, u ? width : 0, u ? width : 0, u, h, width};
                std::size_t offsets[9]{};
                for (std::size_t i = 0; i < 9; ++i) {
                    offsets[i] = dimension; dimension = machine::checked_size_add(dimension, sizes[i]);
                }
                guard.dimension(dimension);
                const auto a = offsets[0], b = offsets[1], c = offsets[2], d = offsets[3];
                const auto f = offsets[4], g = offsets[5], e = offsets[6], t = offsets[7], x = offsets[8];
                rhs_offset = x; output_offset = x;
                if (operation.kind == OperatorKind::GradientPotential) {output_offset = b; output_width = l;}
                numeric::ExactMatrix system(dimension, dimension);
                const auto put = [&](std::size_t row, std::size_t column, const numeric::ExactMatrix& block,
                                     const numeric::Rational& scale = numeric::Rational{1}) {
                    guard.matrix(block);
                    if (machine::checked_size_add(row, block.rows()) > dimension ||
                        machine::checked_size_add(column, block.cols()) > dimension)
                        throw std::out_of_range("prepared Green block insertion");
                    for (const auto& [key, value] : block.entries()) {
                        const auto i = row + key.first, j = column + key.second;
                        system.set(i, j, guard.add(system.get(i, j), guard.mul(scale, value)));
                        guard.entries(system.nonzeros());
                    }
                };
                put(a, a, numeric::ExactMatrix::identity(width)); put(a, x, metric, -1);
                if (l) {
                    put(b, b, numeric::ExactMatrix::identity(l)); put(b, x, lower, -1);
                    put(c, c, numeric::ExactMatrix::identity(l)); put(c, b, lower_metric, -1);
                }
                if (u) {
                    put(d, d, numeric::ExactMatrix::identity(u)); put(d, a, upper.transpose(), -1);
                    put(f, f, numeric::ExactMatrix::identity(width)); put(f, e, upper, -1);
                    put(g, g, numeric::ExactMatrix::identity(width)); put(g, f, metric, -1);
                    put(e, e, upper_metric); put(e, d, numeric::ExactMatrix::identity(u), -1);
                }
                if (inverse) {
                    // L x + H t = j, H^T M x = 0. The complete harmonic
                    // frame fixes the full kernel, including branching H0.
                    if (h) {
                        put(t, a, frame.transpose());
                        put(x, t, numeric::checked_multiply(metric, frame, guard));
                    }
                    if (l) put(x, c, lower.transpose());
                    if (u) put(x, g, numeric::ExactMatrix::identity(width));
                } else {
                    put(x, a, numeric::ExactMatrix::identity(width));
                    if (l) put(x, c, lower.transpose(), operation.lambda);
                    if (u) put(x, g, numeric::ExactMatrix::identity(width), operation.lambda);
                }
                solve.emplace(std::move(system), guard);
                retained_entries = machine::checked_size_add(metric.nonzeros(), solve->stored_entries());
                retained_entries = machine::checked_size_add(retained_entries, frame.nonzeros());
            } else if (operation.kind == OperatorKind::HarmonicProjector && operation.lambda == 0) {
                if (harmonic_frame.rows() != width) throw std::invalid_argument("prepared harmonic frame dimension");
                guard.matrix(harmonic_frame); frame = harmonic_frame;
                if (frame.cols()) {
                    const auto form = owned_metric(metrics[operation.grade.value], guard);
                    dual = numeric::checked_multiply(form, frame, guard).transpose();
                    solve.emplace(numeric::checked_multiply(dual, frame, guard), guard);
                }
                dimension = frame.cols();
                retained_entries = machine::checked_size_add(frame.nonzeros(), dual.nonzeros());
                if (solve) retained_entries = machine::checked_size_add(retained_entries, solve->stored_entries());
            } else {
                throw std::invalid_argument("operator is not a prepared global action");
            }
            guard.entries(retained_entries); preparation_steps = guard.steps() - start;
        }
    };
public:
    PreparedGlobalAction(const PreparedGlobalAction&) = default;
    PreparedGlobalAction& operator=(const PreparedGlobalAction&) = default;
    OperatorRequest operation() const { return state_->operation; }
    std::shared_ptr<const core::RelationalComplex> source() const { return state_->source; }
    std::size_t solve_dimension() const noexcept { return state_->dimension; }
    std::size_t retained_entries() const noexcept { return state_->retained_entries; }
    std::size_t preparation_steps() const noexcept { return state_->preparation_steps; }
    core::Grade output_grade() const noexcept {
        const auto grade = state_->operation.grade;
        return state_->operation.kind == OperatorKind::GradientPotential
            ? core::Grade{static_cast<std::uint16_t>(grade.value - 1u)} : grade;
    }
    std::size_t output_size() const noexcept { return state_->output_width; }
    numeric::RationalVector apply(const numeric::RationalVector& input, numeric::ExactActionGuard& guard) const {
        const auto pin = state_;
        guard.step();
        if (input.size() != pin->width) throw std::invalid_argument("prepared global action input dimension");
        guard.entries(pin->retained_entries); guard.dimension(pin->dimension);
        const auto values = guard.canonical_input(input);
        if (pin->operation.kind == OperatorKind::HarmonicProjector) {
            guard.matrix(pin->frame);
            if (!pin->solve) return numeric::RationalVector(pin->width);
            return numeric::checked_apply(pin->frame,
                pin->solve->apply(numeric::checked_apply(pin->dual, values, guard), guard), guard);
        }
        if (!pin->solve) return values;
        const auto measured = numeric::checked_apply(pin->metric, values, guard);
        numeric::RationalVector rhs(pin->dimension);
        for (std::size_t i = 0; i < pin->width; ++i) rhs[pin->rhs_offset + i] = measured[i];
        // Every original equation is checked. Inverse fields are orthogonal to
        // the complete harmonic kernel. A potential is the certified B x
        // auxiliary, hence orthogonal to the complete kernel of B^dagger.
        const auto solution = pin->solve->apply(rhs, guard);
        const auto end = machine::checked_size_add(pin->output_offset, pin->output_width);
        if (end > solution.size()) throw std::logic_error("prepared action output range");
        return numeric::RationalVector(solution.begin() + static_cast<std::ptrdiff_t>(pin->output_offset),
                                       solution.begin() + static_cast<std::ptrdiff_t>(end));
    }
    numeric::RationalVector apply(const numeric::RationalVector& input,
            numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        numeric::ExactActionGuard guard(limits, std::move(check)); return apply(input, guard);
    }
    numeric::RationalVector apply(std::initializer_list<numeric::Rational> input,
            numeric::ExactActionGuard& guard) const {
        return apply(numeric::RationalVector(input),guard);
    }
    numeric::RationalVector apply(std::initializer_list<numeric::Rational> input,
            numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        numeric::ExactActionGuard guard(limits,std::move(check));
        return apply(numeric::RationalVector(input),guard);
    }
    numeric::SparseRationalVector apply(const numeric::SparseRationalVector& input,numeric::ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();
        if(input.size()!=pin->width)throw std::invalid_argument("prepared sparse global action input dimension");
        guard.entries(machine::checked_size_add(pin->retained_entries,input.nonzeros()));guard.dimension(pin->dimension);guard.sparse_vector(input);
        if(pin->operation.kind==OperatorKind::HarmonicProjector){
            guard.matrix(pin->frame);if(!pin->solve)return numeric::SparseRationalVector(pin->width);
            return numeric::checked_apply(pin->frame,pin->solve->apply(numeric::checked_apply(pin->dual,input,guard),guard),guard);
        }
        if(!pin->solve)return input;
        const auto measured=numeric::checked_apply(pin->metric,input,guard);
        std::vector<numeric::SparseRationalEntry> entries;entries.reserve(measured.nonzeros());
        for(const auto& entry:measured.entries()){
            guard.step();entries.push_back({machine::checked_size_add(pin->rhs_offset,entry.index),entry.value});
            guard.entries(machine::checked_size_add(pin->retained_entries,machine::checked_size_add(measured.nonzeros(),entries.size())));
        }
        const auto solution=pin->solve->apply(numeric::SparseRationalVector(pin->dimension,std::move(entries)),guard);
        const auto end=machine::checked_size_add(pin->output_offset,pin->output_width);
        if(end>solution.size())throw std::logic_error("prepared sparse action output range");
        std::vector<numeric::SparseRationalEntry> selected;
        for(const auto& entry:solution.entries()){
            guard.step();if(entry.index>=pin->output_offset && entry.index<end)selected.push_back({entry.index-pin->output_offset,entry.value});
            guard.entries(machine::checked_size_add(pin->retained_entries,machine::checked_size_add(solution.nonzeros(),selected.size())));
        }
        return numeric::SparseRationalVector(pin->output_width,std::move(selected));
    }
    numeric::SparseRationalVector apply(const numeric::SparseRationalVector& input,
            numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        numeric::ExactActionGuard guard(limits,std::move(check));return apply(input,guard);
    }
private:
    friend class PreparedExactExecutionTower;
    PreparedGlobalAction(std::shared_ptr<const core::RelationalComplex> source, const std::vector<ExactMetric>& metrics,
            OperatorRequest operation, const numeric::ExactMatrix& harmonic_frame, numeric::ExactActionGuard& guard)
        : state_(std::make_shared<const State>(std::move(source), metrics, std::move(operation), harmonic_frame, guard)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
