#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <stdexcept>

namespace rfc::temporal {

struct CrossMomentChange {
    numeric::Rational pullback_form{0};
    numeric::Rational left_innovation{0};
    numeric::Rational right_innovation{0};
    numeric::Rational innovation_interaction{0};
    numeric::Rational total{0};
};

inline CrossMomentChange changing_dimension_cross_moment(
    const numeric::RationalVector& old_x,
    const numeric::RationalVector& old_y,
    const numeric::ExactMatrix& old_cross_form,
    const numeric::RationalVector& new_x,
    const numeric::RationalVector& new_y,
    const numeric::ExactMatrix& new_cross_form,
    const numeric::ExactMatrix& x_correspondence,
    const numeric::ExactMatrix& y_correspondence){
    if(old_cross_form.rows()!=old_x.size() || old_cross_form.cols()!=old_y.size() ||
       new_cross_form.rows()!=new_x.size() || new_cross_form.cols()!=new_y.size() ||
       x_correspondence.cols()!=old_x.size() || x_correspondence.rows()!=new_x.size() ||
       y_correspondence.cols()!=old_y.size() || y_correspondence.rows()!=new_y.size()) {
        throw std::invalid_argument("changing-dimension cross moment mismatch");
    }
    const auto Jx=x_correspondence.apply(old_x);
    const auto Jy=y_correspondence.apply(old_y);
    const auto eta_x=numeric::sub(new_x,Jx);
    const auto eta_y=numeric::sub(new_y,Jy);
    const auto pulled=x_correspondence.transpose().multiply(new_cross_form).multiply(y_correspondence)
        .add(old_cross_form.scale(numeric::Rational{-1}));
    numeric::Rational t0=numeric::dot(old_x,pulled.apply(old_y));
    numeric::Rational t1=numeric::dot(eta_x,new_cross_form.apply(Jy));
    numeric::Rational t2=numeric::dot(Jx,new_cross_form.apply(eta_y));
    numeric::Rational t3=numeric::dot(eta_x,new_cross_form.apply(eta_y));
    numeric::Rational total=t0+t1+t2+t3; total.canonicalize();
    return {t0,t1,t2,t3,total};
}

inline numeric::Rational finite_rate(const numeric::Rational& change,
                                     const numeric::Rational& interval){
    if(interval==0) throw std::invalid_argument("finite rate requires nonzero interval");
    numeric::Rational out=change/interval; out.canonicalize(); return out;
}

} // namespace rfc::temporal
