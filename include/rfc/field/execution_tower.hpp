#pragma once
#include "rfc/field/operator_contract.hpp"
#include "rfc/field/exact_dirac.hpp"
#include "rfc/field/augmented_green.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
#include <memory>

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

// Owning finite-action profile. Unit/diagonal metrics avoid solver fill and
// keep boundary, coordinate transpose and metric adjoint as distinct actions.
class PreparedExactExecutionTower {
public:
    PreparedExactExecutionTower(std::shared_ptr<const core::RelationalComplex> source,
                                std::vector<ExactMetric> metrics) {
        if (!source) throw std::invalid_argument("prepared tower requires an immutable source owner");
        for (const auto& metric:metrics)
            if (!metric.is_diagonal()) throw std::invalid_argument("prepared finite tower requires unit/diagonal metrics");
        state_=std::make_shared<const State>(std::move(source),std::move(metrics));
    }
    PreparedExactExecutionTower(const PreparedExactExecutionTower&)=default;
    PreparedExactExecutionTower& operator=(const PreparedExactExecutionTower&)=default;
    static void validate_request(const OperatorRequest& request) {
        if (request.lambda != 0 || request.lambda.get_den()!=1)
            throw std::invalid_argument("finite tower has no lambda parameter");
        switch(request.kind) {
        case OperatorKind::Boundary: case OperatorKind::Adjoint:
        case OperatorKind::HodgeDown: case OperatorKind::HodgeUp: case OperatorKind::Hodge: return;
        default: throw std::invalid_argument("operator outside prepared finite tower profile");
        }
    }
    numeric::RationalVector apply(const OperatorRequest& request,const numeric::RationalVector& input) const {
        validate_request(request);
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
        default: throw std::logic_error("validated finite operator");
        }
    }
    const std::vector<ExactMetric>& metrics() const & {return state_->metrics;}
    const std::vector<ExactMetric>& metrics() const && = delete;
private:
    struct State {
        std::shared_ptr<const core::RelationalComplex> source;
        std::vector<ExactMetric> metrics;
        std::vector<std::unique_ptr<const ExactHodgeOperator>> hodges;
        State(std::shared_ptr<const core::RelationalComplex> s,std::vector<ExactMetric> m)
            :source(std::move(s)),metrics(std::move(m)) {
            if (metrics.size()!=source->grade_count()) throw std::invalid_argument("prepared tower metric count");
            hodges.reserve(metrics.size());
            for(std::size_t k=0;k<metrics.size();++k)
                hodges.push_back(std::make_unique<const ExactHodgeOperator>(*source,
                    core::Grade{static_cast<std::uint16_t>(k)},metrics));
        }
        State(const State&)=delete;
        State& operator=(const State&)=delete;
    };
    std::shared_ptr<const State> state_;
};

} // namespace rfc::field
