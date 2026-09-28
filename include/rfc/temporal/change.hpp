#pragma once
#include <cstdint>
#include <string_view>

namespace rfc::temporal {

enum class ChangeChannel : std::uint8_t {
    Structure,
    Field,
    Source,
    Metric,
    Type,
    Accession,
    Observation
};

inline constexpr std::string_view to_string(ChangeChannel channel) noexcept {
    switch (channel) {
        case ChangeChannel::Structure: return "structure";
        case ChangeChannel::Field: return "field";
        case ChangeChannel::Source: return "source";
        case ChangeChannel::Metric: return "metric";
        case ChangeChannel::Type: return "type";
        case ChangeChannel::Accession: return "accession";
        case ChangeChannel::Observation: return "observation";
    }
    return "unknown";
}
} // namespace rfc::temporal
