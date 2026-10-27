#pragma once

#include "rfc/storage/native_durable_store.hpp"
#include <atomic>
#include <mutex>
#include <stop_token>

namespace rfc::storage {
namespace concurrent_history_detail {
struct Origin {};
struct Publication {
    DurableNativeRoot root;
    std::optional<NativeTemporalSeries> series;
};
struct WriterScope {
    const void* owner;
    WriterScope* previous;
    static inline thread_local WriterScope* active{};
    explicit WriterScope(const void* value) : owner(value), previous(active) {
        for (auto scope = active; scope; scope = scope->previous)
            if (scope->owner == value) throw std::logic_error("concurrent history writer reentry");
        active = this;
    }
    ~WriterScope() { active = previous; }
};
} // namespace concurrent_history_detail

// Authentic owning expected-base token. Numerical/root equality alone cannot
// manufacture a token from another service. A local series copy is a private fork.
class NativeHistorySnapshot {
public:
    DurableNativeRoot root() const { valid(); return publication_->root; }
    NativeTemporalSeries series() const { valid(); return *publication_->series; }
    NativeSeriesEndpoint tail() const { valid(); return publication_->series->tail(); }
private:
    friend class ConcurrentNativeHistory;
    NativeHistorySnapshot(std::shared_ptr<const concurrent_history_detail::Origin> origin,
                          std::shared_ptr<const concurrent_history_detail::Publication> publication)
        : origin_(std::move(origin)), publication_(std::move(publication)) {}
    void valid() const {
        if (!origin_ || !publication_ || !publication_->series)
            throw std::invalid_argument("moved-from native history snapshot");
    }
    std::shared_ptr<const concurrent_history_detail::Origin> origin_;
    std::shared_ptr<const concurrent_history_detail::Publication> publication_;
};
class NativeHistoryCancelled : public std::runtime_error {
public:
    NativeHistoryCancelled() : std::runtime_error("native history call cancelled before new head publication") {}
};
class NativeHistoryConflict : public std::runtime_error {
public:
    NativeHistoryConflict() : std::runtime_error("native history expected complete root is stale") {}
};

// One Linux process, concurrent readers, serialized writers. A complete root is
// the conflict domain; no automatic merge, mutable workspace or disk GC is shared.
class ConcurrentNativeHistory {
    struct State {
        std::filesystem::path path;
        DurableNativeLimits limits;
        const pid_t process{::getpid()};
        std::shared_ptr<const concurrent_history_detail::Origin> origin{
            std::make_shared<const concurrent_history_detail::Origin>()};
        std::unique_ptr<DurableNativeStore> store;
        std::mutex writer;
        std::atomic<std::shared_ptr<const concurrent_history_detail::Publication>> current;
        State(std::filesystem::path location, DurableNativeLimits caps) : path(std::move(location)), limits(caps) {}
        void check_process() const {
            if (::getpid() != process) throw std::logic_error("live native history service inherited across fork");
        }
    };
public:
    ConcurrentNativeHistory(const ConcurrentNativeHistory&) = delete;
    ConcurrentNativeHistory& operator=(const ConcurrentNativeHistory&) = delete;
    ConcurrentNativeHistory(ConcurrentNativeHistory&&) = delete;
    ConcurrentNativeHistory& operator=(ConcurrentNativeHistory&&) = delete;
    static ConcurrentNativeHistory create(const std::filesystem::path& path, const TemporalEndpoint& base,
                                           std::uint64_t source_namespace, DurableNativeLimits limits = {}) {
        auto state = std::make_unique<State>(path, limits);
        auto publication = std::make_shared<concurrent_history_detail::Publication>();
        state->store.reset(new DurableNativeStore(DurableNativeStore::create(path, base, source_namespace, limits)));
        publication->series.emplace(state->store->snapshot()); publication->root = state->store->root();
        state->current.store(std::move(publication), std::memory_order_release);
        return ConcurrentNativeHistory(std::move(state));
    }
    static ConcurrentNativeHistory open(const std::filesystem::path& path, DurableNativeLimits limits = {}) {
        auto state = std::make_unique<State>(path, limits);
        auto publication = std::make_shared<concurrent_history_detail::Publication>();
        state->store.reset(new DurableNativeStore(DurableNativeStore::open(path, limits)));
        publication->series.emplace(state->store->snapshot()); publication->root = state->store->root();
        state->current.store(std::move(publication), std::memory_order_release);
        return ConcurrentNativeHistory(std::move(state));
    }
    NativeHistorySnapshot snapshot() const {
        state_->check_process();
        return {state_->origin, state_->current.load(std::memory_order_acquire)};
    }
    std::optional<DurableNativeReceipt> status(std::uint64_t transaction) const {
        state_->check_process(); concurrent_history_detail::WriterScope scope(state_.get());
        std::lock_guard lock(state_->writer); require_store(); return state_->store->status(transaction);
    }
    DurableNativeReceipt append(std::uint64_t transaction, const NativeHistorySnapshot& expected,
                                std::span<const std::vector<std::byte>> frames, std::stop_token stop = {},
                                const DurableFaultHook& hook = {}) {
        state_->check_process(); expected.valid();
        if (expected.origin_ != state_->origin) throw std::invalid_argument("foreign history snapshot token");
        concurrent_history_detail::WriterScope scope(state_.get()); std::lock_guard lock(state_->writer);
        require_store();
        // A cancelled retry can still resolve an already committed, exact request.
        const auto existing = state_->store->status(transaction);
        if (!existing && expected.root() != state_->store->root()) throw NativeHistoryConflict();
        if (stop.stop_requested() && !existing) throw NativeHistoryCancelled();
        auto publication = std::make_shared<concurrent_history_detail::Publication>();
        bool replacing = false;
        const auto observer = [&](DurableEvent event) {
            if (!replacing && stop.stop_requested()) throw NativeHistoryCancelled();
            if (event.stage == DurableStage::LivePublished) {
                // The durable store has validated and synced this complete state. No public
                // callback receives mutable store access; this capture cannot allocate.
                const auto& durable = *state_->store->state_;
                publication->series = durable.series;
                publication->root = native_durable_detail::root(durable.current, durable.commit);
                state_->current.store(publication, std::memory_order_release);
            }
            if (hook) hook(event);
            if (!replacing && stop.stop_requested()) throw NativeHistoryCancelled();
            if (event.stage == DurableStage::BeforeHeadReplace) replacing = true;
        };
        return state_->store->append(transaction, expected.root(), frames, observer);
    }
    // Retained readers stay available even if reopening fails. All writer/status
    // calls require successful full durable-store recovery before resuming.
    void recover() {
        state_->check_process(); concurrent_history_detail::WriterScope scope(state_.get());
        std::lock_guard lock(state_->writer);
        auto publication = std::make_shared<concurrent_history_detail::Publication>();
        const auto old = state_->current.load(std::memory_order_acquire);
        state_->store.reset();
        auto recovered = std::unique_ptr<DurableNativeStore>(new DurableNativeStore(DurableNativeStore::open(state_->path, state_->limits)));
        publication->series.emplace(recovered->snapshot()); publication->root = recovered->root();
        if (publication->root.source_namespace != old->root.source_namespace)
            throw std::invalid_argument("recovered history namespace changed");
        state_->store = std::move(recovered); state_->current.store(std::move(publication), std::memory_order_release);
    }
private:
    explicit ConcurrentNativeHistory(std::unique_ptr<State> state) : state_(std::move(state)) {}
    void require_store() const {
        if (!state_->store) throw std::runtime_error("native history recovery unavailable; retry recovery");
    }
    std::unique_ptr<State> state_;
};
} // namespace rfc::storage
