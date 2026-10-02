#pragma once

#include "rfc/machine/checked_size.hpp"
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#if defined(__linux__)
#include <cerrno>
#include <sched.h>
#endif

namespace rfc::runtime {
enum class ObservationStatus { Observed, Unavailable, Malformed };
struct HardwareObservation {
    std::string source;
    ObservationStatus status;
    std::string detail;
};
struct CpuThreadInfo {
    std::size_t logical_id{};
    std::optional<std::size_t> package_id, core_id;
    std::vector<std::string> instruction_flags;
    bool operator==(const CpuThreadInfo&) const = default;
};
struct CpuCacheInfo {
    std::size_t level{}, bytes{}, line_bytes{};
    std::string type;
    std::vector<std::size_t> shared_logical_ids;
    bool operator==(const CpuCacheInfo&) const = default;
};
struct CpuTimeQuota {
    std::size_t microseconds{}, period_microseconds{};
    bool operator==(const CpuTimeQuota&) const = default;
};
struct CgroupResourceInfo {
    std::string path;
    std::optional<CpuTimeQuota> cpu_quota;
    bool cpu_unlimited{}, memory_unlimited{};
    std::optional<std::size_t> memory_max, memory_current;
};
struct PciHardwareInfo {
    std::string address;
    std::size_t vendor{}, device{};
    std::optional<std::size_t> class_code;
};
// PCI display adapters are observations, not a count of usable compute devices.
struct HardwareInventory {
    std::string platform, cpu_model;
    bool affinity_observed{};
    std::vector<CpuThreadInfo> allowed_cpus;
    std::vector<CpuCacheInfo> caches;
    std::optional<std::size_t> ram_total_bytes, ram_available_bytes;
    std::vector<CgroupResourceInfo> cgroups;
    std::vector<PciHardwareInfo> pci_devices;
    std::vector<std::string> visible_compute_nodes;
    std::vector<HardwareObservation> observations;
    // Neither cache capacity nor a model-name match establishes stacked cache.
    std::optional<bool> stacked_3d_cache;
    std::optional<std::size_t> physical_cores() const {
        if (!affinity_observed || allowed_cpus.empty()) return {};
        std::set<std::pair<std::size_t,std::size_t>> cores;
        for (const auto& cpu : allowed_cpus) {
            if (!cpu.package_id || !cpu.core_id) return {};
            cores.emplace(*cpu.package_id,*cpu.core_id);
        }
        return cores.size();
    }
    std::optional<std::size_t> advisory_memory_headroom() const {
        auto bytes=ram_available_bytes;
        for (const auto& group : cgroups) if (group.memory_max && group.memory_current) {
            const auto room=*group.memory_current>=*group.memory_max?0:*group.memory_max-*group.memory_current;
            bytes=bytes?std::min(*bytes,room):room;
        }
        return bytes;
    }
};
struct HardwareProbeLimits {
    std::size_t max_cpus{65536}, max_devices{4096}, max_file_bytes{4u*1024u*1024u}, max_cache_indices{16};
};
struct LinuxHardwareProbe {
    std::function<std::optional<std::string>(const std::string&)> read_file;
    std::optional<std::vector<std::size_t>> allowed_cpu_ids;
    std::vector<std::string> visible_compute_nodes;
    HardwareProbeLimits limits;
};
namespace hardware_detail {
inline std::string trim(std::string text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    if (first==std::string::npos) return {};
    return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
}
inline std::optional<std::size_t> number(const std::string& text,int base=10) {
    const auto s=trim(text); std::size_t value{};
    const auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),value,base);
    if (error!=std::errc{} || end!=s.data()+s.size() || s.empty()) return {};
    return value;
}
inline std::vector<std::size_t> cpu_list(const std::string& text,std::size_t limit) {
    if(trim(text).empty() || trim(text).back()==',')throw std::invalid_argument("empty CPU list item");
    std::vector<std::size_t> result;
    std::istringstream input(trim(text)); std::string part;
    while (std::getline(input,part,',')) {
        const auto dash=part.find('-');
        const auto begin=number(part.substr(0,dash));
        const auto end=dash==std::string::npos?begin:number(part.substr(dash+1));
        if (!begin || !end || *end<*begin || *end-*begin>=limit || result.size()>limit-(*end-*begin+1))
            throw std::invalid_argument("malformed or oversized CPU list");
        for (auto id=*begin;;++id) { result.push_back(id); if(id==*end)break; }
    }
    std::sort(result.begin(),result.end());
    if (result.empty() || std::adjacent_find(result.begin(),result.end())!=result.end())
        throw std::invalid_argument("empty or duplicate CPU list");
    return result;
}
inline std::optional<std::size_t> cache_bytes(std::string text) {
    text=trim(text); std::size_t multiplier=1;
    if (!text.empty() && (text.back()=='K' || text.back()=='M')) {
        multiplier=text.back()=='K'?1024:1024*1024; text.pop_back();
    }
    const auto value=number(text);
    if (!value) return {};
    return machine::checked_size_mul(*value,multiplier);
}
inline std::string mount_path(std::string path) {
    for (const auto& [escaped,plain] : std::map<std::string,std::string>{{"\\040"," "},{"\\011","\t"},{"\\012","\n"},{"\\134","\\"}}) {
        std::size_t position{};
        while ((position=path.find(escaped,position))!=std::string::npos) { path.replace(position,4,plain); position+=plain.size(); }
    }
    return path;
}
} // namespace hardware_detail

// Injectable observations are useful in tests and restricted deployment hosts.
inline HardwareInventory probe_linux_hardware(const LinuxHardwareProbe& probe) {
    using namespace hardware_detail;
    if (!probe.read_file || !probe.limits.max_cpus || !probe.limits.max_devices || !probe.limits.max_file_bytes)
        throw std::invalid_argument("hardware probe needs bounded inputs");
    HardwareInventory result; result.platform="Linux"; result.visible_compute_nodes=probe.visible_compute_nodes;
    const auto read=[&](const std::string& path)->std::optional<std::string> {
        auto text=probe.read_file(path);
        if (text && text->size()>probe.limits.max_file_bytes) throw std::length_error("hardware probe file exceeds limit: "+path);
        return text;
    };
    const auto note=[&](std::string path,ObservationStatus status,std::string detail) {
        result.observations.push_back({std::move(path),status,std::move(detail)});
    };
    std::map<std::size_t,CpuThreadInfo> cpus;
    if (const auto text=read("/proc/cpuinfo")) {
        std::istringstream lines(*text); std::string line; std::map<std::string,std::string> fields;
        bool malformed{};
        const auto flush=[&] {
            if(fields.empty())return;
            const auto id=number(fields["processor"]);
            if(!id || cpus.size()>=probe.limits.max_cpus || cpus.contains(*id)) { malformed=true; fields.clear(); return; }
            CpuThreadInfo cpu{*id,number(fields["physical id"]),number(fields["core id"]),{}};
            std::istringstream flags(fields["flags"]); std::string flag;
            while(flags>>flag)cpu.instruction_flags.push_back(flag);
            std::sort(cpu.instruction_flags.begin(),cpu.instruction_flags.end());
            cpu.instruction_flags.erase(std::unique(cpu.instruction_flags.begin(),cpu.instruction_flags.end()),cpu.instruction_flags.end());
            if(result.cpu_model.empty())result.cpu_model=fields["model name"];
            cpus.emplace(*id,std::move(cpu)); fields.clear();
        };
        while(std::getline(lines,line)) {
            if(trim(line).empty()) { flush(); continue; }
            const auto colon=line.find(':'); if(colon!=std::string::npos)fields[trim(line.substr(0,colon))]=trim(line.substr(colon+1));
        }
        flush(); if(malformed)cpus.clear();
        note("/proc/cpuinfo",malformed?ObservationStatus::Malformed:ObservationStatus::Observed,"kernel-reported CPU identities and instruction flags");
    } else note("/proc/cpuinfo",ObservationStatus::Unavailable,"CPU model/topology/flags unknown");
    if(probe.allowed_cpu_ids) {
        auto ids=*probe.allowed_cpu_ids; std::sort(ids.begin(),ids.end());
        if(ids.empty() || ids.size()>probe.limits.max_cpus || std::adjacent_find(ids.begin(),ids.end())!=ids.end())
            throw std::invalid_argument("invalid affinity CPU ids");
        result.affinity_observed=true;
        for(const auto id:ids) {
            auto cpu=cpus.contains(id)?cpus.at(id):CpuThreadInfo{id,{},{},{}};
            const auto root="/sys/devices/system/cpu/cpu"+std::to_string(id);
            const auto package=read(root+"/topology/physical_package_id"),core=read(root+"/topology/core_id");
            if(package && core) { cpu.package_id=number(*package); cpu.core_id=number(*core); }
            result.allowed_cpus.push_back(std::move(cpu));
            for(std::size_t index=0;index<probe.limits.max_cache_indices;++index) {
                const auto cache=root+"/cache/index"+std::to_string(index)+"/";
                const auto level=read(cache+"level"); if(!level)continue;
                const auto type=read(cache+"type"),size=read(cache+"size"),shared=read(cache+"shared_cpu_list"),line_size=read(cache+"coherency_line_size");
                if(!type || !size || !shared || !line_size) { note(cache,ObservationStatus::Unavailable,"incomplete cache descriptor"); continue; }
                const auto nlevel=number(*level),bytes=cache_bytes(*size),linebytes=number(*line_size);
                if(!nlevel || !bytes || !linebytes || !*nlevel || !*bytes || !*linebytes) { note(cache,ObservationStatus::Malformed,"invalid cache descriptor"); continue; }
                try {
                    CpuCacheInfo entry{*nlevel,*bytes,*linebytes,trim(*type),cpu_list(*shared,probe.limits.max_cpus)};
                    if(std::find(entry.shared_logical_ids.begin(),entry.shared_logical_ids.end(),id)==entry.shared_logical_ids.end())
                        throw std::invalid_argument("cache sharing omits owning CPU");
                    if(std::find(result.caches.begin(),result.caches.end(),entry)==result.caches.end())result.caches.push_back(std::move(entry));
                } catch(const std::invalid_argument&) { note(cache,ObservationStatus::Malformed,"invalid cache sharing list"); }
            }
        }
        note("sched_getaffinity",ObservationStatus::Observed,"calling-thread allowed logical CPUs; no thread pinning performed");
    } else note("sched_getaffinity",ObservationStatus::Unavailable,"usable logical CPU count unknown");
    note("CPU cache topology",result.caches.empty()?ObservationStatus::Unavailable:ObservationStatus::Observed,
        "shared descriptors; stacked 3D cache is unknown");
    if(const auto text=read("/proc/meminfo")) {
        std::istringstream lines(*text); std::string line;
        while(std::getline(lines,line)) {
            std::istringstream values(line); std::string key,count,unit,extra; values>>key>>count>>unit;
            if(key!="MemTotal:" && key!="MemAvailable:")continue;
            const auto n=number(count);
            if(!n || unit!="kB" || (values>>extra)) { note("/proc/meminfo",ObservationStatus::Malformed,"RAM field needs an exact KiB count"); continue; }
            const auto bytes=machine::checked_size_mul(*n,1024);
            (key=="MemTotal:"?result.ram_total_bytes:result.ram_available_bytes)=bytes;
        }
    }
    note("/proc/meminfo",result.ram_total_bytes?ObservationStatus::Observed:ObservationStatus::Unavailable,
        "RAM availability is advisory, not an allocation guarantee");
    // Resolve the visible cgroup-v2 mount, including non-root mounts. Walk all
    // visible ancestors; hidden ancestors and cgroup-v1 remain unknown.
    const auto membership=read("/proc/self/cgroup"),mounts=read("/proc/self/mountinfo");
    std::string member;
    if(membership) { std::istringstream lines(*membership); std::string line; while(std::getline(lines,line))if(line.starts_with("0::/"))member=line.substr(3); }
    bool found_mount{};
    if(!member.empty() && mounts && member.find("..") == std::string::npos) {
        std::istringstream lines(*mounts); std::string line;
        while(std::getline(lines,line)) {
            const auto split=line.find(" - cgroup2 "); if(split==std::string::npos)continue;
            std::istringstream fields(line.substr(0,split)); std::string a,b,c,mount_root,mount_point;
            fields>>a>>b>>c>>mount_root>>mount_point; mount_root=mount_path(mount_root); mount_point=mount_path(mount_point);
            if(mount_root.empty() || mount_point.empty() || (member!=mount_root && !member.starts_with(mount_root=="/"?"/":mount_root+"/")))continue;
            auto path=mount_point+(member==mount_root?"":member.substr(mount_root=="/"?0:mount_root.size()));
            if(path.size()>1 && path.back()=='/')path.pop_back();
            for(std::size_t depth=0;depth<128;++depth) {
                CgroupResourceInfo group; group.path=path;
                if(const auto maximum=read(path+"/cpu.max")) {
                    std::istringstream values(*maximum); std::string quota,period,extra; values>>quota>>period;
                    const auto p=number(period),q=number(quota);
                    if(!p || !*p || (values>>extra) || (quota!="max" && (!q || !*q)))note(path+"/cpu.max",ObservationStatus::Malformed,"invalid CPU time quota");
                    else if(quota=="max")group.cpu_unlimited=true;
                    else group.cpu_quota=CpuTimeQuota{*q,*p};
                }
                if(const auto maximum=read(path+"/memory.max")) {
                    if(trim(*maximum)=="max")group.memory_unlimited=true;
                    else { group.memory_max=number(*maximum); if(!group.memory_max)note(path+"/memory.max",ObservationStatus::Malformed,"invalid memory ceiling"); }
                }
                if(const auto current=read(path+"/memory.current"))group.memory_current=number(*current);
                result.cgroups.push_back(std::move(group)); found_mount=true;
                if(path==mount_point)break;
                const auto parent=std::filesystem::path(path).parent_path().string();
                if(parent.size()<mount_point.size() || parent==path)break;
                path=parent;
            }
            break;
        }
    }
    note("cgroup-v2",found_mount?ObservationStatus::Observed:ObservationStatus::Unavailable,
        "only readable visible ancestor limits; CPU quota is time, not core identity; hidden/v1 limits unknown");
    if(const auto text=read("/proc/bus/pci/devices")) {
        std::istringstream lines(*text); std::string line;
        while(std::getline(lines,line)) {
            if(result.pci_devices.size()>=probe.limits.max_devices)throw std::length_error("PCI device count exceeds probe limit");
            std::istringstream fields(line); std::string address,identity; fields>>address>>identity;
            const auto id=number(address,16),vendor=identity.size()==8?number(identity.substr(0,4),16):std::nullopt,
                device=identity.size()==8?number(identity.substr(4),16):std::nullopt;
            if(!id || *id>65535 || !vendor || !device) { note("/proc/bus/pci/devices",ObservationStatus::Malformed,"invalid PCI identity"); continue; }
            std::ostringstream bdf,config;
            bdf<<"0000:"<<std::hex; config<<"/proc/bus/pci/"<<std::hex;
            const auto hex2=[&](std::ostream& out,std::size_t n) { if(n<16)out<<'0'; out<<n; };
            hex2(bdf,*id>>8); bdf<<':'; hex2(bdf,(*id&255)>>3); bdf<<'.'<<(*id&7);
            hex2(config,*id>>8); config<<'/'; hex2(config,(*id&255)>>3); config<<'.'<<(*id&7);
            PciHardwareInfo hardware{bdf.str(),*vendor,*device,{}};
            if(const auto code=read("/sys/bus/pci/devices/"+hardware.address+"/class")) {
                auto digits=trim(*code); if(digits.starts_with("0x"))digits.erase(0,2); hardware.class_code=number(digits,16);
            } else if(const auto bytes=read(config.str()); bytes && bytes->size()>=12) {
                const auto byte=[&](std::size_t at) { return static_cast<std::size_t>(static_cast<unsigned char>((*bytes)[at])); };
                hardware.class_code=byte(9)|(byte(10)<<8)|(byte(11)<<16);
            }
            result.pci_devices.push_back(std::move(hardware));
        }
    }
    note("device compute runtimes",ObservationStatus::Unavailable,"HIP/CUDA/Vulkan compute enumeration and exact kernels are not registered; GPU count/VRAM/UMA coherence unknown");
    return result;
}

inline HardwareInventory detect_hardware(const HardwareProbeLimits& limits={}) {
#if defined(__linux__)
    LinuxHardwareProbe probe; probe.limits=limits;
    probe.read_file=[limits](const std::string& path)->std::optional<std::string> {
        std::ifstream file(path,std::ios::binary); if(!file)return {};
        std::string text; char block[4096];
        while(file.read(block,sizeof(block)) || file.gcount()) {
            const auto count=static_cast<std::size_t>(file.gcount());
            if(text.size()>limits.max_file_bytes || count>limits.max_file_bytes-text.size())throw std::length_error("hardware file exceeds limit");
            text.append(block,count);
        }
        if(file.bad())return {};
        return text;
    };
    // A growing mask supports hosts whose CPU identifiers exceed CPU_SETSIZE.
    for(std::size_t count=std::min<std::size_t>(128,limits.max_cpus);count && count<=limits.max_cpus;) {
        auto* mask=CPU_ALLOC(count); if(!mask)throw std::bad_alloc{};
        const auto bytes=CPU_ALLOC_SIZE(count); CPU_ZERO_S(bytes,mask);
        const auto status=sched_getaffinity(0,bytes,mask); const auto error=errno;
        if(status==0) {
            std::vector<std::size_t> ids;
            for(std::size_t id=0;id<count;++id)if(CPU_ISSET_S(id,bytes,mask))ids.push_back(id);
            probe.allowed_cpu_ids=std::move(ids); CPU_FREE(mask); break;
        }
        CPU_FREE(mask); if(error!=EINVAL || count>limits.max_cpus/2)break; count*=2;
    }
    std::error_code error;
    if(std::filesystem::exists("/dev/kfd",error))probe.visible_compute_nodes.push_back("/dev/kfd");
    error.clear();
    const std::filesystem::directory_iterator end;
    for(std::filesystem::directory_iterator it("/dev/dri",error); !error && it!=end; it.increment(error))
        if(it->path().filename().string().starts_with("renderD"))probe.visible_compute_nodes.push_back(it->path().string());
    return probe_linux_hardware(probe);
#else
    (void)limits; HardwareInventory result; result.platform="unsupported";
    result.observations.push_back({"hardware inventory",ObservationStatus::Unavailable,"no native inventory adapter compiled for this platform"});
    return result;
#endif
}
} // namespace rfc::runtime
