#pragma once
#include <cstdint>

namespace rfc::numeric {

enum class DomainKind : std::uint8_t {
    Binary,
    Integer,
    Rational,
    Modular,
    Approximate
};

struct ModularDomain {
    std::uint64_t modulus{};
};

} // namespace rfc::numeric
