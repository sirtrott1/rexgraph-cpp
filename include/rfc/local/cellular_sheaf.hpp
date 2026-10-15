#pragma once
#include "rfc/local/fiber_system.hpp"
#include "rfc/local/stalk_bundle.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::local {

// Exact additive cellular sheaf/local system on the retained relational
// incidence topology.  These maps run in the sheaf direction: from a lower
// face stalk into a containing upper/coface stalk.  They are intentionally
// distinct from LinearRelationalFiberSystem's downward composite-boundary maps.
class LinearCellularSheaf {
public:
    explicit LinearCellularSheaf(const LinearStalkBundle& stalks)
        : stalks_(&stalks) {
        restrictions_.resize(stalks_->complex().grade_count());
        for (std::size_t k = 1; k < stalks_->complex().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            restrictions_[k].resize(stalks_->complex().grade(grade).incidence_count());
        }
    }

    explicit LinearCellularSheaf(const LinearRelationalFiberSystem& fibers) : LinearCellularSheaf(fibers.stalks()) {}

    LinearCellularSheaf(const LinearStalkBundle&&) = delete;
    LinearCellularSheaf(const LinearRelationalFiberSystem&&) = delete;
    [[nodiscard]] const LinearStalkBundle& stalks() const noexcept { return *stalks_; }
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return stalks_->complex(); }

    // Declares rho_{lower -> upper} on one retained adjacent incidence flag.
    void set_restriction(core::Grade upper_grade,
                         std::size_t incidence_slot,
                         numeric::ExactMatrix map) {
        validate_flag(upper_grade, incidence_slot);
        const auto& arena = complex().grade(upper_grade);
        const core::CellId upper{upper_grade, arena.upper_index[incidence_slot]};
        const core::CellId lower{core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)},
                                 arena.lower_index[incidence_slot]};
        if (map.rows() != stalks_->dimension(upper) || map.cols() != stalks_->dimension(lower)) {
            throw std::invalid_argument("sheaf restriction stalk dimensions do not match");
        }
        restrictions_[upper_grade.value][incidence_slot] = std::move(map);
    }

    [[nodiscard]] numeric::ExactMatrix restriction(core::Grade upper_grade,
                                                    std::size_t incidence_slot) const {
        validate_flag(upper_grade, incidence_slot);
        if (restrictions_[upper_grade.value][incidence_slot]) {
            return *restrictions_[upper_grade.value][incidence_slot];
        }
        const auto& arena = complex().grade(upper_grade);
        const core::CellId upper{upper_grade, arena.upper_index[incidence_slot]};
        const core::CellId lower{core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)},
                                 arena.lower_index[incidence_slot]};
        const auto upper_dim = stalks_->dimension(upper);
        const auto lower_dim = stalks_->dimension(lower);
        if (upper_dim != lower_dim) {
            throw std::logic_error("unequal sheaf stalk dimensions require an explicit restriction");
        }
        return numeric::ExactMatrix::identity(upper_dim);
    }

    // Inspect a declared rho without materializing the default identity or
    // copying a sparse map. Unlike downward transport overrides, rho is not
    // already multiplied by the retained incidence coefficient.
    [[nodiscard]] const std::optional<numeric::ExactMatrix>& restriction_override(
        core::Grade upper_grade, std::size_t incidence_slot) const & {
        validate_flag(upper_grade, incidence_slot);
        return restrictions_[upper_grade.value][incidence_slot];
    }
    const std::optional<numeric::ExactMatrix>& restriction_override(
        core::Grade, std::size_t) const && = delete;

    [[nodiscard]] std::size_t cochain_dimension(core::Grade grade) const {
        if (grade.value >= complex().grade_count()) throw std::out_of_range("sheaf cochain grade");
        return stalks_->total_dimension(grade);
    }

    // delta^q : C^q(F) -> C^{q+1}(F).  The exact relational incidence
    // coefficient supplies the oriented/weighted cellular incidence while the
    // declared restriction transports local data into the upper stalk.
    [[nodiscard]] numeric::ExactMatrix coboundary(core::Grade grade) const {
        if (grade.value >= complex().grade_count()) throw std::out_of_range("sheaf coboundary grade");
        const auto source_offsets = stalks_->offsets(grade);
        const auto source_dim = source_offsets.back();
        if (static_cast<std::size_t>(grade.value) + 1u >= complex().grade_count()) {
            return numeric::ExactMatrix(0, source_dim);
        }

        const auto upper_grade = core::Grade{static_cast<std::uint16_t>(grade.value + 1u)};
        const auto target_offsets = stalks_->offsets(upper_grade);
        numeric::ExactMatrix delta(target_offsets.back(), source_dim);
        const auto exists_id = complex().signature().existence_aspect();
        const auto& arena = complex().grade(upper_grade);

        for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
            if (!arena.aspects.get(exists_id, slot)) continue;
            const auto upper = static_cast<std::size_t>(arena.upper_index[slot]);
            const auto lower = static_cast<std::size_t>(arena.lower_index[slot]);
            const auto coefficient = complex().incidence_coefficient(upper_grade, slot);
            const auto rho = restriction(upper_grade, slot);
            const auto row0 = target_offsets[upper];
            const auto col0 = source_offsets[lower];
            for (const auto& [key, value] : rho.entries()) {
                const auto row = row0 + key.first;
                const auto col = col0 + key.second;
                numeric::Rational updated = delta.get(row, col) + coefficient * value;
                updated.canonicalize();
                delta.set(row, col, std::move(updated));
            }
        }
        return delta;
    }

    [[nodiscard]] numeric::ExactMatrix two_step_defect(core::Grade grade) const {
        if (static_cast<std::size_t>(grade.value) + 2u >= complex().grade_count()) {
            return numeric::ExactMatrix(0, cochain_dimension(grade));
        }
        return coboundary(core::Grade{static_cast<std::uint16_t>(grade.value + 1u)}).multiply(coboundary(grade));
    }

    [[nodiscard]] bool is_cochain_complex() const {
        if (complex().grade_count() < 3u) return true;
        for (std::size_t q = 0; q + 2u < complex().grade_count(); ++q) {
            if (two_step_defect(core::Grade{static_cast<std::uint16_t>(q)}).nonzeros() != 0u) return false;
        }
        return true;
    }

    [[nodiscard]] bool has_invertible_restrictions() const {
        const auto exists_id = complex().signature().existence_aspect();
        for (std::size_t k = 1; k < complex().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = complex().grade(grade);
            for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
                if (!arena.aspects.get(exists_id, slot)) continue;
                const auto rho = restriction(grade, slot);
                if (rho.rows() != rho.cols() || rho.rank() != rho.rows()) return false;
            }
        }
        return true;
    }

    // This additive retained-incidence contract requires invertible adjacent
    // restrictions and the paper's exact cochain coherence delta^2 = 0.  It
    // does not identify distinct occurrence routes or require trivial holonomy
    // around a noncontractible loop.
    [[nodiscard]] bool is_local_system() const {
        return has_invertible_restrictions() && is_cochain_complex();
    }

    [[nodiscard]] numeric::ExactMatrix global_section_frame() const {
        return coboundary(core::Grade{0}).kernel_frame();
    }

private:
    void validate_flag(core::Grade upper_grade, std::size_t incidence_slot) const {
        if (upper_grade.value == 0 || upper_grade.value >= complex().grade_count()) {
            throw std::out_of_range("sheaf restriction requires a positive upper grade");
        }
        if (incidence_slot >= complex().grade(upper_grade).incidence_count()) {
            throw std::out_of_range("sheaf restriction incidence");
        }
    }

    const LinearStalkBundle* stalks_{};
    std::vector<std::vector<std::optional<numeric::ExactMatrix>>> restrictions_;
};

} // namespace rfc::local
