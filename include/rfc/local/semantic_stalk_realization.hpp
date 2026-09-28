#pragma once
#include "rfc/local/fiber_system.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::local {

inline bool exact_matrix_equal(const numeric::ExactMatrix& lhs, const numeric::ExactMatrix& rhs) {
    return lhs.rows() == rhs.rows() && lhs.cols() == rhs.cols() && lhs.entries() == rhs.entries();
}

// Vector/tensor realization across semantic scale.  The cell-level
// SemanticScaleRealization says which finer cells realize a macro cell; these
// stalk maps say how the local tensor state itself is transported.  Naturality
// is tested against the assembled local relation tensors, not against a second
// independently invented boundary representation.
class SemanticStalkRealization {
public:
    SemanticStalkRealization(const LinearRelationalFiberSystem& macro,
                             const LinearRelationalFiberSystem& micro,
                             geometry::SemanticScaleRealization cell_realization,
                             std::vector<numeric::ExactMatrix> grade_maps,
                             std::optional<std::vector<numeric::ExactMatrix>> coarsening = std::nullopt)
        : macro_(&macro), micro_(&micro), cell_realization_(std::move(cell_realization)),
          grade_maps_(std::move(grade_maps)), coarsening_(std::move(coarsening)) {
        if (&cell_realization_.realization().source() != &macro.complex() ||
            &cell_realization_.realization().target() != &micro.complex()) {
            throw std::invalid_argument("semantic stalk realization does not match cell realization complexes");
        }
        if (grade_maps_.size() != macro.complex().grade_count()) {
            throw std::invalid_argument("semantic stalk realization grade count mismatch");
        }
        if (micro.complex().grade_count() < macro.complex().grade_count()) {
            throw std::invalid_argument("semantic stalk target has insufficient grades");
        }
        for (std::size_t k = 0; k < grade_maps_.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            if (grade_maps_[k].rows() != micro.stalks().total_dimension(grade) ||
                grade_maps_[k].cols() != macro.stalks().total_dimension(grade)) {
                throw std::invalid_argument("semantic stalk map dimension mismatch");
            }
        }
        if (!support_compatible()) throw std::invalid_argument("semantic stalk map leaves the declared cell realization support");
        if (!is_natural()) throw std::invalid_argument("semantic stalk realization is not natural with local tensor boundaries");

        if (coarsening_) {
            if (coarsening_->size() != grade_maps_.size()) throw std::invalid_argument("semantic stalk coarsening grade count mismatch");
            for (std::size_t k = 0; k < coarsening_->size(); ++k) {
                const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
                const auto& r = coarsening_->at(k);
                if (r.rows() != macro.stalks().total_dimension(grade) ||
                    r.cols() != micro.stalks().total_dimension(grade)) {
                    throw std::invalid_argument("semantic stalk coarsening dimension mismatch");
                }
                const auto roundtrip = r.multiply(grade_maps_[k]);
                if (!exact_matrix_equal(roundtrip, numeric::ExactMatrix::identity(roundtrip.rows()))) {
                    throw std::invalid_argument("semantic stalk coarsening must retract realization");
                }
            }
            if (!coarsening_is_natural()) throw std::invalid_argument("semantic stalk coarsening is not natural");
        }
    }

    [[nodiscard]] const LinearRelationalFiberSystem& macro() const noexcept { return *macro_; }
    [[nodiscard]] const LinearRelationalFiberSystem& micro() const noexcept { return *micro_; }
    [[nodiscard]] const geometry::SemanticScaleRealization& cell_realization() const noexcept { return cell_realization_; }
    [[nodiscard]] const numeric::ExactMatrix& map(core::Grade grade) const { return grade_maps_.at(grade.value); }
    [[nodiscard]] bool has_coarsening() const noexcept { return coarsening_.has_value(); }
    [[nodiscard]] const numeric::ExactMatrix& coarsening(core::Grade grade) const {
        if (!coarsening_) throw std::logic_error("semantic stalk realization has no coarsening");
        return coarsening_->at(grade.value);
    }

    [[nodiscard]] numeric::RationalVector realize(core::Grade grade,
                                                   const numeric::RationalVector& macro_field) const {
        return map(grade).apply(macro_field);
    }

    [[nodiscard]] numeric::RationalVector coarsen(core::Grade grade,
                                                  const numeric::RationalVector& micro_field) const {
        return coarsening(grade).apply(micro_field);
    }

    [[nodiscard]] numeric::ExactMatrix metric_pullback(core::Grade grade,
                                                        const numeric::ExactMatrix& micro_metric) const {
        const auto& j = map(grade);
        if (micro_metric.rows() != j.rows() || micro_metric.cols() != j.rows()) {
            throw std::invalid_argument("semantic micro metric dimension mismatch");
        }
        return j.transpose().multiply(micro_metric).multiply(j);
    }

    [[nodiscard]] bool is_natural() const {
        for (std::size_t k = 1; k < macro().complex().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto lower = core::Grade{static_cast<std::uint16_t>(k - 1u)};
            const GradeTensorField macro_boundary(macro(), grade);
            const GradeTensorField micro_boundary(micro(), grade);
            const auto lhs = micro_boundary.matrix().multiply(map(grade));
            const auto rhs = map(lower).multiply(macro_boundary.matrix());
            if (!exact_matrix_equal(lhs, rhs)) return false;
        }
        return true;
    }

    [[nodiscard]] static SemanticStalkRealization compose(const SemanticStalkRealization& first,
                                                           const SemanticStalkRealization& second) {
        if (&first.micro().complex() != &second.macro().complex()) {
            throw std::invalid_argument("semantic stalk composition complex mismatch");
        }
        const auto chain = geometry::compose_chain_maps(first.cell_realization().realization(),
                                                       second.cell_realization().realization());
        std::optional<geometry::RelationalChainMap> chain_coarsening;
        if (first.cell_realization().has_coarsening() && second.cell_realization().has_coarsening()) {
            chain_coarsening = geometry::compose_chain_maps(second.cell_realization().coarsening(),
                                                           first.cell_realization().coarsening());
        }
        geometry::SemanticScaleRealization cells(first.cell_realization().macro_scale(),
                                       second.cell_realization().micro_scale(),
                                       chain,
                                       chain_coarsening);

        std::vector<numeric::ExactMatrix> maps;
        maps.reserve(first.grade_maps_.size());
        for (std::size_t k = 0; k < first.grade_maps_.size(); ++k) {
            maps.push_back(second.grade_maps_.at(k).multiply(first.grade_maps_.at(k)));
        }
        std::optional<std::vector<numeric::ExactMatrix>> retract;
        if (first.coarsening_ && second.coarsening_) {
            std::vector<numeric::ExactMatrix> result;
            result.reserve(first.coarsening_->size());
            for (std::size_t k = 0; k < first.coarsening_->size(); ++k) {
                result.push_back(first.coarsening_->at(k).multiply(second.coarsening_->at(k)));
            }
            retract = std::move(result);
        }
        return SemanticStalkRealization(first.macro(), second.micro(), std::move(cells), std::move(maps), std::move(retract));
    }

private:
    [[nodiscard]] bool support_compatible() const {
        for (std::size_t k = 0; k < grade_maps_.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto macro_offsets = macro().stalks().offsets(grade);
            const auto micro_offsets = micro().stalks().offsets(grade);
            for (std::size_t macro_cell = 0; macro_cell + 1u < macro_offsets.size(); ++macro_cell) {
                std::vector<bool> allowed(micro_offsets.size() - 1u, false);
                for (const auto& term : cell_realization_.realization().image(grade, macro_cell)) {
                    if (term.coefficient != 0) allowed.at(term.target) = true;
                }
                for (const auto& [key, value] : grade_maps_[k].entries()) {
                    if (value == 0 || key.second < macro_offsets[macro_cell] || key.second >= macro_offsets[macro_cell + 1u]) continue;
                    const auto row = key.first;
                    std::size_t micro_cell = 0;
                    while (micro_cell + 1u < micro_offsets.size() && row >= micro_offsets[micro_cell + 1u]) ++micro_cell;
                    if (micro_cell >= allowed.size() || !allowed[micro_cell]) return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool coarsening_is_natural() const {
        if (!coarsening_) return true;
        for (std::size_t k = 1; k < macro().complex().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto lower = core::Grade{static_cast<std::uint16_t>(k - 1u)};
            const GradeTensorField macro_boundary(macro(), grade);
            const GradeTensorField micro_boundary(micro(), grade);
            const auto lhs = macro_boundary.matrix().multiply(coarsening_->at(k));
            const auto rhs = coarsening_->at(lower.value).multiply(micro_boundary.matrix());
            if (!exact_matrix_equal(lhs, rhs)) return false;
        }
        return true;
    }

    const LinearRelationalFiberSystem* macro_{};
    const LinearRelationalFiberSystem* micro_{};
    geometry::SemanticScaleRealization cell_realization_;
    std::vector<numeric::ExactMatrix> grade_maps_;
    std::optional<std::vector<numeric::ExactMatrix>> coarsening_;
};

} // namespace rfc::local
