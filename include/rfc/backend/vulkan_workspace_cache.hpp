#pragma once

#include "rfc/backend/vulkan_binary.hpp"
#include <mutex>

namespace rfc::backend {
struct VulkanWorkspaceCacheLimits {
    std::size_t workspaces{8};
    std::uint64_t allocation_bytes{std::uint64_t{1} << 26};
};
struct VulkanWorkspaceCacheStats {
    std::size_t creations{}, hits{}, discarded{}, active{}, idle{};
    std::uint64_t allocation_bytes{};
};
// One owning device context, exact prepared owner identity, exclusive loans.
// These are reusable physical buffers/pipelines, never memoized task results.
class VulkanBinaryWorkspaceCache {
    struct Entry {
        PreparedPackedBinary operation;
        std::unique_ptr<VulkanBinaryWorkspace> workspace;
        Entry(const VulkanBinaryDevice& device,const PreparedPackedBinary& op,std::uint64_t byte_limit)
            : operation(op),workspace(std::make_unique<VulkanBinaryWorkspace>(device,op,byte_limit)) {}
    };
public:
    explicit VulkanBinaryWorkspaceCache(VulkanBinaryDevice device,VulkanWorkspaceCacheLimits limits = {})
        : device_(std::move(device)),limits_(limits) {
        if (!limits_.workspaces) throw std::invalid_argument("Vulkan workspace count must be positive");
        idle_.reserve(limits_.workspaces);
    }
    VulkanBinaryWorkspaceCache(const VulkanBinaryWorkspaceCache&) = delete;
    VulkanBinaryWorkspaceCache& operator=(const VulkanBinaryWorkspaceCache&) = delete;
    const VulkanBinaryDevice& device() const & noexcept { return device_; }
    const VulkanBinaryDevice& device() const && = delete;
    VulkanWorkspaceCacheStats stats() const {
        std::lock_guard lock(mutex_); auto stats=stats_; stats.idle=idle_.size(); return stats;
    }
    void clear() {
        std::lock_guard lock(mutex_);
        if(epoch_==std::numeric_limits<std::uint64_t>::max())throw std::length_error("Vulkan workspace clear epoch exhausted");
        ++epoch_;
        for (const auto& entry:idle_) { stats_.allocation_bytes-=entry->workspace->allocated_bytes(); ++stats_.discarded; }
        idle_.clear();
    }
    MixedBinaryResult execute(const PreparedPackedBinary& operation,runtime::CpuResourceCoordinator& coordinator,
        const MixedBinaryPolicy& policy) {
        if (policy.stop.stop_requested()) throw runtime::ExecutionCancelled{};
        std::unique_ptr<Entry> entry; std::uint64_t epoch{};
        {
            std::lock_guard lock(mutex_); epoch=epoch_;
            for (auto it=idle_.begin();it!=idle_.end();++it) if ((*it)->operation.same_owner(operation)) {
                entry=std::move(*it); idle_.erase(it); ++stats_.hits; break;
            }
            if (!entry) {
                // Free incompatible residency before asking the device allocator
                // for a new owner; live loans retain their own device allocations.
                for (const auto& old:idle_) { stats_.allocation_bytes-=old->workspace->allocated_bytes(); ++stats_.discarded; }
                idle_.clear();
                if (stats_.active>=limits_.workspaces) throw std::length_error("Vulkan workspace loan limit");
                entry=std::make_unique<Entry>(device_,operation,limits_.allocation_bytes-stats_.allocation_bytes);
                const auto bytes=entry->workspace->allocated_bytes();
                if (bytes>limits_.allocation_bytes-stats_.allocation_bytes)
                    throw std::length_error("Vulkan workspace allocation limit");
                stats_.allocation_bytes+=bytes; ++stats_.creations;
            }
            ++stats_.active;
        }
        try {
            auto result=entry->workspace->execute(operation,coordinator,policy);
            if (policy.stop.stop_requested()) throw runtime::ExecutionCancelled{};
            finish(std::move(entry),epoch,true); return result;
        } catch (...) { finish(std::move(entry),epoch,false); throw; }
    }
private:
    void finish(std::unique_ptr<Entry> entry,std::uint64_t epoch,bool success) noexcept {
        std::lock_guard lock(mutex_); --stats_.active;
        if (success && epoch==epoch_) idle_.push_back(std::move(entry));
        else { stats_.allocation_bytes-=entry->workspace->allocated_bytes(); ++stats_.discarded; entry.reset(); }
    }
    VulkanBinaryDevice device_; VulkanWorkspaceCacheLimits limits_;
    mutable std::mutex mutex_; std::vector<std::unique_ptr<Entry>> idle_;
    VulkanWorkspaceCacheStats stats_; std::uint64_t epoch_{};
};
} // namespace rfc::backend
