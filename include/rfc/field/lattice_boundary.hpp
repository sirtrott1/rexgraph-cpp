#pragma once
#include "rfc/field/lattice_field.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace rfc::field {

class LatticeBoundaryOperator {
public:
    LatticeBoundaryOperator(const core::RelationalComplex& complex, core::Grade upper_grade)
        : complex_(&complex), upper_grade_(upper_grade) {
        if (upper_grade.value == 0 || upper_grade.value >= complex.grade_count()) throw std::out_of_range("invalid lattice boundary grade");
    }

    [[nodiscard]] core::Grade upper_grade() const noexcept { return upper_grade_; }
    [[nodiscard]] core::Grade lower_grade() const noexcept { return core::Grade{static_cast<std::uint16_t>(upper_grade_.value - 1u)}; }

    [[nodiscard]] ExactLatticeField apply(const ExactLatticeField& upper) const {
        require_field(upper.grade(), upper.complex());
        const auto& arena = complex_->grade(upper_grade_);
        std::vector<numeric::Integer> result(complex_->grade(lower_grade()).cell_count(), numeric::Integer{0});
        const auto exists_id = complex_->signature().existence_aspect();
        for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
            if (upper.values()[cell] == 0) continue;
            const auto [begin, end] = arena.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!arena.aspects.get(exists_id, p)) continue;
                result[arena.lower_index[p]] += arena.lattice_coefficient[p] * upper.values()[cell];
            }
        }
        return ExactLatticeField(*complex_, lower_grade(), std::move(result));
    }

    [[nodiscard]] ExactLatticeField transpose_apply(const ExactLatticeField& lower) const {
        if (&lower.complex() != complex_ || lower.grade() != lower_grade()) throw std::invalid_argument("lattice transpose field mismatch");
        const auto& arena = complex_->grade(upper_grade_);
        std::vector<numeric::Integer> result(arena.cell_count(), numeric::Integer{0});
        const auto exists_id = complex_->signature().existence_aspect();
        for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
            numeric::Integer acc = 0;
            const auto [begin, end] = arena.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!arena.aspects.get(exists_id, p)) continue;
                acc += arena.lattice_coefficient[p] * lower.values()[arena.lower_index[p]];
            }
            result[cell] = std::move(acc);
        }
        return ExactLatticeField(*complex_, upper_grade_, std::move(result));
    }

    [[nodiscard]] MachineLatticeField apply(const MachineLatticeField& upper) const {
        require_field(upper.grade(), upper.complex());
        std::vector<std::int64_t> result(complex_->grade(lower_grade()).cell_count(), 0);
        apply_batch_into(upper.values(), result, 1);
        return MachineLatticeField(*complex_, lower_grade(), std::move(result));
    }

    [[nodiscard]] MachineLatticeField transpose_apply(const MachineLatticeField& lower) const {
        if (&lower.complex() != complex_ || lower.grade() != lower_grade()) throw std::invalid_argument("machine lattice transpose field mismatch");
        std::vector<std::int64_t> result(complex_->grade(upper_grade_).cell_count(), 0);
        transpose_batch_into(lower.values(), result, 1);
        return MachineLatticeField(*complex_, upper_grade_, std::move(result));
    }

    // Buffers contain cells followed by contiguous independent sample columns.
    void apply_batch_into(std::span<const std::int64_t> upper,
                          std::span<std::int64_t> result, std::size_t samples) const {
        const auto& arena = complex_->grade(upper_grade_);
        require_buffers(upper, result, arena.cell_count(), complex_->grade(lower_grade()).cell_count(), samples);
        std::fill(result.begin(), result.end(), 0);
        const auto exists_id = complex_->signature().existence_aspect();
        for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
            const auto values = upper.subspan(cell * samples, samples);
            if (std::all_of(values.begin(), values.end(), [](auto v) { return v == 0; })) continue;
            const auto [begin, end] = arena.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!arena.aspects.get(exists_id, p)) continue;
                const auto coefficient = to_i64(arena.lattice_coefficient[p]);
                auto targets = result.subspan(arena.lower_index[p] * samples, samples);
                for (std::size_t b = 0; b < samples; ++b) {
                    if (values[b] != 0) targets[b] = numeric::checked_mac_i64(targets[b], coefficient, values[b]);
                }
            }
        }
    }

    void transpose_batch_into(std::span<const std::int64_t> lower,
                              std::span<std::int64_t> result, std::size_t samples) const {
        const auto& arena = complex_->grade(upper_grade_);
        require_buffers(lower, result, complex_->grade(lower_grade()).cell_count(), arena.cell_count(), samples);
        std::fill(result.begin(), result.end(), 0);
        const auto exists_id = complex_->signature().existence_aspect();
        for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
            auto targets = result.subspan(cell * samples, samples);
            const auto [begin, end] = arena.incidence_range(cell);
            for (std::size_t p = begin; p < end; ++p) {
                if (!arena.aspects.get(exists_id, p)) continue;
                const auto coefficient = to_i64(arena.lattice_coefficient[p]);
                const auto values = lower.subspan(arena.lower_index[p] * samples, samples);
                for (std::size_t b = 0; b < samples; ++b) {
                    targets[b] = numeric::checked_mac_i64(targets[b], coefficient, values[b]);
                }
            }
        }
    }

private:
    static void require_buffers(std::span<const std::int64_t> input, std::span<std::int64_t> output,
                                std::size_t inputs, std::size_t outputs, std::size_t samples) {
        if (!samples || inputs > std::numeric_limits<std::size_t>::max() / samples ||
            outputs > std::numeric_limits<std::size_t>::max() / samples ||
            input.size() != inputs * samples || output.size() != outputs * samples)
            throw std::invalid_argument("lattice batch buffer shape mismatch");
        const std::less<const std::int64_t*> before;
        if (!input.empty() && !output.empty() && before(input.data(), output.data() + output.size()) &&
            before(output.data(), input.data() + input.size()))
            throw std::invalid_argument("lattice batch buffers overlap");
    }
    static std::int64_t to_i64(const numeric::Integer& value) {
        if (!value.fits_slong_p()) throw std::overflow_error("lattice coefficient exceeds machine signed long");
        const auto raw = value.get_si();
        if constexpr (sizeof(long) > sizeof(std::int64_t)) {
            if (raw < std::numeric_limits<std::int64_t>::min() || raw > std::numeric_limits<std::int64_t>::max()) {
                throw std::overflow_error("lattice coefficient exceeds int64");
            }
        }
        return static_cast<std::int64_t>(raw);
    }
    void require_field(core::Grade grade, const core::RelationalComplex& complex) const {
        if (&complex != complex_ || grade != upper_grade_) throw std::invalid_argument("lattice boundary field mismatch");
    }

    const core::RelationalComplex* complex_{};
    core::Grade upper_grade_{};
};

} // namespace rfc::field
