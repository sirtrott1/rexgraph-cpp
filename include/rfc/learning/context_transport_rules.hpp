#pragma once
#include "rfc/learning/context_transport_hypotheses.hpp"

namespace rfc::learning {
struct TransportContextRule {
    std::size_t origin{};
    std::vector<std::size_t> forced, excluded, open;
    bool admitted{};
};
struct TransportContextRuleAcquisition {
    TransportContextAcquisition comparisons;
    std::vector<TransportContextRule> rules;
    std::vector<TransportParameterRelationStatus> block_statuses;
    std::vector<std::size_t> selected_blocks;
    std::size_t steps{}, retained{};
};

// Learn complete star sharing hypotheses from independent observed map entries.
// Forced training comparisons support a prospective rule. They do not prove
// equality at an unobserved target. Any compatible counterexample or unresolved
// training comparison prevents admission of that origin's rule.
inline TransportContextRuleAcquisition acquire_context_transport_rules(
    const PreparedTransportObservations& observations,
    const std::vector<TransportObservation>& samples,
    const std::vector<std::size_t>& training,
    const std::vector<std::size_t>& targets,
    const std::vector<std::vector<TransportContextFrame>>& atlas,
    std::size_t minimum_peers,numeric::ExactActionGuard& guard) {
    using namespace algebra::owned_gluing_detail;
    const auto start=guard.steps();guard.step();
    if(observations.parameter_basis())
        throw std::invalid_argument("context rules require independent original map entries");
    if(!minimum_peers) throw std::invalid_argument("positive independent peer admission required");
    std::set<std::size_t> train,held;
    for(const auto row:training) {guard.step();if(!train.insert(row).second)
        throw std::invalid_argument("distinct original training rows required");}
    for(const auto row:targets) {guard.step();if(train.contains(row) || !held.insert(row).second)
        throw std::invalid_argument("distinct target rows outside the training family required");}
    std::vector<std::size_t> all=training;all.insert(all.end(),targets.begin(),targets.end());
    guard.dimension(all.size());
    TransportContextRuleAcquisition out;
    out.comparisons=acquire_context_transport_hypotheses(observations,samples,all,atlas,guard);
    const std::set<std::size_t> seeds(out.comparisons.seed_rows.begin(),out.comparisons.seed_rows.end());
    for(const auto row:training) if(!seeds.contains(row))
        throw std::invalid_argument("each training row requires an actual observed field probe");
    out.retained=add(out.comparisons.retained,add(training.size(),targets.size()));
    guard.entries(out.retained);
    if(out.comparisons.relations.empty()) {out.steps=guard.steps()-start;return out;}
    const PreparedTransportParameterRelations proof(observations,samples,out.comparisons.relations,guard);
    out.retained=add(out.retained,proof.retained_entries());guard.entries(out.retained);
    for(const auto& block:out.comparisons.hypotheses) {
        guard.step();auto status=TransportParameterRelationStatus::Forced;
        for(const auto equation:block.equations) {guard.step();
            if(proof.statuses().at(equation)==TransportParameterRelationStatus::Excluded) {
                status=TransportParameterRelationStatus::Excluded;break;
            }
            if(proof.statuses().at(equation)==TransportParameterRelationStatus::Open)
                status=TransportParameterRelationStatus::Open;
        }
        if(status==TransportParameterRelationStatus::Open) {
            const auto family=proof.condition(block.equations,guard);
            if(!family.consistent) status=TransportParameterRelationStatus::Excluded;
        }
        out.block_statuses.push_back(status);out.retained=add(out.retained,1);guard.entries(out.retained);
    }
    std::set<std::size_t> selected_targets;
    for(const auto origin:training) {
        guard.step();TransportContextRule rule;rule.origin=origin;
        for(std::size_t i=0;i<out.comparisons.hypotheses.size();++i) {
            guard.step();const auto& block=out.comparisons.hypotheses[i];
            if(block.origin!=origin || !train.contains(block.target)) continue;
            const auto status=out.block_statuses[i];
            auto& family=status==TransportParameterRelationStatus::Forced?rule.forced:
                status==TransportParameterRelationStatus::Excluded?rule.excluded:rule.open;
            family.push_back(i);out.retained=add(out.retained,1);guard.entries(out.retained);
        }
        rule.admitted=rule.forced.size()>=minimum_peers && rule.excluded.empty() && rule.open.empty();
        if(rule.admitted) for(std::size_t i=0;i<out.comparisons.hypotheses.size();++i) {
            guard.step();const auto& block=out.comparisons.hypotheses[i];
            if(block.origin==origin && held.contains(block.target) &&
               out.block_statuses[i]!=TransportParameterRelationStatus::Excluded &&
               selected_targets.insert(block.target).second) {
                out.selected_blocks.push_back(i);out.retained=add(out.retained,1);guard.entries(out.retained);
            }
        }
        out.rules.push_back(std::move(rule));out.retained=add(out.retained,5);guard.entries(out.retained);
    }
    out.steps=guard.steps()-start;return out;
}
} // namespace rfc::learning
