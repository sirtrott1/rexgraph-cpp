#pragma once

#include "rfc/backend/vulkan_binary.hpp"
#include <algorithm>
#include <chrono>

namespace rfc::backend {
struct MixedBinaryCandidate {
    std::size_t gpu_words{},cpu_workers{1},cpu_tile_words{64};
    bool operator==(const MixedBinaryCandidate&) const=default;
};
struct MixedBinarySample {
    MixedBinaryCandidate candidate;
    std::vector<std::uint64_t> complete_call_ns;
    std::uint64_t median_ns{};
};
// Bounded warm measurements for this immutable owner, device and CPU facts only.
// Every measured candidate is checked against exact CPU output before admission.
class MixedBinaryCalibration {
public:
    MixedBinaryCalibration(const PreparedPackedBinary& owner,const VulkanBinaryDevice& device,
                           runtime::CpuResourceCoordinator& resources,std::vector<MixedBinaryCandidate> candidates,
                           std::size_t repeats=3,std::size_t result_byte_limit=std::size_t{1}<<30)
        :owner_(owner),workspace_(device,owner),facts_(resources.facts()),result_limit_(result_byte_limit) {
        if(candidates.empty() || candidates.size()>32 || !repeats || repeats>9)
            throw std::invalid_argument("mixed calibration candidate/sample bounds");
        auto reference=owner.empty_result(result_limit_);owner.cpu_words(0,owner.words(),reference);
        for(const auto& candidate:candidates) {
            auto policy=make_policy(candidate);
            // Warm pipeline/driver caches. Packing, submit, join and readback remain in samples.
            verify(workspace_.execute(owner_,resources,policy).outputs,reference);
            MixedBinarySample sample{candidate,{},{}};
            for(std::size_t i=0;i<repeats;++i) {
                const auto start=std::chrono::steady_clock::now();
                auto result=workspace_.execute(owner_,resources,policy);
                const auto end=std::chrono::steady_clock::now();
                sample.complete_call_ns.push_back(static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()));
                verify(result.outputs,reference);
            }
            auto ordered=sample.complete_call_ns;std::sort(ordered.begin(),ordered.end());
            sample.median_ns=ordered[ordered.size()/2];samples_.push_back(std::move(sample));
        }
    }
    const std::vector<MixedBinarySample>& samples() const & noexcept {return samples_;}
    const std::vector<MixedBinarySample>& samples() const &&=delete;
    MixedBinaryResult execute(runtime::CpuResourceCoordinator& resources,std::stop_token stop={}) {
        if(resources.facts()!=facts_)throw std::invalid_argument("mixed calibration CPU facts changed");
        const MixedBinarySample* best=nullptr;
        const auto bytes=machine::checked_size_mul(machine::checked_size_mul(owner_.words(),owner_.outputs().size()),sizeof(std::uint64_t));
        for(const auto& sample:samples_) {
            const auto& c=sample.candidate;const auto cpu=owner_.words()-c.gpu_words;
            const auto workers=cpu?c.cpu_workers:1;
            if(workers>resources.available_workers() || bytes>resources.available_declared_buffer_bytes() ||
               machine::ceil_div(cpu,c.cpu_tile_words)>resources.policy().tile_limit)continue;
            if(!best || sample.median_ns<best->median_ns)best=&sample;
        }
        if(!best)throw runtime::CpuResourceBusy{};
        auto policy=make_policy(best->candidate);policy.stop=stop;
        return workspace_.execute(owner_,resources,policy);
    }
private:
    static void verify(const std::vector<machine::PackedBits>& output,const std::vector<machine::PackedBits>& reference) {
        if(output.size()!=reference.size())throw std::runtime_error("mixed calibration output shape mismatch");
        for(std::size_t i=0;i<output.size();++i)if(output[i].size()!=reference[i].size() || output[i].words()!=reference[i].words())
            throw std::runtime_error("mixed calibration exact reference mismatch");
    }
    MixedBinaryPolicy make_policy(const MixedBinaryCandidate& c) const {
        return {c.gpu_words,c.cpu_workers,c.cpu_tile_words,result_limit_,{}, {}};
    }
    PreparedPackedBinary owner_;
    VulkanBinaryWorkspace workspace_;
    runtime::CpuExecutionFacts facts_;
    std::size_t result_limit_;
    std::vector<MixedBinarySample> samples_;
};
} // namespace rfc::backend
