#pragma once
#include "rfc/field/lattice_field.hpp"
#include "rfc/learning/integer_network.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::learning {

struct MachineSemanticTerm {
    std::uint32_t target{};
    std::int64_t coefficient{};
};

class CompiledMachineSemanticMap {
public:
    explicit CompiledMachineSemanticMap(const geometry::SemanticScaleRealization& scale)
        : scale_(&scale) {
        const auto& map = scale.realization();
        compiled_.resize(map.source().grade_count());
        for (std::size_t k = 0; k < map.source().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            compiled_[k].resize(map.source().grade(grade).cell_count());
            for (std::size_t source_cell = 0; source_cell < compiled_[k].size(); ++source_cell) {
                const auto& source_scale = map.source().grade(grade).cell_scales.at(source_cell);
                for (const auto& term : map.image(grade, source_cell)) {
                    const auto& target_scale = map.target().grade(grade).cell_scales.at(term.target);
                    numeric::Rational lattice_coefficient = term.coefficient * source_scale / target_scale;
                    lattice_coefficient.canonicalize();
                    if (lattice_coefficient.get_den() != 1) throw std::domain_error("semantic map is exact but not integral in retained lattice coordinates");
                    const auto integer = lattice_coefficient.get_num();
                    if (!integer.fits_slong_p()) throw std::overflow_error("semantic lattice coefficient exceeds signed long");
                    const auto raw = integer.get_si();
                    if constexpr (sizeof(long) > sizeof(std::int64_t)) {
                        if (raw < std::numeric_limits<std::int64_t>::min() || raw > std::numeric_limits<std::int64_t>::max()) {
                            throw std::overflow_error("semantic lattice coefficient exceeds int64");
                        }
                    }
                    compiled_[k][source_cell].push_back(MachineSemanticTerm{term.target, static_cast<std::int64_t>(raw)});
                }
            }
        }
    }

    [[nodiscard]] const geometry::SemanticScaleRealization& scale() const noexcept { return *scale_; }

    [[nodiscard]] field::MachineLatticeField push(const field::MachineLatticeField& source) const {
        const auto& map = scale_->realization();
        if (&source.complex() != &map.source()) throw std::invalid_argument("semantic machine source complex mismatch");
        const auto grade = source.grade();
        std::vector<std::int64_t> target(map.target().grade(grade).cell_count(), 0);
        for (std::size_t source_cell = 0; source_cell < source.values().size(); ++source_cell) {
            if (source.values()[source_cell] == 0) continue;
            for (const auto& term : compiled_.at(grade.value).at(source_cell)) {
                target[term.target] = numeric::checked_mac_i64(target[term.target], term.coefficient, source.values()[source_cell]);
            }
        }
        return field::MachineLatticeField(map.target(), grade, std::move(target));
    }

private:
    const geometry::SemanticScaleRealization* scale_{};
    std::vector<std::vector<std::vector<MachineSemanticTerm>>> compiled_;
};

class SemanticScaleIntegerStage {
public:
    SemanticScaleIntegerStage(CompiledMachineSemanticMap realization,
                              CompiledIntegerRelationalLayer micro_layer)
        : realization_(std::move(realization)), micro_layer_(std::move(micro_layer)) {}

    [[nodiscard]] field::MachineLatticeField forward(const field::MachineLatticeField& macro_input) const {
        return micro_layer_.forward(realization_.push(macro_input));
    }

private:
    CompiledMachineSemanticMap realization_;
    CompiledIntegerRelationalLayer micro_layer_;
};

} // namespace rfc::learning
