#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rfc::database {

using ProvenanceId = std::uint64_t;

struct ProvenanceRecord {
    ProvenanceId id{};
    std::string source;
    std::string assertion;
    std::string timestamp;
    std::vector<ProvenanceId> parents;
};

} // namespace rfc::database
