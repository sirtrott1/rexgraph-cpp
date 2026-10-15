#pragma once

#include "rfc/machine/checked_size.hpp"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::storage {

// A physical byte view only: it grants neither source identity nor authority.
// Indexed images retain their immutable storage and complete ordered references.
// Contiguous leaves requested by old parsers remain pinned until that image dies.
class ByteRange {
    using Bytes = std::vector<std::byte>;
    using Span = std::span<const std::byte>;
public:
    struct Chunk { std::size_t offset{}, size{}; };
private:
    struct State {
        std::shared_ptr<const Bytes> owner;
        std::vector<Span> chunks;
        std::vector<std::size_t> roots, starts;
        std::size_t material_limit{}, work_limit{}, buffer_limit{};
        std::function<void()> check;
        mutable std::mutex mutex;
        mutable std::map<std::pair<std::size_t, std::size_t>, std::shared_ptr<const Bytes>> leaves;
        mutable std::size_t material_bytes{}, work_bytes{}, pending{};
        Span part(std::size_t offset) const {
            if (offset == starts.back()) return {};
            const auto end = std::upper_bound(starts.begin(), starts.end(), offset);
            const auto index = static_cast<std::size_t>(end - starts.begin() - 1);
            return chunks[roots[index]].subspan(offset - starts[index]);
        }
    };
    std::shared_ptr<const State> state_;
    Span direct_;
    std::size_t offset_{}, size_{};
    ByteRange(std::shared_ptr<const State> state, std::size_t offset, std::size_t size)
        : state_(std::move(state)), offset_(offset), size_(size) {}
    static void require(bool yes, const char* message) {
        if (!yes) throw std::length_error(message);
    }
public:
    ByteRange() = default;
    explicit ByteRange(Span bytes) : direct_(bytes), size_(bytes.size()) {}

    // The caller supplies an immutable owner. Public shared-packet entry points
    // defensively copy untrusted caller bytes before invoking any callback.
    static ByteRange indexed(std::shared_ptr<const Bytes> owner, const std::vector<Chunk>& chunks,
        std::vector<std::size_t> roots, std::size_t expanded, std::size_t material_limit,
        std::size_t work_limit, std::size_t buffer_limit, std::function<void()> check = {}) {
        require(static_cast<bool>(owner), "indexed byte image owner required");
        require(chunks.size() <= buffer_limit && roots.size() <= buffer_limit, "indexed byte image record admission");
        (void)machine::checked_size_mul(machine::checked_size_add(roots.size(), 1), sizeof(std::size_t));
        auto state = std::make_shared<State>(); state->owner = std::move(owner);
        state->material_limit = material_limit; state->work_limit = work_limit; state->buffer_limit = buffer_limit;
        state->check = std::move(check); state->chunks.reserve(chunks.size());
        for (const auto chunk : chunks) {
            require(chunk.size && chunk.offset <= state->owner->size() && chunk.size <= state->owner->size() - chunk.offset,
                "indexed byte image literal bounds");
            state->chunks.push_back(Span(*state->owner).subspan(chunk.offset, chunk.size));
        }
        state->roots = std::move(roots); state->starts.reserve(state->roots.size() + 1); state->starts.push_back(0);
        for (const auto id : state->roots) {
            require(id < state->chunks.size(), "indexed byte image reference bounds");
            const auto next = machine::checked_size_add(state->starts.back(), state->chunks[id].size());
            require(next <= expanded, "indexed byte image expansion admission"); state->starts.push_back(next);
        }
        require(state->starts.back() == expanded, "indexed byte image complete expansion required");
        return ByteRange(std::move(state), 0, expanded);
    }
    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    ByteRange subspan(std::size_t offset, std::size_t count = std::dynamic_extent) const {
        require(offset <= size_, "byte range offset bounds");
        if (count == std::dynamic_extent) count = size_ - offset;
        require(count <= size_ - offset, "byte range length bounds");
        if (!state_) return ByteRange(direct_.subspan(offset, count));
        return ByteRange(state_, machine::checked_size_add(offset_, offset), count);
    }
    Span first_span() const {
        if (!state_) return direct_;
        if (!size_) return {};
        const auto part = state_->part(offset_); return part.first(std::min(size_, part.size()));
    }
    template<class Consume> void for_each_span(Consume&& consume) const {
        const auto pin = *this;
        for (std::size_t offset = 0; offset < pin.size_;) {
            const auto part = pin.subspan(offset).first_span();
            if (pin.state_ && pin.state_->check) pin.state_->check();
            consume(part); offset += part.size();
        }
    }
    Span span() const {
        const auto pin = *this; const auto first = pin.first_span();
        if (first.size() == pin.size_) return first;
        const auto state = pin.state_; const auto key = std::pair{pin.offset_, pin.size_};
        {
            std::lock_guard lock(state->mutex);
            const auto found = state->leaves.find(key); if (found != state->leaves.end()) return *found->second;
            require(pin.size_ <= state->material_limit - state->material_bytes, "byte image materialization admission");
            require(pin.size_ <= state->work_limit - state->work_bytes, "byte image copy work admission");
            require(state->leaves.size() + state->pending < state->buffer_limit, "byte image retained leaf admission");
            state->material_bytes += pin.size_; state->work_bytes += pin.size_; ++state->pending;
        }
        try {
            auto bytes = std::make_shared<Bytes>(pin.size_); std::size_t offset{};
            pin.for_each_span([&](Span part) { std::copy(part.begin(), part.end(), bytes->begin() + static_cast<std::ptrdiff_t>(offset)); offset += part.size(); });
            std::shared_ptr<const Bytes> immutable = std::move(bytes);
            std::lock_guard lock(state->mutex);
            const auto [it, inserted] = state->leaves.try_emplace(key, std::move(immutable)); --state->pending;
            if (!inserted) state->material_bytes -= pin.size_;
            return *it->second;
        } catch (...) {
            std::lock_guard lock(state->mutex); state->material_bytes -= pin.size_; --state->pending; throw;
        }
    }
    bool equals(Span expected) const {
        if (size_ != expected.size()) return false;
        std::size_t offset{}; bool equal=true;
        for_each_span([&](Span part) {
            equal = equal && std::equal(part.begin(), part.end(), expected.begin() + static_cast<std::ptrdiff_t>(offset));
            offset += part.size();
        });
        return equal;
    }
    void require_equal(Span expected) const {
        if (!equals(expected)) throw std::invalid_argument("noncanonical logical byte image");
    }
    std::size_t materialized_bytes() const {
        if (!state_) return 0;
        std::lock_guard lock(state_->mutex); return state_->material_bytes;
    }
};
} // namespace rfc::storage
