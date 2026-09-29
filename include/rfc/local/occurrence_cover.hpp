#pragma once
#include "rfc/local/fiber_system.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::local {

class LinearOccurrenceCover {
public:
    LinearOccurrenceCover(const LinearStalkBundle& stalks, core::Grade upper_grade)
        : stalks_(&stalks), upper_grade_(upper_grade) {
        if (upper_grade.value == 0 || upper_grade.value >= stalks.complex().grade_count()) {
            throw std::out_of_range("occurrence cover requires a positive upper grade");
        }
        lower_grade_ = core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)};
        global_offsets_ = stalks.offsets(lower_grade_);
        multiplicity_.assign(stalks.complex().grade(lower_grade_).cell_count(), 0u);
        incidence_to_occurrence_.assign(stalks.complex().grade(upper_grade_).incidence_count(), absent);

        const auto exists_id = stalks.complex().signature().existence_aspect();
        const auto& arena = stalks.complex().grade(upper_grade_);
        occurrence_offsets_.push_back(0u);
        for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
            if (!arena.aspects.get(exists_id, slot)) continue;
            const auto lower = arena.lower_index[slot];
            const auto dim = stalks.dimension(core::CellId{lower_grade_, lower});
            incidence_to_occurrence_[slot] = occurrence_slots_.size();
            occurrence_slots_.push_back(slot);
            occurrence_lower_.push_back(lower);
            ++multiplicity_[lower];
            occurrence_offsets_.push_back(machine::checked_size_add(occurrence_offsets_.back(), dim));
        }
    }

    LinearOccurrenceCover(const LinearStalkBundle&&, core::Grade) = delete;
    [[nodiscard]] core::Grade upper_grade() const noexcept { return upper_grade_; }
    [[nodiscard]] core::Grade lower_grade() const noexcept { return lower_grade_; }
    [[nodiscard]] std::size_t occurrence_count() const noexcept { return occurrence_slots_.size(); }
    [[nodiscard]] std::size_t occurrence_dimension() const noexcept { return occurrence_offsets_.back(); }
    [[nodiscard]] std::size_t global_dimension() const noexcept { return global_offsets_.back(); }
    [[nodiscard]] const std::vector<std::size_t>& multiplicities() const noexcept { return multiplicity_; }
    [[nodiscard]] std::size_t occurrence_index_for_incidence(std::size_t incidence_slot) const {
        if (incidence_slot >= incidence_to_occurrence_.size()) throw std::out_of_range("incidence slot");
        const auto index = incidence_to_occurrence_[incidence_slot];
        if (index == absent) throw std::invalid_argument("incidence is not active in occurrence cover");
        return index;
    }
    [[nodiscard]] std::size_t occurrence_offset(std::size_t occurrence_index) const { return occurrence_offsets_.at(occurrence_index); }
    [[nodiscard]] std::uint32_t occurrence_lower_index(std::size_t occurrence_index) const { return occurrence_lower_.at(occurrence_index); }

    [[nodiscard]] numeric::ExactMatrix duplication_matrix() const {
        numeric::ExactMatrix out(occurrence_dimension(), global_dimension());
        for (std::size_t o = 0; o < occurrence_count(); ++o) {
            const auto lower = occurrence_lower_[o];
            const auto dim = global_offsets_[lower + 1u] - global_offsets_[lower];
            for (std::size_t j = 0; j < dim; ++j) out.set(occurrence_offsets_[o] + j, global_offsets_[lower] + j, numeric::Rational{1});
        }
        return out;
    }

    [[nodiscard]] numeric::ExactMatrix sum_assembly_matrix() const {
        numeric::ExactMatrix out(global_dimension(), occurrence_dimension());
        for (std::size_t o = 0; o < occurrence_count(); ++o) {
            const auto lower = occurrence_lower_[o];
            const auto dim = global_offsets_[lower + 1u] - global_offsets_[lower];
            for (std::size_t j = 0; j < dim; ++j) out.set(global_offsets_[lower] + j, occurrence_offsets_[o] + j, numeric::Rational{1});
        }
        return out;
    }

    // Quotient/coarsening for local sections.  On the covered global subspace,
    // Q R = I where R duplicates a global value into every local occurrence.
    [[nodiscard]] numeric::ExactMatrix quotient_matrix() const {
        numeric::ExactMatrix out(global_dimension(), occurrence_dimension());
        for (std::size_t o = 0; o < occurrence_count(); ++o) {
            const auto lower = occurrence_lower_[o];
            const auto mult = multiplicity_[lower];
            if (mult == 0) continue;
            const numeric::Rational weight{1, static_cast<unsigned long>(mult)};
            const auto dim = global_offsets_[lower + 1u] - global_offsets_[lower];
            for (std::size_t j = 0; j < dim; ++j) out.set(global_offsets_[lower] + j, occurrence_offsets_[o] + j, weight);
        }
        return out;
    }

    [[nodiscard]] numeric::RationalVector duplicate_global(const numeric::RationalVector& global) const {
        return duplication_matrix().apply(global);
    }
    [[nodiscard]] numeric::RationalVector sum_assemble(const numeric::RationalVector& local) const {
        return sum_assembly_matrix().apply(local);
    }
    [[nodiscard]] numeric::RationalVector quotient(const numeric::RationalVector& local) const {
        return quotient_matrix().apply(local);
    }

    [[nodiscard]] bool compatible(const numeric::RationalVector& local) const {
        if (local.size() != occurrence_dimension()) throw std::invalid_argument("occurrence section size mismatch");
        const auto global_cells = stalks_->complex().grade(lower_grade_).cell_count();
        std::vector<bool> seen(global_cells, false);
        std::vector<numeric::RationalVector> first(global_cells);
        for (std::size_t o = 0; o < occurrence_count(); ++o) {
            const auto lower = occurrence_lower_[o];
            const auto dim = occurrence_offsets_[o + 1u] - occurrence_offsets_[o];
            numeric::RationalVector value(dim);
            for (std::size_t j = 0; j < dim; ++j) value[j] = local[occurrence_offsets_[o] + j];
            if (!seen[lower]) {
                first[lower] = std::move(value);
                seen[lower] = true;
            } else if (first[lower] != value) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] numeric::RationalVector glue_compatible(const numeric::RationalVector& local) const {
        if (!compatible(local)) throw std::invalid_argument("local occurrence section is not compatible on shared stalks");
        return quotient(local);
    }

    [[nodiscard]] numeric::RationalVector scatter_sections(const std::vector<RelationLocalSection>& sections) const {
        numeric::RationalVector local(occurrence_dimension(), numeric::Rational{0});
        std::vector<bool> written(occurrence_count(), false);
        const auto& arena = stalks_->complex().grade(upper_grade_);
        std::vector<bool> source_written(arena.cell_count(), false);
        for (const auto& section : sections) {
            if (section.relation.grade != upper_grade_) throw std::invalid_argument("relation section belongs to another grade");
            if (section.relation.index >= arena.cell_count()) throw std::out_of_range("relation section owner");
            if (source_written[section.relation.index]) throw std::invalid_argument("duplicate relation section");
            source_written[section.relation.index] = true;
            if (section.upper_value.size() != stalks_->dimension(section.relation))
                throw std::invalid_argument("relation section upper stalk dimension mismatch");
            for (const auto& occurrence : section.boundary_values) {
                if (occurrence.incidence_slot >= incidence_to_occurrence_.size()) throw std::out_of_range("relation section incidence slot");
                const auto slot = occurrence.incidence_slot;
                if (arena.upper_index[slot] != section.relation.index ||
                    occurrence.lower != core::CellId{lower_grade_, arena.lower_index[slot]})
                    throw std::invalid_argument("relation section occurrence owner/participant mismatch");
                const auto o = incidence_to_occurrence_[slot];
                if (o == absent) throw std::logic_error("relation section refers to inactive occurrence");
                if (written[o]) throw std::invalid_argument("occurrence contribution written more than once");
                const auto dim = occurrence_offsets_[o + 1u] - occurrence_offsets_[o];
                if (occurrence.value.size() != dim) throw std::invalid_argument("relation section lower stalk dimension mismatch");
                for (std::size_t j = 0; j < dim; ++j) local[occurrence_offsets_[o] + j] = occurrence.value[j];
                written[o] = true;
            }
        }
        return local;
    }

    [[nodiscard]] numeric::ExactMatrix section_metric_pullback(const numeric::ExactMatrix& global_metric) const {
        if (global_metric.rows() != global_dimension() || global_metric.cols() != global_dimension()) {
            throw std::invalid_argument("global metric dimension mismatch");
        }
        const auto q = quotient_matrix();
        return q.transpose().multiply(global_metric).multiply(q);
    }

    [[nodiscard]] numeric::ExactMatrix assembly_metric_pullback(const numeric::ExactMatrix& global_metric) const {
        if (global_metric.rows() != global_dimension() || global_metric.cols() != global_dimension()) {
            throw std::invalid_argument("global metric dimension mismatch");
        }
        const auto a = sum_assembly_matrix();
        return a.transpose().multiply(global_metric).multiply(a);
    }

private:
    static constexpr std::size_t absent = std::numeric_limits<std::size_t>::max();
    const LinearStalkBundle* stalks_{};
    core::Grade upper_grade_{};
    core::Grade lower_grade_{};
    std::vector<std::size_t> global_offsets_;
    std::vector<std::size_t> occurrence_slots_;
    std::vector<std::uint32_t> occurrence_lower_;
    std::vector<std::size_t> occurrence_offsets_;
    std::vector<std::size_t> incidence_to_occurrence_;
    std::vector<std::size_t> multiplicity_;
};

// Compatibility alias.  Prefer LinearOccurrenceCover in new code.
using OccurrenceCover = LinearOccurrenceCover;

} // namespace rfc::local
