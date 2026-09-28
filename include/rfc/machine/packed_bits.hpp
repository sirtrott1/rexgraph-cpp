#pragma once
#include "rfc/machine/checked_size.hpp"
#include <utility>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::machine {

class PackedBits {
public:
    using word_type = std::uint64_t;
    static constexpr std::size_t bits_per_word = 64;

    // Default per-plane allocation budget: 1 GiB. A caller may explicitly
    // supply another word budget. Arithmetic and allocator limits still apply.
    static constexpr std::size_t default_word_budget = std::size_t{1} << 27u;
    [[nodiscard]] static constexpr std::size_t words_for_size(std::size_t size) {
        return ceil_div(size, bits_per_word);
    }
    PackedBits() = default;
    explicit PackedBits(std::size_t size, bool value = false,
                        std::size_t word_budget = default_word_budget)
        : size_(size), word_budget_(word_budget),
          words_(allocation_words(size, word_budget), value ? ~word_type{0} : word_type{0}) {
        trim_tail();
    }
    PackedBits(const PackedBits&) = default;
    PackedBits& operator=(const PackedBits&) = default;
    PackedBits(PackedBits&& other) noexcept
        : size_(std::exchange(other.size_, 0)), word_budget_(other.word_budget_), words_(std::move(other.words_)) {
        other.words_.clear();
    }
    PackedBits& operator=(PackedBits&& other) noexcept {
        if (this != &other) {
            size_ = std::exchange(other.size_, 0);
            word_budget_ = other.word_budget_;
            words_ = std::move(other.words_);
            other.words_.clear();
        }
        return *this;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t word_count() const noexcept { return words_.size(); }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    void set(std::size_t index, bool value = true) {
        require_index(index);
        auto& word = words_[index >> 6u];
        const auto mask = word_type{1} << (index & 63u);
        value ? word |= mask : word &= ~mask;
    }

    [[nodiscard]] bool get(std::size_t index) const {
        require_index(index);
        return ((words_[index >> 6u] >> (index & 63u)) & word_type{1}) != 0;
    }

    [[nodiscard]] word_type word(std::size_t index) const { return words_.at(index); }
    void assign_word(std::size_t index, word_type value) {
        words_.at(index) = value;
        if (index + 1u == words_.size()) trim_tail();
    }
    [[nodiscard]] const std::vector<word_type>& words() const noexcept { return words_; }

    [[nodiscard]] bool any() const noexcept {
        for (const auto value : words_) if (value != 0) return true;
        return false;
    }
    [[nodiscard]] bool none() const noexcept { return !any(); }

    [[nodiscard]] std::size_t count() const noexcept {
        std::size_t result = 0;
        for (const auto word_value : words_) {
            result += static_cast<std::size_t>(std::popcount(word_value));
        }
        return result;
    }

    [[nodiscard]] bool intersects(const PackedBits& other) const {
        require_same_size(other);
        for (std::size_t i = 0; i < words_.size(); ++i) {
            if ((words_[i] & other.words_[i]) != 0) return true;
        }
        return false;
    }

    [[nodiscard]] PackedBits operator&(const PackedBits& other) const {
        require_same_size(other);
        PackedBits result(size_, false, word_budget_);
        for (std::size_t i = 0; i < words_.size(); ++i) result.words_[i] = words_[i] & other.words_[i];
        return result;
    }

    [[nodiscard]] PackedBits operator|(const PackedBits& other) const {
        require_same_size(other);
        PackedBits result(size_, false, word_budget_);
        for (std::size_t i = 0; i < words_.size(); ++i) result.words_[i] = words_[i] | other.words_[i];
        return result;
    }

    [[nodiscard]] PackedBits operator^(const PackedBits& other) const {
        require_same_size(other);
        PackedBits result(size_, false, word_budget_);
        for (std::size_t i = 0; i < words_.size(); ++i) result.words_[i] = words_[i] ^ other.words_[i];
        return result;
    }

    [[nodiscard]] PackedBits and_not(const PackedBits& other) const {
        require_same_size(other);
        PackedBits result(size_, false, word_budget_);
        for (std::size_t i = 0; i < words_.size(); ++i) result.words_[i] = words_[i] & ~other.words_[i];
        result.trim_tail();
        return result;
    }

    [[nodiscard]] static PackedBits select(const PackedBits& mask,
                                           const PackedBits& when_true,
                                           const PackedBits& when_false) {
        mask.require_same_size(when_true);
        mask.require_same_size(when_false);
        PackedBits result(mask.size_, false, mask.word_budget_);
        for (std::size_t i = 0; i < result.words_.size(); ++i) {
            result.words_[i] = (mask.words_[i] & when_true.words_[i]) | (~mask.words_[i] & when_false.words_[i]);
        }
        result.trim_tail();
        return result;
    }

    [[nodiscard]] PackedBits operator~() const {
        PackedBits result(size_, false, word_budget_);
        for (std::size_t i = 0; i < words_.size(); ++i) result.words_[i] = ~words_[i];
        result.trim_tail();
        return result;
    }

    template <class Fn>
    void for_each_set(Fn&& fn) const {
        for (std::size_t word_index = 0; word_index < words_.size(); ++word_index) {
            auto current = words_[word_index];
            while (current != 0) {
                const auto bit = static_cast<unsigned>(std::countr_zero(current));
                const auto index = word_index * bits_per_word + bit;
                if (index < size_) fn(index);
                current &= current - 1u;
            }
        }
    }

private:
    [[nodiscard]] static std::size_t allocation_words(std::size_t size, std::size_t budget) {
        const auto words = words_for_size(size);
        if (words > budget || words > std::vector<word_type>{}.max_size())
            throw std::length_error("PackedBits allocation exceeds word budget");
        (void)checked_size_mul(words, sizeof(word_type));
        return words;
    }
    void require_index(std::size_t index) const {
        if (index >= size_) throw std::out_of_range("PackedBits index");
    }
    void require_same_size(const PackedBits& other) const {
        if (size_ != other.size_) throw std::invalid_argument("PackedBits size mismatch");
    }
    void trim_tail() {
        if (words_.empty() || (size_ & 63u) == 0) return;
        const auto tail_bits = static_cast<unsigned>(size_ & 63u);
        words_.back() &= (word_type{1} << tail_bits) - 1u;
    }

    std::size_t size_{0};
    std::size_t word_budget_{default_word_budget};
    std::vector<word_type> words_;
};

} // namespace rfc::machine
