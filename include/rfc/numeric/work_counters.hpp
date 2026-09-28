#pragma once
#include <cstdint>

namespace rfc::numeric {

// Optional execution instrumentation.  Normal action paths compile without
// counter updates; counted overloads use the same kernels with Count=true.
struct WorkCounters {
    std::uint64_t active_coordinates{};
    std::uint64_t bit_words_scanned{};
    std::uint64_t incidences_touched{};
    std::uint64_t exact_integer_ops{};
    std::uint64_t rational_promotions{};
    std::uint64_t cache_hits{};
};

} // namespace rfc::numeric
