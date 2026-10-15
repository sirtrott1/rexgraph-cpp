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
    numeric::ExactMatrix result(boundary.cols(),boundary.rows());
    const auto boundary_transpose=boundary.transpose(),delta_transpose=boundary_delta.transpose();
    // Apply the exact product rule columnwise using the existing metric solve.
    // Certified external metric actions need no coordinate materializer.
    for(std::size_t j=0;j<boundary.rows();++j){
        if(target_metric.supports_sparse_apply() && source_metric.supports_sparse_solve()){
            const auto basis=numeric::SparseRationalVector::basis(boundary.rows(),j);
            const auto measured=target_metric.apply(basis.view()),dual=boundary_transpose.apply_sparse(measured.view());
            const auto adjoint=source_metric.solve(dual.view());
            const auto first=delta_transpose.apply_sparse(measured.view()),second=source_metric_delta.apply_sparse(adjoint.view());
            const auto target_change=target_metric_delta.apply_sparse(basis.view()),third=boundary_transpose.apply_sparse(target_change.view());
            auto entries=first.entries();
            for(const auto& entry:second.entries())entries.push_back({entry.index,-entry.value});
            for(const auto& entry:third.entries())entries.push_back(entry);
            const numeric::SparseRationalVector rhs(boundary.cols(),std::move(entries));
            const auto column=source_metric.solve(rhs.view());
            for(const auto& entry:column.entries())result.set(entry.index,j,entry.value);
        }else{
            // This branch preserves explicitly declared external full-vector
            // callbacks; all owned metrics use the sparse action above.
            numeric::RationalVector basis(boundary.rows());basis[j]=1;
            const auto measured=target_metric.apply(basis);
            const auto adjoint=source_metric.solve(boundary_transpose.apply(measured));
            const auto rhs=numeric::add(numeric::sub(delta_transpose.apply(measured),
                source_metric_delta.apply(adjoint)),boundary_transpose.apply(target_metric_delta.apply(basis)));
            const auto column=source_metric.solve(rhs);
            for(std::size_t i=0;i<column.size();++i)result.set(i,j,column[i]);
        }
    }
    return result;
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
