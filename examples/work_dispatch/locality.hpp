#pragma once
#include "rfc/database/locality_native_field.hpp"
#include "rfc/database/staged_field_work.hpp"
#include <iostream>
#include <limits>

namespace locality_sample {
using namespace rfc;
using Clock=std::chrono::steady_clock;
inline std::uint64_t elapsed(Clock::time_point start) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());
}
inline numeric::RationalVector expected(const database::NativeFieldAnalysis& analysis,std::size_t count) {
    numeric::RationalVector values(analysis.binding().target_dimension());
    std::map<std::uint64_t,std::size_t> offsets;
    for(const auto& b:analysis.binding().outputs())offsets.emplace(b.cell.key,b.offset);
    // Independent sample definition: eight signed endpoints, eight coordinates.
    for(std::size_t i=0;i<count;++i)for(std::size_t j=0;j<8;++j)for(std::size_t d=0;d<8;++d) {
        const numeric::Rational q{static_cast<long>((i+d)%7+1),11};
        auto& value=values[offsets.at(10+(i*7+j)%256)+d];if(j%2)value-=q;else value+=q;
    }
    return values;
}
inline void samples(const std::vector<std::uint64_t>& values) {
    std::cout<<",\"samples_ns\":[";
    for(std::size_t i=0;i<values.size();++i)std::cout<<(i?",":"")<<values[i];
    std::cout<<"]}\n";
}
inline database::PreparedParallelNativeField field_profiles(const database::NativeFieldAnalysis& analysis,
    const runtime::HardwareInventory& hardware,std::size_t capacity,std::size_t count,std::uint64_t source_ns,bool profile) {
    const auto reference=expected(analysis,count);
    std::optional<database::PreparedParallelNativeField> selected;
    std::uint64_t best=std::numeric_limits<std::uint64_t>::max();std::size_t selected_workers{};std::string selected_layout,selected_partition;
    for(const bool fibers:{false,true}) {
        auto start=Clock::now();const auto base=fibers?database::prepare_fiber_field(analysis):database::prepare_parallel_field(analysis);
        const auto prepare_ns=elapsed(start);
        for(const bool incidence:{false,true})for(const auto target:{32u,128u,512u}) {
            start=Clock::now();runtime::LocalityTilePolicy policy;
            policy.max_rows=target;policy.scalar_work=target*16u;policy.object_bytes=target*512u;
            if(!incidence){policy.scalar_work=std::numeric_limits<std::size_t>::max();policy.object_bytes=std::numeric_limits<std::size_t>::max();policy.cache_level=0;}
            const auto operation=database::retile_field(base,hardware,policy);
            const auto partition_prepare_ns=elapsed(start);
            std::set<std::size_t> workers{1,std::min(std::size_t{2},capacity),capacity};
            for(const auto worker_count:workers) {
                runtime::CpuTileExecutor executor({worker_count,capacity,1000000});start=Clock::now();auto workspace=database::make_parallel_workspace(operation);
                const auto scratch_prepare_ns=elapsed(start);std::vector<std::uint64_t> times;
                for(unsigned repetition=0;repetition<(profile?5u:3u);++repetition) {
                    start=Clock::now();const auto result=database::execute_parallel(operation,workspace,executor);times.push_back(elapsed(start));
                    if(result.values()!=reference)throw std::runtime_error("locality field differs from independent sample definition");
                }
                auto sorted=times;std::sort(sorted.begin(),sorted.end());const auto median=sorted[sorted.size()/2];
                if(median<best){best=median;selected=operation;selected_workers=worker_count;selected_layout=fibers?"fibers":"target_rows";selected_partition=incidence?"incidence":"uniform";}
                const auto& facts=operation.ranges().action().statistics();
                std::cout<<"{\"phase\":\"field_locality\",\"layout\":\""<<(fibers?"fibers":"target_rows")<<"\",\"partition\":\""<<(incidence?"incidence":"uniform")
                    <<"\",\"relations\":"<<count<<",\"workers\":"<<worker_count<<",\"row_target\":"<<target
                    <<",\"tiles\":"<<operation.ranges().tiles().size()<<",\"source_prepare_ns\":"<<source_ns
                    <<",\"kernel_prepare_ns\":"<<prepare_ns<<",\"partition_prepare_ns\":"<<partition_prepare_ns<<",\"scratch_prepare_ns\":"<<scratch_prepare_ns
                    <<",\"median_ns\":"<<median<<",\"logical_entries\":"<<facts.entries<<",\"initial_coefficient_copies\":"<<facts.coefficient_copies
                    <<",\"scalar_identity_blocks\":"<<facts.scalar_identity_blocks<<",\"cache_descriptors\":"<<hardware.caches.size();samples(times);
            }
        }
    }
    std::cout<<"{\"phase\":\"field_locality_selection\",\"relations\":"<<count<<",\"layout\":\""<<selected_layout<<"\",\"partition\":\""<<selected_partition
        <<"\",\"workers\":"<<selected_workers<<",\"warm_median_ns\":"<<best<<",\"scope\":\"this prepared source, complete output, retained CPU scratch\"}\n";
    return *selected;
}
} // namespace locality_sample
