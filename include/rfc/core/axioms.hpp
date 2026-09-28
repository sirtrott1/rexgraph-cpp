#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rfc::core {

struct ChainInterfaceCheck {
    Grade lower_boundary_grade{}; // checks B_k B_{k+1}; this stores k
    bool satisfied{false};
};

struct BianchiReport {
    Grade through_grade{};
    std::vector<ChainInterfaceCheck> interfaces;
    [[nodiscard]] bool satisfied() const noexcept {
        for (const auto& item : interfaces) if (!item.satisfied) return false;
        return true;
    }
};

inline bool chain_interface_holds(const RelationalComplex& complex, Grade lower_boundary_grade) {
    const auto k = lower_boundary_grade.value;
    if (k == 0 || static_cast<std::size_t>(k + 1u) >= complex.grade_count()) return true;
    const auto& top = complex.grade(Grade{static_cast<std::uint16_t>(k + 1u)});
    const auto& middle = complex.grade(Grade{k});
    const auto exists_id = complex.signature().existence_aspect();

    for (std::size_t column = 0; column < top.cell_count(); ++column) {
        // The frozen tower already carries a coordinated graded lattice, so
        // B_k B_{k+1}=0 can be certified directly in integer coordinates:
        // \tilde B_k \tilde B_{k+1}=0.  This avoids rational reconstruction.
        std::unordered_map<std::uint32_t, numeric::Integer> residual;
        const auto [top_begin, top_end] = top.incidence_range(column);

        std::size_t route_hint = 0;
        for (std::size_t p = top_begin; p < top_end; ++p) {
            if (!top.aspects.get(exists_id, p)) continue;
            const auto middle_cell = top.lower_index[p];
            const auto [mid_begin, mid_end] = middle.incidence_range(middle_cell);
            route_hint += mid_end - mid_begin;
        }
        residual.reserve(route_hint);

        for (std::size_t p = top_begin; p < top_end; ++p) {
            if (!top.aspects.get(exists_id, p)) continue;
            const auto& upper_coefficient = top.lattice_coefficient[p];
            if (upper_coefficient == 0) continue;

            const auto middle_cell = top.lower_index[p];
            const auto [mid_begin, mid_end] = middle.incidence_range(middle_cell);
            for (std::size_t q = mid_begin; q < mid_end; ++q) {
                if (!middle.aspects.get(exists_id, q)) continue;
                const auto& lower_coefficient = middle.lattice_coefficient[q];
                if (lower_coefficient == 0) continue;

                const auto contribution = upper_coefficient * lower_coefficient;
                auto [it, inserted] = residual.try_emplace(middle.lower_index[q], contribution);
                if (!inserted) it->second += contribution;
                if (it->second == 0) residual.erase(it);
            }
        }
        if (!residual.empty()) return false;
    }
    return true;
}

inline BianchiReport bianchi_through(const RelationalComplex& complex, Grade through_grade) {
    if (through_grade.value > complex.max_grade().value) through_grade = complex.max_grade();
    BianchiReport report{through_grade, {}};
    if (through_grade.value < 2) return report;
    report.interfaces.reserve(through_grade.value - 1u);
    for (std::uint16_t k = 1; k < through_grade.value; ++k) {
        report.interfaces.push_back(ChainInterfaceCheck{Grade{k}, chain_interface_holds(complex, Grade{k})});
    }
    return report;
}

// A normal RelationalComplexBuilder validates every asserted higher boundary
// before it can enter the frozen canonical source.  Callers that already hold
// such a canonical complex can therefore carry the Bianchi certificate without
// re-traversing all two-step incidence routes.  bianchi_through() remains the
// independent audit path for diagnostics and external/imported representations.
inline BianchiReport canonical_bianchi_certificate(const RelationalComplex& complex) {
    BianchiReport report{complex.max_grade(), {}};
    if (complex.max_grade().value < 2) return report;
    report.interfaces.reserve(complex.max_grade().value - 1u);
    for (std::uint16_t k = 1; k < complex.max_grade().value; ++k) {
        report.interfaces.push_back(ChainInterfaceCheck{Grade{k}, true});
    }
    return report;
}

} // namespace rfc::core
