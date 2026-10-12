#pragma once

#include "rfc/storage/native_series.hpp"

namespace rfc::storage {
// A bounded publication prefix contains no later frames, checkpoints or tail.
// Sequences are publication order within one namespace, not wall-clock dates.
class NativeSeriesPrefix {
public:
    static NativeSeriesPrefix capture(const NativeTemporalSeries& history,std::uint64_t cutoff) {
        const auto source=history;
        if(cutoff<source.first_sequence() || cutoff>source.last_sequence())
            throw std::out_of_range("native temporal cutoff outside history");
        const auto first=source.at(source.first_sequence(),true);
        NativeTemporalSeries prefix(first.endpoint(),source.source_namespace(),source.limits());
        const auto count=cutoff-source.first_sequence();
        for(std::uint64_t i=0;i<count;++i)prefix.append_encoded(source.segment_bytes(static_cast<std::size_t>(i)));
        return NativeSeriesPrefix(std::move(prefix));
    }
    std::uint64_t source_namespace() const {return prefix_.source_namespace();}
    std::uint64_t cutoff() const {return prefix_.last_sequence();}
    std::uint64_t first_sequence() const {return prefix_.first_sequence();}
    std::size_t segment_count() const {return prefix_.segment_count();}
    NativeSeriesEndpoint tail() const {return prefix_.tail();}
    // An independent immutable fork containing only this selected prefix.
    NativeTemporalSeries history() const {return prefix_;}
    NativeSeriesEndpoint at(std::uint64_t sequence) const {return prefix_.at(sequence);}
    NativeSeriesDifference difference(std::uint64_t first,std::uint64_t last) const {return prefix_.difference(first,last);}
    std::span<const std::byte> segment_bytes(std::size_t index) const & {return prefix_.segment_bytes(index);}
    std::span<const std::byte> segment_bytes(std::size_t) const &&=delete;
private:
    explicit NativeSeriesPrefix(NativeTemporalSeries prefix):prefix_(std::move(prefix)){}
    NativeTemporalSeries prefix_;
};
} // namespace rfc::storage
