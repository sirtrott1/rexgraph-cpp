#pragma once
#include "rfc/learning/context_transport_pattern.hpp"
#include "rfc/storage/native_series_codec.hpp"

namespace rfc::learning {
// Persist the pattern source and exact tensors in the canonical series format.
// The inference artifact contains one publication and no training observations.
inline std::vector<std::byte> encode_transport_pattern(const PreparedTransportPattern& pattern,
    std::uint64_t model_namespace,const storage::NativeSeriesLimits& limits={}) {
    return storage::encode_native_series(storage::NativeTemporalSeries(pattern.endpoint(),model_namespace,limits));
}
inline PreparedTransportPattern decode_transport_pattern(std::span<const std::byte> bytes,
    std::uint64_t expected_namespace,numeric::ExactActionGuard& guard,const storage::NativeSeriesLimits& limits={}) {
    const auto series=storage::decode_native_series(bytes,limits);
    if(series.source_namespace()!=expected_namespace || series.segment_count() || series.storage_stats().checkpoint_count ||
        series.last_sequence()) throw std::invalid_argument("single pattern publication in the declared model namespace required");
    const auto publication=series.tail();
    return PreparedTransportPattern::from_endpoint(publication.endpoint(),guard);
}
} // namespace rfc::learning
