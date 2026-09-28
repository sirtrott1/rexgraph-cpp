#pragma once
#include "rfc/capability/epistemic.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rfc::inference {

struct Query {
    std::string text;
    std::string relation_type;
    std::vector<std::uint64_t> relation_ids;
};

struct Result {
    capability::EpistemicClass epistemic{capability::EpistemicClass::Unknown};
    std::optional<numeric::Rational> exact_value;
    std::string generated_value;
    std::vector<std::string> provenance;
};

} // namespace rfc::inference
