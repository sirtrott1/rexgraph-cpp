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

namespace bit_cursor_detail {
inline void put(std::span<std::byte> bytes, std::size_t bits, std::size_t& position,
                std::uint64_t value, unsigned width) {
    if (width > 64 || (width < 64 && (value >> width) != 0))
        throw std::invalid_argument("bit value does not fit width");
    if (width > bits - position) throw std::out_of_range("bit writer extent");
    while (width) {
        const auto offset = static_cast<unsigned>(position % 8);
        const auto take = std::min(width, 8u - offset);
        const auto mask = (std::uint64_t{1} << take) - 1u;
        bytes[position / 8] |= static_cast<std::byte>((value & mask) << offset);
        position += take; width -= take; value >>= take;
    }
}
inline void put_octets(std::span<std::byte> target, std::size_t bits, std::size_t& position,
                       std::span<const std::byte> bytes) {
    if (bytes.size() > (bits - position) / 8) throw std::out_of_range("bit writer octet extent");
    if (bytes.empty()) return;
    const auto first = position / 8;
    const auto offset = static_cast<unsigned>(position % 8);
    if (!offset) std::copy(bytes.begin(), bytes.end(), target.data() + first);
    else for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto value = std::to_integer<unsigned>(bytes[i]);
        target[first + i] |= static_cast<std::byte>(value << offset);
        target[first + i + 1] = static_cast<std::byte>(value >> (8u - offset));
    }
    position += bytes.size() * 8;
}
// The generator writes complete octets before their in-place LSB alignment.
// Only the selected region is exposed and all extent checks precede the call.
template<class Fill> inline void generate_octets(std::span<std::byte> bytes,
    std::size_t bits, std::size_t& position, std::size_t count, Fill&& fill) {
    if (count > (bits - position) / 8) throw std::out_of_range("bit writer octet extent");
    if (!count) return;
    const auto first = position / 8;
    const auto offset = static_cast<unsigned>(position % 8);
    const auto prefix = bytes[first];
    std::forward<Fill>(fill)(bytes.subspan(first, count));
    if (offset) {
        for (std::size_t j = count; j != 0; --j) {
            const auto low = std::to_integer<unsigned>(bytes[first + j - 1]);
            const auto high = j < count ? std::to_integer<unsigned>(bytes[first + j]) : 0u;
            bytes[first + j] = static_cast<std::byte>((low >> (8u - offset)) | (high << offset));
        }
        bytes[first] = prefix | static_cast<std::byte>(std::to_integer<unsigned>(bytes[first]) << offset);
    }
    position += count * 8;
}
inline void finish(std::size_t bits, std::size_t position) {
    if (position != bits) throw std::logic_error("incomplete bit writer");
}
} // namespace bit_cursor_detail

// A nonowning writer for a caller's admitted, exclusive output region.
// The caller keeps the region alive and does not resize it during encoding.
class BitSpanWriter {
public:
    BitSpanWriter(std::span<std::byte> bytes, std::size_t bits, std::size_t byte_budget)
        : bytes_(bytes), bits_(bits) {
        if (bytes.size() > byte_budget) throw std::length_error("bit writer allocation budget");
        if (bytes.size() != ceil_div(bits, 8)) throw std::invalid_argument("bit extent/byte count mismatch");
        std::fill(bytes.begin(), bytes.end(), std::byte{0});
    }
    void put(std::uint64_t value, unsigned width) { bit_cursor_detail::put(bytes_, bits_, position_, value, width); }
    void put_octets(std::span<const std::byte> bytes) { bit_cursor_detail::put_octets(bytes_, bits_, position_, bytes); }
    template<class Fill> void put_generated_octets(std::size_t count, Fill&& fill) {
        bit_cursor_detail::generate_octets(bytes_, bits_, position_, count, std::forward<Fill>(fill));
    }
    void finish() const { bit_cursor_detail::finish(bits_, position_); }
private:
    std::span<std::byte> bytes_;
    std::size_t bits_{}, position_{};
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
    void put(std::uint64_t value, unsigned width) { bit_cursor_detail::put(bytes_, bits_, position_, value, width); }
    void put_octets(std::span<const std::byte> bytes) { bit_cursor_detail::put_octets(bytes_, bits_, position_, bytes); }
    template<class Fill> void put_generated_octets(std::size_t count, Fill&& fill) {
        bit_cursor_detail::generate_octets(bytes_, bits_, position_, count, std::forward<Fill>(fill));
    }
    [[nodiscard]] EncodedBits finish() && {
        bit_cursor_detail::finish(bits_, position_);
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
    [[nodiscard]] std::vector<std::byte> get_octets(std::size_t count) {
        if (count > remaining() / 8)
            throw std::invalid_argument("truncated bit octets");
        if (!count) return {};
        std::vector<std::byte> bytes(count);
        const auto first = position_ / 8;
        const auto offset = static_cast<unsigned>(position_ % 8);
        if (!offset) {
            std::copy_n(bytes_.data() + first, count, bytes.begin());
        } else {
            for (std::size_t i = 0; i < count; ++i)
                bytes[i] = static_cast<std::byte>(
                    (std::to_integer<unsigned>(bytes_[first + i]) >> offset) |
                    (std::to_integer<unsigned>(bytes_[first + i + 1]) << (8u - offset)));
        }
        position_ += count * 8;
        return bytes;
    }
    void finish() const {
        if (remaining()) throw std::invalid_argument("trailing declared bits");
    }
private:
    std::span<const std::byte> bytes_;
    std::size_t bits_{}, position_{};
};
} // namespace rfc::machine
