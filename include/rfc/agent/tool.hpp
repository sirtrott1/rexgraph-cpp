#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rfc::agent {

struct ToolResult {
    std::string output;
    bool exact{false};
};

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string_view name() const noexcept = 0;
    virtual ToolResult invoke(std::string_view arguments) = 0;
};

class ToolRegistry {
public:
    void add(std::shared_ptr<Tool> tool) {
        tools_.insert_or_assign(std::string(tool->name()), std::move(tool));
    }

    ToolResult invoke(const std::string& name, std::string_view arguments) const {
        const auto it = tools_.find(name);
        if (it == tools_.end()) throw std::out_of_range("tool not registered");
        return it->second->invoke(arguments);
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};

} // namespace rfc::agent
