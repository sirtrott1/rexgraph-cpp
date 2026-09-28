#pragma once
#include "rfc/field/lattice_boundary.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

class MachineLatticeTowerField {
public:
    MachineLatticeTowerField(const core::RelationalComplex& complex,
                             std::vector<std::vector<std::int64_t>> grades)
        : complex_(&complex), grades_(std::move(grades)) {
        if (grades_.size() != complex.grade_count()) throw std::invalid_argument("machine tower grade count mismatch");
        for (std::size_t k = 0; k < grades_.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            if (grades_[k].size() != complex.grade(grade).cell_count()) throw std::invalid_argument("machine tower field grade size mismatch");
        }
    }

    explicit MachineLatticeTowerField(const core::RelationalComplex& complex)
        : complex_(&complex) {
        grades_.reserve(complex.grade_count());
        for (std::size_t k = 0; k < complex.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            grades_.emplace_back(complex.grade(grade).cell_count(), 0);
        }
    }

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] std::size_t grade_count() const noexcept { return grades_.size(); }
    [[nodiscard]] const std::vector<std::int64_t>& grade_values(core::Grade grade) const { return grades_.at(grade.value); }
    [[nodiscard]] std::vector<std::int64_t>& grade_values(core::Grade grade) { return grades_.at(grade.value); }

    [[nodiscard]] MachineLatticeField field(core::Grade grade) const {
        return MachineLatticeField(*complex_, grade, grades_.at(grade.value));
    }

private:
    const core::RelationalComplex* complex_{};
    std::vector<std::vector<std::int64_t>> grades_;
};

inline void add_scaled_tower_grade(std::vector<std::int64_t>& destination,
                                   const std::vector<std::int64_t>& source,
                                   std::int64_t gain) {
    if (destination.size() != source.size()) throw std::invalid_argument("tower channel size mismatch");
    if (gain == 0) return;
    for (std::size_t i = 0; i < destination.size(); ++i) destination[i] = numeric::checked_mac_i64(destination[i], gain, source[i]);
}

inline MachineLatticeTowerField lattice_lowering_apply(const MachineLatticeTowerField& input) {
    MachineLatticeTowerField result(input.complex());
    for (std::size_t k = 1; k < input.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        LatticeBoundaryOperator boundary(input.complex(), grade);
        const auto lowered = boundary.apply(input.field(grade));
        add_scaled_tower_grade(result.grade_values(core::Grade{static_cast<std::uint16_t>(k - 1u)}), lowered.values(), 1);
    }
    return result;
}

inline MachineLatticeTowerField lattice_raising_apply(const MachineLatticeTowerField& input) {
    MachineLatticeTowerField result(input.complex());
    for (std::size_t k = 1; k < input.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        LatticeBoundaryOperator boundary(input.complex(), grade);
        const auto raised = boundary.transpose_apply(input.field(core::Grade{static_cast<std::uint16_t>(k - 1u)}));
        add_scaled_tower_grade(result.grade_values(grade), raised.values(), 1);
    }
    return result;
}

inline MachineLatticeTowerField lattice_dirac_apply(const MachineLatticeTowerField& input) {
    auto lower = lattice_lowering_apply(input);
    const auto raise = lattice_raising_apply(input);
    for (std::size_t k = 0; k < input.grade_count(); ++k) {
        add_scaled_tower_grade(lower.grade_values(core::Grade{static_cast<std::uint16_t>(k)}),
                               raise.grade_values(core::Grade{static_cast<std::uint16_t>(k)}), 1);
    }
    return lower;
}

inline MachineLatticeTowerField lattice_dirac_square_apply(const MachineLatticeTowerField& input) {
    return lattice_dirac_apply(lattice_dirac_apply(input));
}

} // namespace rfc::field
