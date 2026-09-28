#pragma once
#include "rfc/numeric/integer_matrix.hpp"
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace rfc::numeric {

struct SmithNormalForm {
    IntegerMatrix diagonal;
    IntegerMatrix left;
    IntegerMatrix right;
    std::size_t rank{};
    std::vector<Integer> invariant_factors;
};

namespace detail {

inline void row_swap(IntegerMatrix& a, IntegerMatrix& u, std::size_t i, std::size_t j) {
    a.swap_rows(i,j); u.swap_rows(i,j);
}
inline void col_swap(IntegerMatrix& a, IntegerMatrix& v, std::size_t i, std::size_t j) {
    a.swap_cols(i,j); v.swap_cols(i,j);
}
inline void row_add(IntegerMatrix& a, IntegerMatrix& u, std::size_t target, std::size_t source, const Integer& q) {
    a.row_add_multiple(target, source, q); u.row_add_multiple(target, source, q);
}
inline void col_add(IntegerMatrix& a, IntegerMatrix& v, std::size_t target, std::size_t source, const Integer& q) {
    a.col_add_multiple(target, source, q); v.col_add_multiple(target, source, q);
}
inline void row_scale(IntegerMatrix& a, IntegerMatrix& u, std::size_t row, const Integer& q) {
    a.row_scale(row,q); u.row_scale(row,q);
}

inline bool zero_out_pivot_row_col(IntegerMatrix& a, IntegerMatrix& u, IntegerMatrix& v, std::size_t k) {
    bool changed = false;
    // Euclidean row reductions on column k.
    for (std::size_t i = k + 1; i < a.rows(); ++i) {
        while (a.get(i,k) != 0) {
            if (a.get(k,k) == 0) { row_swap(a,u,k,i); changed = true; continue; }
            Integer q = a.get(i,k) / a.get(k,k);
            row_add(a,u,i,k,-q);
            changed = true;
            if (a.get(i,k) != 0 && abs_integer(a.get(i,k)) < abs_integer(a.get(k,k))) row_swap(a,u,i,k);
            else if (a.get(i,k) == 0) break;
        }
    }
    // Euclidean column reductions on row k.
    for (std::size_t j = k + 1; j < a.cols(); ++j) {
        while (a.get(k,j) != 0) {
            if (a.get(k,k) == 0) { col_swap(a,v,k,j); changed = true; continue; }
            Integer q = a.get(k,j) / a.get(k,k);
            col_add(a,v,j,k,-q);
            changed = true;
            if (a.get(k,j) != 0 && abs_integer(a.get(k,j)) < abs_integer(a.get(k,k))) col_swap(a,v,j,k);
            else if (a.get(k,j) == 0) break;
        }
    }
    return changed;
}

} // namespace detail

inline SmithNormalForm smith_normal_form(const IntegerMatrix& input) {
    IntegerMatrix a = input;
    IntegerMatrix u = IntegerMatrix::identity(input.rows());
    IntegerMatrix v = IntegerMatrix::identity(input.cols());
    const std::size_t limit = std::min(input.rows(), input.cols());

    std::size_t k = 0;
    while (k < limit) {
        // Bring a nonzero entry of the remaining submatrix into the pivot position.
        std::size_t pi = input.rows(), pj = input.cols();
        Integer best = 0;
        for (std::size_t i = k; i < a.rows(); ++i) {
            for (std::size_t j = k; j < a.cols(); ++j) {
                const auto value = a.get(i,j);
                if (value == 0) continue;
                const auto magnitude = abs_integer(value);
                if (best == 0 || magnitude < best) { best = magnitude; pi = i; pj = j; }
            }
        }
        if (best == 0) break;
        if (pi != k) detail::row_swap(a,u,k,pi);
        if (pj != k) detail::col_swap(a,v,k,pj);

        // Reduce the pivot to the gcd of the entire remaining block.  Whenever
        // an off-pivot entry is not divisible by the pivot, mix its row into
        // the pivot row and repeat Euclidean row/column reduction.
        while (true) {
            (void)detail::zero_out_pivot_row_col(a,u,v,k);
            if (a.get(k,k) == 0) throw std::logic_error("smith pivot unexpectedly zero");

            bool divides = true;
            std::size_t oi = 0, oj = 0;
            const Integer pivot = abs_integer(a.get(k,k));
            for (std::size_t i = k + 1; i < a.rows() && divides; ++i) {
                for (std::size_t j = k + 1; j < a.cols(); ++j) {
                    if (a.get(i,j) != 0 && a.get(i,j) % pivot != 0) {
                        divides = false; oi = i; oj = j; break;
                    }
                }
            }
            if (divides) break;

            // Column k is zero below the pivot at this point. Adding row oi to
            // the pivot row preserves the pivot entry but exposes the offending
            // a[oi,oj] in the pivot row, so the following Euclidean column
            // reduction replaces the pivot by a proper gcd divisor.
            detail::row_add(a,u,k,oi,Integer{1});
            if (a.get(k,oj) == 0) throw std::logic_error("smith reduction failed to expose offending entry");
        }

        // Exact elimination is now possible because the pivot divides every
        // entry in the remaining block.
        const Integer d = a.get(k,k);
        for (std::size_t i = k + 1; i < a.rows(); ++i) {
            if (a.get(i,k) != 0) detail::row_add(a,u,i,k,-(a.get(i,k) / d));
        }
        for (std::size_t j = k + 1; j < a.cols(); ++j) {
            if (a.get(k,j) != 0) detail::col_add(a,v,j,k,-(a.get(k,j) / d));
        }
        if (a.get(k,k) < 0) detail::row_scale(a,u,k,Integer{-1});
        ++k;
    }

    std::vector<Integer> invariants;
    std::size_t rank = 0;
    for (std::size_t i = 0; i < limit; ++i) {
        const auto d = a.get(i,i);
        if (d == 0) continue;
        ++rank;
        invariants.push_back(abs_integer(d));
    }
    for (std::size_t i = 1; i < invariants.size(); ++i) {
        if (invariants[i] % invariants[i-1] != 0) throw std::logic_error("smith invariant divisibility failure");
    }
    if (!(u * input * v == a)) throw std::logic_error("smith factorization certificate failed");
    return SmithNormalForm{std::move(a), std::move(u), std::move(v), rank, std::move(invariants)};
}

} // namespace rfc::numeric
