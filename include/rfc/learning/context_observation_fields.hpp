#pragma once
#include "rfc/learning/transport_evidence.hpp"

namespace rfc::learning {
// Context probes expose original stalks and observation membership. An owning
// evidence handle reads encoded columns without whole-field materialization.
class TransportContextSamples {
    using V=numeric::RationalVector;
    using Guard=numeric::ExactActionGuard;
    const PreparedTransportObservations* model_;
    const std::vector<TransportObservation>* dense_{};
    std::optional<PreparedTransportEvidence> evidence_;
    std::size_t retained_{};
public:
    TransportContextSamples(const PreparedTransportObservations& model,
        const std::vector<TransportObservation>& samples,Guard& guard)
        :model_(&model),dense_(&samples) {
        const auto& fibers=model.owner().fibers();
        const auto lower=core::Grade{static_cast<std::uint16_t>(model.grade().value-1)};
        for(const auto& sample:samples) {
            guard.step();guard.vector(sample.input);guard.vector(sample.target);
            if(sample.input.size()!=model.input_stalk_offsets().back() ||
                sample.target.size()!=fibers.stalks().total_dimension(lower) ||
                sample.observed.size()!=fibers.complex().grade(lower).cell_count() ||
                std::any_of(sample.observed.begin(),sample.observed.end(),[](auto bit){return bit>1;}))
                throw std::invalid_argument("complete Boolean contextual observation fields required");
            retained_=machine::checked_size_add(retained_,machine::checked_size_add(sample.input.size(),
                machine::checked_size_add(sample.target.size(),sample.observed.size())));
            guard.entries(machine::checked_size_add(model.retained_entries(),retained_));
        }
    }
    explicit TransportContextSamples(PreparedTransportEvidence evidence)
        :model_(&evidence.model()),evidence_(std::move(evidence)),retained_(evidence_->retained_entries()) {}
    const PreparedTransportObservations& model() const & {return *model_;}
    const PreparedTransportObservations& model() const &&=delete;
    const PreparedTransportEvidence* evidence() const noexcept {return evidence_?&*evidence_:nullptr;}
    const std::vector<TransportObservation>& dense() const & {
        if(!dense_)throw std::logic_error("context evidence has no whole-field observations");
        return *dense_;
    }
    const std::vector<TransportObservation>& dense() const &&=delete;
    std::size_t size() const noexcept {return evidence_?evidence_->observation_count():dense_->size();}
    std::size_t retained_entries() const noexcept {return retained_;}
    std::vector<std::size_t> active_inputs(std::size_t sample,Guard& guard) const {
        if(evidence_)return evidence_->active_input_stalks(sample,guard);
        const auto& field=dense_->at(sample).input;const auto& offsets=model_->input_stalk_offsets();
        std::vector<std::size_t> out;
        for(std::size_t cell=0;cell+1<offsets.size();++cell) {
            bool active=false;for(auto i=offsets[cell];i<offsets[cell+1];++i){guard.step();active=active || field[i]!=0;}
            if(active){out.push_back(cell);guard.entries(machine::checked_size_add(retained_,out.size()));}
        }
        return out;
    }
    V input(std::size_t sample,std::size_t cell,Guard& guard) const {
        if(evidence_)return evidence_->input_stalk(sample,cell,guard);
        const auto& field=dense_->at(sample).input;const auto& offsets=model_->input_stalk_offsets();
        if(cell+1>=offsets.size())throw std::invalid_argument("original context input stalk required");
        const auto n=offsets[cell+1]-offsets[cell];guard.dimension(n);
        guard.entries(machine::checked_size_add(retained_,n));V out(n);
        for(std::size_t i=0;i<n;++i){guard.step();out[i]=field[offsets[cell]+i];}
        return out;
    }
    bool observed(std::size_t sample,std::size_t cell,Guard& guard) const {
        if(evidence_)return evidence_->observed_stalk(sample,cell,guard);
        guard.step();return dense_->at(sample).observed.at(cell)!=0;
    }
};
} // namespace rfc::learning
