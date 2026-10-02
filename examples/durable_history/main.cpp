#include "rfc/storage/native_durable_store.hpp"
#include <iostream>

int main(int argc, char** argv) {
    using namespace rfc;
    using namespace rfc::storage;
    try {
        if (argc != 2) throw std::invalid_argument("usage: durable_history NEW_STORE_DIRECTORY");
        core::RelationSignature signature;
        const auto relation = signature.add_type("retained-pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(core::RelationKey{10}, relation, {
            {1, true, false, numeric::Rational{0}, {}, core::OccurrenceKey{100}},
            {2, false, true, numeric::Rational{0}, {}, core::OccurrenceKey{101}}});
        const TemporalEndpoint first(builder.freeze(), 0);
        TemporalEndpoint second = first; second.sequence = 1;
        TemporalEndpoint third = second; third.sequence = 2;
        const std::vector<std::vector<std::byte>> a{encode_temporal_segment(make_temporal_segment(first, second))};
        const std::vector<std::vector<std::byte>> b{encode_temporal_segment(make_temporal_segment(second, third))};
        const auto retained = [&] {
            auto store = DurableNativeStore::create(argv[1], first, 700);
            const auto base = store.root();
            const auto one = store.append(101, base, a);
            const auto two = store.append(102, one.root, b);
            if (store.append(101, base, a).root != one.root || store.root() != two.root)
                throw std::runtime_error("historical retry changed the head");
            return store.snapshot();
        }();
        auto reopened = DurableNativeStore::open(argv[1]);
        if (!reopened.snapshot().same_history(retained) || retained.segment_count() != 2 ||
            !reopened.status(101) || !reopened.status(102)) throw std::runtime_error("recovery differs");
        std::cout << "Recovered namespace " << reopened.root().source_namespace << ", sequence "
                  << reopened.root().sequence << ", " << retained.segment_count() << " ordered frames\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
