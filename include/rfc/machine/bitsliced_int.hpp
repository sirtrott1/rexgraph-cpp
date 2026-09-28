#pragma once
#include "rfc/machine/packed_bits.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rfc::machine {

// Two's-complement signed integers represented bit-sliced across many lanes.
// Each plane is a PackedBits vector, so arithmetic is performed with Boolean
// machine-word operations over all lanes in parallel.
template <std::size_t Bits>
class BitSlicedInt {
    static_assert(Bits >= 2 && Bits <= 63, "BitSlicedInt supports 2..63 bits");
public:
    BitSlicedInt() = default;
    explicit BitSlicedInt(std::size_t lanes) : lanes_(lanes) {
        if (checked_size_mul(Bits, PackedBits::words_for_size(lanes)) > PackedBits::default_word_budget)
            throw std::length_error("bit-sliced allocation exceeds aggregate word budget");
        for (auto& plane : planes_) plane = PackedBits(lanes, false);
    }
    BitSlicedInt(const BitSlicedInt&) = default;
    BitSlicedInt& operator=(const BitSlicedInt&) = default;
    BitSlicedInt(BitSlicedInt&& other) noexcept
        : lanes_(std::exchange(other.lanes_,0)), planes_(std::move(other.planes_)) {}
    BitSlicedInt& operator=(BitSlicedInt&& other) noexcept {
        if (this != &other) { lanes_=std::exchange(other.lanes_,0); planes_=std::move(other.planes_); }
        return *this;
    }

    [[nodiscard]] std::size_t size() const noexcept { return lanes_; }
    [[nodiscard]] const PackedBits& plane(std::size_t bit) const { return planes_.at(bit); }
    [[nodiscard]] PackedBits& plane(std::size_t bit) { return planes_.at(bit); }

    [[nodiscard]] static constexpr std::int64_t min_value() noexcept {
        return -(std::int64_t{1} << (Bits - 1u));
    }
    [[nodiscard]] static constexpr std::int64_t max_value() noexcept {
        return (std::int64_t{1} << (Bits - 1u)) - 1;
    }

    [[nodiscard]] static BitSlicedInt constant(std::size_t lanes, std::int64_t value) {
        require_representable(value);
        BitSlicedInt result(lanes);
        const auto encoded = encode_bits(value);
        for (std::size_t bit = 0; bit < Bits; ++bit) {
            if (((encoded >> bit) & std::uint64_t{1}) != 0) result.planes_[bit] = PackedBits(lanes, true);
        }
        return result;
    }

    [[nodiscard]] static BitSlicedInt masked_constant(const PackedBits& mask, std::int64_t value) {
        require_representable(value);
        BitSlicedInt result(mask.size());
        const auto encoded = encode_bits(value);
        for (std::size_t bit = 0; bit < Bits; ++bit) {
            if (((encoded >> bit) & std::uint64_t{1}) != 0) result.planes_[bit] = mask;
        }
        return result;
    }

    [[nodiscard]] static BitSlicedInt from_values(const std::vector<std::int64_t>& values) {
        BitSlicedInt result(values.size());
        for (std::size_t lane = 0; lane < values.size(); ++lane) {
            require_representable(values[lane]);
            const auto encoded = encode_bits(values[lane]);
            for (std::size_t bit = 0; bit < Bits; ++bit) {
                if (((encoded >> bit) & std::uint64_t{1}) != 0) result.planes_[bit].set(lane);
            }
        }
        return result;
    }

    [[nodiscard]] std::int64_t value(std::size_t lane) const {
        if (lane >= lanes_) throw std::out_of_range("BitSlicedInt lane");
        std::uint64_t encoded = 0;
        for (std::size_t bit = 0; bit < Bits; ++bit) {
            if (planes_[bit].get(lane)) encoded |= (std::uint64_t{1} << bit);
        }
        return decode_bits(encoded);
    }

    [[nodiscard]] std::vector<std::int64_t> values() const {
        std::vector<std::int64_t> result(lanes_);
        for (std::size_t lane = 0; lane < lanes_; ++lane) result[lane] = value(lane);
        return result;
    }

    // Addition modulo 2^Bits. For exact fixed-width inference the caller can
    // separately inspect signed_overflow() or provision enough accumulator bits.
    [[nodiscard]] BitSlicedInt add_mod(const BitSlicedInt& rhs) const {
        require_same_size(rhs);
        BitSlicedInt result(lanes_);
        PackedBits carry(lanes_, false);
        for (std::size_t bit = 0; bit < Bits; ++bit) {
            const auto axb = planes_[bit] ^ rhs.planes_[bit];
            result.planes_[bit] = axb ^ carry;
            carry = (planes_[bit] & rhs.planes_[bit]) | (carry & axb);
        }
        return result;
    }

    [[nodiscard]] PackedBits signed_overflow(const BitSlicedInt& rhs,
                                             const BitSlicedInt& sum) const {
        require_same_size(rhs);
        require_same_size(sum);
        const auto& sa = planes_[Bits - 1u];
        const auto& sb = rhs.planes_[Bits - 1u];
        const auto& ss = sum.planes_[Bits - 1u];
        return ~(sa ^ sb) & (sa ^ ss);
    }

    [[nodiscard]] BitSlicedInt negate_mod() const {
        BitSlicedInt inverted(lanes_);
        for (std::size_t bit = 0; bit < Bits; ++bit) inverted.planes_[bit] = ~planes_[bit];
        return inverted.add_mod(constant(lanes_, 1));
    }

    [[nodiscard]] BitSlicedInt sub_mod(const BitSlicedInt& rhs) const {
        return add_mod(rhs.negate_mod());
    }

    // Exact fixed-width multiplication by a scalar, modulo 2^Bits. This is a
    // shift/add machine-algebra primitive; choose accumulator width so overflow
    // is impossible when an unwrapped mathematical result is required.
    [[nodiscard]] BitSlicedInt mul_constant_mod(std::int64_t scalar) const {
        require_representable(scalar);
        BitSlicedInt result(lanes_);
        BitSlicedInt multiplicand = *this;
        auto encoded = encode_bits(scalar);
        for (std::size_t bit = 0; bit < Bits; ++bit) {
            if (((encoded >> bit) & std::uint64_t{1}) != 0) result = result.add_mod(multiplicand);
            if (bit + 1u < Bits) multiplicand = multiplicand.add_mod(multiplicand);
        }
        return result;
    }

    // Signed comparison against a scalar threshold. Flipping the sign bit maps
    // two's-complement order to unsigned lexicographic order.
    [[nodiscard]] PackedBits greater_equal(std::int64_t threshold) const {
        require_representable(threshold);
        PackedBits gt(lanes_, false);
        PackedBits eq(lanes_, true);
        auto encoded = encode_bits(threshold);
        encoded ^= (std::uint64_t{1} << (Bits - 1u));
        for (std::size_t rev = 0; rev < Bits; ++rev) {
            const auto bit = Bits - 1u - rev;
            PackedBits lhs = planes_[bit];
            if (bit == Bits - 1u) lhs = ~lhs;
            const bool rhs_bit = ((encoded >> bit) & std::uint64_t{1}) != 0;
            if (rhs_bit) {
                eq = eq & lhs;
            } else {
                gt = gt | (eq & lhs);
                eq = eq & ~lhs;
            }
        }
        return gt | eq;
    }

private:
    static void require_representable(std::int64_t value) {
        if (value < min_value() || value > max_value()) throw std::out_of_range("BitSlicedInt value not representable");
    }
    static constexpr std::uint64_t width_mask() noexcept {
        return (std::uint64_t{1} << Bits) - 1u;
    }
    static constexpr std::uint64_t encode_bits(std::int64_t value) noexcept {
        return static_cast<std::uint64_t>(value) & width_mask();
    }
    static constexpr std::int64_t decode_bits(std::uint64_t encoded) noexcept {
        encoded &= width_mask();
        const auto sign = std::uint64_t{1} << (Bits - 1u);
        if ((encoded & sign) == 0) return static_cast<std::int64_t>(encoded);
        const auto magnitude = ((~encoded) + 1u) & width_mask();
        return -static_cast<std::int64_t>(magnitude);
    }
    void require_same_size(const BitSlicedInt& rhs) const {
        if (lanes_ != rhs.lanes_) throw std::invalid_argument("BitSlicedInt lane mismatch");
    }

    std::size_t lanes_{0};
    std::array<PackedBits, Bits> planes_{};
};

} // namespace rfc::machine
