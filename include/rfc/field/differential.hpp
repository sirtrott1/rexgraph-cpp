#pragma once
#include "rfc/field/augmented_green.hpp"
#include "rfc/field/exact_hodge.hpp"
#include <stdexcept>

namespace rfc::field {

// Exact coordinate variation of B^dagger=M_source^{-1} B^T M_target.
inline numeric::ExactMatrix metric_adjoint_variation(
    const numeric::ExactMatrix& boundary,
    const ExactMetric& source_metric,
    const ExactMetric& target_metric,
    const numeric::ExactMatrix& boundary_delta,
    const numeric::ExactMatrix& source_metric_delta,
    const numeric::ExactMatrix& target_metric_delta) {
    if(boundary.rows()!=target_metric.size() || boundary.cols()!=source_metric.size() ||
       boundary_delta.rows()!=boundary.rows() || boundary_delta.cols()!=boundary.cols() ||
       source_metric_delta.rows()!=source_metric.size() || source_metric_delta.cols()!=source_metric.size() ||
       target_metric_delta.rows()!=target_metric.size() || target_metric_delta.cols()!=target_metric.size()) {
        throw std::invalid_argument("metric adjoint variation dimension mismatch");
    }
    const auto Msi=source_metric.materialize().inverse();
    const auto Bdag=Msi.multiply(boundary.transpose()).multiply(target_metric.materialize());
    const auto term1=Msi.multiply(source_metric_delta).multiply(Bdag).scale(numeric::Rational{-1});
    const auto term2=Msi.multiply(boundary_delta.transpose()).multiply(target_metric.materialize());
    const auto term3=Msi.multiply(boundary.transpose()).multiply(target_metric_delta);
    return term1.add(term2).add(term3);
}

struct GreenDifferential {
    numeric::RationalVector response;
    numeric::RationalVector differential;
};

// dg=G dj-G[(d lambda)L+lambda dL]g. dL is a declared exact coordinate
// variation; Green application itself stays operator-first.
inline GreenDifferential exact_green_response_differential(
    const ExactHodgeOperator& hodge,
    const numeric::Rational& lambda,
    const numeric::RationalVector& source,
    const numeric::RationalVector& source_delta,
    const numeric::Rational& lambda_delta,
    const numeric::ExactMatrix& hodge_delta) {
    if(source.size()!=hodge.size() || source_delta.size()!=hodge.size() ||
       hodge_delta.rows()!=hodge.size() || hodge_delta.cols()!=hodge.size()) {
        throw std::invalid_argument("Green differential dimension mismatch");
    }
    const auto g=exact_green_apply(hodge,lambda,source).value;
    const auto term=numeric::add(numeric::scale(lambda_delta,hodge.apply(g)),
                                 numeric::scale(lambda,hodge_delta.apply(g)));
    const auto rhs=numeric::sub(source_delta,term);
    const auto dg=exact_green_apply(hodge,lambda,rhs).value;
    return {g,dg};
}

} // namespace rfc::field
