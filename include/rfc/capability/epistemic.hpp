#pragma once
#include <string_view>

namespace rfc::capability {

enum class EpistemicClass {
    Stored,
    Derived,
    Prospective,
    Proposed,
    Unknown
};

inline constexpr std::string_view to_string(EpistemicClass value) noexcept {
    switch (value) {
        case EpistemicClass::Stored: return "stored";
        case EpistemicClass::Derived: return "derived";
        case EpistemicClass::Prospective: return "prospective";
        case EpistemicClass::Proposed: return "proposed";
        case EpistemicClass::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace rfc::capability
