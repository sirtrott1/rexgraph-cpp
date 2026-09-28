#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/topology/integral_homology.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::topology {

using ModularVector = std::vector<std::uint64_t>;

inline std::uint64_t multiply_mod(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t modulus) {
    std::uint64_t result = 0;
    lhs %= modulus;
    rhs %= modulus;
    while (rhs != 0) {
        if ((rhs & 1u) != 0u) result = (result >= modulus - lhs) ? result - (modulus - lhs) : result + lhs;
        rhs >>= 1u;
        if (rhs != 0) lhs = (lhs >= modulus - lhs) ? lhs - (modulus - lhs) : lhs + lhs;
    }
    return result;
}

inline std::uint64_t reduce_integer(const numeric::Integer& value, std::uint64_t modulus) {
    if (modulus < 2) throw std::invalid_argument("modulus must be >= 2");
    return mpz_fdiv_ui(value.get_mpz_t(), static_cast<unsigned long>(modulus));
}

inline ModularVector lattice_boundary_apply(const core::RelationalComplex& complex,
                                             core::Grade upper_grade,
                                             const ModularVector& field,
                                             std::uint64_t modulus) {
    if (upper_grade.value == 0 || upper_grade.value >= complex.grade_count()) throw std::out_of_range("invalid upper grade");
    const auto& upper = complex.grade(upper_grade);
    const auto& lower = complex.grade(core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)});
    if (field.size() != upper.cell_count()) throw std::invalid_argument("modular field size mismatch");
    ModularVector result(lower.cell_count(), 0u);
    const auto exists_id = complex.signature().existence_aspect();
    for (std::size_t cell = 0; cell < upper.cell_count(); ++cell) {
        const auto value = field[cell] % modulus;
        if (value == 0) continue;
        const auto [begin, end] = upper.incidence_range(cell);
        for (std::size_t p = begin; p < end; ++p) {
            if (!upper.aspects.get(exists_id, p)) continue;
            const auto coefficient = reduce_integer(upper.lattice_coefficient[p], modulus);
            const auto contribution = multiply_mod(coefficient, value, modulus);
            result[upper.lower_index[p]] = (result[upper.lower_index[p]] + contribution) % modulus;
        }
    }
    return result;
}

} // namespace rfc::topology

namespace rfc::topology {

inline std::uint64_t power_mod(std::uint64_t base, std::uint64_t exponent, std::uint64_t modulus) {
    if (modulus < 2) throw std::invalid_argument("modulus must be >= 2");
    std::uint64_t result = 1u % modulus;
    base %= modulus;
    while (exponent != 0) {
        if ((exponent & 1u) != 0u) result = multiply_mod(result,base,modulus);
        exponent >>= 1u;
        if (exponent != 0) base = multiply_mod(base,base,modulus);
    }
    return result;
}

inline std::uint64_t inverse_mod_prime(std::uint64_t value, std::uint64_t prime) {
    value %= prime;
    if (value == 0) throw std::invalid_argument("zero has no modular inverse");
    // Fermat inversion: this API is deliberately named for prime moduli.
    const auto inverse = power_mod(value,prime-2u,prime);
    if (multiply_mod(value,inverse,prime) != 1u) throw std::invalid_argument("modulus is not a field for this pivot");
    return inverse;
}

inline std::size_t rank_mod_prime(const numeric::IntegerMatrix& matrix, std::uint64_t prime) {
    if (prime < 2u) throw std::invalid_argument("prime modulus must be >= 2");
    std::vector<std::vector<std::uint64_t>> a(matrix.rows(),std::vector<std::uint64_t>(matrix.cols(),0u));
    for (std::size_t i=0;i<matrix.rows();++i)
        for(std::size_t j=0;j<matrix.cols();++j)
            a[i][j]=reduce_integer(matrix.get(i,j),prime);
    std::size_t row=0;
    for(std::size_t col=0; col<matrix.cols() && row<matrix.rows(); ++col) {
        std::size_t pivot=row;
        while(pivot<matrix.rows() && a[pivot][col]==0u) ++pivot;
        if(pivot==matrix.rows()) continue;
        if(pivot!=row) std::swap(a[pivot],a[row]);
        const auto inv=inverse_mod_prime(a[row][col],prime);
        for(std::size_t j=col;j<matrix.cols();++j) a[row][j]=multiply_mod(a[row][j],inv,prime);
        for(std::size_t i=0;i<matrix.rows();++i) {
            if(i==row || a[i][col]==0u) continue;
            const auto factor=a[i][col];
            for(std::size_t j=col;j<matrix.cols();++j) {
                const auto product=multiply_mod(factor,a[row][j],prime);
                a[i][j]=(a[i][j]+prime-product)%prime;
            }
        }
        ++row;
    }
    return row;
}

struct ModularHomologyDimension {
    core::Grade grade{};
    std::uint64_t prime{};
    std::size_t dimension{};
};

inline ModularHomologyDimension modular_homology_dimension(const core::RelationalComplex& complex,
                                                           core::Grade grade,
                                                           std::uint64_t prime) {
    if(grade.value>=complex.grade_count()) throw std::out_of_range("modular homology grade");
    const auto dk=integral_boundary_matrix(complex,grade);
    numeric::IntegerMatrix dkp1(complex.grade(grade).cell_count(),0u);
    if(static_cast<std::size_t>(grade.value)+1u<complex.grade_count())
        dkp1=integral_boundary_matrix(complex,core::Grade{static_cast<std::uint16_t>(grade.value+1u)});
    const auto rk=rank_mod_prime(dk,prime);
    const auto rkp1=rank_mod_prime(dkp1,prime);
    const auto n=complex.grade(grade).cell_count();
    if(rk+rkp1>n) throw std::logic_error("modular chain ranks violate im subset ker");
    return ModularHomologyDimension{grade,prime,n-rk-rkp1};
}

} // namespace rfc::topology
