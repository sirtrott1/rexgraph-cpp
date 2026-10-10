#pragma once
#include "rfc/learning/transport_observations.hpp"
#include "rfc/numeric/sparse_tensor.hpp"
#include <map>

namespace rfc::learning {
struct TransportColumnField {
    std::size_t cell{};
    numeric::SparseRationalTensor tensor;
};
// Listed input columns carry complete stalk fields. Other original columns
// have the declared zero field. Targets list whole observed lower stalks.
struct TransportColumnObservation {
    local::OwnedLinearFiberSystem owner;
    core::Grade grade;
    std::vector<TransportColumnField> input, target;
};

class PreparedColumnTransportObservations {
    using Q=numeric::Rational;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct Term {std::size_t occurrence{},cell{},axis{},parameter{};Q coefficient;};
    std::shared_ptr<const PreparedTransportObservations> model_;
    std::map<std::pair<std::size_t,std::size_t>,std::vector<Term>> variable_,fixed_;
    std::size_t retained_{};

    void fields(const std::vector<TransportColumnField>& values,core::Grade grade,Guard& guard) const {
        guard.dimension(values.size());std::optional<std::size_t> previous;
        const auto& fibers=model_->owner().fibers();
        for(const auto& field:values) {
            guard.step();
            if(field.cell>=fibers.complex().grade(grade).cell_count() || (previous && field.cell<=*previous))
                throw std::invalid_argument("ordered distinct original stalk cells required");
            previous=field.cell;
            const auto dimension=fibers.stalks().dimension({grade,static_cast<std::uint32_t>(field.cell)});
            if(field.tensor.domain().extents()!=std::vector<std::size_t>{dimension} ||
                    field.tensor.support().count()!=dimension)
                throw std::invalid_argument("complete sparse tensor on the original stalk required");
            guard.dimension(dimension);
            for(const auto& e:field.tensor.values().entries()){guard.step();guard.value(e.value);}
        }
    }
public:
    explicit PreparedColumnTransportObservations(PreparedTransportObservations model,Guard& guard)
        :PreparedColumnTransportObservations(std::make_shared<const PreparedTransportObservations>(std::move(model)),guard) {}
    explicit PreparedColumnTransportObservations(std::shared_ptr<const PreparedTransportObservations> model,Guard& guard)
        :model_(std::move(model)),retained_(model_?model_->retained_entries():0) {
        if(!model_)throw std::invalid_argument("original transport observation owner required");
        const auto grade=model_->grade();const auto& source=model_->owner().fibers().complex();
        const auto& a=source.grade(grade);
        const auto& lo=source.grade(core::Grade{static_cast<std::uint16_t>(grade.value-1)});
        const auto insert=[&](auto& index,std::size_t slot,std::size_t row,std::size_t column,
                             std::size_t parameter,const Q& coefficient) {
            guard.step();retained_=machine::checked_size_add(retained_,5);guard.entries(retained_);
            const auto key=std::pair<std::size_t,std::size_t>{a.upper_index[slot],column};
            if(!index.contains(key)){retained_=machine::checked_size_add(retained_,5);guard.entries(retained_);}
            index[key].push_back({slot,a.lower_index[slot],row,parameter,coefficient});
        };
        const auto variable=[&](std::size_t i,std::size_t parameter,const Q& gain) {
            const auto& s=model_->variable_coordinates()[i];guard.step();
            if(!a.aspects.get(source.signature().existence_aspect(),s.occurrence)) return;
            const auto coefficient=guard.div(guard.mul(lo.cell_scales[a.lower_index[s.occurrence]],
                a.lattice_coefficient[s.occurrence]),a.cell_scales[a.upper_index[s.occurrence]]);
            if(coefficient!=0) insert(variable_,s.occurrence,s.row,s.column,parameter,guard.mul(coefficient,gain));
        };
        if(model_->parameter_basis()) {
            for(const auto& [key,q]:model_->parameter_basis()->entries())variable(key.first,key.second,q);
        } else for(std::size_t i=0;i<model_->variable_coordinates().size();++i)variable(i,i,Q{1});
        for(std::size_t p=0;p<a.incidence_count();++p) {
            guard.step();
            for(const auto& [key,q]:model_->owner().fibers().transport_override(grade,p)->entries())
                insert(fixed_,p,key.first,key.second,0,q);
        }
    }
    const PreparedTransportObservations& model() const & {return *model_;}
    const PreparedTransportObservations& model() const &&=delete;
    std::size_t retained_entries() const noexcept {return retained_;}
    TransportColumnObservation observation(std::vector<TransportColumnField> input,
                                           std::vector<TransportColumnField> target) const {
        return {model_->owner(),model_->grade(),std::move(input),std::move(target)};
    }
    std::size_t observation_entries(const TransportColumnObservation& o) const {
        std::size_t n{};
        for(const auto* fields:{&o.input,&o.target}) for(const auto& field:*fields)
            n=machine::checked_size_add(n,machine::checked_size_add(2,
                machine::checked_size_mul(field.tensor.stored_nonzeros(),2)));
        return n;
    }
    TransportObservationSystem assemble(const std::vector<TransportColumnObservation>& observations,Guard& guard) const {
        guard.dimension(observations.size());std::size_t rows{},inputs{};
        const auto lower=core::Grade{static_cast<std::uint16_t>(model_->grade().value-1)};
        for(const auto& o:observations) {
            guard.step();
            if(!o.owner.same_owner(model_->owner()) || o.grade!=model_->grade())
                throw std::invalid_argument("column observation belongs to a different original fiber owner");
            fields(o.input,model_->grade(),guard);fields(o.target,lower,guard);
            if(o.target.empty())throw std::invalid_argument("nonempty observed original stalks required");
            for(const auto& field:o.target)rows=machine::checked_size_add(rows,field.tensor.domain().logical_size());
            inputs=machine::checked_size_add(inputs,observation_entries(o));
        }
        guard.dimension(rows);const auto n=model_->parameter_count();
        TransportObservationSystem out{M(rows,n),M(rows,machine::checked_size_add(n,1)),{}, {}};
        const auto admit=[&](std::size_t scratch=0){guard.entries(machine::checked_size_add(retained_,
            machine::checked_size_add(inputs,machine::checked_size_add(machine::checked_size_mul(rows,4),
                machine::checked_size_add(scratch,machine::checked_size_add(out.design.nonzeros(),out.augmented.nonzeros()))))));};
        admit();out.target.resize(rows);out.constraints.reserve(rows);std::size_t row{};
        for(std::size_t sample=0;sample<observations.size();++sample) {
            const auto& o=observations[sample];std::map<std::size_t,std::size_t> selected;
            for(const auto& field:o.target) {
                guard.step();selected.emplace(field.cell,row);admit(machine::checked_size_mul(selected.size(),2));
                for(std::size_t axis=0;axis<field.tensor.domain().logical_size();++axis) {
                    guard.step();out.constraints.push_back({sample,field.cell,axis});++row;
                }
                for(const auto& e:field.tensor.values().entries())out.target[selected.at(field.cell)+e.index]=e.value;
            }
            for(const auto& field:o.input) for(const auto& e:field.tensor.values().entries()) {
                guard.step();
                for(const bool fixed:{false,true}) {
                    const auto& index=fixed?fixed_:variable_;const auto found=index.find({field.cell,e.index});
                    if(found==index.end())continue;
                    for(const auto& term:found->second) {
                        guard.step();const auto target=selected.find(term.cell);if(target==selected.end())continue;
                        const auto r=target->second+term.axis;const auto q=guard.mul(term.coefficient,e.value);
                        if(fixed)out.target[r]=guard.sub(out.target[r],q);
                        else out.design.set(r,term.parameter,guard.add(out.design.get(r,term.parameter),q));
                        admit(machine::checked_size_mul(selected.size(),2));
                    }
                }
            }
        }
        for(const auto& [key,q]:out.design.entries()){guard.step();out.augmented.set(key.first,key.second,q);admit();}
        for(std::size_t i=0;i<rows;++i){guard.step();out.augmented.set(i,n,out.target[i]);admit();}
        return out;
    }
    TransportObservation materialize(const TransportColumnObservation& o,Guard& guard) const {
        // Compatibility for consumers that explicitly request whole fields.
        if(!o.owner.same_owner(model_->owner()) || o.grade!=model_->grade())
            throw std::invalid_argument("column observation belongs to a different original fiber owner");
        fields(o.input,model_->grade(),guard);
        const auto lower=core::Grade{static_cast<std::uint16_t>(model_->grade().value-1)};
        fields(o.target,lower,guard);
        if(o.target.empty())throw std::invalid_argument("nonempty observed original stalks required");
        std::vector<std::size_t> lo{0},hi{0};const auto& fibers=model_->owner().fibers();
        for(const bool upper:{false,true}) {
            auto& offsets=upper?hi:lo;const auto grade=upper?model_->grade():lower;
            for(std::size_t cell=0;cell<fibers.complex().grade(grade).cell_count();++cell) {
                guard.step();offsets.push_back(machine::checked_size_add(offsets.back(),
                    fibers.stalks().dimension({grade,static_cast<std::uint32_t>(cell)})));guard.dimension(offsets.back());
            }
        }
        guard.entries(machine::checked_size_add(retained_,machine::checked_size_add(hi.back(),
            machine::checked_size_add(lo.back(),lo.size()-1))));
        TransportObservation out{numeric::RationalVector(hi.back()),numeric::RationalVector(lo.back()),
            std::vector<std::size_t>(lo.size()-1)};
        for(const auto& field:o.input)for(const auto& e:field.tensor.values().entries())out.input[hi[field.cell]+e.index]=e.value;
        for(const auto& field:o.target) {
            out.observed[field.cell]=1;
            for(const auto& e:field.tensor.values().entries())out.target[lo[field.cell]+e.index]=e.value;
        }
        return out;
    }
};
} // namespace rfc::learning
