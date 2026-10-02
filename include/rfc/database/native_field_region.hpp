#pragma once

#include "rfc/database/native_field_view.hpp"
#include "rfc/runtime/work_graph.hpp"

namespace rfc::database {
class NativeFieldRegionResult;
class PreparedNativeFieldRegion {
    struct State {
        PreparedParallelNativeField operation;
        std::vector<storage::NativeFieldCellBinding> outputs,inputs;
        std::vector<storage::NativeFieldOccurrenceBinding> incoming;
        std::vector<std::size_t> rows;
        explicit State(PreparedParallelNativeField p):operation(std::move(p)){}
    };
public:
    PreparedNativeFieldRegion(const PreparedNativeFieldRegion&)=default;
    PreparedNativeFieldRegion& operator=(const PreparedNativeFieldRegion&)=default;
    static PreparedNativeFieldRegion select(const PreparedParallelNativeField& operation,
        const std::vector<storage::StableCellKey>& requested) {
        auto state=std::make_shared<State>(operation);std::set<storage::StableCellKey> cells(requested.begin(),requested.end());
        if(cells.size()!=requested.size())throw std::invalid_argument("region output contains duplicate stalks");
        for(const auto& binding:operation.analysis().binding().outputs())if(cells.contains(binding.cell)) {
            auto compact=binding;compact.offset=state->rows.size();state->outputs.push_back(compact);
            for(std::size_t j=0;j<binding.dimension;++j)state->rows.push_back(binding.offset+j);
            cells.erase(binding.cell);
        }
        if(!cells.empty())throw std::invalid_argument("region requested a stalk outside the declared output grade");
        std::set<storage::StableCellKey> selected,reads;
        for(const auto& b:state->outputs)selected.insert(b.cell);
        for(const auto& occurrence:operation.analysis().binding().occurrences())if(selected.contains(occurrence.lower)) {
            state->incoming.push_back(occurrence);reads.insert(occurrence.upper);
        }
        for(const auto& b:operation.analysis().binding().inputs())if(reads.contains(b.cell))state->inputs.push_back(b);
        return PreparedNativeFieldRegion(std::move(state));
    }
    const auto& outputs() const & { return state_->outputs; }
    const auto& incoming() const & { return state_->incoming; }
    const auto& inputs() const & { return state_->inputs; }
    const auto& operation() const & { return state_->operation; }
    const std::vector<storage::NativeFieldCellBinding>& outputs() const &&=delete;
    const std::vector<storage::NativeFieldOccurrenceBinding>& incoming() const &&=delete;
    const std::vector<storage::NativeFieldCellBinding>& inputs() const &&=delete;
    const PreparedParallelNativeField& operation() const &&=delete;
    std::size_t output_dimension() const { return state_->rows.size(); }
    NativeFieldRegionResult execute(const runtime::ExecutionLease& lease=runtime::ExecutionLease{},
        std::optional<runtime::WorkAvailability> cutoff={}) const;
private:
    explicit PreparedNativeFieldRegion(std::shared_ptr<const State> s):state_(std::move(s)){}
    std::shared_ptr<const State> state_;
    friend class NativeFieldRegionResult;
};
class NativeFieldRegionResult {
public:
    const numeric::RationalVector& values() const & { return *values_; }
    const numeric::RationalVector& values() const &&=delete;
    std::shared_ptr<const numeric::RationalVector> values_pin() const { return values_; }
    const PreparedNativeFieldRegion& plan() const & { return plan_; }
    const PreparedNativeFieldRegion& plan() const &&=delete;
    std::span<const numeric::Rational> at_cell(storage::StableCellKey cell) const & {
        for(const auto& b:plan_.outputs())if(b.cell==cell)return std::span<const numeric::Rational>(*values_).subspan(b.offset,b.dimension);
        throw std::out_of_range("cell is outside selected field output");
    }
    std::span<const numeric::Rational> at_cell(storage::StableCellKey) const &&=delete;
private:
    NativeFieldRegionResult(PreparedNativeFieldRegion p,numeric::RationalVector v)
        :plan_(std::move(p)),values_(std::make_shared<const numeric::RationalVector>(std::move(v))){}
    PreparedNativeFieldRegion plan_;
    std::shared_ptr<const numeric::RationalVector> values_;
    friend class PreparedNativeFieldRegion;
    friend struct StagedNativeFieldResult;
};
inline NativeFieldRegionResult PreparedNativeFieldRegion::execute(const runtime::ExecutionLease& lease,
    std::optional<runtime::WorkAvailability> cutoff) const {
    lease.check_cancelled();const auto& operation=state_->operation;
    runtime::admit_work_availability(operation.report().source_namespace,operation.report().sequence,cutoff);
    // Complete native receipts and preparation admission stay conservative.
    runtime::admit_execution(operation.report().requirements,lease.policy());
    auto values=operation.ranges().action().apply_selected(*operation.input_pin(),state_->rows,
        lease.policy().limits.integer_bits,[&]{lease.check_cancelled();});
    lease.check_cancelled();return {*this,std::move(values)};
}
inline runtime::WorkJob make_region_work(runtime::WorkKey key,const PreparedNativeFieldRegion& region,
    std::vector<runtime::WorkKey> after={},std::optional<runtime::WorkAvailability> cutoff={},
    const runtime::ExecutionHostPolicy& host={}) {
    runtime::admit_execution(region.operation().report().requirements,host);
    runtime::admit_work_availability(region.operation().report().source_namespace,region.operation().report().sequence,cutoff);
    const auto bytes=machine::checked_size_mul(machine::checked_size_add(region.output_dimension(),1),sizeof(numeric::Rational));
    return runtime::WorkJob::make<NativeFieldRegionResult>(key,{1,bytes,bytes},std::move(after),
        [region,host,cutoff](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto cancel=std::make_shared<std::atomic_bool>(stop.stop_requested());
            std::stop_callback forward(stop,[cancel]{cancel->store(true,std::memory_order_release);});
            auto reservation=assigned.reserve(1);return region.execute(runtime::ExecutionLease(host,cancel),cutoff);
        });
}
} // namespace rfc::database
