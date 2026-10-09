#pragma once
#include "rfc/learning/context_transport_pattern_bank.hpp"
#include "rfc/storage/native_series_codec.hpp"

namespace rfc::learning {
inline std::vector<std::byte> encode_transport_pattern_bank(const PreparedTransportPatternBank& bank,
    std::uint64_t model_namespace,const storage::NativeSeriesLimits& limits={}) {
    return storage::encode_native_series(storage::NativeTemporalSeries(bank.endpoint(),model_namespace,limits));
}
inline PreparedTransportPatternBank decode_transport_pattern_bank(std::span<const std::byte> bytes,
    std::uint64_t expected_namespace,numeric::ExactActionGuard& guard,const storage::NativeSeriesLimits& limits={}) {
    const auto series=storage::decode_native_series(bytes,limits);
    if(series.source_namespace()!=expected_namespace || series.segment_count() || series.storage_stats().checkpoint_count || series.last_sequence())
        throw std::invalid_argument("single bank publication in the declared model namespace required");
    const auto publication=series.tail();
    return PreparedTransportPatternBank::from_endpoint(publication.endpoint(),guard);
}
} // namespace rfc::learning
