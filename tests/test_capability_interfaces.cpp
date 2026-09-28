#include "rfc/generation/write_barrier.hpp"
#include "rfc/inference/router.hpp"
#include "rfc/llm/context.hpp"
#include "rfc/learning/scalar_policy.hpp"
#include <optional>

int main() {
    using rfc::capability::EpistemicClass;
    using rfc::inference::Query;
    using rfc::inference::Result;

    rfc::inference::Router router{
        [](const Query& q) -> std::optional<Result> {
            if (q.text == "fact") return Result{EpistemicClass::Stored, rfc::numeric::rational(3, 2), {}, {"fact:1"}};
            return std::nullopt;
        },
        [](const Query& q) -> std::optional<Result> {
            if (q.text == "derived") return Result{EpistemicClass::Derived, rfc::numeric::rational(9, 4), {}, {"field:1"}};
            return std::nullopt;
        },
        [](const Query&) -> Result {
            return Result{EpistemicClass::Prospective, std::nullopt, "prediction", {"model:1"}};
        }};

    if (router.resolve(Query{"fact", {}, {}}).epistemic != EpistemicClass::Stored) return 1;
    if (router.resolve(Query{"derived", {}, {}}).epistemic != EpistemicClass::Derived) return 2;
    if (router.resolve(Query{"future", {}, {}}).epistemic != EpistemicClass::Prospective) return 3;

    bool blocked = false;
    try {
        rfc::generation::CanonicalWriteBarrier::require_canonical_write(EpistemicClass::Prospective);
    } catch (...) {
        blocked = true;
    }
    if (!blocked) return 4;
    rfc::generation::CanonicalWriteBarrier::require_canonical_write(EpistemicClass::Stored);

    rfc::llm::StructuredContext context;
    context.exact_facts = "x=3/2";
    context.evidence.push_back({"fact:1", EpistemicClass::Stored, "unit"});
    auto request = rfc::llm::ContextBuilder::make_request(
        "local", {{rfc::llm::Role::User, "question"}}, std::move(context));
    if (request.evidence.size() != 1u) return 5;
    if (request.messages.size() != 2u) return 6;

    const rfc::learning::ScalarPolicy policy{rfc::learning::ScalarDomain::Int16, 0, false};
    if (policy.domain != rfc::learning::ScalarDomain::Int16) return 7;
    return 0;
}
