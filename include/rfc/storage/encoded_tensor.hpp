#pragma once

#include "rfc/storage/temporal_codec_primitives.hpp"
#include "rfc/numeric/prepared_sparse_map.hpp"
#include <memory>
#include <set>

namespace rfc::storage {

class NativeEncodedTensor;

struct EncodedTensorReadStats {
    std::size_t visited_entries{}, decoded_coefficients{};
};

// An owning reader of the existing temporal tensor encoding. Structural support
// is independent of numerical nonzeros. No coefficient family or seek index is
// retained; sorted selections scan encoded entries and decode requested values.
class EncodedRationalTensor {
    struct State {
        std::shared_ptr<const void> owner;
        std::span<const std::byte> bytes;
        TemporalReadLimits limits;
        std::size_t values_offset{}, nonzeros{};
        temporal_codec_detail::TensorHeader header;

        State(std::shared_ptr<const void> storage, std::span<const std::byte> input,
              TemporalReadLimits policy, bool validate_values = true)
            : owner(std::move(storage)), bytes(input), limits(std::move(policy)),
              header(read_header()) {
            using namespace temporal_codec_detail;
            Reader r(bytes.subspan(values_offset), limits);
            r.consume_records(header.domain.rank());
            if (!header.encoded_full) r.consume_records(header.support.count());
            nonzeros = r.count(18);
            values_offset += r.position();
            // The native factory inherits immutable component admission and
            // checks every value against the selected publication itself.
            if (!validate_values) return;
            std::optional<std::size_t> previous;
            for (std::size_t i = 0; i < nonzeros; ++i) {
                const auto index = r.u64();
                require(index < header.domain.logical_size() && (!previous || index > *previous),
                        "unordered/out-of-domain sparse coordinate");
                const auto value = r.rational();
                require(value != 0, "explicit numerical zero in sparse lane");
                require(header.support.contains(static_cast<std::size_t>(index)),
                        "tensor value lies outside structural support");
                previous = static_cast<std::size_t>(index);
            }
            require(r.remaining() == 0, "trailing encoded tensor bytes");
        }
        temporal_codec_detail::TensorHeader read_header() {
            temporal_codec_detail::Reader r(bytes, limits);
            auto h = temporal_codec_detail::tensor_header(r);
            temporal_codec_detail::require(!limits.require_canonical || h.encoded_full ||
                h.support.count() != h.domain.logical_size(), "noncanonical full tensor support");
            values_offset = r.position();
            return h;
        }
    };
    std::shared_ptr<const State> state_;
    friend class NativeEncodedTensor;
    EncodedRationalTensor(std::shared_ptr<const void> owner,
                          std::span<const std::byte> bytes, TemporalReadLimits limits)
        : state_(std::make_shared<const State>(std::move(owner), bytes, std::move(limits), false)) {}
    const State& state() const {
        if (!state_) throw std::logic_error("moved-from encoded tensor");
        return *state_;
    }
public:
    // Caller buffers can have mutable aliases. Keep an independent byte owner.
    explicit EncodedRationalTensor(std::span<const std::byte> bytes,
                                  TemporalReadLimits limits = {}) {
        temporal_codec_detail::require(bytes.size() <= limits.max_file_bytes,
                                      "encoded tensor exceeds byte limit");
        auto owner = std::make_shared<const std::vector<std::byte>>(bytes.begin(), bytes.end());
        state_ = std::make_shared<const State>(owner, std::span<const std::byte>(*owner), std::move(limits));
    }
    const numeric::IndexDomain& domain() const & { return state().header.domain; }
    const numeric::IndexDomain& domain() const && = delete;
    const numeric::StructuralSupport& support() const & { return state().header.support; }
    const numeric::StructuralSupport& support() const && = delete;
    std::span<const std::byte> encoded_bytes() const & { return state().bytes; }
    std::span<const std::byte> encoded_bytes() const && = delete;
    std::span<const std::byte> encoded_value_bytes() const & {
        return state().bytes.subspan(state().values_offset - 8);
    }
    std::span<const std::byte> encoded_value_bytes() const && = delete;
    std::size_t stored_nonzeros() const { return state().nonzeros; }

    numeric::SparseRationalTensor select(std::span<const std::size_t> coordinates,
                                        EncodedTensorReadStats* stats = nullptr,
                                        const numeric::SparseMapCheck& check = {}) const {
        using namespace temporal_codec_detail;
        const auto& s = state();
        if (stats) *stats = {};
        std::vector<std::size_t> active;
        for (std::size_t i = 0; i < coordinates.size(); ++i) {
            if (check) check();
            require(coordinates[i] < s.header.domain.logical_size() &&
                    (i == 0 || coordinates[i] > coordinates[i - 1]),
                    "encoded tensor selection requires sorted unique coordinates");
            if (s.header.support.contains(coordinates[i])) active.push_back(coordinates[i]);
        }
        std::vector<numeric::SparseRationalEntry> values;
        Reader r(s.bytes.subspan(s.values_offset), s.limits);
        std::size_t selected = 0;
        for (std::size_t i = 0; i < s.nonzeros && selected < coordinates.size(); ++i) {
            if (check) check();
            const auto index = static_cast<std::size_t>(r.u64());
            if (stats) ++stats->visited_entries;
            while (selected < coordinates.size() && coordinates[selected] < index) ++selected;
            if (selected == coordinates.size()) break;
            if (index == coordinates[selected]) {
                values.push_back({index, r.rational()});
                if (stats) ++stats->decoded_coefficients;
                ++selected;
            } else {
                (void)r.integer_bytes(); (void)r.integer_bytes();
            }
        }
        return numeric::SparseRationalTensor(s.header.domain,
            numeric::StructuralSupport::sparse(s.header.domain.logical_size(), std::move(active)),
            numeric::SparseRationalVector(s.header.domain.logical_size(), std::move(values)));
    }
    std::optional<numeric::Rational> value(std::size_t index) const {
        if (!support().contains(index)) return std::nullopt;
        return select(std::span<const std::size_t>(&index, 1)).state(index);
    }
    template<class Function> void for_each_nonzero(Function&& function) const {
        const auto& s = state();
        temporal_codec_detail::Reader r(s.bytes.subspan(s.values_offset), s.limits);
        for (std::size_t i = 0; i < s.nonzeros; ++i) {
            const auto index = static_cast<std::size_t>(r.u64());
            const auto value = r.rational();
            function(index, value);
        }
    }
    numeric::SparseRationalTensor materialize() const {
        const auto& s = state();
        std::vector<numeric::SparseRationalEntry> entries;
        entries.reserve(s.nonzeros);
        for_each_nonzero([&](std::size_t i, const numeric::Rational& value) {
            entries.push_back({i, value});
        });
        return numeric::SparseRationalTensor(s.header.domain, s.header.support,
            numeric::SparseRationalVector(s.header.domain.logical_size(), std::move(entries)));
    }
    // Numerical application only; the scientific consumer owns carrier maps.
    numeric::SparseRationalVector apply(const numeric::PreparedSparseExactMap& map,
                                       EncodedTensorReadStats* stats = nullptr,
                                       const numeric::SparseMapCheck& check = {}) const {
        temporal_codec_detail::require(map.matrix().cols() == domain().logical_size(),
                                      "encoded tensor map input size");
        std::set<std::size_t> required;
        for (const auto& [coordinate, coefficient] : map.matrix().entries()) {
            (void)coefficient;
            if (check) check();
            required.insert(coordinate.second);
        }
        const std::vector<std::size_t> coordinates(required.begin(), required.end());
        const auto selected = select(coordinates, stats, check);
        return map.apply(selected.values(), check);
    }
};
} // namespace rfc::storage
