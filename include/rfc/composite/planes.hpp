#pragma once
#include "rfc/composite/aspect.hpp"
#include "rfc/machine/packed_bits.hpp"
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace rfc::composite {

class AspectPlanes {
public:
    AspectPlanes() = default;
    AspectPlanes(const AspectSchema& schema, std::size_t slots,
                 std::size_t word_budget = machine::PackedBits::default_word_budget) : slots_(slots) {
        const auto total_words = machine::checked_size_mul(schema.size(), machine::PackedBits::words_for_size(slots));
        if (total_words > word_budget) throw std::length_error("aspect planes exceed total word budget");
        planes_.reserve(schema.size());
        for (std::size_t i = 0; i < schema.size(); ++i) {
            planes_.emplace_back(slots, schema.at(static_cast<AspectId>(i)).default_value, word_budget);
        }
    }

    AspectPlanes(const AspectPlanes&) = default;
    AspectPlanes& operator=(const AspectPlanes&) = default;
    AspectPlanes(AspectPlanes&& other) noexcept
        : slots_(std::exchange(other.slots_,0)), planes_(std::move(other.planes_)) { other.planes_.clear(); }
    AspectPlanes& operator=(AspectPlanes&& other) noexcept {
        if (this != &other) { slots_=std::exchange(other.slots_,0); planes_=std::move(other.planes_); other.planes_.clear(); }
        return *this;
    }

    [[nodiscard]] std::size_t slots() const noexcept { return slots_; }
    [[nodiscard]] std::size_t aspect_count() const noexcept { return planes_.size(); }

    void set(AspectId aspect, std::size_t slot, bool value = true) { planes_.at(aspect).set(slot, value); }
    [[nodiscard]] bool get(AspectId aspect, std::size_t slot) const { return planes_.at(aspect).get(slot); }
    [[nodiscard]] const machine::PackedBits& plane(AspectId aspect) const { return planes_.at(aspect); }
    [[nodiscard]] machine::PackedBits& plane(AspectId aspect) { return planes_.at(aspect); }

private:
    std::size_t slots_{0};
    std::vector<machine::PackedBits> planes_;
};

} // namespace rfc::composite
