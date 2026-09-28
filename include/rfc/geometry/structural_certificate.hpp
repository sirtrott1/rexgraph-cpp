#pragma once
#include "rfc/core/axioms.hpp"
#include "rfc/core/attachment_update.hpp"
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/geometry/column_expansion.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::geometry {

// Exact algebraic certificate of one rational boundary realization B_k.
// The certificate is optional derived state: it never replaces retained source
// identity or occurrence data in RelationalComplex.
struct BoundaryStructureCertificate {
    core::Grade upper_grade{};
    std::size_t rows{};
    std::size_t cols{};
    std::size_t rank{};
    std::vector<std::size_t> pivot_columns;
    numeric::ExactMatrix image_frame;
    numeric::ExactMatrix kernel_frame;

    [[nodiscard]] std::size_t nullity() const noexcept { return cols - rank; }
};

struct GradeStructureSummary {
    core::Grade grade{};
    std::size_t cell_count{};
    std::size_t boundary_rank{};
    std::size_t upper_boundary_rank{};
    std::size_t closed_dimension{};
    std::size_t betti_number{};
};

inline numeric::RationalVector boundary_column(const core::RelationalComplex& complex,
                                                core::Grade upper_grade,
                                                std::size_t relation_index) {
    if (upper_grade.value == 0 || upper_grade.value >= complex.grade_count()) {
        throw std::out_of_range("boundary column requires positive retained grade");
    }
    const auto lower_grade = core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)};
    numeric::RationalVector out(complex.grade(lower_grade).cell_count(), numeric::Rational{0});
    const core::BoundaryOccurrenceFiber fiber(complex, upper_grade, relation_index);
    for (const auto& occurrence : fiber.occurrences()) {
        if (!occurrence.exists || occurrence.coefficient == 0) continue;
        out.at(occurrence.lower.index) += occurrence.coefficient;
        out.at(occurrence.lower.index).canonicalize();
    }
    return out;
}

inline numeric::ExactMatrix column_matrix(const numeric::RationalVector& column) {
    numeric::ExactMatrix out(column.size(), 1u);
    for (std::size_t i = 0; i < column.size(); ++i) if (column[i] != 0) out.set(i, 0u, column[i]);
    return out;
}

inline numeric::ExactMatrix extend_rows_with_zeros(const numeric::ExactMatrix& matrix,
                                                   std::size_t new_rows) {
    if (new_rows < matrix.rows()) throw std::invalid_argument("cannot shrink exact frame row count");
    numeric::ExactMatrix out(new_rows, matrix.cols());
    for (const auto& [key, value] : matrix.entries()) out.set(key.first, key.second, value);
    return out;
}

inline numeric::ExactMatrix extend_kernel_domain(const numeric::ExactMatrix& kernel,
                                                  std::size_t new_rows) {
    return extend_rows_with_zeros(kernel, new_rows);
}

// Coordinates of v in an independent frame F, or nullopt when v is outside
// span(F).  Only a rank(F)-square exact solve is required.
inline std::optional<numeric::RationalVector>
coordinates_in_independent_frame(const numeric::ExactMatrix& frame,
                                 const numeric::RationalVector& v) {
    if (frame.rows() != v.size()) throw std::invalid_argument("frame-coordinate dimension mismatch");
    if (frame.cols() == 0u) {
        for (const auto& value : v) if (value != 0) return std::nullopt;
        return numeric::RationalVector{};
    }

    std::vector<std::size_t> independent_rows;
    (void)frame.transpose().rref(independent_rows);
    if (independent_rows.size() != frame.cols()) throw std::invalid_argument("frame must have independent columns");

    numeric::ExactMatrix square(frame.cols(), frame.cols());
    numeric::RationalVector rhs(frame.cols(), numeric::Rational{0});
    for (std::size_t i = 0; i < independent_rows.size(); ++i) {
        const auto row = independent_rows[i];
        rhs[i] = v[row];
        for (std::size_t j = 0; j < frame.cols(); ++j) {
            const auto value = frame.get(row, j);
            if (value != 0) square.set(i, j, value);
        }
    }
    const auto coordinates = square.solve(rhs);
    if (frame.apply(coordinates) != v) return std::nullopt;
    return coordinates;
}

inline BoundaryStructureCertificate certify_boundary(const core::RelationalComplex& complex,
                                                       core::Grade upper_grade) {
    const auto boundary = exact_boundary_matrix(complex, upper_grade);
    std::vector<std::size_t> pivots;
    (void)boundary.rref(pivots);
    BoundaryStructureCertificate out;
    out.upper_grade = upper_grade;
    out.rows = boundary.rows();
    out.cols = boundary.cols();
    out.rank = pivots.size();
    out.pivot_columns = pivots;
    out.image_frame = boundary.select_columns(pivots);
    out.kernel_frame = boundary.kernel_frame();
    return out;
}

inline BoundaryStructureCertificate
append_boundary_column_incremental(const BoundaryStructureCertificate& previous,
                                   const numeric::RationalVector& new_column) {
    if (new_column.size() != previous.rows) throw std::invalid_argument("incremental boundary column row mismatch");
    if (previous.image_frame.rows() != previous.rows || previous.image_frame.cols() != previous.rank) {
        throw std::invalid_argument("invalid prior image certificate");
    }
    if (previous.kernel_frame.rows() != previous.cols || previous.kernel_frame.cols() != previous.nullity()) {
        throw std::invalid_argument("invalid prior kernel certificate");
    }

    BoundaryStructureCertificate out = previous;
    out.cols = previous.cols + 1u;
    const auto coordinates = coordinates_in_independent_frame(previous.image_frame, new_column);

    if (!coordinates) {
        out.rank = previous.rank + 1u;
        out.pivot_columns.push_back(previous.cols);
        out.image_frame = numeric::ExactMatrix::horizontal_concat(previous.image_frame, column_matrix(new_column));
        out.kernel_frame = extend_kernel_domain(previous.kernel_frame, out.cols);
        return out;
    }

    // z is already in im(B).  If F contains original pivot columns B e_p,
    // z = F c has the exact preimage sum_p c_p e_p.  Therefore
    // (-sum_p c_p e_p, 1) is the new kernel direction of [B z].
    numeric::ExactMatrix kernel(out.cols, previous.nullity() + 1u);
    for (const auto& [key, value] : previous.kernel_frame.entries()) kernel.set(key.first, key.second, value);
    const auto new_kernel_column = previous.nullity();
    for (std::size_t a = 0; a < coordinates->size(); ++a) {
        const auto coefficient = -coordinates->at(a);
        if (coefficient != 0) kernel.set(previous.pivot_columns.at(a), new_kernel_column, coefficient);
    }
    kernel.set(previous.cols, new_kernel_column, numeric::Rational{1});
    out.kernel_frame = std::move(kernel);
    return out;
}

struct IncrementalAttachmentCertificate;
class ChainStructureCertificate {
public:
    ChainStructureCertificate() = default;

    [[nodiscard]] static ChainStructureCertificate build(const core::RelationalComplex& complex) {
        ChainStructureCertificate out;
        out.cell_counts_.reserve(complex.grade_count());
        out.boundaries_.resize(complex.grade_count());
        for (std::size_t k = 0; k < complex.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            out.cell_counts_.push_back(complex.grade(grade).cell_count());
            if (k > 0) out.boundaries_[k] = certify_boundary(complex, grade);
        }
        out.bianchi_ = core::bianchi_through(complex, complex.max_grade());
        if (!out.bianchi_.satisfied()) throw std::invalid_argument("cannot certify a non-Bianchi canonical complex");
        out.refresh_grade_summaries();
        return out;
    }

    [[nodiscard]] std::size_t grade_count() const noexcept { return cell_counts_.size(); }
    [[nodiscard]] const std::vector<std::size_t>& cell_counts() const noexcept { return cell_counts_; }
    [[nodiscard]] const core::BianchiReport& bianchi() const noexcept { return bianchi_; }
    [[nodiscard]] bool bianchi_satisfied() const noexcept { return bianchi_.satisfied(); }

    [[nodiscard]] const BoundaryStructureCertificate& boundary(core::Grade upper_grade) const {
        if (upper_grade.value == 0 || upper_grade.value >= boundaries_.size()) throw std::out_of_range("boundary certificate grade");
        return boundaries_.at(upper_grade.value);
    }

    [[nodiscard]] const GradeStructureSummary& grade(core::Grade grade) const {
        return grades_.at(grade.value);
    }

    [[nodiscard]] std::size_t boundary_rank(core::Grade upper_grade) const {
        return upper_grade.value == 0 ? 0u : boundary(upper_grade).rank;
    }

    [[nodiscard]] std::size_t betti(core::Grade grade) const { return this->grade(grade).betti_number; }
    [[nodiscard]] std::size_t hodge_zero_multiplicity(core::Grade grade) const { return betti(grade); }

    [[nodiscard]] bool matches(const core::RelationalComplex& complex) const noexcept {
        if (complex.grade_count() != cell_counts_.size()) return false;
        for (std::size_t k = 0; k < cell_counts_.size(); ++k) {
            if (complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count() != cell_counts_[k]) return false;
        }
        return true;
    }

private:
    std::vector<std::size_t> cell_counts_;
    std::vector<BoundaryStructureCertificate> boundaries_;
    std::vector<GradeStructureSummary> grades_;
    core::BianchiReport bianchi_;

    void refresh_grade_summaries() {
        grades_.clear();
        grades_.reserve(cell_counts_.size());
        for (std::size_t k = 0; k < cell_counts_.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto down = k == 0 ? 0u : boundaries_.at(k).rank;
            const auto up = k + 1u < boundaries_.size() ? boundaries_.at(k + 1u).rank : 0u;
            if (down + up > cell_counts_[k]) throw std::logic_error("invalid structural rank certificate");
            grades_.push_back(GradeStructureSummary{
                grade,
                cell_counts_[k],
                down,
                up,
                cell_counts_[k] - down,
                cell_counts_[k] - down - up});
        }
    }

    friend struct IncrementalAttachmentCertificate;
    friend IncrementalAttachmentCertificate update_certificate_after_attachment(
        const ChainStructureCertificate&, const core::RelationalComplex&, const core::RelationalComplex&, core::Grade);
};

struct StructuralAttachmentDelta {
    core::Grade attached_grade{};
    std::size_t boundary_rank_gain{};
    std::ptrdiff_t lower_betti_change{};
    std::ptrdiff_t attached_betti_change{};
    numeric::RationalVector boundary_column;

    [[nodiscard]] bool creates_new_lower_filling_direction() const noexcept { return boundary_rank_gain != 0u; }
    [[nodiscard]] bool creates_new_attached_homology_direction() const noexcept { return attached_betti_change > 0; }
};

struct IncrementalAttachmentCertificate {
    ChainStructureCertificate certificate;
    StructuralAttachmentDelta delta;
};

inline IncrementalAttachmentCertificate update_certificate_after_attachment(
    const ChainStructureCertificate& previous,
    const core::RelationalComplex& source,
    const core::RelationalComplex& updated,
    core::Grade attached_grade) {
    if (!previous.matches(source)) throw std::invalid_argument("structural certificate does not match source complex");
    if (!previous.bianchi_satisfied()) throw std::invalid_argument("incremental attachment requires a valid source certificate");
    if (attached_grade.value < 2 || attached_grade.value >= updated.grade_count()) {
        throw std::invalid_argument("incremental certificate update requires an attached higher grade");
    }
    const auto updated_count = updated.grade(attached_grade).cell_count();
    const auto source_count = static_cast<std::size_t>(attached_grade.value) < source.grade_count()
        ? source.grade(attached_grade).cell_count() : 0u;
    if (updated_count != source_count + 1u) {
        throw std::invalid_argument("incremental certificate update expects exactly one attached primary relation");
    }

    ChainStructureCertificate next = previous;
    if (updated.grade_count() == previous.grade_count() + 1u) {
        next.cell_counts_.push_back(0u);
        next.boundaries_.resize(updated.grade_count());
    } else if (updated.grade_count() != previous.grade_count()) {
        throw std::invalid_argument("unexpected grade-count change during attachment");
    }

    const auto g = static_cast<std::size_t>(attached_grade.value);
    const auto relation_index = updated.grade(attached_grade).cell_count() - 1u;
    const auto z = boundary_column(updated, attached_grade, relation_index);

    BoundaryStructureCertificate old_boundary;
    if (g < previous.boundaries_.size() && previous.cell_counts_.size() > g) {
        old_boundary = previous.boundaries_.at(g);
    } else {
        const auto lower_count = updated.grade(core::Grade{static_cast<std::uint16_t>(g - 1u)}).cell_count();
        old_boundary.upper_grade = attached_grade;
        old_boundary.rows = lower_count;
        old_boundary.cols = 0u;
        old_boundary.rank = 0u;
        old_boundary.image_frame = numeric::ExactMatrix(lower_count, 0u);
        old_boundary.kernel_frame = numeric::ExactMatrix(0u, 0u);
    }

    auto new_boundary = append_boundary_column_incremental(old_boundary, z);
    const auto rank_gain = new_boundary.rank - old_boundary.rank;
    next.boundaries_.at(g) = std::move(new_boundary);
    next.cell_counts_.at(g) = updated.grade(attached_grade).cell_count();

    // Existing B_{g+1} gains one all-zero codomain row because no old coface
    // can reference the newly asserted relation.  Its rank and kernel therefore
    // remain exact invariants; only the image frame's ambient row count changes.
    if (g + 1u < next.boundaries_.size() && g + 1u < previous.boundaries_.size()) {
        auto& upper = next.boundaries_[g + 1u];
        upper.rows = next.cell_counts_.at(g);
        upper.image_frame = extend_rows_with_zeros(upper.image_frame, upper.rows);
    }

    next.bianchi_ = core::canonical_bianchi_certificate(updated);
    next.refresh_grade_summaries();
    if (!next.matches(updated)) throw std::logic_error("incremental structural certificate dimension mismatch");

    const auto lower_grade = core::Grade{static_cast<std::uint16_t>(g - 1u)};
    const auto old_lower_betti = static_cast<std::ptrdiff_t>(previous.betti(lower_grade));
    const auto new_lower_betti = static_cast<std::ptrdiff_t>(next.betti(lower_grade));
    const auto old_attached_betti = g < previous.grade_count() ? static_cast<std::ptrdiff_t>(previous.betti(attached_grade)) : 0;
    const auto new_attached_betti = static_cast<std::ptrdiff_t>(next.betti(attached_grade));

    return IncrementalAttachmentCertificate{
        std::move(next),
        StructuralAttachmentDelta{
            attached_grade,
            rank_gain,
            new_lower_betti - old_lower_betti,
            new_attached_betti - old_attached_betti,
            z}};
}

struct CertifiedHigherAttachmentResult {
    core::RelationalComplex complex;
    core::BianchiReport bianchi;
    ChainStructureCertificate structural;
    StructuralAttachmentDelta delta;
};

inline CertifiedHigherAttachmentResult append_higher_attachment_certified(
    const core::RelationalComplex& source,
    const ChainStructureCertificate& source_certificate,
    core::Grade grade,
    core::RelationTypeId type,
    const std::vector<core::HigherIncidenceInput>& incidences) {
    auto attachment = core::append_higher_attachment(source, grade, type, incidences);
    auto update = update_certificate_after_attachment(source_certificate, source, attachment.complex, grade);
    return {std::move(attachment.complex), std::move(attachment.bianchi),
            std::move(update.certificate), std::move(update.delta)};
}

} // namespace rfc::geometry
