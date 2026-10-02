#include "rfc/storage/native_durable_store.hpp"
#include "native_series_fixture.hpp"
#include "declaration_temporal_fixture.hpp"
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>

namespace {
using namespace native_series_fixture;
using Bytes = std::vector<std::byte>;
using Frames = std::vector<Bytes>;
std::size_t checks{};
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& f, const char* message) {
    bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } check(failed, message);
}
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        const auto* configured = std::getenv("RFC_DURABLE_TEST_DIRECTORY");
        const auto parent = configured ? std::filesystem::path(configured) : std::filesystem::temp_directory_path();
        auto pattern = (parent / "rfc-durable-XXXXXX").string();
        const auto p = ::mkdtemp(pattern.data()); if (!p) throw std::runtime_error("test temporary directory"); path = p;
    }
    ~Temporary() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
bool published(DurableEvent e) { return e.stage == DurableStage::HeadReplaced; }
auto inventory(const std::filesystem::path& path) {
    std::map<std::string, std::pair<Bytes, std::uint64_t>> out;
    for (const auto& entry : std::filesystem::directory_iterator(path / "objects")) {
        struct stat info{}; if (::stat(entry.path().c_str(), &info) != 0) throw std::runtime_error("test stat");
        out.emplace(entry.path().filename().string(), std::pair{read_file_exact(entry.path(), 1u << 24),
                                                             static_cast<std::uint64_t>(info.st_ino)});
    }
    return out;
}
void unchanged(const auto& before, const auto& after) {
    for (const auto& [name, value] : before) check(after.at(name) == value, "prior object bytes/inode rewritten");
}
void basics(const std::filesystem::path& path, const std::filesystem::path& export_to) {
    PaperHistory h; const Frames first{encode_temporal_segment(h.first)}, second{encode_temporal_segment(h.second)};
    DurableNativeRoot origin, final;
    NativeTemporalSeries surviving(h.a, 777);
    {
        auto store = DurableNativeStore::create(path, h.a, 777); origin = store.root();
        check(origin.generation == 0 && origin.sequence == 40 && origin.source_namespace == 777, "genesis root");
        rejects([&] { (void)DurableNativeStore::open(path); }, "second coordinator admitted");
        rejects([&] { (void)store.status(0); }, "genesis ID exposed as transaction");
        rejects([&] { (void)store.append(0, origin, first); }, "transaction zero admitted");
        rejects([&] { (void)store.append(1, origin, {}); }, "empty transaction admitted");
        auto bad = first; bad[0].pop_back();
        const auto original = inventory(path);
        rejects([&] { (void)store.append(1, origin, bad); }, "truncated frame admitted");
        check(inventory(path) == original && store.root() == origin, "failed validation wrote objects");
        rejects([&] { (void)store.append(1, origin, second); }, "wrong mathematical predecessor admitted");
        bool reentry = false;
        const auto receipt = store.append(1, origin, first, [&](DurableEvent) {
            if (!reentry) { rejects([&] { (void)store.snapshot(); }, "callback reentered store"); reentry = true; }
        });
        check(receipt.transaction == 1 && receipt.first_sequence == 40 && receipt.frame_count == 1, "receipt binding");
        check(receipt.root.sequence == 41 && receipt.root.generation == 1, "first commit");
        unchanged(original, inventory(path));
        const auto before_retry = inventory(path);
        check(store.append(1, origin, first).root == receipt.root && inventory(path) == before_retry, "retry rewrote or reapplied");
        rejects([&] { (void)store.append(1, origin, second); }, "ID reused with changed content");
        rejects([&] { (void)store.append(1, receipt.root, first); }, "ID reused with changed predecessor");
        rejects([&] { (void)store.append(2, origin, second); }, "stale new transaction admitted");
        const auto pinned = store.snapshot().tail();
        final = store.append(UINT64_MAX, receipt.root, second).root;
        check(pinned.sequence() == 41 && encode_temporal_endpoint(pinned.endpoint()) == encode_temporal_endpoint(h.b), "old endpoint pin changed");
        check(store.status(1)->root == receipt.root && store.status(UINT64_MAX)->root == final && !store.status(9), "persistent transaction lookup");
        check(store.append(1, origin, first).root == receipt.root && store.root() == final, "historical retry changed head");
        surviving = store.snapshot(); unchanged(before_retry, inventory(path));
        rejects([&] { (void)DurableNativeStore::create(path, h.a, 777); }, "existing root overwritten");
    }
    auto recovered = DurableNativeStore::open(path);
    check(recovered.root() == final && recovered.snapshot().same_history(surviving), "complete recovery differs");
    check(recovered.append(1, origin, first).root.sequence == 41 && recovered.root() == final, "reopened retry reapplied");
    check(std::ranges::equal(surviving.segment_bytes(0), first[0]) && std::ranges::equal(surviving.segment_bytes(1), second[0]), "canonical frames changed");
    const auto range = surviving.difference(40, 42);
    check(range.correspondence(1) == h.J1.multiply(h.J0), "rectangular order changed");
    check(range.field_evolution(10).accumulation.terminal == Vec({13, -7}), "exact field replay changed");
    const auto moments = range.moments({{10, {}}}, 20);
    check(moments.gram.get(1, 2) == 28, "finite cross interaction lost");
    if (!export_to.empty()) {
        std::filesystem::create_directories(export_to);
        std::filesystem::copy(path, export_to / "store", std::filesystem::copy_options::recursive);
        save(export_to / "expected.rfcxs", encode_native_series(surviving));
        save(export_to / "first.rfcxt", first[0]); save(export_to / "second.rfcxt", second[0]);
    }
}
void grouped(const std::filesystem::path& path) {
    PaperHistory h; Frames both{encode_temporal_segment(h.first), encode_temporal_segment(h.second)};
    {
        auto store = DurableNativeStore::create(path, h.a, 55); const auto base = inventory(path);
        const auto receipt = store.append(8, store.root(), both, [&](DurableEvent) { both.clear(); });
        check(receipt.frame_count == 2 && receipt.root.generation == 1 && receipt.root.sequence == 42, "batch split publication");
        check(inventory(path).size() == base.size() + 3, "batch created object per primitive frame"); unchanged(base, inventory(path));
    }
    auto store = DurableNativeStore::open(path);
    check(store.status(8)->frame_count == 2 && store.snapshot().segment_count() == 2, "grouped history lost frames");
}
void fault_cases(const std::filesystem::path& directory, bool kill) {
    PaperHistory h; const Frames frames{encode_temporal_segment(h.first)};
    std::vector<DurableEvent> events;
    {
        auto probe = DurableNativeStore::create(directory / (kill ? "kill-probe" : "throw-probe"), h.a, 99);
        (void)probe.append(72, probe.root(), frames, [&](DurableEvent event) { events.push_back(event); });
    }
    bool visible = false;
    for (std::size_t cut = 0; cut < events.size(); ++cut) {
        if (published(events[cut])) visible = true;
        const auto path = directory / ((kill ? "kill-" : "throw-") + std::to_string(cut));
        DurableNativeRoot base;
        {
            auto store = DurableNativeStore::create(path, h.a, 99); base = store.root();
            if (!kill) {
                std::size_t seen{}; bool unknown = false, failed = false;
                try { (void)store.append(72, base, frames, [&](DurableEvent) {
                    if (seen++ == cut) throw std::runtime_error("injected failure");
                }); } catch (const DurableUnknownOutcome& e) { failed = true; unknown = true; check(e.transaction() == 72, "unknown ID"); }
                catch (const std::exception&) { failed = true; }
                check(failed && unknown == visible, "publication error classification");
                if (unknown) rejects([&] { (void)store.root(); }, "uncertain instance stayed usable");
                else check(store.root() == base && !store.status(72), "prepublication failure changed state");
            }
        }
        if (kill) {
            const auto child = ::fork(); if (child < 0) throw std::runtime_error("fork");
            if (child == 0) {
                try {
                    auto store = DurableNativeStore::open(path); std::size_t seen{};
                    (void)store.append(72, base, frames, [&](DurableEvent) { if (seen++ == cut) ::_exit(73); });
                } catch (...) { ::_exit(74); }
                ::_exit(75);
            }
            int result{}; check(::waitpid(child, &result, 0) == child && WIFEXITED(result) && WEXITSTATUS(result) == 73, "crash cut not reached");
        }
        {
            auto store = DurableNativeStore::open(path);
            check(static_cast<bool>(store.status(72)) == visible && store.root().sequence == (visible ? 41u : 40u), "recovery exposed incomplete transition");
            const auto receipt = store.append(72, base, frames); const auto before_retry = inventory(path);
            check(receipt.root.sequence == 41 && store.snapshot().segment_count() == 1, "retry double applied");
            check(store.append(72, base, frames).root == receipt.root && inventory(path) == before_retry, "retry after crash wrote objects");
        }
        auto reopened = DurableNativeStore::open(path);
        check(reopened.root().sequence == 41 && reopened.status(72)->root == reopened.root(), "acknowledged commit lost after reopen");
    }
    std::cout << (kill ? "process termination" : "exception") << " cuts: " << events.size() << '\n';
}
void creation_faults(const std::filesystem::path& directory) {
    PaperHistory h; std::vector<DurableEvent> events;
    { auto probe = DurableNativeStore::create(directory / "genesis-probe", h.a, 99, {}, [&](DurableEvent e) { events.push_back(e); }); }
    bool visible = false;
    for (std::size_t cut = 0; cut < events.size(); ++cut) {
        visible = visible || published(events[cut]); const auto path = directory / ("genesis-" + std::to_string(cut));
        bool unknown = false; std::size_t seen{};
        try { (void)DurableNativeStore::create(path, h.a, 99, {}, [&](DurableEvent) { if (seen++ == cut) throw std::runtime_error("injected genesis"); }); }
        catch (const DurableUnknownOutcome& e) { unknown = true; check(e.transaction() == 0, "genesis unknown ID"); }
        catch (const std::exception&) {}
        check(unknown == visible, "genesis outcome classification");
        if (visible) { auto store = DurableNativeStore::open(path); check(store.root().sequence == 40, "genesis recovery"); }
        else rejects([&] { (void)DurableNativeStore::open(path); }, "headless genesis invented");
        rejects([&] { (void)DurableNativeStore::create(path, h.a, 99); }, "failed genesis overwritten");
    }
    std::cout << "creation exception cuts: " << events.size() << '\n';
}
void corrupt(const std::filesystem::path& directory) {
    PaperHistory h; const auto pristine = directory / "pristine";
    {
        auto store = DurableNativeStore::create(pristine, h.a, 99);
        (void)store.append(7, store.root(), Frames{encode_temporal_segment(h.first)});
    }
    std::vector<std::filesystem::path> files{pristine / "HEAD"};
    for (const auto& f : std::filesystem::directory_iterator(pristine / "objects")) files.push_back(f.path());
    std::size_t serial{};
    for (const auto& file : files) {
        for (unsigned mode = 0; mode < 5; ++mode) {
            const auto copy = directory / ("corrupt-" + std::to_string(serial++));
            std::filesystem::copy(pristine, copy, std::filesystem::copy_options::recursive);
            const auto victim = copy / std::filesystem::relative(file, pristine);
            if (mode == 0) std::filesystem::remove(victim);
            else if (mode == 1) { auto b = read_file_exact(victim, 1u << 24); b.pop_back(); save(victim, b); }
            else if (mode == 2) { auto b = read_file_exact(victim, 1u << 24); b.back() ^= std::byte{1}; save(victim, b); }
            else if (mode == 3) { std::filesystem::remove(victim); std::filesystem::create_symlink(file, victim); }
            else { std::filesystem::remove(victim); if (::mkfifo(victim.c_str(), 0600) != 0) throw std::runtime_error("test FIFO"); }
            rejects([&] { (void)DurableNativeStore::open(copy); }, "corrupt referenced object accepted");
        }
    }
    // A correctly checksummed but mathematically unrelated base is not certified by hashing.
    const auto replaced = directory / "forged-base";
    std::filesystem::copy(pristine, replaced, std::filesystem::copy_options::recursive);
    for (const auto& f : std::filesystem::directory_iterator(replaced / "objects")) {
        const auto bytes = read_file_exact(f.path(), 1u << 24); rfcx_detail::Reader r(bytes); (void)r.slice(12);
        if (r.u32() == static_cast<std::uint32_t>(DurableObjectKind::Base)) {
            NativeTemporalSeries other(endpoint(40, {9, 10}), 99);
            save(f.path(), native_durable_detail::envelope(DurableObjectKind::Base, encode_native_series(other), {}));
        }
    }
    rejects([&] { (void)DurableNativeStore::open(replaced); }, "checksum replaced full replay proof");
    // Repairing an envelope checksum cannot bless false index or commit links.
    for (const auto kind : {DurableObjectKind::Index, DurableObjectKind::Commit}) {
        const auto copy = directory / (kind == DurableObjectKind::Index ? "forged-index" : "forged-commit");
        std::filesystem::copy(pristine, copy, std::filesystem::copy_options::recursive);
        for (const auto& f : std::filesystem::directory_iterator(copy / "objects")) {
            auto bytes = read_file_exact(f.path(), 1u << 24); rfcx_detail::Reader r(bytes); (void)r.slice(12);
            if (r.u32() == static_cast<std::uint32_t>(kind)) {
                bytes.back() ^= std::byte{1}; rehash(bytes); save(f.path(), bytes);
            }
        }
        rejects([&] { (void)DurableNativeStore::open(copy); }, "repaired checksum certified false lineage/index");
    }
}
void declarations(const std::filesystem::path& path) {
    using namespace f2_fixture;
    const auto a = f2_fixture::endpoint();
    const auto migration = source_map(a.source.signature()); const auto b = evolved(a, migration);
    const auto transition = f2_fixture::segment(a, b, migration);
    const Frames frames{encode_temporal_segment(transition)};
    {
        auto store = DurableNativeStore::create(path, a, 411);
        (void)store.append(17, store.root(), frames);
        check(encode_temporal_endpoint(*store.snapshot().tail().pin()) == encode_temporal_endpoint(b), "declaration migration changed");
    }
    auto store = DurableNativeStore::open(path); const auto snapshot = store.snapshot();
    check(std::ranges::equal(snapshot.segment_bytes(0), frames[0]), "declaration/occurrence frame rewritten");
    check(encode_rfcx(snapshot.tail().pin()->source).bytes == encode_rfcx(b.source).bytes, "inactive repeated zero occurrences changed");
}
void many(const std::filesystem::path& path, const std::filesystem::path& output) {
    auto endpoint_value = endpoint(100, {0, 0}); DurableNativeRoot base;
    std::map<std::uint64_t, DurableNativeReceipt> receipts;
    std::vector<std::size_t> new_bytes;
    {
        auto store = DurableNativeStore::create(path, endpoint_value, 123); base = store.root();
        for (std::size_t i = 0; i < 70; ++i) {
            auto next = endpoint(101 + i, {0, 0}); const auto segment = make_temporal_segment(endpoint_value, next);
            const auto before = inventory(path); const auto expected = store.root();
            // Both high and low bits, with many shared paths; numerically unchanged fields retained.
            const auto transaction = (std::uint64_t{i + 1} << 32) | (71 - i);
            const auto receipt = store.append(transaction, expected, Frames{encode_temporal_segment(segment)});
            receipts.emplace(transaction, receipt); const auto after = inventory(path); unchanged(before, after);
            check(after.size() == before.size() + 3, "incremental append object count");
            std::size_t bytes{};
            for (const auto& [name, data] : after) if (!before.contains(name)) bytes += data.first.size();
            new_bytes.push_back(bytes);
            for (const auto& [key, value] : receipts) check(store.status(key)->root == value.root, "shared trie path lost transaction");
            endpoint_value = std::move(next);
        }
        check(store.snapshot().segment_count() == 70 && store.root().generation == 70, "equal outputs deduplicated operations");
        check(std::ranges::all_of(new_bytes, [&](auto n) { return n == new_bytes.front(); }), "append bytes grew with old prefix");
    }
    auto store = DurableNativeStore::open(path);
    for (const auto& [key, value] : receipts) check(store.status(key)->root == value.root, "recovered persistent trie lost transaction");
    check(store.root().sequence == 170 && base.sequence == 100, "many recovery sequence");
    if (!output.empty()) {
        std::ofstream report(output / "write-cost.txt");
        report << "commits " << new_bytes.size() << "\nnew_object_bytes " << new_bytes.front() << "\nhead_bytes 48\n";
    }
    std::cout << "70 equal-output commits: " << new_bytes.front() << " new object bytes + 48 HEAD bytes each\n";
}
void limits(const std::filesystem::path& directory) {
    PaperHistory h; DurableNativeLimits cap; cap.max_commits = 1;
    {
        auto store = DurableNativeStore::create(directory / "limits", h.a, 1, cap);
        const auto receipt = store.append(1, store.root(), Frames{encode_temporal_segment(h.first)});
        rejects([&] { (void)store.append(2, store.root(), Frames{encode_temporal_segment(h.second)}); }, "commit budget ignored");
        check(store.root() == receipt.root, "failed budget changed root");
    }
    cap.max_commits = 0; rejects([&] { (void)DurableNativeStore::open(directory / "limits", cap); }, "recovery commit budget ignored");
    cap.max_object_bytes = 4191; rejects([&] { (void)DurableNativeStore::create(directory / "small", h.a, 1, cap); }, "index budget ignored");
    check(!std::filesystem::exists(directory / "small"), "invalid limits created root");
    auto alias = directory / "alias"; std::filesystem::create_directory_symlink(directory / "limits", alias);
    rejects([&] { (void)DurableNativeStore::open(alias); }, "symlink root admitted");
    rejects([&] { (void)DurableNativeStore::create(directory / "namespace", h.a, 0); }, "namespace zero admitted");
}
} // namespace
int main(int argc, char** argv) {
    try {
        Temporary temp; const std::filesystem::path output = argc == 2 ? argv[1] : "";
        basics(temp.path / "basic", output); grouped(temp.path / "grouped");
        fault_cases(temp.path, false); fault_cases(temp.path, true); creation_faults(temp.path);
        corrupt(temp.path); declarations(temp.path / "declarations"); many(temp.path / "many", output); limits(temp.path);
        std::cout << checks << " durable native checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << "durable native test: " << e.what() << '\n'; return 1; }
}
