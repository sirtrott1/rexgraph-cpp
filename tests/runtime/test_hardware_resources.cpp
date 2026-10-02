#include "rfc/runtime/resource_budget.hpp"
#include <barrier>
#include <iostream>
#include <thread>

using namespace rfc::runtime;
namespace {
std::size_t checks{};
void check(bool condition,const char* message) { ++checks;if(!condition)throw std::runtime_error(message); }
template<class F>void rejects(F&& function,const char* message) {
    bool rejected{};try { function(); }catch(const std::exception&) { rejected=true; }check(rejected,message);
}
struct Fixture {
    std::map<std::string,std::string> files;
    std::vector<std::size_t> allowed{0,1,8,9};
    Fixture() {
        for(const auto id:allowed)files["/proc/cpuinfo"]+="processor : "+std::to_string(id)+"\nphysical id : "+std::to_string(id/8)+
            "\ncore id : 0\nmodel name : test CPU\nflags : avx2 sse2 avx2\n\n";
        files["/proc/meminfo"]="MemTotal: 16384 kB\nMemAvailable: 8192 kB\n";
        files["/proc/self/cgroup"]="0::/tenant/job\n";
        files["/proc/self/mountinfo"]="12 1 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw\n";
        files["/sys/fs/cgroup/tenant/job/cpu.max"]="max 100000\n";
        files["/sys/fs/cgroup/tenant/cpu.max"]="150000 100000\n";
        files["/sys/fs/cgroup/cpu.max"]="max 100000\n";
        files["/sys/fs/cgroup/tenant/job/memory.max"]="10485760\n";
        files["/sys/fs/cgroup/tenant/job/memory.current"]="1048576\n";
        files["/sys/fs/cgroup/tenant/memory.max"]="6291456\n";
        files["/sys/fs/cgroup/tenant/memory.current"]="2097152\n";
        for(const auto id:allowed) {
            const auto p="/sys/devices/system/cpu/cpu"+std::to_string(id)+"/cache/index0/";
            files[p+"level"]="3";files[p+"type"]="Unified";files[p+"size"]="32M";files[p+"coherency_line_size"]="64";
            files[p+"shared_cpu_list"]=id<8?"0-1":"8-9";
        }
        files["/proc/bus/pci/devices"]="c400\t10021586\t00\n0100\t10de1234\t00\n";
        files["/sys/bus/pci/devices/0000:c4:00.0/class"]="0x030000";
        std::string config(64,'\0');config[11]=3;
        files["/proc/bus/pci/01/00.0"]=config;
    }
    HardwareInventory inspect(HardwareProbeLimits limits={}) const {
        return probe_linux_hardware({[&](const std::string& p)->std::optional<std::string> {
            const auto it=files.find(p);return it==files.end()?std::nullopt:std::optional(it->second);
        },allowed,{},limits});
    }
};
void observations() {
    Fixture fixture;const auto h=fixture.inspect();
    check(h.affinity_observed && h.allowed_cpus.size()==4,"allowed logical CPU count changed");
    check(h.physical_cores()==2,"package/core identities collapsed across packages");
    check(h.allowed_cpus[2].package_id==1 && h.allowed_cpus[2].logical_id==8,"sparse allowed CPU id lost");
    check(h.allowed_cpus[0].instruction_flags==std::vector<std::string>({"avx2","sse2"}),"CPU flags not explicit/deduplicated");
    check(h.caches.size()==2 && h.caches[0].bytes==32u*1024u*1024u,"shared caches double counted by logical CPU");
    check(!h.stacked_3d_cache.has_value(),"cache size inferred 3D V-Cache");
    check(h.ram_total_bytes==16u*1024u*1024u && h.ram_available_bytes==8u*1024u*1024u,"exact KiB conversion incorrect");
    check(h.cgroups.size()==3 && h.advisory_memory_headroom()==4u*1024u*1024u,"parent cgroup memory ceiling ignored");
    check(admitted_cpu_workers(h,{8,0,100,CpuWorkerBasis::AllowedLogicalThreads})==2,"parent fractional CPU quota ignored");
    check(admitted_cpu_workers(h,{8,0,100,CpuWorkerBasis::ObservedPhysicalCoreCount})==2,"physical core budget changed");
    check(h.pci_devices.size()==2 && h.pci_devices[0].address=="0000:c4:00.0" && h.pci_devices[0].class_code==0x030000,
        "PCI identity/class observation incorrect");
    check(h.pci_devices[1].class_code==0x030000,"PCI config class fallback missing");
    check(h.visible_compute_nodes.empty(),"PCI presence inferred compute access");
    fixture.files["/sys/fs/cgroup/tenant/memory.current"]="99999999";
    check(fixture.inspect().advisory_memory_headroom()==0,"exhausted cgroup headroom underflowed");
    fixture.files["/proc/self/mountinfo"]="12 1 0:1 /tenant /cgroup\\040mount rw - cgroup2 cgroup rw\n";
    fixture.files["/cgroup mount/job/cpu.max"]="25000 100000";
    fixture.files["/cgroup mount/cpu.max"]="max 100000";
    const auto namespaced=fixture.inspect();
    check(namespaced.cgroups.size()==2 && namespaced.cgroups[0].path=="/cgroup mount/job","non-root cgroup mount resolved incorrectly");
    check(admitted_cpu_workers(namespaced,{8,0,100})==1,"sub-one CPU quota treated as zero or full host");
    fixture.files["/cgroup mount/job/cpu.max"]="0 100000";
    const auto invalid_quota=fixture.inspect();
    rejects([&]{CpuResourceCoordinator coordinator(invalid_quota);},"malformed observed quota admitted as unlimited");
    fixture.files.erase("/proc/self/mountinfo");fixture.files.erase("/proc/cpuinfo");fixture.files.erase("/proc/meminfo");
    const auto missing=fixture.inspect();
    check(missing.affinity_observed && !missing.physical_cores() && !missing.ram_total_bytes && missing.cgroups.empty(),"missing facts fabricated");
    rejects([&]{(void)admitted_cpu_workers(missing,{4,0,10,CpuWorkerBasis::ObservedPhysicalCoreCount});},"unknown physical cores admitted");
    const auto absent=probe_linux_hardware({[](const auto&)->std::optional<std::string>{return {};},{},{},{}});
    check(!absent.affinity_observed && absent.allowed_cpus.empty(),"missing affinity became hardware_concurrency");
    rejects([&]{CpuResourceCoordinator coordinator(absent);},"unknown allowed CPUs admitted");
    fixture.allowed={0,0};rejects([&]{(void)fixture.inspect();},"duplicate affinity ids accepted");
    fixture.allowed={0,1};fixture.files["/proc/cpuinfo"]="processor: 0\n\nprocessor: 0\n";
    const auto duplicate=fixture.inspect();check(!duplicate.physical_cores(),"duplicate proc CPU records accepted");
    HardwareProbeLimits small;small.max_file_bytes=8;
    rejects([&]{(void)fixture.inspect(small);},"oversized hardware probe file admitted");
    fixture.files["/sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_list"]="0-18446744073709551615";
    const auto malformed=fixture.inspect();
    check(std::any_of(malformed.observations.begin(),malformed.observations.end(),[](const auto& note){return note.status==ObservationStatus::Malformed;}),"bad cache sharing lacks diagnostic");
    fixture.files["/proc/meminfo"]="MemTotal: 7 MB\nMemAvailable: -1 kB\n";
    check(!fixture.inspect().ram_total_bytes && !fixture.inspect().ram_available_bytes,"malformed RAM fields accepted");
    rejects([]{(void)rfc::runtime::hardware_detail::cpu_list("0,",4);},"trailing CPU list delimiter accepted");
}
void budgets() {
    Fixture f;f.files.erase("/proc/self/cgroup");const auto h=f.inspect();CpuResourceCoordinator coordinator(h,{4,100,100});
    check(coordinator.capacity()==4 && coordinator.available_workers()==4,"initial resource capacity wrong");
    {
        auto first=coordinator.reserve(2,60);auto second=coordinator.reserve(2,40);
        check(coordinator.available_workers()==0 && coordinator.available_declared_buffer_bytes()==0,"overlapping reservations unaccounted");
        rejects([&]{auto extra=coordinator.reserve(1);},"CPU budget overbooked");
        check(first.workers()==2 && second.declared_buffer_bytes()==40,"reservation metadata wrong");
        auto moved=std::move(first);rejects([&]{(void)first.workers();},"moved lease accepted");
        check(moved.workers()==2,"move released reservation early");
    }
    check(coordinator.available_workers()==4 && coordinator.available_declared_buffer_bytes()==100,"lease did not release");
    rejects([&]{auto lease=coordinator.reserve(5);},"CPU capacity exceeded");
    rejects([&]{auto lease=coordinator.reserve(1,101);},"declared byte cap exceeded");
    rejects([&]{auto lease=coordinator.reserve(0);},"zero-caller reservation accepted");
    try {auto lease=coordinator.reserve(3,80);throw std::bad_alloc{};}catch(const std::bad_alloc&){}
    check(coordinator.available_workers()==4 && coordinator.available_declared_buffer_bytes()==100,"exception leaked resources");
    {
        auto first=coordinator.reserve(1,30);auto second=coordinator.reserve(2,40);first=std::move(second);
        check(coordinator.available_workers()==2 && coordinator.available_declared_buffer_bytes()==60,"move assignment leaked old reservation");
    }
    auto changed=h;changed.ram_available_bytes=1;coordinator.readmit(changed);
    changed.allowed_cpus.pop_back();rejects([&]{coordinator.readmit(changed);},"changed affinity accepted");
    changed=h;changed.allowed_cpus[0].instruction_flags.push_back("new-feature");rejects([&]{coordinator.readmit(changed);},"changed instruction contract accepted");
    changed=h;changed.cgroups.push_back({"/new",CpuTimeQuota{1,1},false,false,{},{}});rejects([&]{coordinator.readmit(changed);},"changed CPU quota accepted");
    changed=h;changed.cgroups.push_back({"/invalid",CpuTimeQuota{0,1},false,false,{},{}});rejects([&]{CpuResourceCoordinator c(changed);},"zero quota accepted");
    std::atomic_size_t inside{},peak{};std::barrier together(4);std::vector<std::jthread> threads;
    for(std::size_t i=0;i<4;++i)threads.emplace_back([&]{
        for(std::size_t n=0;n<50;++n) {auto lease=coordinator.reserve(1,10);const auto count=inside.fetch_add(1)+1;
            auto old=peak.load();while(old<count && !peak.compare_exchange_weak(old,count)){}
            together.arrive_and_wait();inside.fetch_sub(1);together.arrive_and_wait();}
    });
    threads.clear();check(peak.load()==4 && coordinator.available_workers()==4,"cooperating concurrent lease accounting raced");
    std::optional<CpuResourceCoordinator::Reservation> retained;
    {CpuResourceCoordinator temporary(h,{4,100,100});retained.emplace(temporary.reserve(2,10));}
    check(retained->workers()==2,"reservation did not own coordinator state");retained.reset();
    const auto real=detect_hardware();
    if(real.platform=="Linux")check(real.affinity_observed || !real.observations.empty(),"live probe lacks observed result or reason");
}
}
int main() {
    try {observations();budgets();std::cout<<"checks "<<checks<<'\n';return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
