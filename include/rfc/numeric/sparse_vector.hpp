#pragma once
#include "rfc/numeric/exact.hpp"
#include <algorithm>
#include <cstddef>
#include <compare>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::storage::temporal_codec_detail { class Reader; }

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

class SparseRationalCursor {
public:
    virtual ~SparseRationalCursor() = default;
    virtual bool next(SparseRationalEntry& entry) = 0;
};

// An immutable value source admitted by the canonical reader. This interface
// owns numerical coordinates only; tensor support and carriers remain separate.
class RetainedSparseRationalSource {
public:
    virtual ~RetainedSparseRationalSource() = default;
    virtual std::size_t nonzeros() const noexcept = 0;
    virtual std::unique_ptr<SparseRationalCursor> open() const = 0;
    virtual void indices(const std::function<void(std::size_t)>& function) const = 0;
    virtual Rational coefficient(std::size_t index) const = 0;
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
    [[nodiscard]] std::size_t nonzeros() const noexcept { return retained_ ? retained_->nonzeros() : entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return nonzeros() == 0; }
    [[nodiscard]] const std::vector<SparseRationalEntry>& entries() const {
        if (!retained_) return entries_;
        std::call_once(cache_->once, [&] {
            std::vector<SparseRationalEntry> entries;
            entries.reserve(nonzeros());
            for_each_nonzero([&](std::size_t index, const Rational& value) { entries.push_back({index, value}); });
            cache_->entries = std::move(entries);
            cache_->ready.store(true, std::memory_order_release);
        });
        return cache_->entries;
    }
    [[nodiscard]] SparseRationalView view() const { return SparseRationalView(logical_size_, entries()); }
    [[nodiscard]] std::shared_ptr<const RetainedSparseRationalSource> retained_source() const noexcept { return retained_; }
    // Observe allocation state without causing a decode. Copies share only
    // immutable admitted values and their immutable materialization cache.
    [[nodiscard]] const std::vector<SparseRationalEntry>* materialized_entries() const noexcept {
        if (!retained_) return &entries_;
        return cache_->ready.load(std::memory_order_acquire) ? &cache_->entries : nullptr;
    }
    template<class Function> void for_each_nonzero(Function&& function) const {
        if (!retained_) {
            for (const auto& entry : entries_) function(entry.index, entry.value);
            return;
        }
        auto cursor = retained_->open();
        SparseRationalEntry entry;
        while (cursor->next(entry)) function(entry.index, entry.value);
    }
    template<class Function> void for_each_index(Function&& function) const {
        if (retained_) retained_->indices(std::forward<Function>(function));
        else for (const auto& entry : entries_) function(entry.index);
    }
    [[nodiscard]] Rational coefficient(std::size_t index) const {
        if (index >= size()) throw std::out_of_range("sparse rational coordinate");
        if (retained_) return retained_->coefficient(index);
        const auto it = std::lower_bound(entries_.begin(), entries_.end(), index,
            [](const auto& entry, auto key) { return entry.index < key; });
        return it != entries_.end() && it->index == index ? it->value : Rational{0};
    }

    [[nodiscard]] RationalVector dense() const {
        RationalVector out(logical_size_, Rational{0});
        for_each_nonzero([&](std::size_t index, const Rational& value) { out[index] = value; });
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
    friend class storage::temporal_codec_detail::Reader;
    struct Materialization {
        std::once_flag once;
        std::atomic<bool> ready{false};
        std::vector<SparseRationalEntry> entries;
    };
    static SparseRationalVector admitted(std::size_t size,
                                        std::shared_ptr<const RetainedSparseRationalSource> source) {
        SparseRationalVector out(size);
        out.retained_ = std::move(source);
        out.cache_ = std::make_shared<Materialization>();
        return out;
    }
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
    std::shared_ptr<const RetainedSparseRationalSource> retained_;
    std::shared_ptr<Materialization> cache_;
};

} // namespace rfc::numeric
