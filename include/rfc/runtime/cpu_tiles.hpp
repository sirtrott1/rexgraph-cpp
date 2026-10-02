#pragma once

#include "rfc/runtime/execution_policy.hpp"
#include "rfc/machine/checked_size.hpp"
#include <algorithm>
#include <exception>
#include <functional>
#include <thread>
#include <vector>

namespace rfc::runtime {
struct CpuTilePolicy {
    std::size_t workers{1}, worker_limit{1}, tile_limit{1000000};
};
struct CpuTileRunReport {
    std::size_t requested_workers{}, worker_slots{}, launched_threads{}, tiles{}, completed_tiles{};
};
class CpuTileInterrupted : public std::runtime_error {
public:
    CpuTileInterrupted() : std::runtime_error("CPU tile interrupted after another tile failed") {}
};
class CpuTileStop {
public:
    void check() const { if (stop_->load(std::memory_order_relaxed)) throw CpuTileInterrupted(); }
private:
    explicit CpuTileStop(const std::atomic_bool& stop) : stop_(&stop) {}
    const std::atomic_bool* stop_;
    friend class CpuTileExecutor;
};

// Opt-in synchronous pure tasks, with the caller included in the worker budget.
// Existing serial providers are not routed here. No persistent threads or pool.
class CpuTileExecutor {
public:
    explicit CpuTileExecutor(CpuTilePolicy policy = {}) : policy_(policy) {
        if (!policy.workers || !policy.worker_limit) throw std::invalid_argument("CPU worker counts must be positive");
        if (policy.workers > policy.worker_limit)
            throw ExecutionLimitExceeded("CPU workers",policy.workers,policy.worker_limit);
    }
    CpuTileExecutor(const CpuTileExecutor&) = delete;
    CpuTileExecutor& operator=(const CpuTileExecutor&) = delete;
    const CpuTilePolicy& policy() const & noexcept { return policy_; }
    const CpuTilePolicy& policy() const && = delete;
    template<class Task>
    CpuTileRunReport run_tiles(std::size_t tiles, const Task& task,
                               const std::function<void(std::size_t)>& before_launch = {}) {
        if (busy_.exchange(true,std::memory_order_acquire)) throw std::logic_error("CPU executor already running");
        struct Release { std::atomic_bool& busy; ~Release() { busy.store(false,std::memory_order_release); } } release{busy_};
        if (tiles > policy_.tile_limit) throw ExecutionLimitExceeded("CPU tiles",tiles,policy_.tile_limit);
        const auto workers = std::min(policy_.workers,tiles);
        (void)machine::checked_size_add(tiles,workers);
        CpuTileRunReport report{policy_.workers,workers,workers?workers-1:0,tiles,0};
        if (!tiles) return report;
        std::atomic_bool start{},stop{}; std::atomic_size_t next{};
        std::vector<std::exception_ptr> errors(tiles);
        std::vector<unsigned char> completed(tiles),interrupted(tiles);
        const CpuTileStop token(stop);
        const auto worker = [&](std::size_t id) {
            while (!stop.load(std::memory_order_relaxed)) {
                const auto tile = next.fetch_add(1,std::memory_order_relaxed);
                if (tile>=tiles) return;
                try { task(tile,id,token); completed[tile]=1; }
                catch (const CpuTileInterrupted&) { errors[tile]=std::current_exception(); interrupted[tile]=1; stop.store(true,std::memory_order_relaxed); }
                catch (...) { errors[tile]=std::current_exception(); stop.store(true,std::memory_order_relaxed); }
            }
        };
        std::vector<std::jthread> threads; threads.reserve(workers-1);
        try {
            for (std::size_t id=1;id<workers;++id) {
                if (before_launch) before_launch(id);
                threads.emplace_back([&,id] { start.wait(false,std::memory_order_acquire); worker(id); });
            }
        } catch (...) {
            stop.store(true,std::memory_order_relaxed); start.store(true,std::memory_order_release); start.notify_all();
            threads.clear(); throw;
        }
        start.store(true,std::memory_order_release); start.notify_all(); worker(0);
        for (auto& thread : threads) thread.join();
        for (std::size_t i=0;i<tiles;++i) if (errors[i] && !interrupted[i]) std::rethrow_exception(errors[i]);
        for (const auto& error : errors) if (error) std::rethrow_exception(error);
        report.completed_tiles = static_cast<std::size_t>(std::count(completed.begin(),completed.end(),1));
        if (report.completed_tiles != tiles) throw std::logic_error("CPU tile execution incomplete");
        return report;
    }
private:
    CpuTilePolicy policy_;
    std::atomic_bool busy_{};
};
} // namespace rfc::runtime
