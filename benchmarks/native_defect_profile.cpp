#include "../tests/storage/native_defect_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace native_defect_fixture;
int main(int argc, char** argv) {
    try {
        const auto repeats = argc > 1 ? std::stoul(argv[1]) : 3ul;
        if (repeats < 1 || repeats > 100) throw std::invalid_argument("repeat count must be in [1,100]");
        using Clock = std::chrono::steady_clock;
        const auto elapsed = [](auto start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); };
        std::cout << "triangles,trial,routes,full_products,updated_products,reused_routes,copied_blocks,scanned_occurrences,build_ms,rebuild_ms,replay_ms,advance_ms\n";
        std::cout << std::fixed << std::setprecision(6);
        for (auto n : {std::size_t{8}, std::size_t{64}, std::size_t{256}}) {
            auto base = endpoint(n, true); auto target = base; target.sequence = 1;
            set_map(target, occ(1, 1000), M::diagonal({Q{-3, 7}, Q{17, 19}}));
            const auto segment = make_temporal_segment(base, target); const auto r = realization(base);
            for (unsigned long trial = 0; trial < repeats; ++trial) {
                auto start = Clock::now(); NativeDefectStore index(base, 17, r, queries()); const auto build_ms = elapsed(start);
                start = Clock::now(); auto replayed = replay_temporal_segment(base, segment); const auto replay_ms = elapsed(start);
                start = Clock::now(); NativeDefectStore rebuilt(target, 17, r, queries()); const auto rebuild_ms = elapsed(start);
                start = Clock::now(); index.advance(segment); const auto advance_ms = elapsed(start);
                const auto incremental = index.pin(), full = rebuilt.pin();
                if (incremental.route_keys() != full.route_keys()) throw std::runtime_error("profile route support mismatch");
                for (auto key : full.route_keys()) if (incremental.route(key).contribution != full.route(key).contribution)
                    throw std::runtime_error("profile exact route mismatch");
                for (const auto& q : queries()) if (incremental.certificate(q.key).sums != full.certificate(q.key).sums)
                    throw std::runtime_error("profile exact chart mismatch");
                if (encode_temporal_endpoint(replayed) != encode_temporal_endpoint(*incremental.endpoint())) throw std::runtime_error("profile endpoint mismatch");
                const auto s = incremental.stats();
                std::cout << n << ',' << trial << ',' << s.routes << ',' << full.stats().route_products << ',' << s.route_products
                          << ',' << s.reused_routes << ',' << s.copied_blocks << ',' << s.scanned_occurrences << ','
                          << build_ms << ',' << rebuild_ms << ',' << replay_ms << ',' << advance_ms << '\n';
            }
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
