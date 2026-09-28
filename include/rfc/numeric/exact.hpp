#pragma once
#include <gmpxx.h>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace rfc::numeric {

using Integer = mpz_class;
using Rational = mpq_class;
using RationalVector = std::vector<Rational>;

inline Rational rational(long numerator) { return Rational(numerator); }
inline Rational rational(long numerator, long denominator) {
    if (denominator == 0) throw std::invalid_argument("zero denominator");
    Rational value(numerator, denominator);
    value.canonicalize();
    return value;
}
inline std::string str(const Rational& value) { return value.get_str(); }

inline Rational dot(const RationalVector& lhs, const RationalVector& rhs) {
    if (lhs.size() != rhs.size()) throw std::invalid_argument("dot size mismatch");
    Rational result = 0;
    for (std::size_t i = 0; i < lhs.size(); ++i) result += lhs[i] * rhs[i];
    return result;
}

inline RationalVector add(const RationalVector& lhs, const RationalVector& rhs) {
    if (lhs.size() != rhs.size()) throw std::invalid_argument("add size mismatch");
    RationalVector result(lhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) result[i] = lhs[i] + rhs[i];
    return result;
}

inline RationalVector sub(const RationalVector& lhs, const RationalVector& rhs) {
    if (lhs.size() != rhs.size()) throw std::invalid_argument("sub size mismatch");
    RationalVector result(lhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) result[i] = lhs[i] - rhs[i];
    return result;
}

inline RationalVector scale(const Rational& scalar, const RationalVector& value) {
    RationalVector result(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) result[i] = scalar * value[i];
    return result;
}

inline bool is_zero(const RationalVector& value) {
    for (const auto& scalar : value) if (scalar != 0) return false;
    return true;
}

inline Integer lcm_positive(const Integer& lhs, const Integer& rhs) {
    if (lhs == 0 || rhs == 0) return 0;
    Integer result;
    mpz_lcm(result.get_mpz_t(), lhs.get_mpz_t(), rhs.get_mpz_t());
    if (result < 0) result = -result;
    return result;
}

inline Integer denominator(const Rational& value) { return value.get_den(); }
inline Integer numerator(const Rational& value) { return value.get_num(); }

} // namespace rfc::numeric
