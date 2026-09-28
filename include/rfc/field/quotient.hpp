#pragma once
#include "rfc/field/exact_hodge.hpp"
#include <stdexcept>

namespace rfc::field {

struct FieldQuotient {
    numeric::Rational coefficient{0};
    numeric::Rational residual_quadrance{0};
};

inline FieldQuotient down_field_quotient(const ExactHodgeOperator& hodge,
                                         const numeric::RationalVector& x,
                                         const numeric::RationalVector& y) {
    if(x.size()!=hodge.size() || y.size()!=hodge.size()) throw std::invalid_argument("down field quotient size mismatch");
    const auto Ly=hodge.apply_down(y);
    const auto denominator=hodge.metric().inner(y,Ly);
    if(denominator<=0) throw std::invalid_argument("down field quotient requires positive denominator");
    const auto numerator=hodge.metric().inner(x,Ly);
    numeric::Rational coefficient=numerator/denominator; coefficient.canonicalize();
    const auto Lx=hodge.apply_down(x);
    numeric::Rational residual=hodge.metric().inner(x,Lx)-numerator*numerator/denominator; residual.canonicalize();
    return {coefficient,residual};
}

inline FieldQuotient up_field_quotient(const ExactHodgeOperator& hodge,
                                       const numeric::RationalVector& x,
                                       const numeric::RationalVector& y) {
    if(x.size()!=hodge.size() || y.size()!=hodge.size()) throw std::invalid_argument("up field quotient size mismatch");
    const auto Ly=hodge.apply_up(y);
    const auto denominator=hodge.metric().inner(y,Ly);
    if(denominator<=0) throw std::invalid_argument("up field quotient requires positive denominator");
    const auto numerator=hodge.metric().inner(x,Ly);
    numeric::Rational coefficient=numerator/denominator; coefficient.canonicalize();
    const auto Lx=hodge.apply_up(x);
    numeric::Rational residual=hodge.metric().inner(x,Lx)-numerator*numerator/denominator; residual.canonicalize();
    return {coefficient,residual};
}

} // namespace rfc::field
