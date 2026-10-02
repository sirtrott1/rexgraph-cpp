#include "rfc/storage/concurrent_native_history.hpp"
#include "native_series_fixture.hpp"
#include "native_defect_fixture.hpp"
#include <barrier>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>
#include <sys/wait.h>

namespace {
using namespace native_series_fixture;
using Frames = std::vector<std::vector<std::byte>>;
std::size_t checks{};
void check(bool value, const char* text) { ++checks; if (!value) throw std::runtime_error(text); }
void ensure(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
template<class F> void rejects(F&& f, const char* text) {
    bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } check(failed, text);
}
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        const auto* configured = std::getenv("RFC_DURABLE_TEST_DIRECTORY");
        auto name = ((configured ? std::filesystem::path(configured) : std::filesystem::temp_directory_path()) / "rfc-p2-XXXXXX").string();
        const auto p = ::mkdtemp(name.data()); if (!p) throw std::runtime_error("test directory"); path = p;
    }
    ~Temporary() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
void index() {
    native_series_detail::AppendIndex<std::size_t> values;
    std::vector<decltype(values)> backups;
    std::vector<std::shared_ptr<const std::size_t>> owners;
    std::size_t max_pages{}, max_copies{};
    for (std::size_t i = 0; i < 40000; ++i) {
        auto value = std::make_shared<const std::size_t>(i); owners.push_back(value);
        const auto before = values; values = values.appended(std::move(value)); const auto stats = values.stats();
        check(values.size() == i + 1 && *values.at(i) == i && before.size() == i, "shared index append");
        check(stats.last_append_pages <= stats.levels && stats.last_append_pointer_copies <= 32 * stats.levels,
              "index copied old prefix");
        max_pages = std::max(max_pages, stats.last_append_pages); max_copies = std::max(max_copies, stats.last_append_pointer_copies);
        if (i) check(values.at(i / 2) == owners[i / 2], "earlier owner replaced");
        if (i == 31 || i == 32 || i == 1023 || i == 1024 || i == 32767 || i == 32768) backups.push_back(values);
    }
    std::size_t cursor{}; values.for_each([&](const auto& value) { check(value == owners[cursor++], "directory traversal order"); });
    check(cursor == values.size(), "directory traversal count");
    for (const auto& backup : backups) {
        auto fork = backup.appended(std::make_shared<const std::size_t>(90000));
        check(*fork.at(backup.size()) == 90000 && *values.at(backup.size()) == backup.size(), "fork escaped into original");
        for (std::size_t i = 0; i < backup.size(); ++i) check(backup.at(i) == owners[i], "backup owner changed");
    }
    rejects([&] { (void)values.at(values.size()); }, "index bounds ignored");
    rejects([&] { (void)values.appended({}); }, "null owner admitted");
    std::weak_ptr<const std::size_t> weak;
    {
        decltype(values) retained;
        {
            auto value = std::make_shared<const std::size_t>(42); weak = value;
            retained = retained.appended(value); auto branch = retained.appended(std::make_shared<const std::size_t>(43));
            check(branch.at(0) == retained.at(0), "shared value identity");
        }
        check(!weak.expired(), "live pin lost value");
    }
    check(weak.expired(), "unpinned in-memory pages retained forever");
    std::cout << "40000 indexed values: max " << max_pages << " allocated pages, " << max_copies << " pointer copies per append\n";
}
void series() {
    auto current = endpoint(0, {0, 0}); NativeTemporalSeries history(current, 53);
    std::vector<NativeTemporalSeries> backups; std::vector<std::vector<std::byte>> original;
    for (std::size_t i = 0; i < 1100; ++i) {
        auto next = current; next.sequence = i + 1; auto frame = encode_temporal_segment(make_temporal_segment(current, next));
        const auto previous_bytes = history.archive_bytes(); history.append_encoded(frame);
        check(history.archive_bytes() == previous_bytes + 8 + frame.size(), "incremental byte accounting changed");
        check(history.storage_stats().last_append_pages <= history.storage_stats().frame_index_levels, "series path growth");
        if (i % 100 == 0) backups.push_back(history);
        original.push_back(std::move(frame)); current = std::move(next);
    }
    history.checkpoint(1000); const auto checkpointed = history;
    auto next = current; ++next.sequence; history.append(make_temporal_segment(current, next));
    check(history.storage_stats().checkpoint_count == 1 && checkpointed.last_sequence() == 1100, "checkpoint fork changed");
    for (const auto& backup : backups) for (std::size_t i = 0; i < backup.segment_count(); ++i) {
        check(backup.segment_bytes(i).data() == history.segment_bytes(i).data(), "frame storage not shared");
        check(std::ranges::equal(backup.segment_bytes(i), original[i]), "frame bytes changed");
    }
    history.repack_checkpoints({}); check(checkpointed.checkpoint_sequences() == std::vector<std::uint64_t>{1000}, "checkpoint map changed older snapshot");
    const auto bytes = encode_native_series(history); check(bytes.size() == history.archive_bytes(), "archive byte count");
    check(encode_native_series(decode_native_series(bytes)) == bytes, "new frame directory changed canonical encoding");
    const auto stats = history.storage_stats();
    std::cout << "1101 canonical frames: " << stats.frame_index_pages << " reachable index pages, " << stats.frame_index_levels << " levels\n";
}
void paused(const std::filesystem::path& directory) {
    PaperHistory h; const Frames frames{encode_temporal_segment(h.first)};
    for (const auto stage : {DurableStage::BeforeHeadReplace, DurableStage::HeadReplaced,
                            DurableStage::HeadDirectorySynced, DurableStage::LivePublished}) {
        auto service = ConcurrentNativeHistory::create(directory / ("pause-" + std::to_string(static_cast<int>(stage))), h.a, 71);
        const auto base = service.snapshot(); std::promise<void> reached, release; auto go = release.get_future();
        std::exception_ptr error;
        std::thread writer([&] {
            try { (void)service.append(1, base, frames, {}, [&](DurableEvent event) {
                if (event.stage == stage) { reached.set_value(); go.wait(); }
            }); } catch (...) { error = std::current_exception(); }
        });
        reached.get_future().wait();
        const auto snapshot = service.snapshot(); const auto root = snapshot.root(); const auto pin = snapshot.tail();
        const bool new_state = stage == DurableStage::LivePublished;
        const bool complete = root.sequence == (new_state ? 41u : 40u) && pin.sequence() == root.sequence &&
            encode_temporal_endpoint(pin.endpoint()) == encode_temporal_endpoint(new_state ? h.b : h.a);
        release.set_value(); writer.join(); if (error) std::rethrow_exception(error);
        check(complete, "reader saw torn source/channel/root publication");
        check(service.snapshot().root().sequence == 41 && base.root().sequence == 40, "old snapshot altered");
    }
}
void errors(const std::filesystem::path& directory) {
    PaperHistory h; const Frames frames{encode_temporal_segment(h.first)};
    std::vector<DurableEvent> events;
    {
        auto probe = ConcurrentNativeHistory::create(directory / "fault-probe", h.a, 99);
        (void)probe.append(1, probe.snapshot(), frames, {}, [&](DurableEvent e) { events.push_back(e); });
    }
    bool uncertain = false, live = false;
    for (std::size_t cut = 0; cut < events.size(); ++cut) {
        uncertain = uncertain || events[cut].stage == DurableStage::HeadReplaced;
        live = live || events[cut].stage == DurableStage::LivePublished;
        auto service = ConcurrentNativeHistory::create(directory / ("fault-" + std::to_string(cut)), h.a, 99);
        const auto base = service.snapshot(); std::size_t seen{}; bool unknown = false, failed = false;
        try { (void)service.append(1, base, frames, {}, [&](DurableEvent) { if (seen++ == cut) throw std::runtime_error("injected"); }); }
        catch (const DurableUnknownOutcome&) { unknown = true; failed = true; }
        catch (const std::exception&) { failed = true; }
        check(failed && unknown == uncertain, "concurrent unknown outcome classification");
        check(service.snapshot().root().sequence == (live ? 41u : 40u), "unconfirmed state exposed");
        if (unknown) { rejects([&] { (void)service.status(1); }, "uncertain status usable"); service.recover(); }
        const auto receipt = service.append(1, base, frames);
        check(receipt.root.sequence == 41 && service.snapshot().series().segment_count() == 1, "recovery/retry applied twice");
        check(service.append(1, base, frames).root == receipt.root, "same ID retry changed publication");
    }
    std::cout << "concurrent exception cuts: " << events.size() << '\n';
    auto service = ConcurrentNativeHistory::create(directory / "cancel", h.a, 99); const auto base = service.snapshot();
    std::stop_source early; early.request_stop();
    rejects([&] { (void)service.append(2, base, frames, early.get_token()); }, "pre-cancel ignored");
    check(service.snapshot().root() == base.root() && !service.status(2), "cancel published");
    std::stop_source during;
    rejects([&] { (void)service.append(2, base, frames, during.get_token(), [&](DurableEvent e) {
        if (e.stage == DurableStage::BeforeHeadReplace) during.request_stop();
    }); }, "pre-head cancellation ignored");
    check(service.snapshot().root() == base.root() && !service.status(2), "cancel crossed head publication");
    std::stop_source late;
    const auto receipt = service.append(2, base, frames, late.get_token(), [&](DurableEvent e) {
        if (e.stage == DurableStage::HeadReplaced) late.request_stop();
    });
    check(service.snapshot().root() == receipt.root && service.append(2, base, frames, early.get_token()).root == receipt.root,
          "late cancellation lied about a commit");
    auto clone_path = directory / "clone";
    std::filesystem::copy(directory / "cancel", clone_path, std::filesystem::copy_options::recursive);
    auto clone = ConcurrentNativeHistory::open(clone_path);
    check(clone.snapshot().root() == service.snapshot().root(), "clone root fixture differs");
    rejects([&] { (void)clone.append(2, base, frames); }, "equal root identified foreign owner");
    auto moved = base; auto pin = std::move(moved); rejects([&] { (void)moved.root(); }, "moved snapshot usable");
    check(pin.root() == base.root(), "moved snapshot changed");
    const auto head = read_file_exact(directory / "cancel/HEAD", 48); const auto retained = service.snapshot();
    std::filesystem::remove(directory / "cancel/HEAD");
    rejects([&] { service.recover(); }, "missing HEAD recovered silently");
    check(service.snapshot().root() == retained.root(), "failed recovery cleared readers");
    rejects([&] { (void)service.status(2); }, "failed recovery allowed status");
    write_file_exact(directory / "cancel/HEAD", head); service.recover();
    check(service.snapshot().root() == retained.root(), "repaired recovery differs");
    std::cout.flush();
    const auto child = ::fork(); if (child < 0) throw std::runtime_error("fork");
    if (child == 0) { try { (void)service.snapshot(); } catch (const std::logic_error&) { ::_exit(73); } ::_exit(74); }
    int result{}; check(::waitpid(child, &result, 0) == child && WIFEXITED(result) && WEXITSTATUS(result) == 73, "inherited service admitted");
}
void stress(const std::filesystem::path& path) {
    constexpr std::size_t writers = 4, readers = 4, per_writer = 30;
    auto source = endpoint(100, {100, -100}); auto service = ConcurrentNativeHistory::create(path, source, 901);
    const auto original = service.snapshot(); std::barrier start(static_cast<std::ptrdiff_t>(writers + readers));
    std::atomic<std::size_t> finished{}, conflicts{}, observations{};
    std::array<std::exception_ptr, writers + readers> errors{};
    std::array<std::vector<NativeHistorySnapshot>, readers> saved;
    std::vector<std::thread> threads;
    for (std::size_t w = 0; w < writers; ++w) threads.emplace_back([&, w] {
        try {
            start.arrive_and_wait();
            for (std::size_t n = 0; n < per_writer; ++n) for (;;) {
                const auto base = service.snapshot(); const auto a = *base.tail().pin();
                const auto next = endpoint(a.sequence + 1, {Q{static_cast<long>(a.sequence + 1)}, Q{-static_cast<long>(a.sequence + 1)}});
                const Frames frames{encode_temporal_segment(make_temporal_segment(a, next))};
                try {
                    const auto receipt = service.append((std::uint64_t{w + 1} << 32) | (n + 1), base, frames);
                    ensure(receipt.root.sequence == a.sequence + 1, "wrong writer receipt"); break;
                } catch (const NativeHistoryConflict&) { ++conflicts; }
            }
        } catch (...) { errors[w] = std::current_exception(); }
        ++finished;
    });
    for (std::size_t r = 0; r < readers; ++r) threads.emplace_back([&, r] {
        try {
            start.arrive_and_wait(); std::size_t n{}; std::uint64_t previous{};
            while (finished.load() < writers || n < 1000) {
                const auto snapshot = service.snapshot(); const auto root = snapshot.root(); const auto series_value = snapshot.series();
                const auto pin = snapshot.tail();
                ensure(root.sequence == pin.sequence() && root.sequence == 100 + root.generation &&
                       series_value.segment_count() == root.generation && root.sequence >= previous, "torn/nonmonotone reader");
                const auto& tensor = pin.endpoint().tensors[0].tensor;
                ensure(tensor.state(0) == Q{static_cast<long>(root.sequence)} && tensor.state(1) == Q{-static_cast<long>(root.sequence)},
                       "source/channel publication differs");
                if (n % 100 == 0) {
                    const auto evolution = series_value.difference(root.sequence, root.sequence).field_evolution(10);
                    ensure(evolution.accumulation.terminal == Vec({Q{static_cast<long>(root.sequence)}, Q{-static_cast<long>(root.sequence)}}),
                           "derived exact read changed");
                    if (saved[r].size() < 10) saved[r].push_back(snapshot);
                }
                previous = root.sequence; ++n; ++observations;
                if (n % 50 == 0) std::this_thread::yield();
            }
        } catch (...) { errors[writers + r] = std::current_exception(); }
    });
    for (auto& thread : threads) thread.join();
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    check(service.snapshot().root().generation == writers * per_writer && observations >= readers * 1000, "stress did not complete");
    for (std::size_t w = 0; w < writers; ++w) for (std::size_t n = 0; n < per_writer; ++n)
        check(service.status((std::uint64_t{w + 1} << 32) | (n + 1)).has_value(), "writer ID lost");
    for (const auto& pins : saved) for (const auto& snapshot : pins)
        check(snapshot.root().sequence == snapshot.tail().sequence(), "reader pin changed after writers");
    rejects([&] { (void)service.append(999, original, Frames{encode_temporal_segment(make_temporal_segment(source, endpoint(101, {1, 2})))}); },
            "stale whole-root dependency bypassed");
    std::cout << "threaded commits " << writers * per_writer << ", reader observations " << observations.load()
              << ", conservative conflicts " << conflicts.load() << '\n';
}
void reentry(const std::filesystem::path& path) {
    PaperHistory h; auto service = ConcurrentNativeHistory::create(path, h.a, 19); const auto base = service.snapshot();
    const Frames first{encode_temporal_segment(h.first)}, second{encode_temporal_segment(h.second)};
    bool tested{};
    (void)service.append(1, base, first, {}, [&](DurableEvent) {
        if (!tested) {
            check(service.snapshot().root() == base.root(), "read callback blocked or changed state");
            rejects([&] { (void)service.status(1); }, "status callback deadlocked");
            rejects([&] { service.recover(); }, "recovery callback deadlocked");
            rejects([&] { (void)service.append(2, base, first); }, "writer callback deadlocked"); tested = true;
        }
    });
    const auto saved = service.snapshot(); const auto next = service.append(2, saved, second);
    check(service.append(1, base, first).root.sequence == 41 && service.snapshot().root() == next.root, "historical retry changed current root");
    check(saved.series().difference(40, 41).correspondence(1) == h.J0, "retained rectangular map changed");
    auto bad = second; bad[0].back() ^= std::byte{1};
    rejects([&] { (void)service.append(3, service.snapshot(), bad); }, "invalid mathematical frame admitted");
    check(service.snapshot().root() == next.root, "failed validation changed readers");
}
void coupled(const std::filesystem::path& directory) {
    const auto base_endpoint = endpoint(0, {0, 0});
    auto service = ConcurrentNativeHistory::create(directory / "write-skew", base_endpoint, 111);
    const auto expected = service.snapshot();
    const auto left = endpoint(1, {1, 0}), right = endpoint(1, {0, 1});
    (void)service.append(1, expected, Frames{encode_temporal_segment(make_temporal_segment(base_endpoint, left))});
    bool conflict = false;
    try { (void)service.append(2, expected, Frames{encode_temporal_segment(make_temporal_segment(base_endpoint, right))}); }
    catch (const NativeHistoryConflict&) { conflict = true; }
    check(conflict && !service.status(2), "disjoint writes bypassed complete read/membership dependency");
    const TemporalEndpoint a(native_defect_fixture::source());
    const TemporalEndpoint b(native_defect_fixture::source(1, true), 1);
    auto complete = make_temporal_segment(a, b); auto incomplete = complete;
    const auto erased = std::erase_if(incomplete.source.grades, [](const auto& patch) { return patch.grade.value == 2; });
    check(erased == 1, "coordinated higher repair fixture missing");
    auto complex = ConcurrentNativeHistory::create(directory / "closure", a, 112); const auto origin = complex.snapshot();
    rejects([&] { (void)complex.append(1, origin, Frames{encode_temporal_segment(incomplete)}); }, "unrepaired higher closure admitted");
    check(complex.snapshot().root() == origin.root() && !complex.status(1), "failed chain validator published");
    (void)complex.append(1, origin, Frames{encode_temporal_segment(complete)});
    check(encode_rfcx(complex.snapshot().tail().pin()->source).bytes == encode_rfcx(b.source).bytes, "coordinated higher repair changed");
}
} // namespace
int main() {
    try {
        Temporary directory; index(); series(); paused(directory.path); errors(directory.path);
        reentry(directory.path / "reentry"); coupled(directory.path); stress(directory.path / "stress");
        std::cout << checks << " P2 checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << "P2 test: " << e.what() << '\n'; return 1; }
}
