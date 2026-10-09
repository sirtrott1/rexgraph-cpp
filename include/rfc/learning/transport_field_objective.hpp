#pragma once
#include "rfc/field/relational_channels.hpp"
#include "rfc/learning/transport_observations.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"

namespace rfc::learning {

// Each term reads a named lower stalk axis on an original occurrence. The
// resulting scalar belongs to that occurrence's original upper relation.
struct TransportFieldViewTerm {
    std::size_t occurrence{}, axis{};
    numeric::Rational coefficient;
};
struct TransportFieldView {
    std::string name;
    std::vector<TransportFieldViewTerm> terms;
};
struct TransportObjectiveField {
    numeric::RationalVector origin;
    numeric::ExactMatrix directions;
};
struct TransportObjectiveSpecification {
    numeric::Rational mass{0};
    std::array<numeric::Rational,4> channels{};
    bool trace_normalized{};
};

// Minimize a declared positive semidefinite field action within the complete
// evidence family. Both families retain their original parameter coordinates.
class PreparedTransportFieldObjective {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    PreparedTransportObservations observations_;
    V evidence_origin_, origin_, coordinate_origin_;
    M evidence_kernel_, kernel_, coordinate_kernel_, hessian_;
    V target_;
    std::vector<TransportFieldView> views_;
    std::vector<V> probes_;
    std::vector<TransportObjectiveField> fields_;
    std::vector<M> view_maps_;
    std::shared_ptr<const field::PreparedRelationalChannels> channels_;
    TransportObjectiveSpecification specification_;
    std::vector<std::size_t> free_columns_;
    Q initial_energy_{0}, minimum_energy_{0};
    std::size_t rank_{}, retained_{}, preparation_steps_{};

    V action(const V& x,Guard& guard) const {
        auto out=guard.canonical_input(x);
        for(auto& value:out) value=guard.mul(specification_.mass,value);
        for(std::size_t k=0;k<4;++k) {
            if(specification_.channels[k]==0) continue;
            const auto y=channels_->apply(static_cast<field::RelationalChannel>(k),x,
                specification_.trace_normalized,[&guard]{guard.step();});
            for(std::size_t i=0;i<out.size();++i)
                out[i]=guard.add(out[i],guard.mul(specification_.channels[k],y[i]));
        }
        return out;
    }
    static Q dot(const V& x,const V& y,Guard& guard) {
        if(x.size()!=y.size()) throw std::logic_error("field objective pairing dimensions");
        Q out{0};for(std::size_t i=0;i<x.size();++i) out=guard.add(out,guard.mul(x[i],y[i]));
        return out;
    }
    V shifted(const TransportObjectiveField& field,const V& t,Guard& guard) const {
        auto x=numeric::checked_apply(field.directions,t,guard);
        for(std::size_t i=0;i<x.size();++i) x[i]=guard.add(x[i],field.origin[i]);
        return x;
    }
public:
    PreparedTransportFieldObjective(PreparedTransportObservations observations,V evidence_origin,M evidence_kernel,
        std::vector<V> probes,std::vector<TransportFieldView> views,V weights,
        field::CoparticipationReading reading,TransportObjectiveSpecification specification,Guard& guard)
        : observations_(std::move(observations)),evidence_origin_(guard.canonical_input(evidence_origin)),
          evidence_kernel_(std::move(evidence_kernel)),views_(std::move(views)),
          specification_(std::move(specification)) {
        const auto start=guard.steps();guard.matrix(evidence_kernel_);
        const auto n=observations_.parameter_count(),d=evidence_kernel_.cols();
        if(evidence_origin_.size()!=n || evidence_kernel_.rows()!=n)
            throw std::invalid_argument("complete evidence parameter family required");
        guard.dimension(machine::checked_size_add(d,1));
        specification_.mass=numeric::canonical_rational(specification_.mass);guard.value(specification_.mass);
        if(specification_.mass<0) throw std::invalid_argument("nonnegative field mass required");
        for(auto& q:specification_.channels) {
            q=numeric::canonical_rational(q);
            guard.value(q);if(q<0) throw std::invalid_argument("nonnegative channel coefficients required");
        }
        if(views_.empty() || probes.empty()) throw std::invalid_argument("declared field views and probes required");
        guard.dimension(views_.size());guard.dimension(probes.size());
        const auto pin=std::make_shared<const local::OwnedLinearFiberSystem>(observations_.owner());
        const auto& source=pin->fibers().complex();const auto& arena=source.grade(core::Grade{1});
        const auto owner=std::shared_ptr<const core::RelationalComplex>(pin,&source);
        channels_=std::make_shared<const field::PreparedRelationalChannels>(owner,std::move(weights),reading,
            guard.limits(),[&guard]{guard.step();});
        retained_=machine::checked_size_add(observations_.retained_entries(),channels_->retained_entries());
        retained_=machine::checked_size_add(retained_,machine::checked_size_add(n,evidence_kernel_.nonzeros()));
        std::vector<std::size_t> offsets{0};
        for(std::size_t p=0;p<arena.incidence_count();++p) {
            guard.step();offsets.push_back(machine::checked_size_add(offsets.back(),
                pin->fibers().stalks().dimension({core::Grade{0},arena.lower_index[p]})));
        }
        std::set<std::string> names;
        for(auto& view:views_) {
            guard.step();
            if(view.name.empty() || view.name.size()>256 || !names.insert(view.name).second || view.terms.empty())
                throw std::invalid_argument("distinct bounded names and nonempty field view support required");
            M map(channels_->size(),offsets.back());std::set<std::pair<std::size_t,std::size_t>> support;
            for(auto& term:view.terms) {
                guard.step();term.coefficient=numeric::canonical_rational(term.coefficient);guard.value(term.coefficient);
                if(term.occurrence>=arena.incidence_count() ||
                    term.axis>=offsets[term.occurrence+1]-offsets[term.occurrence] ||
                    !support.emplace(term.occurrence,term.axis).second)
                    throw std::invalid_argument("distinct original occurrence stalk directions required");
                map.set(arena.upper_index[term.occurrence],offsets[term.occurrence]+term.axis,term.coefficient);
                retained_=machine::checked_size_add(retained_,3);guard.entries(retained_);
            }
            retained_=machine::checked_size_add(retained_,map.nonzeros());guard.entries(retained_);
            view_maps_.push_back(std::move(map));
        }
        hessian_=M(d,d);target_.resize(d);coordinate_origin_.resize(d);
        for(const auto& input:probes) {
            auto probe=guard.canonical_input(input);
            const auto fixed=observations_.fixed_response(probe,guard);
            const auto local=observations_.observation_map(probe,false,guard);
            auto value=numeric::checked_apply(local,evidence_origin_,guard);
            for(std::size_t i=0;i<value.size();++i) value[i]=guard.add(value[i],fixed.occurrences[i]);
            const auto tangent=numeric::checked_multiply(local,evidence_kernel_,guard);
            retained_=machine::checked_size_add(retained_,probe.size());guard.entries(retained_);
            probes_.push_back(std::move(probe));
            for(const auto& map:view_maps_) {
                auto a=numeric::checked_apply(map,value,guard);
                auto h=numeric::checked_multiply(map,tangent,guard);
                retained_=machine::checked_size_add(retained_,machine::checked_size_add(a.size(),h.nonzeros()));
                guard.entries(retained_);
                const auto qa=action(a,guard),linear=numeric::checked_transpose_apply(h,qa,guard);
                initial_energy_=guard.add(initial_energy_,guard.div(dot(a,qa,guard),Q{2}));
                for(std::size_t i=0;i<d;++i) target_[i]=guard.sub(target_[i],linear[i]);
                // The reduced map can fill. It is a bounded parameter pullback,
                // not a new adjacency or a replacement for the source complex.
                std::map<std::size_t,V> columns;
                for(const auto& [key,q]:h.entries()) {
                    guard.step();
                    if(!columns.contains(key.second)) {
                        guard.entries(machine::checked_size_add(retained_,machine::checked_size_add(hessian_.nonzeros(),
                            machine::checked_size_mul(columns.size()+1,channels_->size()))));
                        columns.emplace(key.second,V(channels_->size()));
                    }
                    columns.at(key.second)[key.first]=q;
                }
                for(const auto& [j,column]:columns) {
                    const auto image=numeric::checked_transpose_apply(h,action(column,guard),guard);
                    for(std::size_t i=0;i<d;++i) {
                        hessian_.set(i,j,guard.add(hessian_.get(i,j),image[i]));
                        guard.entries(machine::checked_size_add(retained_,hessian_.nonzeros()));
                    }
                }
                fields_.push_back({std::move(a),std::move(h)});
            }
        }
        if(hessian_!=hessian_.transpose()) throw std::logic_error("field action pullback is not symmetric");
        M augmented(d,d+1);
        for(const auto& [key,q]:hessian_.entries()) augmented.set(key.first,key.second,q);
        for(std::size_t i=0;i<d;++i) augmented.set(i,d,target_[i]);
        const numeric::PreparedExactFrames frames(hessian_,guard),joint(augmented,guard);
        if(frames.rank()!=joint.rank()) throw std::logic_error("field action has no compatible stationary family");
        rank_=frames.rank();coordinate_kernel_=frames.kernel();free_columns_=frames.free_columns();
        for(std::size_t i=0;i<joint.rank();++i) coordinate_origin_[joint.pivots()[i]]=joint.reduced().get(i,d);
        if(numeric::checked_apply(hessian_,coordinate_origin_,guard)!=target_ ||
            numeric::checked_multiply(hessian_,coordinate_kernel_,guard).nonzeros())
            throw std::logic_error("field objective stationary certificate failed");
        origin_=numeric::checked_apply(evidence_kernel_,coordinate_origin_,guard);
        for(std::size_t i=0;i<n;++i) origin_[i]=guard.add(origin_[i],evidence_origin_[i]);
        kernel_=numeric::checked_multiply(evidence_kernel_,coordinate_kernel_,guard);
        for(const auto& field:fields_) {
            const auto x=shifted(field,coordinate_origin_,guard);
            minimum_energy_=guard.add(minimum_energy_,guard.div(dot(x,action(x,guard),guard),Q{2}));
        }
        if(minimum_energy_<0 || minimum_energy_>initial_energy_)
            throw std::logic_error("field objective energy certificate failed");
        for(const auto size:{origin_.size(),coordinate_origin_.size(),target_.size(),kernel_.nonzeros(),
                coordinate_kernel_.nonzeros(),hessian_.nonzeros(),frames.stored_entries(),joint.stored_entries()})
            retained_=machine::checked_size_add(retained_,size);
        guard.entries(retained_);preparation_steps_=guard.steps()-start;
    }
    const PreparedTransportObservations& observations() const & {return observations_;}
    const V& evidence_origin() const & {return evidence_origin_;}
    const M& evidence_kernel() const & {return evidence_kernel_;}
    const V& origin() const & {return origin_;}
    const M& kernel() const & {return kernel_;}
    const V& coordinate_origin() const & {return coordinate_origin_;}
    const M& coordinate_kernel() const & {return coordinate_kernel_;}
    const M& hessian() const & {return hessian_;}
    const V& target() const & {return target_;}
    const std::vector<std::size_t>& free_columns() const & {return free_columns_;}
    const std::vector<TransportObjectiveField>& fields() const & {return fields_;}
    const std::vector<TransportFieldView>& views() const & {return views_;}
    const std::vector<V>& probes() const & {return probes_;}
    const TransportObjectiveSpecification& specification() const & {return specification_;}
    const field::PreparedRelationalChannels& channel_plan() const & {return *channels_;}
    const PreparedTransportObservations& observations() const &&=delete;
    const V& evidence_origin() const &&=delete;
    const M& evidence_kernel() const &&=delete;
    const V& origin() const &&=delete;
    const M& kernel() const &&=delete;
    const V& coordinate_origin() const &&=delete;
    const M& coordinate_kernel() const &&=delete;
    const M& hessian() const &&=delete;
    const V& target() const &&=delete;
    const std::vector<std::size_t>& free_columns() const &&=delete;
    const std::vector<TransportObjectiveField>& fields() const &&=delete;
    const std::vector<TransportFieldView>& views() const &&=delete;
    const std::vector<V>& probes() const &&=delete;
    const TransportObjectiveSpecification& specification() const &&=delete;
    const field::PreparedRelationalChannels& channel_plan() const &&=delete;
    Q initial_energy() const {return initial_energy_;}
    Q minimum_energy() const {return minimum_energy_;}
    std::size_t rank() const noexcept {return rank_;}
    std::size_t retained_entries() const noexcept {return retained_;}
    std::size_t preparation_steps() const noexcept {return preparation_steps_;}
};
} // namespace rfc::learning
