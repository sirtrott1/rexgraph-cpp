#pragma once

#include "rfc/machine/checked_size.hpp"
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <variant>

namespace rfc::storage::native_series_detail {
struct AppendIndexStats {
    std::size_t reachable_pages{}, last_append_pages{}, last_append_pointer_copies{};
    unsigned levels{};
};

// Immutable radix-32 pages. Only the path to the new slot is copied; values and
// unchanged branches retain their original owners. This is a physical directory.
template<class T> class AppendIndex {
    struct Node;
    using Leaf = std::array<std::shared_ptr<const T>, 32>;
    using Branch = std::array<std::shared_ptr<const Node>, 32>;
    struct Node {
        std::variant<Leaf, Branch> slots;
        explicit Node(Leaf values) : slots(std::move(values)) {}
        explicit Node(Branch values) : slots(std::move(values)) {}
    };
public:
    std::size_t size() const { return size_; }
    AppendIndexStats stats() const { return stats_; }
    const std::shared_ptr<const T>& at(std::size_t index) const {
        if (index >= size_) throw std::out_of_range("persistent frame index");
        auto page = root_.get();
        for (unsigned depth = depth_; depth > 0; --depth)
            page = std::get<Branch>(page->slots)[(index >> (5 * depth)) & 31].get();
        return std::get<Leaf>(page->slots)[index & 31];
    }
    AppendIndex appended(std::shared_ptr<const T> value) const {
        if (!value) throw std::invalid_argument("null persistent frame owner");
        AppendIndex next = *this; next.size_ = machine::checked_size_add(size_, 1);
        next.stats_.last_append_pages = next.stats_.last_append_pointer_copies = 0;
        const auto bits = 5 * (depth_ + 1);
        if (bits < std::numeric_limits<std::size_t>::digits && (size_ >> bits) != 0) {
            auto page = std::make_shared<Node>(Branch{});
            ++next.stats_.last_append_pages; ++next.stats_.reachable_pages;
            auto& children = std::get<Branch>(page->slots);
            children[0] = root_; ++next.stats_.last_append_pointer_copies;
            next.depth_ = depth_ + 1;
            const auto slot = (size_ >> (5 * next.depth_)) & 31;
            children[slot] = insert({}, depth_, size_, std::move(value), next.stats_);
            next.root_ = std::move(page);
        } else next.root_ = insert(root_, depth_, size_, std::move(value), next.stats_);
        next.stats_.levels = next.depth_ + 1; return next;
    }
    template<class F> void for_each(F&& f) const {
        auto remaining = size_; if (remaining) visit(*root_, depth_, remaining, f);
    }
private:
    static std::shared_ptr<const Node> insert(const std::shared_ptr<const Node>& old, unsigned depth,
                                              std::size_t index, std::shared_ptr<const T> value, AppendIndexStats& stats) {
        auto next = old ? std::make_shared<Node>(*old) :
            (depth ? std::make_shared<Node>(Branch{}) : std::make_shared<Node>(Leaf{}));
        ++stats.last_append_pages;
        if (old) stats.last_append_pointer_copies += 32;
        else ++stats.reachable_pages;
        if (depth == 0) std::get<Leaf>(next->slots)[index & 31] = std::move(value);
        else {
            auto& children = std::get<Branch>(next->slots); const auto slot = (index >> (5 * depth)) & 31;
            children[slot] = insert(children[slot], depth - 1, index, std::move(value), stats);
        }
        return next;
    }
    template<class F> static void visit(const Node& page, unsigned depth, std::size_t& remaining, F& f) {
        if (depth == 0) {
            for (const auto& value : std::get<Leaf>(page.slots)) {
                if (!remaining) break;
                f(value); --remaining;
            }
        } else for (const auto& child : std::get<Branch>(page.slots)) {
            if (!remaining) break;
            visit(*child, depth - 1, remaining, f);
        }
    }
    std::shared_ptr<const Node> root_;
    std::size_t size_{};
    unsigned depth_{};
    AppendIndexStats stats_{};
};
} // namespace rfc::storage::native_series_detail
