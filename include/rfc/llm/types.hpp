#pragma once
#include "rfc/capability/epistemic.hpp"
#include <string>
#include <vector>

namespace rfc::llm {

enum class Role { System, User, Assistant, Tool };

struct Message {
    Role role{Role::User};
    std::string content;
};

struct EvidenceRef {
    std::string relation_id;
    capability::EpistemicClass epistemic{capability::EpistemicClass::Unknown};
    std::string provenance;
};

struct ToolSpec {
    std::string name;
    std::string description;
    std::string input_schema;
};

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};

struct Request {
    std::vector<Message> messages;
    std::vector<EvidenceRef> evidence;
    std::vector<ToolSpec> tools;
    std::string model;
    std::string response_schema;
};

struct Response {
    std::string text;
    std::vector<ToolCall> tool_calls;
    std::vector<EvidenceRef> evidence_used;
};

} // namespace rfc::llm
