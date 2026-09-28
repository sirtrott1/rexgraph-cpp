#pragma once
#include "rfc/core/grade.hpp"
#include "rfc/geometry/chain_map.hpp"
#include "rfc/geometry/contract.hpp"
#include <optional>
#include <stdexcept>
#include <utility>

namespace rfc::geometry {

class SemanticScaleRealization {
public:
    [[nodiscard]] static constexpr GeometryDescriptor geometry_descriptor() noexcept {
        return {GeometryKind::Semantic, "exact semantic chain realization", true};
    }

    SemanticScaleRealization(core::SemanticScale macro_scale,
                             core::SemanticScale micro_scale,
                             RelationalChainMap realization,
                             std::optional<RelationalChainMap> coarsening = std::nullopt)
        : macro_scale_(macro_scale), micro_scale_(micro_scale),
          realization_(std::move(realization)), coarsening_(std::move(coarsening)) {
        if (macro_scale_.value <= micro_scale_.value) throw std::invalid_argument("semantic realization must map coarser scale into finer scale");
        if (realization_.source().semantic_scale() != macro_scale_ || realization_.target().semantic_scale() != micro_scale_) {
            throw std::invalid_argument("semantic realization complex scales do not match declaration");
        }
        if (!realization_.is_chain_map()) throw std::invalid_argument("semantic realization must commute with boundary");
        if (coarsening_) {
            if (&coarsening_->source() != &realization_.target() || &coarsening_->target() != &realization_.source()) {
                throw std::invalid_argument("semantic coarsening source/target mismatch");
            }
            if (!coarsening_->is_chain_map()) throw std::invalid_argument("semantic coarsening must commute with boundary");
            const auto roundtrip = compose_chain_maps(realization_, *coarsening_);
            const auto identity = identity_chain_map(realization_.source());
            if (!same_chain_map(roundtrip, identity)) throw std::invalid_argument("semantic coarsening must retract realization");
        }
    }

    [[nodiscard]] core::SemanticScale macro_scale() const noexcept { return macro_scale_; }
    [[nodiscard]] core::SemanticScale micro_scale() const noexcept { return micro_scale_; }
    [[nodiscard]] const RelationalChainMap& realization() const noexcept { return realization_; }
    [[nodiscard]] bool has_coarsening() const noexcept { return coarsening_.has_value(); }
    [[nodiscard]] const RelationalChainMap& coarsening() const {
        if (!coarsening_) throw std::logic_error("semantic realization has no coarsening");
        return *coarsening_;
    }

private:
    core::SemanticScale macro_scale_{};
    core::SemanticScale micro_scale_{};
    RelationalChainMap realization_;
    std::optional<RelationalChainMap> coarsening_;
};

} // namespace rfc::geometry
