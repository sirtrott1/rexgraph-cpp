#pragma once

#include "rfc/storage/temporal_finite_types.hpp"
#include "rfc/storage/relational_code_types.hpp"
#include "rfc/field/exact_metric.hpp"
#include "rfc/storage/declared_code_binding.hpp"
#include <algorithm>

namespace rfc::storage {

enum class TemporalCarrierKind : std::uint8_t { Explicit, SourceGrade, CellStalk };
struct TemporalCarrier {
    std::uint64_t key{};
    TemporalCarrierKind kind{TemporalCarrierKind::Explicit};
    core::Grade grade{};
    std::optional<StableCellKey> cell;
    // Nonzero for explicitly declared coordinates. Equal dimensions alone do
    // not authorize identity; a changed coordinate system requires an explicit J.
    std::uint64_t coordinate_system{};
    numeric::IndexDomain domain;
    bool operator==(const TemporalCarrier&) const = default;
};

enum class TemporalTensorRole : std::uint8_t {
    Field, Metric, CrossForm, Accession, Reconstruction, PhysicalRealization,
    SourceRealization, Observation, SemanticScaleMap, RealizedIncidenceMap,
    CoefficientIndependentMap, SheafRestriction, IncidenceMagnitude, OpaqueExact
};
enum class TemporalOwnerKind : std::uint8_t { Global, Block, Cell, Occurrence };
struct TemporalTensorOwner {
    TemporalOwnerKind kind{TemporalOwnerKind::Global};
    std::uint64_t block{};
    StableCellKey cell{};
    StableOccurrenceKey occurrence{};
    bool operator==(const TemporalTensorOwner&) const = default;
};
struct TemporalTensor {
    std::uint64_t key{};
    TemporalTensorRole role{TemporalTensorRole::OpaqueExact};
    TemporalTensorOwner owner;
    // Field uses output only. Maps use input/output; forms use column/row
    // carriers. IncidenceMagnitude and OpaqueExact use neither.
    std::optional<std::uint64_t> input, output;
    std::string interpretation;
    numeric::SparseRationalTensor tensor = numeric::SparseRationalTensor::zero(
        numeric::IndexDomain{}, numeric::StructuralSupport::full(1));
};
struct TemporalEndpoint {
    explicit TemporalEndpoint(core::RelationalComplex retained, std::uint64_t sequence_number = 0)
        : source(std::move(retained)), sequence(sequence_number) {}
    core::RelationalComplex source;
    std::uint64_t sequence{};
    std::vector<LocalBlockDefinition> local_blocks;
    std::vector<TemporalCarrier> carriers;
    std::vector<TemporalTensor> tensors;
    std::vector<TemporalFiniteSystem> finite_systems;
    std::optional<RelationalCodeState> codes;
    std::optional<TemporalDeclarationState> declarations;
};
struct TemporalCorrespondence {
    std::uint64_t carrier{};
    numeric::ExactMatrix map;
};

namespace temporal_endpoint_detail {
using temporal_source_detail::require;
using temporal_source_detail::unique;

template <class T, class Key> inline std::map<std::uint64_t, const T*> index(const std::vector<T>& items, Key key) {
    std::map<std::uint64_t, const T*> out;
    for (const auto& item : items) require(out.emplace(key(item), &item).second, "duplicate temporal endpoint key");
    return out;
}
inline auto carriers(const TemporalEndpoint& e) { return index(e.carriers, [](const auto& x) { return x.key; }); }
inline auto tensors(const TemporalEndpoint& e) { return index(e.tensors, [](const auto& x) { return x.key; }); }
inline bool support_equal(const numeric::StructuralSupport& a, const numeric::StructuralSupport& b) {
    if (a.size() != b.size() || a.count() != b.count()) return false;
    if (a.count() == a.size()) return true;
    bool same = true;
    a.for_each_active([&](std::size_t i) { if (!b.contains(i)) same = false; });
    return same;
}
inline bool values_equal(const numeric::SparseRationalVector& a, const numeric::SparseRationalVector& b) {
    if (a.size() != b.size() || a.nonzeros() != b.nonzeros()) return false;
    if (a.retained_source() && a.retained_source() == b.retained_source()) return true;
    auto cursor = b.retained_source() ? b.retained_source()->open() : nullptr;
    bool same = true;
    std::size_t offset = 0;
    numeric::SparseRationalEntry entry;
    a.for_each_nonzero([&](std::size_t index, const numeric::Rational& value) {
        if (cursor) {
            if (!cursor->next(entry) || entry.index != index || entry.value != value) same = false;
        } else {
            const auto& original = b.entries()[offset++];
            if (original.index != index || original.value != value) same = false;
        }
    });
    return same;
}
inline bool tensor_equal(const numeric::SparseRationalTensor& a, const numeric::SparseRationalTensor& b) {
    return a.domain() == b.domain() && support_equal(a.support(), b.support()) && values_equal(a.values(), b.values());
}
inline bool metadata_equal(const TemporalTensor& a, const TemporalTensor& b) {
    return a.key == b.key && a.role == b.role && a.owner == b.owner && a.input == b.input &&
        a.output == b.output && a.interpretation == b.interpretation && a.tensor.domain() == b.tensor.domain() &&
        support_equal(a.tensor.support(), b.tensor.support());
}
inline bool tensor_equal(const TemporalTensor& a, const TemporalTensor& b) {
    return metadata_equal(a, b) && values_equal(a.tensor.values(), b.tensor.values());
}
inline numeric::SparseRationalVector sparse_apply(const numeric::ExactMatrix& J,
                                                 const numeric::SparseRationalVector& x) {
    require(J.cols() == x.size(), "sparse temporal correspondence input size");
    return J.apply_sparse(x.view());
}
inline numeric::SparseRationalVector sparse_sum(const numeric::SparseRationalVector& a,
                                               const numeric::SparseRationalVector& b, int sign = 1) {
    require(a.size() == b.size(), "sparse temporal sum size");
    std::vector<numeric::SparseRationalEntry> entries(a.entries().begin(), a.entries().end());
    for (const auto& v : b.entries()) entries.push_back({v.index, v.value * sign});
    return numeric::SparseRationalVector(a.size(), std::move(entries));
}
inline LocalBlockDefinition normalize_local(const core::RelationalComplex& source, const StableSourceIndex& source_index, LocalBlockDefinition b) {
    require(b.kind == LocalBlockKind::StrictSubcomplex || b.kind == LocalBlockKind::PartialChart, "unknown local block kind");
    canonicalize_local_block(b);
    if (b.kind == LocalBlockKind::StrictSubcomplex) {
        // Strict fibers are implied by the *target* source, never carried over
        // as stale old-source occurrence selections.
        b.occurrences.clear();
        for (const auto key : b.cells) {
            const auto cell = source_index.cell(key);
            require(cell.has_value(), "local block cell missing from endpoint");
            if (key.grade.value == 0) continue;
            const auto& arena = source.grade(key.grade);
            const auto [begin, end] = arena.incidence_range(cell->index);
            for (std::size_t p = begin; p < end; ++p)
                b.occurrences.push_back({key.grade, arena.occurrence_identities[p]});
        }
        canonicalize_local_block(b);
    }
    for (const auto& payload : b.payloads) {
        require(static_cast<std::uint8_t>(payload.binding) <= static_cast<std::uint8_t>(LocalPayloadBinding::Occurrence),
                "unknown local payload binding");
        require((payload.binding == LocalPayloadBinding::Cell) == payload.cell.has_value(), "invalid local payload cell binding");
        require((payload.binding == LocalPayloadBinding::Occurrence) == payload.occurrence.has_value(), "invalid local payload occurrence binding");
    }
    validate_local_block_against_source(source, source_index, b);
    return b;
}
inline LocalBlockDefinition normalize_local(const core::RelationalComplex& source, LocalBlockDefinition b) {
    const StableSourceIndex source_index(source);
    return normalize_local(source, source_index, std::move(b));
}
inline bool local_equal(const LocalBlockDefinition& a, const LocalBlockDefinition& b) {
    if (a.block_key != b.block_key || a.kind != b.kind || a.cells != b.cells ||
        (a.kind == LocalBlockKind::PartialChart && a.occurrences != b.occurrences) || a.payloads.size() != b.payloads.size()) return false;
    for (std::size_t i = 0; i < a.payloads.size(); ++i) {
        const auto& x = a.payloads[i]; const auto& y = b.payloads[i];
        if (x.payload_key != y.payload_key || x.binding != y.binding || x.cell != y.cell ||
            x.occurrence != y.occurrence || !tensor_equal(x.tensor, y.tensor)) return false;
    }
    return true;
}
} // namespace temporal_endpoint_detail

inline numeric::ExactMatrix temporal_tensor_matrix(const TemporalTensor& tensor) {
    const auto& shape = tensor.tensor.domain().extents();
    if (shape.size() != 2) throw std::invalid_argument("temporal map/form must have rank two");
    numeric::ExactMatrix out(shape[0], shape[1]);
    tensor.tensor.values().for_each_nonzero([&](std::size_t index, const numeric::Rational& value) {
        out.set(index / shape[1], index % shape[1], value);
    });
    return out;
}
inline numeric::SparseRationalTensor temporal_matrix_tensor(const numeric::ExactMatrix& matrix) {
    numeric::IndexDomain domain({matrix.rows(), matrix.cols()});
    std::vector<numeric::SparseRationalEntry> entries;
    for (const auto& [ij, value] : matrix.entries()) entries.push_back({ij.first * matrix.cols() + ij.second, value});
    const auto size = domain.logical_size();
    return numeric::SparseRationalTensor(std::move(domain), numeric::StructuralSupport::full(size),
                                        numeric::SparseRationalVector(size, std::move(entries)));
}

namespace temporal_endpoint_detail {
inline void validate(const TemporalEndpoint& endpoint, const StableSourceIndex& source_index) {
    using namespace temporal_endpoint_detail;
    std::set<std::uint64_t> blocks;
    for (const auto& b : endpoint.local_blocks) {
        unique(blocks, b.block_key);
        (void)normalize_local(endpoint.source, source_index, b);
    }
    const auto carrier_index = carriers(endpoint);
    for (const auto& [key, c] : carrier_index) {
        (void)key;
        require(static_cast<std::uint8_t>(c->kind) <= static_cast<std::uint8_t>(TemporalCarrierKind::CellStalk), "unknown carrier kind");
        if (c->kind == TemporalCarrierKind::SourceGrade) {
            require(!c->cell && c->coordinate_system == 0, "source-grade carrier has extraneous coordinate declaration");
            require(c->grade.value < endpoint.source.grade_count(), "source-grade carrier grade absent");
            require(c->domain == numeric::IndexDomain::linear(endpoint.source.grade(c->grade).cell_count()), "source-grade carrier dimension mismatch");
        } else {
            require(c->coordinate_system != 0, "explicit/stalk carrier must declare coordinate system identity");
            if (c->kind == TemporalCarrierKind::CellStalk) {
                require(c->cell && source_index.cell(*c->cell), "stalk carrier cell missing");
                require(c->grade == c->cell->grade, "stalk carrier grade mismatch");
            } else require(!c->cell && c->grade.value == 0, "explicit carrier has source metadata");
        }
    }
    const auto tensor_index = tensors(endpoint);
    std::set<StableOccurrenceKey> down_maps, up_maps, magnitudes;
    for (const auto& [key, t] : tensor_index) {
        (void)key;
        require(static_cast<std::uint8_t>(t->role) <= static_cast<std::uint8_t>(TemporalTensorRole::OpaqueExact), "unsupported temporal tensor role");
        require(static_cast<std::uint8_t>(t->owner.kind) <= static_cast<std::uint8_t>(TemporalOwnerKind::Occurrence), "unknown tensor owner");
        if (t->owner.kind == TemporalOwnerKind::Block) require(blocks.contains(t->owner.block), "tensor block owner absent");
        if (t->owner.kind == TemporalOwnerKind::Cell) require(source_index.cell(t->owner.cell).has_value(), "tensor cell owner absent");
        if (t->owner.kind == TemporalOwnerKind::Occurrence) require(source_index.occurrence(t->owner.occurrence).has_value(), "tensor occurrence owner absent");
        require(t->owner.kind == TemporalOwnerKind::Block || t->owner.block == 0, "extraneous block owner key");
        require(t->owner.kind == TemporalOwnerKind::Cell || t->owner.cell == StableCellKey{}, "extraneous cell owner key");
        require(t->owner.kind == TemporalOwnerKind::Occurrence || t->owner.occurrence == StableOccurrenceKey{}, "extraneous occurrence owner key");
        auto carrier = [&](const std::optional<std::uint64_t>& id) -> const TemporalCarrier& {
            require(id && carrier_index.contains(*id), "tensor has missing carrier"); return *carrier_index.at(*id);
        };
        if (t->role == TemporalTensorRole::OpaqueExact) {
            require(!t->input && !t->output && !t->interpretation.empty(), "opaque exact tensor needs interpretation, not inferred carriers");
            continue;
        }
        if (t->role == TemporalTensorRole::IncidenceMagnitude) {
            require(!t->input && !t->output && t->owner.kind == TemporalOwnerKind::Occurrence && t->tensor.domain().scalar(), "invalid incidence magnitude declaration");
            unique(magnitudes, t->owner.occurrence);
            const auto state = t->tensor.state(0);
            require(state && *state >= 0, "incidence magnitude must be supported and nonnegative");
            const auto slot = source_index.occurrence(t->owner.occurrence).value();
            const auto grade = t->owner.occurrence.upper_grade;
            const auto& arena = endpoint.source.grade(grade);
            if (arena.aspects.get(endpoint.source.signature().existence_aspect(), slot)) {
                numeric::Rational c = *state;
                if (arena.aspects.get(endpoint.source.signature().orientation_aspect(), slot)) c = -c;
                require(c == endpoint.source.incidence_coefficient(grade, slot), "declared magnitude disagrees with source coefficient");
            }
            continue;
        }
        if (t->role == TemporalTensorRole::Field) {
            require(!t->input && t->tensor.domain() == carrier(t->output).domain, "field tensor carrier/domain mismatch");
            continue;
        }
        const auto& in = carrier(t->input); const auto& out = carrier(t->output);
        require(t->tensor.domain() == numeric::IndexDomain({out.domain.logical_size(), in.domain.logical_size()}), "map/form dimensions disagree with declared carriers");
        if (t->role == TemporalTensorRole::Metric) {
            require(t->input == t->output, "metric carriers differ");
            const auto size = out.domain.logical_size();
            bool diagonal = true, positive = true;
            t->tensor.values().for_each_nonzero([&](std::size_t index, const numeric::Rational& value) {
                diagonal = diagonal && size && index / size == index % size;
                positive = positive && value > 0;
            });
            if (diagonal) {
                // A complete positive diagonal certifies the stored form.
                require(t->tensor.values().nonzeros() == size && positive, "metric must be positive definite");
            } else {
                (void)field::ExactMetric(temporal_tensor_matrix(*t));
            }
        }
        if (t->role == TemporalTensorRole::RealizedIncidenceMap || t->role == TemporalTensorRole::CoefficientIndependentMap ||
            t->role == TemporalTensorRole::SheafRestriction) {
            require(t->owner.kind == TemporalOwnerKind::Occurrence, "incidence tensor lacks occurrence owner");
            if (t->role == TemporalTensorRole::SheafRestriction) unique(up_maps, t->owner.occurrence);
            else unique(down_maps, t->owner.occurrence);
            const auto slot = source_index.occurrence(t->owner.occurrence).value();
            const auto grade = t->owner.occurrence.upper_grade;
            const auto& arena = endpoint.source.grade(grade);
            const auto upper = stable_cell_key(endpoint.source, {grade, arena.upper_index[slot]});
            const auto lower = stable_cell_key(endpoint.source, {core::Grade{static_cast<std::uint16_t>(grade.value - 1u)}, arena.lower_index[slot]});
            require(in.kind == TemporalCarrierKind::CellStalk && out.kind == TemporalCarrierKind::CellStalk, "local map carriers must be declared cell stalks");
            const bool upward = t->role == TemporalTensorRole::SheafRestriction;
            require(in.cell == (upward ? lower : upper) && out.cell == (upward ? upper : lower), "local map direction/owners mismatch");
        }
    }
    if (endpoint.codes) {
        validate_relational_code_state(*endpoint.codes);
        const auto code_blocks = index(endpoint.local_blocks, [](const auto& b) { return b.block_key; });
        const auto code_types = relational_code_detail::index(endpoint.codes->types);
        for (const auto& section : endpoint.codes->sections) {
            if (section.cell) require(source_index.cell(*section.cell).has_value(), "code section cell absent");
            if (section.occurrence) require(source_index.occurrence(*section.occurrence).has_value(), "code section occurrence absent");
            if (section.source_aspects) {
                for (const auto& factor : code_types.at(section.type)->factors)
                    require(endpoint.source.signature().aspects().find_id(factor.source_aspect).has_value(), "source-view aspect absent");
            }
            if (section.block != 0) {
                require(code_blocks.contains(section.block), "code section block absent");
                const auto block = normalize_local(endpoint.source, source_index, *code_blocks.at(section.block));
                if (section.cell) require(std::find(block.cells.begin(), block.cells.end(), *section.cell) != block.cells.end(), "code cell outside local block");
                if (section.occurrence) require(std::find(block.occurrences.begin(), block.occurrences.end(), *section.occurrence) != block.occurrences.end(), "code occurrence outside local block");
            }
        }
    }
    if (endpoint.declarations) {
        temporal_declaration_detail::validate_against_source(
            endpoint.source, source_index, endpoint.local_blocks, *endpoint.declarations);
        validate_declared_code_bindings(endpoint.codes, *endpoint.declarations);
    }
    std::set<std::uint64_t> finite_keys;
    for (const auto& system : endpoint.finite_systems) {
        unique(finite_keys, system.key);
        validate_temporal_finite_system(endpoint.source, source_index, system);
    }
}

} // namespace temporal_endpoint_detail

inline void validate_temporal_endpoint(const TemporalEndpoint& endpoint) {
    const StableSourceIndex source_index(endpoint.source);
    temporal_endpoint_detail::validate(endpoint, source_index);
}

namespace temporal_endpoint_detail {
// Ephemeral execution index. It borrows immutable endpoint declarations and
// explicit maps for one operation; no cache becomes part of source semantics.
class CarrierCorrespondenceResolver {
public:
    CarrierCorrespondenceResolver(const TemporalEndpoint& old_endpoint,
                                  const TemporalEndpoint& new_endpoint,
                                  const std::vector<TemporalCorrespondence>& explicit_maps)
        : old_(old_endpoint), next_(new_endpoint), old_index_(carriers(old_)), next_index_(carriers(next_)) {
        for (const auto& c : explicit_maps) {
            require(next_index_.contains(c.carrier), "explicit correspondence has no target carrier");
            require(explicit_maps_.emplace(c.carrier, &c.map).second, "duplicate explicit correspondence");
        }
    }
    [[nodiscard]] const numeric::ExactMatrix& get(std::uint64_t key) {
        if (const auto it = cache_.find(key); it != cache_.end()) return it->second.map;
        require(next_index_.contains(key), "correspondence target carrier absent");
        const auto& next = *next_index_.at(key);
        const auto rows = next.domain.logical_size();
        const auto cols = old_index_.contains(key) ? old_index_.at(key)->domain.logical_size() : 0;
        numeric::ExactMatrix result(rows, cols);
        if (const auto map = explicit_maps_.find(key); map != explicit_maps_.end()) {
            require(map->second->rows() == rows && map->second->cols() == cols,
                    "rectangular correspondence dimension mismatch");
            result = *map->second;
        } else if (old_index_.contains(key)) {
            const auto& old = *old_index_.at(key);
            if (old.kind == TemporalCarrierKind::SourceGrade && next.kind == old.kind && old.grade == next.grade) {
                require(temporal::detail::same_signature_for_temporal_alignment(old_.source.signature(), next_.source.signature()),
                        "stable-source correspondence requires the same signature");
                require(old.grade.value < old_.source.grade_count() && next.grade.value < next_.source.grade_count(),
                        "source correspondence grade absent");
                require(cols == old_.source.grade(old.grade).cell_count() && rows == next_.source.grade(next.grade).cell_count(),
                        "source correspondence carrier dimensions disagree");
                // Only the retained cells are needed for J, not a repeated
                // construction of all occurrence/aspect source defects.
                if (!old_source_index_) old_source_index_.emplace(old_.source);
                const auto& arena = next_.source.grade(next.grade);
                for (std::size_t row = 0; row < rows; ++row) {
                    // Read the declared arena identity directly. Besides
                    // avoiding redundant resolution, this avoids a
                    // padding-sensitive narrow-grade/32-bit-index aggregate
                    // argument in optimized Clang's loop code generation.
                    const auto identity = next.grade.value == 0 ? arena.identities[row]
                                                               : arena.relation_identities[row].value;
                    const StableCellKey target{next.grade, identity};
                    if (const auto from = old_source_index_->cell(target)) result.set(row, from->index, numeric::Rational{1});
                }
            } else {
                require(old == next, "changed tensor coordinates/dimensions require an explicit rectangular correspondence");
                result = numeric::ExactMatrix::identity(rows);
            }
        }
        const auto identity = result.is_identity();
        return cache_.emplace(key, CachedMap{std::move(result), identity}).first->second.map;
    }
    [[nodiscard]] bool is_identity(std::uint64_t key) {
        (void)get(key); return cache_.at(key).identity;
    }
    void validate_all() {
        for (const auto& [key, carrier] : next_index_) { (void)carrier; (void)get(key); }
    }
private:
    const TemporalEndpoint& old_;
    const TemporalEndpoint& next_;
    std::map<std::uint64_t, const TemporalCarrier*> old_index_, next_index_;
    std::map<std::uint64_t, const numeric::ExactMatrix*> explicit_maps_;
    std::optional<StableSourceIndex> old_source_index_;
    struct CachedMap { numeric::ExactMatrix map; bool identity; };
    std::map<std::uint64_t, CachedMap> cache_;
};
} // namespace temporal_endpoint_detail

inline numeric::ExactMatrix temporal_carrier_correspondence(const TemporalEndpoint& old_endpoint,
                                                            const TemporalEndpoint& new_endpoint,
                                                            std::uint64_t key,
                                                            const std::vector<TemporalCorrespondence>& explicit_maps = {}) {
    temporal_endpoint_detail::CarrierCorrespondenceResolver resolver(old_endpoint, new_endpoint, explicit_maps);
    return resolver.get(key);
}

inline numeric::ExactMatrix temporal_realized_incidence(const TemporalEndpoint& endpoint, const TemporalTensor& tensor) {
    using temporal_source_detail::require;
    require(tensor.role == TemporalTensorRole::RealizedIncidenceMap || tensor.role == TemporalTensorRole::CoefficientIndependentMap,
            "not a downward incidence tensor");
    auto map = temporal_tensor_matrix(tensor);
    if (tensor.role == TemporalTensorRole::CoefficientIndependentMap) {
        const auto slot = resolve_stable_occurrence(endpoint.source, tensor.owner.occurrence);
        require(slot.has_value(), "local tensor occurrence missing");
        map = map.scale(endpoint.source.incidence_coefficient(tensor.owner.occurrence.upper_grade, *slot));
    }
    return map; // A realized override is deliberately NOT multiplied a second time.
}

} // namespace rfc::storage
