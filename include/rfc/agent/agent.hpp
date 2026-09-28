#pragma once
#include "rfc/inference/router.hpp"
#include "rfc/llm/provider.hpp"
#include <memory>
#include <optional>

namespace rfc::agent {

struct AgentPolicy {
    bool model_for_exact_resolution{false};
};

struct AgentTurn {
    inference::Result grounding;
    std::optional<llm::Response> response;
};

class Agent {
public:
    Agent(inference::Router router, std::shared_ptr<llm::Provider> provider, AgentPolicy policy = {})
        : router_(std::move(router)), provider_(std::move(provider)), policy_(policy) {}

    AgentTurn run(const inference::Query& query, const llm::Request& request) {
        auto grounding = router_.resolve(query);
        const bool prospective = grounding.epistemic == capability::EpistemicClass::Prospective ||
                                 grounding.epistemic == capability::EpistemicClass::Proposed ||
                                 grounding.epistemic == capability::EpistemicClass::Unknown;
        if (prospective || policy_.model_for_exact_resolution) {
            return {std::move(grounding), provider_->complete(request)};
        }
        return {std::move(grounding), std::nullopt};
    }

private:
    inference::Router router_;
    std::shared_ptr<llm::Provider> provider_;
    AgentPolicy policy_;
};

} // namespace rfc::agent
