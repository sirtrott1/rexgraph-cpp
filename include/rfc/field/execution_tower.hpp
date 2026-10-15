#pragma once
#include "rfc/field/operator_contract.hpp"
#include "rfc/field/exact_dirac.hpp"
#include "rfc/field/augmented_green.hpp"
#include "rfc/field/prepared_global_action.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
#include <memory>
#include <span>
#include <functional>

namespace rfc::field {

class ExactExecutionTower {
public:
    static ExactExecutionTower unweighted(const core::RelationalComplex& complex) {
        std::vector<ExactMetric> metrics;
        metrics.reserve(complex.grade_count());
        for (std::size_t k=0;k<complex.grade_count();++k) {
            metrics.push_back(ExactMetric::identity(complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()));
        }
        return ExactExecutionTower(complex,ExactTowerKind::Unweighted,std::move(metrics));
    }

    static ExactExecutionTower weighted(const core::RelationalComplex& complex,
                                        std::vector<ExactMetric> metrics) {
        return ExactExecutionTower(complex,ExactTowerKind::Weighted,std::move(metrics));
    }

    [[nodiscard]] ExactTowerKind kind() const noexcept { return kind_; }
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] const std::vector<ExactMetric>& metrics() const noexcept { return metrics_; }

    [[nodiscard]] numeric::RationalVector apply(const OperatorRequest& request,
                                                 const numeric::RationalVector& input) const {
        if (request.grade.value >= complex_->grade_count()) throw std::out_of_range("execution tower grade");
        switch(request.kind) {
            case OperatorKind::Boundary: {
                if (request.grade.value==0) throw std::invalid_argument("grade zero has no boundary");
                return complex_->boundary_apply(request.grade,input);
            }
            case OperatorKind::Adjoint: {
                if (request.grade.value==0) throw std::invalid_argument("grade zero has no boundary adjoint");
                const BoundaryOperator B(*complex_,request.grade);
                return metric_adjoint_apply(B,metrics_[request.grade.value],metrics_[request.grade.value-1u],input);
            }
            case OperatorKind::HodgeDown: {
                ExactHodgeOperator L(*complex_,request.grade,metrics_);
                return L.apply_down(input);
            }
            case OperatorKind::HodgeUp: {
                ExactHodgeOperator L(*complex_,request.grade,metrics_);
                return L.apply_up(input);
            }
            case OperatorKind::Hodge: {
                ExactHodgeOperator L(*complex_,request.grade,metrics_);
                return L.apply(input);
            }
            case OperatorKind::Green: {
                ExactHodgeOperator L(*complex_,request.grade,metrics_);
                return exact_green_apply(L,request.lambda,input).value;
            }
            case OperatorKind::HarmonicProjector: {
                ExactHodgeOperator L(*complex_,request.grade,metrics_);
                return exact_hodge_projectors(L).project_harmonic(input);
            }
            case OperatorKind::HarmonicInverse: {
                if (request.lambda != 0) throw std::invalid_argument("harmonic inverse has no lambda parameter");
                return generalized_hodge_inverse_apply(ExactHodgeOperator(*complex_,request.grade,metrics_),input);
            }
            case OperatorKind::GradientPotential: {
                if (request.lambda != 0) throw std::invalid_argument("potential has no lambda parameter");
                if (!request.grade.value) throw std::invalid_argument("grade zero has no lower potential carrier");
                return complex_->boundary_apply(request.grade,
                    generalized_hodge_inverse_apply(ExactHodgeOperator(*complex_,request.grade,metrics_),input));
            }
            case OperatorKind::Dirac:
                throw std::invalid_argument("Dirac acts on an entire tower field; use apply_dirac");
        }
        throw std::logic_error("unknown exact operator request");
    }

    [[nodiscard]] ExactTowerField apply_dirac(const ExactTowerField& input) const {
        return exact_dirac_apply(*complex_,metrics_,input);
    }

    [[nodiscard]] ExactTowerField apply_hodge_tower(const ExactTowerField& input) const {
        return exact_hodge_tower_apply(*complex_,metrics_,input);
    }

private:
    ExactExecutionTower(const core::RelationalComplex& complex,
                        ExactTowerKind kind,
                        std::vector<ExactMetric> metrics)
        : complex_(&complex), kind_(kind), metrics_(std::move(metrics)) {
        if (metrics_.size()!=complex.grade_count()) throw std::invalid_argument("execution tower metric count mismatch");
        for (std::size_t k=0;k<metrics_.size();++k) {
            if (metrics_[k].size()!=complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()) throw std::invalid_argument("execution tower metric size mismatch");
        }
    }
    const core::RelationalComplex* complex_{};
    ExactTowerKind kind_{ExactTowerKind::Unweighted};
    std::vector<ExactMetric> metrics_;
};

enum class PreparedExactProfile {FiniteActions, GlobalActions};

// Both profiles own the same source and use the canonical field actions.
// GlobalActions additionally retains harmonic frames and permits exact solves.
class PreparedExactExecutionTower {
    struct State;
public:
    class BatchWorkspace {
    public:
        std::size_t capacity() const noexcept { return capacity_; }
        core::Grade grade() const noexcept { return grade_; }
        std::size_t retained_slots() const {
            auto n = machine::checked_size_add(down_.size(), up_.size());
            for (const auto size : {middle_.size(), lower_.size(), upper_.size(), scaled_.size()})
                n = machine::checked_size_add(n, size);
            return n;
        }
    private:
        friend class PreparedExactExecutionTower;
        BatchWorkspace(std::shared_ptr<const State> owner, core::Grade grade, std::size_t capacity)
            : owner_(std::move(owner)), grade_(grade), capacity_(capacity) {
            const auto& source = *owner_->source;
            const auto width = source.grade(grade).cell_count();
            const auto below = grade.value ? source.grade(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)}).cell_count() : 0;
            const auto above = grade.value < source.max_grade().value
                ? source.grade(core::Grade{static_cast<std::uint16_t>(grade.value + 1u)}).cell_count() : 0;
            const auto slots = [capacity](auto rows) { return machine::checked_size_mul(rows, capacity); };
            down_.resize(slots(width)); up_.resize(slots(width)); middle_.resize(slots(width));
            lower_.resize(slots(below)); upper_.resize(slots(above)); scaled_.resize(slots(std::max(width, below)));
        }
        std::shared_ptr<const State> owner_;
        core::Grade grade_;
        std::size_t capacity_;
        numeric::RationalVector down_, up_, middle_, lower_, upper_, scaled_;
    };
    PreparedExactExecutionTower(std::shared_ptr<const core::RelationalComplex> source,
                                std::vector<ExactMetric> metrics,PreparedExactProfile profile=PreparedExactProfile::FiniteActions) {
        if (!source) throw std::invalid_argument("prepared tower requires an immutable source owner");
        if(profile!=PreparedExactProfile::FiniteActions && profile!=PreparedExactProfile::GlobalActions)
            throw std::invalid_argument("unknown prepared exact profile");
        for (const auto& metric:metrics)
            if (profile==PreparedExactProfile::FiniteActions && !metric.is_diagonal())
                throw std::invalid_argument("prepared finite tower requires unit/diagonal metrics");
        state_=std::make_shared<const State>(std::move(source),std::move(metrics),profile);
    }
    PreparedExactExecutionTower(const PreparedExactExecutionTower&)=default;
    PreparedExactExecutionTower& operator=(const PreparedExactExecutionTower&)=default;
    static void validate_request(const OperatorRequest& request,PreparedExactProfile profile=PreparedExactProfile::FiniteActions) {
        if(profile!=PreparedExactProfile::FiniteActions && profile!=PreparedExactProfile::GlobalActions)
            throw std::invalid_argument("unknown prepared exact profile");
        if(request.kind==OperatorKind::Green && profile==PreparedExactProfile::GlobalActions)return;
        if (request.lambda != 0)throw std::invalid_argument("operator has no lambda parameter");
        switch(request.kind) {
        case OperatorKind::Boundary: case OperatorKind::Adjoint:
        case OperatorKind::HodgeDown: case OperatorKind::HodgeUp: case OperatorKind::Hodge: return;
        case OperatorKind::HarmonicProjector: case OperatorKind::HarmonicInverse: case OperatorKind::GradientPotential:
            if(profile==PreparedExactProfile::GlobalActions)return;
            break;
        default: throw std::invalid_argument("operator outside prepared finite tower profile");
        }
        throw std::invalid_argument("operator outside prepared finite tower profile");
    }
    numeric::RationalVector apply(const OperatorRequest& request,const numeric::RationalVector& input) const {
        validate_request(request,state_->profile);
        const auto pin=state_;
        const auto& source=*pin->source;
        if (request.grade.value>=source.grade_count()) throw std::out_of_range("prepared tower grade");
        if (request.kind==OperatorKind::Boundary) {
            if (!request.grade.value) throw std::invalid_argument("grade zero has no boundary");
            return source.boundary_apply(request.grade,input);
        }
        if (request.kind==OperatorKind::Adjoint) {
            if (!request.grade.value) throw std::invalid_argument("grade zero has no boundary adjoint");
            return BoundaryOperator(source,request.grade).adjoint(input,
                pin->metrics.at(request.grade.value-1u),pin->metrics.at(request.grade.value));
        }
        const auto& hodge=*pin->hodges.at(request.grade.value);
        switch(request.kind) {
        case OperatorKind::HodgeDown: return hodge.apply_down(input);
        case OperatorKind::HodgeUp: return hodge.apply_up(input);
        case OperatorKind::Hodge: return hodge.apply(input);
        case OperatorKind::Green: {
            const auto result=augmented_green_apply_reference(source,request.grade,pin->metrics,request.lambda,input);
            if(!result.exact)throw std::runtime_error("prepared Green residual is nonzero");
            return result.value;
        }
        case OperatorKind::HarmonicProjector:
            return projector_apply(pin->harmonic_frames.at(request.grade.value),hodge.metric(),input);
        case OperatorKind::HarmonicInverse: case OperatorKind::GradientPotential:
            return prepare_action(request).apply(input);
        default: throw std::logic_error("validated exact operator");
        }
    }
    PreparedExactProfile profile() const noexcept {return state_->profile;}
    PreparedGlobalAction prepare_action(const OperatorRequest& request, numeric::ExactActionGuard& guard) const {
        const auto pin = state_;
        validate_request(request, pin->profile);
        if (pin->profile != PreparedExactProfile::GlobalActions ||
            (request.kind != OperatorKind::Green && request.kind != OperatorKind::HarmonicProjector &&
             request.kind != OperatorKind::HarmonicInverse && request.kind != OperatorKind::GradientPotential))
            throw std::invalid_argument("reusable global action requires the global exact profile");
        if (request.grade.value >= pin->source->grade_count()) throw std::out_of_range("prepared global action grade");
        return PreparedGlobalAction(pin->source, pin->metrics, request,
            pin->harmonic_frames.at(request.grade.value), guard);
    }
    PreparedGlobalAction prepare_action(const OperatorRequest& request, numeric::ExactActionLimits limits = {},
                                       std::function<void()> check = {}) const {
        numeric::ExactActionGuard guard(limits, std::move(check)); return prepare_action(request, guard);
    }
    std::size_t harmonic_frame_integer_bits() const noexcept {return state_->frame_bits;}
    const std::vector<ExactMetric>& metrics() const & {return state_->metrics;}
    const std::vector<ExactMetric>& metrics() const && = delete;
    std::shared_ptr<const core::RelationalComplex> source() const {return state_->source;}
    std::size_t batch_workspace_slots(core::Grade grade, std::size_t capacity) const {
        if (state_->profile != PreparedExactProfile::FiniteActions)
            throw std::invalid_argument("exact batch requires the finite action profile");
        if (!capacity || grade.value >= state_->source->grade_count())
            throw std::invalid_argument("exact batch workspace grade or capacity mismatch");
        const auto& source = *state_->source;
        const auto width = source.grade(grade).cell_count();
        const auto below = grade.value ? source.grade(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)}).cell_count() : 0;
        const auto above = grade.value < source.max_grade().value
            ? source.grade(core::Grade{static_cast<std::uint16_t>(grade.value + 1u)}).cell_count() : 0;
        auto rows = machine::checked_size_add(machine::checked_size_mul(width, 3), std::max(width, below));
        rows = machine::checked_size_add(rows, machine::checked_size_add(below, above));
        return machine::checked_size_mul(rows, capacity);
    }
    BatchWorkspace make_batch_workspace(core::Grade grade, std::size_t capacity) const {
        (void)batch_workspace_slots(grade, capacity);
        return BatchWorkspace(state_, grade, capacity);
    }
    void validate_batch_workspace(core::Grade grade, std::size_t samples, const BatchWorkspace& workspace) const {
        if (state_->profile != PreparedExactProfile::FiniteActions || workspace.owner_ != state_ ||
            workspace.grade_ != grade || !samples || samples > workspace.capacity_)
            throw std::invalid_argument("exact batch workspace owner, grade or capacity mismatch");
    }
    // Finite actions preserve the original occurrence order within each sample.
    // A caller owns the output and exclusive scratch for the complete call.
    void apply_batch_into(const OperatorRequest& request, std::span<const numeric::Rational> input,
                          std::span<numeric::Rational> output, std::size_t samples,
                          BatchWorkspace& workspace, const std::function<void()>& check = {}) const {
        validate_request(request, PreparedExactProfile::FiniteActions);
        validate_batch_workspace(request.grade, samples, workspace);
        const auto pin = state_;
        const auto& source = *pin->source;
        const auto grade = request.grade;
        const auto width = source.grade(grade).cell_count();
        const auto below = grade.value ? source.grade(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)}).cell_count() : 0;
        const auto above = grade.value < source.max_grade().value
            ? source.grade(core::Grade{static_cast<std::uint16_t>(grade.value + 1u)}).cell_count() : 0;
        if ((request.kind == OperatorKind::Boundary || request.kind == OperatorKind::Adjoint) && !grade.value)
            throw std::invalid_argument("grade zero has no boundary or adjoint");
        const auto count = [samples](auto rows) { return machine::checked_size_mul(rows, samples); };
        const auto rows_in = request.kind == OperatorKind::Adjoint ? below : width;
        const auto rows_out = request.kind == OperatorKind::Boundary ? below : width;
        if (input.size() != count(rows_in) || output.size() != count(rows_out))
            throw std::invalid_argument("exact batch input or output shape mismatch");
        const std::less<const numeric::Rational*> before;
        if (!input.empty() && !output.empty() && before(input.data(), output.data() + output.size()) &&
            before(output.data(), input.data() + input.size()))
            throw std::invalid_argument("exact batch requires disjoint input and output");
        const auto take = [&](auto& buffer, auto rows) { return std::span<numeric::Rational>(buffer).first(count(rows)); };
        auto down = take(workspace.down_, width), up = take(workspace.up_, width);
        auto middle = take(workspace.middle_, width), lower = take(workspace.lower_, below), upper = take(workspace.upper_, above);
        auto scaled = take(workspace.scaled_, std::max(width, below));
        const auto checkpoint = [&] { if (check) check(); };
        checkpoint();
        if (request.kind == OperatorKind::Boundary) {
            BoundaryOperator(source, grade).apply_batch_into(input, output, samples, scaled);
        } else if (request.kind == OperatorKind::Adjoint) {
            pin->metrics[grade.value - 1u].apply_batch_into(input, lower, samples);
            checkpoint();
            BoundaryOperator(source, grade).transpose_batch_into(lower, output, samples, scaled);
            pin->metrics[grade.value].solve_batch_into(output, output, samples);
        } else {
            const auto evaluate_down = [&] {
                std::fill(down.begin(), down.end(), numeric::Rational{0});
                if (!grade.value) return;
                const BoundaryOperator boundary(source, grade);
                boundary.apply_batch_into(input, lower, samples, scaled);
                pin->metrics[grade.value - 1u].apply_batch_into(lower, lower, samples);
                checkpoint();
                boundary.transpose_batch_into(lower, down, samples, scaled);
                pin->metrics[grade.value].solve_batch_into(down, down, samples);
                checkpoint();
            };
            const auto evaluate_up = [&] {
                std::fill(up.begin(), up.end(), numeric::Rational{0});
                if (grade.value == source.max_grade().value) return;
                const auto next = core::Grade{static_cast<std::uint16_t>(grade.value + 1u)};
                const BoundaryOperator boundary(source, next);
                pin->metrics[grade.value].apply_batch_into(input, middle, samples);
                boundary.transpose_batch_into(middle, upper, samples, scaled);
                pin->metrics[next.value].solve_batch_into(upper, upper, samples);
                checkpoint();
                boundary.apply_batch_into(upper, up, samples, scaled);
                checkpoint();
            };
            if (request.kind == OperatorKind::HodgeDown || request.kind == OperatorKind::Hodge) evaluate_down();
            if (request.kind == OperatorKind::HodgeUp || request.kind == OperatorKind::Hodge) evaluate_up();
            for (std::size_t i = 0; i < output.size(); ++i) {
                if (request.kind == OperatorKind::HodgeDown) output[i] = down[i];
                else if (request.kind == OperatorKind::HodgeUp) output[i] = up[i];
                else output[i] = down[i] + up[i];
            }
        }
        checkpoint();
    }
private:
    struct State {
        std::shared_ptr<const core::RelationalComplex> source;
        std::vector<ExactMetric> metrics;
        std::vector<std::unique_ptr<const ExactHodgeOperator>> hodges;
        PreparedExactProfile profile;
        std::vector<numeric::ExactMatrix> harmonic_frames;
        std::size_t frame_bits{1};
        State(std::shared_ptr<const core::RelationalComplex> s,std::vector<ExactMetric> m,PreparedExactProfile p)
            :source(std::move(s)),metrics(std::move(m)),profile(p) {
            if (metrics.size()!=source->grade_count()) throw std::invalid_argument("prepared tower metric count");
            hodges.reserve(metrics.size());
            for(std::size_t k=0;k<metrics.size();++k)
                hodges.push_back(std::make_unique<const ExactHodgeOperator>(*source,
                    core::Grade{static_cast<std::uint16_t>(k)},metrics));
            if(profile==PreparedExactProfile::GlobalActions) {
                harmonic_frames.reserve(metrics.size());
                for(const auto& hodge:hodges) {
                    harmonic_frames.push_back(hodge->harmonic_frame());
                    for(const auto& [position,value]:harmonic_frames.back().entries()) {
                        (void)position;
                        frame_bits=std::max({frame_bits,mpz_sizeinbase(mpq_numref(value.get_mpq_t()),2),
                            mpz_sizeinbase(mpq_denref(value.get_mpq_t()),2)});
                    }
                }
            }
        }
        State(const State&)=delete;
        State& operator=(const State&)=delete;
    };
    std::shared_ptr<const State> state_;
};

} // namespace rfc::field
