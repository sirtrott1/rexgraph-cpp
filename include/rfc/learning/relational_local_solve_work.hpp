#pragma once

#include "rfc/learning/relational_available_local_solve_codec.hpp"
#include "rfc/runtime/work_graph.hpp"

namespace rfc::learning {
enum class RelationalLocalWorkPhase : std::uint8_t { Preparation, Action, Replay };
// The graph owns this entire unpublished preparation/action. WorkRun::cancel
// requests stop; WorkRun::wait joins before a caller accepts any completion.
inline runtime::WorkJob relational_local_solve_job(runtime::WorkKey key, runtime::WorkDemand demand,
    AvailableRelationalSolveRhs rhs, std::uint64_t node, inference::RelationalFieldSolveDeclaration declaration,
    RelationalSolveReadoutDeclaration readout, RelationalSolveOrigins origins, inference::RelationalAvailabilityContext context,
    numeric::ExactActionLimits limits = {}, std::function<void()> checkpoint = {},
    std::function<void(RelationalLocalWorkPhase)> phase = {}) {
    const auto frozen_declaration = declaration; const auto frozen_readout = readout;
    return runtime::WorkJob::make<AvailableRelationalLocalResult>(key, demand, {},
        [rhs = std::move(rhs), node, declaration = frozen_declaration, readout = frozen_readout, origins,
         context = std::move(context), limits, checkpoint = std::move(checkpoint), phase = std::move(phase), demand]
        (const auto&, auto& assigned, std::stop_token stop) {
            auto lease = assigned.reserve(demand.workers, demand.buffer_bytes);
            const auto check = [&] { if (stop.stop_requested()) throw runtime::ExecutionCancelled{};
                if (checkpoint) checkpoint();
                if (stop.stop_requested()) throw runtime::ExecutionCancelled{}; };
            check(); if (phase) phase(RelationalLocalWorkPhase::Preparation); check();
            auto prepared = PreparedAvailableRelationalLocalSolve(rhs, node, declaration, readout, origins, context, limits, check);
            if (phase) phase(RelationalLocalWorkPhase::Action);
            check();
            auto result = prepared.execute(context, limits, check); check(); return result;
        });
}
inline runtime::WorkJob relational_local_replay_job(runtime::WorkKey key, runtime::WorkDemand demand,
    std::vector<std::byte> packet, inference::RelationalAvailabilityContext context, storage::TemporalReadLimits policy,
    numeric::ExactActionLimits limits = {}, std::function<void()> checkpoint = {},
    std::function<void(RelationalLocalWorkPhase)> phase = {}) {
    const auto frozen_packet = packet;
    return runtime::WorkJob::make<AvailableRelationalLocalResult>(key, demand, {},
        [packet = frozen_packet, context = std::move(context), policy, limits, checkpoint = std::move(checkpoint),
         phase = std::move(phase), demand](const auto&, auto& assigned, std::stop_token stop) {
            auto lease = assigned.reserve(demand.workers, demand.buffer_bytes);
            const auto check = [&] {
                if (stop.stop_requested()) throw runtime::ExecutionCancelled{};
                if (checkpoint) checkpoint();
                if (stop.stop_requested()) throw runtime::ExecutionCancelled{};
            };
            check(); if (phase) phase(RelationalLocalWorkPhase::Replay); check();
            auto result = decode_available_relational_local_result(packet, context, policy, limits, check); check(); return result;
        });
}
} // namespace rfc::learning
