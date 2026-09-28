#pragma once
#include "rfc/models/runtime.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace rfc::models {

class Registry {
public:
    void add(std::string name, std::shared_ptr<Runtime> runtime) {
        if (!runtime) throw std::invalid_argument("cannot register null model runtime");
        runtimes_.insert_or_assign(std::move(name), std::move(runtime));
    }

    [[nodiscard]] std::shared_ptr<Runtime> get(const std::string& name) const {
        const auto it = runtimes_.find(name);
        if (it == runtimes_.end()) throw std::out_of_range("model runtime not registered");
        return it->second;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Runtime>> runtimes_;
};

} // namespace rfc::models
