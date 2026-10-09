#pragma once
#include "rfc/numeric/exact.hpp"
#include <algorithm>
#include <cstddef>
#include <compare>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::numeric {

struct SparseRationalEntry {
    std::size_t index{};
    Rational value{0};
    // GMP rational comparisons are exact but neither constexpr nor three-way.
    // Spell out the intended lexicographic order rather than defaulting a
    // deleted spaceship operator (rejected by Clang for the constexpr case).
    bool operator==(const SparseRationalEntry&) const = default;
    std::strong_ordering operator<=>(const SparseRationalEntry& other) const {
        if (const auto order = index <=> other.index; order != 0) return order;
        if (value < other.value) return std::strong_ordering::less;
        if (value > other.value) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
};

// Non-owning execution view over canonical sparse rational coordinates.
// The owner is responsible for preserving sorted, unique, nonzero entries for
// the lifetime of the view.
class SparseRationalView {
public:
    SparseRationalView() = default;
    SparseRationalView(std::size_t logical_size, std::span<const SparseRationalEntry> entries)
        : logical_size_(logical_size), entries_(entries) {}

    [[nodiscard]] std::size_t size() const noexcept { return logical_size_; }
    [[nodiscard]] std::size_t nonzeros() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] std::span<const SparseRationalEntry> entries() const noexcept { return entries_; }

private:
    std::size_t logical_size_{};
    std::span<const SparseRationalEntry> entries_{};
};

// Sorted, unique, nonzero exact coordinates over a declared logical size.
// This is a numeric execution container; structural existence remains owned by
// a carrier/support object rather than inferred from numeric nonzeros.
class SparseRationalVector {
public:
    SparseRationalVector() = default;
    explicit SparseRationalVector(std::size_t logical_size) : logical_size_(logical_size) {}

    SparseRationalVector(std::size_t logical_size, std::vector<SparseRationalEntry> entries)
        : logical_size_(logical_size), entries_(canonicalize(logical_size, std::move(entries))) {}

    [[nodiscard]] std::size_t size() const noexcept { return logical_size_; }
    [[nodiscard]] std::size_t nonzeros() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] const std::vector<SparseRationalEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] SparseRationalView view() const noexcept { return SparseRationalView(logical_size_, entries_); }

    [[nodiscard]] RationalVector dense() const {
        RationalVector out(logical_size_, Rational{0});
        for (const auto& entry : entries_) out[entry.index] = entry.value;
        return out;
    }

    [[nodiscard]] static SparseRationalVector basis(std::size_t logical_size,
                                                     std::size_t index,
                                                     Rational value = Rational{1}) {
        if (index >= logical_size) throw std::out_of_range("sparse basis index");
        if (value == 0) return SparseRationalVector(logical_size);
        value.canonicalize();
        return SparseRationalVector(logical_size, {{index, std::move(value)}});
    }

private:
    static std::vector<SparseRationalEntry> canonicalize(std::size_t logical_size,
                                                          std::vector<SparseRationalEntry> entries) {
        bool canonical_order = true;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            auto& entry = entries[i];
            if (entry.index >= logical_size) throw std::out_of_range("sparse rational index");
            entry.value.canonicalize();
            canonical_order = canonical_order && entry.value != 0 &&
                              (i == 0 || entry.index > entries[i - 1].index);
        }
        if (canonical_order && entries.capacity() == entries.size()) return entries;
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.index < rhs.index;
        });

        std::vector<SparseRationalEntry> out;
        out.reserve(entries.size());
        for (auto& entry : entries) {
            if (entry.value == 0) continue;
            if (!out.empty() && out.back().index == entry.index) {
                out.back().value += entry.value;
                out.back().value.canonicalize();
                if (out.back().value == 0) out.pop_back();
            } else {
                out.push_back(std::move(entry));
            }
        }
        return out;
    }

    std::size_t logical_size_{};
    std::vector<SparseRationalEntry> entries_;
};

} // namespace rfc::numeric
