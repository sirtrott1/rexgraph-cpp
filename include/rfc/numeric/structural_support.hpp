#pragma once
#include "rfc/machine/packed_bits.hpp"
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace rfc::numeric {

// Structural existence over a declared index domain.  This is intentionally
// independent of numerical state: a supported coordinate may carry value zero.
class StructuralSupport {
public:
    enum class Storage {
        Full,
        SparseIndices,
        PackedBits,
    };

    [[nodiscard]] static StructuralSupport full(std::size_t logical_size) {
        return StructuralSupport(logical_size, FullTag{});
    }

    [[nodiscard]] static StructuralSupport sparse(std::size_t logical_size,
                                                   std::vector<std::size_t> active) {
        std::sort(active.begin(), active.end());
        active.erase(std::unique(active.begin(), active.end()), active.end());
        for (const auto index : active) {
            if (index >= logical_size) throw std::out_of_range("structural support index");
        }
        return StructuralSupport(logical_size, std::move(active));
    }

    [[nodiscard]] static StructuralSupport packed(std::size_t logical_size,
                                                   const std::vector<std::size_t>& active) {
        machine::PackedBits bits(logical_size);
        for (const auto index : active) {
            if (index >= logical_size) throw std::out_of_range("structural support index");
            bits.set(index);
        }
        return StructuralSupport(logical_size, std::move(bits));
    }

    [[nodiscard]] std::size_t size() const noexcept { return logical_size_; }

    [[nodiscard]] Storage storage() const noexcept {
        if (std::holds_alternative<FullTag>(storage_)) return Storage::Full;
        if (std::holds_alternative<std::vector<std::size_t>>(storage_)) return Storage::SparseIndices;
        return Storage::PackedBits;
    }

    [[nodiscard]] std::size_t count() const noexcept {
        if (const auto* sparse = std::get_if<std::vector<std::size_t>>(&storage_)) return sparse->size();
        if (const auto* packed = std::get_if<machine::PackedBits>(&storage_)) return packed->count();
        return logical_size_;
    }

    [[nodiscard]] bool contains(std::size_t index) const {
        if (index >= logical_size_) throw std::out_of_range("structural support index");
        if (std::holds_alternative<FullTag>(storage_)) return true;
        if (const auto* sparse = std::get_if<std::vector<std::size_t>>(&storage_)) {
            return std::binary_search(sparse->begin(), sparse->end(), index);
        }
        return std::get<machine::PackedBits>(storage_).get(index);
    }

    template <class Fn>
    void for_each_active(Fn&& fn) const {
        if (std::holds_alternative<FullTag>(storage_)) {
            for (std::size_t index = 0; index < logical_size_; ++index) fn(index);
            return;
        }
        if (const auto* sparse = std::get_if<std::vector<std::size_t>>(&storage_)) {
            for (const auto index : *sparse) fn(index);
            return;
        }
        std::get<machine::PackedBits>(storage_).for_each_set(std::forward<Fn>(fn));
    }

    [[nodiscard]] const std::vector<std::size_t>* sparse_indices() const noexcept {
        return std::get_if<std::vector<std::size_t>>(&storage_);
    }

    [[nodiscard]] const machine::PackedBits* packed_bits() const noexcept {
        return std::get_if<machine::PackedBits>(&storage_);
    }

private:
    struct FullTag {};
    using StorageVariant = std::variant<FullTag, std::vector<std::size_t>, machine::PackedBits>;

    StructuralSupport(std::size_t logical_size, FullTag tag)
        : logical_size_(logical_size), storage_(tag) {}
    StructuralSupport(std::size_t logical_size, std::vector<std::size_t> sparse)
        : logical_size_(logical_size), storage_(std::move(sparse)) {}
    StructuralSupport(std::size_t logical_size, machine::PackedBits packed)
        : logical_size_(logical_size), storage_(std::move(packed)) {}

    std::size_t logical_size_{};
    StorageVariant storage_{FullTag{}};
};

} // namespace rfc::numeric
