#pragma once
#include "rfc/field/exact_hodge.hpp"
#include "rfc/field/exact_operator.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::field {

// L_tau = (1+tau)L_down + (1-tau)L_up.
inline numeric::RationalVector oriented_hodge_apply(const ExactHodgeOperator& hodge,
                                                    const numeric::Rational& tau,
                                                    const numeric::RationalVector& x) {
    const auto down=hodge.apply_down(x);
    const auto up=hodge.apply_up(x);
    return numeric::add(numeric::scale(numeric::Rational{1}+tau,down),
                        numeric::scale(numeric::Rational{1}-tau,up));
}

inline ExactSpdSolveResult exact_oriented_stationary_field(
    const ExactHodgeOperator& hodge,
    const numeric::Rational& lambda,
    const numeric::Rational& tau,
    const numeric::RationalVector& source) {
    if(lambda<0 || tau<numeric::Rational{-1} || tau>numeric::Rational{1}) {
        throw std::invalid_argument("oriented stationary SPD path requires lambda>=0 and |tau|<=1");
    }
    return exact_metric_spd_solve(hodge.metric(),source,[&](const numeric::RationalVector& x){
        return numeric::add(x,numeric::scale(lambda,oriented_hodge_apply(hodge,tau,x)));
    });
}

inline numeric::Rational oriented_action_value(const ExactHodgeOperator& hodge,
                                               const numeric::Rational& lambda,
                                               const numeric::Rational& tau,
                                               const numeric::RationalVector& x,
                                               const numeric::RationalVector& source) {
    if(x.size()!=hodge.size() || source.size()!=hodge.size()) throw std::invalid_argument("oriented action size mismatch");
    const auto applied=numeric::add(x,numeric::scale(lambda,oriented_hodge_apply(hodge,tau,x)));
    numeric::Rational value=hodge.metric().inner(x,applied)/2-hodge.metric().inner(source,x);
    value.canonicalize();
    return value;
}

inline numeric::Rational oriented_action_tau_derivative(const ExactHodgeOperator& hodge,
                                                        const numeric::Rational& lambda,
                                                        const numeric::RationalVector& x) {
    numeric::Rational value=lambda*hodge.metric().inner(x,numeric::sub(hodge.apply_down(x),hodge.apply_up(x)))/2;
    value.canonicalize();
    return value;
}

// Xi_k(W)=B_k W B_{k+1}.  The identity term in W-I cancels by the chain law,
// so no dense identity or difference matrix is constructed.
class ExactChainStrainOperator {
public:
    ExactChainStrainOperator(const core::RelationalComplex& complex,
                             core::Grade shared_grade,
                             const std::vector<ExactMetric>& metrics,
                             numeric::ExactMatrix weight)
        : grade_(shared_grade), metrics_(&metrics), weight_(std::move(weight)),
          lower_(complex,shared_grade),
          upper_(complex,core::Grade{static_cast<std::uint16_t>(shared_grade.value+1u)}) {
        if(shared_grade.value==0 || shared_grade.value>=complex.max_grade().value) {
            throw std::invalid_argument("chain strain requires both adjacent boundaries");
        }
        if(metrics.size()!=complex.grade_count()) throw std::invalid_argument("chain strain metric count mismatch");
        const auto n=complex.grade(shared_grade).cell_count();
        if(weight_.rows()!=n || weight_.cols()!=n) throw std::invalid_argument("chain strain weight dimension mismatch");
    }
    [[nodiscard]] std::size_t domain_size() const noexcept { return upper_.domain_size(); }
    [[nodiscard]] std::size_t codomain_size() const noexcept { return lower_.codomain_size(); }
    [[nodiscard]] core::Grade shared_grade() const noexcept { return grade_; }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& z) const {
        return lower_.apply(weight_.apply(upper_.apply(z)));
    }

    [[nodiscard]] numeric::RationalVector adjoint_apply(const numeric::RationalVector& y) const {
        if(y.size()!=codomain_size()) throw std::invalid_argument("chain strain adjoint size mismatch");
        auto v=metric_adjoint_apply(lower_,metrics_->at(grade_.value),metrics_->at(grade_.value-1u),y);
        v=metrics_->at(grade_.value).solve(weight_.transpose().apply(metrics_->at(grade_.value).apply(v)));
        return metric_adjoint_apply(upper_,metrics_->at(grade_.value+1u),metrics_->at(grade_.value),v);
    }

    [[nodiscard]] numeric::RationalVector stiffness_apply(const numeric::RationalVector& z) const {
        return adjoint_apply(apply(z));
    }

    [[nodiscard]] numeric::Rational energy(const numeric::RationalVector& z) const {
        const auto value=apply(z);
        return metrics_->at(grade_.value-1u).inner(value,value);
    }
private:
    core::Grade grade_{};
    const std::vector<ExactMetric>* metrics_{};
    numeric::ExactMatrix weight_;
    BoundaryOperator lower_;
    BoundaryOperator upper_;
};

inline ExactSpdSolveResult exact_composite_stationary_field(
    const ExactHodgeOperator& hodge,
    const ExactChainStrainOperator& strain,
    const numeric::Rational& lambda,
    const numeric::Rational& tau,
    const numeric::Rational& gamma,
    const numeric::RationalVector& source) {
    if(strain.domain_size()!=hodge.size()) throw std::invalid_argument("composite action strain/Hodge grade mismatch");
    if(lambda<0 || gamma<0 || tau<numeric::Rational{-1} || tau>numeric::Rational{1}) {
        throw std::invalid_argument("composite stationary SPD path requires lambda,gamma>=0 and |tau|<=1");
    }
    return exact_metric_spd_solve(hodge.metric(),source,[&](const numeric::RationalVector& x){
        auto value=numeric::add(x,numeric::scale(lambda,oriented_hodge_apply(hodge,tau,x)));
        value=numeric::add(value,numeric::scale(gamma,strain.stiffness_apply(x)));
        return value;
    });
}

} // namespace rfc::field
