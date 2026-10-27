#pragma once

#include "rfc/composite/layout.hpp"
#include "rfc/machine/bit_cursor.hpp"
#include <array>
#include <span>

namespace rfc::storage {

namespace declaration_codec_detail {
using composite::Declaration;
using composite::DeclarationId;
using composite::DeclarationKind;
using composite::DeclarationLimits;
using composite::DeclaredValue;
using composite::CompiledLayout;
using composite::declaration_detail::require;
using composite::declaration_detail::budget;

template<class Output> inline void put_var(Output& out, std::uint64_t value) {
    do {
        const auto digit = value & 127u; value >>= 7u;
        out.put(digit | (value ? 128u : 0u), 8);
    } while (value);
}
inline std::uint64_t get_var(machine::BitReader& in) {
    std::uint64_t value = 0;
    for (unsigned group = 0; group < 10; ++group) {
        const auto byte = in.get(8); const auto digit = byte & 127u;
        require(group != 9 || digit <= 1, "varuint overflow");
        value |= digit << (7u * group);
        if ((byte & 128u) == 0) {
            require(group == 0 || digit != 0, "nonminimal varuint"); return value;
        }
    }
    throw std::invalid_argument("unterminated varuint");
}
inline std::size_t get_size(machine::BitReader& in) { return machine::checked_size(get_var(in)); }

struct DecodeBudget {
    const DeclarationLimits& limits;
    std::size_t nodes{}, allocated{};
    void work(std::size_t n) {
        nodes = machine::checked_size_add(nodes, n); budget(nodes, limits.max_value_nodes, "decode work budget");
    }
    void room(std::size_t n) const { budget(n, limits.max_value_nodes - nodes, "decode work budget"); }
    template<class T> void allocate(std::size_t n) {
        allocated = machine::checked_size_add(allocated, machine::checked_size_mul(n, sizeof(T)));
        budget(allocated, limits.max_materialized_bytes, "decode materialization budget");
    }
};

template<class Output> inline void put_magnitude(Output& out, const numeric::Integer& x) {
    const auto n = machine::ceil_div(composite::declaration_detail::integer_bits(x), 8);
    put_var(out, n);
    if (!n) return;
    out.put_generated_octets(n, [&](std::span<std::byte> bytes) {
        std::size_t written = 0;
        mpz_export(bytes.data(), &written, -1, 1, 0, 0, x.get_mpz_t());
        if (written != n) throw std::logic_error("integer layout/export disagreement");
    });
}
inline numeric::Integer get_magnitude(machine::BitReader& in, DecodeBudget& use) {
    const auto n = get_size(in);
    budget(n, machine::ceil_div(use.limits.max_integer_bits, 8), "integer magnitude byte budget");
    require(n <= in.remaining() / 8, "truncated integer magnitude");
    use.allocate<unsigned char>(n);
    const auto bytes = in.get_octets(n);
    require(n == 0 || bytes.back() != std::byte{0}, "nonminimal integer magnitude");
    numeric::Integer value = 0;
    if (n) mpz_import(value.get_mpz_t(), n, -1, 1, 0, 0, bytes.data());
    budget(composite::declaration_detail::integer_bits(value), use.limits.max_integer_bits, "integer bit budget");
    return value;
}
template<class Output> inline void put_integer(Output& out, const numeric::Integer& x) {
    out.put(x < 0 ? 1 : 0, 1); put_magnitude(out, x);
}
inline numeric::Integer get_integer(machine::BitReader& in, DecodeBudget& use) {
    const bool negative = in.get(1) != 0;
    numeric::Integer x = get_magnitude(in, use);
    require(!negative || x != 0, "negative zero integer");
    if (negative) x = -x;
    return x;
}
template<class Output> inline void put_rational(Output& out, const numeric::Rational& x) {
    put_integer(out, x.get_num()); put_magnitude(out, x.get_den());
}
inline numeric::Rational get_rational(machine::BitReader& in, DecodeBudget& use) {
    // Explicit sequencing: argument evaluation order must not reorder wire reads.
    numeric::Integer n = get_integer(in, use);
    numeric::Integer d = get_magnitude(in, use);
    require(d > 0, "zero rational denominator");
    numeric::Integer gcd; mpz_gcd(gcd.get_mpz_t(), n.get_mpz_t(), d.get_mpz_t());
    require(gcd == 1, "nonreduced rational encoding");
    numeric::Rational q(n, d); return q;
}

template<class Output> inline void put_value(Output& out, const CompiledLayout& layout,
                      DeclarationId id, const DeclaredValue& value) {
    const auto& d = layout.declarations().at(id);
    switch (d.kind) {
        case DeclarationKind::Boolean: out.put(std::get<bool>(value.data), 1); break;
        case DeclarationKind::Finite: {
            const auto key = std::get<DeclaredValue::Symbol>(value.data).key;
            const auto position = layout.declarations().symbol_ordinal(d.id, key);
            out.put(static_cast<std::uint64_t>(position), static_cast<unsigned>(composite::declaration_detail::tag_bits(d.symbols.size()))); break;
        }
        case DeclarationKind::Integer: put_integer(out, std::get<numeric::Integer>(value.data)); break;
        case DeclarationKind::Rational: put_rational(out, std::get<numeric::Rational>(value.data)); break;
        case DeclarationKind::Product: {
            const auto& vs = std::get<DeclaredValue::Product>(value.data).elements;
            for (std::size_t i = 0; i < vs.size(); ++i) put_value(out, layout, d.factors[i].type, vs[i]);
            break;
        }
        case DeclarationKind::Alternative: {
            const auto& v = std::get<DeclaredValue::Alternative>(value.data);
            const auto index = layout.declarations().alternative_ordinal(d.id, v.key);
            out.put(index, static_cast<unsigned>(composite::declaration_detail::tag_bits(d.factors.size())));
            put_value(out, layout, d.factors[index].type, v.element.front()); break;
        }
        case DeclarationKind::Optional: {
            const auto& v = std::get<DeclaredValue::Optional>(value.data).element;
            out.put(!v.empty(), 1);
            if (!v.empty()) put_value(out, layout, d.element, v.front());
            break;
        }
        case DeclarationKind::Repeated: {
            const auto& v = std::get<DeclaredValue::Repeated>(value.data).elements;
            put_var(out, v.size());
            for (const auto& x : v) put_value(out, layout, d.element, x);
            break;
        }
        case DeclarationKind::RationalTensor: {
            const auto& v = std::get<numeric::SparseRationalTensor>(value.data);
            const auto count = v.support().count();
            const bool compact = layout.profile() == composite::BooleanLayoutProfile::Lsb0OctetsV2;
            const bool full = compact && count == v.domain().logical_size();
            if (compact) out.put(full ? 1 : 0, 8);
            if (!full) {
                put_var(out, count);
                v.support().for_each_active([&](auto i){ put_var(out, i); });
            }
            put_var(out, v.stored_nonzeros());
            for (const auto& e : v.values().entries()) { put_var(out, e.index); put_rational(out, e.value); }
            break;
        }
        default: throw std::invalid_argument("unknown encoded value constructor");
    }
}

inline void put_var(machine::BitWriter& out, std::uint64_t value) { put_var<machine::BitWriter>(out,value); }
inline void put_magnitude(machine::BitWriter& out, const numeric::Integer& value) { put_magnitude<machine::BitWriter>(out,value); }
inline void put_integer(machine::BitWriter& out, const numeric::Integer& value) { put_integer<machine::BitWriter>(out,value); }
inline void put_rational(machine::BitWriter& out, const numeric::Rational& value) { put_rational<machine::BitWriter>(out,value); }
inline void put_value(machine::BitWriter& out, const CompiledLayout& layout,
                      DeclarationId id, const DeclaredValue& value) {
    put_value<machine::BitWriter>(out,layout,id,value);
}

inline DeclaredValue get_value(machine::BitReader& in, const CompiledLayout& layout,
                               DeclarationId id, DecodeBudget& use, std::size_t depth = 1) {
    use.work(1); budget(depth, use.limits.max_depth, "decode nesting budget");
    const auto& d = layout.declarations().at(id);
    auto child = [&](DeclarationId type) { return get_value(in, layout, type, use, depth + 1); };
    auto sequence = [&](std::size_t count, auto&& type_at) {
        use.room(count); use.allocate<DeclaredValue>(count);
        std::vector<DeclaredValue> values; values.reserve(count);
        for (std::size_t i = 0; i < count; ++i) values.push_back(child(type_at(i)));
        return values;
    };
    switch (d.kind) {
        case DeclarationKind::Boolean: return DeclaredValue::boolean(in.get(1) != 0);
        case DeclarationKind::Finite: {
            const auto i = in.get(static_cast<unsigned>(composite::declaration_detail::tag_bits(d.symbols.size())));
            require(i < d.symbols.size(), "reserved finite code pattern");
            return DeclaredValue::symbol(d.symbols[static_cast<std::size_t>(i)]);
        }
        case DeclarationKind::Integer: return DeclaredValue::integer(get_integer(in, use));
        case DeclarationKind::Rational: return DeclaredValue::rational(get_rational(in, use));
        case DeclarationKind::Product:
            return DeclaredValue::product(sequence(d.factors.size(), [&](auto i){return d.factors[i].type;}));
        case DeclarationKind::Alternative: {
            const auto i = in.get(static_cast<unsigned>(composite::declaration_detail::tag_bits(d.factors.size())));
            require(i < d.factors.size(), "reserved alternative pattern");
            const auto& f = d.factors[static_cast<std::size_t>(i)];
            use.allocate<DeclaredValue>(1); return DeclaredValue::alternative(f.key, child(f.type));
        }
        case DeclarationKind::Optional:
            if (in.get(1) == 0) return DeclaredValue::absent();
            use.allocate<DeclaredValue>(1); return DeclaredValue::present(child(d.element));
        case DeclarationKind::Repeated: {
            const auto count = get_size(in);
            require(count >= d.min_count && count <= d.max_count, "encoded repetition bounds");
            return DeclaredValue::repeated(sequence(count, [&](auto){return d.element;}));
        }
        case DeclarationKind::RationalTensor: {
            const auto domain = composite::DeclarationRegistry::tensor_domain(d);
            const bool compact = layout.profile() == composite::BooleanLayoutProfile::Lsb0OctetsV2;
            const auto tag = compact ? in.get(8) : 0;
            require(tag <= 1, "reserved tensor support tag");
            const bool full = tag == 1;
            const auto count = full ? domain.logical_size() : get_size(in);
            std::vector<std::size_t> indices;
            if (!full) {
                require(count <= domain.logical_size() && count <= in.remaining()/8, "encoded tensor support count");
                require(!compact || count < domain.logical_size(), "noncanonical full tensor support");
                use.work(count); use.allocate<std::size_t>(count); indices.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    const auto index = get_size(in);
                    require(index < domain.logical_size() && (indices.empty() || indices.back() < index), "unordered/invalid tensor support");
                    indices.push_back(index);
                }
            }
            const auto nz = get_size(in);
            require(nz <= count && nz <= in.remaining()/33, "encoded tensor nonzero count");
            use.work(nz); use.allocate<numeric::SparseRationalEntry>(nz);
            std::vector<numeric::SparseRationalEntry> entries; entries.reserve(nz);
            for (std::size_t i = 0; i < nz; ++i) {
                const auto index = get_size(in);
                require(index < domain.logical_size() && (full || std::binary_search(indices.begin(), indices.end(), index)) &&
                        (entries.empty() || entries.back().index < index), "invalid tensor nonzero support/order");
                auto q = get_rational(in, use); require(q != 0, "explicit tensor numerical zero");
                entries.push_back({index, std::move(q)});
            }
            auto support = full ? numeric::StructuralSupport::full(domain.logical_size())
                                : numeric::StructuralSupport::sparse(domain.logical_size(), std::move(indices));
            auto values = numeric::SparseRationalVector(domain.logical_size(), std::move(entries));
            return DeclaredValue::tensor(numeric::SparseRationalTensor(domain, std::move(support), std::move(values)));
        }
        default: throw std::invalid_argument("unknown decoded value constructor");
    }
}
} // namespace declaration_codec_detail

// Raw payloads deliberately require their supplied declaration. They are not
// self-describing; use the archive below to persist the declaration binding.
inline machine::EncodedBits encode_declared_bits(const composite::CompiledLayout& layout,
                                                  const composite::DeclaredValue& value) {
    const auto allocation = layout.allocation(value); // validate/preflight before allocating output
    machine::BitWriter out(allocation.bits, layout.limits().max_encoded_bytes);
    declaration_codec_detail::put_value(out, layout, layout.root(), value);
    return std::move(out).finish();
}
// Preflight before requesting an exclusive output region from the caller.
// The destination remains stable for this complete encoding call.
template<class Destination> inline void encode_declared_bits_to(
    const composite::CompiledLayout& layout, const composite::DeclaredValue& value,
    Destination&& destination) {
    const auto allocation = layout.allocation(value);
    const auto bytes = machine::ceil_div(allocation.bits, 8);
    if (bytes > layout.limits().max_encoded_bytes)
        throw std::length_error("bit writer allocation budget");
    auto target = std::forward<Destination>(destination)(allocation.bits,bytes);
    machine::BitSpanWriter out(target,allocation.bits,layout.limits().max_encoded_bytes);
    declaration_codec_detail::put_value(out,layout,layout.root(),value);
    out.finish();
}
// Reuse an owning layout while tightening only this payload's remaining
// value-node/materialization allowances. Schema, profile and scalar limits stay
// bound to the same immutable compilation; no subsequent allocation is unguarded.
inline composite::DeclaredValue decode_declared_bits(const composite::CompiledLayout& layout,
    std::span<const std::byte> bytes,std::size_t bits,std::size_t value_nodes,
    std::size_t materialized_bytes) {
    auto policy=layout.limits();
    policy.max_value_nodes=std::min(policy.max_value_nodes,value_nodes);
    policy.max_materialized_bytes=std::min(policy.max_materialized_bytes,materialized_bytes);
    machine::BitReader in(bytes,bits,policy.max_encoded_bytes);
    declaration_codec_detail::DecodeBudget use{policy};
    auto value=declaration_codec_detail::get_value(in,layout,layout.root(),use);
    in.finish();layout.validate(value);return value;
}
inline composite::DeclaredValue decode_declared_bits(const composite::CompiledLayout& layout,
                                                      std::span<const std::byte> bytes, std::size_t bits) {
    return decode_declared_bits(layout,bytes,bits,layout.limits().max_value_nodes,
        layout.limits().max_materialized_bytes);
}
inline composite::DeclaredValue decode_declared_bits(const composite::CompiledLayout& layout,
                                                      const machine::EncodedBits& bits) {
    return decode_declared_bits(layout, bits.bytes, bits.bit_count);
}

namespace declaration_codec_detail {
class OctetWriter {
public:
    explicit OctetWriter(std::size_t limit) : limit_(limit) {}
    void bytes(std::span<const std::byte> data) {
        budget(machine::checked_size_add(data_.size(), data.size()), limit_, "declaration archive byte budget");
        data_.insert(data_.end(), data.begin(), data.end());
    }
    void fixed(std::uint64_t x, unsigned n) {
        require(n <= 8 && (n == 8 || (x >> (n*8u)) == 0), "fixed metadata width");
        for (unsigned i = 0; i < n; ++i) { const auto b = static_cast<std::byte>(x & 255u); bytes({&b, 1}); x >>= 8u; }
    }
    void var(std::uint64_t x) {
        do { const auto digit = x & 127u; x >>= 7u; fixed(digit | (x ? 128u : 0u), 1); } while (x);
    }
    void id(DeclarationId id) { fixed(id.name_space, 8); fixed(id.key, 8); fixed(id.version, 8); }
    std::vector<std::byte> finish() && { return std::move(data_); }
private:
    std::size_t limit_;
    std::vector<std::byte> data_;
};
class OctetReader {
public:
    OctetReader(std::span<const std::byte> data, std::size_t limit) : data_(data) {
        budget(data.size(), limit, "archive input budget");
    }
    std::span<const std::byte> bytes(std::size_t n) {
        require(n <= remaining(), "truncated declaration archive");
        const auto out = data_.subspan(position_, n); position_ += n; return out;
    }
    std::uint64_t fixed(unsigned n) {
        require(n <= 8, "fixed metadata width"); const auto b = bytes(n);
        std::uint64_t value = 0;
        for (unsigned i = 0; i < n; ++i) value |= static_cast<std::uint64_t>(std::to_integer<unsigned>(b[i])) << (i*8u);
        return value;
    }
    std::uint64_t var() {
        std::uint64_t value = 0;
        for (unsigned group = 0; group < 10; ++group) {
            const auto b = fixed(1), digit = b & 127u;
            require(group != 9 || digit <= 1, "archive varuint overflow"); value |= digit << (group*7u);
            if ((b & 128u) == 0) { require(!group || digit, "nonminimal archive varuint"); return value; }
        }
        throw std::invalid_argument("unterminated archive varuint");
    }
    std::size_t size() { return machine::checked_size(var()); }
    DeclarationId id() {
        const auto ns = fixed(8); const auto key = fixed(8); const auto version = fixed(8);
        return {ns, key, version};
    }
    std::size_t remaining() const noexcept { return data_.size() - position_; }
    void finish() const { require(remaining() == 0, "trailing archive bytes"); }
private:
    std::span<const std::byte> data_;
    std::size_t position_{};
};
inline constexpr std::array<std::byte, 8> declaration_magic = {
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'D'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> value_magic = {
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'V'},std::byte{1},std::byte{0},std::byte{0}};
inline void require_magic(OctetReader& in, const std::array<std::byte, 8>& expected) {
    const auto actual = in.bytes(8); require(std::equal(actual.begin(), actual.end(), expected.begin()), "unsupported declaration/value archive");
}
} // namespace declaration_codec_detail

inline std::vector<std::byte> encode_declarations(const composite::CompiledLayout& layout) {
    using namespace declaration_codec_detail;
    OctetWriter out(layout.limits().max_encoded_bytes); out.bytes(declaration_magic);
    out.fixed(static_cast<std::uint8_t>(layout.profile()), 1); out.id(layout.root());
    out.var(layout.declarations().definitions().size());
    for (const auto& d : layout.declarations().definitions()) {
        out.id(d.id); out.fixed(static_cast<std::uint8_t>(d.kind), 1);
        switch (d.kind) {
            case DeclarationKind::Boolean: case DeclarationKind::Integer: case DeclarationKind::Rational: break;
            case DeclarationKind::Finite:
                out.var(d.symbols.size()); for (const auto key : d.symbols) out.fixed(key, 8); break;
            case DeclarationKind::Product: case DeclarationKind::Alternative:
                out.var(d.factors.size());
                for (const auto& f : d.factors) {
                    out.fixed(f.key, 8); out.id(f.type);
                    if (d.kind == DeclarationKind::Product) out.fixed(f.grade, 2);
                } break;
            case DeclarationKind::Optional: out.id(d.element); break;
            case DeclarationKind::Repeated: out.id(d.element); out.var(d.min_count); out.var(d.max_count); break;
            case DeclarationKind::RationalTensor:
                out.var(d.axes.size());
                for (const auto& a : d.axes) {
                    out.fixed(a.key, 8); out.var(a.extent); out.var(a.coordinates.size());
                    for (const auto key : a.coordinates) out.fixed(key, 8);
                } break;
            default: throw std::invalid_argument("unknown declaration constructor");
        }
    }
    return std::move(out).finish();
}

inline composite::CompiledLayout decode_declarations(std::span<const std::byte> bytes,
                                                       composite::DeclarationLimits limits = {}) {
    using namespace declaration_codec_detail;
    limits.validate(); OctetReader in(bytes, limits.max_encoded_bytes); require_magic(in, declaration_magic);
    const auto profile = static_cast<composite::BooleanLayoutProfile>(in.fixed(1));
    composite::validate_layout_profile(profile);
    const auto root = in.id(); const auto count = in.size();
    budget(count, limits.max_definitions, "declaration count budget");
    require(count <= in.remaining()/25, "truncated declaration definitions");
    std::vector<Declaration> defs; defs.reserve(count); std::size_t components = 0;
    auto component_count = [&](std::size_t minimum_bytes) {
        const auto n = in.size(); components = machine::checked_size_add(components, n);
        budget(components, limits.max_components, "declaration components budget");
        require(n <= in.remaining()/minimum_bytes, "truncated declaration components"); return n;
    };
    for (std::size_t i = 0; i < count; ++i) {
        Declaration d; d.id = in.id(); d.kind = static_cast<DeclarationKind>(in.fixed(1));
        require(defs.empty() || defs.back().id < d.id, "noncanonical declaration identity order");
        switch (d.kind) {
            case DeclarationKind::Boolean: case DeclarationKind::Integer: case DeclarationKind::Rational: break;
            case DeclarationKind::Finite: {
                const auto n = component_count(8); d.symbols.reserve(n);
                for (std::size_t j = 0; j < n; ++j) d.symbols.push_back(in.fixed(8));
                break;
            }
            case DeclarationKind::Product: case DeclarationKind::Alternative: {
                const auto n = component_count(d.kind == DeclarationKind::Product ? 34 : 32); d.factors.reserve(n);
                for (std::size_t j = 0; j < n; ++j) {
                    const auto key = in.fixed(8); const auto type = in.id();
                    const auto grade = d.kind == DeclarationKind::Product ? in.fixed(2) : 0;
                    d.factors.push_back({key, type, static_cast<std::uint16_t>(grade)});
                } break;
            }
            case DeclarationKind::Optional: d.element = in.id(); break;
            case DeclarationKind::Repeated: d.element = in.id(); d.min_count = in.size(); d.max_count = in.size(); break;
            case DeclarationKind::RationalTensor: {
                const auto n = component_count(10); budget(n, limits.max_tensor_rank, "tensor rank budget"); d.axes.reserve(n);
                for (std::size_t j = 0; j < n; ++j) {
                    composite::DeclarationAxis a; a.key = in.fixed(8); a.extent = in.size();
                    const auto nc = component_count(8); a.coordinates.reserve(nc);
                    for (std::size_t k = 0; k < nc; ++k) a.coordinates.push_back(in.fixed(8));
                    d.axes.push_back(std::move(a));
                } break;
            }
            default: throw std::invalid_argument("unsupported declaration constructor");
        }
        defs.push_back(std::move(d));
    }
    in.finish();
    return composite::CompiledLayout(composite::DeclarationRegistry(std::move(defs), limits), root, profile, limits);
}

struct DeclaredArchive {
    composite::CompiledLayout layout;
    std::vector<composite::DeclaredValue> values;
};
// Full definitions are stored once per archive, never replaced by a digest.
// This is a standalone F1 format, not a new RFCXE/RFCXT channel or migration.
inline std::vector<std::byte> encode_declared_archive(const composite::CompiledLayout& layout,
                                                     std::span<const composite::DeclaredValue> values) {
    using namespace declaration_codec_detail;
    std::size_t total_nodes = 0;
    std::size_t materialized = machine::checked_size_mul(values.size(), sizeof(composite::DeclaredValue));
    budget(materialized, layout.limits().max_materialized_bytes, "archive materialization budget");
    budget(values.size(), layout.limits().max_value_nodes, "archive value count budget");
    for (const auto& value : values) {
        const auto allocation = layout.allocation(value);
        total_nodes = machine::checked_size_add(total_nodes, allocation.value_nodes);
        materialized = machine::checked_size_add(materialized, allocation.materialized_bytes);
        budget(materialized, layout.limits().max_materialized_bytes, "archive materialization budget");
        budget(total_nodes, layout.limits().max_value_nodes, "archive value work budget");
    }
    const auto defs = encode_declarations(layout);
    OctetWriter out(layout.limits().max_encoded_bytes); out.bytes(value_magic); out.var(defs.size()); out.bytes(defs);
    out.var(values.size());
    for (const auto& v : values) {
        const auto bits = encode_declared_bits(layout, v); out.var(bits.bit_count); out.bytes(bits.bytes);
    }
    return std::move(out).finish();
}
inline DeclaredArchive decode_declared_archive(std::span<const std::byte> bytes,
                                               composite::DeclarationLimits limits = {}) {
    using namespace declaration_codec_detail;
    limits.validate(); OctetReader in(bytes, limits.max_encoded_bytes); require_magic(in, value_magic);
    const auto nd = in.size();
    auto layout = decode_declarations(in.bytes(nd), limits);
    const auto count = in.size(); require(count <= in.remaining(), "truncated archive value lengths");
    DecodeBudget use{limits}; use.room(count); use.allocate<DeclaredValue>(count);
    std::vector<DeclaredValue> values; values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto bits = in.size(); const auto payload = in.bytes(machine::ceil_div(bits, 8));
        machine::BitReader reader(payload, bits, limits.max_encoded_bytes);
        auto value = get_value(reader, layout, layout.root(), use); reader.finish(); layout.validate(value);
        values.push_back(std::move(value));
    }
    in.finish(); return {std::move(layout), std::move(values)};
}
} // namespace rfc::storage
