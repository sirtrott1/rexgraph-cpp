#pragma once

#include "rfc/database/native_field_analysis.hpp"
#include "rfc/local/prepared_field_ranges.hpp"
#include "rfc/runtime/cpu_tiles.hpp"

namespace rfc::runtime { struct HardwareInventory; struct LocalityTilePolicy; }

namespace rfc::database {
enum class ParallelFieldStage { BeforeLaunch, BeforeTile, AfterTile, BeforeResult };
// Launch/result callbacks run on the caller; tile callbacks may run concurrently.
using ParallelFieldHook = std::function<void(ParallelFieldStage,std::size_t,std::size_t)>;
class PreparedParallelNativeField;
class ParallelNativeFieldWorkspace;
class DerivedParallelNativeField;
enum class PreparedFieldLayout { TargetRows, IncidenceFibers };

class PreparedParallelNativeField {
public:
    PreparedParallelNativeField(const PreparedParallelNativeField&) = default;
    PreparedParallelNativeField& operator=(const PreparedParallelNativeField&) = default;
    const NativeFieldAnalysis& analysis() const & { return analysis_; }
    const NativeExecutionReport& report() const & { return report_; }
    const local::PreparedGradeFieldRanges& ranges() const & { return ranges_; }
    std::shared_ptr<const numeric::RationalVector> input_pin() const {
        if (!input_) throw std::invalid_argument("moved-from prepared native field");
        return input_;
    }
    const NativeFieldAnalysis& analysis() const && = delete;
    const NativeExecutionReport& report() const && = delete;
    const local::PreparedGradeFieldRanges& ranges() const && = delete;
private:
    PreparedParallelNativeField(NativeFieldAnalysis analysis,local::PreparedGradeFieldRanges ranges,NativeExecutionReport report,
        std::shared_ptr<const numeric::RationalVector> input)
        : analysis_(std::move(analysis)),ranges_(std::move(ranges)),report_(std::move(report)),input_(std::move(input)) {}
    NativeFieldAnalysis analysis_;
    local::PreparedGradeFieldRanges ranges_;
    NativeExecutionReport report_;
    std::shared_ptr<const numeric::RationalVector> input_;
    friend DerivedParallelNativeField execute_parallel(const PreparedParallelNativeField&,ParallelNativeFieldWorkspace&,
        runtime::CpuTileExecutor&,const runtime::ExecutionLease&,const ParallelFieldHook&);
    friend PreparedParallelNativeField prepare_parallel_field_impl(const NativeFieldAnalysis&,std::size_t,
        const runtime::ExecutionHostPolicy&,PreparedFieldLayout);
    friend PreparedParallelNativeField retile_field(const PreparedParallelNativeField&,const runtime::HardwareInventory&,
        const runtime::LocalityTilePolicy&,const runtime::ExecutionHostPolicy&);
};
inline PreparedParallelNativeField prepare_parallel_field_impl(const NativeFieldAnalysis& analysis,
    std::size_t rows_per_tile,const runtime::ExecutionHostPolicy& host,PreparedFieldLayout layout) {
    if (!rows_per_tile) throw std::invalid_argument("parallel field tile rows must be positive");
    const auto& plan = analysis.binding();
    auto requirements = analysis.report().requirements;
    const auto targets = plan.target_dimension();
    const auto tiles = targets/rows_per_tile + static_cast<std::size_t>(targets%rows_per_tile!=0);
    requirements.records = machine::checked_size_add(requirements.records,machine::checked_size_mul(tiles,6));
    requirements.rational_slots = machine::checked_size_add(requirements.rational_slots,
        machine::checked_size_add(machine::checked_size_mul(targets,2),tiles));
    requirements.scalar_work = machine::checked_size_add(requirements.scalar_work,
        machine::checked_size_add(machine::checked_size_mul(tiles,6),machine::checked_size_mul(targets,2)));
    runtime::admit_execution(requirements,host);
    auto fibers = storage::temporal_linear_fiber_system(*plan.endpoint(),plan.selected_carriers());
    if(layout!=PreparedFieldLayout::TargetRows && layout!=PreparedFieldLayout::IncidenceFibers)
        throw std::invalid_argument("unknown prepared field layout");
    local::PreparedGradeFieldRanges ranges(layout==PreparedFieldLayout::IncidenceFibers?
        local::PreparedGradeFieldAction::factored(fibers,plan.upper_grade()):
        local::PreparedGradeFieldAction(fibers,plan.upper_grade()),rows_per_tile);
    auto report = analysis.report(); report.requirements = requirements; report.preparation_policy = host;
    report.selected_backend = runtime::ExecutionBackend::ExactCpu;
    report.fallback = host.backend != runtime::ExecutionBackend::ExactCpu;
    report.implementation = layout==PreparedFieldLayout::IncidenceFibers?
        "PreparedGradeFieldAction/GMP/fiber-tiles-v1":"PreparedGradeFieldAction/GMP/cpu-tiles-v1";
    auto& kernel = report.decisions.at(3); kernel.origin = "cpu-field-tiles-v1"; kernel.value = report.implementation;
    kernel.constraints = "complete coordinates; exclusive row owners; original per-row contribution order; exact GMP";
    report.decisions.push_back({runtime::DecisionKind::Profile,"tile-rows","cpu-field-tiles-v1","1",std::to_string(rows_per_tile),
        "physical ranges over the existing reversible target schedule; no source quotient","physical preparation"});
    numeric::RationalVector input(plan.source_dimension());
    const auto tensors = storage::temporal_endpoint_detail::tensors(*plan.endpoint());
    for (const auto& b : plan.inputs())
        for (const auto& value : tensors.at(b.field)->tensor.values().entries()) input[b.offset+value.index]=value.value;
    report.decisions.push_back({runtime::DecisionKind::Derived,"prepared-input","immutable-native-field-input-v1","1","owning-exact-copy",
        "fixed declared field values from the pinned analysis; read-only across workers; source retained","new analysis/preparation"});
    return {analysis,std::move(ranges),std::move(report),std::make_shared<const numeric::RationalVector>(std::move(input))};
}
inline PreparedParallelNativeField prepare_parallel_field(const NativeFieldAnalysis& analysis,
    std::size_t rows_per_tile=128,const runtime::ExecutionHostPolicy& host={}) {
    return prepare_parallel_field_impl(analysis,rows_per_tile,host,PreparedFieldLayout::TargetRows);
}
inline PreparedParallelNativeField prepare_fiber_field(const NativeFieldAnalysis& analysis,
    std::size_t rows_per_tile=128,const runtime::ExecutionHostPolicy& host={}) {
    return prepare_parallel_field_impl(analysis,rows_per_tile,host,PreparedFieldLayout::IncidenceFibers);
}
class ParallelNativeFieldWorkspace {
public:
    ParallelNativeFieldWorkspace(const ParallelNativeFieldWorkspace&) = delete;
    ParallelNativeFieldWorkspace& operator=(const ParallelNativeFieldWorkspace&) = delete;
    ParallelNativeFieldWorkspace(ParallelNativeFieldWorkspace&&) noexcept = default;
    ParallelNativeFieldWorkspace& operator=(ParallelNativeFieldWorkspace&&) noexcept = default;
    std::size_t scratch_slots() const { return workspace_.scratch_slots(); }
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return analysis_.source_publication(); }
private:
    ParallelNativeFieldWorkspace(NativeFieldAnalysis analysis,local::PreparedGradeFieldRanges::Workspace workspace)
        : analysis_(std::move(analysis)),workspace_(std::move(workspace)) {}
    NativeFieldAnalysis analysis_;
    local::PreparedGradeFieldRanges::Workspace workspace_;
    friend ParallelNativeFieldWorkspace make_parallel_workspace(const PreparedParallelNativeField&,const runtime::ExecutionLease&);
    friend DerivedParallelNativeField execute_parallel(const PreparedParallelNativeField&,ParallelNativeFieldWorkspace&,
        runtime::CpuTileExecutor&,const runtime::ExecutionLease&,const ParallelFieldHook&);
};
inline ParallelNativeFieldWorkspace make_parallel_workspace(const PreparedParallelNativeField& operation,
    const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) {
    lease.check_cancelled(); runtime::admit_execution(operation.report().requirements,lease.policy());
    auto workspace = ParallelNativeFieldWorkspace(operation.analysis(),operation.ranges().make_workspace());
    lease.check_cancelled(); return workspace;
}
class DerivedParallelNativeField {
    struct Data {
        NativeFieldAnalysis analysis;
        NativeExecutionReport report;
        runtime::CpuTileRunReport execution;
        numeric::RationalVector values;
        std::size_t bits;
        Data(NativeFieldAnalysis a,NativeExecutionReport r,runtime::CpuTileRunReport e,numeric::RationalVector v,std::size_t b)
            : analysis(std::move(a)),report(std::move(r)),execution(e),values(std::move(v)),bits(b) {}
    };
public:
    const numeric::RationalVector& values() const & { return data().values; }
    numeric::RationalVector values() const && { return data().values; }
    std::shared_ptr<const numeric::RationalVector> values_pin() const {
        (void)data(); return {data_,&data_->values};
    }
    const NativeFieldAnalysis& analysis() const & { return data().analysis; }
    const NativeFieldAnalysis& analysis() const && = delete;
    const auto& carriers() const & { return data().analysis.binding().outputs(); }
    std::vector<storage::NativeFieldCellBinding> carriers() const && { return data().analysis.binding().outputs(); }
    const NativeExecutionReport& provenance() const & { return data().report; }
    NativeExecutionReport provenance() const && { return data().report; }
    const runtime::CpuTileRunReport& execution() const & { return data().execution; }
    const runtime::CpuTileRunReport& execution() const && = delete;
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return data().analysis.source_publication(); }
    std::size_t observed_integer_bits() const { return data().bits; }
    const numeric::Rational& state(storage::StableCellKey cell,std::size_t coordinate) const & {
        const auto& bindings = carriers();
        const auto it = std::find_if(bindings.begin(),bindings.end(),[&](const auto& b) { return b.cell==cell; });
        if (it==bindings.end() || coordinate>=it->dimension) throw std::out_of_range("parallel native field coordinate");
        return values().at(it->offset+coordinate);
    }
    numeric::Rational state(storage::StableCellKey cell,std::size_t coordinate) const && { return std::as_const(*this).state(cell,coordinate); }
private:
    explicit DerivedParallelNativeField(std::shared_ptr<const Data> data) : data_(std::move(data)) {}
    const Data& data() const { if (!data_) throw std::invalid_argument("moved-from parallel native result"); return *data_; }
    std::shared_ptr<const Data> data_;
    friend DerivedParallelNativeField execute_parallel(const PreparedParallelNativeField&,ParallelNativeFieldWorkspace&,
        runtime::CpuTileExecutor&,const runtime::ExecutionLease&,const ParallelFieldHook&);
};
inline DerivedParallelNativeField execute_parallel(const PreparedParallelNativeField& operation,
    ParallelNativeFieldWorkspace& workspace,runtime::CpuTileExecutor& executor,
    const runtime::ExecutionLease& lease = runtime::ExecutionLease{},const ParallelFieldHook& hook = {}) {
    lease.check_cancelled(); runtime::admit_execution(operation.report().requirements,lease.policy());
    if (operation.ranges().tiles().size()>executor.policy().tile_limit)
        throw runtime::ExecutionLimitExceeded("CPU tiles",operation.ranges().tiles().size(),executor.policy().tile_limit);
    const auto& input = *operation.input_;
    runtime::CpuTileRunReport execution;
    const auto runner = [&](std::size_t tiles,const auto& task) {
        execution = executor.run_tiles(tiles,[&](std::size_t tile,std::size_t worker,const auto& stop) {
            lease.check_cancelled(); stop.check();
            if (hook) hook(ParallelFieldStage::BeforeTile,tile,worker);
            task(tile,stop);
            if (hook) hook(ParallelFieldStage::AfterTile,tile,worker);
        },[&](std::size_t worker) {
            lease.check_cancelled(); if (hook) hook(ParallelFieldStage::BeforeLaunch,0,worker);
        });
    };
    auto values = operation.ranges().apply(input,workspace.workspace_,runner,lease.policy().limits.integer_bits,
        [&] { lease.check_cancelled(); },[&] { if (hook) hook(ParallelFieldStage::BeforeResult,0,0); });
    std::size_t bits{};
    for (const auto& q : values) bits=std::max(bits,storage::native_execution_detail::integer_bits(q));
    if (bits>lease.policy().limits.integer_bits)
        throw runtime::ExecutionLimitExceeded("parallel result integer bits",bits,lease.policy().limits.integer_bits);
    auto report = operation.report(); report.execution_policy = lease.policy();
    report.fallback = lease.policy().backend != runtime::ExecutionBackend::ExactCpu;
    report.decisions.push_back({runtime::DecisionKind::Profile,"CPU-workers","cpu-tiles-v1","1",std::to_string(execution.worker_slots),
        "requested "+std::to_string(execution.requested_workers)+"; caller included; limit "+std::to_string(executor.policy().worker_limit)+
        "; launched "+std::to_string(execution.launched_threads)+"; every tile joined","each execution"});
    auto result = DerivedParallelNativeField(std::make_shared<const DerivedParallelNativeField::Data>(
        operation.analysis(),std::move(report),execution,std::move(values),bits));
    lease.check_cancelled(); return result;
}
} // namespace rfc::database
