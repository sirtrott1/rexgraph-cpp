#pragma once

#include "rfc/runtime/hardware_inventory.hpp"

namespace rfc::runtime {
struct LocalityRowDemand { std::size_t scalar_work{},object_bytes{}; };
struct LocalityTilePolicy {
    std::size_t max_rows{128},scalar_work{4096},object_bytes{64u*1024u};
    std::size_t cache_level{2},cache_fraction_denominator{4};
};
struct LocalityTilePlan {
    std::vector<std::pair<std::size_t,std::size_t>> rows;
    std::size_t working_set_target{},oversized_rows{};
    std::optional<CpuCacheInfo> cache;
};
// Working-set estimates guide traversal; they do not cap resident/GMP memory or
// establish cache residency. One indivisible output row may exceed the target.
inline LocalityTilePlan plan_locality_tiles(const std::vector<LocalityRowDemand>& demands,
    const HardwareInventory& hardware,const LocalityTilePolicy& policy={}) {
    if(!policy.max_rows || !policy.scalar_work || !policy.object_bytes || !policy.cache_fraction_denominator)
        throw std::invalid_argument("locality tile targets must be positive");
    LocalityTilePlan plan;plan.working_set_target=policy.object_bytes;
    for(const auto& cache:hardware.caches) {
        if(cache.level!=policy.cache_level || (cache.type!="Data" && cache.type!="Unified") || !cache.bytes)continue;
        const bool allowed=std::any_of(hardware.allowed_cpus.begin(),hardware.allowed_cpus.end(),[&](const auto& cpu){
            return std::find(cache.shared_logical_ids.begin(),cache.shared_logical_ids.end(),cpu.logical_id)!=cache.shared_logical_ids.end();
        });
        if(!allowed)continue;
        if(!plan.cache || cache.bytes<plan.cache->bytes)plan.cache=cache;
    }
    if(plan.cache)plan.working_set_target=std::min(plan.working_set_target,
        std::max(std::size_t{1},plan.cache->bytes/policy.cache_fraction_denominator));
    std::size_t begin{},work{},bytes{};
    for(std::size_t row=0;row<demands.size();++row) {
        const auto demand=demands[row];
        if(demand.scalar_work>policy.scalar_work || demand.object_bytes>plan.working_set_target)++plan.oversized_rows;
        if(row>begin && (row-begin>=policy.max_rows || demand.scalar_work>policy.scalar_work-std::min(work,policy.scalar_work) ||
            demand.object_bytes>plan.working_set_target-std::min(bytes,plan.working_set_target))) {
            plan.rows.emplace_back(begin,row);begin=row;work=bytes=0;
        }
        work=machine::checked_size_add(work,demand.scalar_work);bytes=machine::checked_size_add(bytes,demand.object_bytes);
    }
    if(begin<demands.size())plan.rows.emplace_back(begin,demands.size());
    return plan;
}
} // namespace rfc::runtime
