#pragma once

#include "rfc/storage/temporal_endpoint.hpp"
#include "rfc/local/constraint_system.hpp"
#include <type_traits>

namespace rfc::storage {
namespace temporal_finite_detail {
template <class Value>
inline constexpr TemporalFiniteValueKind value_kind = [] {
    static_assert(std::is_same_v<Value, std::int64_t> || std::is_same_v<Value, core::RelationIdentity>,
                  "temporal finite adapters support int64 and source-local RelationIdentity values");
    if constexpr (std::is_same_v<Value, std::int64_t>) return TemporalFiniteValueKind::SignedInteger;
    else return TemporalFiniteValueKind::RetainedRelation;
}();
inline const TemporalFiniteSystem& find(const TemporalEndpoint& endpoint, std::uint64_t key) {
    for (const auto& system : endpoint.finite_systems) if (system.key == key) return system;
    throw std::invalid_argument("finite system identity absent");
}
} // namespace temporal_finite_detail

// Explicit stable chart keys are supplied by the application. Native chart
// positions are not promoted to mathematical identities during capture.
template <class Value>
inline TemporalFiniteSystem capture_temporal_finite_system(
    std::uint64_t key, const local::FiniteConstraintSystem<Value>& native,
    const std::vector<std::uint64_t>& chart_keys) {
    using temporal_source_detail::require;
    require(chart_keys.size() == native.charts().size(), "one stable key required for each native finite chart");
    TemporalFiniteSystem result;
    result.key = key; result.kind = temporal_finite_detail::value_kind<Value>;
    const auto& source = native.complex();
    for (const auto& [cell, values] : native.domains()) {
        TemporalFiniteDomain domain; domain.cell = stable_cell_key(source, cell);
        for (const auto& value : values) domain.values.emplace_back(value);
        result.domains.push_back(std::move(domain));
    }
    for (std::size_t i = 0; i < native.charts().size(); ++i) {
        const auto& chart = native.charts()[i];
        TemporalFiniteChart record; record.key = chart_keys[i]; record.name = chart.name;
        for (std::size_t k = 0; k < source.grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            chart.patch.grade(grade).for_each_set([&](std::size_t index) {
                record.patch.push_back(stable_cell_key(source, {grade, static_cast<std::uint32_t>(index)}));
            });
        }
        for (const auto cell : chart.support) record.support.push_back(stable_cell_key(source, cell));
        for (const auto& tuple : chart.allowed) {
            std::vector<TemporalFiniteValue> values;
            for (const auto& value : tuple) values.emplace_back(value);
            record.allowed.push_back(std::move(values));
        }
        result.charts.push_back(std::move(record));
    }
    result = normalize_temporal_finite_system(std::move(result));
    validate_temporal_finite_system(source, StableSourceIndex(source), result);
    return result;
}

template <class Value>
inline TemporalFiniteSection capture_temporal_finite_section(
    const core::RelationalComplex& source, std::uint64_t key, const local::BasicSection<Value>& native) {
    (void)temporal_finite_detail::value_kind<Value>;
    TemporalFiniteSection result; result.key = key;
    for (const auto& [cell, value] : native) result.assignments.push_back({stable_cell_key(source, cell), value});
    std::sort(result.assignments.begin(), result.assignments.end(), [](const auto& a, const auto& b) { return a.cell < b.cell; });
    return result;
}

// As with the native local carrier, the endpoint's source must outlive the
// returned constraint system. No solver or linear innovation is invoked.
template <class Value>
inline local::FiniteConstraintSystem<Value> temporal_finite_constraint_system(
    const TemporalEndpoint& endpoint, std::uint64_t key) {
    validate_temporal_endpoint(endpoint);
    const auto& system = temporal_finite_detail::find(endpoint, key);
    temporal_source_detail::require(system.kind == temporal_finite_detail::value_kind<Value>, "finite adapter value-kind mismatch");
    const StableSourceIndex index(endpoint.source);
    local::FiniteConstraintSystem<Value> result(endpoint.source);
    for (const auto& domain : system.domains) {
        std::vector<Value> values;
        for (const auto& value : domain.values) values.push_back(std::get<Value>(value));
        result.set_domain(index.cell(domain.cell).value(), std::move(values));
    }
    for (const auto& chart : system.charts) {
        algebra::SubcomplexMask patch(endpoint.source);
        for (const auto cell : chart.patch) patch.select(index.cell(cell).value());
        std::vector<core::CellId> support;
        for (const auto cell : chart.support) support.push_back(index.cell(cell).value());
        std::vector<std::vector<Value>> allowed;
        for (const auto& tuple : chart.allowed) {
            std::vector<Value> values;
            for (const auto& value : tuple) values.push_back(std::get<Value>(value));
            allowed.push_back(std::move(values));
        }
        result.add_chart(chart.name, std::move(patch), std::move(support), std::move(allowed));
    }
    return result;
}
template <class Value>
local::FiniteConstraintSystem<Value> temporal_finite_constraint_system(const TemporalEndpoint&&, std::uint64_t) = delete;

template <class Value>
inline local::BasicSection<Value> temporal_finite_section(const TemporalEndpoint& endpoint,
                                                        std::uint64_t system_key, std::uint64_t section_key) {
    validate_temporal_endpoint(endpoint);
    const auto& system = temporal_finite_detail::find(endpoint, system_key);
    temporal_source_detail::require(system.kind == temporal_finite_detail::value_kind<Value>, "finite section value-kind mismatch");
    const StableSourceIndex index(endpoint.source);
    for (const auto& section : system.sections) if (section.key == section_key) {
        local::BasicSection<Value> result;
        for (const auto& a : section.assignments) result.emplace(index.cell(a.cell).value(), std::get<Value>(a.value));
        return result;
    }
    throw std::invalid_argument("finite section identity absent");
}

} // namespace rfc::storage
