#pragma once
#include "rfc/local/fiber_system.hpp"
#include "rfc/local/occurrence_cover.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::local {

// A grade tensor field is the local-to-global realization of all primary
// relation tensors at one grade.  It retains the local blocks, realizes them
// into occurrence space, then assembles through shared lower stalks.
class GradeTensorField {
public:
    GradeTensorField(const LinearRelationalFiberSystem& fibers, core::Grade upper_grade)
        : fibers_(&fibers), upper_grade_(upper_grade), cover_(fibers.stalks(), upper_grade) {
        if (upper_grade.value == 0 || upper_grade.value >= fibers.complex().grade_count()) {
            throw std::out_of_range("grade tensor field requires a positive grade");
        }
        upper_offsets_ = fibers.stalks().offsets(upper_grade_);
    }

    [[nodiscard]] core::Grade upper_grade() const noexcept { return upper_grade_; }
    [[nodiscard]] const LinearOccurrenceCover& occurrence_cover() const noexcept { return cover_; }
    [[nodiscard]] std::size_t source_dimension() const noexcept { return upper_offsets_.back(); }
    [[nodiscard]] std::size_t target_dimension() const noexcept { return cover_.global_dimension(); }

    [[nodiscard]] std::vector<RelationLocalSection> local_sections(const numeric::RationalVector& upper_field) const {
        if (upper_field.size() != source_dimension()) throw std::invalid_argument("grade tensor source field size mismatch");
        const auto relation_count = fibers_->complex().grade(upper_grade_).cell_count();
        std::vector<RelationLocalSection> sections;
        sections.reserve(relation_count);
        for (std::size_t relation = 0; relation < relation_count; ++relation) {
            const auto begin = upper_offsets_[relation];
            const auto end = upper_offsets_[relation + 1u];
            numeric::RationalVector local_value(end - begin);
            bool any = false;
            for (std::size_t j = begin; j < end; ++j) {
                local_value[j - begin] = upper_field[j];
                if (upper_field[j] != 0) any = true;
            }
            if (any) sections.push_back(fibers_->realize_relation(upper_grade_, relation, local_value));
        }
        return sections;
    }

    [[nodiscard]] numeric::RationalVector occurrence_apply(const numeric::RationalVector& upper_field) const {
        return cover_.scatter_sections(local_sections(upper_field));
    }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& upper_field) const {
        return cover_.sum_assemble(occurrence_apply(upper_field));
    }

    [[nodiscard]] numeric::ExactMatrix occurrence_matrix() const {
        numeric::ExactMatrix out(cover_.occurrence_dimension(), source_dimension());
        const auto relation_count = fibers_->complex().grade(upper_grade_).cell_count();
        for (std::size_t relation = 0; relation < relation_count; ++relation) {
            const auto tensor = fibers_->relation_tensor(upper_grade_, relation);
            const auto source_offset = upper_offsets_[relation];
            for (const auto& block : tensor.blocks()) {
                const auto occurrence = cover_.occurrence_index_for_incidence(block.incidence_slot);
                const auto target_offset = cover_.occurrence_offset(occurrence);
                for (const auto& [key, value] : block.map.entries()) {
                    out.set(target_offset + key.first, source_offset + key.second, value);
                }
            }
        }
        return out;
    }

    [[nodiscard]] numeric::ExactMatrix matrix() const {
        return cover_.sum_assembly_matrix().multiply(occurrence_matrix());
    }

    [[nodiscard]] numeric::ExactMatrix local_metric_from_global(const numeric::ExactMatrix& global_metric) const {
        const auto local_map = occurrence_matrix();
        const auto assembly_metric = cover_.assembly_metric_pullback(global_metric);
        return local_map.transpose().multiply(assembly_metric).multiply(local_map);
    }

private:
    const LinearRelationalFiberSystem* fibers_{};
    core::Grade upper_grade_{};
    LinearOccurrenceCover cover_;
    std::vector<std::size_t> upper_offsets_;
};

} // namespace rfc::local
