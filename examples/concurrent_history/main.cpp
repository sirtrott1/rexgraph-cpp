#include "rfc/storage/concurrent_native_history.hpp"
#include <future>
#include <iostream>

namespace {
rfc::storage::TemporalEndpoint endpoint(std::uint64_t sequence) {
    using namespace rfc;
    core::RelationSignature signature;
    const auto pair = signature.add_type("retained-pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{10}, pair, {
        {1, true, false, numeric::Rational{0}, {}, core::OccurrenceKey{100}},
        {2, false, true, numeric::Rational{0}, {}, core::OccurrenceKey{101}}});
    storage::TemporalEndpoint result(builder.freeze(), sequence);
    result.carriers.push_back({1, storage::TemporalCarrierKind::SourceGrade, core::Grade{1}, {}, 0, numeric::IndexDomain::linear(1)});
    storage::TemporalTensor field; field.key = 10; field.output = 1; field.role = storage::TemporalTensorRole::Field;
    const auto values = sequence ? std::vector<numeric::SparseRationalEntry>{{0, numeric::Rational{static_cast<long>(sequence)}}} :
                                  std::vector<numeric::SparseRationalEntry>{};
    field.tensor = numeric::SparseRationalTensor(numeric::IndexDomain::linear(1), numeric::StructuralSupport::full(1),
                                                 numeric::SparseRationalVector(1, values));
    result.tensors.push_back(std::move(field)); return result;
}
}
int main(int argc, char** argv) {
    using namespace rfc::storage;
    try {
        if (argc != 2) throw std::invalid_argument("usage: concurrent_history NEW_STORE_DIRECTORY");
        auto service = ConcurrentNativeHistory::create(argv[1], endpoint(0), 700);
        const auto original = service.snapshot(); std::atomic<bool> finished{};
        auto reader = std::async(std::launch::async, [&] {
            std::size_t reads{};
            while (!finished.load() || reads < 100) {
                const auto snapshot = service.snapshot(); const auto root = snapshot.root(); const auto pin = snapshot.tail();
                if (pin.sequence() != root.sequence || snapshot.series().segment_count() != root.generation ||
                    pin.endpoint().tensors[0].tensor.state(0) != rfc::numeric::Rational{static_cast<long>(root.sequence)})
                    throw std::runtime_error("incoherent reader");
                ++reads;
            }
            return reads;
        });
        try {
            for (std::uint64_t sequence = 1; sequence <= 32; ++sequence) {
                const auto expected = service.snapshot(); const auto before = expected.tail();
                const std::vector<std::vector<std::byte>> frames{
                    encode_temporal_segment(make_temporal_segment(before.endpoint(), endpoint(sequence)))};
                (void)service.append(sequence, expected, frames);
            }
        } catch (...) { finished.store(true); (void)reader.get(); throw; }
        finished.store(true); const auto count = reader.get();
        if (original.root().sequence != 0 || original.series().segment_count() != 0) throw std::runtime_error("old snapshot changed");
        service.recover();
        std::cout << "Published and recovered " << service.snapshot().root().generation << " transactions; "
                  << count << " coherent reader observations; original snapshot retained\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
