#pragma once
#include "rfc/learning/context_transport_pattern_bank_codec.hpp"

namespace rfc::learning {
struct TransportPatternScopeCoverage {
    std::vector<std::vector<std::size_t>> patterns;
};

// Original observed stars and their input images. Source and evidence digests
// refer to external archives; the retained columns do not authenticate them.
class PreparedTransportPatternScope {
    using G=numeric::ExactActionGuard;
    using M=numeric::ExactMatrix;
    using Q=numeric::Rational;
    struct State {
        storage::TemporalEndpoint endpoint;
        TransportPatternProvenance provenance;
        std::vector<M> inputs;
        std::size_t retained{};
        State(storage::TemporalEndpoint value,G& guard):endpoint(std::move(value)) {
            using namespace context_view_detail;
            guard.step();storage::validate_temporal_endpoint(endpoint);
            const auto& source=endpoint.source;
            if(endpoint.sequence || source.grade_count()<2 || endpoint.codes || endpoint.declarations ||
                !endpoint.local_blocks.empty() || !endpoint.finite_systems.empty())
                throw std::invalid_argument("original observed star scope required");
            for(std::size_t k=2;k<source.grade_count();++k)
                if(source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count())
                    throw std::invalid_argument("scope has a nonempty higher grade");
            const auto lower=source.grade(core::Grade{0}).cell_count();
            const auto count=source.grade(core::Grade{1}).cell_count();
            if(!count || endpoint.carriers.size()!=add(lower,count) || endpoint.tensors.size()!=add(count,1))
                throw std::invalid_argument("complete observed scope carriers and tensors required");
            const auto carriers=storage::temporal_endpoint_detail::carriers(endpoint);
            const auto tensors=storage::temporal_endpoint_detail::tensors(endpoint);
            std::map<std::uint64_t,std::size_t> dimensions;
            retained=add(algebra::owned_gluing_detail::words(source),mul(endpoint.carriers.size(),8));
            for(std::size_t i=0;i<add(lower,count);++i) {
                guard.step();if(!carriers.contains(i+1))throw std::invalid_argument("scope carrier missing");
                const auto& c=*carriers.at(i+1);
                const core::CellId cell{core::Grade{static_cast<std::uint16_t>(i<lower?0:1)},
                    static_cast<std::uint32_t>(i<lower?i:i-lower)};
                guard.dimension(c.domain.logical_size());
                if(c.kind!=storage::TemporalCarrierKind::CellStalk || c.domain.rank()!=1 ||
                    c.cell!=storage::stable_cell_key(source,cell) || !c.coordinate_system)
                    throw std::invalid_argument("original scope cell carrier required");
                const auto [it,inserted]=dimensions.emplace(c.coordinate_system,c.domain.logical_size());
                if(!inserted && it->second!=c.domain.logical_size())
                    throw std::invalid_argument("scope common frame dimensions conflict");
            }
            if(!tensors.contains(1))throw std::invalid_argument("scope provenance missing");
            const auto& receipt=*tensors.at(1);
            if(receipt.role!=storage::TemporalTensorRole::OpaqueExact || receipt.owner!=storage::TemporalTensorOwner{} ||
                receipt.input || receipt.output || receipt.interpretation!="rfc.observed-transport-pattern-scope.v1" ||
                receipt.tensor.domain()!=numeric::IndexDomain::linear(65) || receipt.tensor.support().count()!=65)
                throw std::invalid_argument("complete observed scope receipt required");
            for(std::size_t i=0;i<65;++i) {
                guard.step();const auto q=receipt.tensor.state(i).value();guard.value(q);
                if(q<0 || q.get_den()!=1 || q.get_num()>numeric::Integer(std::to_string(std::numeric_limits<std::size_t>::max())))
                    throw std::invalid_argument("invalid scope provenance value");
                const auto n=machine::checked_size(std::stoull(q.get_num().get_str()));
                if(i<64) {
                    if(n>255)throw std::invalid_argument("invalid scope digest byte");
                    (i<32?provenance.source_sha256[i]:provenance.evidence_sha256[i-32])=static_cast<std::byte>(n);
                } else provenance.observations=n;
            }
            if(!provenance.observations ||
                std::all_of(provenance.source_sha256.begin(),provenance.source_sha256.end(),[](auto b){return b==std::byte{0};}) ||
                std::all_of(provenance.evidence_sha256.begin(),provenance.evidence_sha256.end(),[](auto b){return b==std::byte{0};}))
                throw std::invalid_argument("nonempty external scope provenance required");
            retained=add(retained,65);
            for(std::size_t i=0;i<count;++i) {
                guard.step();if(!tensors.contains(i+2))throw std::invalid_argument("scope input image missing");
                const auto& t=*tensors.at(i+2);
                if(t.role!=storage::TemporalTensorRole::OpaqueExact || t.owner!=storage::TemporalTensorOwner{} ||
                    t.input || t.output || t.interpretation!="observed-common-input-image" ||
                    t.tensor.support().count()!=t.tensor.domain().logical_size())
                    throw std::invalid_argument("complete observed scope input image required");
                auto image=storage::temporal_tensor_matrix(t);guard.matrix(image);
                if(image.rows()!=carriers.at(lower+i+1)->domain.logical_size() || !image.cols())
                    throw std::invalid_argument("observed scope input carrier mismatch");
                const numeric::PreparedExactFrames frames(image,guard);
                if(frames.rank()!=image.cols())throw std::invalid_argument("independent observed scope input image required");
                retained=add(retained,add(8,image.nonzeros()));guard.entries(add(retained,frames.stored_entries()));
                inputs.push_back(std::move(image));
            }
            guard.entries(retained);
        }
    };
    std::shared_ptr<const State> state_;
    explicit PreparedTransportPatternScope(storage::TemporalEndpoint endpoint,G& guard)
        :state_(std::make_shared<const State>(std::move(endpoint),guard)) {}
public:
    static PreparedTransportPatternScope from_endpoint(storage::TemporalEndpoint endpoint,G& guard) {
        return PreparedTransportPatternScope(std::move(endpoint),guard);
    }
    static PreparedTransportPatternScope acquire(const TransportContextSamples& samples,
        const std::vector<std::vector<TransportContextView>>& atlas,TransportPatternProvenance provenance,G& guard) {
        using namespace context_view_detail;
        const auto acquired=acquire_context_transport_views(samples,{},atlas,guard,TransportContextInputScope::ObservedFields);
        const auto& rows=acquired.acquisition.seed_rows;
        if(rows.empty() || (provenance.observations && provenance.observations!=samples.size()))
            throw std::invalid_argument("complete nonempty observed scope required");
        const auto& source=samples.model().owner().fibers().complex();
        const auto& arena=source.grade(core::Grade{1});
        algebra::SubcomplexMask mask(source);
        for(const auto row:rows){guard.step();mask.select({core::Grade{1},static_cast<std::uint32_t>(row)});}
        mask.close_boundaries(source);auto patch=algebra::materialize_subcomplex(source,mask);
        storage::TemporalEndpoint endpoint(std::move(patch.complex));
        std::size_t resident=add(acquired.acquisition.retained,algebra::owned_gluing_detail::words(endpoint.source));
        for(std::size_t k=0;k<2;++k)for(std::size_t i=0;i<patch.to_ambient[k].size();++i) {
            guard.step();const auto& chart=atlas[k][patch.to_ambient[k][i]];
            if(!chart.frame)throw std::invalid_argument("every observed scope cell requires a common frame");
            storage::TemporalCarrier c;c.key=endpoint.carriers.size()+1;c.kind=storage::TemporalCarrierKind::CellStalk;
            c.grade=core::Grade{static_cast<std::uint16_t>(k)};
            c.cell=storage::stable_cell_key(endpoint.source,{c.grade,static_cast<std::uint32_t>(i)});
            c.coordinate_system=chart.frame;c.domain=numeric::IndexDomain::linear(chart.accession.rows());
            endpoint.carriers.push_back(std::move(c));resident=add(resident,8);guard.entries(resident);
        }
        std::map<std::size_t,std::vector<std::size_t>> observed;
        for(std::size_t sample=0;sample<samples.size();++sample)for(const auto row:samples.active_inputs(sample,guard)) {
            const auto [begin,end]=arena.incidence_range(row);bool active=false;
            for(auto p=begin;p<end;++p){guard.step();active=active || (samples.observed(sample,arena.lower_index[p],guard) &&
                arena.aspects.get(source.signature().existence_aspect(),p) &&
                algebra::owned_gluing_detail::coefficient(source,core::Grade{1},p,guard)!=0);}
            if(active){observed[row].push_back(sample);resident=add(resident,1);guard.entries(resident);}
        }
        for(std::size_t i=0;i<rows.size();++i) {
            const auto row=rows[i];const auto& ids=observed.at(row);const auto& a=atlas[1][row].accession;
            M inputs(a.cols(),ids.size());guard.dimension(ids.size());
            for(std::size_t j=0;j<ids.size();++j) {
                const auto input=samples.input(ids[j],row,guard);
                guard.entries(add(resident,add(inputs.nonzeros(),input.size())));
                for(std::size_t r=0;r<input.size();++r){guard.step();inputs.set(r,j,input[r]);}
            }
            const auto common=product(a,inputs,guard,add(resident,inputs.nonzeros()));
            const numeric::PreparedExactFrames frames(common,guard);
            guard.entries(add(resident,add(common.nonzeros(),frames.stored_entries())));
            storage::TemporalTensor t;t.key=i+2;t.role=storage::TemporalTensorRole::OpaqueExact;
            t.interpretation="observed-common-input-image";t.tensor=storage::temporal_matrix_tensor(frames.image());
            resident=add(resident,add(8,frames.image().nonzeros()));guard.entries(resident);endpoint.tensors.push_back(std::move(t));
        }
        std::vector<numeric::SparseRationalEntry> receipt;
        for(std::size_t i=0;i<64;++i)receipt.push_back({i,Q{std::to_integer<unsigned>(i<32?provenance.source_sha256[i]:provenance.evidence_sha256[i-32])}});
        receipt.push_back({64,Q{numeric::Integer(std::to_string(samples.size()))}});
        storage::TemporalTensor t;t.key=1;t.role=storage::TemporalTensorRole::OpaqueExact;
        t.interpretation="rfc.observed-transport-pattern-scope.v1";
        t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(65),numeric::StructuralSupport::full(65),
            numeric::SparseRationalVector(65,std::move(receipt)));endpoint.tensors.push_back(std::move(t));
        return PreparedTransportPatternScope(std::move(endpoint),guard);
    }
    const storage::TemporalEndpoint& endpoint() const & {return state_->endpoint;}
    const TransportPatternProvenance& provenance() const & {return state_->provenance;}
    const std::vector<M>& input_images() const & {return state_->inputs;}
    const storage::TemporalEndpoint& endpoint() const &&=delete;
    const TransportPatternProvenance& provenance() const &&=delete;
    const std::vector<M>& input_images() const &&=delete;
    std::size_t retained_entries() const noexcept {return state_->retained;}

    // Keep every observed branch sharing a stored pattern's primary type and
    // upper common carrier. Branch membership never depends on response values.
    PreparedTransportPatternScope for_bank(const PreparedTransportPatternBank& bank,G& guard) const {
        using namespace context_view_detail;
        (void)coverage(bank,guard);
        const auto& source=state_->endpoint.source;const auto n=bank.input_images().size();
        const auto lower=source.grade(core::Grade{0}).cell_count();
        const auto carriers=storage::temporal_endpoint_detail::carriers(state_->endpoint);
        const auto bc=storage::temporal_endpoint_detail::carriers(bank.endpoint());
        std::set<std::pair<std::size_t,std::uint64_t>> groups;
        for(std::size_t i=0;i<n;++i){guard.step();groups.emplace(bank.endpoint().source.grade(core::Grade{1}).relation_types[i],bc.at(3+n+i)->coordinate_system);}
        algebra::SubcomplexMask mask(source);
        for(std::size_t i=0;i<state_->inputs.size();++i){guard.step();
            if(groups.contains({source.grade(core::Grade{1}).relation_types[i],carriers.at(lower+i+1)->coordinate_system}))
                mask.select({core::Grade{1},static_cast<std::uint32_t>(i)});}
        mask.close_boundaries(source);auto patch=algebra::materialize_subcomplex(source,mask);
        storage::TemporalEndpoint endpoint(std::move(patch.complex));
        auto resident=add(state_->retained,algebra::owned_gluing_detail::words(endpoint.source));guard.entries(resident);
        for(std::size_t k=0;k<2;++k)for(std::size_t i=0;i<patch.to_ambient[k].size();++i) {
            guard.step();auto c=*carriers.at((k?lower:0)+patch.to_ambient[k][i]+1);
            c.key=endpoint.carriers.size()+1;c.grade=core::Grade{static_cast<std::uint16_t>(k)};
            c.cell=storage::stable_cell_key(endpoint.source,{c.grade,static_cast<std::uint32_t>(i)});
            endpoint.carriers.push_back(std::move(c));resident=add(resident,8);guard.entries(resident);
        }
        const auto tensors=storage::temporal_endpoint_detail::tensors(state_->endpoint);
        endpoint.tensors.push_back(*tensors.at(1));
        for(std::size_t i=0;i<patch.to_ambient[1].size();++i) {
            guard.step();auto t=*tensors.at(patch.to_ambient[1][i]+2);t.key=i+2;
            resident=add(resident,add(8,t.tensor.values().nonzeros()));guard.entries(resident);endpoint.tensors.push_back(std::move(t));
        }
        return PreparedTransportPatternScope(std::move(endpoint),guard);
    }

    // Coverage concerns complete typed stars and observed input subspaces.
    // It does not assert equality of output fields or unseen branch completeness.
    TransportPatternScopeCoverage coverage(const PreparedTransportPatternBank& bank,G& guard) const {
        using namespace context_view_detail;
        const auto& p=bank.provenance();const auto& s=*state_;guard.step();
        const auto resident=add(s.retained,bank.retained_entries());guard.entries(resident);
        if(p.source_sha256!=s.provenance.source_sha256 || p.evidence_sha256!=s.provenance.evidence_sha256 ||
            p.observations!=s.provenance.observations)throw std::invalid_argument("foreign observed scope provenance");
        const auto& source=s.endpoint.source;const auto& target=bank.endpoint().source;
        const auto lower=source.grade(core::Grade{0}).cell_count();const auto count=s.inputs.size();
        const auto n=bank.input_images().size();const auto carriers=storage::temporal_endpoint_detail::carriers(s.endpoint);
        const auto bc=storage::temporal_endpoint_detail::carriers(bank.endpoint());
        std::map<std::uint64_t,std::size_t> indices;
        for(std::size_t i=0;i<count;++i){guard.step();indices.emplace(storage::stable_cell_key(source,{core::Grade{1},static_cast<std::uint32_t>(i)}).key,i);}
        std::vector<std::size_t> origins;algebra::SubcomplexMask mask(source);
        for(std::size_t i=0;i<n;++i) {
            guard.step();const auto key=storage::stable_cell_key(target,{core::Grade{1},static_cast<std::uint32_t>(i)}).key;
            if(!indices.contains(key))throw std::invalid_argument("bank star absent from its observed scope");
            origins.push_back(indices.at(key));mask.select({core::Grade{1},static_cast<std::uint32_t>(origins.back())});
            const auto& original=*carriers.at(lower+origins.back()+1);const auto& current=*bc.at(3+n+i);
            if(original.coordinate_system!=current.coordinate_system || original.domain!=current.domain)
                throw std::invalid_argument("bank input differs from its original scope carrier");
        }
        mask.close_boundaries(source);const auto patch=algebra::materialize_subcomplex(source,mask);
        if(storage::encode_rfcx(patch.complex).bytes!=storage::encode_rfcx(target).bytes)
            throw std::invalid_argument("bank differs from its complete original scope columns");
        for(std::size_t i=0;i<patch.to_ambient[0].size();++i) {
            guard.step();const auto& a=*carriers.at(patch.to_ambient[0][i]+1);const auto& b=*bc.at(3+2*n+i);
            if(a.coordinate_system!=b.coordinate_system || a.domain!=b.domain)
                throw std::invalid_argument("bank boundary differs from its original scope carrier");
        }
        std::map<std::pair<std::size_t,std::uint64_t>,std::vector<std::size_t>> groups;
        std::vector<M> annihilators;std::size_t live=add(resident,add(count,mul(n,2)));
        for(std::size_t i=0;i<n;++i) {
            guard.step();groups[{source.grade(core::Grade{1}).relation_types[origins[i]],bc.at(3+n+i)->coordinate_system}].push_back(i);
            const numeric::PreparedExactFrames frame(bank.input_images()[i].transpose(),guard);
            guard.entries(add(live,frame.stored_entries()));annihilators.push_back(frame.kernel().transpose());
            live=add(live,annihilators.back().nonzeros());guard.entries(live);
        }
        TransportPatternScopeCoverage out;out.patterns.resize(count);
        for(std::size_t row=0;row<count;++row) {
            guard.step();const auto found=groups.find({source.grade(core::Grade{1}).relation_types[row],carriers.at(lower+row+1)->coordinate_system});
            if(found==groups.end())continue;
            for(const auto pattern:found->second) {
                guard.step();const auto origin=origins[pattern];TransportContextHypothesis h;h.origin=row;h.target=origin;
                if(row!=origin && !context_transport_detail::star(source,h,guard).empty())continue;
                if(row==origin) {
                    const auto [begin,end]=source.grade(core::Grade{1}).incidence_range(row);
                    for(auto pos=begin;pos<end;++pos){guard.step();const auto cell=source.grade(core::Grade{1}).lower_index[pos];h.cells.emplace_back(cell,cell);}
                }
                bool compatible=true;
                for(const auto& [a,b]:h.cells){guard.step();const auto& x=*carriers.at(a+1);const auto& y=*carriers.at(b+1);
                    compatible=compatible && x.coordinate_system==y.coordinate_system && x.domain==y.domain;}
                if(compatible && product(annihilators[pattern],s.inputs[row],guard,live).nonzeros()==0) {
                    out.patterns[row].push_back(pattern);live=add(live,1);guard.entries(live);
                }
            }
        }
        return out;
    }
};
inline std::vector<std::byte> encode_transport_pattern_scope(const PreparedTransportPatternScope& scope,
    std::uint64_t source_namespace,const storage::NativeSeriesLimits& limits={}) {
    return storage::encode_native_series(storage::NativeTemporalSeries(scope.endpoint(),source_namespace,limits));
}
inline PreparedTransportPatternScope decode_transport_pattern_scope(std::span<const std::byte> bytes,
    std::uint64_t expected_namespace,numeric::ExactActionGuard& guard,const storage::NativeSeriesLimits& limits={}) {
    const auto series=storage::decode_native_series(bytes,limits);
    if(series.source_namespace()!=expected_namespace || series.segment_count() || series.storage_stats().checkpoint_count || series.last_sequence())
        throw std::invalid_argument("single observed scope publication in its declared namespace required");
    const auto publication=series.tail();
    return PreparedTransportPatternScope::from_endpoint(publication.endpoint(),guard);
}
} // namespace rfc::learning
