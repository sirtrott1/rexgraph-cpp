#pragma once
#include "rfc/learning/context_transport_rules.hpp"
#include "rfc/learning/context_transport_views.hpp"

namespace rfc::learning {
struct TransportContextViewRuleAcquisition {
    TransportContextViewAcquisition comparisons;
    std::shared_ptr<const PreparedTransportParameterRelations> proof;
    std::vector<TransportContextRule> rules;
    std::vector<TransportParameterRelationStatus> block_statuses;
    std::vector<std::size_t> block_ranks, selected_blocks;
    std::vector<std::vector<std::size_t>> covering_peers;
    std::map<std::string,std::size_t> rejections;
    std::size_t steps{}, retained{};
};

// Compare independent observed stars through faithful rectangular views.
// Each target input must lie in the shared image of enough agreeing peers.
// Target transfer is conditional; unshared inputs remain unidentified.
inline TransportContextViewRuleAcquisition acquire_context_transport_view_rules(
    const PreparedTransportObservations& observations,
    const std::vector<TransportObservation>& samples,
    const std::vector<std::size_t>& training,const std::vector<std::size_t>& targets,
    const std::vector<std::vector<TransportContextView>>& atlas,
    std::size_t minimum_peers,numeric::ExactActionGuard& guard,
    TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
    using namespace context_view_detail;
    const auto start=guard.steps();guard.step();
    if(observations.parameter_basis())
        throw std::invalid_argument("context view rules require independent original map entries");
    if(!minimum_peers) throw std::invalid_argument("positive independent peer admission required");
    std::set<std::size_t> train,held;
    for(const auto row:training) {guard.step();if(!train.insert(row).second)
        throw std::invalid_argument("distinct original training rows required");}
    for(const auto row:targets) {guard.step();if(train.contains(row) || !held.insert(row).second)
        throw std::invalid_argument("distinct target rows outside the training family required");}
    std::vector<std::size_t> all=training;all.insert(all.end(),targets.begin(),targets.end());guard.dimension(all.size());
    TransportContextViewRuleAcquisition out;
    out.comparisons=acquire_context_transport_views(observations,samples,all,atlas,guard,scope);
    const auto& acquired=out.comparisons.acquisition;
    const std::set<std::size_t> seeds(acquired.seed_rows.begin(),acquired.seed_rows.end());
    for(const auto row:training) if(!seeds.contains(row))
        throw std::invalid_argument("each training row requires an actual observed field probe");
    out.retained=add(acquired.retained,add(training.size(),targets.size()));guard.entries(out.retained);
    if(acquired.relations.empty()) {out.steps=guard.steps()-start;return out;}
    out.proof=std::make_shared<const PreparedTransportParameterRelations>(observations,samples,acquired.relations,guard,
        TransportParameterSupportPolicy::IncludeConstants);
    out.retained=add(out.retained,out.proof->retained_entries());guard.entries(out.retained);
    for(const auto& block:acquired.hypotheses) {
        guard.step();auto status=TransportParameterRelationStatus::Forced;std::size_t rank{};
        for(const auto equation:block.equations) {guard.step();
            if(out.proof->statuses().at(equation)==TransportParameterRelationStatus::Excluded) {
                status=TransportParameterRelationStatus::Excluded;break;
            }
            if(out.proof->statuses().at(equation)==TransportParameterRelationStatus::Open)
                status=TransportParameterRelationStatus::Open;
        }
        if(status==TransportParameterRelationStatus::Open) {
            const auto joint=out.proof->condition_status(block.equations,guard);rank=joint.rank;
            if(!joint.consistent) status=TransportParameterRelationStatus::Excluded;
        }
        out.block_statuses.push_back(status);out.block_ranks.push_back(rank);out.covering_peers.emplace_back();
        out.retained=add(out.retained,3);guard.entries(out.retained);
    }
    std::map<std::size_t,M> annihilators;std::set<std::size_t> selected_targets;
    for(const auto origin:training) {
        guard.step();TransportContextRule rule;rule.origin=origin;
        for(std::size_t i=0;i<acquired.hypotheses.size();++i) {
            guard.step();const auto& block=acquired.hypotheses[i];
            if(block.origin!=origin || !train.contains(block.target)) continue;
            const auto status=out.block_statuses[i];
            auto& family=status==TransportParameterRelationStatus::Forced?rule.forced:
                status==TransportParameterRelationStatus::Excluded?rule.excluded:rule.open;
            family.push_back(i);out.retained=add(out.retained,1);guard.entries(out.retained);
        }
        rule.admitted=rule.forced.size()>=minimum_peers && rule.excluded.empty() && rule.open.empty();
        if(rule.admitted) for(std::size_t i=0;i<acquired.hypotheses.size();++i) {
            guard.step();const auto& block=acquired.hypotheses[i];
            if(block.origin!=origin || !held.contains(block.target)) continue;
            if(out.block_statuses[i]==TransportParameterRelationStatus::Excluded) {
                ++out.rejections["contradictory-target-block"];continue;
            }
            const auto& candidate=out.comparisons.witnesses[i].common_input;
            for(const auto peer:rule.forced) {
                guard.step();auto found=annihilators.find(peer);
                if(found==annihilators.end()) {
                    const auto& input=out.comparisons.witnesses[peer].common_input;
                    guard.entries(add(out.retained,input.nonzeros()));
                    const numeric::PreparedExactFrames frames(input.transpose(),guard);
                    guard.entries(add(out.retained,add(frames.stored_entries(),frames.kernel().nonzeros())));
                    auto annihilator=frames.kernel().transpose();
                    out.retained=add(out.retained,add(2,annihilator.nonzeros()));guard.entries(out.retained);
                    found=annihilators.emplace(peer,std::move(annihilator)).first;
                }
                if(product(found->second,candidate,guard,out.retained).nonzeros()==0) {
                    out.covering_peers[i].push_back(peer);out.retained=add(out.retained,1);guard.entries(out.retained);
                }
            }
            if(out.covering_peers[i].size()<minimum_peers) {
                ++out.rejections["insufficient-independent-input-coverage"];continue;
            }
            if(selected_targets.insert(block.target).second) {
                out.selected_blocks.push_back(i);out.retained=add(out.retained,1);guard.entries(out.retained);
            }
        }
        out.rules.push_back(std::move(rule));out.retained=add(out.retained,5);guard.entries(out.retained);
    }
    out.steps=guard.steps()-start;return out;
}
} // namespace rfc::learning
