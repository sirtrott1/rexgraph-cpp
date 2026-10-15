#pragma once

#include "rfc/local/cellular_sheaf.hpp"
#include <memory>

namespace rfc::local {

// Freeze the retained source, intrinsic stalk dimensions, and the separately
// declared upward restrictions. Copies share this owner. Equal coordinates in
// independently frozen sources never establish an implicit correspondence.
// Snapshot construction is proportional to the represented source and maps;
// implicit identities remain implicit and unused stalk coordinates are metadata.
class OwnedLinearCellularSheaf {
    struct State {
        core::RelationalComplex source;
        LinearStalkBundle bundle;
        LinearCellularSheaf sheaf;

        static LinearStalkBundle copy_bundle(const core::RelationalComplex& source,
                                            const LinearStalkBundle& original) {
            LinearStalkBundle result(source);
            for (std::size_t k = 0; k < source.grade_count(); ++k) {
                const core::Grade grade{static_cast<std::uint16_t>(k)};
                for (std::size_t i = 0; i < source.grade(grade).cell_count(); ++i) {
                    const core::CellId cell{grade, static_cast<std::uint32_t>(i)};
                    const auto n = original.dimension(cell);
                    if (n != 1u) result.set_dimension(cell, n);
                }
            }
            return result;
        }

        explicit State(const LinearCellularSheaf& original)
            : source(original.complex()), bundle(copy_bundle(source, original.stalks())),
              sheaf(bundle) {
            for (std::size_t k = 1; k < source.grade_count(); ++k) {
                const core::Grade grade{static_cast<std::uint16_t>(k)};
                for (std::size_t slot = 0; slot < source.grade(grade).incidence_count(); ++slot)
                    if (const auto& map = original.restriction_override(grade, slot))
                        sheaf.set_restriction(grade, slot, *map);
            }
        }
    };
    std::shared_ptr<const State> state_;

public:
    explicit OwnedLinearCellularSheaf(const LinearCellularSheaf& original)
        : state_(std::make_shared<const State>(original)) {}
    OwnedLinearCellularSheaf(const OwnedLinearCellularSheaf&) = default;
    OwnedLinearCellularSheaf& operator=(const OwnedLinearCellularSheaf&) = default;
    [[nodiscard]] const LinearCellularSheaf& sheaf() const & noexcept { return state_->sheaf; }
    const LinearCellularSheaf& sheaf() const && = delete;
    [[nodiscard]] bool same_owner(const OwnedLinearCellularSheaf& other) const noexcept {
        return state_ == other.state_;
    }
};

} // namespace rfc::local
