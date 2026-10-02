#pragma once

#include "rfc/database/parallel_native_field.hpp"
#include "rfc/local/prepared_field_ranges.hpp"
#include "rfc/runtime/cpu_tiles.hpp"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>

namespace rfc::database {
struct NativeFieldCacheLimits {
    std::size_t plans{32}, results{128}, descriptor_bytes{64u * 1024u * 1024u}, result_slots{16000000};
    std::size_t in_flight{32}, pending_descriptor_bytes{64u * 1024u * 1024u};
    std::size_t idle_workspaces{8}, idle_workspace_slots{1000000};
    std::size_t retained_result_bytes{512u*1024u*1024u},idle_workspace_bytes{256u*1024u*1024u};
};
struct NativeFieldCacheStats {
    std::size_t plan_hits{}, result_hits{}, in_flight_hits{}, compiled_plans{}, applications{}, failed_jobs{};
    std::size_t descriptor_comparisons{}, evicted_plans{}, evicted_results{};
    std::size_t plans{}, results{}, in_flight{}, charged_descriptor_bytes{}, result_slots{}, pending_descriptor_bytes{};
    std::size_t workspace_creations{}, workspace_hits{}, idle_workspaces{}, idle_workspace_slots{};
    std::size_t adopted_plans{}, prepared_input_uses{};
    std::size_t retained_result_bytes{},idle_workspace_bytes{};
};
struct NativeFieldReuse { bool plan{}, result{}, in_flight{}, prepared{}; };
enum class NativeFieldCacheStage { BeforeCompile, BeforeApply, BeforePublish, BeforeAdopt };
using NativeFieldCacheHook = std::function<void(NativeFieldCacheStage)>;
namespace native_field_cache_detail {
struct Values {
    numeric::RationalVector data; std::size_t bits{},object_bytes{};
    std::string implementation{"PreparedGradeFieldAction/GMP/serial-prepared-v1"};
    std::optional<runtime::CpuTileRunReport> execution;
};
inline NativeExecutionReport parallel_report(const NativeFieldAnalysis& analysis, std::size_t rows,
    std::optional<std::size_t> tile_count={}) {
    if (!rows) throw std::invalid_argument("cached field tile rows must be positive");
    auto report = analysis.report(); const auto targets = analysis.binding().target_dimension();
    const auto tiles = tile_count.value_or(machine::ceil_div(targets,rows));
    auto& req = report.requirements;
    req.records = machine::checked_size_add(req.records,machine::checked_size_mul(tiles,6));
    req.rational_slots = machine::checked_size_add(req.rational_slots,
        machine::checked_size_add(machine::checked_size_mul(targets,2),tiles));
    req.scalar_work = machine::checked_size_add(req.scalar_work,
        machine::checked_size_add(machine::checked_size_mul(tiles,6),machine::checked_size_mul(targets,2)));
    report.decisions.push_back({runtime::DecisionKind::Profile,"tile-rows","cached-cpu-field-v1","1",std::to_string(rows),
        "physical output ranges; original ordered occurrence contributions; complete carriers","each request"});
    return report;
}
struct Guard {
    const void* owner; Guard* previous;
    static inline thread_local Guard* active{};
    explicit Guard(const void* value) : owner(value),previous(active) {
        for (auto p = active; p; p = p->previous)
            if (p->owner == value) throw std::logic_error("native field cache callback reentry");
        active = this;
    }
    ~Guard() { active = previous; }
};
inline std::size_t charge(const NativeFieldAnalysis& a) {
    return machine::checked_size_add(a.receipt().plan_bytes().size(),a.receipt().result_bytes().size());
}
} // namespace native_field_cache_detail

class CachedNativeField {
public:
    const numeric::RationalVector& values() const & { valid(); return values_->data; }
    numeric::RationalVector values() const && { valid(); return values_->data; }
    std::shared_ptr<const numeric::RationalVector> values_pin() const { valid(); return {values_,&values_->data}; }
    const auto& carriers() const & { valid(); return analysis_.binding().outputs(); }
    std::vector<storage::NativeFieldCellBinding> carriers() const && { valid(); return analysis_.binding().outputs(); }
    const NativeExecutionReport& provenance() const & { valid(); return report_; }
    NativeExecutionReport provenance() const && { valid(); return report_; }
    const NativeFieldAnalysis& analysis() const & { valid(); return analysis_; }
    const NativeFieldAnalysis& analysis() const && = delete;
    NativeFieldReuse reuse() const { valid(); return reuse_; }
    // Producer execution facts remain separate from the requesting publication.
    const std::optional<runtime::CpuTileRunReport>& producer_execution() const & { valid(); return values_->execution; }
    const std::optional<runtime::CpuTileRunReport>& producer_execution() const && = delete;
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { valid(); return analysis_.source_publication(); }
    const numeric::Rational& state(storage::StableCellKey cell, std::size_t coordinate) const & {
        const auto& outputs = carriers();
        const auto it = std::find_if(outputs.begin(),outputs.end(),[&](const auto& b) { return b.cell == cell; });
        if (it == outputs.end() || coordinate >= it->dimension) throw std::out_of_range("cached native field coordinate");
        return values().at(it->offset + coordinate);
    }
    numeric::Rational state(storage::StableCellKey cell, std::size_t coordinate) const && {
        return std::as_const(*this).state(cell,coordinate);
    }
private:
    friend class NativeFieldCache;
    CachedNativeField(NativeFieldAnalysis analysis, std::shared_ptr<const native_field_cache_detail::Values> values,
                      const runtime::ExecutionLease& lease, NativeFieldReuse reuse, NativeExecutionReport report)
        : analysis_(std::move(analysis)),values_(std::move(values)),report_(std::move(report)),reuse_(reuse) {
        if (values_->bits > lease.policy().limits.integer_bits)
            throw runtime::ExecutionLimitExceeded("cached result exact integer bits",values_->bits,lease.policy().limits.integer_bits);
        report_.implementation = values_->implementation;
        report_.fallback = lease.policy().backend != runtime::ExecutionBackend::ExactCpu;
        report_.execution_policy = lease.policy();
        auto& kernel = report_.decisions.at(3);
        kernel.origin = values_->execution ? "cached-cpu-field-v1" : "compiled-field-v1"; kernel.value = report_.implementation;
        kernel.constraints = "exact scoped receipts; retained occurrence blocks; producer kernel; exclusive workspace";
        report_.decisions.push_back({runtime::DecisionKind::Derived,"cache-reuse","scoped-native-field-v1","1",
            reuse.result ? "completed-result" : (reuse.in_flight ? "in-flight-completion" :
                (reuse.plan ? "compiled-plan" : (reuse.prepared ? "adopted-prepared-plan" : "fresh-compiled-plan"))),
            "authentic directory authority; exact descriptors; current source/report and lease","every request"});
        lease.check_cancelled();
    }
    void valid() const { if (!values_) throw std::invalid_argument("moved-from cached native field"); }
    NativeFieldAnalysis analysis_;
    std::shared_ptr<const native_field_cache_detail::Values> values_;
    NativeExecutionReport report_;
    NativeFieldReuse reuse_;
};

// Bounded rebuildable caches for one named pure action. This service supplies
// synchronization and sharing, not a worker pool, writer certificate or disk I/O.
class NativeFieldCache {
    using Values = native_field_cache_detail::Values;
    struct Compiled {
        local::PreparedGradeFieldAction action;
        explicit Compiled(local::PreparedGradeFieldAction a) : action(std::move(a)) {}
    };
    struct PlanEntry { std::uint64_t hash, order; NativeFieldAnalysis analysis; std::shared_ptr<const Compiled> compiled; };
    struct Scratch {
        std::shared_ptr<const Compiled> compiled;
        local::PreparedGradeFieldRanges ranges;
        local::PreparedGradeFieldRanges::Workspace workspace;
        Scratch(std::shared_ptr<const Compiled> c, std::size_t rows)
            : compiled(std::move(c)),ranges(compiled->action,rows),workspace(ranges.make_workspace()) {}
        Scratch(std::shared_ptr<const Compiled> c,const local::PreparedGradeFieldRanges& partition)
            : compiled(std::move(c)),ranges(make_partition(compiled->action,partition)),workspace(ranges.make_workspace()) {}
        static local::PreparedGradeFieldRanges make_partition(const local::PreparedGradeFieldAction& action,
            const local::PreparedGradeFieldRanges& partition) {
            std::vector<std::pair<std::size_t,std::size_t>> rows;
            for(const auto& tile:partition.tiles())rows.emplace_back(tile.begin,tile.end);
            return local::PreparedGradeFieldRanges::partition(action,rows);
        }
    };
    struct ResultEntry { std::uint64_t hash, order; NativeFieldAnalysis analysis; std::shared_ptr<const Values> values; };
    struct Published {
        std::vector<PlanEntry> plans; std::vector<ResultEntry> results; std::uint64_t order{};
        std::size_t bytes{}, slots{},result_bytes{};
    };
    struct Job {
        std::uint64_t hash; NativeFieldAnalysis analysis;
        std::condition_variable ready; bool done{}; std::exception_ptr error;
        std::shared_ptr<const Values> values;
        Job(std::uint64_t h, NativeFieldAnalysis a) : hash(h),analysis(std::move(a)) {}
    };
public:
    using Selector = std::function<std::uint64_t(std::span<const std::byte>)>;
    explicit NativeFieldCache(NativeFieldCacheLimits limits = {}, Selector selector = {})
        : limits_(limits),selector_(std::move(selector)),published_(std::make_shared<const Published>()) {
        if (!limits_.plans || !limits_.results || !limits_.in_flight)
            throw std::invalid_argument("native field cache requires positive entry/job limits");
        idle_.reserve(limits_.idle_workspaces);
    }
    NativeFieldCache(const NativeFieldCache&) = delete;
    NativeFieldCache& operator=(const NativeFieldCache&) = delete;
    NativeFieldCache(NativeFieldCache&&) = delete;
    NativeFieldCache& operator=(NativeFieldCache&&) = delete;
    NativeFieldCacheStats stats() const {
        native_field_cache_detail::Guard guard(this); std::lock_guard lock(mutex_);
        auto out = stats_; out.plans = published_->plans.size(); out.results = published_->results.size();
        out.in_flight = jobs_.size(); out.charged_descriptor_bytes = published_->bytes;
        out.result_slots = published_->slots; out.pending_descriptor_bytes = pending_bytes_;
        out.idle_workspaces = idle_.size(); out.idle_workspace_slots = idle_slots_;
        out.retained_result_bytes=published_->result_bytes;out.idle_workspace_bytes=idle_bytes_;return out;
    }
    void clear() {
        native_field_cache_detail::Guard guard(this); auto empty = std::make_shared<const Published>();
        std::lock_guard lock(mutex_); published_ = std::move(empty); idle_.clear(); idle_slots_ = idle_bytes_ = 0;
    }
    CachedNativeField evaluate(const NativeFieldAnalysis& analysis, const runtime::ExecutionLease& lease = runtime::ExecutionLease{},
                               const NativeFieldCacheHook& hook = {}) {
        return evaluate_impl(analysis,lease,hook,nullptr,0,analysis.report());
    }
    CachedNativeField evaluate_parallel(const NativeFieldAnalysis& analysis, runtime::CpuTileExecutor& executor,
        std::size_t rows_per_tile = 128, const runtime::ExecutionLease& lease = runtime::ExecutionLease{},
        const NativeFieldCacheHook& hook = {}) {
        auto report = native_field_cache_detail::parallel_report(analysis,rows_per_tile);
        if (machine::ceil_div(analysis.binding().target_dimension(),rows_per_tile)>executor.policy().tile_limit)
            throw std::length_error("cached field CPU tile limit");
        return evaluate_impl(analysis,lease,hook,&executor,rows_per_tile,std::move(report));
    }
    // Only the sealed operation binds the adopted action/input to its analysis.
    CachedNativeField evaluate_parallel(const PreparedParallelNativeField& operation, runtime::CpuTileExecutor& executor,
        const runtime::ExecutionLease& lease = runtime::ExecutionLease{}, const NativeFieldCacheHook& hook = {}) {
        const auto rows = operation.ranges().rows_per_tile();
        auto report = native_field_cache_detail::parallel_report(operation.analysis(),rows,operation.ranges().tiles().size());
        if (operation.ranges().tiles().size()>executor.policy().tile_limit)
            throw std::length_error("cached prepared field CPU tile limit");
        return evaluate_impl(operation.analysis(),lease,hook,&executor,rows,std::move(report),&operation);
    }
private:
    CachedNativeField evaluate_impl(const NativeFieldAnalysis& analysis, const runtime::ExecutionLease& lease,
        const NativeFieldCacheHook& hook, runtime::CpuTileExecutor* executor, std::size_t rows, NativeExecutionReport report,
        const PreparedParallelNativeField* prepared = nullptr) {
        native_field_cache_detail::Guard guard(this);
        lease.check_cancelled(); runtime::admit_execution(report.requirements,lease.policy());
        std::shared_ptr<Job> job; std::shared_ptr<const Compiled> compiled;
        std::unique_ptr<Scratch> scratch;
        std::uint64_t plan_hash{}; bool reused_plan = false, adopted = false;
        {
            std::unique_lock lock(mutex_);
            const auto result_hash = selector_ ? selector_(analysis.receipt().result_bytes()) : analysis.receipt().result_fingerprint();
            for (const auto& entry : published_->results) if (entry.hash == result_hash) {
                ++stats_.descriptor_comparisons;
                if (entry.analysis.receipt().same_result(analysis.receipt())) {
                    auto values = entry.values; ++stats_.result_hits; lock.unlock();
                    return CachedNativeField(analysis,std::move(values),lease,{false,true,false},report);
                }
            }
            for (const auto& pending : jobs_) if (pending->hash == result_hash) {
                ++stats_.descriptor_comparisons;
                if (pending->analysis.receipt().same_result(analysis.receipt())) { job = pending; break; }
            }
            if (job) {
                ++stats_.in_flight_hits;
                while (!job->done) {
                    job->ready.wait_for(lock,std::chrono::milliseconds(10));
                    lease.check_cancelled();
                }
                const auto error = job->error; const auto values = job->values; lock.unlock();
                if (error) std::rethrow_exception(error);
                return CachedNativeField(analysis,values,lease,{false,false,true},report);
            }
            const auto charge = native_field_cache_detail::charge(analysis);
            storage::native_accession_detail::budget(machine::checked_size_mul(charge,2),limits_.descriptor_bytes,"cache descriptor entry budget");
            storage::native_accession_detail::budget(analysis.binding().target_dimension(),limits_.result_slots,"cache result-slot budget");
            storage::native_accession_detail::budget(machine::checked_size_add(jobs_.size(),1),limits_.in_flight,"cache in-flight count budget");
            const auto pending = machine::checked_size_add(pending_bytes_,charge);
            storage::native_accession_detail::budget(pending,limits_.pending_descriptor_bytes,"cache pending descriptor budget");
            plan_hash = selector_ ? selector_(analysis.receipt().plan_bytes()) : analysis.receipt().plan_fingerprint();
            for (const auto& entry : published_->plans) if (entry.hash == plan_hash) {
                ++stats_.descriptor_comparisons;
                if (entry.analysis.receipt().same_plan(analysis.receipt())) {
                    compiled = entry.compiled; reused_plan = true; ++stats_.plan_hits; break;
                }
            }
            job = std::make_shared<Job>(result_hash,analysis); jobs_.push_back(job); pending_bytes_ = pending;
        }
        try {
            if (!compiled) {
                if (hook) hook(prepared ? NativeFieldCacheStage::BeforeAdopt : NativeFieldCacheStage::BeforeCompile);
                lease.check_cancelled();
                if (prepared) {
                    compiled = std::make_shared<const Compiled>(prepared->ranges().action()); adopted = true;
                    std::lock_guard lock(mutex_); ++stats_.adopted_plans;
                } else {
                    const auto& plan = analysis.binding();
                    auto fibers = storage::temporal_linear_fiber_system(*plan.endpoint(),plan.selected_carriers());
                    compiled = std::make_shared<const Compiled>(local::PreparedGradeFieldAction(fibers,plan.upper_grade()));
                    std::lock_guard lock(mutex_); ++stats_.compiled_plans;
                }
            }
            lease.check_cancelled();
            numeric::RationalVector materialized;
            std::shared_ptr<const numeric::RationalVector> pinned_input;
            if (prepared) {
                pinned_input = prepared->input_pin();
                std::lock_guard lock(mutex_); ++stats_.prepared_input_uses;
            } else {
                const auto& plan = analysis.binding(); materialized.resize(plan.source_dimension());
                const auto tensors = storage::temporal_endpoint_detail::tensors(*plan.endpoint());
                for (const auto& b : plan.inputs())
                    for (const auto& value : tensors.at(b.field)->tensor.values().entries()) materialized[b.offset+value.index] = value.value;
            }
            const auto& input = pinned_input ? *pinned_input : materialized;
            if (hook) hook(NativeFieldCacheStage::BeforeApply);
            lease.check_cancelled();
            { std::lock_guard lock(mutex_); ++stats_.applications; }
            auto values = std::make_shared<Values>();
            if (executor) {
                scratch = borrow(compiled,rows,prepared?&prepared->ranges():nullptr);
                const auto runner = [&](std::size_t tiles,const auto& task) {
                    values->execution = executor->run_tiles(tiles,[&](std::size_t tile,std::size_t,const auto& stop) {
                        lease.check_cancelled(); task(tile,stop);
                    },[&](std::size_t) { lease.check_cancelled(); });
                };
                values->data = scratch->ranges.apply(input,scratch->workspace,runner,lease.policy().limits.integer_bits,
                    [&] { lease.check_cancelled(); },[] {});
                values->implementation = compiled->action.statistics().factored?
                    "PreparedGradeFieldAction/GMP/fiber-tiles-v1":"PreparedGradeFieldAction/GMP/cpu-tiles-v1";
            } else {
                auto workspace = compiled->action.make_workspace();
                values->data = compiled->action.apply(input,workspace,lease.policy().limits.integer_bits);
                if(compiled->action.statistics().factored)values->implementation="PreparedGradeFieldAction/GMP/serial-fiber-v1";
            }
            for (const auto& q : values->data) values->bits = std::max(values->bits,storage::native_execution_detail::integer_bits(q));
            values->object_bytes=numeric::rational_vector_object_bytes(values->data);
            auto result = CachedNativeField(analysis,values,lease,{reused_plan,false,false,adopted},report);
            if (hook) hook(NativeFieldCacheStage::BeforePublish);
            lease.check_cancelled();
            {
                std::lock_guard lock(mutex_); auto next = std::make_shared<Published>(*published_);
                if (next->order > std::numeric_limits<std::uint64_t>::max() - 2)
                    throw std::length_error("native field cache eviction order exhausted");
                const auto same = std::find_if(next->plans.begin(),next->plans.end(),[&](const auto& entry) {
                    return entry.hash == plan_hash && entry.analysis.receipt().same_plan(analysis.receipt());
                });
                if (same == next->plans.end()) next->plans.push_back({plan_hash,++next->order,analysis,compiled});
                next->results.push_back({job->hash,++next->order,analysis,values});
                std::size_t evicted_plans{}, evicted_results{};
                trim(*next,evicted_plans,evicted_results);
                // No allocation or throwing operation follows this final check.
                lease.check_cancelled(); published_ = std::move(next);
                stats_.evicted_plans += evicted_plans; stats_.evicted_results += evicted_results;
                job->values = std::move(values); job->done = true; remove(job);
            }
            job->ready.notify_all(); recycle(std::move(scratch)); return result;
        } catch (...) {
            {
                std::lock_guard lock(mutex_); job->error = std::current_exception(); job->done = true;
                ++stats_.failed_jobs; remove(job);
            }
            job->ready.notify_all(); throw;
        }
    }
    std::unique_ptr<Scratch> borrow(const std::shared_ptr<const Compiled>& compiled, std::size_t rows,
        const local::PreparedGradeFieldRanges* partition=nullptr) {
        {
            std::lock_guard lock(mutex_);
            for (auto it = idle_.begin(); it != idle_.end(); ++it)
                if ((*it)->compiled == compiled && (*it)->ranges.rows_per_tile() == rows &&
                    (partition ? (*it)->ranges.tiles()==partition->tiles() : uniform((*it)->ranges))) {
                    auto scratch = std::move(*it); idle_.erase(it);
                    idle_slots_ -= scratch->workspace.scratch_slots();idle_bytes_-=scratch->workspace.scratch_object_bytes(); ++stats_.workspace_hits; return scratch;
                }
        }
        auto scratch = partition?std::make_unique<Scratch>(compiled,*partition):std::make_unique<Scratch>(compiled,rows);
        { std::lock_guard lock(mutex_); ++stats_.workspace_creations; }
        return scratch;
    }
    static bool uniform(const local::PreparedGradeFieldRanges& ranges) {
        const auto rows=ranges.rows_per_tile();const auto total=ranges.action().statistics().target_dimension;
        for(const auto& tile:ranges.tiles())if(tile.end!=tile.begin+std::min(rows,total-tile.begin))return false;
        return true;
    }
    void recycle(std::unique_ptr<Scratch> scratch) noexcept {
        if (!scratch) return;
        std::size_t bytes{};try{bytes=scratch->workspace.scratch_object_bytes();}catch(...){return;}
        const auto slots = scratch->workspace.scratch_slots();
        std::lock_guard lock(mutex_);
        // Active scratch stays private; retain only bounded completed loans.
        if (idle_.size() < limits_.idle_workspaces && slots <= limits_.idle_workspace_slots-idle_slots_ && bytes<=limits_.idle_workspace_bytes-idle_bytes_) {
            idle_slots_ += slots;idle_bytes_+=bytes; idle_.push_back(std::move(scratch));
        }
    }
    void remove(const std::shared_ptr<Job>& job) noexcept {
        pending_bytes_ -= native_field_cache_detail::charge(job->analysis);
        std::erase(jobs_,job);
    }
    void trim(Published& p, std::size_t& plans, std::size_t& results) const {
        const auto count = [&] {
            p.bytes = p.slots = p.result_bytes = 0;
            for (const auto& entry : p.plans) p.bytes = machine::checked_size_add(p.bytes,native_field_cache_detail::charge(entry.analysis));
            for (const auto& entry : p.results) {
                p.bytes = machine::checked_size_add(p.bytes,native_field_cache_detail::charge(entry.analysis));
                p.slots = machine::checked_size_add(p.slots,entry.values->data.size());
                p.result_bytes=machine::checked_size_add(p.result_bytes,entry.values->object_bytes);
            }
        };
        while (p.plans.size() > limits_.plans) { p.plans.erase(p.plans.begin()); ++plans; }
        while (p.results.size() > limits_.results) { p.results.erase(p.results.begin()); ++results; }
        count();
        while (p.bytes > limits_.descriptor_bytes || p.slots > limits_.result_slots || p.result_bytes>limits_.retained_result_bytes) {
            if (p.slots > limits_.result_slots || p.result_bytes>limits_.retained_result_bytes || p.plans.empty() ||
                (!p.results.empty() && p.results.front().order < p.plans.front().order)) {
                p.results.erase(p.results.begin()); ++results;
            } else { p.plans.erase(p.plans.begin()); ++plans; }
            count();
        }
    }
    NativeFieldCacheLimits limits_; Selector selector_;
    std::shared_ptr<const Published> published_;
    std::vector<std::shared_ptr<Job>> jobs_; std::size_t pending_bytes_{};
    mutable std::mutex mutex_; NativeFieldCacheStats stats_;
    std::vector<std::unique_ptr<Scratch>> idle_; std::size_t idle_slots_{},idle_bytes_{};
};
} // namespace rfc::database
