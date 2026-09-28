#pragma once
#include "rfc/machine/checked_size.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::machine {
struct EncodedBits {
    std::size_t bit_count{};
    std::vector<std::byte> bytes;
    bool operator==(const EncodedBits&) const = default;
};

// General bounded LSB-first octet cursors. These impose no type or source semantics.
class BitWriter {
public:
    explicit BitWriter(std::size_t bits, std::size_t byte_budget)
        : bits_(bits) {
        const auto bytes = ceil_div(bits, 8);
        if (bytes > byte_budget) throw std::length_error("bit writer allocation budget");
        bytes_.resize(bytes, std::byte{0});
    }
    BitWriter(const BitWriter&) = default;
    BitWriter& operator=(const BitWriter&) = default;
    BitWriter(BitWriter&& other) noexcept
        : bits_(std::exchange(other.bits_, 0)), position_(std::exchange(other.position_, 0)), bytes_(std::move(other.bytes_)) {
        other.bytes_.clear();
    }
    BitWriter& operator=(BitWriter&& other) noexcept {
        if (this != &other) {
            bits_ = std::exchange(other.bits_, 0); position_ = std::exchange(other.position_, 0);
            bytes_ = std::move(other.bytes_); other.bytes_.clear();
        }
        return *this;
    }
    void put(std::uint64_t value, unsigned width) {
        if (width > 64 || (width < 64 && (value >> width) != 0))
            throw std::invalid_argument("bit value does not fit width");
        if (width > bits_ - position_) throw std::out_of_range("bit writer extent");
        while (width) {
            const auto offset = static_cast<unsigned>(position_ % 8);
            const auto take = std::min(width, 8u - offset);
            const auto mask = (std::uint64_t{1} << take) - 1u;
            bytes_[position_ / 8] |= static_cast<std::byte>((value & mask) << offset);
            position_ += take; width -= take; value >>= take;
        }
    }
    [[nodiscard]] EncodedBits finish() && {
        if (position_ != bits_) throw std::logic_error("incomplete bit writer");
        EncodedBits result{bits_, std::move(bytes_)};
        bits_ = 0; position_ = 0; bytes_.clear(); return result;
    }
private:
    std::size_t bits_{}, position_{};
    std::vector<std::byte> bytes_;
};

class BitReader {
public:
    BitReader(std::span<const std::byte> bytes, std::size_t bits, std::size_t byte_budget)
        : bytes_(bytes), bits_(bits) {
        if (bytes.size() > byte_budget) throw std::length_error("bit reader input budget");
        if (bytes.size() != ceil_div(bits, 8)) throw std::invalid_argument("bit extent/byte count mismatch");
        if (bits % 8 && (std::to_integer<unsigned>(bytes.back()) >> (bits % 8)) != 0)
            throw std::invalid_argument("nonzero tail padding");
    }
    [[nodiscard]] std::size_t remaining() const noexcept { return bits_ - position_; }
    std::uint64_t get(unsigned width) {
        if (width > 64 || width > remaining()) throw std::invalid_argument("truncated/oversized bit read");
        std::uint64_t value = 0; unsigned shift = 0;
        while (width) {
            const auto offset = static_cast<unsigned>(position_ % 8);
            const auto take = std::min(width, 8u - offset);
            const auto mask = (1u << take) - 1u;
            value |= static_cast<std::uint64_t>((std::to_integer<unsigned>(bytes_[position_ / 8]) >> offset) & mask) << shift;
            shift += take; position_ += take; width -= take;
        }
        return value;
    }
    void finish() const {
        if (remaining()) throw std::invalid_argument("trailing declared bits");
    }
private:
    std::span<const std::byte> bytes_;
    std::size_t bits_{}, position_{};
};
} // namespace rfc::machine
