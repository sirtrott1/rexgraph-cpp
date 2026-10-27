#pragma once

#include "rfc/local/cellular_sheaf.hpp"
#include "rfc/core/owned_source.hpp"
#include <memory>

namespace rfc::local {

// Freeze the retained source, intrinsic stalk dimensions, and the separately
// declared upward restrictions. Copies share this owner. Equal coordinates in
// independently frozen sources never establish an implicit correspondence.
// Snapshot construction is proportional to the represented source and maps;
// implicit identities remain implicit and unused stalk coordinates are metadata.
class OwnedLinearCellularSheaf {
    struct State {
        core::OwnedRelationalSource source_owner;
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
            : State(core::OwnedRelationalSource(original.complex()), original) {}
        State(core::OwnedRelationalSource owner, const LinearCellularSheaf& original)
            : source_owner(std::move(owner)),
              bundle(copy_bundle(source_owner.source(), original.stalks())), sheaf(bundle) {
            const auto& source = source_owner.source();
            for (std::size_t k = 1; k < source.grade_count(); ++k) {
                const core::Grade grade{static_cast<std::uint16_t>(k)};
                for (std::size_t slot = 0; slot < source.grade(grade).incidence_count(); ++slot)
                    if (const auto& map = original.restriction_override(grade, slot))
                        sheaf.set_restriction(grade, slot, *map);
            }
        }
    };
    std::shared_ptr<const State> state_;
    static std::shared_ptr<const State> share_checked(
        core::OwnedRelationalSource owner, const LinearCellularSheaf& original) {
        if (&owner.source() != &original.complex())
            throw std::invalid_argument("sheaf declaration must use the shared source owner");
        return std::make_shared<const State>(std::move(owner), original);
    }

public:
    explicit OwnedLinearCellularSheaf(const LinearCellularSheaf& original)
        : state_(std::make_shared<const State>(original)) {}
    // Share only an actual source anchor, never an equal independently built source.
    OwnedLinearCellularSheaf(core::OwnedRelationalSource owner,
                             const LinearCellularSheaf& original)
        : state_(share_checked(std::move(owner), original)) {}
    [[nodiscard]] const core::OwnedRelationalSource& source_owner() const & noexcept {
        return state_->source_owner;
    }
    const core::OwnedRelationalSource& source_owner() const && = delete;
    OwnedLinearCellularSheaf(const OwnedLinearCellularSheaf&) = default;
    OwnedLinearCellularSheaf& operator=(const OwnedLinearCellularSheaf&) = default;
    [[nodiscard]] const LinearCellularSheaf& sheaf() const & noexcept { return state_->sheaf; }
    const LinearCellularSheaf& sheaf() const && = delete;
    [[nodiscard]] bool same_owner(const OwnedLinearCellularSheaf& other) const noexcept {
        return state_ == other.state_;
    }
};

} // namespace rfc::local
