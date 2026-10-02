#pragma once
#include "rfc/machine/checked_size.hpp"
#include <gmpxx.h>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace rfc::numeric {

using Integer = mpz_class;
using Rational = mpq_class;
using RationalVector = std::vector<Rational>;

// Read-only GNU MP physical accounting. This measures retained limb capacity,
// including capacity left after cancellation, without changing host allocators.
// It excludes allocator metadata and GMP's transient internal workspaces.
inline std::size_t integer_limb_bytes(mpz_srcptr value) {
    if (value->_mp_alloc < 0) throw std::invalid_argument("invalid GMP limb allocation");
    return machine::checked_size_mul(static_cast<std::size_t>(value->_mp_alloc), sizeof(mp_limb_t));
}
inline std::size_t integer_limb_bytes(const Integer& value) {return integer_limb_bytes(value.get_mpz_t());}
inline std::size_t rational_limb_bytes(const Rational& value) {
    return machine::checked_size_add(integer_limb_bytes(mpq_numref(value.get_mpq_t())),
                                    integer_limb_bytes(mpq_denref(value.get_mpq_t())));
}
inline std::size_t rational_limb_envelope(std::size_t integer_bits) {
    // Guard limbs cover retained capacity beyond the declared arithmetic bound.
    // Callers check the measured result against this envelope before publication.
    return machine::checked_size_mul(machine::checked_size_add(machine::ceil_div(integer_bits, GMP_NUMB_BITS),4),
                                     2*sizeof(mp_limb_t));
}
inline std::size_t rational_vector_object_bytes(const RationalVector& values) {
    auto bytes=machine::checked_size_add(sizeof(RationalVector),machine::checked_size_mul(values.capacity(),sizeof(Rational)));
    for(const auto& value:values)bytes=machine::checked_size_add(bytes,rational_limb_bytes(value));
    return bytes;
}

inline Rational canonical_rational(Rational value) {
    if (mpz_sgn(mpq_denref(value.get_mpq_t())) == 0)
        throw std::invalid_argument("zero rational denominator");
    value.canonicalize();
    return value;
}
inline RationalVector canonical_vector(const RationalVector& values) {
    RationalVector out;
    out.reserve(values.size());
    for (const auto& value : values) out.push_back(canonical_rational(value));
    return out;
}

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
