#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/integer_matrix.hpp"
#include "rfc/numeric/smith_normal_form.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::topology {

inline numeric::IntegerMatrix integral_boundary_matrix(const core::RelationalComplex& complex,
                                                       core::Grade upper_grade) {
    if (upper_grade.value == 0) {
        return numeric::IntegerMatrix(0u, complex.grade(core::Grade{0}).cell_count());
    }
    if (upper_grade.value >= complex.grade_count()) throw std::out_of_range("integral boundary grade");
    const auto& upper = complex.grade(upper_grade);
    const auto& lower = complex.grade(core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)});
    numeric::IntegerMatrix out(lower.cell_count(), upper.cell_count());
    const auto exists_id = complex.signature().existence_aspect();
    for (std::size_t slot = 0; slot < upper.incidence_count(); ++slot) {
        if (!upper.aspects.get(exists_id, slot)) continue;
        const auto row = upper.lower_index[slot], col = upper.upper_index[slot];
        out.set(row, col, out.get(row, col) + upper.lattice_coefficient[slot]);
    }
    return out;
}

inline numeric::IntegerMatrix integer_identity(std::size_t n) {
    return numeric::IntegerMatrix::identity(n);
}

inline numeric::IntegerMatrix unimodular_inverse(const numeric::IntegerMatrix& matrix) {
    if (matrix.rows() != matrix.cols()) throw std::invalid_argument("unimodular inverse requires square matrix");
    const auto smith = numeric::smith_normal_form_actions(matrix);
    if (smith.rank != matrix.rows()) throw std::logic_error("matrix is not unimodular");
    for (const auto& invariant : smith.invariant_factors)
        if (invariant != 1) throw std::logic_error("matrix is not unimodular");
    // A complete inverse is explicitly requested here.  U*A*V=I gives
    // A^{-1}=V*U using integral elementary actions, with no rational solves.
    auto out = smith.right_inverse.apply_inverse(
        smith.left.apply(numeric::IntegerMatrix::identity(matrix.rows())));
    if (!(matrix * out == numeric::IntegerMatrix::identity(matrix.rows())) ||
        !(out * matrix == numeric::IntegerMatrix::identity(matrix.rows()))) {
        throw std::logic_error("unimodular inverse certificate failed");
    }
    return out;
}

inline numeric::IntegerMatrix select_columns(const numeric::IntegerMatrix& matrix,
                                             std::size_t begin_column,
                                             std::size_t count) {
    if (begin_column > matrix.cols() || count > matrix.cols() - begin_column) throw std::out_of_range("integer select columns");
    numeric::IntegerMatrix out(matrix.rows(), count);
    for (const auto& [key, value] : matrix.entries())
        if (key.second >= begin_column && key.second - begin_column < count && value != 0)
            out.set(key.first, key.second - begin_column, value);
    return out;
}

inline numeric::IntegerMatrix select_rows(const numeric::IntegerMatrix& matrix,
                                          std::size_t begin_row,
                                          std::size_t count) {
    if (begin_row > matrix.rows() || count > matrix.rows() - begin_row) throw std::out_of_range("integer select rows");
    numeric::IntegerMatrix out(count, matrix.cols());
    for (const auto& [key, value] : matrix.entries())
        if (key.first >= begin_row && key.first - begin_row < count && value != 0)
            out.set(key.first - begin_row, key.second, value);
    return out;
}


struct IntegralChainCertificate {
    core::Grade grade{};
    bool cycle{false};
    bool boundary{false};
    // When boundary is true, this is one exact integral (k+1)-chain y with
    // d_{k+1} y = x. Free Smith coordinates are chosen to be zero.
    std::vector<numeric::Integer> boundary_preimage;

    [[nodiscard]] bool represents_nonzero_homology_class() const noexcept {
        return cycle && !boundary;
    }
};

inline bool integer_vector_is_zero(const std::vector<numeric::Integer>& values) {
    for (const auto& value : values) if (value != 0) return false;
    return true;
}

// Exact membership certificate for one retained integral k-chain. This is
// independent of relation arity: a branching primary relation contributes one
// chain coordinate, and higher branching relations can make combinations of
// such coordinates exact boundaries without pairwise expansion.
inline IntegralChainCertificate integral_chain_certificate(
    const core::RelationalComplex& complex,
    core::Grade grade,
    const std::vector<numeric::Integer>& chain) {
    if (grade.value >= complex.grade_count()) throw std::out_of_range("integral chain grade");
    const auto n = complex.grade(grade).cell_count();
    if (chain.size() != n) throw std::invalid_argument("integral chain coordinate size mismatch");

    const auto dk = integral_boundary_matrix(complex, grade);
    const bool cycle = integer_vector_is_zero(dk.apply(chain));
    if (!cycle) return IntegralChainCertificate{grade, false, false, {}};

    if (static_cast<std::size_t>(grade.value) + 1u >= complex.grade_count()) {
        return IntegralChainCertificate{grade, true, integer_vector_is_zero(chain), {}};
    }

    const auto dkp1 = integral_boundary_matrix(
        complex, core::Grade{static_cast<std::uint16_t>(grade.value + 1u)});
    const auto snf = numeric::smith_normal_form_actions(dkp1);
    const auto transformed = snf.left.apply(chain);

    std::vector<numeric::Integer> smith_preimage(dkp1.cols(), numeric::Integer{0});
    bool boundary = true;
    for (std::size_t i = 0; i < transformed.size(); ++i) {
        if (i < snf.rank) {
            const auto d = snf.diagonal.get(i, i);
            if (d == 0 || transformed[i] % d != 0) {
                boundary = false;
                break;
            }
            smith_preimage[i] = transformed[i] / d;
        } else if (transformed[i] != 0) {
            boundary = false;
            break;
        }
    }
    if (!boundary) return IntegralChainCertificate{grade, true, false, {}};

    auto preimage = snf.right_inverse.apply_inverse(smith_preimage);
    if (dkp1.apply(preimage) != chain) {
        throw std::logic_error("integral boundary preimage certificate failed");
    }
    return IntegralChainCertificate{grade, true, true, std::move(preimage)};
}

struct IntegralHomologyGroup {
    core::Grade grade{};
    std::size_t chain_rank{};
    std::size_t cycle_rank{};
    std::size_t boundary_rank{};
    std::size_t free_rank{};
    std::vector<numeric::Integer> torsion_invariants;

    // Columns give an integral basis of ker(d_k) in the original C_k lattice.
    numeric::IntegerMatrix cycle_basis;
    // d_{k+1} expressed in coordinates of cycle_basis.
    numeric::IntegerMatrix boundaries_in_cycle_coordinates;
    numeric::SmithNormalForm boundary_smith;
};

inline IntegralHomologyGroup integral_homology(const core::RelationalComplex& complex,
                                               core::Grade grade) {
    if (grade.value >= complex.grade_count()) throw std::out_of_range("integral homology grade");
    const std::size_t n = complex.grade(grade).cell_count();
    const auto dk = integral_boundary_matrix(complex, grade);
    numeric::IntegerMatrix dkp1(n, 0u);
    if (static_cast<std::size_t>(grade.value) + 1u < complex.grade_count()) {
        dkp1 = integral_boundary_matrix(complex, core::Grade{static_cast<std::uint16_t>(grade.value + 1u)});
    }
    if (!(dk * dkp1 == numeric::IntegerMatrix(dk.rows(), dkp1.cols()))) {
        throw std::logic_error("integral homology requires an integral chain complex");
    }

    const auto dk_snf = numeric::smith_normal_form_actions(dk);
    const std::size_t kernel_rank = n - dk_snf.rank;
    const auto cycle_basis = dk_snf.right_columns(dk_snf.rank, kernel_rank);
    const auto dkp1_new = dk_snf.right_inverse.apply(dkp1);

    for (const auto& [key, value] : dkp1_new.entries())
        if (key.first < dk_snf.rank && value != 0) throw std::logic_error("boundary does not lie in integral cycle lattice");

    const auto boundary_coordinates = select_rows(dkp1_new, dk_snf.rank, kernel_rank);
    if (!(cycle_basis * boundary_coordinates == dkp1))
        throw std::logic_error("integral cycle-coordinate reconstruction certificate failed");
    const auto boundary_snf = numeric::smith_normal_form(boundary_coordinates);
    std::vector<numeric::Integer> torsion;
    for (const auto& d : boundary_snf.invariant_factors) if (d > 1) torsion.push_back(d);
    const std::size_t free_rank = kernel_rank - boundary_snf.rank;

    return IntegralHomologyGroup{grade, n, kernel_rank, boundary_snf.rank, free_rank,
                                 std::move(torsion), cycle_basis, boundary_coordinates, boundary_snf};
}

inline std::vector<IntegralHomologyGroup> integral_homology(const core::RelationalComplex& complex) {
    std::vector<IntegralHomologyGroup> groups;
    groups.reserve(complex.grade_count());
    for (std::size_t k = 0; k < complex.grade_count(); ++k)
        groups.push_back(integral_homology(complex, core::Grade{static_cast<std::uint16_t>(k)}));
    return groups;
}

} // namespace rfc::topology
