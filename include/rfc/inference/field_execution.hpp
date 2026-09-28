#pragma once
#include "rfc/approximation/tower.hpp"
#include "rfc/field/execution_tower.hpp"
#include <cstdint>
#include <stdexcept>
#include <variant>
#include <vector>

namespace rfc::inference {

enum class ExecutionTower : std::uint8_t {
    UnweightedExact,
    WeightedExact,
    Approximate
};

struct FieldExecutionRequest {
    ExecutionTower tower{ExecutionTower::UnweightedExact};
    field::OperatorRequest op{};
};

using FieldExecutionValue = std::variant<numeric::RationalVector, approximation::Vector>;

inline FieldExecutionValue execute_field(const core::RelationalComplex& complex,
                                         const std::vector<field::ExactMetric>& weighted_metrics,
                                         const FieldExecutionRequest& request,
                                         const numeric::RationalVector& exact_input) {
    switch(request.tower) {
        case ExecutionTower::UnweightedExact: {
            const auto tower=field::ExactExecutionTower::unweighted(complex);
            return tower.apply(request.op,exact_input);
        }
        case ExecutionTower::WeightedExact: {
            const auto tower=field::ExactExecutionTower::weighted(complex,weighted_metrics);
            return tower.apply(request.op,exact_input);
        }
        case ExecutionTower::Approximate: {
            const auto tower=approximation::ApproximationTower::from_exact(complex,weighted_metrics);
            return tower.apply(request.op,approximation::to_approximate(exact_input));
        }
    }
    throw std::logic_error("unknown execution tower");
}

} // namespace rfc::inference
