#pragma once
#include "rfc/llm/provider.hpp"
#include "rfc/models/registry.hpp"
#include <memory>
#include <stdexcept>
#include <string>

namespace rfc::llm {

inline std::shared_ptr<Provider> provider_from(const models::Registry& registry, const std::string& name) {
    auto provider = std::dynamic_pointer_cast<Provider>(registry.get(name));
    if (!provider) throw std::invalid_argument("registered runtime is not an LLM provider");
    return provider;
}

} // namespace rfc::llm
