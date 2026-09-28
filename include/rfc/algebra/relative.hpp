#pragma once
#include "rfc/algebra/subcomplex.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::algebra {

// Exact chain-space quotient C(A)/C(B) for B subset A inside one ambient
// relational complex. It is intentionally a quotient view rather than a claim
// that cosets are themselves new primary relations.
class RelativeChainComplex {
public:
    RelativeChainComplex(const core::RelationalComplex& ambient,
                         SubcomplexMask whole,
                         SubcomplexMask sub)
        : ambient_(&ambient), whole_(std::move(whole)), sub_(std::move(sub)) {
        if (!whole_.boundary_closed(ambient) || !sub_.boundary_closed(ambient)) {
            throw std::invalid_argument("relative pair requires boundary-closed subcomplexes");
        }
        quotient_cells_.resize(ambient.grade_count());
        ambient_to_quotient_.resize(ambient.grade_count());
        for (std::size_t k = 0; k < ambient.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            ambient_to_quotient_[k].assign(ambient.grade(grade).cell_count(), absent);
            for (std::size_t cell = 0; cell < ambient.grade(grade).cell_count(); ++cell) {
                if (sub_.grade(grade).get(cell) && !whole_.grade(grade).get(cell)) {
                    throw std::invalid_argument("relative subcomplex is not contained in whole complex");
                }
                if (whole_.grade(grade).get(cell) && !sub_.grade(grade).get(cell)) {
                    ambient_to_quotient_[k][cell] = static_cast<std::uint32_t>(quotient_cells_[k].size());
                    quotient_cells_[k].push_back(static_cast<std::uint32_t>(cell));
                }
            }
        }
    }

    [[nodiscard]] std::size_t cell_count(core::Grade grade) const { return quotient_cells_.at(grade.value).size(); }

    [[nodiscard]] numeric::RationalVector boundary_apply(core::Grade upper_grade,
                                                          const numeric::RationalVector& quotient_field) const {
        if (upper_grade.value == 0) throw std::invalid_argument("relative grade zero has no boundary");
        if (quotient_field.size() != cell_count(upper_grade)) throw std::invalid_argument("relative field size mismatch");
        const auto& upper_arena = ambient_->grade(upper_grade);
        numeric::RationalVector ambient_field(upper_arena.cell_count(), numeric::Rational{0});
        for (std::size_t q = 0; q < quotient_field.size(); ++q) {
            ambient_field[quotient_cells_[upper_grade.value][q]] = quotient_field[q];
        }
        const auto ambient_boundary = ambient_->boundary_apply(upper_grade, ambient_field);
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)};
        numeric::RationalVector result(cell_count(lower_grade), numeric::Rational{0});
        for (std::size_t q = 0; q < result.size(); ++q) result[q] = ambient_boundary[quotient_cells_[lower_grade.value][q]];
        return result;
    }

private:
    static constexpr std::uint32_t absent = static_cast<std::uint32_t>(-1);
    const core::RelationalComplex* ambient_{};
    SubcomplexMask whole_;
    SubcomplexMask sub_;
    std::vector<std::vector<std::uint32_t>> quotient_cells_;
    std::vector<std::vector<std::uint32_t>> ambient_to_quotient_;
};

} // namespace rfc::algebra
