#pragma once

#include "rfc/storage/encoded_tensor.hpp"
#include "rfc/storage/native_series.hpp"

namespace rfc::storage {

// A field reader tied to an actual admitted publication. Encoded storage can
// precede that publication only when the complete tensor is unchanged.
class NativeEncodedTensor {
    NativeSeriesEndpoint publication_;
    EncodedRationalTensor tensor_;
    std::uint64_t encoding_sequence_;
    NativeEncodedTensor(NativeSeriesEndpoint publication, EncodedRationalTensor tensor,
                        std::uint64_t sequence)
        : publication_(std::move(publication)), tensor_(std::move(tensor)),
          encoding_sequence_(sequence) {}

    // These readers traverse immutable components already admitted by the
    // series codec. Skipping a coefficient is not admission of untrusted data.
    static void skip_tensor(temporal_codec_detail::Reader& r) {
        (void)r.domain();
        if (!r.flag()) {
            const auto n = r.count(8);
            for (std::size_t i = 0; i < n; ++i) (void)r.u64();
        }
        const auto n = r.count(18);
        for (std::size_t i = 0; i < n; ++i) {
            (void)r.u64(); (void)r.integer_bytes(); (void)r.integer_bytes();
        }
    }
    static void skip_local(temporal_codec_detail::Reader& r) {
        using namespace temporal_codec_detail;
        (void)r.u64(); (void)r.u8();
        auto n = r.count(10);
        for (std::size_t i = 0; i < n; ++i) (void)cell(r);
        n = r.count(10);
        for (std::size_t i = 0; i < n; ++i) (void)occurrence(r);
        n = r.count();
        for (std::size_t i = 0; i < n; ++i) {
            (void)r.u64(); (void)r.u8();
            if (r.flag()) (void)cell(r);
            if (r.flag()) (void)occurrence(r);
            skip_tensor(r);
        }
    }
    static std::optional<EncodedRationalTensor> find(
        const NativeSeriesComponent& component, const TemporalTensor& expected,
        const TemporalReadLimits& limits) {
        using namespace temporal_codec_detail;
        const auto bytes = std::span<const std::byte>(*component.bytes);
        // RFCXE v1 has a 32 byte envelope followed by four 64 bit bindings.
        Reader r(bytes.subspan(32), limits);
        (void)r.u64(); (void)r.u64(); (void)r.u64(); (void)r.u64();
        auto n = r.count();
        for (std::size_t i = 0; i < n; ++i) skip_local(r);
        n = r.count();
        for (std::size_t i = 0; i < n; ++i) (void)carrier(r);
        n = r.count();
        for (std::size_t i = 0; i < n; ++i) {
            auto descriptor = tensor_record_header(r);
            if (descriptor.key > expected.key) return std::nullopt;
            const auto begin = 32 + r.position();
            skip_tensor(r);
            if (descriptor.key != expected.key) continue;
            EncodedRationalTensor tensor(component.bytes,
                bytes.subspan(begin, 32 + r.position() - begin), limits);
            descriptor.tensor = numeric::SparseRationalTensor(tensor.domain(), tensor.support(),
                numeric::SparseRationalVector(tensor.domain().logical_size(), {}));
            if (!temporal_endpoint_detail::metadata_equal(descriptor, expected) ||
                tensor.stored_nonzeros() != expected.tensor.values().nonzeros()) return std::nullopt;
            if (const auto retained = std::dynamic_pointer_cast<const RetainedTemporalValues>(
                    expected.tensor.values().retained_source())) {
                const auto actual = tensor.encoded_value_bytes(), original = retained->encoded_bytes();
                if (actual.size() == original.size() && std::equal(actual.begin(), actual.end(), original.begin()))
                    return tensor;
                return std::nullopt;
            }
            bool same = true;
            std::size_t entry = 0;
            tensor.for_each_nonzero([&](std::size_t index, const numeric::Rational& value) {
                const auto& original = expected.tensor.values().entries()[entry++];
                if (index != original.index || value != original.value) same = false;
            });
            if (same) return tensor;
            return std::nullopt;
        }
        return std::nullopt;
    }
public:
    // Returns no reader if the selected tensor has no matching retained base or
    // checkpoint encoding. It does not create a checkpoint or reencode a field.
    static std::optional<NativeEncodedTensor> from_series(
        const NativeTemporalSeries& series, std::uint64_t key,
        std::optional<std::uint64_t> cutoff = std::nullopt) {
        const auto history = series;
        auto publication = !cutoff || *cutoff == history.last_sequence()
            ? history.tail() : history.at(*cutoff);
        const TemporalTensor* expected = nullptr;
        for (const auto& tensor : publication.endpoint().tensors)
            if (tensor.key == key) { expected = &tensor; break; }
        if (!expected) throw std::out_of_range("tensor not present at selected publication");
        std::vector<NativeSeriesComponent> candidates;
        history.visit_components([&](const NativeSeriesComponent& component) {
            if ((component.kind == NativeSeriesComponentKind::BaseChannels ||
                 component.kind == NativeSeriesComponentKind::CheckpointChannels) &&
                component.sequence <= publication.sequence()) candidates.push_back(component);
        });
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return a.sequence > b.sequence;
        });
        for (const auto& component : candidates)
            if (auto tensor = find(component, *expected, history.limits().temporal))
                return NativeEncodedTensor(std::move(publication), std::move(*tensor), component.sequence);
        return std::nullopt;
    }
    const NativeSeriesEndpoint& publication() const & { return publication_; }
    const NativeSeriesEndpoint& publication() const && = delete;
    const EncodedRationalTensor& tensor() const & { return tensor_; }
    const EncodedRationalTensor& tensor() const && = delete;
    std::uint64_t encoding_sequence() const noexcept { return encoding_sequence_; }
};
} // namespace rfc::storage
