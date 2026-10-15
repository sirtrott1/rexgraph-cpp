#pragma once
#include "rfc/learning/context_transport_hypotheses.hpp"
#include "rfc/learning/context_observation_fields.hpp"

namespace rfc::learning {
// Accession and reconstruction into an explicitly declared common carrier.
// R A = I retains every original direction; R need not be the transpose of A.
struct TransportContextView {
    std::uint64_t frame{};
    numeric::ExactMatrix accession, reconstruction;
};
struct TransportContextViewWitness {
    numeric::ExactMatrix common_input, origin_input, target_input;
    std::vector<std::pair<std::size_t,std::size_t>> response_shapes;
    std::vector<std::array<std::size_t,3>> equation_coordinates;
};
struct TransportContextViewAcquisition {
    TransportContextAcquisition acquisition;
    std::vector<TransportContextViewWitness> witnesses;
};
enum class TransportContextInputScope { FullCarrier, ObservedFields };
namespace context_view_detail {
using M=numeric::ExactMatrix;
using Q=numeric::Rational;
using Guard=numeric::ExactActionGuard;
using algebra::owned_gluing_detail::add;
using algebra::owned_gluing_detail::mul;

// Sparse products account for the retained atlas and concurrent row index.
inline M product(const M& a,const M& b,Guard& guard,std::size_t resident) {
    if(a.cols()!=b.rows()) throw std::logic_error("context view product dimensions");
    guard.matrix(a);guard.matrix(b);
    // A common frame may have a large declared carrier and only a few
    // represented directions. Index those directions without one row bucket
    // per ambient coordinate. Keep original row identities in the sparse map.
    std::map<std::size_t,std::vector<std::pair<std::size_t,Q>>> rows;
    std::size_t scratch=resident;guard.entries(scratch);
    for(const auto& [key,q]:b.entries()) {
        guard.step();const bool new_row=!rows.contains(key.first);
        const auto next=add(scratch,new_row?3u:2u);guard.entries(next);
        rows[key.first].emplace_back(key.second,q);scratch=next;
    }
    M out(a.rows(),b.cols());
    for(const auto& [key,q]:a.entries()) {
        guard.step();const auto row=rows.find(key.second);if(row==rows.end())continue;
        for(const auto& [j,v]:row->second) {
            guard.entries(add(scratch,add(out.nonzeros(),1)));
            out.set(key.first,j,guard.add(out.get(key.first,j),guard.mul(q,v)));
        }
    }
    return out;
}
inline M slice(const M& a,std::size_t begin,std::size_t count,Guard& guard) {
    M out(count,a.cols());
    for(const auto& [key,q]:a.entries()) {guard.step();if(key.first>=begin && key.first-begin<count)
        out.set(key.first-begin,key.second,q);}
    return out;
}
inline TransportContextViewWitness intersection(const TransportContextView& a,const TransportContextView& b,
    Guard& guard,std::size_t resident) {
    const auto n=a.accession.cols(),m=b.accession.cols();guard.dimension(add(n,m));
    // Equal maps in one declared carrier retain the original coordinate order.
    if(a.frame && a.frame==b.frame && a.accession==b.accession) {
        guard.matrix(a.accession);
        guard.entries(add(resident,add(a.accession.nonzeros(),mul(n,2))));
        auto identity=M::identity(n);
        const auto live=add(resident,add(a.accession.nonzeros(),identity.nonzeros()));
        if(product(a.reconstruction,a.accession,guard,live)!=identity ||
            product(b.reconstruction,b.accession,guard,live)!=identity)
            throw std::logic_error("context common input reconstruction certificate");
        return {a.accession,identity,std::move(identity),{}, {}};
    }
    M joined(a.accession.rows(),add(n,m));
    for(const auto& [key,q]:a.accession.entries()) {guard.step();joined.set(key.first,key.second,q);}
    for(const auto& [key,q]:b.accession.entries()) joined.set(key.first,add(n,key.second),guard.sub(0,q));
    guard.entries(add(resident,joined.nonzeros()));
    const numeric::PreparedExactFrames frames(joined,guard);
    const auto live=add(resident,add(joined.nonzeros(),frames.stored_entries()));guard.entries(live);
    const auto left=slice(frames.kernel(),0,n,guard),right=slice(frames.kernel(),n,m,guard);
    TransportContextViewWitness out;
    out.common_input=product(a.accession,left,guard,add(live,add(left.nonzeros(),right.nonzeros())));
    out.origin_input=product(a.reconstruction,out.common_input,guard,add(live,out.common_input.nonzeros()));
    out.target_input=product(b.reconstruction,out.common_input,guard,add(live,
        add(out.common_input.nonzeros(),out.origin_input.nonzeros())));
    const auto retained=add(live,add(out.common_input.nonzeros(),add(out.origin_input.nonzeros(),out.target_input.nonzeros())));
    if(out.origin_input!=left || out.target_input!=right ||
        product(b.accession,out.target_input,guard,retained)!=out.common_input)
        throw std::logic_error("context common input reconstruction certificate");
    return out;
}
inline TransportContextViewWitness intersection_inputs(const TransportContextView& a,const TransportContextView& b,
    const M& observed_origin,const M* observed_target,Guard& guard,std::size_t resident) {
    const auto u=product(a.accession,observed_origin,guard,resident);
    const auto v=observed_target ? product(b.accession,*observed_target,guard,add(resident,u.nonzeros())) : b.accession;
    const auto n=u.cols(),m=v.cols();guard.dimension(add(n,m));
    M joined(u.rows(),add(n,m));
    for(const auto& [key,q]:u.entries()) {guard.step();joined.set(key.first,key.second,q);}
    for(const auto& [key,q]:v.entries()) {guard.step();joined.set(key.first,add(n,key.second),guard.sub(0,q));}
    const auto inputs_live=add(resident,add(u.nonzeros(),v.nonzeros()));guard.entries(add(inputs_live,joined.nonzeros()));
    const numeric::PreparedExactFrames frames(joined,guard);
    const auto left=slice(frames.kernel(),0,n,guard),right=slice(frames.kernel(),n,m,guard);
    const auto live=add(inputs_live,add(joined.nonzeros(),add(frames.stored_entries(),add(left.nonzeros(),right.nonzeros()))));
    TransportContextViewWitness out;out.common_input=product(u,left,guard,live);
    out.origin_input=product(a.reconstruction,out.common_input,guard,add(live,out.common_input.nonzeros()));
    out.target_input=product(b.reconstruction,out.common_input,guard,add(live,add(out.common_input.nonzeros(),out.origin_input.nonzeros())));
    const auto retained=add(live,add(out.common_input.nonzeros(),add(out.origin_input.nonzeros(),out.target_input.nonzeros())));
    if(out.origin_input!=product(observed_origin,left,guard,retained) ||
        out.target_input!=(observed_target ? product(*observed_target,right,guard,retained) : right) ||
        product(b.accession,out.target_input,guard,retained)!=out.common_input)
        throw std::logic_error("observed context input reconstruction certificate");
    return out;
}
} // namespace context_view_detail

// Compare full retained stars only on their actual shared represented inputs.
// Unshared inputs and all original parameter directions remain in the evidence.
inline TransportContextViewAcquisition acquire_context_transport_views(
    const TransportContextSamples& samples,
    const std::vector<std::size_t>& targets,const std::vector<std::vector<TransportContextView>>& atlas,
    numeric::ExactActionGuard& guard,TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
    using namespace context_view_detail;
    const auto& observations=samples.model();
    observations.require_grade_one_profile();
    const auto start=guard.steps();guard.step();
    if(scope!=TransportContextInputScope::FullCarrier && scope!=TransportContextInputScope::ObservedFields)
        throw std::invalid_argument("declared context input scope required");
    const auto& fibers=observations.owner().fibers();const auto& source=fibers.complex();
    const core::Grade lo{0},up{1};const auto& arena=source.grade(up);
    TransportContextViewAcquisition result;auto& out=result.acquisition;
    out.retained=add(observations.retained_entries(),algebra::owned_gluing_detail::words(source));
    const auto admit=[&](std::size_t live=0){guard.entries(add(out.retained,live));};admit();
    if(atlas.size()!=2 || atlas[0].size()!=source.grade(lo).cell_count() || atlas[1].size()!=arena.cell_count())
        throw std::invalid_argument("context view atlas must cover both original grades");
    std::map<std::uint64_t,std::size_t> dimensions;
    for(std::size_t k=0;k<2;++k) for(std::size_t i=0;i<atlas[k].size();++i) {
        guard.step();const auto& chart=atlas[k][i];const auto& a=chart.accession;const auto& r=chart.reconstruction;
        guard.matrix(a);guard.matrix(r);out.retained=add(out.retained,add(5,add(a.nonzeros(),r.nonzeros())));admit();
        if(!chart.frame) {
            if(a.rows() || a.cols() || r.rows() || r.cols()) throw std::invalid_argument("absent context view has maps");
            continue;
        }
        const auto n=fibers.stalks().dimension({core::Grade{static_cast<std::uint16_t>(k)},static_cast<std::uint32_t>(i)});
        if(a.cols()!=n || r.rows()!=n || a.rows()!=r.cols()) throw std::invalid_argument("original context view dimensions");
        const auto [it,inserted]=dimensions.emplace(chart.frame,a.rows());
        if(!inserted && it->second!=a.rows()) throw std::invalid_argument("declared common carrier dimensions conflict");
        if(product(r,a,guard,out.retained)!=M::identity(n)) throw std::invalid_argument("context reconstruction must retain every original direction");
    }
    std::set<std::size_t> distinct;
    for(const auto row:targets) {guard.step();if(row>=arena.cell_count() || !distinct.insert(row).second)
        throw std::invalid_argument("distinct original target rows required");}
    out.retained=add(out.retained,samples.retained_entries());admit();
    std::vector<std::vector<std::size_t>> evidence(arena.cell_count());
    for(std::size_t sample=0;sample<samples.size();++sample) {
        guard.step();
        for(const auto row:samples.active_inputs(sample,guard)) {
            bool observed=false;const auto [begin,end]=arena.incidence_range(row);
            for(auto p=begin;p<end;++p) {guard.step();observed=observed ||
                (samples.observed(sample,arena.lower_index[p],guard) &&
                 arena.aspects.get(source.signature().existence_aspect(),p) &&
                 algebra::owned_gluing_detail::coefficient(source,up,p,guard)!=0);}
            if(observed) {evidence[row].push_back(sample);out.retained=add(out.retained,1);admit();}
        }
    }
    for(std::size_t row=0;row<evidence.size();++row) if(!evidence[row].empty()) out.seed_rows.push_back(row);
    std::map<std::size_t,M> observed_images;
    if(scope==TransportContextInputScope::ObservedFields) for(const auto row:out.seed_rows) {
        guard.step();const auto n=fibers.stalks().dimension({up,static_cast<std::uint32_t>(row)});
        M input(n,evidence[row].size());guard.dimension(evidence[row].size());
        for(std::size_t j=0;j<evidence[row].size();++j) {
            const auto field=samples.input(evidence[row][j],row,guard);admit(add(input.nonzeros(),field.size()));
            for(std::size_t i=0;i<n;++i) {guard.step();input.set(i,j,field[i]);admit(add(input.nonzeros(),field.size()));}
        }
        const numeric::PreparedExactFrames frames(input,guard);admit(add(input.nonzeros(),frames.stored_entries()));
        auto image=frames.image();out.retained=add(out.retained,add(2,image.nonzeros()));admit();
        observed_images.emplace(row,std::move(image));
    }
    guard.dimension(out.seed_rows.size());
    const context_transport_detail::TargetIndex index(source,targets,atlas,guard,out.retained);
    out.retained=add(out.retained,index.entries());admit();
    std::vector<std::vector<std::size_t>> variables(arena.incidence_count()),constants(arena.incidence_count());
    for(std::size_t i=0;i<observations.variable_coordinates().size();++i) {
        guard.step();variables[observations.variable_coordinates()[i].occurrence].push_back(i);
    }
    for(std::size_t i=0;i<observations.fixed_coordinates().size();++i) {
        guard.step();constants[observations.fixed_coordinates()[i].occurrence].push_back(i);
    }
    out.retained=add(out.retained,add(mul(arena.incidence_count(),2),
        add(observations.variable_coordinates().size(),observations.fixed_coordinates().size())));admit();
    const auto same=[&](std::size_t k,std::size_t a,std::size_t b) {guard.step();return atlas[k][a].frame && atlas[k][a].frame==atlas[k][b].frame;};
    for(const auto origin:out.seed_rows) for(const auto target:index.targets(source,origin,atlas[1][origin].frame,out,guard)) {
        if(origin==target)continue;
        guard.step();TransportContextHypothesis h;h.origin=origin;h.target=target;
        std::string failure;
        failure=context_transport_detail::star(source,h,guard);
        if(failure.empty()) for(const auto& [a,b]:h.cells) if(!same(0,a,b)) {failure="undeclared-lower-frame";break;}
        if(!failure.empty()) {++out.rejections[failure];continue;}
        const auto target_image=observed_images.find(target);
        auto witness=scope==TransportContextInputScope::FullCarrier ?
            intersection(atlas[1][origin],atlas[1][target],guard,out.retained) :
            intersection_inputs(atlas[1][origin],atlas[1][target],observed_images.at(origin),
                target_image==observed_images.end()?nullptr:&target_image->second,guard,out.retained);
        if(!witness.common_input.cols()) {++out.rejections["empty-input-intersection"];continue;}
        const auto witness_entries=add(witness.common_input.nonzeros(),add(witness.origin_input.nonzeros(),witness.target_input.nonzeros()));
        admit(witness_entries);
        std::vector<TransportParameterRelation> equations;
        for(const auto& [p,q]:h.occurrences) {
            const auto a=arena.lower_index[p],b=arena.lower_index[q];
            using Key=std::pair<std::size_t,std::size_t>;
            struct Equation {std::map<std::size_t,Q> terms;Q fixed;};
            std::map<Key,Equation> rows;std::size_t support{};
            const auto accumulate=[&](std::size_t occurrence,const M& accession,const M& input,int sign) {
                const auto coefficient=algebra::owned_gluing_detail::coefficient(source,up,occurrence,guard);
                const auto index_entries=add(add(accession.cols(),input.rows()),add(accession.nonzeros(),input.nonzeros()));
                admit(add(witness_entries,index_entries));
                std::vector<std::vector<std::pair<std::size_t,Q>>> columns(accession.cols()),inputs(input.rows());
                for(const auto& [key,value]:accession.entries()) {guard.step();columns[key.second].emplace_back(key.first,value);}
                for(const auto& [key,value]:input.entries()) {guard.step();inputs[key.first].emplace_back(key.second,value);}
                const auto add_entry=[&](std::size_t i,std::size_t j,const Q& value,std::optional<std::size_t> entry) {
                    for(const auto& [r,aq]:columns[i]) {
                        guard.step();for(const auto& [c,uq]:inputs[j]) {
                            guard.step();const Key key{r,c};auto& row=rows[key];
                            const auto gain=guard.mul(Q{sign},guard.mul(aq,uq));
                            if(entry) {if(!row.terms.contains(*entry)) ++support;
                                row.terms[*entry]=guard.add(row.terms[*entry],guard.mul(coefficient,gain));}
                            else row.fixed=guard.add(row.fixed,guard.mul(gain,value));
                            admit(add(witness_entries,add(index_entries,add(mul(rows.size(),4),mul(support,2)))));
                        }
                    }
                };
                for(const auto e:variables[occurrence]) {
                    guard.step();const auto& c=observations.variable_coordinates()[e];
                    add_entry(c.row,c.column,Q{0},e);
                }
                const auto& fixed=fibers.transport_override(up,occurrence);
                for(const auto e:constants[occurrence]) {
                    guard.step();const auto& c=observations.fixed_coordinates()[e];
                    add_entry(c.row,c.column,fixed->get(c.row,c.column),std::nullopt);
                }
            };
            accumulate(p,atlas[0][a].accession,witness.origin_input,1);
            accumulate(q,atlas[0][b].accession,witness.target_input,-1);
            witness.response_shapes.emplace_back(atlas[0][a].accession.rows(),witness.common_input.cols());
            for(auto& [key,row]:rows) {
                TransportParameterRelation relation;
                relation.name="view:"+std::to_string(origin)+":"+std::to_string(target)+":"+
                    std::to_string(p)+":"+std::to_string(key.first)+":"+std::to_string(key.second);
                relation.expected=guard.sub(0,row.fixed);
                for(const auto& [entry,gain]:row.terms) relation.terms.push_back({entry,gain});
                witness.equation_coordinates.push_back({p,key.first,key.second});equations.push_back(std::move(relation));
                guard.dimension(add(out.relations.size(),equations.size()));
                admit(add(witness_entries,add(mul(rows.size(),4),add(mul(support,2),mul(equations.size(),128)))));
            }
        }
        if(equations.empty()) {++out.rejections["no-map-support-on-common-input"];continue;}
        h.evidence_samples=evidence[origin];
        out.retained=add(out.retained,add(witness_entries,add(8,add(mul(h.cells.size(),2),
            add(mul(h.occurrences.size(),4),add(h.evidence_samples.size(),mul(equations.size(),3)))))));
        for(auto& equation:equations) {
            guard.step();h.equations.push_back(out.relations.size());
            out.retained=add(out.retained,add(equation.name.size(),add(2,mul(equation.terms.size(),2))));
            out.relations.push_back(std::move(equation));admit();
        }
        out.hypotheses.push_back(std::move(h));result.witnesses.push_back(std::move(witness));admit();
    }
    out.steps=guard.steps()-start;return result;
}
inline TransportContextViewAcquisition acquire_context_transport_views(
    const PreparedTransportObservations& observations,const std::vector<TransportObservation>& samples,
    const std::vector<std::size_t>& targets,const std::vector<std::vector<TransportContextView>>& atlas,
    numeric::ExactActionGuard& guard,TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
    return acquire_context_transport_views(TransportContextSamples(observations,samples,guard),targets,atlas,guard,scope);
}
inline TransportContextViewAcquisition acquire_context_transport_views(
    const PreparedTransportEvidence& evidence,const std::vector<std::size_t>& targets,
    const std::vector<std::vector<TransportContextView>>& atlas,numeric::ExactActionGuard& guard,
    TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
    return acquire_context_transport_views(TransportContextSamples(evidence),targets,atlas,guard,scope);
}
} // namespace rfc::learning
