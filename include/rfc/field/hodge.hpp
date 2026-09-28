#pragma once
#include "rfc/field/exact_hodge.hpp"
#include "rfc/field/metric.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::field {

// Diagonal compatibility facade over the canonical ExactHodgeOperator.
class HodgeOperator {
public:
    HodgeOperator(const core::RelationalComplex& complex,
                  core::Grade grade,
                  const DiagonalMetric& metric_below,
                  const DiagonalMetric& metric_here,
                  const DiagonalMetric* metric_above = nullptr)
        : metric_here_(&metric_here),
          metrics_(make_metrics(complex,grade,metric_below,metric_here,metric_above)),
          exact_(complex,grade,metrics_) {
        if (grade.value == 0 || grade.value >= complex.grade_count()) throw std::out_of_range("HodgeOperator currently requires an interior/nonzero grade");
    }

    [[nodiscard]] std::size_t size() const { return exact_.size(); }
    [[nodiscard]] const DiagonalMetric& metric() const noexcept { return *metric_here_; }
    [[nodiscard]] const ExactHodgeOperator& exact_operator() const noexcept { return exact_; }
    [[nodiscard]] numeric::RationalVector apply_down(const numeric::RationalVector& value) const { return exact_.apply_down(value); }
    [[nodiscard]] numeric::RationalVector apply_up(const numeric::RationalVector& value) const { return exact_.apply_up(value); }
    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& value) const { return exact_.apply(value); }

private:
    static std::vector<ExactMetric> make_metrics(const core::RelationalComplex& complex,
                                                  core::Grade grade,
                                                  const DiagonalMetric& below,
                                                  const DiagonalMetric& here,
                                                  const DiagonalMetric* above) {
        if (grade.value == 0 || grade.value >= complex.grade_count()) throw std::out_of_range("HodgeOperator currently requires an interior/nonzero grade");
        if (below.size()!=complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value-1u)}).cell_count() ||
            here.size()!=complex.grade(grade).cell_count()) throw std::invalid_argument("Hodge metric size mismatch");
        if (grade.value<complex.max_grade().value) {
            if (!above || above->size()!=complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value+1u)}).cell_count()) {
                throw std::invalid_argument("Hodge upper metric size mismatch");
            }
        }
        std::vector<ExactMetric> metrics;
        metrics.reserve(complex.grade_count());
        for(std::size_t k=0;k<complex.grade_count();++k){
            const auto n=complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
            metrics.push_back(ExactMetric::identity(n));
        }
        metrics[grade.value-1u]=below.exact();
        metrics[grade.value]=here.exact();
        if(grade.value<complex.max_grade().value) metrics[grade.value+1u]=above->exact();
        return metrics;
    }

    const DiagonalMetric* metric_here_{};
    std::vector<ExactMetric> metrics_;
    ExactHodgeOperator exact_;
};

} // namespace rfc::field
