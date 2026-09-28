#pragma once
#include <cstdint>
#include <string_view>

namespace rfc::learning {

enum class ScalarDomain {
    ExactInteger,
    ExactRational,
    Int8,
    Int16,
    Int32,
    Int64,
    FixedPoint,
    Float16,
    BFloat16,
    Float32,
    Float64
};

struct ScalarPolicy {
    ScalarDomain domain{ScalarDomain::ExactInteger};
    std::int32_t fractional_bits{0};
    bool stochastic_rounding{false};
};

inline constexpr std::string_view to_string(ScalarDomain domain) noexcept {
    switch (domain) {
        case ScalarDomain::ExactInteger: return "exact-int";
        case ScalarDomain::ExactRational: return "exact-rational";
        case ScalarDomain::Int8: return "int8";
        case ScalarDomain::Int16: return "int16";
        case ScalarDomain::Int32: return "int32";
        case ScalarDomain::Int64: return "int64";
        case ScalarDomain::FixedPoint: return "fixed";
        case ScalarDomain::Float16: return "float16";
        case ScalarDomain::BFloat16: return "bfloat16";
        case ScalarDomain::Float32: return "float32";
        case ScalarDomain::Float64: return "float64";
    }
    return "unknown";
}

} // namespace rfc::learning
