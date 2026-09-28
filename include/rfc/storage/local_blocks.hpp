#pragma once

#include "rfc/algebra/materialize.hpp"
#include "rfc/local/carrier.hpp"
#include "rfc/numeric/sparse_tensor.hpp"
#include "rfc/storage/rfcx.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace rfc::storage {

// Stable source identities used by local RFCX block indexes. Grade zero uses
// IdentityKey; positive grades use RelationKey. A block therefore never needs
// the ambient arena index as persistent identity.
struct StableCellKey {
    core::Grade grade{};
    std::uint64_t key{};
    constexpr auto operator<=>(const StableCellKey&) const = default;
};

struct StableOccurrenceKey {
    core::Grade upper_grade{};
    core::OccurrenceKey key{};
    constexpr auto operator<=>(const StableOccurrenceKey&) const = default;
};

enum class LocalBlockKind : std::uint8_t {
    StrictSubcomplex = 1,
    PartialChart = 2,
};

enum class LocalPayloadBinding : std::uint8_t {
    Block = 0,
    Cell = 1,
    Occurrence = 2,
};

struct LocalExactTensorPayload {
    std::uint64_t payload_key{};
    LocalPayloadBinding binding{LocalPayloadBinding::Block};
    std::optional<StableCellKey> cell;
    std::optional<StableOccurrenceKey> occurrence;
    numeric::SparseRationalTensor tensor{numeric::IndexDomain{},
                                         numeric::StructuralSupport::full(1),
                                         numeric::SparseRationalVector(1)};
};

struct LocalBlockDefinition {
    std::uint64_t block_key{};
    LocalBlockKind kind{LocalBlockKind::StrictSubcomplex};
    std::vector<StableCellKey> cells;
    std::vector<StableOccurrenceKey> occurrences;
    std::vector<LocalExactTensorPayload> payloads;
};

[[nodiscard]] inline StableCellKey stable_cell_key(const core::RelationalComplex& source,
                                                     core::CellId cell) {
    if (cell.grade.value >= source.grade_count()) throw std::out_of_range("stable cell grade");
    const auto& arena = source.grade(cell.grade);
    if (cell.index >= arena.cell_count()) throw std::out_of_range("stable cell index");
    if (cell.grade.value == 0) return {cell.grade, arena.identities.at(cell.index)};
    return {cell.grade, arena.relation_identities.at(cell.index).value};
}

[[nodiscard]] inline std::optional<core::CellId> resolve_stable_cell(const core::RelationalComplex& source,
                                                                      StableCellKey key) {
    if (key.grade.value >= source.grade_count()) return std::nullopt;
    const auto& arena = source.grade(key.grade);
    if (key.grade.value == 0) {
        const auto it = std::find(arena.identities.begin(), arena.identities.end(), key.key);
        if (it == arena.identities.end()) return std::nullopt;
        return core::CellId{key.grade, static_cast<std::uint32_t>(std::distance(arena.identities.begin(), it))};
    }
    const auto it = std::find_if(arena.relation_identities.begin(), arena.relation_identities.end(),
                                 [&](core::RelationKey candidate) { return candidate.value == key.key; });
    if (it == arena.relation_identities.end()) return std::nullopt;
    return core::CellId{key.grade, static_cast<std::uint32_t>(std::distance(arena.relation_identities.begin(), it))};
}

[[nodiscard]] inline std::optional<core::CellId> resolve_stable_cell(const MappedRfcx& source,
                                                                      StableCellKey key) {
    if (key.grade.value >= source.grade_count()) return std::nullopt;
    const auto grade = source.grade(key.grade);
    if (key.grade.value == 0) {
        const auto found = grade.find_identity(key.key);
        if (!found) return std::nullopt;
        return core::CellId{key.grade, *found};
    }
    const auto found = grade.find_relation(core::RelationKey{key.key});
    if (!found) return std::nullopt;
    return core::CellId{key.grade, *found};
}

[[nodiscard]] inline std::optional<std::size_t> resolve_stable_occurrence(const core::RelationalComplex& source,
                                                                           StableOccurrenceKey key) {
    if (key.upper_grade.value == 0 || key.upper_grade.value >= source.grade_count()) return std::nullopt;
    const auto& arena = source.grade(key.upper_grade);
    const auto it = std::find(arena.occurrence_identities.begin(), arena.occurrence_identities.end(), key.key);
    if (it == arena.occurrence_identities.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(arena.occurrence_identities.begin(), it));
}

[[nodiscard]] inline std::optional<std::size_t> resolve_stable_occurrence(const MappedRfcx& source,
                                                                           StableOccurrenceKey key) {
    if (key.upper_grade.value == 0 || key.upper_grade.value >= source.grade_count()) return std::nullopt;
    const auto found = source.grade(key.upper_grade).find_occurrence(key.key);
    if (!found) return std::nullopt;
    return static_cast<std::size_t>(*found);
}

inline void canonicalize_local_block(LocalBlockDefinition& block) {
    if (block.kind != LocalBlockKind::StrictSubcomplex && block.kind != LocalBlockKind::PartialChart)
        throw std::invalid_argument("unknown local block kind");
    std::sort(block.cells.begin(), block.cells.end());
    block.cells.erase(std::unique(block.cells.begin(), block.cells.end()), block.cells.end());
    std::sort(block.occurrences.begin(), block.occurrences.end());
    block.occurrences.erase(std::unique(block.occurrences.begin(), block.occurrences.end()), block.occurrences.end());
    std::sort(block.payloads.begin(), block.payloads.end(), [](const auto& a, const auto& b) {
        return a.payload_key < b.payload_key;
    });
    for (std::size_t i = 1; i < block.payloads.size(); ++i) {
        if (block.payloads[i - 1].payload_key == block.payloads[i].payload_key) {
            throw std::invalid_argument("duplicate local payload key");
        }
    }
}

[[nodiscard]] inline LocalBlockDefinition make_strict_local_block(const core::RelationalComplex& source,
                                                                   std::uint64_t block_key,
                                                                   const algebra::SubcomplexMask& patch,
                                                                   std::vector<LocalExactTensorPayload> payloads = {}) {
    if (!patch.boundary_closed(source)) throw std::invalid_argument("strict local block must be boundary closed");
    LocalBlockDefinition out;
    out.block_key = block_key;
    out.kind = LocalBlockKind::StrictSubcomplex;
    out.payloads = std::move(payloads);
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const auto& arena = source.grade(grade);
        patch.grade(grade).for_each_set([&](std::size_t index) {
            out.cells.push_back(stable_cell_key(source, core::CellId{grade, static_cast<std::uint32_t>(index)}));
            if (k == 0) return;
            const auto [begin, end] = arena.incidence_range(index);
            for (std::size_t p = begin; p < end; ++p) {
                out.occurrences.push_back({grade, arena.occurrence_identities.at(p)});
            }
        });
    }
    canonicalize_local_block(out);
    return out;
}

[[nodiscard]] inline LocalBlockDefinition make_partial_local_chart(const core::RelationalComplex& source,
                                                                    std::uint64_t block_key,
                                                                    std::vector<core::CellId> cells,
                                                                    std::vector<StableOccurrenceKey> occurrences,
                                                                    std::vector<LocalExactTensorPayload> payloads = {}) {
    LocalBlockDefinition out;
    out.block_key = block_key;
    out.kind = LocalBlockKind::PartialChart;
    out.payloads = std::move(payloads);
    out.occurrences = std::move(occurrences);
    out.cells.reserve(cells.size());
    for (const auto cell : cells) out.cells.push_back(stable_cell_key(source, cell));
    canonicalize_local_block(out);

    // A retained chart occurrence must be a source occurrence and both of its
    // endpoint cells must be represented by the chart. The chart need not
    // contain every boundary occurrence of its selected upper cells.
    for (const auto occurrence : out.occurrences) {
        const auto slot = resolve_stable_occurrence(source, occurrence);
        if (!slot) throw std::invalid_argument("partial chart occurrence is not in source");
        const auto& arena = source.grade(occurrence.upper_grade);
        const auto upper = stable_cell_key(source, core::CellId{occurrence.upper_grade, arena.upper_index.at(*slot)});
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(occurrence.upper_grade.value - 1u)};
        const auto lower = stable_cell_key(source, core::CellId{lower_grade, arena.lower_index.at(*slot)});
        if (!std::binary_search(out.cells.begin(), out.cells.end(), upper) ||
            !std::binary_search(out.cells.begin(), out.cells.end(), lower)) {
            throw std::invalid_argument("partial chart occurrence endpoints must both be retained by chart");
        }
    }
    return out;
}

class GlueIndex {
public:
    struct Membership {
        StableCellKey cell{};
        std::vector<std::uint64_t> block_keys;
    };

    GlueIndex() = default;
    explicit GlueIndex(const std::vector<LocalBlockDefinition>& blocks) { rebuild(blocks); }

    void rebuild(const std::vector<LocalBlockDefinition>& blocks) {
        std::map<StableCellKey, std::vector<std::uint64_t>> memberships;
        for (const auto& block : blocks) {
            for (const auto cell : block.cells) memberships[cell].push_back(block.block_key);
        }
        shared_.clear();
        for (auto& [cell, members] : memberships) {
            std::sort(members.begin(), members.end());
            members.erase(std::unique(members.begin(), members.end()), members.end());
            if (members.size() > 1u) shared_.push_back({cell, std::move(members)});
        }
    }

    [[nodiscard]] const std::vector<Membership>& shared_cells() const noexcept { return shared_; }
    [[nodiscard]] std::size_t shared_cell_count() const noexcept { return shared_.size(); }

    [[nodiscard]] const Membership* find(StableCellKey cell) const noexcept {
        const auto it = std::lower_bound(shared_.begin(), shared_.end(), cell,
            [](const Membership& item, StableCellKey key) { return item.cell < key; });
        if (it == shared_.end() || it->cell != cell) return nullptr;
        return &*it;
    }

    [[nodiscard]] std::vector<StableCellKey> shared_between(std::uint64_t left,
                                                             std::uint64_t right) const {
        std::vector<StableCellKey> out;
        if (left > right) std::swap(left, right);
        for (const auto& item : shared_) {
            if (std::binary_search(item.block_keys.begin(), item.block_keys.end(), left) &&
                std::binary_search(item.block_keys.begin(), item.block_keys.end(), right)) out.push_back(item.cell);
        }
        return out;
    }

private:
    std::vector<Membership> shared_;
};

[[nodiscard]] inline algebra::SubcomplexMask strict_block_mask(const core::RelationalComplex& source,
                                                                const LocalBlockDefinition& block) {
    if (block.kind != LocalBlockKind::StrictSubcomplex) throw std::invalid_argument("block is not a strict subcomplex");
    algebra::SubcomplexMask mask(source);
    for (const auto stable : block.cells) {
        const auto cell = resolve_stable_cell(source, stable);
        if (!cell) throw std::invalid_argument("local block cell does not resolve in source");
        mask.select(*cell);
    }
    if (!mask.boundary_closed(source)) throw std::invalid_argument("strict local block cell set is not boundary closed");
    return mask;
}

[[nodiscard]] inline algebra::MaterializedSubcomplex materialize_glued_strict_blocks(
    const core::RelationalComplex& source,
    std::span<const LocalBlockDefinition> blocks) {
    algebra::SubcomplexMask union_mask(source);
    for (const auto& block : blocks) {
        const auto mask = strict_block_mask(source, block);
        union_mask = join(union_mask, mask);
    }
    if (!union_mask.boundary_closed(source)) throw std::logic_error("union of strict local blocks lost boundary closure");
    return algebra::materialize_subcomplex(source, union_mask);
}

namespace local_rfcx_detail {

inline constexpr std::array<std::byte, 8> magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'X'},
    std::byte{'L'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::uint32_t version = 1;
inline constexpr std::size_t header_size = 144;
inline constexpr std::size_t block_descriptor_size = 64;
inline constexpr std::size_t stable_ref_size = 16;

struct BlockDescriptor {
    std::uint64_t block_key{};
    std::uint64_t kind{};
    std::uint64_t cell_begin{};
    std::uint64_t cell_count{};
    std::uint64_t occurrence_begin{};
    std::uint64_t occurrence_count{};
    std::uint64_t payload_begin{};
    std::uint64_t payload_count{};
};

inline void write_stable_cell(rfcx_detail::Writer& w, StableCellKey key) {
    w.u16(key.grade.value);
    for (int i = 0; i < 6; ++i) w.u8(0);
    w.u64(key.key);
}
inline StableCellKey read_stable_cell(rfcx_detail::Reader& r) {
    StableCellKey key{core::Grade{r.u16()}, 0};
    for (int i = 0; i < 6; ++i) if (r.u8() != 0) throw std::runtime_error("RFCX local reference padding");
    key.key = r.u64();
    return key;
}
inline void write_stable_occurrence(rfcx_detail::Writer& w, StableOccurrenceKey key) {
    w.u16(key.upper_grade.value);
    for (int i = 0; i < 6; ++i) w.u8(0);
    w.u64(key.key.value);
}
inline StableOccurrenceKey read_stable_occurrence(rfcx_detail::Reader& r) {
    StableOccurrenceKey key{core::Grade{r.u16()}, {0}};
    for (int i = 0; i < 6; ++i) if (r.u8() != 0) throw std::runtime_error("RFCX local reference padding");
    key.key.value = r.u64();
    return key;
}

inline void write_payload(rfcx_detail::Writer& w, const LocalExactTensorPayload& payload) {
    w.u64(payload.payload_key);
    w.u8(static_cast<std::uint8_t>(payload.binding));
    w.u8(payload.cell ? 1u : 0u);
    w.u8(payload.occurrence ? 1u : 0u);
    for (int i = 0; i < 5; ++i) w.u8(0);
    write_stable_cell(w, payload.cell.value_or(StableCellKey{}));
    write_stable_occurrence(w, payload.occurrence.value_or(StableOccurrenceKey{}));
    if (payload.tensor.domain().rank() > std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("local tensor rank overflow");
    w.u32(static_cast<std::uint32_t>(payload.tensor.domain().rank()));
    w.u32(0u);
    for (const auto extent : payload.tensor.domain().extents()) w.u64(static_cast<std::uint64_t>(extent));
    std::vector<std::size_t> support;
    support.reserve(payload.tensor.support().count());
    payload.tensor.support().for_each_active([&](std::size_t index) { support.push_back(index); });
    w.u64(static_cast<std::uint64_t>(support.size()));
    for (const auto index : support) w.u64(static_cast<std::uint64_t>(index));
    w.u64(static_cast<std::uint64_t>(payload.tensor.values().nonzeros()));
    for (const auto& entry : payload.tensor.values().entries()) {
        w.u64(static_cast<std::uint64_t>(entry.index));
        rfcx_detail::write_rational(w, entry.value);
    }
}

inline LocalExactTensorPayload read_payload(std::span<const std::byte> bytes, std::size_t offset) {
    rfcx_detail::Reader r(bytes, offset);
    LocalExactTensorPayload payload;
    payload.payload_key = r.u64();
    payload.binding = static_cast<LocalPayloadBinding>(r.u8());
    if (payload.binding != LocalPayloadBinding::Block && payload.binding != LocalPayloadBinding::Cell &&
        payload.binding != LocalPayloadBinding::Occurrence) throw std::runtime_error("unknown local payload binding");
    const bool has_cell = r.boolean();
    const bool has_occurrence = r.boolean();
    if ((payload.binding == LocalPayloadBinding::Cell && !has_cell) ||
        (payload.binding == LocalPayloadBinding::Occurrence && !has_occurrence))
        throw std::runtime_error("missing local payload binding key");
    for (int i = 0; i < 5; ++i) if (r.u8() != 0) throw std::runtime_error("RFCX local payload flags");
    const auto cell = read_stable_cell(r);
    const auto occurrence = read_stable_occurrence(r);
    if (has_cell) payload.cell = cell;
    if (has_occurrence) payload.occurrence = occurrence;
    const auto rank = r.u32();
    if (r.u32() != 0) throw std::runtime_error("RFCX local tensor flags");
    r.require_items(rank,8u);
    std::vector<std::size_t> extents;
    extents.reserve(rank);
    for (std::uint32_t i = 0; i < rank; ++i) extents.push_back(r.size_value());
    numeric::IndexDomain domain(std::move(extents));
    const auto support_count = r.size_value();
    r.require_items(support_count,8u);
    std::vector<std::size_t> support;
    support.reserve(support_count);
    for (std::size_t i = 0; i < support_count; ++i) support.push_back(r.size_value());
    const auto nonzero_count = r.size_value();
    r.require_items(nonzero_count,18u);
    std::vector<numeric::SparseRationalEntry> entries;
    entries.reserve(nonzero_count);
    for (std::size_t i = 0; i < nonzero_count; ++i) {
        entries.push_back({r.size_value(), rfcx_detail::read_rational(r)});
    }
    if (r.remaining() > 7u) throw std::runtime_error("trailing local payload bytes");
    while (r.remaining()) if (r.u8() != 0) throw std::runtime_error("nonzero local payload padding");
    for (std::size_t i=1; i<support.size(); ++i)
        if (support[i-1]>=support[i]) throw std::runtime_error("unsorted local tensor support");
    for (std::size_t i=0; i<entries.size(); ++i)
        if (entries[i].value==0 || (i && entries[i-1].index>=entries[i].index))
            throw std::runtime_error("noncanonical local tensor entries");
    const auto logical_size = domain.logical_size();
    payload.tensor = numeric::SparseRationalTensor(std::move(domain),
        numeric::StructuralSupport::sparse(logical_size, std::move(support)),
        numeric::SparseRationalVector(logical_size, std::move(entries)));
    return payload;
}

} // namespace local_rfcx_detail

// Phase-2 writer intentionally stores local selection/index metadata separately
// from the RFCX source arrays. Local blocks therefore refer to one physical copy
// of the stable source data instead of duplicating bitplanes, lattice lanes, and
// occurrence fibers.
struct LocalBlockWriteStats {
    std::uint64_t bytes{};
    std::uint64_t blocks{};
    std::uint64_t shared_cells{};
};

} // namespace rfc::storage

namespace rfc::storage {



class StableSourceIndex {
public:
    explicit StableSourceIndex(const core::RelationalComplex& source) {
        cells_.resize(source.grade_count());
        occurrences_.resize(source.grade_count());
        for (std::size_t k=0;k<source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena=source.grade(grade);
            auto& cell_map=cells_[k];
            cell_map.reserve(arena.cell_count());
            if (k==0) {
                for (std::size_t i=0;i<arena.identities.size();++i) cell_map.emplace(arena.identities[i],static_cast<std::uint32_t>(i));
            } else {
                for (std::size_t i=0;i<arena.relation_identities.size();++i) cell_map.emplace(arena.relation_identities[i].value,static_cast<std::uint32_t>(i));
                auto& occ_map=occurrences_[k];
                occ_map.reserve(arena.occurrence_identities.size());
                for (std::size_t i=0;i<arena.occurrence_identities.size();++i) occ_map.emplace(arena.occurrence_identities[i].value,i);
            }
        }
    }

    [[nodiscard]] std::optional<core::CellId> cell(StableCellKey key) const {
        if (key.grade.value>=cells_.size()) return std::nullopt;
        const auto it=cells_[key.grade.value].find(key.key);
        if (it==cells_[key.grade.value].end()) return std::nullopt;
        return core::CellId{key.grade,it->second};
    }
    [[nodiscard]] std::optional<std::size_t> occurrence(StableOccurrenceKey key) const {
        if (key.upper_grade.value==0 || key.upper_grade.value>=occurrences_.size()) return std::nullopt;
        const auto& map=occurrences_[key.upper_grade.value];
        const auto it=map.find(key.key.value);
        if (it==map.end()) return std::nullopt;
        return it->second;
    }
private:
    std::vector<std::unordered_map<std::uint64_t,std::uint32_t>> cells_;
    std::vector<std::unordered_map<std::uint64_t,std::size_t>> occurrences_;
};

inline void validate_local_payload_binding(const LocalBlockDefinition& block,
                                           const LocalExactTensorPayload& payload) {
    if (payload.binding != LocalPayloadBinding::Block && payload.binding != LocalPayloadBinding::Cell &&
        payload.binding != LocalPayloadBinding::Occurrence) throw std::invalid_argument("unknown local payload binding");
    if (payload.binding == LocalPayloadBinding::Cell) {
        if (!payload.cell) throw std::invalid_argument("cell-bound local payload missing cell key");
        if (!std::binary_search(block.cells.begin(), block.cells.end(), *payload.cell)) {
            throw std::invalid_argument("local payload cell is outside block");
        }
    } else if (payload.binding == LocalPayloadBinding::Occurrence) {
        if (!payload.occurrence) throw std::invalid_argument("occurrence-bound local payload missing occurrence key");
        if (!std::binary_search(block.occurrences.begin(), block.occurrences.end(), *payload.occurrence)) {
            throw std::invalid_argument("local payload occurrence is outside block");
        }
    }
}

inline void validate_local_block_against_source(const core::RelationalComplex& source,
                                                const StableSourceIndex& index,
                                                const LocalBlockDefinition& block) {
    for (const auto stable : block.cells) {
        if (!index.cell(stable)) throw std::invalid_argument("local block cell does not resolve in source");
    }
    for (const auto stable : block.occurrences) {
        const auto slot = index.occurrence(stable);
        if (!slot) throw std::invalid_argument("local block occurrence does not resolve in source");
        const auto& arena = source.grade(stable.upper_grade);
        const auto upper = stable_cell_key(source, core::CellId{stable.upper_grade, arena.upper_index.at(*slot)});
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(stable.upper_grade.value - 1u)};
        const auto lower = stable_cell_key(source, core::CellId{lower_grade, arena.lower_index.at(*slot)});
        if (!std::binary_search(block.cells.begin(), block.cells.end(), upper) ||
            !std::binary_search(block.cells.begin(), block.cells.end(), lower)) {
            throw std::invalid_argument("local block occurrence endpoints lie outside block");
        }
    }
    for (const auto& payload : block.payloads) validate_local_payload_binding(block, payload);

    if (block.kind == LocalBlockKind::StrictSubcomplex) {
            std::vector<StableOccurrenceKey> expected;
        for (const auto stable_cell : block.cells) {
            if (stable_cell.grade.value==0) continue;
            const auto cell=index.cell(stable_cell);
            const auto& arena=source.grade(stable_cell.grade);
            const auto [begin,end]=arena.incidence_range(cell->index);
            for (std::size_t p=begin;p<end;++p) {
                expected.push_back({stable_cell.grade,arena.occurrence_identities.at(p)});
            }
        }
        std::sort(expected.begin(), expected.end());
        expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
        if (expected != block.occurrences) throw std::invalid_argument("strict local block must retain complete occurrence fibers");

        // Check boundary closure directly through stable keys and the shared
        // source index without constructing a second SubcomplexMask.
        for (const auto stable_cell : block.cells) {
            if (stable_cell.grade.value==0) continue;
            const auto cell=index.cell(stable_cell);
            const auto& arena=source.grade(stable_cell.grade);
            const auto lower_grade=core::Grade{static_cast<std::uint16_t>(stable_cell.grade.value-1u)};
            const auto [begin,end]=arena.incidence_range(cell->index);
            for (std::size_t p=begin;p<end;++p) {
                const auto lower=stable_cell_key(source,core::CellId{lower_grade,arena.lower_index.at(p)});
                if (!std::binary_search(block.cells.begin(),block.cells.end(),lower)) {
                    throw std::invalid_argument("strict local block cell set is not boundary closed");
                }
            }
        }
    }
}

inline void validate_local_block_against_source(const core::RelationalComplex& source,
                                                const LocalBlockDefinition& block) {
    const StableSourceIndex index(source);
    validate_local_block_against_source(source,index,block);
}

inline LocalBlockWriteStats write_local_blocks(const core::RelationalComplex& source,
                                                std::uint64_t source_payload_hash,
                                                std::vector<LocalBlockDefinition> blocks,
                                                const std::filesystem::path& path) {
    using namespace local_rfcx_detail;
    using namespace rfcx_detail;
    validate_host();
    for (auto& block : blocks) canonicalize_local_block(block);
    std::sort(blocks.begin(), blocks.end(), [](const auto& a, const auto& b) { return a.block_key < b.block_key; });
    for (std::size_t i = 1; i < blocks.size(); ++i) {
        if (blocks[i - 1].block_key == blocks[i].block_key) throw std::invalid_argument("duplicate local block key");
    }
    const StableSourceIndex source_index(source);
    for (const auto& block : blocks) {
        validate_local_block_against_source(source, source_index, block);
    }
    GlueIndex glue(blocks);
    std::vector<StableCellKey> cells;
    std::vector<StableOccurrenceKey> occurrences;
    std::vector<const LocalExactTensorPayload*> payloads;
    std::vector<BlockDescriptor> descriptors;
    cells.reserve(256);
    occurrences.reserve(256);
    descriptors.reserve(blocks.size());
    for (const auto& block : blocks) {
        BlockDescriptor d;
        d.block_key = block.block_key;
        d.kind = static_cast<std::uint64_t>(block.kind);
        d.cell_begin = cells.size(); d.cell_count = block.cells.size();
        d.occurrence_begin = occurrences.size();
        d.occurrence_count = block.kind == LocalBlockKind::PartialChart ? block.occurrences.size() : 0u;
        d.payload_begin = payloads.size(); d.payload_count = block.payloads.size();
        cells.insert(cells.end(), block.cells.begin(), block.cells.end());
        if (block.kind == LocalBlockKind::PartialChart) {
            occurrences.insert(occurrences.end(), block.occurrences.begin(), block.occurrences.end());
        }
        for (const auto& payload : block.payloads) payloads.push_back(&payload);
        descriptors.push_back(d);
    }

    Writer w;
    const auto header = w.reserve(local_rfcx_detail::header_size);

    w.align(8);
    const auto cells_offset = static_cast<std::uint64_t>(w.size());
    for (const auto key : cells) write_stable_cell(w, key);

    w.align(8);
    const auto occurrences_offset = static_cast<std::uint64_t>(w.size());
    for (const auto key : occurrences) write_stable_occurrence(w, key);

    std::vector<std::uint64_t> payload_offsets;
    payload_offsets.reserve(payloads.size() + 1u);
    w.align(8);
    const auto payload_blob_offset = static_cast<std::uint64_t>(w.size());
    for (const auto* payload : payloads) {
        payload_offsets.push_back(static_cast<std::uint64_t>(w.size()));
        write_payload(w, *payload);
        w.align(8);
    }
    payload_offsets.push_back(static_cast<std::uint64_t>(w.size()));
    w.align(8);
    const auto payload_offsets_offset = static_cast<std::uint64_t>(w.size());
    for (const auto off : payload_offsets) w.u64(off);

    w.align(8);
    const auto glue_cells_offset = static_cast<std::uint64_t>(w.size());
    for (const auto& item : glue.shared_cells()) write_stable_cell(w, item.cell);
    std::vector<std::uint64_t> glue_offsets;
    std::vector<std::uint64_t> glue_members;
    glue_offsets.reserve(glue.shared_cell_count() + 1u);
    glue_offsets.push_back(0);
    for (const auto& item : glue.shared_cells()) {
        glue_members.insert(glue_members.end(), item.block_keys.begin(), item.block_keys.end());
        glue_offsets.push_back(glue_members.size());
    }
    w.align(8);
    const auto glue_offsets_offset = static_cast<std::uint64_t>(w.size());
    for (const auto off : glue_offsets) w.u64(off);
    w.align(8);
    const auto glue_members_offset = static_cast<std::uint64_t>(w.size());
    for (const auto key : glue_members) w.u64(key);

    w.align(8);
    const auto block_dir_offset = static_cast<std::uint64_t>(w.size());
    for (const auto& d : descriptors) {
        w.u64(d.block_key); w.u64(d.kind); w.u64(d.cell_begin); w.u64(d.cell_count);
        w.u64(d.occurrence_begin); w.u64(d.occurrence_count); w.u64(d.payload_begin); w.u64(d.payload_count);
    }

    for (std::size_t i = 0; i < local_rfcx_detail::magic.size(); ++i) w.patch_byte(header + i, local_rfcx_detail::magic[i]);
    w.patch_u32(header + 8u, local_rfcx_detail::version);
    w.patch_u32(header + 12u, rfcx_detail::endian_tag);
    w.patch_u64(header + 16u, static_cast<std::uint64_t>(w.size()));
    w.patch_u64(header + 24u, static_cast<std::uint64_t>(blocks.size()));
    w.patch_u64(header + 32u, static_cast<std::uint64_t>(cells.size()));
    w.patch_u64(header + 40u, static_cast<std::uint64_t>(occurrences.size()));
    w.patch_u64(header + 48u, static_cast<std::uint64_t>(payloads.size()));
    w.patch_u64(header + 56u, block_dir_offset);
    w.patch_u64(header + 64u, cells_offset);
    w.patch_u64(header + 72u, occurrences_offset);
    w.patch_u64(header + 80u, payload_offsets_offset);
    w.patch_u64(header + 88u, payload_blob_offset);
    w.patch_u64(header + 96u, static_cast<std::uint64_t>(glue.shared_cell_count()));
    w.patch_u64(header + 104u, glue_cells_offset);
    w.patch_u64(header + 112u, glue_offsets_offset);
    w.patch_u64(header + 120u, glue_members_offset);
    w.patch_u64(header + 128u, rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(local_rfcx_detail::header_size)));
    w.patch_u64(header + 136u, source_payload_hash);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("unable to create RFCX local-block file");
    out.write(reinterpret_cast<const char*>(w.bytes().data()), static_cast<std::streamsize>(w.bytes().size()));
    if (!out) throw std::runtime_error("unable to write RFCX local-block file");
    return {static_cast<std::uint64_t>(w.size()), static_cast<std::uint64_t>(blocks.size()),
            static_cast<std::uint64_t>(glue.shared_cell_count())};
}

class MappedLocalBlocks {
    struct ViewLifetime { const MappedLocalBlocks* owner; };
public:
    explicit MappedLocalBlocks(const std::filesystem::path& path, bool verify_integrity = true)
        : file_(path) {
        rfcx_detail::validate_host();
        parse(verify_integrity);
    }

    MappedLocalBlocks(const MappedLocalBlocks&) = delete;
    MappedLocalBlocks& operator=(const MappedLocalBlocks&) = delete;
    MappedLocalBlocks(MappedLocalBlocks&& other) noexcept
        : file_(std::move(other.file_)),
          block_count_(std::exchange(other.block_count_,0)),
          cell_ref_count_(std::exchange(other.cell_ref_count_,0)),
          occurrence_ref_count_(std::exchange(other.occurrence_ref_count_,0)),
          payload_count_(std::exchange(other.payload_count_,0)),
          block_dir_offset_(std::exchange(other.block_dir_offset_,0)),
          cells_offset_(std::exchange(other.cells_offset_,0)),
          occurrences_offset_(std::exchange(other.occurrences_offset_,0)),
          payload_offsets_offset_(std::exchange(other.payload_offsets_offset_,0)),
          payload_blob_offset_(std::exchange(other.payload_blob_offset_,0)),
          glue_count_(std::exchange(other.glue_count_,0)),
          glue_cells_offset_(std::exchange(other.glue_cells_offset_,0)),
          glue_offsets_offset_(std::exchange(other.glue_offsets_offset_,0)),
          glue_members_offset_(std::exchange(other.glue_members_offset_,0)),
          payload_hash_(std::exchange(other.payload_hash_,0)),
          source_fingerprint_(std::exchange(other.source_fingerprint_,0)),
          lifetime_(std::move(other.lifetime_)) { if (lifetime_) lifetime_->owner=this; }
    MappedLocalBlocks& operator=(MappedLocalBlocks&& other) noexcept {
        if (this != &other) {
            file_=std::move(other.file_);
            block_count_=std::exchange(other.block_count_,0);
            cell_ref_count_=std::exchange(other.cell_ref_count_,0);
            occurrence_ref_count_=std::exchange(other.occurrence_ref_count_,0);
            payload_count_=std::exchange(other.payload_count_,0);
            block_dir_offset_=std::exchange(other.block_dir_offset_,0);
            cells_offset_=std::exchange(other.cells_offset_,0);
            occurrences_offset_=std::exchange(other.occurrences_offset_,0);
            payload_offsets_offset_=std::exchange(other.payload_offsets_offset_,0);
            payload_blob_offset_=std::exchange(other.payload_blob_offset_,0);
            glue_count_=std::exchange(other.glue_count_,0);
            glue_cells_offset_=std::exchange(other.glue_cells_offset_,0);
            glue_offsets_offset_=std::exchange(other.glue_offsets_offset_,0);
            glue_members_offset_=std::exchange(other.glue_members_offset_,0);
            payload_hash_=std::exchange(other.payload_hash_,0);
            source_fingerprint_=std::exchange(other.source_fingerprint_,0);
            lifetime_=std::move(other.lifetime_);
            if (lifetime_) lifetime_->owner=this;
        }
        return *this;
    }

    struct BlockView {
        const MappedLocalBlocks* owner{};
        local_rfcx_detail::BlockDescriptor desc{};
        std::weak_ptr<ViewLifetime> lifetime;
        [[nodiscard]] const MappedLocalBlocks* live_owner() const {
            const auto state=lifetime.lock();
            if (!state) throw std::logic_error("expired RFCX local block view");
            return state->owner;
        }
        [[nodiscard]] std::uint64_t key() const noexcept { return desc.block_key; }
        [[nodiscard]] LocalBlockKind kind() const noexcept { return static_cast<LocalBlockKind>(desc.kind); }
        [[nodiscard]] std::size_t cell_count() const noexcept { return static_cast<std::size_t>(desc.cell_count); }
        [[nodiscard]] std::size_t occurrence_count() const noexcept { return static_cast<std::size_t>(desc.occurrence_count); }
        [[nodiscard]] std::size_t payload_count() const noexcept { return static_cast<std::size_t>(desc.payload_count); }
        [[nodiscard]] StableCellKey cell(std::size_t index) const {
            if (index >= desc.cell_count) throw std::out_of_range("local block cell index");
            return live_owner()->cell_ref(static_cast<std::size_t>(desc.cell_begin) + index);
        }
        [[nodiscard]] StableOccurrenceKey occurrence(std::size_t index) const {
            if (index >= desc.occurrence_count) throw std::out_of_range("local block occurrence index");
            return live_owner()->occurrence_ref(static_cast<std::size_t>(desc.occurrence_begin) + index);
        }
        [[nodiscard]] LocalExactTensorPayload payload(std::size_t index) const {
            if (index >= desc.payload_count) throw std::out_of_range("local block payload index");
            return live_owner()->payload(static_cast<std::size_t>(desc.payload_begin) + index);
        }
    };

    [[nodiscard]] std::size_t block_count() const noexcept { return static_cast<std::size_t>(block_count_); }
    [[nodiscard]] std::size_t shared_cell_count() const noexcept { return static_cast<std::size_t>(glue_count_); }
    [[nodiscard]] std::uint64_t file_bytes() const noexcept { return file_.size(); }

    [[nodiscard]] BlockView block(std::size_t index) const {
        if (index >= block_count_) throw std::out_of_range("local block index");
        return {this, descriptor(index), lifetime_};
    }

    [[nodiscard]] std::optional<BlockView> find_block(std::uint64_t key) const {
        std::size_t lo = 0, hi = block_count();
        while (lo < hi) {
            const auto mid = (lo + hi) / 2u;
            const auto d = descriptor(mid);
            if (d.block_key < key) lo = mid + 1u; else hi = mid;
        }
        if (lo >= block_count() || descriptor(lo).block_key != key) return std::nullopt;
        return BlockView{this, descriptor(lo), lifetime_};
    }

    [[nodiscard]] StableCellKey shared_cell(std::size_t index) const {
        if (index >= glue_count_) throw std::out_of_range("glue cell index");
        return read_cell_at(glue_cells_offset_ + static_cast<std::uint64_t>(index) * local_rfcx_detail::stable_ref_size);
    }

    [[nodiscard]] std::vector<std::uint64_t> memberships(std::size_t shared_index) const {
        if (shared_index >= glue_count_) throw std::out_of_range("glue membership index");
        const auto begin = u64_at(glue_offsets_offset_ + static_cast<std::uint64_t>(shared_index) * 8u);
        const auto end = u64_at(glue_offsets_offset_ + static_cast<std::uint64_t>(shared_index + 1u) * 8u);
        if (end < begin) throw std::runtime_error("corrupt local glue offsets");
        std::vector<std::uint64_t> out;
        out.reserve(static_cast<std::size_t>(end - begin));
        for (auto i = begin; i < end; ++i) out.push_back(u64_at(glue_members_offset_ + i * 8u));
        return out;
    }

    [[nodiscard]] std::optional<std::size_t> find_shared_cell(StableCellKey key) const {
        std::size_t lo = 0, hi = shared_cell_count();
        while (lo < hi) {
            const auto mid = (lo + hi) / 2u;
            const auto found = shared_cell(mid);
            if (found < key) lo = mid + 1u; else hi = mid;
        }
        if (lo >= shared_cell_count() || shared_cell(lo) != key) return std::nullopt;
        return lo;
    }

    [[nodiscard]] std::vector<StableCellKey> shared_between(std::uint64_t left,
                                                             std::uint64_t right) const {
        if (left > right) std::swap(left, right);
        std::vector<StableCellKey> out;
        for (std::size_t i = 0; i < shared_cell_count(); ++i) {
            const auto members = memberships(i);
            if (std::binary_search(members.begin(), members.end(), left) &&
                std::binary_search(members.begin(), members.end(), right)) out.push_back(shared_cell(i));
        }
        return out;
    }

    void validate_against(const MappedRfcx& source) const {
        if (source.payload_hash() != source_fingerprint_) {
            throw std::runtime_error("RFCX local-block source payload hash mismatch");
        }
        for (std::size_t bi = 0; bi < block_count(); ++bi) {
            const auto view = block(bi);
            std::vector<StableCellKey> cells;
            cells.reserve(view.cell_count());
            for (std::size_t i = 0; i < view.cell_count(); ++i) {
                const auto key = view.cell(i);
                if (!resolve_stable_cell(source, key)) throw std::runtime_error("local block cell missing from RFCX source");
                cells.push_back(key);
            }
            std::vector<StableOccurrenceKey> occurrences;
            occurrences.reserve(view.occurrence_count());
            for (std::size_t i = 0; i < view.occurrence_count(); ++i) {
                const auto key = view.occurrence(i);
                const auto slot = resolve_stable_occurrence(source, key);
                if (!slot) throw std::runtime_error("local block occurrence missing from RFCX source");
                const auto arena = source.grade(key.upper_grade);
                const auto upper_index = arena.upper_index(*slot);
                const auto lower_index = arena.lower_index(*slot);
                StableCellKey upper{key.upper_grade, arena.relation_key(upper_index).value};
                const auto lower_grade = core::Grade{static_cast<std::uint16_t>(key.upper_grade.value - 1u)};
                const auto lower_arena = source.grade(lower_grade);
                StableCellKey lower{lower_grade, lower_grade.value == 0 ? lower_arena.identity(lower_index)
                                                                        : lower_arena.relation_key(lower_index).value};
                if (!std::binary_search(cells.begin(), cells.end(), upper) ||
                    !std::binary_search(cells.begin(), cells.end(), lower)) {
                    throw std::runtime_error("local block occurrence endpoint missing from block");
                }
                occurrences.push_back(key);
            }
            if (view.kind() == LocalBlockKind::StrictSubcomplex) {
                // Strict blocks persist only their stable cell set. Complete
                // occurrence fibers are recovered from the bound RFCX source.
                if (!occurrences.empty()) throw std::runtime_error("strict mapped local block should not duplicate occurrence refs");
                            std::vector<StableOccurrenceKey> expected;
                for (const auto cell : cells) {
                    if (cell.grade.value == 0) continue;
                    const auto resolved = resolve_stable_cell(source, cell);
                    const auto arena = source.grade(cell.grade);
                    const auto [begin, end] = arena.incidence_range(resolved->index);
                    for (std::size_t p = begin; p < end; ++p) {
                        expected.push_back({cell.grade, arena.occurrence_key(p)});
                    }
                }
                std::sort(expected.begin(), expected.end());
                expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
                occurrences = expected;
                // Boundary closure is checked without materializing a second object graph.
                for (const auto cell : cells) {
                    if (cell.grade.value == 0) continue;
                    const auto resolved = resolve_stable_cell(source, cell);
                    const auto arena = source.grade(cell.grade);
                    const auto [begin, end] = arena.incidence_range(resolved->index);
                    const auto lower_grade = core::Grade{static_cast<std::uint16_t>(cell.grade.value - 1u)};
                    const auto lower_arena = source.grade(lower_grade);
                    for (std::size_t p = begin; p < end; ++p) {
                        const auto li = arena.lower_index(p);
                        StableCellKey lower{lower_grade, lower_grade.value == 0 ? lower_arena.identity(li)
                                                                              : lower_arena.relation_key(li).value};
                        if (!std::binary_search(cells.begin(), cells.end(), lower)) {
                            throw std::runtime_error("strict mapped local block is not boundary closed");
                        }
                    }
                }
            }
            for (std::size_t pi = 0; pi < view.payload_count(); ++pi) {
                const auto p = view.payload(pi);
                LocalBlockDefinition temp{view.key(), view.kind(), cells, occurrences, {}};
                validate_local_payload_binding(temp, p);
            }
        }
    }

private:
    void parse(bool verify_integrity) {
        using namespace local_rfcx_detail;
        const auto bytes = file_.bytes();
        if (bytes.size() < header_size) throw std::runtime_error("RFCX local-block image too small");
        for (std::size_t i = 0; i < magic.size(); ++i) if (bytes[i] != magic[i]) throw std::runtime_error("RFCX local-block magic mismatch");
        rfcx_detail::Reader r(bytes, 8);
        if (r.u32() != version) throw std::runtime_error("unsupported RFCX local-block version");
        if (r.u32() != rfcx_detail::endian_tag) throw std::runtime_error("RFCX local-block endian mismatch");
        const auto declared_size = r.u64();
        block_count_ = r.u64(); cell_ref_count_ = r.u64(); occurrence_ref_count_ = r.u64(); payload_count_ = r.u64();
        block_dir_offset_ = r.u64(); cells_offset_ = r.u64(); occurrences_offset_ = r.u64(); payload_offsets_offset_ = r.u64();
        payload_blob_offset_ = r.u64(); glue_count_ = r.u64(); glue_cells_offset_ = r.u64(); glue_offsets_offset_ = r.u64();
        glue_members_offset_ = r.u64(); payload_hash_ = r.u64(); source_fingerprint_ = r.u64();
        if (declared_size != bytes.size()) throw std::runtime_error("RFCX local-block file size mismatch");
        rfcx_detail::SectionRanges sections(bytes.size(),local_rfcx_detail::header_size);
        sections.add(block_dir_offset_,block_count_,block_descriptor_size);
        sections.add(cells_offset_,cell_ref_count_,stable_ref_size);
        sections.add(occurrences_offset_,occurrence_ref_count_,stable_ref_size);
        if (payload_count_ == std::numeric_limits<std::uint64_t>::max() || glue_count_ == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("RFCX local count overflow");
        sections.add(payload_offsets_offset_,payload_count_+1u,8u);
        sections.add(glue_cells_offset_,glue_count_,stable_ref_size);
        sections.add(glue_offsets_offset_,glue_count_+1u,8u);
        if (u64_at(payload_offsets_offset_) != payload_blob_offset_) throw std::runtime_error("RFCX local payload start");
        std::uint64_t previous=payload_blob_offset_;
        for (std::size_t i=0; i<payload_count_; ++i) {
            const auto end=u64_at(payload_offsets_offset_+(i+1u)*8u);
            if (end<previous) throw std::runtime_error("RFCX local payload offsets");
            sections.add(previous,end-previous);
            previous=end;
        }
        if (!payload_count_) sections.add(payload_blob_offset_,0);
        if (u64_at(glue_offsets_offset_)!=0) throw std::runtime_error("RFCX local glue start");
        const auto members=u64_at(glue_offsets_offset_+glue_count_*8u);
        sections.add(glue_members_offset_,members,8u);
        previous=0;
        for (std::size_t i=0; i<glue_count_; ++i) {
            const auto end=u64_at(glue_offsets_offset_+(i+1u)*8u);
            if (end<previous || end>members || end-previous<2u) throw std::runtime_error("RFCX local glue offsets");
            previous=end;
        }
        sections.finish();
        if (verify_integrity && rfcx_detail::fnv1a64(bytes.subspan(local_rfcx_detail::header_size)) != payload_hash_) {
            throw std::runtime_error("RFCX local-block payload integrity failure");
        }
        validate_index_contents();
    }

    void validate_index_contents() const {
        std::uint64_t cells_end=0, occurrences_end=0, payloads_end=0, last_key=0;
        std::map<StableCellKey,std::vector<std::uint64_t>> expected;
        for (std::size_t i=0; i<block_count_; ++i) {
            const auto d=descriptor(i);
            if ((i && d.block_key<=last_key) || d.cell_begin!=cells_end || d.occurrence_begin!=occurrences_end || d.payload_begin!=payloads_end)
                throw std::runtime_error("RFCX local block ordering/partition");
            if (d.kind==static_cast<std::uint64_t>(LocalBlockKind::StrictSubcomplex) && d.occurrence_count)
                throw std::runtime_error("RFCX strict local block duplicates occurrence references");
            last_key=d.block_key;
            cells_end+=d.cell_count; occurrences_end+=d.occurrence_count; payloads_end+=d.payload_count;
            StableCellKey last_cell{};
            for (std::size_t j=0; j<d.cell_count; ++j) {
                const auto key=cell_ref(d.cell_begin+j);
                if (j && key<=last_cell) throw std::runtime_error("RFCX local cells not strictly sorted");
                expected[key].push_back(d.block_key); last_cell=key;
            }
            StableOccurrenceKey last_occ{};
            for (std::size_t j=0; j<d.occurrence_count; ++j) {
                const auto key=occurrence_ref(d.occurrence_begin+j);
                if (key.upper_grade.value==0 || (j && key<=last_occ)) throw std::runtime_error("RFCX local occurrences not strictly sorted");
                last_occ=key;
            }
            std::uint64_t last_payload=0;
            for (std::size_t j=0; j<d.payload_count; ++j) {
                const auto value=payload(d.payload_begin+j);
                if (j && value.payload_key<=last_payload) throw std::runtime_error("RFCX local payload keys not strictly sorted");
                last_payload=value.payload_key;
            }
        }
        if (cells_end!=cell_ref_count_ || occurrences_end!=occurrence_ref_count_ || payloads_end!=payload_count_)
            throw std::runtime_error("RFCX unused local index references");
        std::size_t index=0;
        for (const auto& [key,blocks] : expected) {
            if (blocks.size()<2u) continue;
            if (index>=glue_count_ || shared_cell(index)!=key || memberships(index)!=blocks)
                throw std::runtime_error("RFCX glue index does not match retained block memberships");
            ++index;
        }
        if (index!=glue_count_) throw std::runtime_error("RFCX spurious glue index entries");
    }

    [[nodiscard]] local_rfcx_detail::BlockDescriptor descriptor(std::size_t index) const {
        if (index >= block_count_) throw std::out_of_range("local block descriptor index");
        const auto base = block_dir_offset_ + static_cast<std::uint64_t>(index) * local_rfcx_detail::block_descriptor_size;
        local_rfcx_detail::BlockDescriptor d;
        d.block_key = u64_at(base + 0u); d.kind = u64_at(base + 8u); d.cell_begin = u64_at(base + 16u); d.cell_count = u64_at(base + 24u);
        d.occurrence_begin = u64_at(base + 32u); d.occurrence_count = u64_at(base + 40u);
        d.payload_begin = u64_at(base + 48u); d.payload_count = u64_at(base + 56u);
        if ((d.kind != static_cast<std::uint64_t>(LocalBlockKind::StrictSubcomplex) &&
             d.kind != static_cast<std::uint64_t>(LocalBlockKind::PartialChart)) ||
            d.cell_begin > cell_ref_count_ || d.cell_count > cell_ref_count_-d.cell_begin ||
            d.occurrence_begin > occurrence_ref_count_ || d.occurrence_count > occurrence_ref_count_-d.occurrence_begin ||
            d.payload_begin > payload_count_ || d.payload_count > payload_count_-d.payload_begin)
            throw std::runtime_error("RFCX local block descriptor kind/range");
        return d;
    }

    [[nodiscard]] StableCellKey cell_ref(std::size_t index) const {
        if (index >= cell_ref_count_) throw std::out_of_range("local cell ref index");
        return read_cell_at(cells_offset_ + static_cast<std::uint64_t>(index) * local_rfcx_detail::stable_ref_size);
    }
    [[nodiscard]] StableOccurrenceKey occurrence_ref(std::size_t index) const {
        if (index >= occurrence_ref_count_) throw std::out_of_range("local occurrence ref index");
        rfcx_detail::Reader r(file_.bytes(), static_cast<std::size_t>(occurrences_offset_ + static_cast<std::uint64_t>(index) * local_rfcx_detail::stable_ref_size));
        return local_rfcx_detail::read_stable_occurrence(r);
    }
    [[nodiscard]] LocalExactTensorPayload payload(std::size_t index) const {
        if (index >= payload_count_) throw std::out_of_range("local payload index");
        const auto begin = u64_at(payload_offsets_offset_ + static_cast<std::uint64_t>(index) * 8u);
        const auto end = u64_at(payload_offsets_offset_ + static_cast<std::uint64_t>(index + 1u) * 8u);
        if (end < begin || end > file_.size()) throw std::runtime_error("local payload record range");
        return local_rfcx_detail::read_payload(file_.bytes().subspan(0, static_cast<std::size_t>(end)), static_cast<std::size_t>(begin));
    }
    [[nodiscard]] StableCellKey read_cell_at(std::uint64_t offset) const {
        require_range(offset, local_rfcx_detail::stable_ref_size);
        rfcx_detail::Reader r(file_.bytes(), static_cast<std::size_t>(offset));
        return local_rfcx_detail::read_stable_cell(r);
    }
    [[nodiscard]] std::uint64_t u64_at(std::uint64_t offset) const {
        require_range(offset, 8u);
        return rfcx_detail::load_u64_at(file_.bytes(), static_cast<std::size_t>(offset));
    }
    void require_range(std::uint64_t offset, std::size_t bytes) const {
        if (offset > file_.size() || bytes > file_.size() - static_cast<std::size_t>(offset)) throw std::runtime_error("RFCX local-block section range");
    }

    ReadOnlyMappedFile file_;
    std::uint64_t block_count_{};
    std::uint64_t cell_ref_count_{};
    std::uint64_t occurrence_ref_count_{};
    std::uint64_t payload_count_{};
    std::uint64_t block_dir_offset_{};
    std::uint64_t cells_offset_{};
    std::uint64_t occurrences_offset_{};
    std::uint64_t payload_offsets_offset_{};
    std::uint64_t payload_blob_offset_{};
    std::uint64_t glue_count_{};
    std::uint64_t glue_cells_offset_{};
    std::uint64_t glue_offsets_offset_{};
    std::uint64_t glue_members_offset_{};
    std::uint64_t payload_hash_{};
    std::uint64_t source_fingerprint_{};
    std::shared_ptr<ViewLifetime> lifetime_{std::make_shared<ViewLifetime>(ViewLifetime{this})};
};

} // namespace rfc::storage
