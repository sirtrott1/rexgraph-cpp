#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/local/carrier.hpp"
#include "rfc/local/stalk.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::local {

class LinearStalkBundle {
public:
    explicit LinearStalkBundle(const core::RelationalComplex& complex, std::size_t default_dimension = 1)
        : carrier_(complex) {
        if (default_dimension == 0) throw std::invalid_argument("stalk dimension must be positive");
        dimensions_.reserve(complex.grade_count());
        for (std::size_t k = 0; k < complex.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            dimensions_.push_back(std::vector<std::size_t>(complex.grade(grade).cell_count(), default_dimension));
        }
    }

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return carrier_.complex(); }

    void set_dimension(core::CellId cell, std::size_t dimension) {
        if (dimension == 0) throw std::invalid_argument("stalk dimension must be positive");
        dimensions_.at(cell.grade.value).at(cell.index) = dimension;
    }

    [[nodiscard]] std::size_t dimension(core::CellId cell) const {
        return dimensions_.at(cell.grade.value).at(cell.index);
    }

    [[nodiscard]] LinearStalk stalk(core::CellId cell) const { return LinearStalk{cell, dimension(cell)}; }

    [[nodiscard]] std::vector<std::size_t> offsets(core::Grade grade) const {
        const auto& dims = dimensions_.at(grade.value);
        std::vector<std::size_t> result(dims.size() + 1u, 0u);
        for (std::size_t i = 0; i < dims.size(); ++i) result[i + 1u] = result[i] + dims[i];
        return result;
    }

    [[nodiscard]] std::size_t total_dimension(core::Grade grade) const {
        const auto offs = offsets(grade);
        return offs.back();
    }

private:
    RelationalLocalCarrier carrier_;
    std::vector<std::vector<std::size_t>> dimensions_;
};

// Compatibility alias.  Prefer LinearStalkBundle in new code.
using StalkBundle = LinearStalkBundle;

} // namespace rfc::local
