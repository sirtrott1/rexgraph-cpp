#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <stdexcept>

namespace rfc::field {

inline numeric::ExactMatrix solve_columns_exact(const numeric::ExactMatrix& A,
                                                const numeric::ExactMatrix& B) {
    if (A.rows() != A.cols() || A.rows() != B.rows()) throw std::invalid_argument("column solve dimension mismatch");
    numeric::ExactMatrix out(A.cols(), B.cols());
    for (std::size_t j = 0; j < B.cols(); ++j) {
        numeric::RationalVector rhs(B.rows(), numeric::Rational{0});
        for (std::size_t i = 0; i < B.rows(); ++i) rhs[i] = B.get(i,j);
        const auto x = A.solve(rhs);
        for (std::size_t i = 0; i < x.size(); ++i) if (x[i] != 0) out.set(i,j,x[i]);
    }
    return out;
}

inline numeric::RationalVector woodbury_solve(const numeric::ExactMatrix& A,
                                              const numeric::ExactMatrix& U,
                                              const numeric::ExactMatrix& C,
                                              const numeric::ExactMatrix& V,
                                              const numeric::RationalVector& rhs) {
    if (A.rows() != A.cols() || rhs.size() != A.rows() || U.rows() != A.rows() || V.rows() != A.rows() ||
        U.cols() != C.rows() || C.rows() != C.cols() || C.cols() != V.cols()) {
        throw std::invalid_argument("Woodbury dimension mismatch");
    }
    const auto x = A.solve(rhs);
    const auto AU = solve_columns_exact(A, U);
    const auto Cinv = solve_columns_exact(C, numeric::ExactMatrix::identity(C.rows()));
    const auto middle = Cinv.add(V.transpose().multiply(AU));
    const auto vx = V.transpose().apply(x);
    const auto correction_coeff = middle.solve(vx);
    return numeric::sub(x, AU.apply(correction_coeff));
}

inline numeric::ExactMatrix low_rank_updated_matrix(const numeric::ExactMatrix& A,
                                                    const numeric::ExactMatrix& U,
                                                    const numeric::ExactMatrix& C,
                                                    const numeric::ExactMatrix& V) {
    return A.add(U.multiply(C).multiply(V.transpose()));
}

} // namespace rfc::field
