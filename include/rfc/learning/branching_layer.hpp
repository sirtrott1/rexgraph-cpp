#pragma once
#include "rfc/local/tensor_field.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace rfc::learning {

// Exact branch-aware gain layer.  A trainable gain acts on the retained upper
// relation stalk before any boundary fan-out, preserving the defining fact
// that all branch legs are projections of one primary relation state.
class BranchTypeGainLayer {
public:
    BranchTypeGainLayer(const local::RelationalFiberSystem& fibers, core::Grade grade)
        : fibers_(&fibers), grade_(grade), tensor_(fibers, grade) {}

    void set_gain(core::RelationTypeId type, numeric::Rational gain) {
        gain.canonicalize();
        gains_[type] = std::move(gain);
    }

    [[nodiscard]] numeric::Rational gain(core::RelationTypeId type) const {
        const auto it = gains_.find(type);
        return it == gains_.end() ? numeric::Rational{1} : it->second;
    }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& upper_field) const {
        if (upper_field.size() != tensor_.source_dimension()) throw std::invalid_argument("branch gain layer source field size mismatch");
        auto scaled = upper_field;
        const auto offsets = fibers_->stalks().offsets(grade_);
        const auto& arena = fibers_->complex().grade(grade_);
        for (std::size_t relation = 0; relation < arena.cell_count(); ++relation) {
            const auto g = gain(arena.relation_types.at(relation));
            for (std::size_t j = offsets[relation]; j < offsets[relation + 1u]; ++j) scaled[j] *= g;
        }
        return tensor_.apply(scaled);
    }

private:
    const local::RelationalFiberSystem* fibers_{};
    core::Grade grade_{};
    local::GradeTensorField tensor_;
    std::unordered_map<core::RelationTypeId, numeric::Rational> gains_;
};

} // namespace rfc::learning
