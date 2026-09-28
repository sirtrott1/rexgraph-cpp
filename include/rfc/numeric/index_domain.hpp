#pragma once
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::numeric {

// Declared coordinate shape, independent of which coordinates structurally
// exist and independent of their numerical values.  Strides are row-major.
class IndexDomain {
public:
    IndexDomain() = default;

    explicit IndexDomain(std::vector<std::size_t> extents)
        : extents_(std::move(extents)) {
        rebuild();
    }

    [[nodiscard]] static IndexDomain linear(std::size_t extent) {
        return IndexDomain({extent});
    }

    [[nodiscard]] std::size_t rank() const noexcept { return extents_.size(); }
    [[nodiscard]] bool scalar() const noexcept { return extents_.empty(); }
    [[nodiscard]] std::size_t logical_size() const noexcept { return logical_size_; }
    [[nodiscard]] const std::vector<std::size_t>& extents() const noexcept { return extents_; }
    [[nodiscard]] const std::vector<std::size_t>& strides() const noexcept { return strides_; }

    [[nodiscard]] std::size_t flatten(std::span<const std::size_t> coordinates) const {
        if (coordinates.size() != extents_.size()) throw std::invalid_argument("index coordinate rank mismatch");
        if (extents_.empty()) return 0;
        std::size_t flat = 0;
        for (std::size_t axis = 0; axis < extents_.size(); ++axis) {
            if (coordinates[axis] >= extents_[axis]) throw std::out_of_range("index coordinate");
            flat += coordinates[axis] * strides_[axis];
        }
        return flat;
    }

    [[nodiscard]] std::vector<std::size_t> unflatten(std::size_t flat) const {
        if (flat >= logical_size_) throw std::out_of_range("flat index");
        if (extents_.empty()) return {};
        std::vector<std::size_t> coordinates(extents_.size(), 0);
        for (std::size_t axis = 0; axis < extents_.size(); ++axis) {
            coordinates[axis] = flat / strides_[axis];
            flat %= strides_[axis];
        }
        return coordinates;
    }

    friend bool operator==(const IndexDomain&, const IndexDomain&) = default;

private:
    void rebuild() {
        if (extents_.empty()) {
            logical_size_ = 1;
            strides_.clear();
            return;
        }
        strides_.assign(extents_.size(), 1);
        std::size_t running = 1;
        for (std::size_t rev = extents_.size(); rev-- > 0;) {
            const auto extent = extents_[rev];
            strides_[rev] = running;
            if (extent == 0) {
                running = 0;
                continue;
            }
            if (running > std::numeric_limits<std::size_t>::max() / extent) {
                throw std::overflow_error("index domain size overflow");
            }
            running *= extent;
        }
        logical_size_ = running;
    }

    std::vector<std::size_t> extents_;
    std::vector<std::size_t> strides_;
    std::size_t logical_size_{1};
};

} // namespace rfc::numeric
