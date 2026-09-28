#pragma once
#include "rfc/field/augmented_green.hpp"
#include <stdexcept>

namespace rfc::field {

struct ResolventIdentityCertificate {
    numeric::RationalVector left;
    numeric::RationalVector right;
    bool exact{};
};

inline ResolventIdentityCertificate exact_resolvent_identity(
    const ExactHodgeOperator& hodge,
    const numeric::Rational& lambda,
    const numeric::Rational& mu,
    const numeric::RationalVector& source) {
    if(lambda<=0 || mu<=0) throw std::invalid_argument("operator-first resolvent identity requires positive parameters");
    const auto Rl=exact_green_apply(hodge,lambda,source).value;
    const auto Rm=exact_green_apply(hodge,mu,source).value;
    const auto left=numeric::sub(Rl,Rm);
    const auto LRm=hodge.apply(Rm);
    const auto RlLRm=exact_green_apply(hodge,lambda,LRm).value;
    const auto right=numeric::scale(mu-lambda,RlLRm);
    return {left,right,left==right};
}

} // namespace rfc::field
