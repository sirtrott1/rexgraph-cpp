#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

class ExactLatticeField {
public:
    ExactLatticeField(const core::RelationalComplex& complex,
                      core::Grade grade,
                      std::vector<numeric::Integer> values)
        : complex_(&complex), grade_(grade), values_(std::move(values)) {
        if (values_.size() != complex.grade(grade).cell_count()) throw std::invalid_argument("lattice field size mismatch");
    }
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const std::vector<numeric::Integer>& values() const noexcept { return values_; }
    [[nodiscard]] std::vector<numeric::Integer>& values() noexcept { return values_; }
private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    std::vector<numeric::Integer> values_;
};

class MachineLatticeField {
public:
    MachineLatticeField(const core::RelationalComplex& complex,
                        core::Grade grade,
                        std::vector<std::int64_t> values)
        : complex_(&complex), grade_(grade), values_(std::move(values)) {
        if (values_.size() != complex.grade(grade).cell_count()) throw std::invalid_argument("machine lattice field size mismatch");
    }
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const std::vector<std::int64_t>& values() const noexcept { return values_; }
    [[nodiscard]] std::vector<std::int64_t>& values() noexcept { return values_; }
private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    std::vector<std::int64_t> values_;
};

} // namespace rfc::field
