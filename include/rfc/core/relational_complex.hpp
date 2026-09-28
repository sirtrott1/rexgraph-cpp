#pragma once
#include "rfc/core/grade_arena.hpp"
#include "rfc/core/relation_signature.hpp"
#include "rfc/numeric/exact.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/numeric/work_counters.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::core {

class RelationalComplexBuilder;

class RelationalComplex {
public:
    [[nodiscard]] const RelationSignature& signature() const noexcept { return signature_; }
    [[nodiscard]] SemanticScale semantic_scale() const noexcept { return semantic_scale_; }
    [[nodiscard]] Grade max_grade() const noexcept { return Grade{static_cast<std::uint16_t>(grades_.size() - 1u)}; }
    [[nodiscard]] std::size_t grade_count() const noexcept { return grades_.size(); }
    [[nodiscard]] const GradeArena& grade(Grade grade) const { return grades_.at(grade.value); }

    [[nodiscard]] RelationIdentity relation_identity(RelationId id) const {
        if (id.grade.value == 0 || id.grade.value >= grades_.size()) throw std::out_of_range("relation identity grade");
        const auto& arena = grades_[id.grade.value];
        return RelationIdentity{id.grade, arena.relation_identities.at(id.index)};
    }

    [[nodiscard]] OccurrenceKey occurrence_identity(Grade upper_grade, std::size_t incidence) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("occurrence identity grade");
        return grades_[upper_grade.value].occurrence_identities.at(incidence);
    }

    [[nodiscard]] numeric::Rational incidence_coefficient(Grade upper_grade, std::size_t incidence) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("invalid upper grade");
        const auto& upper = grades_[upper_grade.value];
        const auto& lower = grades_[upper_grade.value - 1u];
        if (incidence >= upper.incidence_count()) throw std::out_of_range("incidence index");
        const auto lower_id = upper.lower_index[incidence];
        if (lower_id >= lower.cell_count()) throw std::logic_error("corrupt lower incidence index");
        const auto upper_cell = upper.upper_index.at(incidence);
        const auto& lower_scale = lower.cell_scales[lower_id];
        const auto& upper_scale = upper.cell_scales[upper_cell];
        numeric::Rational coefficient = lower_scale * upper.lattice_coefficient[incidence] / upper_scale;
        coefficient.canonicalize();
        return coefficient;
    }

    [[nodiscard]] numeric::RationalVector boundary_apply(Grade upper_grade,
                                                          const numeric::RationalVector& upper_field) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("invalid upper grade");
        const auto& upper = grades_[upper_grade.value];
        const auto& lower = grades_[upper_grade.value - 1u];
        if (upper_field.size() != upper.cell_count()) throw std::invalid_argument("boundary field size mismatch");

        // Execute the retained exact lattice factorization directly:
        //     B_k = S_{k-1} * B~_k * S_k^{-1}.
        // The upper cell scale is paid once per active source coordinate and
        // the lower scale once per output coordinate, rather than rebuilding
        // a normalized rational coefficient for every retained occurrence.
        numeric::RationalVector lattice_result(lower.cell_count(), numeric::Rational{0});
        const auto exists_id = signature_.existence_aspect();
        for (std::size_t cell = 0; cell < upper.cell_count(); ++cell) {
            if (upper_field[cell] == 0) continue;
            numeric::Rational scaled_source = upper_field[cell] / upper.cell_scales[cell];
            scaled_source.canonicalize();
            const auto [begin, end] = upper.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!upper.aspects.get(exists_id, p)) continue;
                lattice_result[upper.lower_index[p]] += scaled_source * upper.lattice_coefficient[p];
            }
        }
        for (std::size_t i = 0; i < lattice_result.size(); ++i) {
            if (lattice_result[i] == 0) continue;
            lattice_result[i] *= lower.cell_scales[i];
            lattice_result[i].canonicalize();
        }
        return lattice_result;
    }

    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse(
        Grade upper_grade,
        numeric::SparseRationalView upper_field) const {
        return boundary_apply_sparse_impl<false>(upper_grade, upper_field, nullptr);
    }

    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse(
        Grade upper_grade,
        const numeric::SparseRationalVector& upper_field) const {
        return boundary_apply_sparse(upper_grade, upper_field.view());
    }

    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse_counted(
        Grade upper_grade,
        numeric::SparseRationalView upper_field,
        numeric::WorkCounters& counters) const {
        return boundary_apply_sparse_impl<true>(upper_grade, upper_field, &counters);
    }

    [[nodiscard]] numeric::RationalVector transpose_apply(Grade upper_grade,
                                                           const numeric::RationalVector& lower_field) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("invalid upper grade");
        const auto& upper = grades_[upper_grade.value];
        const auto& lower = grades_[upper_grade.value - 1u];
        if (lower_field.size() != lower.cell_count()) throw std::invalid_argument("transpose field size mismatch");

        // B_k^T = S_k^{-1} * B~_k^T * S_{k-1}.
        numeric::RationalVector scaled_lower(lower.cell_count(), numeric::Rational{0});
        for (std::size_t i = 0; i < lower_field.size(); ++i) {
            if (lower_field[i] == 0) continue;
            scaled_lower[i] = lower_field[i] * lower.cell_scales[i];
        }

        numeric::RationalVector result(upper.cell_count(), numeric::Rational{0});
        const auto exists_id = signature_.existence_aspect();
        for (std::size_t cell = 0; cell < upper.cell_count(); ++cell) {
            numeric::Rational acc = 0;
            const auto [begin, end] = upper.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!upper.aspects.get(exists_id, p)) continue;
                acc += upper.lattice_coefficient[p] * scaled_lower[upper.lower_index[p]];
            }
            if (acc != 0) {
                acc /= upper.cell_scales[cell];
                acc.canonicalize();
            }
            result[cell] = std::move(acc);
        }
        return result;
    }

    [[nodiscard]] numeric::SparseRationalVector transpose_apply_sparse(
        Grade upper_grade,
        numeric::SparseRationalView lower_field) const {
        return transpose_apply_sparse_impl<false>(upper_grade, lower_field, nullptr);
    }

    [[nodiscard]] numeric::SparseRationalVector transpose_apply_sparse(
        Grade upper_grade,
        const numeric::SparseRationalVector& lower_field) const {
        return transpose_apply_sparse(upper_grade, lower_field.view());
    }

    [[nodiscard]] numeric::SparseRationalVector transpose_apply_sparse_counted(
        Grade upper_grade,
        numeric::SparseRationalView lower_field,
        numeric::WorkCounters& counters) const {
        return transpose_apply_sparse_impl<true>(upper_grade, lower_field, &counters);
    }

private:
    template <bool Count>
    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse_impl(
        Grade upper_grade,
        numeric::SparseRationalView upper_field,
        numeric::WorkCounters* counters) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("invalid upper grade");
        const auto& upper = grades_[upper_grade.value];
        const auto& lower = grades_[upper_grade.value - 1u];
        if (upper_field.size() != upper.cell_count()) throw std::invalid_argument("boundary field size mismatch");
        const auto exists_id = signature_.existence_aspect();
        std::vector<numeric::SparseRationalEntry> contributions;

        std::size_t reserve_hint = 0;
        for (const auto& active : upper_field.entries()) {
            const auto [begin, end] = upper.incidence_range(active.index);
            reserve_hint += end - begin;
        }
        contributions.reserve(reserve_hint);

        for (const auto& active : upper_field.entries()) {
            if constexpr (Count) ++counters->active_coordinates;
            numeric::Rational scaled_source = active.value / upper.cell_scales[active.index];
            scaled_source.canonicalize();
            const auto [begin, end] = upper.incidence_range(active.index);
            for (std::size_t p = begin; p < end; ++p) {
                if constexpr (Count) ++counters->incidences_touched;
                if (!upper.aspects.get(exists_id, p)) continue;
                if (upper.lattice_coefficient[p] == 0) continue;
                contributions.push_back({upper.lower_index[p], scaled_source * upper.lattice_coefficient[p]});
            }
        }
        const auto lattice = numeric::SparseRationalVector(lower.cell_count(), std::move(contributions));
        std::vector<numeric::SparseRationalEntry> realized;
        realized.reserve(lattice.nonzeros());
        for (const auto& entry : lattice.entries()) {
            numeric::Rational value = entry.value * lower.cell_scales[entry.index];
            value.canonicalize();
            if (value != 0) realized.push_back({entry.index, std::move(value)});
        }
        return numeric::SparseRationalVector(lower.cell_count(), std::move(realized));
    }

    template <bool Count>
    [[nodiscard]] numeric::SparseRationalVector transpose_apply_sparse_impl(
        Grade upper_grade,
        numeric::SparseRationalView lower_field,
        numeric::WorkCounters* counters) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("invalid upper grade");
        const auto& upper = grades_[upper_grade.value];
        const auto& lower = grades_[upper_grade.value - 1u];
        if (lower_field.size() != lower.cell_count()) throw std::invalid_argument("transpose field size mismatch");
        const auto exists_id = signature_.existence_aspect();
        std::vector<numeric::SparseRationalEntry> contributions;

        std::size_t reserve_hint = 0;
        for (const auto& active : lower_field.entries()) {
            const auto [begin, end] = upper.coface_incidence_range(active.index);
            reserve_hint += end - begin;
        }
        contributions.reserve(reserve_hint);

        for (const auto& active : lower_field.entries()) {
            if constexpr (Count) ++counters->active_coordinates;
            numeric::Rational scaled_source = active.value * lower.cell_scales[active.index];
            scaled_source.canonicalize();
            const auto [begin, end] = upper.coface_incidence_range(active.index);
            for (std::size_t q = begin; q < end; ++q) {
                if constexpr (Count) ++counters->incidences_touched;
                const auto p = upper.incidence_by_lower[q];
                if (!upper.aspects.get(exists_id, p)) continue;
                if (upper.lattice_coefficient[p] == 0) continue;
                contributions.push_back({upper.upper_index[p], scaled_source * upper.lattice_coefficient[p]});
            }
        }
        const auto lattice = numeric::SparseRationalVector(upper.cell_count(), std::move(contributions));
        std::vector<numeric::SparseRationalEntry> realized;
        realized.reserve(lattice.nonzeros());
        for (const auto& entry : lattice.entries()) {
            numeric::Rational value = entry.value / upper.cell_scales[entry.index];
            value.canonicalize();
            if (value != 0) realized.push_back({entry.index, std::move(value)});
        }
        return numeric::SparseRationalVector(upper.cell_count(), std::move(realized));
    }

    friend class RelationalComplexBuilder;

    RelationalComplex(RelationSignature signature,
                      SemanticScale semantic_scale,
                      std::vector<GradeArena> grades)
        : signature_(std::move(signature)), semantic_scale_(semantic_scale), grades_(std::move(grades)) {
        if (grades_.empty()) throw std::invalid_argument("relational complex requires grade zero");
        for (std::size_t k = 0; k < grades_.size(); ++k) {
            const auto& arena = grades_[k];
            if (arena.grade.value != k) throw std::invalid_argument("grade arenas must be contiguous and ordered");
            if (arena.grade.value == 0) {
                if (arena.identities.size() != arena.cell_count()) throw std::invalid_argument("grade-zero identity count mismatch");
            } else {
                if (arena.relation_identities.size() != arena.cell_count()) throw std::invalid_argument("primary relation identity count mismatch");
                if (arena.occurrence_identities.size() != arena.incidence_count()) throw std::invalid_argument("boundary occurrence identity count mismatch");
                if (arena.offsets.size() != arena.cell_count() + 1u) throw std::invalid_argument("boundary offset count mismatch");
                const auto lower_size = grades_[k - 1u].cell_count();
                if (arena.lower_offsets.size() != lower_size + 1u) throw std::invalid_argument("reverse incidence offset count mismatch");
                if (arena.incidence_by_lower.size() != arena.incidence_count()) throw std::invalid_argument("reverse incidence slot count mismatch");
            }
        }
    }

    RelationSignature signature_;
    SemanticScale semantic_scale_{};
    std::vector<GradeArena> grades_;
};

} // namespace rfc::core
