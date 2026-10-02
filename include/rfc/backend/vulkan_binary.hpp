#pragma once

#include "rfc/backend/packed_binary.hpp"
#include "rfc/runtime/resource_budget.hpp"
#include <array>
#include <functional>
#include <optional>
#include <string>

namespace rfc::backend {
struct VulkanMemoryHeap {
    std::uint64_t bytes{};
    bool device_local{};
    std::optional<std::uint64_t> advisory_budget,advisory_usage;
};
struct VulkanMemoryType {std::uint32_t heap{};bool device_local{},host_visible{},host_coherent{},host_cached{};};
struct VulkanDeviceInfo {
    std::string name;
    std::array<std::uint8_t,16> uuid{};
    std::uint32_t vendor{},device{},api_version{},driver_version{},device_type{},compute_queue_family{};
    bool physical_gpu{},compute{};
    std::uint32_t max_storage_buffer_bytes{},max_dispatch_x{},max_workgroup_x{},max_workgroup_invocations{};
    std::vector<VulkanMemoryHeap> heaps;
    std::vector<VulkanMemoryType> memory_types;
};
class VulkanError : public std::runtime_error {
public:
    VulkanError(std::string operation,std::int32_t result);
    std::int32_t result() const noexcept {return result_;}
private: std::int32_t result_;
};
struct VulkanBinaryOptions {
    std::size_t device_index{};
    bool allow_software_device{false};
    std::uint64_t allocation_byte_limit{std::uint64_t{1} << 30};
};
// Enumeration queries Vulkan itself. A loader without visible devices may throw.
std::vector<VulkanDeviceInfo> enumerate_vulkan_devices();
class VulkanBinaryDevice {
public:
    explicit VulkanBinaryDevice(VulkanBinaryOptions options={});
    const VulkanDeviceInfo& info() const & noexcept;
    const VulkanDeviceInfo& info() const &&=delete;
    std::uint64_t available_allocation_bytes() const;
    std::size_t selected_device_index() const noexcept;
private:
    struct State;
    std::shared_ptr<State> state_;
    friend class VulkanBinaryWorkspace;
};
struct MixedBinaryPolicy {
    std::size_t gpu_words{},cpu_workers{1},cpu_tile_words{64};
    std::size_t result_byte_limit{std::size_t{1} << 30};
    std::stop_token stop;
    // Diagnostic hook after submission, before CPU tiles. Failures still join.
    std::function<void()> after_submit;
};
struct MixedBinaryTimings {
    std::uint64_t host_upload_ns{},record_submit_ns{},cpu_ns{},completion_readback_ns{},total_ns{};
};
struct MixedBinaryResult {
    PreparedPackedBinary owner;
    VulkanDeviceInfo device;
    std::size_t gpu_words{},cpu_words{},cpu_workers{};
    runtime::CpuTileRunReport cpu_report;
    MixedBinaryTimings timings;
    std::vector<machine::PackedBits> outputs;
};
// A workspace pins its immutable input/device and cannot overlap or move.
class VulkanBinaryWorkspace {
public:
    VulkanBinaryWorkspace(const VulkanBinaryDevice&,const PreparedPackedBinary&);
    VulkanBinaryWorkspace(const VulkanBinaryDevice&,const PreparedPackedBinary&,std::uint64_t allocation_byte_limit);
    ~VulkanBinaryWorkspace();
    std::uint64_t allocated_bytes() const noexcept;
    VulkanBinaryWorkspace(const VulkanBinaryWorkspace&)=delete;
    VulkanBinaryWorkspace& operator=(const VulkanBinaryWorkspace&)=delete;
    MixedBinaryResult execute(const PreparedPackedBinary&,runtime::CpuResourceCoordinator&,
                              const MixedBinaryPolicy&);
private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace rfc::backend
