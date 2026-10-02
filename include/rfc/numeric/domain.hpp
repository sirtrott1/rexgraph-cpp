#pragma once
#include <cstdint>

namespace rfc::numeric::domain_detail {

enum class DomainKind : std::uint8_t {
    Binary,
    Integer,
    Rational,
    Modular,
    Approximate
};
} // namespace rfc::numeric::domain_detail

namespace rfc::numeric {
using DomainKind = domain_detail::DomainKind;

struct ModularDomain {
    std::uint64_t modulus{};
};

} // namespace rfc::numeric
