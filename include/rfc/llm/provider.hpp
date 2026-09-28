#pragma once
#include "rfc/llm/types.hpp"
#include "rfc/models/runtime.hpp"
#include <array>
#include <span>
#include <string_view>

namespace rfc::llm {

class Provider : public models::Runtime {
public:
    ~Provider() override = default;
    virtual std::string_view provider_name() const noexcept = 0;
    virtual Response complete(const Request& request) = 0;

    std::string_view runtime_name() const noexcept final { return provider_name(); }
    std::span<const models::Capability> capabilities() const noexcept override {
        static constexpr std::array caps{models::Capability::Chat, models::Capability::TextGeneration};
        return caps;
    }
};

class LocalProvider : public Provider {
public:
    ~LocalProvider() override = default;
};

class RemoteProvider : public Provider {
public:
    ~RemoteProvider() override = default;
};

} // namespace rfc::llm
