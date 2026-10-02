#include "rfc/backend/vulkan_binary.hpp"
#include "packed_binary_spirv.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>

namespace rfc::backend {
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t nanos(Clock::time_point a,Clock::time_point b) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count());
}
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS)throw VulkanError(operation,static_cast<std::int32_t>(result));
}
std::uint32_t u32(std::size_t value) {
    if(value>std::numeric_limits<std::uint32_t>::max())throw std::length_error("Vulkan shader index domain exceeded");
    return static_cast<std::uint32_t>(value);
}
struct Instance {
    VkInstance handle{};
    Instance() {
        VkApplicationInfo app{};app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;app.pApplicationName="RFC exact packed binary";app.apiVersion=VK_API_VERSION_1_1;
        VkInstanceCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;ci.pApplicationInfo=&app;
        check(vkCreateInstance(&ci,nullptr,&handle),"create Vulkan instance");
    }
    ~Instance(){if(handle)vkDestroyInstance(handle,nullptr);}
};
std::vector<VkPhysicalDevice> physical_devices(VkInstance instance) {
    for(unsigned retry=0;retry<8;++retry) {
        std::uint32_t count{};check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"enumerate Vulkan device count");
        std::vector<VkPhysicalDevice> devices(count);
        auto result=vkEnumeratePhysicalDevices(instance,&count,devices.data());
        if(result==VK_INCOMPLETE)continue;
        check(result,"enumerate Vulkan devices");devices.resize(count);return devices;
    }
    throw std::runtime_error("Vulkan device enumeration remained unstable");
}
VulkanDeviceInfo describe(VkPhysicalDevice physical) {
    VkPhysicalDeviceIDProperties id{};id.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceProperties2 props{};props.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;props.pNext=&id;
    vkGetPhysicalDeviceProperties2(physical,&props);
    const auto& p=props.properties;VulkanDeviceInfo out;
    out.name=p.deviceName;std::copy(std::begin(id.deviceUUID),std::end(id.deviceUUID),out.uuid.begin());
    out.vendor=p.vendorID;out.device=p.deviceID;out.api_version=p.apiVersion;out.driver_version=p.driverVersion;out.device_type=p.deviceType;
    out.physical_gpu=p.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU || p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    out.max_storage_buffer_bytes=p.limits.maxStorageBufferRange;out.max_dispatch_x=p.limits.maxComputeWorkGroupCount[0];
    out.max_workgroup_x=p.limits.maxComputeWorkGroupSize[0];out.max_workgroup_invocations=p.limits.maxComputeWorkGroupInvocations;
    std::uint32_t count{};vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,queues.data());
    for(std::uint32_t i=0;i<count;++i)if(queues[i].queueCount && (queues[i].queueFlags&VK_QUEUE_COMPUTE_BIT)) {
        out.compute=true;out.compute_queue_family=i;break;
    }
    check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,nullptr),"enumerate Vulkan memory extensions");
    std::vector<VkExtensionProperties> extensions(count);
    const auto extension_result=vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,extensions.data());
    check(extension_result,"enumerate Vulkan memory extension values");
    const bool budget=std::any_of(extensions.begin(),extensions.end(),[](const auto& e){return std::strcmp(e.extensionName,VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)==0;});
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgets{};budgets.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    VkPhysicalDeviceMemoryProperties2 memory{};memory.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    if(budget)memory.pNext=&budgets;
    vkGetPhysicalDeviceMemoryProperties2(physical,&memory);
    const auto& m=memory.memoryProperties;
    for(std::uint32_t i=0;i<m.memoryHeapCount;++i) {
        VulkanMemoryHeap heap{m.memoryHeaps[i].size,(m.memoryHeaps[i].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)!=0,{},{}};
        if(budget){heap.advisory_budget=budgets.heapBudget[i];heap.advisory_usage=budgets.heapUsage[i];}
        out.heaps.push_back(heap);
    }
    for(std::uint32_t i=0;i<m.memoryTypeCount;++i) {
        const auto f=m.memoryTypes[i].propertyFlags;
        out.memory_types.push_back({m.memoryTypes[i].heapIndex,(f&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)!=0,
            (f&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)!=0,(f&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0,(f&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)!=0});
    }
    return out;
}
}
VulkanError::VulkanError(std::string operation,std::int32_t result)
    :std::runtime_error(std::move(operation)+": VkResult "+std::to_string(result)),result_(result){}
std::vector<VulkanDeviceInfo> enumerate_vulkan_devices() {
    Instance instance;std::vector<VulkanDeviceInfo> result;
    for(auto physical:physical_devices(instance.handle))result.push_back(describe(physical));
    return result;
}
struct VulkanBinaryDevice::State {
    std::shared_ptr<Instance> instance;
    VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};
    VulkanDeviceInfo info;
    VkPhysicalDeviceMemoryProperties memory{};
    VkPhysicalDeviceProperties properties{};
    std::mutex queue_mutex,budget_mutex;
    std::uint64_t byte_limit{},used_bytes{};std::uint32_t allocations{};
    std::size_t selected_index{};
    std::atomic_bool lost{};
    ~State(){if(device)vkDestroyDevice(device,nullptr);}
    void checked(VkResult r,const char* operation) {
        if(r==VK_ERROR_DEVICE_LOST)lost.store(true,std::memory_order_release);
        check(r,operation);
    }
    void healthy() const {if(lost.load(std::memory_order_acquire))throw VulkanError("Vulkan device is lost",VK_ERROR_DEVICE_LOST);}
};
VulkanBinaryDevice::VulkanBinaryDevice(VulkanBinaryOptions options):state_(std::make_shared<State>()) {
    auto& s=*state_;s.instance=std::make_shared<Instance>();
    const auto devices=physical_devices(s.instance->handle);
    if(options.device_index>=devices.size())throw std::out_of_range("Vulkan selected device absent");
    s.physical=devices[options.device_index];s.info=describe(s.physical);s.selected_index=options.device_index;
    if(!s.info.physical_gpu && !options.allow_software_device)throw std::invalid_argument("Vulkan requires a physical GPU; software admission must be explicit");
    if(!s.info.compute || s.info.api_version<VK_API_VERSION_1_1 || s.info.max_workgroup_x<64 || s.info.max_workgroup_invocations<64)
        throw std::invalid_argument("Vulkan device does not support this packed compute profile");
    s.byte_limit=options.allocation_byte_limit;vkGetPhysicalDeviceMemoryProperties(s.physical,&s.memory);
    vkGetPhysicalDeviceProperties(s.physical,&s.properties);
    float priority=1;VkDeviceQueueCreateInfo qi{};qi.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex=s.info.compute_queue_family;qi.queueCount=1;qi.pQueuePriorities=&priority;
    VkDeviceCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;ci.queueCreateInfoCount=1;ci.pQueueCreateInfos=&qi;
    check(vkCreateDevice(s.physical,&ci,nullptr,&s.device),"create Vulkan compute device");
    vkGetDeviceQueue(s.device,s.info.compute_queue_family,0,&s.queue);
}
const VulkanDeviceInfo& VulkanBinaryDevice::info() const & noexcept {return state_->info;}
std::size_t VulkanBinaryDevice::selected_device_index() const noexcept {return state_->selected_index;}
std::uint64_t VulkanBinaryDevice::available_allocation_bytes() const {
    std::lock_guard lock(state_->budget_mutex);return state_->byte_limit-state_->used_bytes;
}
struct VulkanBinaryWorkspace::State {
    struct Buffer {
        std::shared_ptr<VulkanBinaryDevice::State> device;
        VkBuffer buffer{};VkDeviceMemory memory{};void* mapped{};
        VkDeviceSize bytes{},allocated{};bool coherent{},charged{};
        Buffer(std::shared_ptr<VulkanBinaryDevice::State> d,std::size_t size,std::uint64_t byte_limit):device(std::move(d)),bytes(std::max<std::size_t>(4,size)) {
            auto& s=*device;s.healthy();
            if(bytes>s.info.max_storage_buffer_bytes)throw std::length_error("Vulkan storage buffer limit");
            try {
                VkBufferCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;ci.size=bytes;
                ci.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
                s.checked(vkCreateBuffer(s.device,&ci,nullptr,&buffer),"create packed buffer");
                VkMemoryRequirements required{};vkGetBufferMemoryRequirements(s.device,buffer,&required);allocated=required.size;
                if(allocated>byte_limit)throw std::length_error("Vulkan workspace allocation limit");
                std::optional<std::uint32_t> type;int best=-1;
                for(std::uint32_t t=0;t<s.memory.memoryTypeCount;++t) {
                    const auto f=s.memory.memoryTypes[t].propertyFlags;
                    if(!(required.memoryTypeBits&(std::uint32_t{1}<<t)) || !(f&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))continue;
                    const int score=((f&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)?2:0)+((f&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)?1:0);
                    if(score>best){type=t;best=score;}
                }
                if(!type)throw std::invalid_argument("Vulkan packed profile needs host-visible storage buffer memory");
                coherent=(s.memory.memoryTypes[*type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
                {
                    std::lock_guard lock(s.budget_mutex);
                    if(allocated>s.byte_limit-s.used_bytes || s.allocations>=s.properties.limits.maxMemoryAllocationCount)
                        throw std::length_error("Vulkan cooperating allocation budget occupied");
                    s.used_bytes+=allocated;++s.allocations;charged=true;
                }
                VkMemoryAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;ai.allocationSize=allocated;ai.memoryTypeIndex=*type;
                s.checked(vkAllocateMemory(s.device,&ai,nullptr,&memory),"allocate packed buffer");
                s.checked(vkBindBufferMemory(s.device,buffer,memory,0),"bind packed buffer");
                s.checked(vkMapMemory(s.device,memory,0,VK_WHOLE_SIZE,0,&mapped),"map packed buffer");
            } catch(...) {release();throw;}
        }
        ~Buffer(){release();}
        void release() noexcept {
            auto& s=*device;
            if(mapped)vkUnmapMemory(s.device,memory);
            if(buffer)vkDestroyBuffer(s.device,buffer,nullptr);
            if(memory)vkFreeMemory(s.device,memory,nullptr);
            mapped=nullptr;buffer={};memory={};
            if(charged){std::lock_guard lock(s.budget_mutex);s.used_bytes-=allocated;--s.allocations;charged=false;}
        }
        void flush(bool invalidate=false) {
            if(coherent)return;
            VkMappedMemoryRange range{};range.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;range.memory=memory;range.size=VK_WHOLE_SIZE;
            if(invalidate)device->checked(vkInvalidateMappedMemoryRanges(device->device,1,&range),"invalidate packed output");
            else device->checked(vkFlushMappedMemoryRanges(device->device,1,&range),"flush packed input");
        }
    };
    std::shared_ptr<VulkanBinaryDevice::State> device;
    PreparedPackedBinary owner;
    std::uint64_t allocation_byte_limit;
    std::atomic_bool busy{};
    std::vector<std::uint32_t> input,code;
    std::array<std::unique_ptr<Buffer>,4> buffers;
    VkDescriptorSetLayout descriptor_layout{};VkPipelineLayout pipeline_layout{};VkPipeline pipeline{};
    VkDescriptorPool descriptor_pool{};VkDescriptorSet descriptor{};VkCommandPool command_pool{};VkCommandBuffer command{};VkFence fence{};
    std::uint32_t halfwords{},output_offset{},tail_lo{~std::uint32_t{0}},tail_hi{~std::uint32_t{0}};
    State(std::shared_ptr<VulkanBinaryDevice::State> d,const PreparedPackedBinary& p,std::uint64_t limit):device(std::move(d)),owner(p),allocation_byte_limit(limit){}
    ~State() {
        const auto d=device->device;
        if(fence)vkDestroyFence(d,fence,nullptr);
        if(command_pool)vkDestroyCommandPool(d,command_pool,nullptr);
        if(descriptor_pool)vkDestroyDescriptorPool(d,descriptor_pool,nullptr);
        if(pipeline)vkDestroyPipeline(d,pipeline,nullptr);
        if(pipeline_layout)vkDestroyPipelineLayout(d,pipeline_layout,nullptr);
        if(descriptor_layout)vkDestroyDescriptorSetLayout(d,descriptor_layout,nullptr);
    }
    void initialize() {
        auto& s=*device;s.healthy();
        halfwords=u32(machine::checked_size_mul(owner.words(),2));
        const auto input_count=machine::checked_size_mul(halfwords,owner.planes().aspect_count());
        const auto code_count=machine::checked_size_add(machine::checked_size_mul(owner.program().instruction_count(),6),owner.outputs().size());
        const auto scratch_count=machine::checked_size_mul(halfwords,owner.program().register_count());
        const auto output_count=machine::checked_size_mul(halfwords,owner.outputs().size());
        for(auto count:{input_count,code_count,scratch_count,output_count}) {
            (void)u32(count);
            if(machine::checked_size_mul(count,4)>s.info.max_storage_buffer_bytes)throw std::length_error("Vulkan packed buffer exceeds device limit");
        }
        if(machine::ceil_div(halfwords,64)>s.info.max_dispatch_x)throw std::length_error("Vulkan packed dispatch limit");
        input.resize(input_count);code.reserve(code_count);
        for(std::size_t a=0;a<owner.planes().aspect_count();++a)for(std::size_t w=0;w<owner.words();++w) {
            const auto value=owner.planes().plane(static_cast<composite::AspectId>(a)).word(w);
            input[a*halfwords+w*2]=static_cast<std::uint32_t>(value);
            input[a*halfwords+w*2+1]=static_cast<std::uint32_t>(value>>32);
        }
        for(const auto& i:owner.program().instructions()) {
            code.insert(code.end(),{static_cast<std::uint32_t>(i.opcode),i.dst,i.a,i.b,i.c,i.aspect});
        }
        output_offset=u32(code.size());for(auto out:owner.outputs())code.push_back(out);
        const auto tail=owner.planes().slots()%64;
        if(tail) {
            tail_lo=tail>=32?~std::uint32_t{0}:((std::uint32_t{1}<<tail)-1);
            tail_hi=tail<=32?0:((std::uint32_t{1}<<(tail-32))-1);
        }
        const std::array<std::size_t,4> counts{input_count,code_count,scratch_count,output_count};
        auto remaining=allocation_byte_limit;
        for(std::size_t i=0;i<4;++i) {
            buffers[i]=std::make_unique<Buffer>(device,machine::checked_size_mul(counts[i],4),remaining);
            remaining-=buffers[i]->allocated;
        }
        std::array<VkDescriptorSetLayoutBinding,4> bindings{};
        for(std::uint32_t i=0;i<4;++i)bindings[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo li{};li.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;li.bindingCount=4;li.pBindings=bindings.data();
        s.checked(vkCreateDescriptorSetLayout(s.device,&li,nullptr,&descriptor_layout),"create packed descriptor layout");
        VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,32};
        VkPipelineLayoutCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;pi.setLayoutCount=1;pi.pSetLayouts=&descriptor_layout;pi.pushConstantRangeCount=1;pi.pPushConstantRanges=&push;
        s.checked(vkCreatePipelineLayout(s.device,&pi,nullptr,&pipeline_layout),"create packed pipeline layout");
        VkShaderModuleCreateInfo mi{};mi.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;mi.codeSize=sizeof(packed_binary_spirv);mi.pCode=packed_binary_spirv;
        VkShaderModule module{};s.checked(vkCreateShaderModule(s.device,&mi,nullptr,&module),"create packed shader module");
        VkComputePipelineCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;ci.layout=pipeline_layout;
        ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module;ci.stage.pName="main";
        const auto pipeline_result=vkCreateComputePipelines(s.device,{},1,&ci,nullptr,&pipeline);
        vkDestroyShaderModule(s.device,module,nullptr);s.checked(pipeline_result,"create packed compute pipeline");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4};
        VkDescriptorPoolCreateInfo pool{};pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
        s.checked(vkCreateDescriptorPool(s.device,&pool,nullptr,&descriptor_pool),"create packed descriptor pool");
        VkDescriptorSetAllocateInfo dai{};dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;dai.descriptorPool=descriptor_pool;dai.descriptorSetCount=1;dai.pSetLayouts=&descriptor_layout;
        s.checked(vkAllocateDescriptorSets(s.device,&dai,&descriptor),"allocate packed descriptors");
        std::array<VkDescriptorBufferInfo,4> bi{};std::array<VkWriteDescriptorSet,4> writes{};
        for(std::uint32_t i=0;i<4;++i) {
            bi[i]={buffers[i]->buffer,0,buffers[i]->bytes};
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=descriptor;writes[i].dstBinding=i;
            writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&bi[i];
        }
        vkUpdateDescriptorSets(s.device,4,writes.data(),0,nullptr);
        VkCommandPoolCreateInfo cp{};cp.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;cp.queueFamilyIndex=s.info.compute_queue_family;cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        s.checked(vkCreateCommandPool(s.device,&cp,nullptr,&command_pool),"create packed command pool");
        VkCommandBufferAllocateInfo ca{};ca.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;ca.commandPool=command_pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        s.checked(vkAllocateCommandBuffers(s.device,&ca,&command),"allocate packed command buffer");
        VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;s.checked(vkCreateFence(s.device,&fi,nullptr,&fence),"create packed completion fence");
    }
};
VulkanBinaryWorkspace::VulkanBinaryWorkspace(const VulkanBinaryDevice& d,const PreparedPackedBinary& p)
    :VulkanBinaryWorkspace(d,p,std::numeric_limits<std::uint64_t>::max()){}
VulkanBinaryWorkspace::VulkanBinaryWorkspace(const VulkanBinaryDevice& d,const PreparedPackedBinary& p,std::uint64_t byte_limit)
    :state_(std::make_unique<State>(d.state_,p,byte_limit)){state_->initialize();}
VulkanBinaryWorkspace::~VulkanBinaryWorkspace()=default;
std::uint64_t VulkanBinaryWorkspace::allocated_bytes() const noexcept {
    std::uint64_t bytes{};
    for (const auto& buffer:state_->buffers) if (buffer) bytes+=buffer->allocated;
    return bytes;
}
MixedBinaryResult VulkanBinaryWorkspace::execute(const PreparedPackedBinary& requested_owner,runtime::CpuResourceCoordinator& coordinator,
                                                const MixedBinaryPolicy& requested_policy) {
    const auto owner=requested_owner;
    const auto policy=requested_policy;
    auto& s=*state_;
    if(s.busy.exchange(true,std::memory_order_acquire))throw std::logic_error("Vulkan workspace already executing");
    struct Release {std::atomic_bool& busy;~Release(){busy.store(false,std::memory_order_release);}} release{s.busy};
    const auto start=Clock::now();s.device->healthy();
    if(!s.owner.same_owner(owner))throw std::invalid_argument("Vulkan workspace prepared owner mismatch");
    if(policy.gpu_words>owner.words() || !policy.cpu_tile_words || !policy.cpu_workers)throw std::invalid_argument("mixed binary partition/policy");
    if(policy.stop.stop_requested())throw std::runtime_error("packed execution cancelled");
    const auto cpu_words=owner.words()-policy.gpu_words;
    const auto tiles=machine::ceil_div(cpu_words,policy.cpu_tile_words);
    if(tiles>coordinator.policy().tile_limit)throw std::length_error("mixed CPU tile budget");
    const auto workers=cpu_words?policy.cpu_workers:std::size_t{1};
    const auto bytes=machine::checked_size_mul(machine::checked_size_mul(owner.words(),owner.outputs().size()),sizeof(std::uint64_t));
    auto reservation=coordinator.reserve(workers,bytes);
    auto output=owner.empty_result(policy.result_byte_limit);
    runtime::CpuTileExecutor cpu({workers,coordinator.capacity(),coordinator.policy().tile_limit});
    auto& d=*s.device;MixedBinaryTimings timings;runtime::CpuTileRunReport report{};
    const bool submitted=policy.gpu_words!=0;
    if(submitted) {
        if(!s.input.empty())std::memcpy(s.buffers[0]->mapped,s.input.data(),s.input.size()*4);
        if(!s.code.empty())std::memcpy(s.buffers[1]->mapped,s.code.data(),s.code.size()*4);
        s.buffers[0]->flush();s.buffers[1]->flush();
    }
    const auto uploaded=Clock::now();timings.host_upload_ns=nanos(start,uploaded);
    if(submitted) {
        d.checked(vkResetFences(d.device,1,&s.fence),"reset packed fence");
        d.checked(vkResetCommandBuffer(s.command,0),"reset packed command");
        VkCommandBufferBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        d.checked(vkBeginCommandBuffer(s.command,&begin),"begin packed command");
        vkCmdBindPipeline(s.command,VK_PIPELINE_BIND_POINT_COMPUTE,s.pipeline);
        vkCmdBindDescriptorSets(s.command,VK_PIPELINE_BIND_POINT_COMPUTE,s.pipeline_layout,0,1,&s.descriptor,0,nullptr);
        const std::array<std::uint32_t,8> params{s.halfwords,u32(policy.gpu_words*2),owner.program().register_count(),
            u32(owner.program().instruction_count()),u32(owner.outputs().size()),s.output_offset,s.tail_lo,s.tail_hi};
        vkCmdPushConstants(s.command,s.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(params),params.data());
        vkCmdDispatch(s.command,u32(machine::ceil_div(policy.gpu_words*2,64)),1,1);
        VkMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        d.checked(vkEndCommandBuffer(s.command),"end packed command");
        VkSubmitInfo submit{};submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;submit.commandBufferCount=1;submit.pCommandBuffers=&s.command;
        std::lock_guard lock(d.queue_mutex);d.healthy();d.checked(vkQueueSubmit(d.queue,1,&submit,s.fence),"submit packed kernel");
    }
    const auto queued=Clock::now();timings.record_submit_ns=nanos(uploaded,queued);
    std::exception_ptr error;
    try {
        if(submitted && policy.after_submit)policy.after_submit();
        report=cpu.run_tiles(tiles,[&](auto tile,auto,const auto& token) {
            token.check();const auto begin=policy.gpu_words+tile*policy.cpu_tile_words;
            const auto end=begin+std::min(policy.cpu_tile_words,owner.words()-begin);
            owner.cpu_words(begin,end,output,policy.stop);token.check();
        });
    } catch(...) {error=std::current_exception();}
    const auto cpu_done=Clock::now();timings.cpu_ns=nanos(queued,cpu_done);
    if(submitted) {
        const auto waited=vkWaitForFences(d.device,1,&s.fence,VK_TRUE,std::numeric_limits<std::uint64_t>::max());
        if(waited!=VK_SUCCESS && waited!=VK_ERROR_DEVICE_LOST) {
            std::lock_guard lock(d.queue_mutex);d.checked(vkDeviceWaitIdle(d.device),"quiesce failed packed wait");
        }
        d.checked(waited,"wait packed completion");
    }
    if(error)std::rethrow_exception(error);
    if(policy.stop.stop_requested())throw std::runtime_error("packed execution cancelled after joined completion");
    if(submitted) {
        s.buffers[3]->flush(true);const auto* halves=static_cast<const std::uint32_t*>(s.buffers[3]->mapped);
        const auto active=policy.gpu_words*2;
        for(std::size_t o=0;o<output.size();++o)for(std::size_t w=0;w<policy.gpu_words;++w)
            output[o].assign_word(w,std::uint64_t{halves[o*active+w*2]}|(std::uint64_t{halves[o*active+w*2+1]}<<32));
    }
    const auto finished=Clock::now();timings.completion_readback_ns=nanos(cpu_done,finished);timings.total_ns=nanos(start,finished);
    return {owner,d.info,policy.gpu_words,cpu_words,workers,report,timings,std::move(output)};
}
} // namespace rfc::backend
