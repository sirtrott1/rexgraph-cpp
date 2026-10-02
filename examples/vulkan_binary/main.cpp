#include "rfc/backend/mixed_binary_calibration.hpp"
#include <iostream>
using namespace rfc;
namespace {
void quoted(const std::string& s) {
    std::cout<<'"';for(char raw:s){const auto c=static_cast<unsigned char>(raw);if(c=='"' || c=='\\')std::cout<<'\\'<<c;else if(c>=32)std::cout<<c;else std::cout<<'?';}std::cout<<'"';
}
}
int main(int argc,char** argv) {
    try {
        bool software=false;for(int i=1;i<argc;++i){if(std::string(argv[i])=="--allow-software")software=true;else throw std::invalid_argument("usage: vulkan_binary [--allow-software]");}
        const auto devices=backend::enumerate_vulkan_devices();std::size_t selected=0;
        if(!software){while(selected<devices.size() && (!devices[selected].physical_gpu || !devices[selected].compute))++selected;}
        if(selected==devices.size())throw std::runtime_error("no accessible physical compute GPU");
        backend::VulkanBinaryDevice device({selected,software,std::uint64_t{1}<<28});
        auto hardware=runtime::detect_hardware();runtime::CpuResourceCoordinator resources(hardware,{4,1<<24,100000,runtime::CpuWorkerBasis::AllowedLogicalThreads});
        auto schema=composite::AspectSchema::canonical();composite::AspectPlanes planes(schema,65537);
        for(std::size_t s=0;s<planes.slots();++s){planes.set(0,s,s%5!=0);planes.set(1,s,s%3==0);}
        auto program=composite::compile_active_orientation_query(0,1,true);
        backend::PreparedPackedBinary action(program,planes,{2});
        backend::MixedBinaryCalibration calibration(action,device,resources,
            {{0,1,64},{0,resources.capacity(),64},{action.words(),1,64},{action.words()/2,resources.capacity(),64}});
        auto result=calibration.execute(resources);
        std::cout<<"{\"devices\":[";
        for(std::size_t i=0;i<devices.size();++i){if(i)std::cout<<',';const auto& d=devices[i];std::cout<<"{\"name\":";quoted(d.name);
            std::cout<<",\"physical_gpu\":"<<(d.physical_gpu?"true":"false")<<",\"vendor\":"<<d.vendor<<",\"device\":"<<d.device<<",\"uuid\":[";
            for(std::size_t j=0;j<d.uuid.size();++j){if(j)std::cout<<',';std::cout<<static_cast<unsigned>(d.uuid[j]);}
            std::cout<<"],\"compute\":"<<(d.compute?"true":"false")<<",\"queue_family\":"<<d.compute_queue_family<<",\"heaps\":[";
            for(std::size_t h=0;h<d.heaps.size();++h){if(h)std::cout<<',';const auto& heap=d.heaps[h];std::cout<<"{\"index\":"<<h<<",\"bytes\":"<<heap.bytes<<",\"device_local\":"<<(heap.device_local?"true":"false")<<",\"advisory_budget\":";
                if(heap.advisory_budget)std::cout<<*heap.advisory_budget;else std::cout<<"null";
                std::cout<<",\"advisory_usage\":";if(heap.advisory_usage)std::cout<<*heap.advisory_usage;else std::cout<<"null";std::cout<<'}';}
            std::cout<<"],\"memory_types\":[";for(std::size_t t=0;t<d.memory_types.size();++t){if(t)std::cout<<',';const auto& m=d.memory_types[t];std::cout<<"{\"heap\":"<<m.heap<<",\"device_local\":"<<(m.device_local?"true":"false")<<",\"host_visible\":"<<(m.host_visible?"true":"false")<<",\"host_coherent\":"<<(m.host_coherent?"true":"false")<<'}';}
            std::cout<<"]}";}
        std::cout<<"],\"selected_device\":"<<selected<<",\"cpu_capacity\":"<<resources.capacity()<<",\"samples\":[";
        for(std::size_t i=0;i<calibration.samples().size();++i){if(i)std::cout<<',';const auto& s=calibration.samples()[i];std::cout<<"{\"gpu_words\":"<<s.candidate.gpu_words<<",\"cpu_workers\":"<<s.candidate.cpu_workers<<",\"median_complete_ns\":"<<s.median_ns<<'}';}
        std::cout<<"],\"selected_gpu_words\":"<<result.gpu_words<<",\"selected_cpu_words\":"<<result.cpu_words<<",\"active_negative_count\":"<<result.outputs.front().count()<<",\"warm_action_ns\":"<<result.timings.total_ns<<"}\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
