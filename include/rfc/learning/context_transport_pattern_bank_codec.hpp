#pragma once
#include "rfc/learning/context_transport_pattern_bank.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include <limits>

namespace rfc::learning {
inline storage::NativeSeriesLimits transport_pattern_bank_archive_limits(
    const numeric::ExactActionLimits& action,std::size_t maximum=256u*1024u*1024u) {
    storage::NativeSeriesLimits out;
    out.max_archive_bytes=maximum;out.temporal.max_file_bytes=maximum;
    out.temporal.max_records=action.entries;
    out.temporal.max_dimension=std::max(action.dimension,action.entries);
    const auto largest=std::numeric_limits<std::size_t>::max();
    const auto square=action.dimension && action.dimension>largest/action.dimension?
        largest:action.dimension*action.dimension;
    out.temporal.max_logical_size=std::max(square,out.temporal.max_dimension);
    out.temporal.max_integer_bytes=machine::checked_size_add(action.integer_bits/8,1);
    return out;
}
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
