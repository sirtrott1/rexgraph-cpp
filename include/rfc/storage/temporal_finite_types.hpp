#pragma once

#include "rfc/storage/temporal_source.hpp"
#include <variant>

namespace rfc::storage {

// Finite values are not rational field amplitudes. Relation-valued systems
// refer to positive-grade stable identities in the same retained source.
enum class TemporalFiniteValueKind : std::uint8_t { SignedInteger, RetainedRelation };
using TemporalFiniteValue = std::variant<std::int64_t, core::RelationIdentity>;

struct TemporalFiniteDomain {
    StableCellKey cell;
    // Enumeration order is retained (including the native Boolean {0,1} order).
    std::vector<TemporalFiniteValue> values;
    bool operator==(const TemporalFiniteDomain&) const = default;
};
struct TemporalFiniteChart {
    std::uint64_t key{};
    std::string name;
    // Closed patch is a set; support order determines tuple coordinate meaning.
    std::vector<StableCellKey> patch, support;
    std::vector<std::vector<TemporalFiniteValue>> allowed;
    bool operator==(const TemporalFiniteChart&) const = default;
};
struct TemporalFiniteAssignment {
    StableCellKey cell;
    TemporalFiniteValue value;
    bool operator==(const TemporalFiniteAssignment&) const = default;
};
struct TemporalFiniteSection {
    std::uint64_t key{};
    // Partial assignments may be incompatible with charts: incompatibility is
    // a diagnostic, not corruption. Each assigned value must be in its domain.
    std::vector<TemporalFiniteAssignment> assignments;
    bool operator==(const TemporalFiniteSection&) const = default;
};
struct TemporalFiniteSystem {
    std::uint64_t key{};
    TemporalFiniteValueKind kind{TemporalFiniteValueKind::SignedInteger};
    std::vector<TemporalFiniteDomain> domains;
    // Chart order is retained so native chart-index certificates remain stable.
    std::vector<TemporalFiniteChart> charts;
    std::vector<TemporalFiniteSection> sections;
    bool operator==(const TemporalFiniteSystem&) const = default;
};

inline TemporalFiniteSystem normalize_temporal_finite_system(TemporalFiniteSystem system) {
    std::sort(system.domains.begin(), system.domains.end(), [](const auto& a, const auto& b) { return a.cell < b.cell; });
    for (auto& chart : system.charts) std::sort(chart.patch.begin(), chart.patch.end());
    std::sort(system.sections.begin(), system.sections.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
    for (auto& section : system.sections)
        std::sort(section.assignments.begin(), section.assignments.end(), [](const auto& a, const auto& b) { return a.cell < b.cell; });
    return system;
}

inline void validate_temporal_finite_system(const core::RelationalComplex& source,
                                             const StableSourceIndex& source_index,
                                             const TemporalFiniteSystem& system) {
    using temporal_source_detail::require;
    using temporal_source_detail::unique;
    require(system.kind == TemporalFiniteValueKind::SignedInteger ||
            system.kind == TemporalFiniteValueKind::RetainedRelation, "unknown finite value kind");
    const auto validate_value = [&](const TemporalFiniteValue& value) {
        if (system.kind == TemporalFiniteValueKind::SignedInteger) {
            require(std::holds_alternative<std::int64_t>(value), "noninteger value in integer finite system");
        } else {
            require(std::holds_alternative<core::RelationIdentity>(value), "nonrelation value in relation finite system");
            const auto relation = std::get<core::RelationIdentity>(value);
            require(relation.grade.value > 0 && source_index.cell({relation.grade, relation.key.value}).has_value(),
                    "finite relation value absent from retained source");
        }
    };
    std::map<StableCellKey, std::set<TemporalFiniteValue>> domains;
    for (const auto& domain : system.domains) {
        require(source_index.cell(domain.cell).has_value(), "finite domain owner absent");
        require(!domain.values.empty(), "finite domain must be nonempty");
        require(!domains.contains(domain.cell), "duplicate finite domain owner");
        auto& values = domains[domain.cell];
        for (const auto& value : domain.values) {
            validate_value(value);
            require(values.insert(value).second, "duplicate value in finite domain");
        }
    }
    const auto validate_assignment = [&](StableCellKey cell, const TemporalFiniteValue& value) {
        validate_value(value);
        const auto it = domains.find(cell);
        require(it != domains.end() && it->second.contains(value), "finite assignment outside declared domain");
    };
    std::set<std::uint64_t> chart_keys, section_keys;
    for (const auto& chart : system.charts) {
        unique(chart_keys, chart.key);
        algebra::SubcomplexMask patch(source);
        std::set<StableCellKey> patch_cells, support_cells;
        for (const auto key : chart.patch) {
            unique(patch_cells, key);
            const auto cell = source_index.cell(key);
            require(cell.has_value(), "finite chart patch cell absent");
            patch.select(*cell);
        }
        require(patch.boundary_closed(source), "finite chart patch is not boundary closed");
        require(!chart.support.empty() && !chart.allowed.empty(), "finite chart needs nonempty support and allowed tuples");
        for (const auto key : chart.support) {
            unique(support_cells, key);
            require(patch_cells.contains(key) && domains.contains(key), "finite chart support outside patch/domain");
        }
        for (const auto& tuple : chart.allowed) {
            require(tuple.size() == chart.support.size(), "finite tuple arity mismatch");
            for (std::size_t i = 0; i < tuple.size(); ++i) validate_assignment(chart.support[i], tuple[i]);
        }
    }
    for (const auto& section : system.sections) {
        unique(section_keys, section.key);
        std::set<StableCellKey> assigned;
        for (const auto& entry : section.assignments) {
            unique(assigned, entry.cell);
            validate_assignment(entry.cell, entry.value);
        }
    }
}

} // namespace rfc::storage
