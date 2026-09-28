#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>

namespace rfc::statistics {

struct EffectiveDimension2 {
    numeric::Rational variance{0};
    numeric::Rational second_contraction{0};
    numeric::Rational dimension{0};
    numeric::Rational collision_probability{0};
};

inline numeric::Rational trace_product(const numeric::ExactMatrix& lhs,
                                       const numeric::ExactMatrix& rhs) {
    if (lhs.cols()!=rhs.rows() || lhs.rows()!=rhs.cols()) throw std::invalid_argument("exact trace product shape mismatch");
    numeric::Rational result=0;
    for(const auto& [key,value]:lhs.entries()) {
        const auto other=rhs.get(key.second,key.first);
        if(other!=0) result+=value*other;
    }
    return result;
}

inline EffectiveDimension2 effective_dimension_2(const numeric::ExactMatrix& metric,
                                                  const numeric::ExactMatrix& covariance) {
    if(metric.rows()!=metric.cols() || covariance.rows()!=covariance.cols() || metric.rows()!=covariance.rows()) {
        throw std::invalid_argument("exact effective dimension shape mismatch");
    }
    const numeric::Rational variance=trace_product(metric,covariance);
    const auto gc=metric.multiply(covariance);
    const numeric::Rational second=trace_product(gc,gc);
    if(variance==0 || second==0) throw std::invalid_argument("effective dimension requires nonzero covariance");
    numeric::Rational dimension=variance*variance/second; dimension.canonicalize();
    numeric::Rational collision=numeric::Rational{1}/dimension; collision.canonicalize();
    return {variance,second,dimension,collision};
}

inline numeric::Rational cross_sector_coupling(const numeric::ExactMatrix& metric_s,
                                               const numeric::ExactMatrix& sigma_ss,
                                               const numeric::ExactMatrix& sigma_st,
                                               const numeric::ExactMatrix& metric_t,
                                               const numeric::ExactMatrix& sigma_tt,
                                               const numeric::ExactMatrix& sigma_ts) {
    if(metric_s.rows()!=metric_s.cols() || metric_t.rows()!=metric_t.cols() ||
       sigma_ss.rows()!=metric_s.rows() || sigma_ss.cols()!=metric_s.cols() ||
       sigma_tt.rows()!=metric_t.rows() || sigma_tt.cols()!=metric_t.cols() ||
       sigma_st.rows()!=metric_s.rows() || sigma_st.cols()!=metric_t.rows() ||
       sigma_ts.rows()!=metric_t.rows() || sigma_ts.cols()!=metric_s.rows()) {
        throw std::invalid_argument("cross-sector coupling shape mismatch");
    }
    const auto vs=trace_product(metric_s,sigma_ss);
    const auto vt=trace_product(metric_t,sigma_tt);
    if(vs<=0 || vt<=0) throw std::invalid_argument("cross-sector coupling requires positive sector variances");
    const auto product=metric_s.multiply(sigma_st).multiply(metric_t).multiply(sigma_ts);
    numeric::Rational numerator=0;
    for(std::size_t i=0;i<product.rows();++i) numerator+=product.get(i,i);
    numeric::Rational result=numerator/(vs*vt); result.canonicalize();
    return result;
}

} // namespace rfc::statistics
