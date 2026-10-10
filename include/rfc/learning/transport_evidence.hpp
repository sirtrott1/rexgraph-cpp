#pragma once
#include "rfc/learning/column_transport_observations.hpp"
#include <mutex>
#include <variant>
#include <atomic>

namespace rfc::learning {
struct TransportEvidenceUpdate {
    std::size_t reused_observations{}, assembled_observations{}, execution_steps{};
};

// Immutable original observation history and its complete exact affine family.
// Appends assemble new fields and insert their equations into retained sparse rows.
class PreparedTransportEvidence {
    using Q = numeric::Rational;
    using V = numeric::RationalVector;
    using M = numeric::ExactMatrix;
    using Guard = numeric::ExactActionGuard;
    using Row = std::map<std::size_t,Q>;
    struct State {
        std::shared_ptr<const PreparedTransportObservations> model;
        numeric::ExactActionLimits limits;
        std::shared_ptr<const M> fixed_equations;
        using Observation=std::variant<TransportObservation,TransportColumnObservation>;
        std::vector<Observation> history;
        std::shared_ptr<const PreparedColumnTransportObservations> columns;
        mutable std::once_flag materialization;
        mutable std::vector<TransportObservation> whole_fields;
        mutable std::atomic<std::size_t> materialized_entries{},materialization_steps{};
        TransportObservationSystem system;
        std::map<std::size_t,Row> rows, observation_rows;
        V particular;
        M kernel;
        std::vector<std::size_t> pivots, free_columns;
        std::size_t input_entries{}, row_entries{}, history_steps{}, family_steps{}, initial_steps{}, copied_entries{};
        std::size_t observation_row_entries{}, fixed_rank{};
        TransportEvidenceUpdate update;

        static std::size_t add(std::size_t a,std::size_t b) {return machine::checked_size_add(a,b);}
        std::size_t retained() const {
            auto count=add(model->retained_entries(),input_entries);
            if(columns)count=add(count,columns->retained_entries()-model->retained_entries());
            for(const auto n:{system.design.nonzeros(),system.augmented.nonzeros(),system.target.size(),
                    machine::checked_size_mul(system.constraints.size(),3),row_entries,rows.size(),
                    particular.size(),kernel.nonzeros(),pivots.size(),free_columns.size()}) count=add(count,n);
            if(fixed_equations) {
                count=add(count,add(fixed_equations->nonzeros(),add(observation_row_entries,observation_rows.size())));
            }
            return count;
        }
        void workspace(Guard& guard,std::size_t scratch=0) const {
            guard.entries(add(add(add(retained(),materialized_entries.load()),copied_entries),scratch));
        }
        void admit(Guard& guard) const {
            const auto& requested=guard.limits();
            if(requested.dimension!=limits.dimension || requested.entries!=limits.entries ||
                    requested.steps!=limits.steps || requested.integer_bits!=limits.integer_bits)
                throw std::invalid_argument("transport evidence append requires its original action limits");
            guard.dimension(add(model->parameter_count(),1));guard.dimension(history.size());
            guard.matrix(system.design);guard.matrix(system.augmented);guard.vector(system.target);
            workspace(guard);
        }
        State(std::shared_ptr<const PreparedTransportObservations> owner,Guard& guard,std::shared_ptr<const M> fixed={})
            :model(std::move(owner)),limits(guard.limits()),fixed_equations(std::move(fixed)),
             system{M(0,model->parameter_count()),M(0,add(model->parameter_count(),1)),{},{}},
             kernel(model->parameter_count(),0) {
            const auto start=guard.steps();
            guard.step();guard.dimension(add(model->parameter_count(),1));workspace(guard);
            if(fixed_equations) {
                guard.matrix(*fixed_equations);
                if(fixed_equations->cols()!=model->parameter_count())
                    throw std::invalid_argument("fixed transport equations require the original parameter carrier");
                auto next=fixed_equations->entries().begin();
                for(std::size_t i=0;i<fixed_equations->rows();++i) {
                    guard.step();Row row;
                    while(next!=fixed_equations->entries().end() && next->first.first==i) {
                        guard.step();row.emplace(next->first.second,next->second);++next;workspace(guard,row.size());
                    }
                    insert(std::move(row),guard);
                }
                fixed_rank=rows.size();
            }
            finish(guard);
            initial_steps=guard.steps()-start;
        }
        template<class ObservationType>
        State(const State& parent,const std::vector<ObservationType>& suffix,Guard& guard)
            :model(parent.model),limits(parent.limits),fixed_equations(parent.fixed_equations),kernel(model->parameter_count(),0) {
            const auto start=guard.steps();guard.step();parent.admit(guard);
            if(suffix.empty()) throw std::invalid_argument("nonempty original observation suffix required");
            guard.dimension(add(parent.history.size(),suffix.size()));
            guard.entries(add(machine::checked_size_mul(parent.retained(),2),parent.materialized_entries.load()));
            history=parent.history;columns=parent.columns;system=parent.system;rows=parent.rows;observation_rows=parent.observation_rows;
            observation_row_entries=parent.observation_row_entries;fixed_rank=parent.fixed_rank;
            input_entries=parent.input_entries;row_entries=parent.row_entries;history_steps=parent.history_steps;
            initial_steps=parent.initial_steps;
            if constexpr(std::is_same_v<ObservationType,TransportColumnObservation>) {
                if(!columns) {
                    const auto preparing=guard.steps();
                    columns=std::make_shared<const PreparedColumnTransportObservations>(model,guard);
                    initial_steps=add(initial_steps,guard.steps()-preparing);
                }
            }
            copied_entries=add(parent.retained(),parent.materialized_entries.load());workspace(guard);
            for(const auto& observation:suffix) append(observation,guard);
            finish(guard);copied_entries=0;
            if(replay_steps()>limits.steps)throw std::length_error("complete transport evidence replay exceeds step admission");
            update={parent.history.size(),suffix.size(),guard.steps()-start};
        }
        void change(Row& row,std::size_t column,Q value,Guard& guard,std::size_t scratch,std::size_t& retained_rows) {
            const auto found=row.find(column);
            if(value==0) {
                if(found!=row.end()){row.erase(found);--retained_rows;}
            } else {
                if(found==row.end()) {
                    workspace(guard,add(scratch,1));retained_rows=add(retained_rows,1);
                }
                row[column]=std::move(value);
            }
        }
        void insert(Row row,Guard& guard,bool observation_only=false,std::size_t external=0) {
            auto& family=observation_only?observation_rows:rows;
            auto& retained_rows=observation_only?observation_row_entries:row_entries;
            workspace(guard,add(row.size(),external));
            for(const auto& [pivot,known]:family) {
                guard.step();const auto found=row.find(pivot);
                if(found==row.end()) continue;
                const Q scale=found->second;
                for(const auto& [column,value]:known) {
                    const auto current=row.find(column);
                    auto next=guard.sub(current==row.end()?Q{0}:current->second,guard.mul(scale,value));
                    if(next==0) {if(current!=row.end())row.erase(current);}
                    else {workspace(guard,add(external,add(row.size(),1)));row[column]=std::move(next);}
                }
            }
            if(row.empty()) return;
            const auto pivot=row.begin()->first;
            if(pivot==model->parameter_count()) throw std::invalid_argument("inconsistent exact transport observations");
            const Q scale=row.begin()->second;
            for(auto& [column,value]:row){(void)column;value=guard.div(value,scale);}
            for(auto& [original,known]:family) {
                (void)original;guard.step();const auto found=known.find(pivot);
                if(found==known.end()) continue;
                const Q factor=found->second;
                for(const auto& [column,value]:row) {
                    const auto current=known.find(column);
                    auto next=guard.sub(current==known.end()?Q{0}:current->second,guard.mul(factor,value));
                    change(known,column,std::move(next),guard,add(row.size(),external),retained_rows);
                }
            }
            workspace(guard,add(external,add(row.size(),1)));retained_rows=add(retained_rows,row.size());
            if(!family.emplace(pivot,std::move(row)).second) throw std::logic_error("duplicate transport evidence pivot");
        }
        template<class ObservationType>
        void append(const ObservationType& observation,Guard& guard) {
            const auto start=guard.steps();
            TransportObservationSystem block;
            std::size_t entries{};
            if constexpr(std::is_same_v<ObservationType,TransportColumnObservation>) {
                block=columns->assemble({observation},guard);entries=columns->observation_entries(observation);
            } else {
                block=model->assemble({observation},guard);
                entries=add(observation.input.size(),add(observation.target.size(),observation.observed.size()));
            }
            const auto offset=system.design.rows(),count=add(offset,block.design.rows()),n=model->parameter_count();
            guard.dimension(count);guard.dimension(add(history.size(),1));
            input_entries=add(input_entries,entries);
            workspace(guard,add(block.design.nonzeros(),block.augmented.nonzeros()));
            M design(count,n),augmented(count,add(n,1));
            for(const auto* original:{&system.design,&system.augmented}) {
                auto& destination=original==&system.design?design:augmented;
                for(const auto& [key,value]:original->entries()) {
                    guard.step();destination.set(key.first,key.second,value);
                    workspace(guard,add(design.nonzeros(),augmented.nonzeros()));
                }
            }
            system.design=std::move(design);system.augmented=std::move(augmented);
            for(const auto* original:{&block.design,&block.augmented}) {
                auto& destination=original==&block.design?system.design:system.augmented;
                for(const auto& [key,value]:original->entries()) {
                    guard.step();destination.set(add(offset,key.first),key.second,value);workspace(guard);
                }
            }
            for(const auto& value:block.target){guard.step();system.target.push_back(value);workspace(guard);}
            for(const auto& constraint:block.constraints) {
                guard.step();system.constraints.push_back({history.size(),constraint.cell,constraint.axis});workspace(guard);
            }
            auto next=block.augmented.entries().begin();
            for(std::size_t i=0;i<block.augmented.rows();++i) {
                guard.step();Row row;
                while(next!=block.augmented.entries().end() && next->first.first==i) {
                    guard.step();row.emplace(next->first.second,next->second);++next;workspace(guard,row.size());
                }
                if(fixed_equations)insert(row,guard,true,row.size());
                insert(std::move(row),guard);
            }
            history.emplace_back(observation);workspace(guard);
            history_steps=add(history_steps,guard.steps()-start);
        }
        void finish(Guard& guard) {
            const auto start=guard.steps(),n=model->parameter_count();
            particular=V(n);pivots.clear();free_columns.clear();
            std::vector<std::size_t> free_axis(n,n);
            for(std::size_t j=0;j<n;++j) {
                guard.step();if(rows.contains(j))pivots.push_back(j);
                else {free_axis[j]=free_columns.size();free_columns.push_back(j);}
            }
            kernel=M(n,free_columns.size());workspace(guard,free_axis.size());
            for(std::size_t i=0;i<free_columns.size();++i) {
                guard.step();kernel.set(free_columns[i],i,1);workspace(guard,free_axis.size());
            }
            for(const auto& [pivot,row]:rows) for(const auto& [column,value]:row) {
                guard.step();
                if(column==n)particular[pivot]=value;
                else if(column!=pivot) {
                    if(free_axis[column]==n)throw std::logic_error("transport evidence row is not reduced");
                    kernel.set(pivot,free_axis[column],guard.sub(0,value));workspace(guard,free_axis.size());
                }
            }
            if(numeric::checked_apply(system.design,particular,guard)!=system.target ||
                    numeric::checked_multiply(system.design,kernel,guard).nonzeros())
                throw std::logic_error("complete transport evidence family certificate failed");
            if(fixed_equations && (numeric::checked_apply(*fixed_equations,particular,guard)!=V(fixed_equations->rows()) ||
                    numeric::checked_multiply(*fixed_equations,kernel,guard).nonzeros()))
                throw std::logic_error("fixed transport equation family certificate failed");
            workspace(guard);family_steps=guard.steps()-start;
        }
        const std::vector<TransportObservation>& materialize(Guard& guard) const {
            guard.step();std::size_t complete{};
            const auto& fibers=model->owner().fibers();
            for(const auto grade:{core::Grade{static_cast<std::uint16_t>(model->grade().value-1)},model->grade()})
                for(std::size_t i=0;i<fibers.complex().grade(grade).cell_count();++i) {
                    guard.step();complete=add(complete,fibers.stalks().dimension({grade,static_cast<std::uint32_t>(i)}));
                }
            complete=add(complete,fibers.complex().grade(core::Grade{static_cast<std::uint16_t>(model->grade().value-1)}).cell_count());
            const auto needed=machine::checked_size_mul(history.size(),complete);
            guard.entries(add(retained(),needed));
            const auto start=guard.steps();bool prepared=false;
            std::call_once(materialization,[&] {
                std::vector<TransportObservation> fields;fields.reserve(history.size());
                for(const auto& item:history) {
                    guard.step();
                    if(const auto* dense=std::get_if<TransportObservation>(&item))fields.push_back(*dense);
                    else fields.push_back(columns->materialize(std::get<TransportColumnObservation>(item),guard));
                }
                whole_fields=std::move(fields);materialized_entries.store(needed);
                materialization_steps.store(guard.steps()-start);prepared=true;
            });
            if(!prepared)for(std::size_t i=0;i<materialization_steps.load();++i)guard.step();
            return whole_fields;
        }
        std::size_t replay_steps() const {
            return history.empty()?initial_steps:add(initial_steps,add(1,add(history_steps,family_steps)));
        }
    };
    explicit PreparedTransportEvidence(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
public:
    PreparedTransportEvidence(PreparedTransportObservations owner,Guard& guard)
        :state_(std::make_shared<const State>(std::make_shared<const PreparedTransportObservations>(std::move(owner)),guard)) {}
    // Homogeneous fixed equations retain the original parameter coordinates.
    PreparedTransportEvidence(PreparedTransportObservations owner,M fixed_equations,Guard& guard)
        :state_(std::make_shared<const State>(std::make_shared<const PreparedTransportObservations>(std::move(owner)),guard,
            std::make_shared<const M>(std::move(fixed_equations)))) {}
    std::size_t fixed_rank() const noexcept {return state_->fixed_rank;}
    std::size_t observation_rank() const noexcept {return state_->fixed_equations?state_->observation_rows.size():rank();}
    PreparedTransportEvidence append(const std::vector<TransportObservation>& suffix,Guard& guard) const {
        return PreparedTransportEvidence(std::make_shared<const State>(*state_,suffix,guard));
    }
    const PreparedTransportObservations& model() const & {return *state_->model;}
    const std::vector<TransportObservation>& observations() const & {
        Guard guard(state_->limits);return state_->materialize(guard);
    }
    const std::vector<TransportObservation>& observations(Guard& guard) const & {return state_->materialize(guard);}
    std::size_t materialized_field_entries() const noexcept {return state_->materialized_entries.load();}
    std::size_t observation_count() const noexcept {return state_->history.size();}
    std::size_t column_observation_count() const noexcept {
        return static_cast<std::size_t>(std::count_if(state_->history.begin(),state_->history.end(),
            [](const auto& item){return std::holds_alternative<TransportColumnObservation>(item);}));
    }
    PreparedTransportEvidence append_columns(const std::vector<TransportColumnObservation>& suffix,Guard& guard) const {
        return PreparedTransportEvidence(std::make_shared<const State>(*state_,suffix,guard));
    }
    const TransportObservationSystem& system() const & {return state_->system;}
    const V& particular() const & {return state_->particular;}
    const M& kernel() const & {return state_->kernel;}
    const std::vector<std::size_t>& pivots() const & {return state_->pivots;}
    const std::vector<std::size_t>& free_columns() const & {return state_->free_columns;}
    const PreparedTransportObservations& model() const &&=delete;
    const std::vector<TransportObservation>& observations() const &&=delete;
    const TransportObservationSystem& system() const &&=delete;
    const V& particular() const &&=delete;
    const M& kernel() const &&=delete;
    const std::vector<std::size_t>& pivots() const &&=delete;
    const std::vector<std::size_t>& free_columns() const &&=delete;
    std::size_t rank() const noexcept {return state_->pivots.size();}
    std::size_t retained_entries() const {return state_->retained();}
    // Original ordered equation construction and reduction, plus final certification.
    // Prefix validation and copy work are reported separately by update().
    std::size_t preparation_steps() const {return state_->replay_steps();}
    TransportEvidenceUpdate update() const noexcept {return state_->update;}
};
} // namespace rfc::learning
