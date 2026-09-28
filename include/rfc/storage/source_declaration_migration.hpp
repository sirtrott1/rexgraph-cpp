#pragma once

#include "rfc/storage/temporal_source.hpp"

namespace rfc::storage {

// Names are scoped to the complete, owning endpoint signatures below. A local
// numeric aspect/type position is never interpreted without that binding.
struct SourceDeclarationNameMap {
    std::string from, to;
    bool operator==(const SourceDeclarationNameMap&) const = default;
};
struct SourceAspectAssignment {
    StableOccurrenceKey occurrence;
    bool value{};
    bool operator==(const SourceAspectAssignment&) const = default;
};
struct SourceAspectIntroduction {
    std::string target;
    // An explicit migration fill, NOT the target declaration's builder default.
    // With no fill, all retained base occurrences require individual assignments.
    std::optional<bool> fill;
    std::vector<SourceAspectAssignment> assignments;
};
// Execution policy, not persisted declaration meaning. This bounds retained
// occurrences and accounted aspect buffers before capture/recoding allocates them.
// It is not an upper bound on all maps, GMP allocations or process RSS.
struct SourceDeclarationLimits {
    std::size_t max_occurrences{1000000u};
    std::size_t max_aspect_bytes{64u * 1024u * 1024u};
};

struct SourceDeclarationMigration {
    core::RelationSignature base_signature, target_signature;
    std::vector<SourceDeclarationNameMap> aspects, types;
    std::vector<SourceAspectIntroduction> introduced;
};

namespace source_declaration_detail {
using temporal_source_detail::require;
inline std::vector<std::byte> signature_bytes(const core::RelationSignature& sig) {
    rfcx_detail::Writer w; rfcx_detail::write_signature(w, sig); return w.bytes();
}
struct ResolvedMigration {
    std::vector<composite::AspectId> aspects;
    std::vector<core::RelationTypeId> types;
};
inline void check_source_allocation(const core::RelationalComplex& source,
                                    const SourceDeclarationMigration& m,
                                    const SourceDeclarationLimits& limits) {
    require(limits.max_occurrences && limits.max_aspect_bytes, "zero source declaration budget");
    std::size_t occurrences = 0, bytes = 0;
    const auto old_aspects = m.base_signature.aspects().size();
    const auto new_aspects = m.target_signature.aspects().size();
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        const auto n = source.grade(core::Grade{static_cast<std::uint16_t>(k)}).incidence_count();
        occurrences = machine::checked_size_add(occurrences, n);
        // Conservative simultaneous capture/target occurrence rows, plus the
        // target's packed planes (including a partial word for each grade).
        const auto rows = machine::checked_size_mul(n, machine::checked_size_add(old_aspects, new_aspects));
        const auto planes = machine::checked_size_mul(machine::checked_size_mul(machine::ceil_div(n, 64), new_aspects), 8);
        bytes = machine::checked_size_add(bytes, machine::checked_size_add(rows, planes));
        require(occurrences <= limits.max_occurrences && bytes <= limits.max_aspect_bytes,
                "source declaration expansion exceeds occurrence/aspect buffer budget");
    }
}
inline ResolvedMigration resolve(const SourceDeclarationMigration& m) {
    const auto& a = m.base_signature; const auto& b = m.target_signature;
    require(m.aspects.size() == a.aspects().size() && m.types.size() == a.type_count(),
            "source declaration migration must map every old declaration");
    ResolvedMigration out;
    out.aspects.resize(a.aspects().size()); out.types.resize(a.type_count());
    std::set<std::string> from, to;
    for (const auto& p : m.aspects) {
        require(from.insert(p.from).second && to.insert(p.to).second, "noninjective/duplicate aspect migration");
        const auto x = a.aspects().id(p.from), y = b.aspects().id(p.to);
        require(a.aspects().at(x).role == b.aspects().at(y).role, "aspect meaning changed under recoding");
        out.aspects[x] = y;
    }
    require(out.aspects.at(a.existence_aspect()) == b.existence_aspect() &&
            out.aspects.at(a.orientation_aspect()) == b.orientation_aspect(), "signed incidence aspect migration");
    std::set<std::string> added;
    for (const auto& n : m.introduced) {
        (void)b.aspects().id(n.target);
        require(!to.contains(n.target) && added.insert(n.target).second, "introduced aspect already mapped/duplicated");
        std::set<StableOccurrenceKey> seen;
        for (const auto& v : n.assignments)
            require(v.occurrence.upper_grade.value && seen.insert(v.occurrence).second,
                    "invalid/duplicate introduced-aspect assignment");
    }
    require(to.size() + added.size() == b.aspects().size(), "new aspect lacks explicit initialization policy");
    from.clear(); to.clear();
    for (const auto& p : m.types) {
        require(from.insert(p.from).second && to.insert(p.to).second, "noninjective/duplicate type migration");
        const auto x = a.type_id(p.from), y = b.type_id(p.to);
        const auto& old = a.type(x); const auto& next = b.type(y);
        require(old.boundary_policy == next.boundary_policy && old.min_arity == next.min_arity &&
                old.max_arity == next.max_arity && old.constructor == next.constructor &&
                old.constraints.shape == next.constraints.shape &&
                old.constraints.rules.size() == next.constraints.rules.size(),
                "type rules/constructor cannot change under a declaration embedding");
        auto role = old.constraints.distinguished_role;
        if (role) role = out.aspects.at(*role);
        require(role == next.constraints.distinguished_role, "distinguished role migration mismatch");
        // Rules are an ordered executable declaration in the current source.
        for (std::size_t i = 0; i < old.constraints.rules.size(); ++i) {
            const auto& r = old.constraints.rules[i]; const auto& s = next.constraints.rules[i];
            require(r.kind == s.kind && out.aspects.at(r.aspect) == s.aspect &&
                    r.count == s.count && r.value == s.value, "source rule migration mismatch");
        }
        out.types[x] = y;
    }
    return out;
}
} // namespace source_declaration_detail

// This is an embedding/re-presentation of the retained base, not a source edit.
// An ordinary target-signature patch is applied AFTER this recoding. It keeps
// births, removals, attaching changes, coefficient changes and XOR deltas explicit.
inline core::RelationalComplex recode_temporal_source(const core::RelationalComplex& source,
                                                       const SourceDeclarationMigration& migration,
                                                       const SourceDeclarationLimits& limits = {}) {
    using namespace source_declaration_detail;
    require(signature_bytes(source.signature()) == signature_bytes(migration.base_signature),
            "migration base signature differs from the retained endpoint");
    check_source_allocation(source, migration, limits);
    const auto maps = resolve(migration);
    std::vector<temporal_source_detail::GradeImage> images;
    for (std::size_t k = 0; k < source.grade_count(); ++k) images.push_back(temporal_source_detail::capture(source, k));
    std::vector<std::map<StableOccurrenceKey, bool>> assignments;
    for (const auto& a : migration.introduced) {
        std::map<StableOccurrenceKey, bool> values;
        for (const auto& v : a.assignments) values.emplace(v.occurrence, v.value);
        assignments.push_back(std::move(values));
    }
    for (std::size_t k = 1; k < images.size(); ++k) {
        auto& image = images[k];
        for (auto& [key, type] : image.cells) { (void)key; type = maps.types.at(type); }
        for (auto& [key, occurrence] : image.occurrences) {
            std::vector<std::uint8_t> bits(migration.target_signature.aspects().size(), 0);
            for (std::size_t a = 0; a < maps.aspects.size(); ++a) bits[maps.aspects[a]] = occurrence.aspects.at(a);
            const StableOccurrenceKey stable{core::Grade{static_cast<std::uint16_t>(k)}, core::OccurrenceKey{key}};
            for (std::size_t i = 0; i < migration.introduced.size(); ++i) {
                const auto& init = migration.introduced[i]; const auto it = assignments[i].find(stable);
                require(it != assignments[i].end() || init.fill.has_value(), "missing retained occurrence aspect assignment");
                bits[migration.target_signature.aspects().id(init.target)] =
                    static_cast<std::uint8_t>(it != assignments[i].end() ? it->second : *init.fill);
                if (it != assignments[i].end()) assignments[i].erase(it);
            }
            occurrence.aspects = std::move(bits);
        }
    }
    for (const auto& remaining : assignments) require(remaining.empty(), "aspect assignment refers to a non-base occurrence");
    return temporal_source_detail::materialize_images(images, migration.target_signature, source.semantic_scale());
}
} // namespace rfc::storage
