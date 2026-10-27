#pragma once
#include "rfc/storage/file_io.hpp"
#include "rfc/storage/byte_range.hpp"

#include "rfc/composite/execution.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/storage/mapped_file.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <climits>
#include <optional>
#include <queue>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace rfc::storage {

namespace rfcx_detail {

inline constexpr std::array<std::byte, 8> magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'X'},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
inline constexpr std::uint32_t version = 1;
inline constexpr std::uint32_t endian_tag = 0x01020304u;
inline constexpr std::size_t header_size = 96;
inline constexpr std::size_t grade_descriptor_size = 184;
inline constexpr std::int64_t overflow_sentinel = std::numeric_limits<std::int64_t>::min();

inline std::uint64_t fnv1a64_update(std::uint64_t hash, std::span<const std::byte> bytes) noexcept {
    for (const auto b : bytes) {
        hash ^= static_cast<std::uint8_t>(b);
        hash *= 1099511628211ull;
    }
    return hash;
}

inline std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept {
    return fnv1a64_update(1469598103934665603ull, bytes);
}
inline std::uint64_t fnv1a64(const ByteRange& bytes) {
    std::uint64_t hash = 1469598103934665603ull;
    bytes.for_each_span([&](std::span<const std::byte> part) { hash = fnv1a64_update(hash, part); });
    return hash;
}

class Writer {
public:
    Writer() = default;
    explicit Writer(std::size_t capacity) { bytes_.reserve(capacity); }
    [[nodiscard]] std::vector<std::byte> take_bytes() && noexcept { return std::move(bytes_); }
    // A measuring writer counts the same admitted wire without a payload buffer.
    static Writer measure_only() { Writer out; out.measuring_=true; return out; }
    [[nodiscard]] bool is_measuring() const noexcept { return measuring_; }
    [[nodiscard]] std::size_t size() const noexcept { return measuring_ ? measured_ : bytes_.size(); }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

    void align(std::size_t alignment) {
        if (alignment == 0) return;
        if (measuring_) { (void)reserve((alignment-size()%alignment)%alignment); return; }
        while (bytes_.size() % alignment != 0) bytes_.push_back(std::byte{0});
    }

    std::size_t reserve(std::size_t count) {
        const auto offset = size();
        if (measuring_) measured_=machine::checked_size_add(measured_,count);
        else bytes_.resize(machine::checked_size_add(bytes_.size(), count), std::byte{0});
        return offset;
    }

    // The returned exclusive region is valid until the writer grows again.
    // Its bytes are initialized before a nested encoder fills the region.
    std::span<std::byte> append_zeroed(std::size_t count) {
        if (measuring_) throw std::logic_error("measuring writer has no output region");
        const auto offset = reserve(count);
        return std::span<std::byte>(bytes_).subspan(offset, count);
    }

    void raw(std::span<const std::byte> bytes) {
        if (measuring_) measured_=machine::checked_size_add(measured_,bytes.size());
        else bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    void u8(std::uint8_t v) {
        if (measuring_) measured_=machine::checked_size_add(measured_,1);
        else bytes_.push_back(static_cast<std::byte>(v));
    }
    void u16(std::uint16_t v) {
        u8(static_cast<std::uint8_t>(v & 0xffu));
        u8(static_cast<std::uint8_t>((v >> 8u) & 0xffu));
    }
    void u32(std::uint32_t v) {
        for (unsigned s = 0; s < 32; s += 8) u8(static_cast<std::uint8_t>((v >> s) & 0xffu));
    }
    void u64(std::uint64_t v) {
        for (unsigned s = 0; s < 64; s += 8) u8(static_cast<std::uint8_t>((v >> s) & 0xffu));
    }
    void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }

    void patch_byte(std::size_t offset, std::byte v) {
        require_patch(offset, 1);
        if (measuring_) return;
        bytes_[offset] = v;
    }
    void patch_u32(std::size_t offset, std::uint32_t v) {
        require_patch(offset, 4);
        if (measuring_) return;
        for (unsigned i = 0; i < 4; ++i) bytes_[offset + i] = static_cast<std::byte>((v >> (8u * i)) & 0xffu);
    }
    void patch_u64(std::size_t offset, std::uint64_t v) {
        require_patch(offset, 8);
        if (measuring_) return;
        for (unsigned i = 0; i < 8; ++i) bytes_[offset + i] = static_cast<std::byte>((v >> (8u * i)) & 0xffu);
    }

    void string(std::string_view value) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("RFCX string too long");
        u32(static_cast<std::uint32_t>(value.size()));
        raw({reinterpret_cast<const std::byte*>(value.data()), value.size()});
    }

private:
    void require_patch(std::size_t offset, std::size_t count) const {
        if (offset > size() || count > size() - offset) throw std::out_of_range("RFCX patch");
    }
    std::vector<std::byte> bytes_;
    std::size_t measured_{};
    bool measuring_{};
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes, std::size_t offset = 0) : bytes_(bytes), size_(bytes.size()), pos_(offset) {}
    explicit Reader(ByteRange bytes, std::size_t offset = 0) : image_(std::move(bytes)), size_(image_->size()), pos_(offset) {}
    [[nodiscard]] std::size_t position() const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const {
        require(0); return size_ - pos_;
    }
    void require_items(std::uint64_t count, std::size_t minimum_bytes) const {
        if (minimum_bytes == 0 || count > remaining() / minimum_bytes)
            throw std::runtime_error("RFCX count exceeds bounded record");
    }
    std::size_t size_value() { return machine::checked_size(u64()); }
    bool boolean() {
        const auto value = u8();
        if (value > 1) throw std::runtime_error("RFCX non-Boolean flag");
        return value != 0;
    }

    std::uint8_t u8() {
        require(1);
        if (!image_) return static_cast<std::uint8_t>(bytes_[pos_++]);
        if (pos_ < cached_begin_ || pos_ - cached_begin_ >= cached_.size()) {
            cached_ = image_->subspan(pos_).first_span(); cached_begin_ = pos_;
        }
        return static_cast<std::uint8_t>(cached_[pos_++ - cached_begin_]);
    }
    std::uint16_t u16() {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 2; ++i) v |= static_cast<std::uint32_t>(u8()) << (8u * i);
        return static_cast<std::uint16_t>(v);
    }
    std::uint32_t u32() {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(u8()) << (8u * i);
        return v;
    }
    std::uint64_t u64() {
        std::uint64_t v = 0;
        for (unsigned i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(u8()) << (8u * i);
        return v;
    }
    std::int64_t i64() { return static_cast<std::int64_t>(u64()); }

    std::string string() {
        const auto n = static_cast<std::size_t>(u32());
        const auto bytes = slice(n);
        return std::string(reinterpret_cast<const char*>(bytes.data()), n);
    }

    std::span<const std::byte> slice(std::size_t n) {
        require(n);
        auto result = image_ ? image_->subspan(pos_, n).span() : bytes_.subspan(pos_, n);
        pos_ += n;
        return result;
    }
    ByteRange view_slice(std::size_t n) {
        require(n);
        auto result = image_ ? image_->subspan(pos_, n) : ByteRange(bytes_.subspan(pos_, n));
        pos_ += n; return result;
    }

private:
    void require(std::size_t n) const {
        if (pos_ > size_ || n > size_ - pos_) throw std::runtime_error("truncated RFCX image");
    }
    std::span<const std::byte> bytes_;
    std::optional<ByteRange> image_;
    std::span<const std::byte> cached_;
    std::size_t size_{}, cached_begin_{};
    std::size_t pos_{};
};

inline void write_gmp_integer(Writer& w, mpz_srcptr input) {
    const auto count=mpz_sgn(input)==0 ? std::size_t{0} : machine::ceil_div(
        static_cast<std::size_t>(mpz_sizeinbase(input,2)),8u);
    if (count>std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("RFCX integer byte count overflow");
    w.u8(static_cast<std::uint8_t>(mpz_sgn(input)<0 ? 1u : 0u)); w.u32(static_cast<std::uint32_t>(count));
    if (w.is_measuring()) { (void)w.reserve(count); return; }
    if (count) {
        auto bytes=w.append_zeroed(count);std::size_t written{};
        mpz_export(bytes.data(),&written,1,1,1,0,input);
        if (written!=count) throw std::runtime_error("RFCX integer extent changed");
    }
}
inline void write_bigint(Writer& w, const numeric::Integer& input) {
    write_gmp_integer(w,input.get_mpz_t());
}

inline numeric::Integer read_bigint(Reader& r) {
    const bool negative = r.boolean();
    const auto bytes = r.slice(r.u32());
    if ((bytes.empty() && negative) || (!bytes.empty() && bytes.front() == std::byte{0}))
        throw std::runtime_error("RFCX noncanonical integer");
    numeric::Integer result = 0;
    if (!bytes.empty()) mpz_import(result.get_mpz_t(), bytes.size(), 1, 1, 1, 0, bytes.data());
    if (negative) result = -result;
    return result;
}

inline void write_rational(Writer& w, numeric::Rational value) {
    value.canonicalize();
    write_gmp_integer(w,mpq_numref(value.get_mpq_t()));
    write_gmp_integer(w,mpq_denref(value.get_mpq_t()));
}

inline numeric::Rational read_rational(Reader& r) {
    const auto num = read_bigint(r);
    const auto den = read_bigint(r);
    if (den <= 0) throw std::runtime_error("RFCX rational requires positive denominator");
    numeric::Rational value(num);
    value /= numeric::Rational(den);
    value.canonicalize();
    return value;
}

inline bool fits_fast_i64(const numeric::Integer& v) {
    static const numeric::Integer lo(std::to_string(std::numeric_limits<std::int64_t>::min()));
    static const numeric::Integer hi(std::to_string(std::numeric_limits<std::int64_t>::max()));
    if (v < lo || v > hi) return false;
    // Reserve INT64_MIN as the overflow sentinel.
    return v != lo;
}

inline std::int64_t to_i64(const numeric::Integer& v) {
    return static_cast<std::int64_t>(std::stoll(v.get_str()));
}

struct OverflowEntry {
    std::uint64_t index{};
    numeric::Integer value{0};
};

inline std::uint64_t write_integer_lane(Writer& w, std::span<const numeric::Integer> values) {
    w.align(8);
    const auto lane_offset = static_cast<std::uint64_t>(w.size());
    std::vector<OverflowEntry> overflow;
    overflow.reserve(values.size() / 128u + 1u);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (fits_fast_i64(values[i])) w.i64(to_i64(values[i]));
        else {
            w.i64(overflow_sentinel);
            overflow.push_back({static_cast<std::uint64_t>(i), values[i]});
        }
    }
    // The caller obtains the overflow section separately via write_overflow.
    return lane_offset;
}

inline std::uint64_t write_overflow(Writer& w, const std::vector<OverflowEntry>& overflow) {
    if (overflow.empty()) return 0;
    w.align(8);
    const auto section = static_cast<std::uint64_t>(w.size());
    w.u64(static_cast<std::uint64_t>(overflow.size()));
    const auto directory = w.reserve(machine::checked_size_mul(overflow.size(), 32u));
    for (std::size_t i = 0; i < overflow.size(); ++i) {
        const auto& item = overflow[i];
        numeric::Integer magnitude = item.value < 0 ? -item.value : item.value;
        std::size_t count = 0;
        std::vector<std::byte> bytes;
        if (magnitude != 0) {
            const auto bits = static_cast<std::size_t>(mpz_sizeinbase(magnitude.get_mpz_t(), 2));
            bytes.resize(machine::ceil_div(bits, 8u));
            mpz_export(bytes.data(), &count, 1, 1, 1, 0, magnitude.get_mpz_t());
            bytes.resize(count);
        }
        if (bytes.size() > std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("RFCX overflow integer size");
        w.align(8);
        const auto blob_offset = static_cast<std::uint64_t>(w.size());
        w.raw(bytes);
        const auto rec = directory + i * 32u;
        w.patch_u64(rec + 0u, item.index);
        w.patch_u64(rec + 8u, blob_offset);
        w.patch_u32(rec + 16u, static_cast<std::uint32_t>(bytes.size()));
        w.patch_u32(rec + 20u, item.value < 0 ? 1u : 0u);
        w.patch_u64(rec + 24u, 0u);
    }
    return section;
}

struct IntegerLaneSections {
    std::uint64_t lane{};
    std::uint64_t overflow{};
};

inline IntegerLaneSections write_integer_lane_complete(Writer& w, std::span<const numeric::Integer> values) {
    w.align(8);
    const auto lane_offset = static_cast<std::uint64_t>(w.size());
    std::vector<OverflowEntry> overflow;
    overflow.reserve(values.size() / 128u + 1u);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (fits_fast_i64(values[i])) w.i64(to_i64(values[i]));
        else {
            w.i64(overflow_sentinel);
            overflow.push_back({static_cast<std::uint64_t>(i), values[i]});
        }
    }
    return {lane_offset, write_overflow(w, overflow)};
}

inline std::uint64_t write_u64_array(Writer& w, std::span<const std::uint64_t> values) {
    w.align(8);
    const auto offset = static_cast<std::uint64_t>(w.size());
    for (const auto v : values) w.u64(v);
    return offset;
}
inline std::uint64_t write_size_array(Writer& w, std::span<const std::size_t> values) {
    w.align(8);
    const auto offset = static_cast<std::uint64_t>(w.size());
    for (const auto v : values) w.u64(static_cast<std::uint64_t>(v));
    return offset;
}
inline std::uint64_t write_u32_array(Writer& w, std::span<const std::uint32_t> values) {
    w.align(8);
    const auto offset = static_cast<std::uint64_t>(w.size());
    for (const auto v : values) w.u32(v);
    return offset;
}

inline void write_signature(Writer& w, const core::RelationSignature& sig) {
    w.u32(static_cast<std::uint32_t>(sig.aspects().size()));
    for (std::size_t i = 0; i < sig.aspects().size(); ++i) {
        const auto& aspect = sig.aspects().at(static_cast<composite::AspectId>(i));
        w.string(aspect.name);
        w.u8(static_cast<std::uint8_t>(aspect.role));
        w.u8(aspect.default_value ? 1u : 0u);
    }
    w.u32(static_cast<std::uint32_t>(sig.type_count()));
    for (std::size_t i = 0; i < sig.type_count(); ++i) {
        const auto& type = sig.type(static_cast<core::RelationTypeId>(i));
        w.string(type.name);
        w.u8(static_cast<std::uint8_t>(type.boundary_policy));
        w.u64(type.min_arity);
        w.u64(type.max_arity);
        w.u8(static_cast<std::uint8_t>(type.constraints.shape));
        w.u8(type.constraints.distinguished_role ? 1u : 0u);
        w.u16(type.constraints.distinguished_role.value_or(0u));
        w.u8(static_cast<std::uint8_t>(type.constructor));
        w.u32(static_cast<std::uint32_t>(type.constraints.rules.size()));
        for (const auto& rule : type.constraints.rules) {
            w.u8(static_cast<std::uint8_t>(rule.kind));
            w.u16(rule.aspect);
            w.u64(static_cast<std::uint64_t>(rule.count));
            write_rational(w, rule.value);
        }
    }
}

inline core::RelationSignature read_signature(std::span<const std::byte> image, std::size_t offset) {
    Reader r(image, offset);
    composite::AspectSchema aspects;
    const auto aspect_count = r.u32();
    if (aspect_count > std::numeric_limits<composite::AspectId>::max()) throw std::runtime_error("RFCX aspect count overflow");
    r.require_items(aspect_count, 6u);
    for (std::uint32_t i = 0; i < aspect_count; ++i) {
        const auto name = r.string();
        const auto role = static_cast<composite::AspectRole>(r.u8());
        const bool default_value = r.boolean();
        const auto id = aspects.add(name, role, default_value);
        if (id != static_cast<composite::AspectId>(i)) throw std::runtime_error("RFCX aspect id mismatch");
    }
    core::RelationSignature sig(std::move(aspects));
    const auto type_count = r.u32();
    r.require_items(type_count, 30u);
    for (std::uint32_t i = 0; i < type_count; ++i) {
        auto name = r.string();
        const auto policy = static_cast<core::BoundaryPolicy>(r.u8());
        const auto min_arity = r.size_value();
        const auto max_arity = r.size_value();
        core::RelationConstraints constraints;
        constraints.shape = static_cast<core::RelationShape>(r.u8());
        const bool has_role = r.boolean();
        const auto role = r.u16();
        if (has_role) constraints.distinguished_role = role;
        const auto constructor = static_cast<core::CompositeConstructorKind>(r.u8());
        const auto rule_count = r.u32();
        r.require_items(rule_count, 21u);
        constraints.rules.reserve(rule_count);
        for (std::uint32_t j = 0; j < rule_count; ++j) {
            core::RelationRule rule;
            rule.kind = static_cast<core::RelationRuleKind>(r.u8());
            rule.aspect = r.u16();
            rule.count = r.size_value();
            rule.value = read_rational(r);
            constraints.rules.push_back(std::move(rule));
        }
        const auto id = sig.add_type(std::move(name), policy, min_arity, max_arity,
                                     std::move(constraints), constructor);
        if (id != i) throw std::runtime_error("RFCX relation type id mismatch");
    }
    if (r.remaining() != 0) throw std::runtime_error("RFCX trailing signature bytes");
    return sig;
}

struct GradeDescriptor {
    std::uint64_t grade{};
    std::uint64_t cell_count{};
    std::uint64_t incidence_count{};
    std::uint64_t aspect_count{};
    std::uint64_t aspect_words{};
    std::uint64_t identities{};
    std::uint64_t relation_keys{};
    std::uint64_t relation_types{};
    std::uint64_t cell_scales{};
    std::uint64_t cell_scale_overflow{};
    std::uint64_t offsets{};
    std::uint64_t lower_index{};
    std::uint64_t upper_index{};
    std::uint64_t occurrence_keys{};
    std::uint64_t lattice{};
    std::uint64_t lattice_overflow{};
    std::uint64_t lower_offsets{};
    std::uint64_t incidence_by_lower{};
    std::uint64_t aspect_words_offset{};
    std::uint64_t identity_index{};
    std::uint64_t relation_key_index{};
    std::uint64_t occurrence_key_index{};
    std::uint64_t relation_type_index{};
};
static_assert(grade_descriptor_size == 23u * 8u);

inline void patch_grade_descriptor(Writer& w, std::size_t offset, const GradeDescriptor& d) {
    const std::uint64_t values[] = {d.grade,d.cell_count,d.incidence_count,d.aspect_count,d.aspect_words,
        d.identities,d.relation_keys,d.relation_types,d.cell_scales,d.cell_scale_overflow,d.offsets,
        d.lower_index,d.upper_index,d.occurrence_keys,d.lattice,d.lattice_overflow,d.lower_offsets,
        d.incidence_by_lower,d.aspect_words_offset,d.identity_index,d.relation_key_index,
        d.occurrence_key_index,d.relation_type_index};
    for (std::size_t i = 0; i < 23; ++i) w.patch_u64(offset + i * 8u, values[i]);
}

inline GradeDescriptor read_grade_descriptor(std::span<const std::byte> bytes, std::size_t offset) {
    Reader r(bytes, offset);
    // Each read initializes an actual member; no pointer arithmetic across
    // unrelated struct subobjects and no assumptions about host padding.
    return GradeDescriptor{r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),
        r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),r.u64(),
        r.u64(),r.u64(),r.u64(),r.u64(),r.u64()};
}

inline std::uint64_t write_sorted_identity_index(Writer& w, const core::GradeArena& arena) {
    if (arena.grade.value != 0 || arena.identities.empty()) return 0;
    if (std::is_sorted(arena.identities.begin(), arena.identities.end())) return 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> index;
    index.reserve(arena.identities.size());
    for (std::size_t i = 0; i < arena.identities.size(); ++i) index.emplace_back(arena.identities[i], i);
    std::sort(index.begin(), index.end());
    w.align(8);
    const auto offset = static_cast<std::uint64_t>(w.size());
    for (const auto& [key, idx] : index) { w.u64(key); w.u64(idx); }
    return offset;
}

inline std::uint64_t write_sorted_relation_key_index(Writer& w, const core::GradeArena& arena) {
    if (arena.grade.value == 0 || arena.relation_identities.empty()) return 0;
    if (std::is_sorted(arena.relation_identities.begin(), arena.relation_identities.end())) return 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> index;
    index.reserve(arena.relation_identities.size());
    for (std::size_t i = 0; i < arena.relation_identities.size(); ++i) index.emplace_back(arena.relation_identities[i].value, i);
    std::sort(index.begin(), index.end());
    w.align(8);
    const auto offset = static_cast<std::uint64_t>(w.size());
    for (const auto& [key, idx] : index) { w.u64(key); w.u64(idx); }
    return offset;
}

inline std::uint64_t write_sorted_occurrence_key_index(Writer& w, const core::GradeArena& arena) {
    if (arena.grade.value == 0 || arena.occurrence_identities.empty()) return 0;
    if (std::is_sorted(arena.occurrence_identities.begin(), arena.occurrence_identities.end())) return 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> index;
    index.reserve(arena.occurrence_identities.size());
    for (std::size_t i = 0; i < arena.occurrence_identities.size(); ++i) index.emplace_back(arena.occurrence_identities[i].value, i);
    std::sort(index.begin(), index.end());
    w.align(8);
    const auto offset = static_cast<std::uint64_t>(w.size());
    for (const auto& [key, idx] : index) { w.u64(key); w.u64(idx); }
    return offset;
}

inline std::uint64_t write_sorted_type_index(Writer& w, const core::GradeArena& arena) {
    if (arena.grade.value == 0 || arena.relation_types.empty()) return 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> index;
    index.reserve(arena.relation_types.size());
    for (std::size_t i = 0; i < arena.relation_types.size(); ++i) {
        if (i > std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("RFCX relation index overflow");
        index.emplace_back(arena.relation_types[i], static_cast<std::uint32_t>(i));
    }
    std::sort(index.begin(), index.end());
    w.align(8);
    const auto offset = static_cast<std::uint64_t>(w.size());
    for (const auto& [type, idx] : index) { w.u32(type); w.u32(idx); }
    return offset;
}

inline void validate_host() {
    static_assert(CHAR_BIT == 8 && sizeof(std::uint64_t) == 8 && sizeof(std::uint32_t) == 4);
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
    if constexpr (std::endian::native != std::endian::little) {
        throw std::runtime_error("RFCX v1 zero-copy reader currently requires a little-endian host");
    }
}

// Validates bases/counts/strides BEFORE address arithmetic. Empty sections
// may share positions; nonempty sections may not overlap metadata or each other.
class SectionRanges {
public:
    SectionRanges(std::size_t bytes, std::size_t header) : size_(bytes), header_(header) {
        if (header > bytes) throw std::runtime_error("RFCX header range");
        ranges_.push_back({0, header});
    }
    void add(std::uint64_t offset, std::uint64_t count, std::uint64_t stride = 1,
             std::size_t alignment = 8) {
        if (offset > size_ || stride == 0 || count > (size_ - offset) / stride)
            throw std::runtime_error("RFCX section extent overflow/range");
        if (count == 0) {
            if (offset != 0 && (offset < header_ || (alignment && offset % alignment)))
                throw std::runtime_error("RFCX empty section position");
            return;
        }
        if (offset < header_ || (alignment && offset % alignment))
            throw std::runtime_error("RFCX section base/alignment");
        ranges_.push_back({offset, offset + count * stride});
    }
    void finish() {
        std::sort(ranges_.begin(), ranges_.end());
        for (std::size_t i=1; i<ranges_.size(); ++i)
            if (ranges_[i].first < ranges_[i-1].second) throw std::runtime_error("RFCX overlapping sections");
    }
private:
    std::uint64_t size_, header_;
    std::vector<std::pair<std::uint64_t,std::uint64_t>> ranges_;
};

inline std::uint64_t load_u64_at(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || 8u > bytes.size()-offset) throw std::runtime_error("RFCX u64 range");
    std::uint64_t value{};
    std::memcpy(&value, bytes.data()+offset, sizeof(value));
    return value;
}
inline std::uint32_t load_u32_at(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || 4u > bytes.size()-offset) throw std::runtime_error("RFCX u32 range");
    std::uint32_t value{};
    std::memcpy(&value, bytes.data()+offset, sizeof(value));
    return value;
}

} // namespace rfcx_detail

struct RfcxWriteStats {
    std::uint64_t bytes{};
    std::uint64_t cells{};
    std::uint64_t incidences{};
    std::uint64_t aspect_words{};
    std::uint64_t payload_hash{};
};

struct RfcxImage {
    std::vector<std::byte> bytes;
    RfcxWriteStats stats;
};

inline RfcxImage encode_rfcx(const core::RelationalComplex& complex) {
    using namespace rfcx_detail;
    validate_host();
    if (complex.grade_count()==0 || complex.grade_count()>std::size_t{1}+std::numeric_limits<std::uint16_t>::max())
        throw std::overflow_error("RFCX grade count is not representable");
    Writer w;
    const auto header = w.reserve(header_size);
    w.align(8);
    const auto signature_offset = static_cast<std::uint64_t>(w.size());
    write_signature(w, complex.signature());
    const auto signature_size = static_cast<std::uint64_t>(w.size()) - signature_offset;
    w.align(8);
    const auto grade_dir_offset = static_cast<std::uint64_t>(w.size());
    const auto grade_dir_size = static_cast<std::uint64_t>(machine::checked_size_mul(complex.grade_count(), grade_descriptor_size));
    w.reserve(static_cast<std::size_t>(grade_dir_size));

    RfcxWriteStats stats;
    std::vector<GradeDescriptor> descriptors;
    descriptors.reserve(complex.grade_count());

    for (std::size_t g = 0; g < complex.grade_count(); ++g) {
        const auto& arena = complex.grade(core::Grade{static_cast<std::uint16_t>(g)});
        GradeDescriptor d;
        d.grade = g;
        d.cell_count = arena.cell_count();
        d.incidence_count = arena.incidence_count();
        if (d.cell_count>std::numeric_limits<std::uint32_t>::max() || d.incidence_count>std::numeric_limits<std::uint32_t>::max())
            throw std::overflow_error("RFCX retained index count is not representable");
        d.aspect_count = complex.signature().aspects().size();
        d.aspect_words = arena.grade.value == 0 ? 0u : machine::PackedBits::words_for_size(arena.incidence_count());
        stats.cells += d.cell_count;
        stats.incidences += d.incidence_count;
        stats.aspect_words += d.aspect_words * d.aspect_count;

        if (g == 0) {
            d.identities = write_u64_array(w, arena.identities);
        } else {
            std::vector<std::uint64_t> keys;
            keys.reserve(arena.relation_identities.size());
            for (const auto key : arena.relation_identities) keys.push_back(key.value);
            d.relation_keys = write_u64_array(w, keys);
        }
        d.relation_types = write_u32_array(w, arena.relation_types);

        std::vector<numeric::Integer> scales;
        scales.reserve(arena.cell_scales.size());
        for (const auto& scale : arena.cell_scales) {
            if (scale.get_den() != 1) throw std::logic_error("RFCX requires integral retained cell scale");
            scales.push_back(scale.get_num());
        }
        const auto scale_sections = write_integer_lane_complete(w, scales);
        d.cell_scales = scale_sections.lane;
        d.cell_scale_overflow = scale_sections.overflow;

        if (g > 0) {
            d.offsets = write_size_array(w, arena.offsets);
            d.lower_index = write_u32_array(w, arena.lower_index);
            d.upper_index = write_u32_array(w, arena.upper_index);
            std::vector<std::uint64_t> occ;
            occ.reserve(arena.occurrence_identities.size());
            for (const auto key : arena.occurrence_identities) occ.push_back(key.value);
            d.occurrence_keys = write_u64_array(w, occ);
            const auto lattice_sections = write_integer_lane_complete(w, arena.lattice_coefficient);
            d.lattice = lattice_sections.lane;
            d.lattice_overflow = lattice_sections.overflow;
            d.lower_offsets = write_size_array(w, arena.lower_offsets);
            d.incidence_by_lower = write_size_array(w, arena.incidence_by_lower);

            w.align(8);
            d.aspect_words_offset = static_cast<std::uint64_t>(w.size());
            for (std::size_t a = 0; a < complex.signature().aspects().size(); ++a) {
                const auto& plane = arena.aspects.plane(static_cast<composite::AspectId>(a));
                for (std::size_t word = 0; word < plane.word_count(); ++word) w.u64(plane.word(word));
            }
            d.relation_key_index = write_sorted_relation_key_index(w, arena);
            d.occurrence_key_index = write_sorted_occurrence_key_index(w, arena);
            d.relation_type_index = write_sorted_type_index(w, arena);
        } else {
            d.identity_index = write_sorted_identity_index(w, arena);
        }
        descriptors.push_back(d);
    }

    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        patch_grade_descriptor(w, static_cast<std::size_t>(grade_dir_offset) + i * grade_descriptor_size,
                               descriptors[i]);
    }

    // Header, written after all offsets and payload sizes are known.
    for (std::size_t i = 0; i < magic.size(); ++i) {
        // patch_u8 is not needed elsewhere.
        w.patch_byte(header + i, magic[i]);
    }
    w.patch_u32(header + 8u, version);
    w.patch_u32(header + 12u, endian_tag);
    w.patch_u64(header + 16u, static_cast<std::uint64_t>(w.size()));
    w.patch_u32(header + 24u, complex.semantic_scale().value);
    w.patch_u32(header + 28u, static_cast<std::uint32_t>(complex.grade_count()));
    w.patch_u32(header + 32u, static_cast<std::uint32_t>(complex.signature().aspects().size()));
    w.patch_u32(header + 36u, static_cast<std::uint32_t>(complex.signature().type_count()));
    w.patch_u64(header + 40u, signature_offset);
    w.patch_u64(header + 48u, signature_size);
    w.patch_u64(header + 56u, grade_dir_offset);
    w.patch_u64(header + 64u, grade_dir_size);
    const auto payload_hash = fnv1a64(std::span<const std::byte>(w.bytes()).subspan(header_size));
    w.patch_u64(header + 72u, payload_hash);
    // 80..95 reserved for future flags/secondary directories.

    stats.bytes = static_cast<std::uint64_t>(w.size());
    stats.payload_hash = payload_hash;
    return {w.bytes(), stats};
}

inline RfcxWriteStats write_rfcx(const core::RelationalComplex& complex,
                                 const std::filesystem::path& path) {
    const auto image = encode_rfcx(complex);
    write_file_exact(path, image.bytes);
    return image.stats;
}

class MappedRfcx {
    struct ViewLifetime { const MappedRfcx* owner; };
public:
    explicit MappedRfcx(const std::filesystem::path& path, bool verify_integrity = true)
        : file_(path) {
        rfcx_detail::validate_host();
        parse(verify_integrity);
    }

    explicit MappedRfcx(std::span<const std::byte> bytes, bool verify_integrity = true)
        : file_(bytes) {
        rfcx_detail::validate_host();
        parse(verify_integrity);
    }

    MappedRfcx(const MappedRfcx&) = delete;
    MappedRfcx& operator=(const MappedRfcx&) = delete;
    MappedRfcx(MappedRfcx&& other) noexcept
        : file_(std::move(other.file_)), semantic_scale_(other.semantic_scale_),
          signature_(std::move(other.signature_)), grades_(std::move(other.grades_)), payload_hash_(other.payload_hash_), lifetime_(std::move(other.lifetime_)) {
            rebind_grade_views(); other.grades_.clear(); other.payload_hash_ = 0;
        }
    MappedRfcx& operator=(MappedRfcx&& other) noexcept {
        if (this != &other) {
            file_ = std::move(other.file_);
            semantic_scale_ = other.semantic_scale_;
            signature_ = std::move(other.signature_);
            grades_ = std::move(other.grades_);
            payload_hash_ = other.payload_hash_;
            lifetime_ = std::move(other.lifetime_);
            rebind_grade_views();
            other.grades_.clear(); other.payload_hash_ = 0;
        }
        return *this;
    }

    [[nodiscard]] std::size_t grade_count() const noexcept { return grades_.size(); }
    [[nodiscard]] core::SemanticScale semantic_scale() const noexcept { return semantic_scale_; }
    [[nodiscard]] const core::RelationSignature& signature() const noexcept { return signature_; }
    [[nodiscard]] std::uint64_t file_bytes() const noexcept { return file_.size(); }
    [[nodiscard]] std::uint64_t payload_hash() const noexcept { return payload_hash_; }

    struct GradeView {
        const MappedRfcx* owner{};
        rfcx_detail::GradeDescriptor desc{};
        std::weak_ptr<ViewLifetime> lifetime;
        [[nodiscard]] const MappedRfcx* live_owner() const {
            const auto state = lifetime.lock();
            if (!state) throw std::logic_error("expired RFCX grade view");
            return state->owner;
        }

        [[nodiscard]] std::size_t cell_count() const noexcept { return static_cast<std::size_t>(desc.cell_count); }
        [[nodiscard]] std::size_t incidence_count() const noexcept { return static_cast<std::size_t>(desc.incidence_count); }
        [[nodiscard]] core::Grade grade() const noexcept { return core::Grade{static_cast<std::uint16_t>(desc.grade)}; }
        [[nodiscard]] std::size_t aspect_word_count() const noexcept { return static_cast<std::size_t>(desc.aspect_words); }

        [[nodiscard]] core::IdentityKey identity(std::size_t index) const {
            if (desc.grade != 0 || index >= desc.cell_count) throw std::out_of_range("RFCX identity index");
            return live_owner()->u64_array_value(desc.identities, index);
        }
        [[nodiscard]] core::RelationKey relation_key(std::size_t index) const {
            if (desc.grade == 0 || index >= desc.cell_count) throw std::out_of_range("RFCX relation index");
            return core::RelationKey{live_owner()->u64_array_value(desc.relation_keys, index)};
        }
        [[nodiscard]] core::RelationTypeId relation_type(std::size_t index) const {
            if (index >= desc.cell_count) throw std::out_of_range("RFCX type index");
            return live_owner()->u32_array_value(desc.relation_types, index);
        }
        [[nodiscard]] numeric::Integer cell_scale(std::size_t index) const {
            if (index >= desc.cell_count) throw std::out_of_range("RFCX scale index");
            return live_owner()->integer_lane_value(desc.cell_scales, desc.cell_scale_overflow, index);
        }
        [[nodiscard]] std::pair<std::size_t,std::size_t> incidence_range(std::size_t upper) const {
            if (desc.grade == 0 || upper >= desc.cell_count) throw std::out_of_range("RFCX upper incidence range");
            return {static_cast<std::size_t>(live_owner()->u64_array_value(desc.offsets, upper)),
                    static_cast<std::size_t>(live_owner()->u64_array_value(desc.offsets, upper + 1u))};
        }
        [[nodiscard]] std::pair<std::size_t,std::size_t> coface_incidence_range(std::size_t lower) const {
            if (desc.grade == 0) throw std::logic_error("RFCX grade zero has no incidence");
            const auto lower_count = live_owner()->grades_.at(static_cast<std::size_t>(desc.grade) - 1u).desc.cell_count;
            if (lower >= lower_count) throw std::out_of_range("RFCX lower coface range");
            return {static_cast<std::size_t>(live_owner()->u64_array_value(desc.lower_offsets, lower)),
                    static_cast<std::size_t>(live_owner()->u64_array_value(desc.lower_offsets, lower + 1u))};
        }
        [[nodiscard]] std::uint32_t lower_index(std::size_t incidence) const {
            if (incidence >= desc.incidence_count) throw std::out_of_range("RFCX lower incidence");
            return live_owner()->u32_array_value(desc.lower_index, incidence);
        }
        [[nodiscard]] std::uint32_t upper_index(std::size_t incidence) const {
            if (incidence >= desc.incidence_count) throw std::out_of_range("RFCX upper incidence");
            return live_owner()->u32_array_value(desc.upper_index, incidence);
        }
        [[nodiscard]] std::size_t incidence_by_lower(std::size_t slot) const {
            if (slot >= desc.incidence_count) throw std::out_of_range("RFCX reverse incidence");
            return static_cast<std::size_t>(live_owner()->u64_array_value(desc.incidence_by_lower, slot));
        }
        [[nodiscard]] core::OccurrenceKey occurrence_key(std::size_t incidence) const {
            if (incidence >= desc.incidence_count) throw std::out_of_range("RFCX occurrence key");
            return core::OccurrenceKey{live_owner()->u64_array_value(desc.occurrence_keys, incidence)};
        }
        [[nodiscard]] numeric::Integer lattice_coefficient(std::size_t incidence) const {
            if (incidence >= desc.incidence_count) throw std::out_of_range("RFCX lattice coefficient");
            return live_owner()->integer_lane_value(desc.lattice, desc.lattice_overflow, incidence);
        }
        [[nodiscard]] std::uint64_t aspect_word(composite::AspectId aspect, std::size_t word) const {
            if (desc.grade == 0 || aspect >= desc.aspect_count || word >= desc.aspect_words) {
                throw std::out_of_range("RFCX aspect word");
            }
            const auto linear = static_cast<std::size_t>(aspect) * static_cast<std::size_t>(desc.aspect_words) + word;
            return live_owner()->u64_array_value(desc.aspect_words_offset, linear);
        }
        [[nodiscard]] bool aspect(composite::AspectId aspect, std::size_t incidence) const {
            if (incidence >= desc.incidence_count) throw std::out_of_range("RFCX aspect incidence");
            const auto word = incidence >> 6u;
            const auto bit = incidence & 63u;
            return ((aspect_word(aspect, word) >> bit) & 1ull) != 0;
        }
        [[nodiscard]] numeric::Rational incidence_coefficient(std::size_t incidence) const {
            if (desc.grade == 0 || incidence >= desc.incidence_count) throw std::out_of_range("RFCX incidence coefficient");
            const auto lower = lower_index(incidence);
            const auto upper = upper_index(incidence);
            const auto lower_scale = live_owner()->grades_.at(static_cast<std::size_t>(desc.grade) - 1u).cell_scale(lower);
            const auto upper_scale = cell_scale(upper);
            numeric::Rational value(lower_scale * lattice_coefficient(incidence));
            value /= numeric::Rational(upper_scale);
            value.canonicalize();
            return value;
        }

        [[nodiscard]] std::optional<std::uint32_t> find_identity(core::IdentityKey key) const {
            if (desc.grade != 0) return std::nullopt;
            if (desc.identity_index != 0) return live_owner()->binary_search_index(desc.identity_index, desc.cell_count, key);
            return live_owner()->binary_search_u64_source(desc.identities, desc.cell_count, key);
        }
        [[nodiscard]] std::optional<std::uint32_t> find_relation(core::RelationKey key) const {
            if (desc.grade == 0) return std::nullopt;
            if (desc.relation_key_index != 0) return live_owner()->binary_search_index(desc.relation_key_index, desc.cell_count, key.value);
            return live_owner()->binary_search_u64_source(desc.relation_keys, desc.cell_count, key.value);
        }
        [[nodiscard]] std::optional<std::uint32_t> find_occurrence(core::OccurrenceKey key) const {
            if (desc.grade == 0) return std::nullopt;
            if (desc.occurrence_key_index != 0) return live_owner()->binary_search_index(desc.occurrence_key_index, desc.incidence_count, key.value);
            return live_owner()->binary_search_u64_source(desc.occurrence_keys, desc.incidence_count, key.value);
        }

        template <class F>
        void for_each_relation_of_type(core::RelationTypeId type, F&& fn) const {
            if (desc.grade == 0 || desc.relation_type_index == 0) return;
            std::size_t lo = 0;
            std::size_t hi = static_cast<std::size_t>(desc.cell_count);
            while (lo < hi) {
                const auto mid = (lo + hi) / 2u;
                const auto record = desc.relation_type_index + static_cast<std::uint64_t>(mid) * 8u;
                const auto found = live_owner()->u32_at(record);
                if (found < type) lo = mid + 1u; else hi = mid;
            }
            for (std::size_t i = lo; i < static_cast<std::size_t>(desc.cell_count); ++i) {
                const auto record = desc.relation_type_index + static_cast<std::uint64_t>(i) * 8u;
                const auto found = live_owner()->u32_at(record);
                if (found != type) break;
                fn(live_owner()->u32_at(record + 4u));
            }
        }
    };

    [[nodiscard]] GradeView grade(core::Grade grade) const {
        if (grade.value >= grades_.size()) throw std::out_of_range("RFCX grade");
        return grades_[grade.value];
    }

    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse(
        core::Grade upper_grade, numeric::SparseRationalView field) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("RFCX boundary grade");
        const auto upper = grade(upper_grade);
        const auto lower = grade(core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)});
        if (field.size() != upper.cell_count()) throw std::invalid_argument("RFCX boundary field size");
        const auto exists = signature_.existence_aspect();
        std::vector<numeric::SparseRationalEntry> contributions;
        std::size_t reserve_hint = 0;
        for (const auto& active : field.entries()) {
            const auto [b,e] = upper.incidence_range(active.index);
            reserve_hint += e-b;
        }
        contributions.reserve(reserve_hint);
        for (const auto& active : field.entries()) {
            numeric::Rational scaled = active.value / lane_rational(upper.desc.cell_scales, upper.desc.cell_scale_overflow, active.index);
            scaled.canonicalize();
            const auto [b,e] = upper.incidence_range(active.index);
            for (std::size_t p=b; p<e; ++p) {
                if (!upper.aspect(exists,p)) continue;
                const auto coeff = lane_rational(upper.desc.lattice, upper.desc.lattice_overflow, p);
                if (coeff == 0) continue;
                contributions.push_back({upper.lower_index(p), scaled * coeff});
            }
        }
        const numeric::SparseRationalVector lattice(lower.cell_count(), std::move(contributions));
        std::vector<numeric::SparseRationalEntry> realized;
        realized.reserve(lattice.nonzeros());
        for (const auto& entry : lattice.entries()) {
            numeric::Rational value = entry.value * lane_rational(lower.desc.cell_scales, lower.desc.cell_scale_overflow, entry.index);
            value.canonicalize();
            if (value != 0) realized.push_back({entry.index, std::move(value)});
        }
        return numeric::SparseRationalVector(lower.cell_count(), std::move(realized));
    }

    [[nodiscard]] numeric::SparseRationalVector transpose_apply_sparse(
        core::Grade upper_grade, numeric::SparseRationalView field) const {
        if (upper_grade.value == 0 || upper_grade.value >= grades_.size()) throw std::out_of_range("RFCX transpose grade");
        const auto upper = grade(upper_grade);
        const auto lower = grade(core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)});
        if (field.size() != lower.cell_count()) throw std::invalid_argument("RFCX transpose field size");
        const auto exists = signature_.existence_aspect();
        std::vector<numeric::SparseRationalEntry> contributions;
        std::size_t reserve_hint = 0;
        for (const auto& active : field.entries()) {
            const auto [b,e] = upper.coface_incidence_range(active.index);
            reserve_hint += e-b;
        }
        contributions.reserve(reserve_hint);
        for (const auto& active : field.entries()) {
            numeric::Rational scaled = active.value * lane_rational(lower.desc.cell_scales, lower.desc.cell_scale_overflow, active.index);
            scaled.canonicalize();
            const auto [b,e] = upper.coface_incidence_range(active.index);
            for (std::size_t q=b; q<e; ++q) {
                const auto p = upper.incidence_by_lower(q);
                if (!upper.aspect(exists,p)) continue;
                const auto coeff = lane_rational(upper.desc.lattice, upper.desc.lattice_overflow, p);
                if (coeff == 0) continue;
                contributions.push_back({upper.upper_index(p), scaled * coeff});
            }
        }
        const numeric::SparseRationalVector lattice(upper.cell_count(), std::move(contributions));
        std::vector<numeric::SparseRationalEntry> realized;
        realized.reserve(lattice.nonzeros());
        for (const auto& entry : lattice.entries()) {
            numeric::Rational value = entry.value / lane_rational(upper.desc.cell_scales, upper.desc.cell_scale_overflow, entry.index);
            value.canonicalize();
            if (value != 0) realized.push_back({entry.index, std::move(value)});
        }
        return numeric::SparseRationalVector(upper.cell_count(), std::move(realized));
    }

    [[nodiscard]] std::uint64_t binary_count(core::Grade grade_id,
                                             const composite::BinaryProgram& program,
                                             std::uint16_t output_register) const {
        const auto g = grade(grade_id);
        if (g.grade().value == 0) return 0;
        const composite::BinaryExecutionPlan plan(program, {output_register});
        composite::BinaryExecutionWorkspace workspace;
        return binary_count(grade_id, plan, workspace);
    }

    // Prepared physical plans/workspace can be reused across calls. They never
    // replace persisted BinaryProgram definitions or source declaration checks.
    [[nodiscard]] std::vector<std::uint64_t> binary_counts(
        core::Grade grade_id, const composite::BinaryExecutionPlan& plan,
        composite::BinaryExecutionWorkspace& workspace) const {
        const auto g = grade(grade_id);
        plan.validate_aspects(signature_.aspects().size());
        std::vector<std::uint64_t> total(plan.output_count(), 0);
        if (g.grade().value == 0) return total;
        plan.for_each_word(g.incidence_count(),
            [&](composite::AspectId aspect, std::size_t wi) { return g.aspect_word(aspect, wi); },
            [&](std::size_t, std::span<const std::uint64_t> bits) {
                for (std::size_t i = 0; i < bits.size(); ++i)
                    total[i] += static_cast<std::uint64_t>(std::popcount(bits[i]));
            }, workspace);
        return total;
    }

    [[nodiscard]] std::uint64_t binary_count(
        core::Grade grade_id, const composite::BinaryExecutionPlan& plan,
        composite::BinaryExecutionWorkspace& workspace) const {
        if (plan.output_count() != 1) throw std::invalid_argument("binary_count needs exactly one output");
        const auto g = grade(grade_id);
        plan.validate_aspects(signature_.aspects().size());
        if (g.grade().value == 0) return 0;
        std::uint64_t total = 0;
        plan.for_each_word(g.incidence_count(),
            [&](composite::AspectId aspect, std::size_t wi) { return g.aspect_word(aspect, wi); },
            [&](std::size_t, std::span<const std::uint64_t> bits) {
                total += static_cast<std::uint64_t>(std::popcount(bits.front()));
            }, workspace);
        return total;
    }

    [[nodiscard]] core::RelationalComplex materialize() const {
        validate_occurrence_identities();
        auto arenas = decode_retained_arenas();
        if (!canonical_retained_lanes(arenas)) {
            arenas.clear();
            return materialize_from_assertions();
        }
        return core::RelationalComplexBuilder(signature_, semantic_scale_).freeze_retained(std::move(arenas));
    }

    // Bounded preparation can require the retained canonical lanes directly.
    // It must not enter the legacy assertion-reconstruction fallback, whose
    // normalization may have a different allocation/bit-growth contract.
    [[nodiscard]] core::RelationalComplex materialize_canonical() const {
        validate_occurrence_identities();
        auto arenas = decode_retained_arenas();
        if (!canonical_retained_lanes(arenas))
            throw std::invalid_argument("canonical RFCX retained lanes required");
        return core::RelationalComplexBuilder(signature_, semantic_scale_).freeze_retained(std::move(arenas));
    }

private:
    [[nodiscard]] std::vector<core::GradeArena> decode_retained_arenas() const {
        std::vector<core::GradeArena> arenas;
        arenas.reserve(grade_count());
        for (const auto& view : grades_) {
            const auto& d = view.desc;
            core::GradeArena arena;
            arena.grade = view.grade();
            arena.relation_types.reserve(view.cell_count());
            arena.cell_scales.reserve(view.cell_count());
            if (d.grade == 0) arena.identities.reserve(view.cell_count());
            else arena.relation_identities.reserve(view.cell_count());
            for (std::size_t cell = 0; cell < view.cell_count(); ++cell) {
                arena.relation_types.push_back(u32_array_value(d.relation_types, cell));
                arena.cell_scales.push_back(lane_rational(d.cell_scales, d.cell_scale_overflow, cell));
                if (d.grade == 0) arena.identities.push_back(u64_array_value(d.identities, cell));
                else arena.relation_identities.push_back(core::RelationKey{u64_array_value(d.relation_keys, cell)});
            }
            const auto words = machine::checked_size_mul(signature_.aspects().size(), static_cast<std::size_t>(d.aspect_words));
            arena.aspects = composite::AspectPlanes(signature_.aspects(), view.incidence_count(), words);
            if (d.grade == 0) {
                arena.offsets.push_back(0);
            } else {
                arena.offsets.reserve(view.cell_count() + 1u);
                for (std::size_t i = 0; i <= view.cell_count(); ++i)
                    arena.offsets.push_back(static_cast<std::size_t>(u64_array_value(d.offsets, i)));
                arena.lower_index.reserve(view.incidence_count());
                arena.upper_index.reserve(view.incidence_count());
                arena.occurrence_identities.reserve(view.incidence_count());
                arena.lattice_coefficient.reserve(view.incidence_count());
                arena.incidence_by_lower.reserve(view.incidence_count());
                for (std::size_t slot = 0; slot < view.incidence_count(); ++slot) {
                    arena.lower_index.push_back(u32_array_value(d.lower_index, slot));
                    arena.upper_index.push_back(u32_array_value(d.upper_index, slot));
                    arena.occurrence_identities.push_back(core::OccurrenceKey{u64_array_value(d.occurrence_keys, slot)});
                    arena.lattice_coefficient.push_back(integer_lane_value(d.lattice, d.lattice_overflow, slot));
                    arena.incidence_by_lower.push_back(static_cast<std::size_t>(u64_array_value(d.incidence_by_lower, slot)));
                }
                const auto lower_count = arenas.back().cell_count();
                arena.lower_offsets.reserve(lower_count + 1u);
                for (std::size_t i = 0; i <= lower_count; ++i)
                    arena.lower_offsets.push_back(static_cast<std::size_t>(u64_array_value(d.lower_offsets, i)));
                for (std::size_t a = 0; a < signature_.aspects().size(); ++a) {
                    auto& plane = arena.aspects.plane(static_cast<composite::AspectId>(a));
                    for (std::size_t word = 0; word < plane.word_count(); ++word)
                        plane.assign_word(word, u64_array_value(d.aspect_words_offset, a * plane.word_count() + word));
                }
            }
            arenas.push_back(std::move(arena));
        }
        return arenas;
    }

    [[nodiscard]] bool canonical_retained_lanes(const std::vector<core::GradeArena>& arenas) const {
        for (std::size_t k = 1; k < arenas.size(); ++k) {
            const auto& arena = arenas[k];
            for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
                const auto& scale = arena.cell_scales[cell];
                if (scale == 1) continue;
                numeric::Integer expected = 1;
                const auto [begin, end] = arena.incidence_range(cell);
                for (auto slot = begin; slot < end; ++slot) {
                    numeric::Rational ratio(arena.lattice_coefficient[slot], scale.get_num());
                    ratio.canonicalize();
                    expected = numeric::lcm_positive(expected, numeric::denominator(ratio));
                }
                if (scale.get_num() != expected) return false;
            }
            for (std::size_t cell = 0; cell < arenas[k - 1u].cell_count(); ++cell) {
                const auto [begin, end] = arena.coface_incidence_range(cell);
                for (auto slot = begin + (begin != end); slot < end; ++slot)
                    if (arena.incidence_by_lower[slot - 1u] >= arena.incidence_by_lower[slot]) return false;
            }
        }
        return true;
    }

    void validate_occurrence_identities() const {
        if (grade_count() < 3) return; // One incidence grade is already checked by parse().
        using Cursor = std::tuple<std::uint64_t, std::size_t, std::size_t>;
        std::priority_queue<Cursor, std::vector<Cursor>, std::greater<Cursor>> cursors;
        const auto key = [&](std::size_t grade, std::size_t slot) {
            const auto& d = grades_[grade].desc;
            return u64_array_value(d.occurrence_key_index ? d.occurrence_key_index : d.occurrence_keys,
                                   d.occurrence_key_index ? 2u * slot : slot);
        };
        for (std::size_t grade = 1; grade < grade_count(); ++grade)
            if (grades_[grade].incidence_count()) cursors.emplace(key(grade, 0), grade, 0);
        std::optional<std::uint64_t> previous;
        while (!cursors.empty()) {
            const auto [current, grade, slot] = cursors.top();
            cursors.pop();
            if (previous && *previous == current) throw std::invalid_argument("duplicate boundary occurrence identity");
            previous = current;
            if (slot + 1u < grades_[grade].incidence_count()) cursors.emplace(key(grade, slot + 1u), grade, slot + 1u);
        }
    }

    [[nodiscard]] core::RelationalComplex materialize_from_assertions() const {
        core::RelationalComplexBuilder builder(signature_, semantic_scale_);
        builder.declare_grade(core::Grade{static_cast<std::uint16_t>(grade_count() - 1u)});
        const auto g0 = grade(core::Grade{0});
        for (std::size_t i=0; i<g0.cell_count(); ++i) builder.declare_witness_identity(g0.identity(i));
        const auto exists = signature_.existence_aspect();
        const auto orientation = signature_.orientation_aspect();
        for (std::size_t gi=1; gi<grade_count(); ++gi) {
            const auto g = grade(core::Grade{static_cast<std::uint16_t>(gi)});
            for (std::size_t cell=0; cell<g.cell_count(); ++cell) {
                const auto [b,e] = g.incidence_range(cell);
                if (gi == 1) {
                    std::vector<core::BoundaryOccurrenceInput> incidences;
                    incidences.reserve(e-b);
                    for (std::size_t p=b; p<e; ++p) {
                        std::vector<core::AspectValue> extras;
                        extras.reserve(signature_.aspects().size());
                        for (std::size_t a=0; a<signature_.aspects().size(); ++a) {
                            const auto aid = static_cast<composite::AspectId>(a);
                            if (aid == exists || aid == orientation) continue;
                            extras.push_back({aid, g.aspect(aid,p)});
                        }
                        auto coeff = g.incidence_coefficient(p);
                        if (coeff < 0) coeff = -coeff;
                        incidences.emplace_back(g0.identity(g.lower_index(p)), g.aspect(exists,p),
                                                g.aspect(orientation,p), std::move(coeff), std::move(extras),
                                                g.occurrence_key(p));
                    }
                    builder.add_grade_one(g.relation_key(cell), g.relation_type(cell), incidences);
                } else {
                    std::vector<core::HigherIncidenceInput> incidences;
                    incidences.reserve(e-b);
                    for (std::size_t p=b; p<e; ++p) {
                        std::vector<core::AspectValue> extras;
                        extras.reserve(signature_.aspects().size());
                        for (std::size_t a=0; a<signature_.aspects().size(); ++a) {
                            const auto aid = static_cast<composite::AspectId>(a);
                            if (aid == exists || aid == orientation) continue;
                            extras.push_back({aid, g.aspect(aid,p)});
                        }
                        auto coeff = g.incidence_coefficient(p);
                        if (coeff < 0) coeff = -coeff;
                        incidences.emplace_back(g.lower_index(p), g.aspect(exists,p), g.aspect(orientation,p),
                                                std::move(coeff), std::move(extras), g.occurrence_key(p));
                    }
                    builder.add_higher(core::Grade{static_cast<std::uint16_t>(gi)}, g.relation_key(cell),
                                       g.relation_type(cell), incidences);
                }
            }
        }
        return builder.freeze();
    }

private:
    void rebind_grade_views() noexcept {
        if (lifetime_) lifetime_->owner = this;
        for (auto& grade : grades_) { grade.owner = this; grade.lifetime = lifetime_; }
    }

    void parse(bool verify_integrity) {
        using namespace rfcx_detail;
        const auto bytes = file_.bytes();
        if (bytes.size() < header_size) throw std::runtime_error("RFCX image too small");
        for (std::size_t i=0; i<magic.size(); ++i) if (bytes[i] != magic[i]) throw std::runtime_error("RFCX magic mismatch");
        Reader r(bytes, 8);
        if (r.u32() != version) throw std::runtime_error("unsupported RFCX version");
        if (r.u32() != endian_tag) throw std::runtime_error("RFCX endian marker mismatch");
        const auto declared_size = r.u64();
        const auto semantic_scale_raw = r.u32();
        if (semantic_scale_raw > std::numeric_limits<std::uint16_t>::max()) throw std::runtime_error("RFCX semantic scale overflow");
        semantic_scale_ = core::SemanticScale{static_cast<std::uint16_t>(semantic_scale_raw)};
        const auto grade_count = r.u32();
        const auto aspect_count = r.u32();
        const auto type_count = r.u32();
        const auto signature_offset = r.u64();
        const auto signature_size = r.u64();
        const auto grade_dir_offset = r.u64();
        const auto grade_dir_size = r.u64();
        const auto payload_hash = r.u64();
        payload_hash_ = payload_hash;
        if (declared_size != bytes.size()) throw std::runtime_error("RFCX file size mismatch");
        if (grade_count == 0 || grade_count > std::uint32_t{1} + std::numeric_limits<std::uint16_t>::max())
            throw std::runtime_error("RFCX grade count overflow");
        if (r.u64() != 0 || r.u64() != 0) throw std::runtime_error("RFCX unsupported header flags");
        SectionRanges sections(bytes.size(), header_size);
        sections.add(signature_offset, signature_size);
        sections.add(grade_dir_offset, grade_count, grade_descriptor_size);
        if (signature_offset > bytes.size() || signature_size > bytes.size()-signature_offset) throw std::runtime_error("RFCX signature range");
        if (grade_dir_size != static_cast<std::uint64_t>(grade_count) * grade_descriptor_size) throw std::runtime_error("RFCX grade directory size");
        if (grade_dir_offset > bytes.size() || grade_dir_size > bytes.size()-grade_dir_offset) throw std::runtime_error("RFCX grade directory range");
        if (verify_integrity && fnv1a64(bytes.subspan(header_size)) != payload_hash) throw std::runtime_error("RFCX payload integrity failure");
        signature_ = read_signature(bytes.subspan(static_cast<std::size_t>(signature_offset),
                                                   static_cast<std::size_t>(signature_size)), 0);
        if (signature_.aspects().size() != aspect_count || signature_.type_count() != type_count) throw std::runtime_error("RFCX signature count mismatch");
        grades_.reserve(grade_count);
        for (std::size_t i=0; i<grade_count; ++i) {
            const auto d = read_grade_descriptor(bytes, static_cast<std::size_t>(grade_dir_offset) + i*grade_descriptor_size);
            if (d.grade != i || d.aspect_count != aspect_count) throw std::runtime_error("RFCX grade descriptor mismatch");
            if (d.cell_count > std::numeric_limits<std::uint32_t>::max() || d.incidence_count > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("RFCX retained index count overflow");
            grades_.push_back(GradeView{this,d,lifetime_});
        }
        for (const auto& g : grades_) validate_grade_sections(g.desc, sections);
        sections.finish();
        for (const auto& g : grades_) validate_grade_contents(g);
    }

    void validate_integer_sections(std::uint64_t lane, std::uint64_t overflow, std::uint64_t count,
                                   rfcx_detail::SectionRanges& sections) const {
        sections.add(lane, count, 8u);
        if (overflow == 0) {
            for (std::size_t i=0; i<count; ++i)
                if (static_cast<std::int64_t>(u64_array_value(lane,i)) == rfcx_detail::overflow_sentinel)
                    throw std::runtime_error("RFCX missing integer overflow table");
            return;
        }
        // First bound the count word, then the entire directory, before loading records.
        require_range(overflow, 8u);
        const auto n = u64_array_value(overflow,0);
        if (n == 0 || n > count || n > (file_.size() - overflow - 8u) / 32u)
            throw std::runtime_error("RFCX integer overflow directory range");
        sections.add(overflow, 8u + n * 32u);
        std::uint64_t previous = 0;
        for (std::size_t i=0; i<n; ++i) {
            const auto rec = overflow + 8u + i * 32u;
            const auto key = u64_array_value(rec,0);
            const auto blob = u64_array_value(rec,1);
            const auto size = u32_at(rec+16u);
            const auto flags = u32_at(rec+20u);
            if (key >= count || (i && key <= previous) || flags > 1 || u64_array_value(rec,3) != 0)
                throw std::runtime_error("RFCX invalid integer overflow record");
            if (static_cast<std::int64_t>(u64_array_value(lane,key)) != rfcx_detail::overflow_sentinel)
                throw std::runtime_error("RFCX spurious integer overflow record");
            if (size == 0) throw std::runtime_error("RFCX empty overflow integer");
            sections.add(blob,size);
            if (file_.bytes()[static_cast<std::size_t>(blob)] == std::byte{0})
                throw std::runtime_error("RFCX noncanonical overflow integer");
            previous = key;
        }
        std::uint64_t markers=0;
        for (std::size_t i=0; i<count; ++i)
            if (static_cast<std::int64_t>(u64_array_value(lane,i)) == rfcx_detail::overflow_sentinel) ++markers;
        if (markers != n) throw std::runtime_error("RFCX incomplete integer overflow directory");
    }

    void validate_grade_sections(const rfcx_detail::GradeDescriptor& d,
                                 rfcx_detail::SectionRanges& sections) const {
        if (d.grade == 0) {
            if (d.incidence_count || d.aspect_words || d.relation_keys || d.offsets || d.lower_index ||
                d.upper_index || d.occurrence_keys || d.lattice || d.lattice_overflow || d.lower_offsets ||
                d.incidence_by_lower || d.aspect_words_offset || d.relation_key_index || d.occurrence_key_index || d.relation_type_index)
                throw std::runtime_error("RFCX grade-zero incidence fields");
            sections.add(d.identities,d.cell_count,8u);
            if (d.identity_index) sections.add(d.identity_index,d.cell_count,16u);
        } else {
            if (d.identities || d.identity_index) throw std::runtime_error("RFCX positive-grade identity fields");
            if (d.aspect_words != d.incidence_count / 64u + (d.incidence_count % 64u != 0))
                throw std::runtime_error("RFCX aspect word count mismatch");
            sections.add(d.relation_keys,d.cell_count,8u);
            sections.add(d.offsets,d.cell_count+1u,8u);
            sections.add(d.lower_index,d.incidence_count,4u);
            sections.add(d.upper_index,d.incidence_count,4u);
            sections.add(d.occurrence_keys,d.incidence_count,8u);
            validate_integer_sections(d.lattice,d.lattice_overflow,d.incidence_count,sections);
            const auto lower_count = grades_.at(static_cast<std::size_t>(d.grade)-1u).desc.cell_count;
            sections.add(d.lower_offsets,lower_count+1u,8u);
            sections.add(d.incidence_by_lower,d.incidence_count,8u);
            // Counts are already bounded by the 32-bit incidence and 16-bit aspect declarations.
            sections.add(d.aspect_words_offset,d.aspect_count*d.aspect_words,8u);
            if (d.relation_key_index) sections.add(d.relation_key_index,d.cell_count,16u);
            if (d.occurrence_key_index) sections.add(d.occurrence_key_index,d.incidence_count,16u);
            sections.add(d.relation_type_index,d.cell_count,8u);
        }
        sections.add(d.relation_types,d.cell_count,4u);
        validate_integer_sections(d.cell_scales,d.cell_scale_overflow,d.cell_count,sections);
    }

    void validate_key_index(std::uint64_t lane, std::uint64_t index, std::uint64_t count) const {
        std::uint64_t previous=0;
        for (std::size_t i=0; i<count; ++i) {
            const auto key = u64_array_value(index ? index : lane,index ? 2u*i : i);
            if (i && key <= previous) throw std::runtime_error("RFCX keys must be unique and sorted or indexed");
            if (index) {
                const auto target = u64_array_value(index,2u*i+1u);
                if (target >= count || u64_array_value(lane,static_cast<std::size_t>(target)) != key)
                    throw std::runtime_error("RFCX key index target mismatch");
            }
            previous=key;
        }
    }

    void validate_grade_contents(const GradeView& g) const {
        const auto& d=g.desc;
        validate_key_index(d.grade==0 ? d.identities : d.relation_keys,
                           d.grade==0 ? d.identity_index : d.relation_key_index,d.cell_count);
        for (std::size_t i=0; i<d.cell_count; ++i) {
            const auto scale=g.cell_scale(i);
            if (scale <= 0 || (d.grade == 0 && scale != 1)) throw std::runtime_error("RFCX invalid cell scale");
            const auto type=g.relation_type(i);
            if (d.grade == 0 ? type != 0 : type >= signature_.type_count())
                throw std::runtime_error("RFCX invalid relation type target");
        }
        if (d.grade==0) return;
        validate_key_index(d.occurrence_keys,d.occurrence_key_index,d.incidence_count);
        std::pair<std::uint32_t,std::uint32_t> previous_type{};
        for (std::size_t i=0; i<d.cell_count; ++i) {
            const std::pair value{u32_array_value(d.relation_type_index,2u*i),u32_array_value(d.relation_type_index,2u*i+1u)};
            if (value.second >= d.cell_count || g.relation_type(value.second) != value.first ||
                (i && value <= previous_type)) throw std::runtime_error("RFCX relation type index mismatch");
            previous_type=value;
        }
        if (u64_array_value(d.offsets,0)!=0 || u64_array_value(d.offsets,d.cell_count)!=d.incidence_count)
            throw std::runtime_error("RFCX incidence offsets endpoints");
        const auto lower_count=grades_.at(static_cast<std::size_t>(d.grade)-1u).desc.cell_count;
        const auto exists=signature_.existence_aspect(), orientation=signature_.orientation_aspect();
        for (std::size_t i=0; i<d.cell_count; ++i) {
            const auto [begin,end]=g.incidence_range(i);
            if (begin > end || end > d.incidence_count) throw std::runtime_error("RFCX nonmonotone incidence offsets");
            const auto& type=signature_.type(g.relation_type(i));
            if (end-begin < type.min_arity || (type.max_arity && end-begin > type.max_arity))
                throw std::runtime_error("RFCX relation arity");
            for (std::size_t p=begin; p<end; ++p) {
                if (g.upper_index(p)!=i || g.lower_index(p)>=lower_count) throw std::runtime_error("RFCX incidence target mismatch");
                const auto raw=static_cast<std::int64_t>(u64_array_value(d.lattice,p));
                // Only overflow entries need arbitrary precision in this validation path.
                const int sign = raw==rfcx_detail::overflow_sentinel ? mpz_sgn(g.lattice_coefficient(p).get_mpz_t()) : (raw>0)-(raw<0);
                if ((!g.aspect(exists,p) && sign!=0) || (sign!=0 && (sign<0)!=g.aspect(orientation,p)))
                    throw std::runtime_error("RFCX lattice/aspect disagreement");
            }
        }
        if (u64_array_value(d.lower_offsets,0)!=0 || u64_array_value(d.lower_offsets,lower_count)!=d.incidence_count)
            throw std::runtime_error("RFCX reverse offsets endpoints");
        std::vector<bool> seen(static_cast<std::size_t>(d.incidence_count),false);
        for (std::size_t i=0; i<lower_count; ++i) {
            const auto [begin,end]=g.coface_incidence_range(i);
            if (begin>end || end>d.incidence_count) throw std::runtime_error("RFCX nonmonotone reverse offsets");
            for (std::size_t p=begin; p<end; ++p) {
                const auto slot=g.incidence_by_lower(p);
                if (slot>=d.incidence_count || seen[slot] || g.lower_index(slot)!=i)
                    throw std::runtime_error("RFCX invalid reverse incidence permutation");
                seen[slot]=true;
            }
        }
        if (d.incidence_count%64u) {
            const auto mask=(std::uint64_t{1}<<(d.incidence_count%64u))-1u;
            for (std::size_t a=0; a<d.aspect_count; ++a)
                if (g.aspect_word(static_cast<composite::AspectId>(a),static_cast<std::size_t>(d.aspect_words)-1u)&~mask)
                    throw std::runtime_error("RFCX nonzero aspect padding");
        }
    }

    void require_range(std::uint64_t offset, std::size_t bytes) const {
        if (offset == 0 && bytes != 0) throw std::runtime_error("RFCX missing section");
        if (offset > file_.size() || bytes > file_.size() - static_cast<std::size_t>(offset)) throw std::runtime_error("RFCX section range");
    }

    [[nodiscard]] std::size_t array_offset(std::uint64_t offset, std::size_t index, std::size_t width) const {
        require_range(offset, width);
        if (index > (file_.size() - static_cast<std::size_t>(offset) - width) / width)
            throw std::runtime_error("RFCX indexed section overflow/range");
        return static_cast<std::size_t>(offset) + index * width;
    }

    [[nodiscard]] std::uint32_t u32_at(std::uint64_t offset) const {
        require_range(offset, 4);
        return rfcx_detail::load_u32_at(file_.bytes(), static_cast<std::size_t>(offset));
    }

    [[nodiscard]] std::uint64_t u64_array_value(std::uint64_t offset, std::size_t index) const {
        const auto position = array_offset(offset, index, 8u);
        return rfcx_detail::load_u64_at(file_.bytes(), position);
    }
    [[nodiscard]] std::uint32_t u32_array_value(std::uint64_t offset, std::size_t index) const {
        const auto position = array_offset(offset, index, 4u);
        return rfcx_detail::load_u32_at(file_.bytes(), position);
    }

    [[nodiscard]] numeric::Rational lane_rational(std::uint64_t lane_offset,
                                                       std::uint64_t overflow_offset,
                                                       std::size_t index) const {
        const auto raw = static_cast<std::int64_t>(u64_array_value(lane_offset,index));
        if (raw != rfcx_detail::overflow_sentinel) {
            if (raw >= static_cast<std::int64_t>(LONG_MIN) && raw <= static_cast<std::int64_t>(LONG_MAX)) {
                return numeric::Rational(static_cast<long>(raw));
            }
        }
        return numeric::Rational(integer_lane_value(lane_offset,overflow_offset,index));
    }

    [[nodiscard]] numeric::Integer integer_lane_value(std::uint64_t lane_offset,
                                                       std::uint64_t overflow_offset,
                                                       std::size_t index) const {
        const auto raw = static_cast<std::int64_t>(u64_array_value(lane_offset,index));
        if (raw != rfcx_detail::overflow_sentinel) return numeric::Integer(std::to_string(raw));
        if (overflow_offset == 0) throw std::runtime_error("RFCX integer overflow marker without table");
        const auto count = u64_array_value(overflow_offset,0);
        std::size_t lo=0, hi=static_cast<std::size_t>(count);
        const auto dir = overflow_offset + 8u;
        while (lo<hi) {
            const auto mid=(lo+hi)/2u;
            const auto rec=dir+static_cast<std::uint64_t>(mid)*32u;
            const auto key=rfcx_detail::load_u64_at(file_.bytes(),static_cast<std::size_t>(rec));
            if (key<index) lo=mid+1u; else hi=mid;
        }
        if (lo>=count) throw std::runtime_error("RFCX integer overflow entry missing");
        const auto rec=dir+static_cast<std::uint64_t>(lo)*32u;
        if (rfcx_detail::load_u64_at(file_.bytes(),static_cast<std::size_t>(rec)) != index) throw std::runtime_error("RFCX integer overflow entry mismatch");
        const auto blob=rfcx_detail::load_u64_at(file_.bytes(),static_cast<std::size_t>(rec+8u));
        const auto size=rfcx_detail::load_u32_at(file_.bytes(),static_cast<std::size_t>(rec+16u));
        const auto flags=rfcx_detail::load_u32_at(file_.bytes(),static_cast<std::size_t>(rec+20u));
        require_range(blob,size);
        numeric::Integer value=0;
        if (size!=0) mpz_import(value.get_mpz_t(),size,1,1,1,0,file_.data()+blob);
        if ((flags&1u)!=0u) value=-value;
        return value;
    }

    [[nodiscard]] std::optional<std::uint32_t> binary_search_u64_source(std::uint64_t offset,
                                                                         std::uint64_t count,
                                                                         std::uint64_t key) const {
        if (offset == 0) return std::nullopt;
        std::size_t lo=0, hi=static_cast<std::size_t>(count);
        while (lo<hi) {
            const auto mid=(lo+hi)/2u;
            const auto found=u64_array_value(offset,mid);
            if (found<key) lo=mid+1u; else hi=mid;
        }
        if (lo>=count || u64_array_value(offset,lo)!=key) return std::nullopt;
        if (lo>std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("RFCX source index overflow");
        return static_cast<std::uint32_t>(lo);
    }

    [[nodiscard]] std::optional<std::uint32_t> binary_search_index(std::uint64_t offset,
                                                                    std::uint64_t count,
                                                                    std::uint64_t key) const {
        if (offset == 0) return std::nullopt;
        std::size_t lo=0,hi=static_cast<std::size_t>(count);
        while (lo<hi) {
            const auto mid=(lo+hi)/2u;
            const auto rec=offset+static_cast<std::uint64_t>(mid)*16u;
            const auto found=rfcx_detail::load_u64_at(file_.bytes(),static_cast<std::size_t>(rec));
            if (found<key) lo=mid+1u; else hi=mid;
        }
        if (lo>=count) return std::nullopt;
        const auto rec=offset+static_cast<std::uint64_t>(lo)*16u;
        if (rfcx_detail::load_u64_at(file_.bytes(),static_cast<std::size_t>(rec))!=key) return std::nullopt;
        const auto idx=rfcx_detail::load_u64_at(file_.bytes(),static_cast<std::size_t>(rec+8u));
        if (idx>std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("RFCX index overflow");
        return static_cast<std::uint32_t>(idx);
    }

    ReadOnlyMappedFile file_;
    core::SemanticScale semantic_scale_{};
    core::RelationSignature signature_;
    std::vector<GradeView> grades_;
    std::uint64_t payload_hash_{};
    std::shared_ptr<ViewLifetime> lifetime_{std::make_shared<ViewLifetime>(ViewLifetime{this})};
};

} // namespace rfc::storage
