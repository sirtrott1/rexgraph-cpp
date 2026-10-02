#include "rfc/storage/native_durable_store.hpp"
#include "native_series_fixture.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::invalid_argument("operational_store MODE DIRECTORY");
        using namespace rfc::storage;
        const std::filesystem::path path(argv[2]);
        const std::string mode(argv[1]);
        native_series_fixture::PaperHistory history;
        if (mode == "seed") { auto store = DurableNativeStore::create(path, history.a, 777); return 0; }
        if(mode=="soak") {
            for(unsigned i=0;i<100;++i) {
                auto current=DurableNativeStore::open(path);const auto series=current.snapshot();
                const auto pin=series.tail();const auto& before=pin.endpoint();
                auto next=native_series_fixture::endpoint(before.sequence+1,
                    {rfc::numeric::Rational{static_cast<long>(i+2)},rfc::numeric::Rational{static_cast<long>(2*i+3)}});
                const auto segment=make_temporal_segment(before,next,{{1,native_series_fixture::M::identity(2)}});
                const std::vector<std::vector<std::byte>> frames{encode_temporal_segment(segment)};
                (void)current.append(1000+i,current.root(),frames);
                const auto recovered=current.snapshot();const auto retained=recovered.at(40);
                if(encode_temporal_endpoint(retained.endpoint())!=encode_temporal_endpoint(history.a) ||
                    encode_native_series(decode_native_series(encode_native_series(recovered)))!=encode_native_series(recovered))
                    throw std::runtime_error("soak changed history");
            }
            std::cout<<"100 sequential durable appends and reopened writers passed\n";return 0;
        }
        auto store = DurableNativeStore::open(path);
        if (mode == "append") {
            const std::vector<std::vector<std::byte>> frames{encode_temporal_segment(history.first)};
            (void)store.append(72, store.root(), frames); return 0;
        }
        if (mode != "verify") throw std::invalid_argument("unknown operational mode");
        const auto series = store.snapshot();
        const auto image = encode_native_series(series);
        if (encode_native_series(decode_native_series(image)) != image) throw std::runtime_error("recovery differs");
        const auto before = series.at(40);
        if (encode_rfcx(before.endpoint().source).bytes != encode_rfcx(history.a.source).bytes ||
            encode_temporal_endpoint(before.endpoint()) != encode_temporal_endpoint(history.a))
            throw std::runtime_error("historical source changed");
        if (store.root().sequence != 40 && store.root().sequence != 41) throw std::runtime_error("partial sequence");
        if (static_cast<bool>(store.status(72)) != (store.root().sequence == 41)) throw std::runtime_error("receipt differs");
        std::cout << "{\"sequence\":" << store.root().sequence << "}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
