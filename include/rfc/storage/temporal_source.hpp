#pragma once

#include "rfc/core/builder.hpp"
#include "rfc/storage/local_blocks.hpp"
#include "rfc/temporal/source_delta.hpp"

#include <map>
#include <optional>
#include <set>

namespace rfc::storage {

// Owning source-level records. No lattice coefficient, scale or arena offset
// is used as the meaning of a temporal edit.
struct TemporalOccurrence {
    std::uint64_t key{}, parent{}, lower{};
    numeric::Rational coefficient{0};
    std::vector<std::uint8_t> aspects;
    bool operator==(const TemporalOccurrence&) const = default;
};
struct TemporalCellEdit {
    std::uint64_t key{};
    core::RelationTypeId type{};
    bool birth{};
};
struct TemporalOccurrenceEdit {
    std::uint64_t key{};
    std::optional<std::uint64_t> parent, lower;
    std::optional<numeric::Rational> coefficient;
};
struct TemporalFiberEdit {
    std::uint64_t parent{};
    std::vector<std::uint64_t> order;
};
struct TemporalAspectXorWord {
    composite::AspectId aspect{};
    std::uint64_t word_index{}, bits{};
};
struct TemporalGradePatch {
    core::Grade grade{};
    std::vector<std::uint64_t> removed_cells;
    std::vector<TemporalCellEdit> cells;
    // Arena order is replay metadata, separate from attaching-fiber order.
    std::optional<std::vector<std::uint64_t>> cell_order;
    std::vector<std::uint64_t> removed_occurrences;
    std::vector<TemporalOccurrence> born_occurrences;
    std::vector<TemporalOccurrenceEdit> occurrences;
    std::vector<TemporalFiberEdit> fibers;
    // Alignment: sorted intersection of old and new stable occurrence keys.
    // Only nonzero words are stored, not the whole persistent fiber.
    std::vector<TemporalAspectXorWord> aspect_xor;
    [[nodiscard]] bool empty() const noexcept {
        return removed_cells.empty() && cells.empty() && !cell_order &&
            removed_occurrences.empty() && born_occurrences.empty() &&
            occurrences.empty() && fibers.empty() && aspect_xor.empty();
    }
};
struct TemporalSourcePatch {
    std::uint32_t target_grade_count{1};
    std::vector<TemporalGradePatch> grades;
};

namespace temporal_source_detail {
struct GradeImage {
    std::vector<std::uint64_t> order;
    std::map<std::uint64_t, core::RelationTypeId> cells;
    std::map<std::uint64_t, std::vector<std::uint64_t>> fibers;
    std::map<std::uint64_t, TemporalOccurrence> occurrences;
};
inline GradeImage capture(const core::RelationalComplex& source, std::size_t k) {
    GradeImage out;
    if (k >= source.grade_count()) return out;
    const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
    const auto& arena = source.grade(grade);
    for (std::size_t i = 0; i < arena.cell_count(); ++i) {
        const auto key = k == 0 ? arena.identities[i] : arena.relation_identities[i].value;
        out.order.push_back(key);
        if (!out.cells.emplace(key, k == 0 ? 0u : arena.relation_types[i]).second)
            throw std::invalid_argument("duplicate temporal source cell");
        if (k == 0) continue;
        auto& fiber = out.fibers[key];
        const auto [begin, end] = arena.incidence_range(i);
        for (std::size_t p = begin; p < end; ++p) {
            TemporalOccurrence occ;
            occ.key = arena.occurrence_identities[p].value;
            occ.parent = key;
            const auto& below = source.grade(core::Grade{static_cast<std::uint16_t>(k - 1)});
            occ.lower = k == 1 ? below.identities[arena.lower_index[p]]
                               : below.relation_identities[arena.lower_index[p]].value;
            occ.coefficient = source.incidence_coefficient(grade, p);
            for (std::size_t a = 0; a < source.signature().aspects().size(); ++a)
                occ.aspects.push_back(arena.aspects.get(static_cast<composite::AspectId>(a), p) ? 1u : 0u);
            fiber.push_back(occ.key);
            if (!out.occurrences.emplace(occ.key, std::move(occ)).second)
                throw std::invalid_argument("duplicate temporal source occurrence");
        }
    }
    return out;
}
inline void require(bool value, const char* message) {
    if (!value) throw std::invalid_argument(message);
}
template <class K> inline void unique(std::set<K>& seen, const K& key) {
    require(seen.insert(key).second, "duplicate or conflicting temporal source record");
}
inline void validate_image(const std::vector<GradeImage>& images, std::size_t aspects) {
    std::set<std::uint64_t> global_occurrences;
    for (std::size_t k = 0; k < images.size(); ++k) {
        const auto& g = images[k];
        require(g.cells.size() <= std::numeric_limits<std::uint32_t>::max(), "too many source cells");
        std::set<std::uint64_t> cell_keys, used;
        for (const auto key : g.order) {
            unique(cell_keys, key);
            require(g.cells.contains(key), "temporal cell order contains unknown key");
        }
        require(cell_keys.size() == g.cells.size(), "temporal cell order omits cells");
        if (k == 0) {
            require(g.fibers.empty() && g.occurrences.empty(), "grade zero cannot contain incidences");
            for (const auto& [key, type] : g.cells) { (void)key; require(type == 0, "grade zero type must be zero"); }
            continue;
        }
        require(g.fibers.size() == g.cells.size(), "missing or extra temporal attaching fiber");
        for (const auto& [parent, fiber] : g.fibers) {
            require(g.cells.contains(parent), "attaching fiber has missing parent");
            for (const auto key : fiber) {
                unique(used, key); unique(global_occurrences, key);
                const auto it = g.occurrences.find(key);
                require(it != g.occurrences.end(), "attaching fiber references unknown occurrence");
                const auto& o = it->second;
                require(o.key == key && o.parent == parent, "occurrence parent/fiber mismatch");
                require(images[k - 1].cells.contains(o.lower), "occurrence has missing lower participant");
                require(o.aspects.size() == aspects, "occurrence aspect count mismatch");
                for (const auto bit : o.aspects) require(bit <= 1, "invalid aspect bit");
            }
        }
        require(used.size() == g.occurrences.size(), "unattached retained occurrence");
    }
}
} // namespace temporal_source_detail

inline TemporalSourcePatch make_temporal_source_patch(const core::RelationalComplex& old_source,
                                                       const core::RelationalComplex& new_source) {
    using namespace temporal_source_detail;
    require(temporal::detail::same_signature_for_temporal_alignment(old_source.signature(), new_source.signature()),
            "temporal v1 rejects signature migration");
    require(old_source.semantic_scale() == new_source.semantic_scale(), "temporal v1 rejects source semantic-scale migration");
    TemporalSourcePatch out;
    out.target_grade_count = static_cast<std::uint32_t>(new_source.grade_count());
    for (std::size_t k = 0; k < std::max(old_source.grade_count(), new_source.grade_count()); ++k) {
        const auto old = capture(old_source, k), next = capture(new_source, k);
        TemporalGradePatch p;
        p.grade = core::Grade{static_cast<std::uint16_t>(k)};
        for (const auto& [key, type] : old.cells) {
            (void)type;
            if (!next.cells.contains(key)) p.removed_cells.push_back(key);
        }
        for (const auto& [key, type] : next.cells) {
            const auto it = old.cells.find(key);
            if (it == old.cells.end() || it->second != type) p.cells.push_back({key, type, it == old.cells.end()});
        }
        if (old.order != next.order) p.cell_order = next.order;
        for (const auto& [key, occ] : old.occurrences) {
            (void)occ;
            if (!next.occurrences.contains(key)) p.removed_occurrences.push_back(key);
        }
        std::vector<std::uint64_t> persistent;
        for (const auto& [key, occ] : next.occurrences) {
            const auto it = old.occurrences.find(key);
            if (it == old.occurrences.end()) { p.born_occurrences.push_back(occ); continue; }
            persistent.push_back(key);
            TemporalOccurrenceEdit edit;
            edit.key = key;
            if (it->second.parent != occ.parent) edit.parent = occ.parent;
            if (it->second.lower != occ.lower) edit.lower = occ.lower;
            if (it->second.coefficient != occ.coefficient) edit.coefficient = occ.coefficient;
            if (edit.parent || edit.lower || edit.coefficient) p.occurrences.push_back(std::move(edit));
        }
        for (const auto& [parent, fiber] : next.fibers) {
            const auto it = old.fibers.find(parent);
            if (it == old.fibers.end() || it->second != fiber) p.fibers.push_back({parent, fiber});
        }
        for (std::size_t a = 0; a < old_source.signature().aspects().size(); ++a) {
            for (std::size_t begin = 0; begin < persistent.size(); begin += 64) {
                std::uint64_t bits = 0;
                for (std::size_t bit = 0; bit < 64 && begin + bit < persistent.size(); ++bit) {
                    const auto key = persistent[begin + bit];
                    if (old.occurrences.at(key).aspects[a] != next.occurrences.at(key).aspects[a]) bits |= std::uint64_t{1} << bit;
                }
                if (bits) p.aspect_xor.push_back({static_cast<composite::AspectId>(a), begin / 64, bits});
            }
        }
        if (!p.empty()) out.grades.push_back(std::move(p));
    }
    return out;
}


namespace temporal_source_detail {
template<class Scale>
inline core::RelationalComplex materialize_images(const std::vector<GradeImage>& images,
                                                  const core::RelationSignature& signature, Scale scale) {
    require(!images.empty() && images.size() <= 65536u, "invalid source image grade count");
    validate_image(images, signature.aspects().size());
    core::RelationalComplexBuilder builder(signature, scale);
    builder.declare_grade(core::Grade{static_cast<std::uint16_t>(images.size() - 1)});
    for (const auto key : images[0].order) builder.declare_witness_identity(key);
    const auto existence = signature.existence_aspect(), orientation = signature.orientation_aspect();
    for (std::size_t k = 1; k < images.size(); ++k) {
        std::map<std::uint64_t, std::uint32_t> lower_index;
        for (std::size_t i = 0; i < images[k - 1].order.size(); ++i)
            lower_index.emplace(images[k - 1].order[i], static_cast<std::uint32_t>(i));
        for (const auto key : images[k].order) {
            std::vector<core::BoundaryOccurrenceInput> first;
            std::vector<core::HigherIncidenceInput> higher;
            for (const auto occ_key : images[k].fibers.at(key)) {
                const auto& o = images[k].occurrences.at(occ_key);
                const bool exists = o.aspects.at(existence) != 0, negative = o.aspects.at(orientation) != 0;
                require(exists || o.coefficient == 0, "absent occurrence has nonzero coefficient");
                require(o.coefficient == 0 || (o.coefficient < 0) == negative, "coefficient/orientation mismatch");
                numeric::Rational magnitude = o.coefficient < 0 ? -o.coefficient : o.coefficient;
                std::vector<core::AspectValue> aspects;
                for (std::size_t a = 0; a < o.aspects.size(); ++a)
                    if (a != existence && a != orientation) aspects.push_back({static_cast<composite::AspectId>(a), o.aspects[a] != 0});
                if (k == 1) first.emplace_back(o.lower, exists, negative, magnitude, aspects, core::OccurrenceKey{o.key});
                else higher.emplace_back(lower_index.at(o.lower), exists, negative, magnitude, aspects, core::OccurrenceKey{o.key});
            }
            const auto type = images[k].cells.at(key);
            if (k == 1) builder.add_grade_one(core::RelationKey{key}, type, first);
            else builder.add_higher(core::Grade{static_cast<std::uint16_t>(k)}, core::RelationKey{key}, type, higher);
        }
    }
    return builder.freeze(); // Includes exact constructor and all adjacent chain validation.
}
} // namespace temporal_source_detail

inline core::RelationalComplex replay_temporal_source(const core::RelationalComplex& source,
                                                       const TemporalSourcePatch& patch) {
    using namespace temporal_source_detail;
    require(patch.target_grade_count > 0 && patch.target_grade_count <= 65536u, "invalid temporal grade count");
    const auto count = std::max(source.grade_count(), static_cast<std::size_t>(patch.target_grade_count));
    std::vector<GradeImage> images;
    for (std::size_t k = 0; k < count; ++k) images.push_back(capture(source, k));
    std::set<std::uint16_t> grades_seen;
    for (const auto& p : patch.grades) {
        unique(grades_seen, p.grade.value);
        require(p.grade.value < images.size(), "temporal patch grade outside endpoint");
        auto& g = images[p.grade.value];
        const auto original_occurrences = g.occurrences;
        std::set<std::uint64_t> cells_seen, occurrences_seen, fibers_seen;
        for (const auto key : p.removed_cells) {
            unique(cells_seen, key);
            require(g.cells.erase(key) == 1, "removal of missing source cell");
            g.fibers.erase(key);
        }
        for (const auto& edit : p.cells) {
            unique(cells_seen, edit.key);
            require(g.cells.contains(edit.key) != edit.birth, "cell birth/update precondition failed");
            g.cells[edit.key] = edit.type;
        }
        if (p.cell_order) g.order = *p.cell_order;
        for (const auto key : p.removed_occurrences) {
            unique(occurrences_seen, key);
            require(g.occurrences.erase(key) == 1, "removal of missing occurrence");
        }
        for (const auto& occ : p.born_occurrences) {
            unique(occurrences_seen, occ.key);
            require(g.occurrences.emplace(occ.key, occ).second, "birth of existing occurrence");
        }
        for (const auto& edit : p.occurrences) {
            unique(occurrences_seen, edit.key);
            require(original_occurrences.contains(edit.key) && g.occurrences.contains(edit.key), "update of nonpersistent occurrence");
            require(edit.parent || edit.lower || edit.coefficient, "empty occurrence update");
            auto& occ = g.occurrences.at(edit.key);
            if (edit.parent) occ.parent = *edit.parent;
            if (edit.lower) occ.lower = *edit.lower;
            if (edit.coefficient) occ.coefficient = *edit.coefficient;
        }
        std::vector<std::uint64_t> persistent;
        for (const auto& [key, occ] : g.occurrences) {
            (void)occ;
            if (original_occurrences.contains(key)) persistent.push_back(key);
        }
        std::set<std::pair<composite::AspectId, std::uint64_t>> words_seen;
        for (const auto& word : p.aspect_xor) {
            unique(words_seen, std::pair{word.aspect, word.word_index});
            require(word.aspect < source.signature().aspects().size() && word.bits != 0, "invalid aspect XOR word");
            require(word.word_index < (persistent.size() + 63u) / 64u, "aspect XOR word outside alignment");
            const auto begin = static_cast<std::size_t>(word.word_index) * 64u;
            const auto available = std::min<std::size_t>(64, persistent.size() - begin);
            require(available == 64 || (word.bits >> available) == 0, "nonzero aspect XOR padding");
            for (std::size_t bit = 0; bit < available; ++bit)
                if ((word.bits >> bit) & 1u) g.occurrences.at(persistent[begin + bit]).aspects.at(word.aspect) ^= 1u;
        }
        for (const auto& fiber : p.fibers) {
            unique(fibers_seen, fiber.parent);
            require(g.cells.contains(fiber.parent), "fiber update has missing cell");
            g.fibers[fiber.parent] = fiber.order;
        }
    }
    for (std::size_t k = patch.target_grade_count; k < images.size(); ++k)
        require(images[k].cells.empty() && images[k].occurrences.empty() && images[k].order.empty() && images[k].fibers.empty(),
                "grade removal would discard retained source data");
    images.resize(patch.target_grade_count);
    return materialize_images(images, source.signature(), source.semantic_scale());
}

} // namespace rfc::storage
