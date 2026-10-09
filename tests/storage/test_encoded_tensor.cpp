#include "rfc/storage/native_encoded_tensor.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include "native_accession_fixture.hpp"
#include <future>
#include <iostream>
#include <random>

namespace {
using namespace rfc;
using namespace rfc::storage;
using Q = numeric::Rational;
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F&& f) {
    bool failed = false;
    try { f(); } catch (const std::exception&) { failed = true; }
    check(failed, "invalid encoded tensor accepted");
}
std::vector<std::byte> encode(const numeric::SparseRationalTensor& tensor) {
    temporal_codec_detail::Writer w; temporal_codec_detail::tensor(w, tensor);
    return std::move(w).take_bytes();
}
void tensors() {
    std::mt19937_64 random(741);
    for (std::size_t trial = 0; trial < 180; ++trial) {
        const std::size_t size = 1 + random() % 71;
        std::vector<std::size_t> support, selected;
        std::vector<numeric::SparseRationalEntry> values;
        for (std::size_t i = 0; i < size; ++i) {
            if (random() % 3) {
                support.push_back(i);
                if (random() % 3) {
                    Q value(numeric::Integer(std::to_string(static_cast<long>(random() % 99) - 49)),
                            numeric::Integer(std::to_string(1 + random() % 17)));
                    value.canonicalize();
                    if (value != 0) values.push_back({i, value});
                }
            }
            if (random() % 2) selected.push_back(i);
        }
        const numeric::SparseRationalTensor original(numeric::IndexDomain({1, size}),
            numeric::StructuralSupport::sparse(size, support), numeric::SparseRationalVector(size, values));
        auto bytes = encode(original);
        const auto saved = bytes;
        EncodedRationalTensor reader(bytes);
        std::fill(bytes.begin(), bytes.end(), std::byte{0});
        check(std::ranges::equal(reader.encoded_bytes(), saved), "mutable caller alias retained");
        check(temporal_endpoint_detail::tensor_equal(reader.materialize(), original), "materialization differs");
        EncodedTensorReadStats stats;
        const auto selection = reader.select(selected, &stats);
        std::size_t decoded{};
        for (std::size_t i = 0; i < size; ++i) {
            check(reader.value(i) == original.state(i), "scalar state differs");
            const bool requested = std::binary_search(selected.begin(), selected.end(), i);
            check(selection.state(i) == (requested ? original.state(i) : std::nullopt), "selection support differs");
            if (requested && original.state(i).value_or(Q{0}) != 0) ++decoded;
        }
        check(stats.decoded_coefficients == decoded, "unrequested coefficient decoded");
        numeric::ExactMatrix matrix(3, size);
        for (std::size_t i = 0; i < size; ++i)
            if (i % 7 == 0) { matrix.set(0, i, Q{1, 3}); matrix.set(2, i, Q{-2, 5}); }
        const numeric::PreparedSparseExactMap map(matrix);
        check(temporal_endpoint_detail::values_equal(reader.apply(map), map.apply(original.values())),
              "native sparse map application differs");
        rejects([&] { (void)reader.select(std::vector<std::size_t>{size}); });
        rejects([&] { (void)reader.select(std::vector<std::size_t>{0, 0}); });
        if (!selected.empty())
            rejects([&] { (void)reader.select(selected, nullptr, [] { throw std::runtime_error("cancelled"); }); });
    }
    const auto huge = std::size_t{8000000001ULL};
    TemporalReadLimits limits; limits.max_dimension = huge;
    numeric::Integer numerator = 1; numerator <<= 321; numerator += 7;
    Q large(numerator, numeric::Integer{37}); large.canonicalize();
    const numeric::SparseRationalTensor big(numeric::IndexDomain::linear(huge),
        numeric::StructuralSupport::full(huge), numeric::SparseRationalVector(huge, {{huge - 1, large}}));
    const EncodedRationalTensor reader(encode(big), limits);
    check(reader.support().count() == huge && reader.stored_nonzeros() == 1, "implicit full support expanded");
    check(reader.value(0) == Q{0} && reader.value(huge - 1) == large, "large domain or integer truncated");
    std::vector<std::future<Q>> workers;
    for (std::size_t i = 0; i < 8; ++i)
        workers.push_back(std::async(std::launch::async, [reader, huge] { return *reader.value(huge - 1); }));
    for (auto& worker : workers) check(worker.get() == large, "parallel immutable read differs");
    auto moved = reader;
    auto destination = std::move(moved);
    rejects([&] { (void)moved.stored_nonzeros(); });
    check(destination.value(huge - 1) == large, "moved reader lost owner");
    const numeric::SparseRationalTensor empty(numeric::IndexDomain::linear(0),
        numeric::StructuralSupport::full(0), numeric::SparseRationalVector(0, {}));
    check(EncodedRationalTensor(encode(empty)).materialize().domain().logical_size() == 0, "empty tensor refused");
}
void malformed() {
    const numeric::SparseRationalTensor tensor(numeric::IndexDomain::linear(3),
        numeric::StructuralSupport::sparse(3, {0, 2}), numeric::SparseRationalVector(3, {{2, Q{-5, 11}}}));
    const auto bytes = encode(tensor);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        rejects([&] { (void)EncodedRationalTensor(std::span<const std::byte>(bytes).first(i)); });
    auto trailing = bytes; trailing.push_back(std::byte{0});
    rejects([&] { (void)EncodedRationalTensor(trailing); });
    TemporalReadLimits limits; limits.max_file_bytes = bytes.size() - 1;
    rejects([&] { (void)EncodedRationalTensor(bytes, limits); });
    limits = {}; limits.max_records = 1;
    rejects([&] { (void)EncodedRationalTensor(bytes, limits); });
    limits.max_records = 3;
    rejects([&] { (void)EncodedRationalTensor(bytes, limits); });
    const auto custom = [](std::uint64_t index, bool sign, std::uint8_t numerator,
                           std::uint8_t denominator, bool explicit_full = false) {
        temporal_codec_detail::Writer w;
        w.u64(1); w.u64(3); w.u8(!explicit_full);
        if (explicit_full) { w.u64(3); w.u64(0); w.u64(1); w.u64(2); }
        w.u64(1); w.u64(index);
        w.u8(sign); w.u32(numerator == 0 ? 0 : 1); if (numerator) w.u8(numerator);
        w.u8(0); w.u32(denominator == 0 ? 0 : 1); if (denominator) w.u8(denominator);
        return std::move(w).take_bytes();
    };
    rejects([&] { (void)EncodedRationalTensor(custom(0, false, 2, 4)); });
    rejects([&] { (void)EncodedRationalTensor(custom(0, false, 1, 0)); });
    rejects([&] { (void)EncodedRationalTensor(custom(0, true, 0, 1)); });
    rejects([&] { (void)EncodedRationalTensor(custom(0, false, 0, 1)); });
    rejects([&] { (void)EncodedRationalTensor(custom(3, false, 1, 1)); });
    rejects([&] { (void)EncodedRationalTensor(custom(0, false, 1, 1, true)); });
}
void publications() {
    auto endpoint = native_accession_fixture::endpoint();
    LocalExactTensorPayload payload; payload.payload_key = 90; payload.binding = LocalPayloadBinding::Cell;
    payload.cell = StableCellKey{core::Grade{1}, 1000}; payload.tensor = endpoint.tensors.back().tensor;
    endpoint.local_blocks.front().payloads.push_back(payload);
    NativeTemporalSeries history(endpoint, 871);
    const auto before = encode_native_series(history);
    const auto field = NativeEncodedTensor::from_series(history, 200);
    check(field.has_value() && field->publication().pin() == history.tail().pin(), "actual publication not retained");
    check(field->publication().source_namespace() == 871 && field->tensor().value(2) == Q{0} &&
          !field->tensor().value(1), "source, zero or absence lost");
    check(encode_native_series(history) == before, "reader modified history");
    auto changed = endpoint; ++changed.sequence;
    changed.tensors[3].tensor = numeric::SparseRationalTensor(numeric::IndexDomain({2, 3}),
        numeric::StructuralSupport::sparse(6, {0, 2, 5}), numeric::SparseRationalVector(6, {{0, Q{1, 3}}, {5, Q{9}}}));
    history.append_target(changed, {});
    check(!NativeEncodedTensor::from_series(history, 200), "old values substituted for changed field");
    check(NativeEncodedTensor::from_series(history, 101)->publication().sequence() == changed.sequence,
          "unchanged field bound to old publication");
    history.checkpoint(changed.sequence);
    const auto current = NativeEncodedTensor::from_series(history, 200);
    check(current && current->encoding_sequence() == changed.sequence && current->tensor().value(5) == Q{9},
          "checkpoint field unavailable");
    const auto old = NativeEncodedTensor::from_series(history, 200, endpoint.sequence);
    check(old && old->tensor().value(5) == Q{-2, 7} && old->encoding_sequence() == endpoint.sequence,
          "future checkpoint used for historical field");
    rejects([&] { (void)NativeEncodedTensor::from_series(history, 999); });
    rejects([&] { (void)NativeEncodedTensor::from_series(history, 200, changed.sequence + 1); });
    auto metadata = changed; ++metadata.sequence; metadata.tensors[3].interpretation = "new measurement";
    history.append_target(metadata, {});
    check(!NativeEncodedTensor::from_series(history, 200), "equal values identified different metadata");
    history.repack_checkpoints({});
    check(current->tensor().value(5) == Q{9} && field->tensor().value(5) == Q{-2, 7}, "old byte owner lost");
    const auto detached = [] {
        NativeTemporalSeries local(native_accession_fixture::endpoint(), 912);
        return *NativeEncodedTensor::from_series(local, 200);
    }();
    check(detached.tensor().value(0) == Q{1, 3} && detached.publication().source_namespace() == 912,
          "detached reader lost source ownership");
}
}
int main() {
    try { tensors(); malformed(); publications(); std::cout << checks << " encoded tensor checks passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
