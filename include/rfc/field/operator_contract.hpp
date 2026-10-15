#pragma once
#include "rfc/core/grade.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstdint>

namespace rfc::field {

enum class OperatorKind : std::uint8_t {
    Boundary,
    Adjoint,
    HodgeDown,
    HodgeUp,
    Hodge,
    Dirac,
    Green,
    HarmonicProjector,
    HarmonicInverse,
    GradientPotential
};

struct OperatorRequest {
    OperatorKind kind{OperatorKind::Hodge};
    core::Grade grade{};
    numeric::Rational lambda{0};
};

enum class ExactTowerKind : std::uint8_t {
    Unweighted,
    Weighted
};

} // namespace rfc::field
