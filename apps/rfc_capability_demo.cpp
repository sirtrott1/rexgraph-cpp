#include "rfc/agent/agent.hpp"
#include "rfc/generation/write_barrier.hpp"
#include "rfc/llm/context.hpp"
#include <iostream>
#include <memory>

namespace {
class EchoProvider final : public rfc::llm::LocalProvider {
public:
    std::string_view provider_name() const noexcept override { return "echo-local"; }
    rfc::llm::Response complete(const rfc::llm::Request& request) override {
        rfc::llm::Response out;
        out.text = "prospective response over " + std::to_string(request.evidence.size()) + " evidence items";
        out.evidence_used = request.evidence;
        return out;
    }
};
}

int main() {
    using rfc::capability::EpistemicClass;
    using rfc::inference::Query;
    using rfc::inference::Result;

    rfc::inference::Router router{
        [](const Query& q) -> std::optional<Result> {
            if (q.text == "stored") return Result{EpistemicClass::Stored, rfc::numeric::rational(7, 3), {}, {"relation:42"}};
            return std::nullopt;
        },
        [](const Query& q) -> std::optional<Result> {
            if (q.text == "derived") return Result{EpistemicClass::Derived, rfc::numeric::rational(5, 2), {}, {"green:query:17"}};
            return std::nullopt;
        },
        [](const Query&) -> Result {
            return Result{EpistemicClass::Prospective, std::nullopt, "forecast", {"model:demo"}};
        }};

    auto provider = std::make_shared<EchoProvider>();
    rfc::agent::Agent agent{router, provider};

    rfc::llm::StructuredContext context;
    context.exact_facts = "stored relation 42 = 7/3";
    context.derived_field_summary = "weighted Green response available";
    context.evidence.push_back({"relation:42", EpistemicClass::Stored, "fixture"});
    auto request = rfc::llm::ContextBuilder::make_request(
        "echo-local", {{rfc::llm::Role::User, "forecast the unresolved value"}}, std::move(context));

    for (const char* text : {"stored", "derived", "future"}) {
        const Query query{text, {}, {}};
        auto turn = agent.run(query, request);
        std::cout << text << " -> " << rfc::capability::to_string(turn.grounding.epistemic);
        if (turn.grounding.exact_value) std::cout << " value=" << rfc::numeric::str(*turn.grounding.exact_value);
        if (!turn.grounding.generated_value.empty()) std::cout << " value=" << turn.grounding.generated_value;
        if (turn.response) std::cout << " | " << turn.response->text;
        else std::cout << " | model_call=skipped";
        std::cout << '\n';
    }

    try {
        rfc::generation::CanonicalWriteBarrier::require_canonical_write(EpistemicClass::Prospective);
        return 2;
    } catch (const std::logic_error&) {
        std::cout << "write_barrier=pass\n";
    }
    return 0;
}
