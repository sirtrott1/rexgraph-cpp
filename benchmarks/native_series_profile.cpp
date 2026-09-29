#include "../tests/storage/native_series_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace native_series_fixture;
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
}
int main(int argc, char** argv) {
    try {
        const int repeats = argc > 1 ? std::stoi(argv[1]) : 3;
        if (repeats < 1 || repeats > 100) throw std::invalid_argument("repeats must be 1..100");
        std::cout << "frames,requested_sequence,base_replays,checkpoint_replays,archive_bytes,base_query_ms,checkpoint_query_ms,decode_validate_ms\n";
        for (const std::size_t count : {8u, 32u, 64u}) {
            auto current = endpoint(0, Vec(8, Q{1})); NativeTemporalSeries series(current, 4);
            for (std::size_t t = 1; t <= count; ++t) {
                auto next = endpoint(t, Vec(8, Q{1})); next.tensors[0] = field_tensor(10, 1, {Q(std::to_string(t)), 1, 1, 1, 1, 1, 1, 1});
                series.append(make_temporal_segment(current, next)); current = std::move(next);
                if (t % 8 == 0) series.checkpoint(t);
            }
            const auto bytes = encode_native_series(series); const auto seq = count - 1;
            std::vector<double> base_times, cp_times, load_times;
            std::size_t base_replays = 0, cp_replays = 0;
            for (int i = 0; i < repeats; ++i) {
                auto start = Clock::now(); auto base = series.at(seq, true); base_times.push_back(elapsed(start));
                start = Clock::now(); auto cp = series.at(seq); cp_times.push_back(elapsed(start));
                start = Clock::now(); auto loaded = decode_native_series(bytes); load_times.push_back(elapsed(start));
                if (encode_rfcx(base.endpoint().source).bytes != encode_rfcx(cp.endpoint().source).bytes ||
                    encode_temporal_endpoint(base.endpoint()) != encode_temporal_endpoint(cp.endpoint()) || !loaded.same_history(series))
                    throw std::runtime_error("profile failed full exact equality");
                base_replays = base.stats().replayed_segments; cp_replays = cp.stats().replayed_segments;
            }
            const auto median = [](std::vector<double> xs) { std::sort(xs.begin(), xs.end()); return xs[xs.size()/2]; };
            std::cout << count << ',' << seq << ',' << base_replays << ',' << cp_replays << ',' << bytes.size() << ','
                      << std::fixed << std::setprecision(6) << median(base_times) << ',' << median(cp_times) << ',' << median(load_times) << '\n';
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
