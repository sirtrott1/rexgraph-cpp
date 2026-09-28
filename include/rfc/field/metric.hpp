#pragma once
#include "rfc/field/exact_metric.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

// Compatibility/ergonomic diagonal metric specialization. The canonical exact
// metric implementation is ExactMetric; this class only preserves the older
// diagonal API and exposes its ExactMetric realization.
class DiagonalMetric {
public:
    explicit DiagonalMetric(std::vector<numeric::Rational> diagonal)
        : diagonal_(std::move(diagonal)), exact_(ExactMetric::diagonal(diagonal_)) {}

    static DiagonalMetric identity(std::size_t size) {
        return DiagonalMetric(std::vector<numeric::Rational>(size, numeric::Rational{1}));
    }

    [[nodiscard]] std::size_t size() const noexcept { return diagonal_.size(); }
    [[nodiscard]] const numeric::Rational& operator[](std::size_t index) const { return diagonal_.at(index); }
    [[nodiscard]] const ExactMetric& exact() const noexcept { return exact_; }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& value) const { return exact_.apply(value); }
    [[nodiscard]] numeric::RationalVector inverse_apply(const numeric::RationalVector& value) const { return exact_.solve(value); }
    [[nodiscard]] numeric::Rational inner(const numeric::RationalVector& lhs,
                                          const numeric::RationalVector& rhs) const { return exact_.inner(lhs,rhs); }

private:
    std::vector<numeric::Rational> diagonal_;
    ExactMetric exact_;
};

} // namespace rfc::field
