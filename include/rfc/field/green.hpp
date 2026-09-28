#pragma once
#include "rfc/field/augmented_green.hpp"
#include "rfc/field/hodge.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace rfc::field {

class ResolventOperator {
public:
    ResolventOperator(const HodgeOperator& hodge, numeric::Rational lambda)
        : hodge_(&hodge), lambda_(std::move(lambda)) {}
    [[nodiscard]] std::size_t size() const noexcept { return hodge_->size(); }
    [[nodiscard]] const DiagonalMetric& metric() const noexcept { return hodge_->metric(); }
    [[nodiscard]] const HodgeOperator& hodge() const noexcept { return *hodge_; }
    [[nodiscard]] const numeric::Rational& lambda() const noexcept { return lambda_; }
    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& value) const {
        if (value.size() != size()) throw std::invalid_argument("resolvent size mismatch");
        return numeric::add(value, numeric::scale(lambda_, hodge_->apply(value)));
    }
private:
    const HodgeOperator* hodge_{};
    numeric::Rational lambda_{};
};

struct ExactSolveResult {
    numeric::RationalVector value;
    std::size_t iterations{};
    bool exact{false};
};

inline ExactSolveResult exact_cg(const ResolventOperator& operator_value,
                                 const numeric::RationalVector& source) {
    const auto solved=exact_green_apply(operator_value.hodge().exact_operator(),operator_value.lambda(),source);
    return {solved.value,solved.iterations,solved.exact};
}

} // namespace rfc::field
