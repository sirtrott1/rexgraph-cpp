#pragma once
#include "rfc/llm/types.hpp"
#include <string>
#include <utility>
#include <vector>

namespace rfc::llm {

struct StructuredContext {
    std::string exact_facts;
    std::string derived_field_summary;
    std::string prospective_context;
    std::vector<EvidenceRef> evidence;
};

class ContextBuilder {
public:
    static Request make_request(std::string model,
                                std::vector<Message> messages,
                                StructuredContext context,
                                std::vector<ToolSpec> tools = {}) {
        if (!context.exact_facts.empty()) {
            messages.push_back({Role::Tool, "EXACT_FACTS\n" + context.exact_facts});
        }
        if (!context.derived_field_summary.empty()) {
            messages.push_back({Role::Tool, "DERIVED_FIELDS\n" + context.derived_field_summary});
        }
        if (!context.prospective_context.empty()) {
            messages.push_back({Role::Tool, "PROSPECTIVE_CONTEXT\n" + context.prospective_context});
        }
        return Request{std::move(messages), std::move(context.evidence), std::move(tools), std::move(model), {}};
    }
};

} // namespace rfc::llm
