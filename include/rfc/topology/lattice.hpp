#pragma once
#include "rfc/core/relational_complex.hpp"
#include <cstddef>
#include <cstdint>

namespace rfc::topology {

class IntegralLatticeView {
public:
    explicit IntegralLatticeView(const core::RelationalComplex& complex) : complex_(&complex) {}

    [[nodiscard]] const numeric::Integer& coefficient(core::Grade upper_grade, std::size_t incidence) const {
        return complex_->grade(upper_grade).lattice_coefficient.at(incidence);
    }
    [[nodiscard]] const numeric::Rational& cell_scale(core::CellId cell) const {
        return complex_->grade(cell.grade).cell_scales.at(cell.index);
    }

private:
    const core::RelationalComplex* complex_{};
};

} // namespace rfc::topology
