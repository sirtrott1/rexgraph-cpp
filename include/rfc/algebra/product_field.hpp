#pragma once
#include "rfc/core/identity.hpp"
#include "rfc/numeric/exact.hpp"
#include <algorithm>
#include <cstddef>
#include <compare>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::algebra {

// Native identity of one cell in an external relational product.  The pair is
// the identity.  No flattened Cartesian coordinate is required for execution.
struct ProductCell {
    core::CellId left{};
    core::CellId right{};

    [[nodiscard]] core::Grade grade() const {
        const auto sum = static_cast<std::uint32_t>(left.grade.value) +
                         static_cast<std::uint32_t>(right.grade.value);
        if (sum > std::numeric_limits<std::uint16_t>::max()) {
            throw std::overflow_error("product cell grade overflow");
        }
        return core::Grade{static_cast<std::uint16_t>(sum)};
    }

    constexpr auto operator<=>(const ProductCell&) const = default;
};

// Stable source identity of a factor cell.  Grade zero keys identify derived
// shared entities; positive grades identify asserted primary relations.  Grade
// keeps those two source namespaces distinct without hashing or flattening.
struct ProductFactorIdentity {
    core::Grade grade{};
    std::uint64_t key{};
    constexpr auto operator<=>(const ProductFactorIdentity&) const = default;
};

struct ProductCellIdentity {
    ProductFactorIdentity left{};
    ProductFactorIdentity right{};
    constexpr auto operator<=>(const ProductCellIdentity&) const = default;
};

enum class ProductBoundarySide : std::uint8_t {
    Left,
    Right,
};

// One lazy product-boundary occurrence.  The original factor occurrence is
// retained by identity, and the Koszul orientation flip is represented
// separately rather than being lost in a flattened signed coordinate.
struct ProductBoundaryTerm {
    ProductCell lower{};
    numeric::Rational coefficient{0};
    ProductBoundarySide side{ProductBoundarySide::Left};
    core::OccurrenceKey factor_occurrence{};
    bool koszul_flip{};
};

struct SparseProductEntry {
    ProductCell cell{};
    numeric::Rational value{0};
    // GMP rational comparisons are exact but neither constexpr nor three-way.
    // Spell out the intended lexicographic order rather than defaulting a
    // deleted spaceship operator (rejected by Clang for the constexpr case).
    bool operator==(const SparseProductEntry&) const = default;
    std::strong_ordering operator<=>(const SparseProductEntry& other) const {
        if (const auto order = cell <=> other.cell; order != 0) return order;
        if (value < other.value) return std::strong_ordering::less;
        if (value > other.value) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
};

class SparseProductFieldView {
public:
    SparseProductFieldView() = default;
    SparseProductFieldView(core::Grade grade, std::span<const SparseProductEntry> entries)
        : grade_(grade), entries_(entries) {}

    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] std::size_t nonzeros() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] std::span<const SparseProductEntry> entries() const noexcept { return entries_; }

private:
    core::Grade grade_{};
    std::span<const SparseProductEntry> entries_{};
};

// Exact sparse state on native product-cell identities.  This container has no
// ambient Cartesian size and therefore cannot accidentally allocate or scan the
// complete product coordinate universe.
class SparseProductField {
public:
    SparseProductField() = default;
    explicit SparseProductField(core::Grade grade) : grade_(grade) {}

    SparseProductField(core::Grade grade, std::vector<SparseProductEntry> entries)
        : grade_(grade), entries_(canonicalize(grade, std::move(entries))) {}

    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] std::size_t nonzeros() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] const std::vector<SparseProductEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] SparseProductFieldView view() const noexcept { return SparseProductFieldView(grade_, entries_); }

    [[nodiscard]] static SparseProductField basis(ProductCell cell,
                                                   numeric::Rational value = numeric::Rational{1}) {
        const auto grade = cell.grade();
        value.canonicalize();
        if (value == 0) return SparseProductField(grade);
        return SparseProductField(grade, {{cell, std::move(value)}});
    }

private:
    static std::vector<SparseProductEntry> canonicalize(core::Grade grade,
                                                         std::vector<SparseProductEntry> entries) {
        for (auto& entry : entries) {
            if (entry.cell.grade() != grade) {
                throw std::invalid_argument("product field entry grade mismatch");
            }
            entry.value.canonicalize();
        }
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.cell < rhs.cell;
        });

        std::vector<SparseProductEntry> out;
        out.reserve(entries.size());
        for (auto& entry : entries) {
            if (entry.value == 0) continue;
            if (!out.empty() && out.back().cell == entry.cell) {
                out.back().value += entry.value;
                out.back().value.canonicalize();
                if (out.back().value == 0) out.pop_back();
            } else {
                out.push_back(std::move(entry));
            }
        }
        return out;
    }

    core::Grade grade_{};
    std::vector<SparseProductEntry> entries_;
};

} // namespace rfc::algebra
