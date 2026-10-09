#include <rfc/storage/temporal_codec_primitives.hpp>
#include <iostream>
#include <map>
#include <random>

namespace {
using namespace rfc;
using namespace numeric;
std::size_t checks{};
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class Function> void rejects(Function action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "invalid sparse coordinates accepted");
}
}
int main() {
    std::vector<SparseRationalEntry> ordered{{1, Rational(2, 4)}, {4, Rational(-10, 15)}};
    const auto* original = ordered.data();
    const bool sized = ordered.capacity() == ordered.size();
    const SparseRationalVector retained(6, std::move(ordered));
    check(retained.entries() == std::vector<SparseRationalEntry>{{1, Rational(1, 2)}, {4, Rational(-2, 3)}},
          "exact coefficient normalization changed");
    if (sized) check(retained.entries().data() == original, "sized canonical allocation was replaced");
    auto oversized = retained.entries();
    oversized.reserve(oversized.size() + 1000);
    const auto capacity = oversized.capacity();
    const SparseRationalVector compact(6, std::move(oversized));
    check(compact.entries() == retained.entries(), "oversized normalization changed values");
    check(compact.entries().capacity() < capacity, "unused caller capacity retained");
    const SparseRationalVector cancelled(8, {{5, Rational(3)}, {1, Rational(1, 2)},
                                            {5, Rational(-3)}, {1, Rational(1, 3)}, {4, Rational(0)}});
    check(cancelled.entries() == std::vector<SparseRationalEntry>{{1, Rational(5, 6)}},
          "duplicate summation or cancellation changed");
    check(SparseRationalVector(0, {}).empty(), "empty domain changed");
    rejects([] { (void)SparseRationalVector(0, {{0, Rational(0)}}); });
    rejects([] { (void)SparseRationalVector(6, {{6, Rational(1)}}); });

    std::mt19937 random(92381);
    for (std::size_t trial = 0; trial < 1024; ++trial) {
        const std::size_t domain = 1 + random() % 65;
        std::vector<SparseRationalEntry> entries;
        std::map<std::size_t, Rational> sums;
        const std::size_t count = random() % 130;
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t index = random() % domain;
            Rational value(static_cast<int>(random() % 17) - 8, 1 + random() % 13);
            value.canonicalize();
            entries.push_back({index, value});
            sums[index] += value;
        }
        std::vector<SparseRationalEntry> expected;
        for (const auto& [index, value] : sums)
            if (value != 0) expected.push_back({index, value});
        const SparseRationalVector result(domain, std::move(entries));
        check(result.entries() == expected, "independent exact normalization differs");
    }

    std::vector<SparseRationalEntry> entries;
    entries.reserve(30001);
    for (std::size_t i = 0; i < 30001; ++i) entries.push_back({2 * i, Rational(1, 3)});
    const SparseRationalVector input(60002, std::move(entries));
    storage::temporal_codec_detail::Writer writer;
    storage::temporal_codec_detail::values(writer, input);
    storage::TemporalReadLimits limits;
    limits.max_records = input.nonzeros();
    storage::temporal_codec_detail::Reader reader(writer.bytes(), limits);
    const auto decoded = storage::temporal_codec_detail::values(reader, input.size());
    check(decoded.entries() == input.entries() && reader.remaining() == 0, "complete sparse codec differs");
    check(decoded.entries().capacity() == decoded.nonzeros(), "decoder retains geometric entry capacity");
    storage::temporal_codec_detail::Writer roundtrip;
    storage::temporal_codec_detail::values(roundtrip, decoded);
    check(roundtrip.bytes() == writer.bytes(), "canonical sparse bytes changed");
    --limits.max_records;
    rejects([&] {
        storage::temporal_codec_detail::Reader refused(writer.bytes(), limits);
        (void)storage::temporal_codec_detail::values(refused, input.size());
    });
    storage::temporal_codec_detail::Writer malformed;
    malformed.u64(2);
    malformed.u64(1); storage::temporal_codec_detail::rational(malformed, Rational(1));
    malformed.u64(1); storage::temporal_codec_detail::rational(malformed, Rational(2));
    rejects([&] {
        storage::temporal_codec_detail::Reader refused(malformed.bytes(), limits);
        (void)storage::temporal_codec_detail::values(refused, 3);
    });
    for (const auto& denominator : {Integer(4), Integer(0), Integer(-4)}) {
        storage::temporal_codec_detail::Writer invalid_ratio;
        storage::rfcx_detail::write_bigint(invalid_ratio, Integer(2));
        storage::rfcx_detail::write_bigint(invalid_ratio, denominator);
        rejects([&] {
            storage::temporal_codec_detail::Reader refused(invalid_ratio.bytes(), limits);
            (void)refused.rational();
        });
    }
    const SparseRationalTensor supported(IndexDomain({4}), StructuralSupport::sparse(4, {0, 1, 3}),
                                        SparseRationalVector(4, {{0, Rational(1)}, {3, Rational(2)}}));
    storage::temporal_codec_detail::Writer tensor_bytes;
    storage::temporal_codec_detail::tensor(tensor_bytes, supported);
    storage::temporal_codec_detail::Reader tensor_reader(tensor_bytes.bytes(), limits);
    const auto restored = storage::temporal_codec_detail::tensor(tensor_reader);
    check(restored.state(1) == Rational(0) && !restored.state(2), "supported zero or absence changed");
    storage::temporal_codec_detail::Writer restored_bytes;
    storage::temporal_codec_detail::tensor(restored_bytes, restored);
    check(restored_bytes.bytes() == tensor_bytes.bytes(), "structural support bytes changed");
    std::cout << checks << " sparse storage checks passed\n";
}
