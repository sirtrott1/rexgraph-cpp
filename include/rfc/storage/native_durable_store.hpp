#pragma once

#include "rfc/storage/linux_durable_io.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include <compare>
#include <exception>
#include <functional>

namespace rfc::storage {

struct DurableObjectId {
    std::uint64_t high{}, low{};
    bool empty() const { return high == 0 && low == 0; }
    auto operator<=>(const DurableObjectId&) const = default;
};
struct DurableNativeRoot {
    DurableObjectId commit;
    std::uint64_t source_namespace{}, generation{}, sequence{};
    auto operator<=>(const DurableNativeRoot&) const = default;
};
struct DurableNativeReceipt {
    DurableNativeRoot root;
    std::uint64_t transaction{}, first_sequence{}, frame_count{};
};
struct DurableNativeLimits {
    NativeSeriesLimits series{};
    std::size_t max_commits{100000}; // excludes genesis
    std::size_t max_object_bytes{256u * 1024u * 1024u};
};
enum class DurableObjectKind : std::uint32_t { Base = 1, Batch = 2, Index = 3, Commit = 4, Head = 5 };
enum class DurableStage {
    Created, PartWritten, Written, Synced, Closed, ObjectsDirectorySynced,
    CommitDirectorySynced, BeforeHeadReplace, HeadReplaced, HeadDirectorySynced,
    LivePublished, BeforeAcknowledgement
};
struct DurableEvent { DurableStage stage; DurableObjectKind kind; };
using DurableFaultHook = std::function<void(DurableEvent)>;
class DurableUnknownOutcome : public std::runtime_error {
public:
    explicit DurableUnknownOutcome(std::uint64_t transaction)
        : std::runtime_error("durable publication outcome unknown; reopen and resolve transaction"), transaction_(transaction) {}
    std::uint64_t transaction() const noexcept { return transaction_; }
private:
    std::uint64_t transaction_;
};

namespace native_durable_detail {
using Bytes = std::vector<std::byte>;
using Id = DurableObjectId;
using Kind = DurableObjectKind;
using Writer = rfcx_detail::Writer;
using Reader = rfcx_detail::Reader;
using native_series_detail::require;
using native_series_detail::bound;
inline void id(Writer& w, Id value) { w.u64(value.high); w.u64(value.low); }
inline Id id(Reader& r) { const auto hi = r.u64(); return {hi, r.u64()}; }
inline Id allocate_id() { const auto n = linux_durable_detail::random_name(); return {n[0], n[1]}; }
inline std::string name(Id value) {
    require(!value.empty(), "null durable object reference");
    constexpr char hex[] = "0123456789abcdef";
    std::string out(32, '0');
    for (std::size_t i = 0; i < 16; ++i) {
        out[15 - i] = hex[(value.high >> (4 * i)) & 15];
        out[31 - i] = hex[(value.low >> (4 * i)) & 15];
    }
    return out;
}
inline constexpr std::array<std::byte, 8> magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'},
    std::byte{'X'}, std::byte{'D'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline Bytes envelope(Kind kind, std::span<const std::byte> body, const DurableNativeLimits& limits) {
    bound(machine::checked_size_add(32, body.size()), limits.max_object_bytes, "durable object byte budget");
    Writer w; w.raw(magic); w.u32(1); w.u32(static_cast<std::uint32_t>(kind));
    w.u64(32 + body.size()); w.u64(rfcx_detail::fnv1a64(body)); w.raw(body); return w.bytes();
}
inline std::span<const std::byte> payload(std::span<const std::byte> image, Kind kind) {
    require(image.size() >= 32, "truncated durable envelope"); Reader r(image);
    require(std::ranges::equal(r.slice(8), magic), "invalid durable envelope magic");
    require(r.u32() == 1 && r.u32() == static_cast<std::uint32_t>(kind), "durable version/kind mismatch");
    require(r.u64() == image.size(), "durable envelope length mismatch");
    require(r.u64() == rfcx_detail::fnv1a64(image.subspan(32)), "durable envelope integrity mismatch");
    return image.subspan(32); // integrity is not authentication or semantic proof
}
inline void end(const Reader& r, std::span<const std::byte> body) { require(r.position() == body.size(), "trailing durable data"); }
struct Commit {
    Id previous, content, index;
    std::uint64_t ns{}, generation{}, transaction{}, first{}, last{}, count{};
};
inline Bytes encode(const Commit& c) {
    Writer w; id(w, c.previous); id(w, c.content); id(w, c.index);
    for (auto n : {c.ns, c.generation, c.transaction, c.first, c.last, c.count}) w.u64(n);
    return w.bytes();
}
inline Commit decode_commit(std::span<const std::byte> image) {
    const auto body = payload(image, Kind::Commit); Reader r(body); Commit c;
    c.previous = id(r); c.content = id(r); c.index = id(r);
    c.ns = r.u64(); c.generation = r.u64(); c.transaction = r.u64();
    c.first = r.u64(); c.last = r.u64(); c.count = r.u64(); end(r, body);
    require(c.ns != 0 && !c.content.empty(), "invalid durable commit binding"); return c;
}
inline DurableNativeRoot root(Id value, const Commit& c) { return {value, c.ns, c.generation, c.last}; }
inline DurableNativeReceipt receipt(Id value, const Commit& c) { return {root(value, c), c.transaction, c.first, c.count}; }
struct Ref {
    Id page; std::uint64_t slot{};
    auto operator<=>(const Ref&) const = default;
};
inline void ref(Writer& w, Ref v) { id(w, v.page); w.u64(v.slot); }
inline Ref ref(Reader& r) {
    Ref v; v.page = id(r); v.slot = r.u64();
    require(v.slot < 65 && (!v.page.empty() || v.slot == 0), "invalid durable trie reference"); return v;
}
struct Node {
    Ref child[2]; Id commit;
};
using Page = std::array<Node, 65>;
inline Bytes encode(const Page& page) {
    Writer w;
    for (const auto& n : page) { ref(w, n.child[0]); ref(w, n.child[1]); id(w, n.commit); }
    return w.bytes();
}
inline Page decode_page(std::span<const std::byte> image) {
    const auto body = payload(image, Kind::Index); Reader r(body); Page out{};
    for (auto& n : out) { n.child[0] = ref(r); n.child[1] = ref(r); n.commit = id(r); }
    end(r, body); return out;
}
inline Bytes batch(std::span<const Bytes> frames, const DurableNativeLimits& limits) {
    require(!frames.empty(), "durable transaction needs at least one retained frame");
    bound(frames.size(), limits.series.max_segments, "durable batch frame budget");
    std::size_t bytes = 8;
    for (const auto& f : frames) bytes = native_series_detail::add_blob(bytes, f.size());
    bound(machine::checked_size_add(bytes, 32), limits.max_object_bytes, "durable batch byte budget");
    Writer w; w.u64(frames.size());
    for (const auto& f : frames) native_series_codec_detail::blob(w, f);
    return envelope(Kind::Batch, w.bytes(), limits);
}
inline std::size_t apply(NativeTemporalSeries& series, std::span<const std::byte> image, const DurableNativeLimits& limits) {
    const auto body = payload(image, Kind::Batch); auto policy = native_series_detail::policy(limits.series);
    policy.max_file_bytes = limits.max_object_bytes;
    temporal_codec_detail::Reader r(body, policy); const auto n = r.count(8);
    require(n > 0, "empty durable batch"); bound(n, limits.series.max_segments, "durable batch frame budget");
    for (std::size_t i = 0; i < n; ++i) series.append_encoded(r.blob());
    require(r.remaining() == 0, "trailing durable batch data"); return n;
}
inline void notify(const DurableFaultHook& hook, DurableStage stage, Kind kind) { if (hook) hook({stage, kind}); }
struct State {
    linux_durable_detail::Descriptor directory, objects, lock;
    DurableNativeLimits limits;
    std::optional<NativeTemporalSeries> series;
    Id current;
    Commit commit;
    bool busy{}, poisoned{};
    Bytes load(Id value, Kind kind) const {
        const auto cap = (kind == Kind::Commit ? std::size_t{128} :
                         kind == Kind::Index ? std::size_t{4192} : limits.max_object_bytes);
        auto image = linux_durable_detail::read(objects.get(), name(value), std::min(cap, limits.max_object_bytes));
        (void)payload(image, kind); return image;
    }
    Node node(Ref where, std::map<Id, Page>& cache) const {
        if (where.page.empty()) return {};
        auto it = cache.find(where.page);
        if (it == cache.end()) it = cache.emplace(where.page, decode_page(load(where.page, Kind::Index))).first;
        return it->second.at(static_cast<std::size_t>(where.slot));
    }
    Id lookup(Id page, std::uint64_t transaction) const {
        Ref where{page, page.empty() ? 0u : 64u}; std::map<Id, Page> cache;
        for (unsigned d = 0; d < 64; ++d) {
            const auto n = node(where, cache); require(n.commit.empty(), "internal durable trie leaf");
            where = n.child[(transaction >> (63 - d)) & 1];
            if (where.page.empty()) return {};
        }
        const auto n = node(where, cache);
        require(n.child[0].page.empty() && n.child[1].page.empty(), "durable leaf has children");
        return n.commit;
    }
    Page update(Id previous, Id next_page, Id next_commit, std::uint64_t transaction) const {
        Page out{}; std::array<Node, 64> path{}; std::map<Id, Page> cache;
        Ref where{previous, previous.empty() ? 0u : 64u};
        for (unsigned d = 0; d < 64; ++d) {
            path[d] = node(where, cache); require(path[d].commit.empty(), "internal durable trie leaf");
            where = path[d].child[(transaction >> (63 - d)) & 1];
        }
        const auto old = node(where, cache);
        require(old.commit.empty() && old.child[0].page.empty() && old.child[1].page.empty(), "duplicate durable transaction");
        out[0].commit = next_commit;
        for (unsigned i = 1; i <= 64; ++i) {
            const auto depth = 64 - i; out[i] = path[depth];
            out[i].child[(transaction >> (63 - depth)) & 1] = {next_page, i - 1};
        }
        return out;
    }
    Id head() const {
        const auto image = linux_durable_detail::read(directory.get(), "HEAD", std::min(limits.max_object_bytes, std::size_t{48}));
        const auto body = payload(image, Kind::Head); Reader r(body); const auto value = id(r); end(r, body);
        require(!value.empty(), "null durable HEAD"); return value;
    }
    void write(int parent, const std::string& filename, std::span<const std::byte> image, Kind kind, const DurableFaultHook& hook) {
        auto fd = linux_durable_detail::regular(parent, filename, O_WRONLY | O_CREAT | O_EXCL);
        notify(hook, DurableStage::Created, kind);
        const auto split = image.size() / 2;
        linux_durable_detail::write(fd.get(), image.first(split)); notify(hook, DurableStage::PartWritten, kind);
        linux_durable_detail::write(fd.get(), image.subspan(split)); notify(hook, DurableStage::Written, kind);
        linux_durable_detail::synchronize(fd.get()); notify(hook, DurableStage::Synced, kind);
        fd.close_checked(); notify(hook, DurableStage::Closed, kind);
    }
    void object(Id value, std::span<const std::byte> image, Kind kind, const DurableFaultHook& hook) {
        write(objects.get(), name(value), image, kind, hook);
    }
    void publish(Id value, Id expected, const DurableFaultHook& hook, bool& uncertain) {
        Writer w; id(w, value); const auto image = envelope(Kind::Head, w.bytes(), limits);
        const auto staged = ".head-" + name(allocate_id()); write(directory.get(), staged, image, Kind::Head, hook);
        if (!expected.empty()) require(head() == expected, "durable expected head changed");
        else {
            struct stat status{};
            const int result = ::fstatat(directory.get(), "HEAD", &status, AT_SYMLINK_NOFOLLOW);
            require(result == -1 && errno == ENOENT, "durable genesis HEAD already exists");
        }
        notify(hook, DurableStage::BeforeHeadReplace, Kind::Head);
        uncertain = true; // no negative outcome assertion once rename is attempted
        if (::renameat(directory.get(), staged.c_str(), directory.get(), "HEAD") != 0)
            linux_durable_detail::system_error("replace durable HEAD");
        notify(hook, DurableStage::HeadReplaced, Kind::Head);
        linux_durable_detail::synchronize(directory.get()); notify(hook, DurableStage::HeadDirectorySynced, Kind::Head);
    }
    void recover() {
        current = head(); std::vector<std::pair<Id, Commit>> chain; Id next = current;
        while (!next.empty()) {
            bound(chain.size(), limits.max_commits, "durable recovery commit budget");
            const auto c = decode_commit(load(next, Kind::Commit)); chain.emplace_back(next, c); next = c.previous;
        }
        std::reverse(chain.begin(), chain.end()); const auto& genesis = chain.front().second;
        require(genesis.previous.empty() && genesis.index.empty() && genesis.generation == 0 && genesis.transaction == 0 &&
                genesis.count == 0 && genesis.first == genesis.last, "invalid durable genesis");
        const auto image = load(genesis.content, Kind::Base);
        series.emplace(decode_native_series(payload(image, Kind::Base), limits.series));
        require(series->segment_count() == 0 && series->checkpoint_sequences().empty() &&
                series->source_namespace() == genesis.ns && series->last_sequence() == genesis.last,
                "durable genesis base mismatch");
        for (std::size_t i = 1; i < chain.size(); ++i) {
            const auto& [value, c] = chain[i]; const auto& previous = chain[i - 1];
            require(c.previous == previous.first && c.ns == genesis.ns && c.generation == i &&
                    c.transaction != 0 && !c.index.empty() && c.first == series->last_sequence(), "durable lineage mismatch");
            const auto frames = load(c.content, Kind::Batch); const auto n = apply(*series, frames, limits);
            require(c.count == n && c.last == series->last_sequence(), "durable batch/sequence mismatch");
            const auto expected = encode(update(previous.second.index, c.index, value, c.transaction));
            const auto stored = load(c.index, Kind::Index);
            require(std::ranges::equal(expected, payload(stored, Kind::Index)), "durable index differs from retained lineage");
        }
        commit = chain.back().second;
        // A prior rename may have been visible without its directory sync/ack.
        // Stabilize this fully validated decision before returning durable status.
        auto fd = linux_durable_detail::regular(directory.get(), "HEAD", O_RDONLY);
        linux_durable_detail::synchronize(fd.get()); fd.close_checked();
        linux_durable_detail::synchronize(directory.get());
    }
};
struct Guard {
    State& state;
    explicit Guard(State& value) : state(value) {
        require(!state.poisoned, "durable outcome unresolved; reopen required");
        require(!state.busy, "durable store reentry"); state.busy = true;
    }
    ~Guard() { state.busy = false; }
};
inline std::unique_ptr<State> open_directory(const std::filesystem::path& path, DurableNativeLimits limits, bool create) {
    validate_file_path(path);
    require(limits.max_object_bytes >= 4192, "durable object budget cannot hold one index page");
    auto state = std::make_unique<State>(); state->limits = limits;
    if (create) {
        auto parent = path.parent_path(); if (parent.empty()) parent = ".";
        auto parent_fd = linux_durable_detail::directory(parent);
        if (::mkdir(path.c_str(), 0700) != 0) linux_durable_detail::system_error("create durable root");
        linux_durable_detail::synchronize(parent_fd.get());
    }
    state->directory = linux_durable_detail::directory(path);
    state->lock = linux_durable_detail::coordinator(state->directory.get());
    if (create && ::mkdirat(state->directory.get(), "objects", 0700) != 0)
        linux_durable_detail::system_error("create durable objects");
    state->objects = linux_durable_detail::child_directory(state->directory.get(), "objects");
    linux_durable_detail::synchronize(state->directory.get()); return state;
}
} // namespace native_durable_detail

// One exclusive Linux coordinator. Local filesystem/flush assumptions are part
// of this opt-in profile. Pins/snapshots own their source; callbacks cannot reenter.
// No object deletion, concurrent API use, trusted fast-reopen or device guarantee.
class DurableNativeStore {
public:
    DurableNativeStore(const DurableNativeStore&) = delete;
    DurableNativeStore& operator=(const DurableNativeStore&) = delete;
    DurableNativeStore(DurableNativeStore&&) = delete;
    DurableNativeStore& operator=(DurableNativeStore&&) = delete;
    static DurableNativeStore create(const std::filesystem::path& path, const TemporalEndpoint& base,
                                     std::uint64_t source_namespace, DurableNativeLimits limits = {},
                                     const DurableFaultHook& hook = {}) {
        using namespace native_durable_detail;
        // Complete mathematical validation and limit checks precede filesystem creation.
        NativeTemporalSeries series(base, source_namespace, limits.series);
        const auto data = envelope(Kind::Base, encode_native_series(series), limits);
        auto s = open_directory(path, limits, true); bool uncertain = false;
        const auto content = allocate_id(), commit_id = allocate_id();
        Commit c{{}, content, {}, source_namespace, 0, 0, series.first_sequence(), series.last_sequence(), 0};
        try {
            s->object(content, data, Kind::Base, hook); linux_durable_detail::synchronize(s->objects.get());
            notify(hook, DurableStage::ObjectsDirectorySynced, Kind::Base);
            s->object(commit_id, envelope(Kind::Commit, encode(c), limits), Kind::Commit, hook);
            linux_durable_detail::synchronize(s->objects.get()); notify(hook, DurableStage::CommitDirectorySynced, Kind::Commit);
            s->publish(commit_id, {}, hook, uncertain);
            s->current = commit_id; s->commit = c; s->series.emplace(std::move(series));
            notify(hook, DurableStage::LivePublished, Kind::Head); notify(hook, DurableStage::BeforeAcknowledgement, Kind::Head);
        } catch (...) { if (uncertain) std::throw_with_nested(DurableUnknownOutcome(0)); throw; }
        return DurableNativeStore(std::move(s));
    }
    static DurableNativeStore open(const std::filesystem::path& path, DurableNativeLimits limits = {}) {
        auto s = native_durable_detail::open_directory(path, limits, false); s->recover();
        return DurableNativeStore(std::move(s));
    }
    DurableNativeRoot root() const {
        native_durable_detail::Guard guard(*state_); return native_durable_detail::root(state_->current, state_->commit);
    }
    NativeTemporalSeries snapshot() const { native_durable_detail::Guard guard(*state_); return *state_->series; }
    std::optional<DurableNativeReceipt> status(std::uint64_t transaction) const {
        native_durable_detail::Guard guard(*state_); return find(transaction);
    }
    DurableNativeReceipt append(std::uint64_t transaction, const DurableNativeRoot& expected,
                                std::span<const std::vector<std::byte>> frames, const DurableFaultHook& hook = {}) {
        using namespace native_durable_detail;
        Guard guard(*state_); auto& s = *state_; require(transaction != 0, "durable transaction zero reserved for genesis");
        // Freeze caller bytes before invoking any callback; retries compare full bytes.
        const auto data = batch(frames, s.limits);
        if (const auto existing = find(transaction)) {
            const auto c = decode_commit(s.load(existing->root.commit, Kind::Commit));
            const auto prior = decode_commit(s.load(c.previous, Kind::Commit));
            require(expected == native_durable_detail::root(c.previous, prior) && s.load(c.content, Kind::Batch) == data,
                    "durable transaction ID reused with different predecessor or frames");
            return *existing;
        }
        require(expected == native_durable_detail::root(s.current, s.commit), "stale durable expected root");
        bound(machine::checked_size_add(static_cast<std::size_t>(s.commit.generation), 1), s.limits.max_commits,
              "durable commit count budget");
        auto candidate = *s.series; const auto count = apply(candidate, data, s.limits);
        const auto content = allocate_id(), index = allocate_id(), commit_id = allocate_id();
        Commit c{s.current, content, index, s.commit.ns, s.commit.generation + 1, transaction,
                 s.series->last_sequence(), candidate.last_sequence(), count};
        const auto page = envelope(Kind::Index, encode(s.update(s.commit.index, index, commit_id, transaction)), s.limits);
        const auto record = envelope(Kind::Commit, encode(c), s.limits); const auto result = receipt(commit_id, c);
        bool uncertain = false;
        try {
            s.object(content, data, Kind::Batch, hook); s.object(index, page, Kind::Index, hook);
            linux_durable_detail::synchronize(s.objects.get()); notify(hook, DurableStage::ObjectsDirectorySynced, Kind::Batch);
            s.object(commit_id, record, Kind::Commit, hook); linux_durable_detail::synchronize(s.objects.get());
            notify(hook, DurableStage::CommitDirectorySynced, Kind::Commit);
            s.publish(commit_id, s.current, hook, uncertain);
            s.series = std::move(candidate); s.current = commit_id; s.commit = c;
            notify(hook, DurableStage::LivePublished, Kind::Head); notify(hook, DurableStage::BeforeAcknowledgement, Kind::Head);
        } catch (...) {
            if (uncertain) { s.poisoned = true; std::throw_with_nested(DurableUnknownOutcome(transaction)); }
            throw;
        }
        return result;
    }
private:
    friend class ConcurrentNativeHistory;
    explicit DurableNativeStore(std::unique_ptr<native_durable_detail::State> s) : state_(std::move(s)) {}
    std::optional<DurableNativeReceipt> find(std::uint64_t transaction) const {
        if (transaction == 0) throw std::invalid_argument("durable transaction zero reserved for genesis");
        const auto value = state_->lookup(state_->commit.index, transaction);
        if (value.empty()) return {};
        const auto c = native_durable_detail::decode_commit(state_->load(value, DurableObjectKind::Commit));
        native_durable_detail::require(c.transaction == transaction && c.ns == state_->commit.ns, "durable transaction binding mismatch");
        return native_durable_detail::receipt(value, c);
    }
    std::unique_ptr<native_durable_detail::State> state_;
};
} // namespace rfc::storage
