#pragma once

#include "rfc/storage/temporal_endpoint.hpp"
#include <bit>

namespace rfc::storage {

struct TemporalReadLimits {
    std::size_t max_file_bytes{256u * 1024u * 1024u};
    std::size_t max_records{1000000u};
    std::size_t max_dimension{1000000u};
    std::size_t max_rank{64u};
    std::size_t max_integer_bytes{1024u * 1024u};
    // Strict by default. False is an explicit legacy-ingestion escape hatch.
    bool require_canonical{true};
    composite::DeclarationLimits declarations{};
    SourceDeclarationLimits source_declarations{};
    // Keep checked seed coefficient lanes in one immutable channel buffer.
    // Contiguous numerical consumers may materialize their requested fields.
    bool retain_encoded_values{false};
    // Zero retains the maximum dimension as the logical tensor size bound.
    std::size_t max_logical_size{};
};

namespace temporal_codec_detail {
using Writer = rfcx_detail::Writer;
using temporal_source_detail::require;
using temporal_endpoint_detail::index;

// Bounds are checked before allocations. The record budget is cumulative over
// nested collections; full support stays implicit instead of being enumerated.
class Reader {
public:
    Reader(std::span<const std::byte> bytes, const TemporalReadLimits& limits,
           bool seed_values = false)
        : owner_(freeze(bytes, limits, seed_values)), bytes_(owner_ ? std::span<const std::byte>(*owner_) : bytes),
          r_(bytes_), size_(bytes.size()), limits_(limits), budget_(limits.max_records) {
        require(size_ <= limits.max_file_bytes, "temporal image exceeds byte limit");
    }
    std::uint8_t u8() { return r_.u8(); }
    std::uint16_t u16() { return r_.u16(); }
    std::uint32_t u32() { return r_.u32(); }
    std::uint64_t u64() { return r_.u64(); }
    bool flag() { const auto b = u8(); require(b <= 1, "invalid temporal boolean"); return b != 0; }
    std::size_t remaining() const { return size_ - r_.position(); }
    std::size_t position() const noexcept { return r_.position(); }
    const TemporalReadLimits& limits() const noexcept { return limits_; }
    std::shared_ptr<const std::vector<std::byte>> retained_owner() const noexcept { return owner_; }
    numeric::SparseRationalVector read_values(std::size_t size);
    std::span<const std::byte> blob() {
        const auto n = u64();
        require(n <= remaining() && n <= limits_.max_file_bytes, "truncated/oversized temporal blob");
        return r_.slice(static_cast<std::size_t>(n));
    }
    void consume_records(std::size_t n) {
        require(n <= budget_, "aggregate temporal record budget"); budget_ -= n;
    }
    std::size_t count(std::size_t minimum_bytes = 1) {
        const auto n = u64();
        require(n <= budget_ && n <= remaining() / minimum_bytes, "temporal count exceeds bounds/budget");
        budget_ -= static_cast<std::size_t>(n);
        return static_cast<std::size_t>(n);
    }
    std::size_t dimension() {
        const auto n = u64(); require(n <= limits_.max_dimension, "temporal dimension exceeds configured limit");
        return static_cast<std::size_t>(n);
    }
    std::string string() {
        const auto n = u64(); require(n <= remaining(), "truncated temporal string");
        const auto bytes = r_.slice(static_cast<std::size_t>(n));
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    struct IntegerBytes { bool negative; std::span<const std::byte> magnitude; };
    IntegerBytes integer_bytes() {
        const bool negative = flag();
        const auto n = u32(); require(n <= limits_.max_integer_bytes, "temporal integer exceeds configured limit");
        const auto bytes = r_.slice(n);
        require(bytes.empty() || bytes.front() != std::byte{0}, "noncanonical temporal integer");
        require(!bytes.empty() || !negative, "negative zero integer");
        return {negative, bytes};
    }
    numeric::Integer integer() {
        const auto [negative, bytes] = integer_bytes();
        numeric::Integer out = 0;
        if (!bytes.empty()) mpz_import(out.get_mpz_t(), bytes.size(), 1, 1, 1, 0, bytes.data());
        if (negative) out = -out;
        return out;
    }
    numeric::Rational rational() {
        const auto n = integer(), d = integer();
        require(d > 0, "temporal rational denominator must be positive");
        numeric::Rational out(n, d); out.canonicalize();
        require(out.get_num() == n && out.get_den() == d, "noncanonical temporal rational");
        return out;
    }
    numeric::IndexDomain domain() {
        const auto rank = count(8);
        require(rank <= limits_.max_rank, "temporal rank exceeds configured limit");
        std::vector<std::size_t> extents;
        for (std::size_t i = 0; i < rank; ++i) extents.push_back(dimension());
        numeric::IndexDomain out(std::move(extents));
        const auto maximum=limits_.max_logical_size?limits_.max_logical_size:limits_.max_dimension;
        require(out.logical_size() <= maximum, "temporal logical size exceeds configured limit");
        return out;
    }
private:
    static std::shared_ptr<const std::vector<std::byte>> freeze(
        std::span<const std::byte> bytes, const TemporalReadLimits& limits, bool seed_values) {
        require(bytes.size() <= limits.max_file_bytes, "temporal image exceeds byte limit");
        if (!seed_values || !limits.retain_encoded_values) return {};
        return std::make_shared<const std::vector<std::byte>>(bytes.begin(), bytes.end());
    }
    std::shared_ptr<const std::vector<std::byte>> owner_;
    std::span<const std::byte> bytes_;
    rfcx_detail::Reader r_;
    std::size_t size_;
    const TemporalReadLimits& limits_;
    std::size_t budget_;
};

// The lane is the existing count, coordinate and rational encoding. Construction
// follows complete Reader admission; all cursors borrow its immutable byte owner.
class RetainedTemporalValues final : public numeric::RetainedSparseRationalSource {
    friend class Reader;
    std::shared_ptr<const std::vector<std::byte>> owner_;
    std::span<const std::byte> bytes_;
    TemporalReadLimits limits_;
    std::size_t count_;
    RetainedTemporalValues(std::shared_ptr<const std::vector<std::byte>> owner,
                           std::span<const std::byte> bytes, TemporalReadLimits limits, std::size_t count)
        : owner_(std::move(owner)), bytes_(bytes), limits_(std::move(limits)), count_(count) {}
    class Cursor final : public numeric::SparseRationalCursor {
        std::shared_ptr<const std::vector<std::byte>> owner;
        TemporalReadLimits limits;
        Reader reader;
        std::size_t remaining;
    public:
        explicit Cursor(const RetainedTemporalValues& source)
            : owner(source.owner_), limits(source.limits_), reader(source.bytes_.subspan(8), limits),
              remaining(source.count_) {}
        bool next(numeric::SparseRationalEntry& entry) override {
            if (!remaining) return false;
            entry.index = static_cast<std::size_t>(reader.u64());
            entry.value = reader.rational();
            --remaining;
            return true;
        }
    };
public:
    std::size_t nonzeros() const noexcept override { return count_; }
    std::unique_ptr<numeric::SparseRationalCursor> open() const override { return std::make_unique<Cursor>(*this); }
    std::span<const std::byte> encoded_bytes() const noexcept { return bytes_; }
    std::shared_ptr<const std::vector<std::byte>> owner() const noexcept { return owner_; }
    void indices(const std::function<void(std::size_t)>& function) const override {
        Reader r(bytes_.subspan(8), limits_);
        for (std::size_t i = 0; i < count_; ++i) {
            function(static_cast<std::size_t>(r.u64()));
            (void)r.integer_bytes(); (void)r.integer_bytes();
        }
    }
    numeric::Rational coefficient(std::size_t index) const override {
        Reader r(bytes_.subspan(8), limits_);
        for (std::size_t i = 0; i < count_; ++i) {
            const auto coordinate = r.u64();
            if (coordinate == index) return r.rational();
            if (coordinate > index) break;
            (void)r.integer_bytes(); (void)r.integer_bytes();
        }
        return numeric::Rational{0};
    }
};

inline void string(Writer& w, const std::string& value) {
    w.u64(value.size()); w.raw({reinterpret_cast<const std::byte*>(value.data()), value.size()});
}
inline void rational(Writer& w, const numeric::Rational& value) { rfcx_detail::write_rational(w, value); }
inline void cell(Writer& w, StableCellKey value) { w.u16(value.grade.value); w.u64(value.key); }
inline StableCellKey cell(Reader& r) { const auto grade = r.u16(); const auto key = r.u64(); return {core::Grade{grade}, key}; }
inline void occurrence(Writer& w, StableOccurrenceKey value) { w.u16(value.upper_grade.value); w.u64(value.key.value); }
inline StableOccurrenceKey occurrence(Reader& r) { const auto grade = r.u16(); const auto key = r.u64(); return {core::Grade{grade}, core::OccurrenceKey{key}}; }
inline void optional_key(Writer& w, const std::optional<std::uint64_t>& value) { w.u8(value.has_value()); if (value) w.u64(*value); }
inline std::optional<std::uint64_t> optional_key(Reader& r) { if (r.flag()) return r.u64(); return std::nullopt; }
inline void keys(Writer& w, const std::vector<std::uint64_t>& values) { w.u64(values.size()); for (const auto v : values) w.u64(v); }
inline std::vector<std::uint64_t> keys(Reader& r) {
    std::vector<std::uint64_t> out; const auto n = r.count(8); out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) out.push_back(r.u64());
    return out;
}
inline void domain(Writer& w, const numeric::IndexDomain& value) {
    w.u64(value.rank()); for (const auto n : value.extents()) w.u64(n);
}
inline void values(Writer& w, const numeric::SparseRationalVector& value) {
    if (const auto source = std::dynamic_pointer_cast<const RetainedTemporalValues>(value.retained_source())) {
        w.raw(source->encoded_bytes()); return;
    }
    w.u64(value.nonzeros());
    for (const auto& v : value.entries()) { w.u64(v.index); rational(w, v.value); }
}
inline numeric::SparseRationalVector Reader::read_values(std::size_t size) {
    const auto begin = position();
    const auto n = count(18);
    std::vector<numeric::SparseRationalEntry> entries;
    if (!owner_) entries.reserve(n);
    std::optional<std::size_t> last;
    for (std::size_t i = 0; i < n; ++i) {
        const auto idx = u64(); require(idx < size && (!last || idx > *last), "unordered/out-of-domain sparse coordinate");
        auto value = rational(); require(value != 0, "explicit numerical zero in sparse lane");
        if (!owner_) entries.push_back({static_cast<std::size_t>(idx), std::move(value)});
        last = static_cast<std::size_t>(idx);
    }
    if (owner_) return numeric::SparseRationalVector::admitted(size,
        std::shared_ptr<const RetainedTemporalValues>(new RetainedTemporalValues(
            owner_, bytes_.subspan(begin, position() - begin), limits_, n)));
    return numeric::SparseRationalVector(size, std::move(entries));
}
inline numeric::SparseRationalVector values(Reader& r, std::size_t size) { return r.read_values(size); }
inline void tensor(Writer& w, const numeric::SparseRationalTensor& value) {
    domain(w, value.domain());
    const bool full = value.support().count() == value.domain().logical_size();
    w.u8(full);
    if (!full) {
        w.u64(value.support().count());
        value.support().for_each_active([&](std::size_t i) { w.u64(i); });
    }
    values(w, value.values());
}
struct TensorHeader {
    numeric::IndexDomain domain;
    numeric::StructuralSupport support;
    bool encoded_full;
};
inline TensorHeader tensor_header(Reader& r) {
    auto d = r.domain(); const auto size = d.logical_size();
    auto support = numeric::StructuralSupport::full(size);
    const auto full = r.flag();
    if (!full) {
        const auto n = r.count(8);
        std::vector<std::size_t> active; active.reserve(n);
        std::optional<std::size_t> last;
        for (std::size_t i = 0; i < n; ++i) {
            const auto idx = r.u64(); require(idx < size && (!last || idx > *last), "unordered/out-of-domain structural support");
            active.push_back(static_cast<std::size_t>(idx)); last = static_cast<std::size_t>(idx);
        }
        support = numeric::StructuralSupport::sparse(size, std::move(active));
    }
    return {std::move(d), std::move(support), full};
}
inline numeric::SparseRationalTensor tensor(Reader& r) {
    auto h = tensor_header(r);
    auto v = values(r, h.domain.logical_size());
    return numeric::SparseRationalTensor(std::move(h.domain), std::move(h.support), std::move(v));
}
inline void matrix(Writer& w, const numeric::ExactMatrix& value) {
    w.u64(value.rows()); w.u64(value.cols()); w.u64(value.nonzeros());
    for (const auto& [ij, q] : value.entries()) { w.u64(ij.first); w.u64(ij.second); rational(w, q); }
}
inline numeric::ExactMatrix matrix(Reader& r) {
    const auto rows = r.dimension(), cols = r.dimension(); numeric::ExactMatrix out(rows, cols);
    const auto n = r.count(26); std::optional<numeric::ExactMatrix::Key> last;
    for (std::size_t i = 0; i < n; ++i) {
        const auto row = r.u64(), col = r.u64();
        const numeric::ExactMatrix::Key ij{static_cast<std::size_t>(row), static_cast<std::size_t>(col)};
        require(row < rows && col < cols && (!last || ij > *last), "unordered/out-of-domain matrix entry");
        auto q = r.rational(); require(q != 0, "zero in sparse matrix lane");
        out.set(ij.first, ij.second, std::move(q)); last = ij;
    }
    return out;
}
inline void carrier(Writer& w, const TemporalCarrier& c) {
    w.u64(c.key); w.u8(static_cast<std::uint8_t>(c.kind)); w.u16(c.grade.value);
    w.u8(c.cell.has_value()); if (c.cell) cell(w, *c.cell);
    w.u64(c.coordinate_system); domain(w, c.domain);
}
inline TemporalCarrier carrier(Reader& r) {
    TemporalCarrier c; c.key = r.u64(); c.kind = static_cast<TemporalCarrierKind>(r.u8()); c.grade = core::Grade{r.u16()};
    require(c.kind <= TemporalCarrierKind::CellStalk, "unknown carrier kind");
    if (r.flag()) c.cell = cell(r);
    c.coordinate_system = r.u64(); c.domain = r.domain(); return c;
}
inline void tensor_record(Writer& w, const TemporalTensor& t) {
    w.u64(t.key); w.u8(static_cast<std::uint8_t>(t.role)); w.u8(static_cast<std::uint8_t>(t.owner.kind));
    w.u64(t.owner.block); cell(w, t.owner.cell); occurrence(w, t.owner.occurrence);
    optional_key(w, t.input); optional_key(w, t.output); string(w, t.interpretation); tensor(w, t.tensor);
}
inline TemporalTensor tensor_record_header(Reader& r) {
    TemporalTensor t; t.key = r.u64(); t.role = static_cast<TemporalTensorRole>(r.u8());
    t.owner.kind = static_cast<TemporalOwnerKind>(r.u8());
    require(t.role <= TemporalTensorRole::OpaqueExact && t.owner.kind <= TemporalOwnerKind::Occurrence, "unsupported tensor tag");
    t.owner.block = r.u64(); t.owner.cell = cell(r); t.owner.occurrence = occurrence(r);
    t.input = optional_key(r); t.output = optional_key(r); t.interpretation = r.string(); return t;
}
inline TemporalTensor tensor_record(Reader& r) {
    auto t = tensor_record_header(r); t.tensor = tensor(r); return t;
}
inline void local(Writer& w, const LocalBlockDefinition& b) {
    w.u64(b.block_key); w.u8(static_cast<std::uint8_t>(b.kind));
    w.u64(b.cells.size()); for (const auto c : b.cells) cell(w, c);
    // ADR 0035: only partial charts persist occurrence selections.
    w.u64(b.kind == LocalBlockKind::PartialChart ? b.occurrences.size() : 0);
    if (b.kind == LocalBlockKind::PartialChart) for (const auto o : b.occurrences) occurrence(w, o);
    w.u64(b.payloads.size());
    for (const auto& p : b.payloads) {
        w.u64(p.payload_key); w.u8(static_cast<std::uint8_t>(p.binding));
        w.u8(p.cell.has_value()); if (p.cell) cell(w, *p.cell);
        w.u8(p.occurrence.has_value()); if (p.occurrence) occurrence(w, *p.occurrence);
        tensor(w, p.tensor);
    }
}
inline LocalBlockDefinition local(Reader& r) {
    LocalBlockDefinition b; b.block_key = r.u64(); b.kind = static_cast<LocalBlockKind>(r.u8());
    require(b.kind == LocalBlockKind::StrictSubcomplex || b.kind == LocalBlockKind::PartialChart, "unknown local block kind");
    auto n = r.count(10);
    for (std::size_t i = 0; i < n; ++i) {
        auto c = cell(r); require(b.cells.empty() || b.cells.back() < c, "unordered/duplicate local cell"); b.cells.push_back(c);
    }
    n = r.count(10); require(b.kind == LocalBlockKind::PartialChart || n == 0, "strict block duplicates source occurrences");
    for (std::size_t i = 0; i < n; ++i) {
        auto o = occurrence(r); require(b.occurrences.empty() || b.occurrences.back() < o, "unordered/duplicate chart occurrence"); b.occurrences.push_back(o);
    }
    n = r.count();
    for (std::size_t i = 0; i < n; ++i) {
        LocalExactTensorPayload p; p.payload_key = r.u64(); p.binding = static_cast<LocalPayloadBinding>(r.u8());
        require(p.binding <= LocalPayloadBinding::Occurrence, "unsupported local payload binding");
        if (r.flag()) p.cell = cell(r);
        if (r.flag()) p.occurrence = occurrence(r);
        p.tensor = tensor(r);
        require(b.payloads.empty() || b.payloads.back().payload_key < p.payload_key, "unordered/duplicate local payload");
        b.payloads.push_back(std::move(p));
    }
    return b;
}
inline void source_patch(Writer& w, const TemporalSourcePatch& s) {
    w.u32(s.target_grade_count); w.u64(s.grades.size());
    for (const auto& p : s.grades) {
        w.u16(p.grade.value); keys(w, p.removed_cells);
        w.u64(p.cells.size()); for (const auto& c : p.cells) { w.u64(c.key); w.u32(c.type); w.u8(c.birth); }
        w.u8(p.cell_order.has_value()); if (p.cell_order) keys(w, *p.cell_order);
        keys(w, p.removed_occurrences);
        w.u64(p.born_occurrences.size());
        for (const auto& o : p.born_occurrences) {
            w.u64(o.key); w.u64(o.parent); w.u64(o.lower); rational(w, o.coefficient);
            w.u64(o.aspects.size()); for (const auto a : o.aspects) w.u8(a);
        }
        w.u64(p.occurrences.size());
        for (const auto& o : p.occurrences) {
            w.u64(o.key); optional_key(w, o.parent); optional_key(w, o.lower);
            w.u8(o.coefficient.has_value()); if (o.coefficient) rational(w, *o.coefficient);
        }
        w.u64(p.fibers.size()); for (const auto& f : p.fibers) { w.u64(f.parent); keys(w, f.order); }
        w.u64(p.aspect_xor.size()); for (const auto& a : p.aspect_xor) { w.u32(a.aspect); w.u64(a.word_index); w.u64(a.bits); }
    }
}
inline TemporalSourcePatch source_patch(Reader& r) {
    TemporalSourcePatch s; s.target_grade_count = r.u32();
    require(s.target_grade_count > 0 && s.target_grade_count <= 65536, "invalid target grade count");
    const auto grades = r.count();
    for (std::size_t k = 0; k < grades; ++k) {
        TemporalGradePatch p; p.grade = core::Grade{r.u16()}; p.removed_cells = keys(r);
        auto n = r.count(13);
        for (std::size_t i = 0; i < n; ++i) {
            TemporalCellEdit c; c.key = r.u64(); c.type = r.u32(); c.birth = r.flag(); p.cells.push_back(c);
        }
        if (r.flag()) p.cell_order = keys(r);
        p.removed_occurrences = keys(r);
        n = r.count();
        for (std::size_t i = 0; i < n; ++i) {
            TemporalOccurrence o; o.key = r.u64(); o.parent = r.u64(); o.lower = r.u64(); o.coefficient = r.rational();
            const auto aspects = r.count(); for (std::size_t a = 0; a < aspects; ++a) o.aspects.push_back(r.flag() ? 1u : 0u);
            p.born_occurrences.push_back(std::move(o));
        }
        n = r.count();
        for (std::size_t i = 0; i < n; ++i) {
            TemporalOccurrenceEdit o; o.key = r.u64(); o.parent = optional_key(r); o.lower = optional_key(r);
            if (r.flag()) o.coefficient = r.rational();
            p.occurrences.push_back(std::move(o));
        }
        n = r.count();
        for (std::size_t i = 0; i < n; ++i) { TemporalFiberEdit f; f.parent = r.u64(); f.order = keys(r); p.fibers.push_back(std::move(f)); }
        n = r.count(20);
        for (std::size_t i = 0; i < n; ++i) {
            const auto a = r.u32();
            require(a <= std::numeric_limits<composite::AspectId>::max(), "aspect ID overflow");
            TemporalAspectXorWord x; x.aspect = static_cast<composite::AspectId>(a); x.word_index = r.u64(); x.bits = r.u64(); p.aspect_xor.push_back(x);
        }
        s.grades.push_back(std::move(p));
    }
    return s;
}
// Optional v1 feature. Empty extensions are not emitted: legacy endpoints and
// segments keep their original bytes and binding hashes.
inline constexpr std::uint32_t finite_systems_flag = 1u;
inline void finite_value(Writer& w, TemporalFiniteValueKind kind, const TemporalFiniteValue& value) {
    if (kind == TemporalFiniteValueKind::SignedInteger) {
        require(std::holds_alternative<std::int64_t>(value), "wrong finite integer value tag");
        w.u64(std::bit_cast<std::uint64_t>(std::get<std::int64_t>(value)));
    } else {
        require(kind == TemporalFiniteValueKind::RetainedRelation &&
                std::holds_alternative<core::RelationIdentity>(value), "wrong finite relation value tag");
        const auto relation = std::get<core::RelationIdentity>(value);
        cell(w, {relation.grade, relation.key.value});
    }
}
inline TemporalFiniteValue finite_value(Reader& r, TemporalFiniteValueKind kind) {
    if (kind == TemporalFiniteValueKind::SignedInteger) return std::bit_cast<std::int64_t>(r.u64());
    require(kind == TemporalFiniteValueKind::RetainedRelation, "unknown finite value kind");
    const auto key = cell(r);
    return core::RelationIdentity{key.grade, core::RelationKey{key.key}};
}
inline void finite_values(Writer& w, TemporalFiniteValueKind kind, const std::vector<TemporalFiniteValue>& values) {
    w.u64(values.size());
    for (const auto& value : values) finite_value(w, kind, value);
}
inline std::vector<TemporalFiniteValue> finite_values(Reader& r, TemporalFiniteValueKind kind) {
    const auto count = r.count(kind == TemporalFiniteValueKind::SignedInteger ? 8 : 10);
    std::vector<TemporalFiniteValue> values;
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) values.push_back(finite_value(r, kind));
    return values;
}
inline void finite_cells(Writer& w, const std::vector<StableCellKey>& cells) {
    w.u64(cells.size()); for (const auto key : cells) cell(w, key);
}
inline std::vector<StableCellKey> finite_cells(Reader& r) {
    const auto count = r.count(10); std::vector<StableCellKey> cells; cells.reserve(count);
    for (std::size_t i = 0; i < count; ++i) cells.push_back(cell(r));
    return cells;
}
inline void finite_system(Writer& w, const TemporalFiniteSystem& value) {
    const auto s = normalize_temporal_finite_system(value);
    w.u64(s.key); w.u8(static_cast<std::uint8_t>(s.kind));
    w.u64(s.domains.size());
    for (const auto& d : s.domains) { cell(w, d.cell); finite_values(w, s.kind, d.values); }
    w.u64(s.charts.size());
    for (const auto& chart : s.charts) {
        w.u64(chart.key); string(w, chart.name); finite_cells(w, chart.patch); finite_cells(w, chart.support);
        w.u64(chart.allowed.size());
        for (const auto& tuple : chart.allowed) finite_values(w, s.kind, tuple);
    }
    w.u64(s.sections.size());
    for (const auto& section : s.sections) {
        w.u64(section.key); w.u64(section.assignments.size());
        for (const auto& a : section.assignments) { cell(w, a.cell); finite_value(w, s.kind, a.value); }
    }
}
inline TemporalFiniteSystem finite_system(Reader& r) {
    TemporalFiniteSystem s; s.key = r.u64(); s.kind = static_cast<TemporalFiniteValueKind>(r.u8());
    require(s.kind == TemporalFiniteValueKind::SignedInteger || s.kind == TemporalFiniteValueKind::RetainedRelation,
            "unknown finite value kind");
    auto count = r.count(18);
    for (std::size_t i = 0; i < count; ++i) {
        TemporalFiniteDomain d; d.cell = cell(r); d.values = finite_values(r, s.kind);
        require(s.domains.empty() || s.domains.back().cell < d.cell, "unordered/duplicate finite domain");
        s.domains.push_back(std::move(d));
    }
    count = r.count(40);
    for (std::size_t i = 0; i < count; ++i) {
        TemporalFiniteChart chart; chart.key = r.u64(); chart.name = r.string();
        chart.patch = finite_cells(r); chart.support = finite_cells(r);
        require(std::adjacent_find(chart.patch.begin(), chart.patch.end(), [](auto a, auto b) { return !(a < b); }) == chart.patch.end(),
                "unordered/duplicate finite patch cell");
        const auto tuples = r.count(8);
        for (std::size_t j = 0; j < tuples; ++j) chart.allowed.push_back(finite_values(r, s.kind));
        s.charts.push_back(std::move(chart));
    }
    count = r.count(16);
    for (std::size_t i = 0; i < count; ++i) {
        TemporalFiniteSection section; section.key = r.u64();
        require(s.sections.empty() || s.sections.back().key < section.key, "unordered/duplicate finite section");
        const auto entries = r.count(s.kind == TemporalFiniteValueKind::SignedInteger ? 18 : 20);
        for (std::size_t j = 0; j < entries; ++j) {
            TemporalFiniteAssignment a; a.cell = cell(r); a.value = finite_value(r, s.kind);
            require(section.assignments.empty() || section.assignments.back().cell < a.cell, "unordered/duplicate finite assignment");
            section.assignments.push_back(std::move(a));
        }
        s.sections.push_back(std::move(section));
    }
    return s;
}
inline void finite_systems(Writer& w, const std::vector<TemporalFiniteSystem>& systems) {
    const auto ordered = index(systems, [](const auto& s) { return s.key; });
    w.u64(ordered.size());
    for (const auto& [key, system] : ordered) { (void)key; finite_system(w, *system); }
}
inline std::vector<TemporalFiniteSystem> finite_systems(Reader& r) {
    const auto count = r.count(33); std::vector<TemporalFiniteSystem> result;
    for (std::size_t i = 0; i < count; ++i) {
        auto system = finite_system(r);
        require(result.empty() || result.back().key < system.key, "unordered/duplicate finite system");
        result.push_back(std::move(system));
    }
    return result;
}

} // namespace temporal_codec_detail

} // namespace rfc::storage
