#pragma once

#include "rfc/database/native_field_cache.hpp"
#include "rfc/runtime/locality_tiles.hpp"

namespace rfc::database {
inline PreparedParallelNativeField retile_field(const PreparedParallelNativeField& operation,
    const runtime::HardwareInventory& hardware,const runtime::LocalityTilePolicy& policy={},
    const runtime::ExecutionHostPolicy& host={}) {
    const auto& action=operation.ranges().action();
    std::vector<runtime::LocalityRowDemand> demands;demands.reserve(action.statistics().target_dimension);
    for(std::size_t row=0;row<action.statistics().target_dimension;++row) {
        const auto terms=action.target_offsets()[row+1]-action.target_offsets()[row];
        // Conservative object traffic estimate; coefficient/input limbs remain variable.
        const auto bytes=machine::checked_size_add(sizeof(numeric::Rational),machine::checked_size_mul(terms,
            machine::checked_size_add(sizeof(numeric::Rational),sizeof(local::PreparedGradeFieldAction::Entry))));
        demands.push_back({terms,bytes});
    }
    const auto plan=runtime::plan_locality_tiles(demands,hardware,policy);
    auto ranges=local::PreparedGradeFieldRanges::partition(action,plan.rows);
    auto report=native_field_cache_detail::parallel_report(operation.analysis(),ranges.rows_per_tile(),ranges.tiles().size());
    runtime::admit_execution(report.requirements,host);
    report.preparation_policy=host;report.implementation=operation.report().implementation;
    report.selected_backend=runtime::ExecutionBackend::ExactCpu;
    report.fallback=host.backend!=runtime::ExecutionBackend::ExactCpu;
    report.decisions.at(3)=operation.report().decisions.at(3);
    report.decisions.push_back({runtime::DecisionKind::Profile,"working-set-target","incidence-locality-v1","1",
        std::to_string(plan.working_set_target),"object-byte traffic target; GMP limbs/retained source excluded; oversized rows "+
        std::to_string(plan.oversized_rows)+"; cache fact "+(plan.cache?"observed":"unavailable"),"hardware/policy changes"});
    return {operation.analysis(),std::move(ranges),std::move(report),operation.input_pin()};
}
} // namespace rfc::database
