#pragma once

#include "rfc/local/owned_fiber_system.hpp"
#include "rfc/local/semantic_stalk_realization.hpp"
#include "rfc/local/prepared_field_action.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"

namespace rfc::local {
// An owned declared realization. This profile validates the complete realized
// chain family on both sides; it does not assert Hodge or metric naturality.
class OwnedSemanticBoundary {
    struct State {
        OwnedLinearFiberSystem macro,micro;
        geometry::SemanticScaleRealization cells;
        SemanticStalkRealization realization;
        std::vector<PreparedGradeFieldAction> macro_actions,micro_actions;
        static geometry::SemanticScaleRealization copy_cells(const SemanticStalkRealization& source,
            const OwnedLinearFiberSystem& macro,const OwnedLinearFiberSystem& micro) {
            const auto copy=[](const geometry::RelationalChainMap& map) {
                std::vector<geometry::LinearGradeMap> maps;
                for(std::size_t k=0;k<map.source().grade_count();++k) {
                    const auto grade=core::Grade{static_cast<std::uint16_t>(k)};geometry::LinearGradeMap row;
                    for(std::size_t i=0;i<map.source().grade(grade).cell_count();++i)row.push_back(map.image(grade,i));
                    maps.push_back(std::move(row));
                }
                return maps;
            };
            std::optional<geometry::RelationalChainMap> retract;
            if(source.cell_realization().has_coarsening())retract.emplace(micro.fibers().complex(),macro.fibers().complex(),copy(source.cell_realization().coarsening()));
            return {source.cell_realization().macro_scale(),source.cell_realization().micro_scale(),
                geometry::RelationalChainMap(macro.fibers().complex(),micro.fibers().complex(),copy(source.cell_realization().realization())),std::move(retract)};
        }
        static std::vector<numeric::ExactMatrix> maps(const SemanticStalkRealization& source,bool retract) {
            std::vector<numeric::ExactMatrix> out;
            for(std::size_t k=0;k<source.macro().complex().grade_count();++k) {
                const auto grade=core::Grade{static_cast<std::uint16_t>(k)};out.push_back(retract?source.coarsening(grade):source.map(grade));
            }
            return out;
        }
        explicit State(const SemanticStalkRealization& source)
            :macro(source.macro()),micro(source.micro()),cells(copy_cells(source,macro,micro)),
             realization(macro.fibers(),micro.fibers(),cells,maps(source,false),
                source.has_coarsening()?std::optional{maps(source,true)}:std::nullopt) {
            for(std::size_t k=1;k<macro.fibers().complex().grade_count();++k)
                macro_actions.push_back(PreparedGradeFieldAction::factored(macro.fibers(),core::Grade{static_cast<std::uint16_t>(k)}));
            for(std::size_t k=1;k<micro.fibers().complex().grade_count();++k)
                micro_actions.push_back(PreparedGradeFieldAction::factored(micro.fibers(),core::Grade{static_cast<std::uint16_t>(k)}));
        }
    };
public:
    OwnedSemanticBoundary(const OwnedSemanticBoundary&)=default;
    OwnedSemanticBoundary& operator=(const OwnedSemanticBoundary&)=default;
    struct FineDecomposition {
        numeric::RationalVector macro_input,fine_residual,macro_boundary,realized_boundary,residual_boundary,full_boundary;
    };
    explicit OwnedSemanticBoundary(const SemanticStalkRealization& source,numeric::ExactActionLimits limits={},
        std::function<void()> checkpoint={}) {
        numeric::ExactActionGuard guard(limits,std::move(checkpoint));
        validate_chain(source.macro(),guard);validate_chain(source.micro(),guard);
        for(std::size_t k=0;k<source.macro().complex().grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};guard.matrix(source.map(grade));
            if(source.has_coarsening())guard.matrix(source.coarsening(grade));
        }
        state_=std::make_shared<const State>(source);
    }
    const SemanticStalkRealization& realization() const & {return state_->realization;}
    const SemanticStalkRealization& realization() const &&=delete;
    numeric::RationalVector apply_realized(core::Grade grade,const numeric::RationalVector& macro_input,
        numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}) const {
        require_grade(grade);numeric::ExactActionGuard guard(limits,std::move(checkpoint));
        const auto boundary=checked_boundary(state_->macro_actions.at(grade.value-1u),macro_input,guard);
        return numeric::checked_apply(state_->realization.map(lower(grade)),boundary,guard);
    }
    FineDecomposition apply_fine(core::Grade grade,const numeric::RationalVector& fine_input,
        numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}) const {
        require_grade(grade);const auto& s=state_->realization;
        if(!s.has_coarsening())throw std::invalid_argument("fine decomposition requires a declared checked retraction");
        numeric::ExactActionGuard guard(limits,std::move(checkpoint));FineDecomposition out;
        const auto input=guard.canonical_input(fine_input);
        out.macro_input=numeric::checked_apply(s.coarsening(grade),input,guard);
        const auto realized=numeric::checked_apply(s.map(grade),out.macro_input,guard);
        out.fine_residual.resize(fine_input.size());
        for(std::size_t j=0;j<input.size();++j)out.fine_residual[j]=guard.sub(input[j],realized[j]);
        const auto null=numeric::checked_apply(s.coarsening(grade),out.fine_residual,guard);
        if(std::any_of(null.begin(),null.end(),[](const auto& q){return q!=0;}))throw std::logic_error("semantic residual leaves retraction kernel");
        out.macro_boundary=checked_boundary(state_->macro_actions.at(grade.value-1u),out.macro_input,guard);
        out.realized_boundary=numeric::checked_apply(s.map(lower(grade)),out.macro_boundary,guard);
        out.residual_boundary=checked_boundary(state_->micro_actions.at(grade.value-1u),out.fine_residual,guard);
        out.full_boundary.resize(out.realized_boundary.size());
        for(std::size_t j=0;j<out.full_boundary.size();++j)out.full_boundary[j]=guard.add(out.realized_boundary[j],out.residual_boundary[j]);
        return out;
    }
private:
    static core::Grade lower(core::Grade grade){return {static_cast<std::uint16_t>(grade.value-1u)};}
    void require_grade(core::Grade grade) const {
        if(!grade.value || grade.value>=state_->macro.fibers().complex().grade_count())throw std::out_of_range("semantic boundary grade");
    }
    static numeric::RationalVector checked_boundary(const PreparedGradeFieldAction& action,
        const numeric::RationalVector& input,numeric::ExactActionGuard& guard) {
        guard.dimension(action.statistics().source_dimension);guard.dimension(action.statistics().target_dimension);
        guard.entries(action.statistics().entries);const auto canonical=guard.canonical_input(input);
        std::vector<std::size_t> rows(action.statistics().target_dimension);
        for(std::size_t j=0;j<rows.size();++j)rows[j]=j;
        return action.apply_selected(canonical,rows,guard.limits().integer_bits,[&]{guard.step();});
    }
    static void validate_chain(const LinearRelationalFiberSystem& fibers,numeric::ExactActionGuard& guard) {
        std::optional<numeric::ExactMatrix> previous;
        for(std::size_t k=1;k<fibers.complex().grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            guard.dimension(fibers.stalks().total_dimension(grade));guard.dimension(fibers.stalks().total_dimension(lower(grade)));
            std::size_t entries{};
            const auto& arena=fibers.complex().grade(grade);
            for(std::size_t slot=0;slot<arena.incidence_count();++slot) {
                guard.step();const auto& map=fibers.transport_override(grade,slot);
                if(map)guard.matrix(*map);
                if(!arena.aspects.get(fibers.complex().signature().existence_aspect(),slot))continue;
                const auto count=map?map->nonzeros():(fibers.complex().incidence_coefficient(grade,slot)==0?0:
                    fibers.stalks().dimension({lower(grade),arena.lower_index[slot]}));
                entries=machine::checked_size_add(entries,count);guard.entries(entries);
            }
            auto boundary=GradeTensorField(fibers,grade).matrix();guard.matrix(boundary);
            if(previous && numeric::checked_multiply(*previous,boundary,guard).nonzeros())
                throw std::invalid_argument("owned semantic boundary requires the complete strict realized chain family");
            previous=std::move(boundary);
        }
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::local
