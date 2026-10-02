#pragma once

#include "rfc/database/native_field_region.hpp"
#include "rfc/local/owned_semantic_boundary.hpp"

namespace rfc::database {
class NativeSemanticFieldResult;
class PreparedNativeSemanticField {
    struct State {
        PreparedParallelNativeField macro,micro;
        std::uint64_t static_declaration{};
        local::OwnedSemanticBoundary boundary;
        State(PreparedParallelNativeField a,PreparedParallelNativeField b,std::uint64_t declaration,local::OwnedSemanticBoundary action)
            :macro(std::move(a)),micro(std::move(b)),static_declaration(declaration),boundary(std::move(action)) {
            if(macro.analysis().binding().upper_grade()!=micro.analysis().binding().upper_grade())
                throw std::invalid_argument("native semantic actions require the same chain grade");
        }
    };
public:
    PreparedNativeSemanticField(const PreparedNativeSemanticField&)=default;
    PreparedNativeSemanticField& operator=(const PreparedNativeSemanticField&)=default;
    // Native fibers are reconstructed from the same pinned declared source used
    // by each sealed operation. No dimensions-only/raw-action binding is accepted.
    // Maps in this profile are explicitly declared mathematical constants.
    // Fitted/time-varying maps require a separate source/availability binding.
    static PreparedNativeSemanticField prepare_static(const PreparedParallelNativeField& macro,const PreparedParallelNativeField& micro,
        std::uint64_t declaration,
        std::vector<geometry::LinearGradeMap> cells,std::vector<numeric::ExactMatrix> maps,
        std::optional<std::vector<numeric::ExactMatrix>> retract={},
        std::optional<std::vector<geometry::LinearGradeMap>> cell_retract={},numeric::ExactActionLimits limits={}) {
        if(!declaration)throw std::invalid_argument("native semantic static maps require an explicit declaration identity");
        numeric::ExactActionGuard preflight(limits);
        for(const auto& map:maps)preflight.matrix(map);
        if(retract)for(const auto& map:*retract)preflight.matrix(map);
        auto mf=storage::temporal_linear_fiber_system(*macro.analysis().source_publication(),macro.analysis().binding().selected_carriers());
        auto uf=storage::temporal_linear_fiber_system(*micro.analysis().source_publication(),micro.analysis().binding().selected_carriers());
        const auto& a=mf.complex();const auto& b=uf.complex();std::optional<geometry::RelationalChainMap> reverse;
        if(cell_retract)reverse.emplace(b,a,std::move(*cell_retract));
        geometry::SemanticScaleRealization scale_maps(a.semantic_scale(),b.semantic_scale(),geometry::RelationalChainMap(a,b,std::move(cells)),std::move(reverse));
        local::SemanticStalkRealization realization(mf,uf,std::move(scale_maps),std::move(maps),std::move(retract));
        return PreparedNativeSemanticField(std::make_shared<const State>(macro,micro,declaration,local::OwnedSemanticBoundary(realization,limits)));
    }
    NativeSemanticFieldResult execute_fine(const runtime::ExecutionLease& lease=runtime::ExecutionLease{},
        std::optional<runtime::WorkAvailability> macro_cutoff={},std::optional<runtime::WorkAvailability> micro_cutoff={},
        numeric::ExactActionLimits limits={}) const;
    const auto& macro() const & {return state_->macro;}
    const auto& micro() const & {return state_->micro;}
    std::uint64_t static_declaration() const {return state_->static_declaration;}
    const PreparedParallelNativeField& macro() const &&=delete;
    const PreparedParallelNativeField& micro() const &&=delete;
private:
    explicit PreparedNativeSemanticField(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
    friend class NativeSemanticFieldResult;
};
class NativeSemanticFieldResult {
public:
    const local::OwnedSemanticBoundary::FineDecomposition& decomposition() const & {return data_;}
    const PreparedNativeSemanticField& plan() const & {return plan_;}
    const local::OwnedSemanticBoundary::FineDecomposition& decomposition() const &&=delete;
    const PreparedNativeSemanticField& plan() const &&=delete;
private:
    NativeSemanticFieldResult(PreparedNativeSemanticField plan,local::OwnedSemanticBoundary::FineDecomposition data)
        :plan_(std::move(plan)),data_(std::move(data)){}
    PreparedNativeSemanticField plan_;local::OwnedSemanticBoundary::FineDecomposition data_;
    friend class PreparedNativeSemanticField;
};
inline NativeSemanticFieldResult PreparedNativeSemanticField::execute_fine(const runtime::ExecutionLease& lease,
    std::optional<runtime::WorkAvailability> macro_cutoff,std::optional<runtime::WorkAvailability> micro_cutoff,
    numeric::ExactActionLimits limits) const {
    lease.check_cancelled();
    for(const auto* op:{&state_->macro,&state_->micro})runtime::admit_execution(op->report().requirements,lease.policy());
    runtime::admit_work_availability(state_->macro.report().source_namespace,state_->macro.report().sequence,macro_cutoff);
    runtime::admit_work_availability(state_->micro.report().source_namespace,state_->micro.report().sequence,micro_cutoff);
    limits.integer_bits=std::min(limits.integer_bits,lease.policy().limits.integer_bits);
    limits.steps=std::min(limits.steps,lease.policy().limits.scalar_work);
    auto data=state_->boundary.apply_fine(state_->micro.analysis().binding().upper_grade(),*state_->micro.input_pin(),limits,[&]{lease.check_cancelled();});
    lease.check_cancelled();return {*this,std::move(data)};
}
} // namespace rfc::database
