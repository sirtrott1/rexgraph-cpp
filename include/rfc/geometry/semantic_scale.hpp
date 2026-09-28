#pragma once
#include "rfc/algebra/morphism.hpp"
#include "rfc/core/grade.hpp"
#include "rfc/geometry/contract.hpp"
#include <stdexcept>
#include <utility>

namespace rfc::geometry {

class SemanticScaleMap {
public:
    [[nodiscard]] static constexpr GeometryDescriptor geometry_descriptor() noexcept {
        return {GeometryKind::Semantic, "strict semantic-scale realization", true};
    }

    SemanticScaleMap(core::SemanticScale macro_scale,
                     core::SemanticScale micro_scale,
                     algebra::RelationalMorphism realization)
        : macro_scale_(macro_scale), micro_scale_(micro_scale), realization_(std::move(realization)) {
        if (macro_scale_.value <= micro_scale_.value) {
            throw std::invalid_argument("semantic realization must map a coarser scale into a finer scale");
        }
        if (!realization_.is_strict()) throw std::invalid_argument("semantic realization must preserve relational structure");
        if (!realization_.is_chain_map()) throw std::invalid_argument("semantic realization must commute with boundary");
    }

    [[nodiscard]] core::SemanticScale macro_scale() const noexcept { return macro_scale_; }
    [[nodiscard]] core::SemanticScale micro_scale() const noexcept { return micro_scale_; }
    [[nodiscard]] const algebra::RelationalMorphism& realization() const noexcept { return realization_; }

private:
    core::SemanticScale macro_scale_{};
    core::SemanticScale micro_scale_{};
    algebra::RelationalMorphism realization_;
};

} // namespace rfc::geometry
