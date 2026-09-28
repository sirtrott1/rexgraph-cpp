#pragma once
#include "rfc/field/operator_contract.hpp"
#include "rfc/field/exact_dirac.hpp"
#include "rfc/field/augmented_green.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

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

} // namespace rfc::field
