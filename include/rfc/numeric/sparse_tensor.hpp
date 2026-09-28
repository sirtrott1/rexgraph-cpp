#pragma once
#include "rfc/numeric/index_domain.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/numeric/structural_support.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

namespace rfc::numeric {

// Exact tensor coordinate state with three independent layers:
//   1. declared shape/index domain,
//   2. structural support (which coordinates exist),
//   3. sparse nonzero numerical state on that support.
// A supported coordinate with no stored value is exactly zero, not absent.
class SparseRationalTensor {
public:
    SparseRationalTensor(IndexDomain domain,
                         StructuralSupport support,
                         SparseRationalVector values)
        : domain_(std::move(domain)), support_(std::move(support)), values_(std::move(values)) {
        if (support_.size() != domain_.logical_size()) throw std::invalid_argument("tensor support/domain size mismatch");
        if (values_.size() != domain_.logical_size()) throw std::invalid_argument("tensor values/domain size mismatch");
        for (const auto& entry : values_.entries()) {
            if (!support_.contains(entry.index)) throw std::invalid_argument("tensor value lies outside structural support");
        }
    }

    [[nodiscard]] static SparseRationalTensor zero(IndexDomain domain,
                                                    StructuralSupport support) {
        const auto size = domain.logical_size();
        return SparseRationalTensor(std::move(domain), std::move(support), SparseRationalVector(size));
    }

    [[nodiscard]] const IndexDomain& domain() const noexcept { return domain_; }
    [[nodiscard]] const StructuralSupport& support() const noexcept { return support_; }
    [[nodiscard]] const SparseRationalVector& values() const noexcept { return values_; }
    [[nodiscard]] std::size_t stored_nonzeros() const noexcept { return values_.nonzeros(); }

    // nullopt means structural absence; Rational{0} means an existing coordinate
    // with zero numerical state.
    [[nodiscard]] std::optional<Rational> state(std::size_t index) const {
        if (!support_.contains(index)) return std::nullopt;
        const auto& entries = values_.entries();
        const auto it = std::lower_bound(entries.begin(), entries.end(), index,
                                         [](const SparseRationalEntry& entry, std::size_t key) {
                                             return entry.index < key;
                                         });
        if (it == entries.end() || it->index != index) return Rational{0};
        return it->value;
    }

private:
    IndexDomain domain_;
    StructuralSupport support_;
    SparseRationalVector values_;
};

} // namespace rfc::numeric
