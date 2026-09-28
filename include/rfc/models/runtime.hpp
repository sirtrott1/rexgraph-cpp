#pragma once
#include <span>
#include <string_view>
#include <vector>

namespace rfc::models {

enum class Capability {
    Chat,
    TextGeneration,
    Embedding,
    Reranking,
    Classification,
    Vision,
    Audio,
    Video,
    Multimodal,
    Forecasting,
    Custom
};

class Runtime {
public:
    virtual ~Runtime() = default;
    virtual std::string_view runtime_name() const noexcept = 0;
    virtual std::span<const Capability> capabilities() const noexcept = 0;
};

} // namespace rfc::models
