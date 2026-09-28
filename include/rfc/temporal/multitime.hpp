#pragma once
#include "rfc/field/exact_metric.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rfc::temporal {

struct MultiTimeAccumulation {
    numeric::RationalVector transported_initial;
    std::vector<numeric::RationalVector> propagated_innovations;
    numeric::RationalVector terminal;
};

// Exact finite identity
// x_T = J_{T<-0} x_0 + sum_t J_{T<-t+1} eta_t.
// Rectangular correspondences are supported; every term is accumulated only
// after it has been transported to the common terminal carrier.
inline MultiTimeAccumulation accumulate_innovations(
    const numeric::RationalVector& initial,
    const std::vector<numeric::ExactMatrix>& correspondences,
    const std::vector<numeric::RationalVector>& innovations) {
    if(correspondences.size()!=innovations.size()) {
        throw std::invalid_argument("multi-time innovation count mismatch");
    }
    if(correspondences.empty()) return {initial,{},initial};

    std::size_t current=initial.size();
    for(std::size_t t=0;t<correspondences.size();++t){
        if(correspondences[t].cols()!=current || innovations[t].size()!=correspondences[t].rows()) {
            throw std::invalid_argument("multi-time correspondence dimension mismatch");
        }
        current=correspondences[t].rows();
    }

    const std::size_t T=correspondences.size();
    std::vector<numeric::ExactMatrix> suffix(T+1u);
    suffix[T]=numeric::ExactMatrix::identity(current);
    for(std::size_t tt=T;tt-- > 0;){
        // suffix[tt] maps time tt to terminal time T.
        suffix[tt]=suffix[tt+1u].multiply(correspondences[tt]);
    }

    MultiTimeAccumulation out;
    out.transported_initial=suffix[0].apply(initial);
    out.terminal=out.transported_initial;
    out.propagated_innovations.reserve(T);
    for(std::size_t t=0;t<T;++t){
        auto value=suffix[t+1u].apply(innovations[t]);
        out.terminal=numeric::add(out.terminal,value);
        out.propagated_innovations.push_back(std::move(value));
    }
    return out;
}

struct MultiTimeChannel {
    std::string label;
    numeric::RationalVector target_field;
};

inline numeric::ExactMatrix multi_time_channel_gram(
    const std::vector<MultiTimeChannel>& channels,
    const field::ExactMetric& target_metric) {
    numeric::ExactMatrix out(channels.size(),channels.size());
    for(const auto& channel:channels) {
        if(channel.target_field.size()!=target_metric.size()) {
            throw std::invalid_argument("multi-time channel target dimension mismatch");
        }
    }
    for(std::size_t i=0;i<channels.size();++i){
        for(std::size_t j=i;j<channels.size();++j){
            const auto value=target_metric.inner(channels[i].target_field,channels[j].target_field);
            if(value!=0){ out.set(i,j,value); if(i!=j) out.set(j,i,value); }
        }
    }
    return out;
}

inline MultiTimeChannel realize_terminal_channel(std::string label,
                                                 const numeric::ExactMatrix& map,
                                                 const numeric::RationalVector& source) {
    return {std::move(label),map.apply(source)};
}

} // namespace rfc::temporal
