#include "native_accession_fixture.hpp"
#include "native_series_fixture.hpp"
#include "rfc/storage/native_encoded_tensor.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include <future>
#include <iostream>
#include <random>
using namespace rfc;
using namespace rfc::storage;
namespace {
std::size_t checks{};
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F &&function) {
    bool failed = false;
    try {
        function();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "invalid retained publication accepted");
}
NativeSeriesLimits limits() {
    NativeSeriesLimits p;
    p.temporal.retain_encoded_values = true;
    return p;
}
void varied() {
    std::mt19937_64 random(1467);
    for (std::size_t trial = 0; trial < 150; ++trial) {
        auto original = native_accession_fixture::endpoint();
        original.tensors.clear();
        const auto size = std::size_t{1} + random() % 71;
        std::vector<std::size_t> support;
        std::vector<numeric::SparseRationalEntry> entries;
        for (std::size_t i = 0; i < size; ++i)
            if (random() % 3) {
                support.push_back(i);
                if (random() % 3) {
                    numeric::Rational q(
                        numeric::Integer(std::to_string(static_cast<long>(random() % 199) - 99)),
                        numeric::Integer(std::to_string(1 + random() % 27)));
                    q.canonicalize();
                    if (q != 0)
                        entries.push_back({i, q});
                }
            }
        TemporalTensor field;
        field.key = 91;
        field.interpretation = "stored assay values";
        field.tensor = numeric::SparseRationalTensor(
            numeric::IndexDomain({1, size}), numeric::StructuralSupport::sparse(size, support),
            numeric::SparseRationalVector(size, entries));
        original.tensors.push_back(field);
        field.key = 92;
        original.tensors.push_back(field);
        const NativeTemporalSeries eager(original, 700);
        const auto bytes = encode_native_series(eager);
        auto input = bytes;
        const auto retained = decode_native_series(input, limits());
        std::fill(input.begin(), input.end(), std::byte{0});
        auto publication = retained.tail();
        const auto &a = publication.endpoint().tensors[0].tensor;
        const auto &b = publication.endpoint().tensors[1].tensor;
        check(a.values().retained_source() && !a.values().materialized_entries(),
              "initial field materialized");
        const auto provider =
            std::dynamic_pointer_cast<const temporal_codec_detail::RetainedTemporalValues>(
                a.values().retained_source());
        const auto other =
            std::dynamic_pointer_cast<const temporal_codec_detail::RetainedTemporalValues>(
                b.values().retained_source());
        check(provider->owner() == other->owner(), "fields retain duplicate channel buffers");
        retained.visit_components([&](const auto &part) {
            if (part.kind == NativeSeriesComponentKind::BaseChannels)
                check(part.bytes == provider->owner(),
                      "snapshot and fields have different byte owners");
        });
        for (std::size_t i = 0; i < size; ++i)
            check(a.streamed_state(i) == field.tensor.state(i),
                  "zero, absence or signed value differs");
        check(temporal_endpoint_detail::tensor_equal(a, field.tensor), "full values differ");
        check(encode_native_series(retained) == bytes && retained.same_history(eager),
              "complete history differs");
        const auto selected = NativeEncodedTensor::from_series(retained, 91);
        check(selected && selected->publication().source_namespace() == 700,
              "actual publication lost");
        check(temporal_endpoint_detail::tensor_equal(selected->tensor().materialize(), a),
              "reader field differs");
        check(!a.values().materialized_entries(),
              "coordinate, comparison, serialization or selected read expanded source");
        auto fork = a.values();
        std::vector<std::future<const void *>> workers;
        for (std::size_t i = 0; i < 8; ++i)
            workers.push_back(std::async(
                std::launch::async, [fork] { return static_cast<const void *>(&fork.entries()); }));
        for (auto &worker : workers)
            check(worker.get() == &a.values().entries(), "concurrent caches differ");
        check(a.values().entries() == field.tensor.values().entries(),
              "contiguous consumer differs");
        check(a.values().materialized_entries() != nullptr, "materialization state missing");
        check(!b.values().materialized_entries(), "unrequested field expanded");
        if (!support.empty()) {
            check(b.state(support.front()) == field.tensor.state(support.front()),
                  "indexed state differs");
            check(b.values().materialized_entries() != nullptr,
                  "indexed state failed to retain a cache");
        }
    }
}
void malformed() {
    using namespace temporal_codec_detail;
    Writer w;
    w.u64(1);
    w.u64(3);
    w.u8(0);
    w.u64(2);
    w.u64(0);
    w.u64(2);
    w.u64(1);
    w.u64(2);
    rational(w, numeric::Rational{-5, 11});
    const auto original = w.bytes();
    auto policy = limits().temporal;
    for (std::size_t size = 0; size < original.size(); ++size)
        rejects([&] {
            Reader r(std::span<const std::byte>(original).first(size), policy, true);
            (void)tensor(r);
        });
    policy.max_records = 3;
    rejects([&] {
        Reader r(original, policy, true);
        (void)tensor(r);
    });
    policy = limits().temporal;
    policy.max_file_bytes = original.size() - 1;
    rejects([&] {
        Reader r(original, policy, true);
        (void)tensor(r);
    });
    policy = limits().temporal;
    auto invalid = original;
    invalid[49] = std::byte{1};
    rejects([&] {
        Reader r(invalid, policy, true);
        (void)tensor(r);
    });
    for (unsigned kind = 0; kind < 3; ++kind) {
        Writer bad;
        bad.u64(1);
        bad.u64(1);
        bad.u8(1);
        bad.u64(1);
        bad.u64(0);
        if (kind == 0)
            rational(bad, 0);
        else if (kind == 1) {
            bad.u8(1);
            bad.u32(0);
            bad.u8(0);
            bad.u32(1);
            bad.u8(1);
        } else {
            bad.u8(0);
            bad.u32(1);
            bad.u8(2);
            bad.u8(0);
            bad.u32(1);
            bad.u8(4);
        }
        rejects([&] {
            Reader r(bad.bytes(), policy, true);
            (void)tensor(r);
        });
    }
    // Valid grammar and unsupported value are independently refused.
    Writer unsupported;
    unsupported.u64(1);
    unsupported.u64(3);
    unsupported.u8(0);
    unsupported.u64(1);
    unsupported.u64(0);
    unsupported.u64(1);
    unsupported.u64(2);
    rational(unsupported, 1);
    rejects([&] {
        Reader r(unsupported.bytes(), policy, true);
        (void)tensor(r);
    });
    auto endpoint = native_accession_fixture::endpoint();
    const auto bytes = encode_native_series(NativeTemporalSeries(endpoint, 700));
    for (std::size_t i = 0; i < bytes.size(); ++i)
        rejects([&] {
            (void)decode_native_series(std::span<const std::byte>(bytes).first(i), limits());
        });
    auto corrupt = bytes;
    corrupt.back() ^= std::byte{1};
    rejects([&] { (void)decode_native_series(corrupt, limits()); });
}
void large_domain() {
    auto endpoint = native_accession_fixture::endpoint();
    endpoint.tensors.clear();
    const auto size = std::size_t{8000000001ULL};
    auto policy = limits();
    policy.temporal.max_dimension = size;
    numeric::Integer numerator = 1;
    numerator <<= 321;
    numerator += 7;
    numeric::Rational q(numerator, numeric::Integer{37});
    q.canonicalize();
    TemporalTensor t;
    t.key = 19;
    t.interpretation = "large numerical field";
    t.tensor = numeric::SparseRationalTensor(numeric::IndexDomain::linear(size),
                                             numeric::StructuralSupport::full(size),
                                             numeric::SparseRationalVector(size, {{size - 1, q}}));
    endpoint.tensors.push_back(t);
    const auto publication = NativeTemporalSeries(endpoint, 702, policy).tail();
    const auto &tensor = publication.endpoint().tensors[0].tensor;
    check(tensor.streamed_state(0) == numeric::Rational{0} && tensor.streamed_state(size - 1) == q,
          "large coordinate or coefficient truncated");
    check(tensor.support().count() == size && !tensor.values().materialized_entries(),
          "large implicit support or value expanded");
    auto cursor = tensor.values().retained_source()->open();
    numeric::SparseRationalEntry entry;
    check(cursor->next(entry) && entry.index == size - 1 && entry.value == q &&
              !cursor->next(entry),
          "large cursor differs");
}
void history() {
    native_series_fixture::PaperHistory fixture;
    NativeTemporalSeries original(fixture.a, 771, limits());
    const auto before = original.tail();
    original.append(fixture.first);
    original.append(fixture.second);
    original.checkpoint(41);
    original.checkpoint(42);
    const auto bytes = encode_native_series(original);
    const auto restored = decode_native_series(bytes, limits());
    for (std::uint64_t seq = 40; seq <= 42; ++seq) {
        const auto eager = original.at(seq);
        const auto retained = restored.at(seq);
        check(retained.sequence() == seq && retained.source_namespace() == 771,
              "historical identity differs");
        check(encode_temporal_endpoint(retained.endpoint()) ==
                  encode_temporal_endpoint(eager.endpoint()),
              "historical endpoint differs");
    }
    check(encode_native_series(restored) == bytes, "checkpoint archive changed");
    check(before.sequence() == 40 &&
              before.endpoint().tensors[0].tensor.streamed_state(0) == numeric::Rational{1},
          "later append changed old cutoff");
    const auto detached = [&] {
        const auto temporary = decode_native_series(bytes, limits());
        return temporary.at(40).pin();
    }();
    check(detached->tensors[0].tensor.state(1) == numeric::Rational{2},
          "detached cutoff lost source owner");
    rejects([&] { (void)restored.at(39); });
    rejects([&] { (void)restored.at(43); });
}
} // namespace
int main() {
    try {
        varied();
        malformed();
        large_domain();
        history();
        std::cout << checks << " retained publication checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
