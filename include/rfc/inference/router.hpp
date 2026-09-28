#pragma once
#include "rfc/inference/query.hpp"
#include <functional>
#include <optional>
#include <stdexcept>

namespace rfc::inference {

class Router {
public:
    using ExactRoute = std::function<std::optional<Result>(const Query&)>;
    using ProspectiveRoute = std::function<Result(const Query&)>;

    Router(ExactRoute stored, ExactRoute derived, ProspectiveRoute prospective)
        : stored_(std::move(stored)), derived_(std::move(derived)), prospective_(std::move(prospective)) {
        if (!stored_ || !derived_ || !prospective_) throw std::invalid_argument("Router requires all routes");
    }

    Result resolve(const Query& query) const {
        if (auto result = stored_(query)) return *result;
        if (auto result = derived_(query)) return *result;
        return prospective_(query);
    }

private:
    ExactRoute stored_;
    ExactRoute derived_;
    ProspectiveRoute prospective_;
};

} // namespace rfc::inference
