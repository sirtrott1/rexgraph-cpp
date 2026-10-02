#pragma once

#include "rfc/backend/binary_work.hpp"
#include "rfc/backend/native_vulkan_binary.hpp"
#include "rfc/backend/vulkan_workspace_cache.hpp"

namespace rfc::backend {
namespace vulkan_work_detail {
inline std::size_t workers(const PreparedPackedBinary& operation,const MixedBinaryPolicy& policy) {
    if(policy.gpu_words>operation.words() || !policy.cpu_workers || !policy.cpu_tile_words)
        throw std::invalid_argument("Vulkan work partition policy");
    return policy.gpu_words==operation.words()?1:policy.cpu_workers;
}
inline runtime::WorkDemand demand(const PreparedPackedBinary& operation,const MixedBinaryPolicy& policy,bool copy_inputs=false) {
    auto d=binary_work_detail::demand(operation,workers(operation,policy),copy_inputs);
    const auto upload=machine::checked_size_mul(machine::checked_size_mul(operation.words(),operation.planes().aspect_count()),8);
    const auto code=machine::checked_size_mul(machine::checked_size_add(machine::checked_size_mul(operation.program().instruction_count(),6),operation.outputs().size()),4);
    d.buffer_bytes=machine::checked_size_add(d.buffer_bytes,machine::checked_size_add(upload,code));return d;
}
inline PackedWorkDeviceReport report(const VulkanBinaryDevice& device,const MixedBinaryResult& result) {
    return {result.device.name,result.device.uuid,result.device.physical_gpu,device.selected_device_index(),
        result.gpu_words,result.cpu_words,result.cpu_workers,result.timings.host_upload_ns,result.timings.record_submit_ns,
        result.timings.cpu_ns,result.timings.completion_readback_ns,result.timings.total_ns};
}
inline MixedBinaryResult execute(const PreparedPackedBinary& operation,const VulkanBinaryDevice& device,
    runtime::CpuResourceCoordinator& assigned,MixedBinaryPolicy policy,std::stop_token stop,
    VulkanBinaryWorkspaceCache* cache=nullptr) {
    std::stop_source joined;
    std::stop_callback work_cancel(stop,[&]{joined.request_stop();});
    std::stop_callback host_cancel(policy.stop,[&]{joined.request_stop();});
    policy.stop=joined.get_token();if(policy.stop.stop_requested())throw runtime::ExecutionCancelled{};
    try {
        if(cache)return cache->execute(operation,assigned,policy);
        VulkanBinaryWorkspace workspace(device,operation);return workspace.execute(operation,assigned,policy);
    }
    catch(...) {if(policy.stop.stop_requested())throw runtime::ExecutionCancelled{};throw;}
}
} // namespace vulkan_work_detail
inline runtime::WorkJob make_vulkan_binary_work(runtime::WorkKey key,const PreparedPackedBinary& operation,
    const PackedWorkCarrier& carrier,std::shared_ptr<VulkanBinaryWorkspaceCache> cache,MixedBinaryPolicy policy,
    std::vector<runtime::WorkKey> after={}) {
    if(!cache)throw std::invalid_argument("Vulkan work needs an owning workspace service");
    binary_work_detail::validate(operation,carrier,vulkan_work_detail::workers(operation,policy),policy.cpu_tile_words,policy.result_byte_limit);
    return runtime::WorkJob::make<PackedWorkResult>(key,vulkan_work_detail::demand(operation,policy),std::move(after),
        [key,operation,carrier,cache=std::move(cache),policy](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto result=vulkan_work_detail::execute(operation,cache->device(),assigned,policy,stop,cache.get());
            auto report=vulkan_work_detail::report(cache->device(),result);
            return PackedWorkResult{key,carrier,operation,std::move(result.outputs),{},{},{},std::move(report)};
        });
}
inline runtime::WorkJob make_vulkan_binary_work(runtime::WorkKey key,const PackedWorkRecipe& recipe,
    std::shared_ptr<VulkanBinaryWorkspaceCache> cache,MixedBinaryPolicy policy,std::optional<runtime::WorkAvailability> cutoff={}) {
    if(!cache)throw std::invalid_argument("Vulkan work needs an owning workspace service");
    binary_work_detail::validate(recipe.prototype(),recipe.carrier(),vulkan_work_detail::workers(recipe.prototype(),policy),policy.cpu_tile_words,policy.result_byte_limit);
    return runtime::WorkJob::make<PackedWorkResult>(key,vulkan_work_detail::demand(recipe.prototype(),policy,true),recipe.dependencies(),
        [key,recipe,cache=std::move(cache),policy,cutoff](const runtime::WorkInputs& inputs,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto input=recipe.materialize(inputs,cutoff);
            auto result=vulkan_work_detail::execute(input.operation,cache->device(),assigned,policy,stop,cache.get());
            auto report=vulkan_work_detail::report(cache->device(),result);
            return recipe.result(key,std::move(input),std::move(result.outputs),std::move(report));
        });
}
inline runtime::WorkJob make_native_vulkan_work(runtime::WorkKey key,const PreparedNativeVulkanBinary& native,
    const PackedWorkCarrier& carrier,std::shared_ptr<VulkanBinaryWorkspaceCache> cache,MixedBinaryPolicy policy,
    std::optional<runtime::WorkAvailability> cutoff={},std::vector<runtime::WorkKey> after={}) {
    if(!cache)throw std::invalid_argument("Vulkan work needs an owning workspace service");
    const auto pin=native.publication();runtime::admit_work_availability(pin.source_namespace(),pin.endpoint()->sequence,cutoff);
    binary_work_detail::validate(native.packed(),carrier,vulkan_work_detail::workers(native.packed(),policy),policy.cpu_tile_words,policy.result_byte_limit);
    const auto authority=runtime::WorkValue::own(pin);
    return runtime::WorkJob::make<PackedWorkResult>(key,vulkan_work_detail::demand(native.packed(),policy),std::move(after),
        [key,native,carrier,cache=std::move(cache),policy,authority](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto result=vulkan_work_detail::execute(native.packed(),cache->device(),assigned,policy,stop,cache.get());
            auto report=vulkan_work_detail::report(cache->device(),result);const auto publication=native.publication();
            return PackedWorkResult{key,carrier,native.packed(),std::move(result.outputs),{},{},
                {{publication.source_namespace(),publication.endpoint()->sequence,authority}},std::move(report)};
        });
}
inline runtime::WorkJob make_vulkan_binary_work(runtime::WorkKey key,const PreparedPackedBinary& operation,
    const PackedWorkCarrier& carrier,const VulkanBinaryDevice& device,MixedBinaryPolicy policy,
    std::vector<runtime::WorkKey> after={}) {
    binary_work_detail::validate(operation,carrier,vulkan_work_detail::workers(operation,policy),policy.cpu_tile_words,policy.result_byte_limit);
    return runtime::WorkJob::make<PackedWorkResult>(key,vulkan_work_detail::demand(operation,policy),std::move(after),
        [key,operation,carrier,device,policy](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto result=vulkan_work_detail::execute(operation,device,assigned,policy,stop);
            auto report=vulkan_work_detail::report(device,result);
            return PackedWorkResult{key,carrier,operation,std::move(result.outputs),{},{},{},std::move(report)};
        });
}
inline runtime::WorkJob make_vulkan_binary_work(runtime::WorkKey key,const PackedWorkRecipe& recipe,
    const VulkanBinaryDevice& device,MixedBinaryPolicy policy,std::optional<runtime::WorkAvailability> cutoff={}) {
    binary_work_detail::validate(recipe.prototype(),recipe.carrier(),vulkan_work_detail::workers(recipe.prototype(),policy),policy.cpu_tile_words,policy.result_byte_limit);
    return runtime::WorkJob::make<PackedWorkResult>(key,vulkan_work_detail::demand(recipe.prototype(),policy,true),recipe.dependencies(),
        [key,recipe,device,policy,cutoff](const runtime::WorkInputs& inputs,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto input=recipe.materialize(inputs,cutoff);
            auto result=vulkan_work_detail::execute(input.operation,device,assigned,policy,stop);
            auto report=vulkan_work_detail::report(device,result);
            return recipe.result(key,std::move(input),std::move(result.outputs),std::move(report));
        });
}
inline runtime::WorkJob make_native_vulkan_work(runtime::WorkKey key,const PreparedNativeVulkanBinary& native,
    const PackedWorkCarrier& carrier,const VulkanBinaryDevice& device,MixedBinaryPolicy policy,
    std::optional<runtime::WorkAvailability> cutoff={},std::vector<runtime::WorkKey> after={}) {
    const auto pin=native.publication();runtime::admit_work_availability(pin.source_namespace(),pin.endpoint()->sequence,cutoff);
    binary_work_detail::validate(native.packed(),carrier,vulkan_work_detail::workers(native.packed(),policy),policy.cpu_tile_words,policy.result_byte_limit);
    const auto authority=runtime::WorkValue::own(pin);
    return runtime::WorkJob::make<PackedWorkResult>(key,vulkan_work_detail::demand(native.packed(),policy),std::move(after),
        [key,native,carrier,device,policy,authority](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto result=vulkan_work_detail::execute(native.packed(),device,assigned,policy,stop);
            auto report=vulkan_work_detail::report(device,result);const auto publication=native.publication();
            return PackedWorkResult{key,carrier,native.packed(),std::move(result.outputs),{},{},
                {{publication.source_namespace(),publication.endpoint()->sequence,authority}},std::move(report)};
        });
}
} // namespace rfc::backend
