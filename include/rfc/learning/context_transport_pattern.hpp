#pragma once
#include "rfc/learning/transport_parameter_basis.hpp"
#include "rfc/learning/context_transport_view_rules.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/storage/temporal_endpoint.hpp"
#include "rfc/storage/temporal_codec_primitives.hpp"

namespace rfc::learning {
struct TransportPatternProvenance {
    std::array<std::byte,32> source_sha256{}, evidence_sha256{};
    std::size_t observations{}, peers{};
    bool declared_templates{};
};
struct TransportPatternCoordinates {
    std::uint64_t domain{}, weights{}, parameters{};
};
struct TransportPatternFrameLabel {
    std::uint64_t frame{};
    std::string name;
    std::vector<std::string> axes;
};
struct TransportPatternResponse {
    numeric::RationalVector origin, coordinates;
    numeric::ExactMatrix directions;
    std::vector<std::size_t> offsets;
};

// An owned relation pattern on the common input image of its agreeing peers.
// The complete projected affine parameter family survives without observations.
// Provenance digests refer to external source and evidence archives.
class PreparedTransportPattern {
    friend class PreparedTransportPatternBank;
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using G=numeric::ExactActionGuard;
    struct State {
        storage::TemporalEndpoint endpoint;
        M domain, reconstruction, directions;
        V weights;
        TransportPatternProvenance provenance;
        std::vector<TransportPatternFrameLabel> labels;
        std::vector<std::size_t> weights_at, lower_at, occurrence_at;
        std::size_t retained{};
        State(storage::TemporalEndpoint value,G& guard):endpoint(std::move(value)) {
            using namespace context_view_detail;
            guard.step();storage::validate_temporal_endpoint(endpoint);
            const auto& source=endpoint.source;
            if(endpoint.sequence || source.grade_count()<2 || source.grade(core::Grade{1}).cell_count()!=1 ||
                !endpoint.local_blocks.empty() || !endpoint.finite_systems.empty() || endpoint.codes || endpoint.declarations)
                throw std::invalid_argument("complete single relation pattern profile required");
            for(std::size_t k=2;k<source.grade_count();++k)
                if(source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count())
                    throw std::invalid_argument("relation pattern has a nonempty higher grade");
            if(endpoint.tensors.size()!=4 && endpoint.tensors.size()!=5)
                throw std::invalid_argument("complete pattern tensor family required");
            const auto tensors=storage::temporal_endpoint_detail::tensors(endpoint);
            for(std::uint64_t i=1;i<=4;++i)
                if(!tensors.contains(i)) throw std::invalid_argument("pattern tensor key missing");
            const auto& a=*tensors.at(1);const auto& w=*tensors.at(2);
            const auto& f=*tensors.at(3);const auto& receipt=*tensors.at(4);
            const storage::TemporalTensorOwner global{};
            if(a.role!=storage::TemporalTensorRole::Accession || w.role!=storage::TemporalTensorRole::Field ||
                f.role!=storage::TemporalTensorRole::PhysicalRealization || a.owner!=global || w.owner!=global || f.owner!=global ||
                a.interpretation!="shared-input-image" || w.interpretation!="ordered-occurrence-weights" ||
                f.interpretation!="observable-parameter-directions" || receipt.role!=storage::TemporalTensorRole::OpaqueExact ||
                receipt.owner!=global || receipt.interpretation!="rfc.context-transport-pattern.v1" ||
                a.input!=1 || a.output!=2 || w.output!=3 || f.input!=4 || f.output!=3)
                throw std::invalid_argument("invalid relation pattern tensor roles");
            if(endpoint.carriers.size()!=source.grade(core::Grade{0}).cell_count()+4)
                throw std::invalid_argument("complete pattern carrier family required");
            const auto carriers=storage::temporal_endpoint_detail::carriers(endpoint);
            for(std::uint64_t i=1;i<=endpoint.carriers.size();++i)
                if(!carriers.contains(i)) throw std::invalid_argument("pattern carrier key missing");
            const auto& in=*carriers.at(1);
            if(in.kind!=storage::TemporalCarrierKind::CellStalk || in.cell!=storage::stable_cell_key(source,{core::Grade{1},0}))
                throw std::invalid_argument("pattern input stalk owner mismatch");
            for(std::uint64_t i=2;i<=4;++i)
                if(carriers.at(i)->kind!=storage::TemporalCarrierKind::Explicit)
                    throw std::invalid_argument("explicit pattern coordinate carrier required");
            std::set<std::uint64_t> derived{in.coordinate_system,carriers.at(3)->coordinate_system,carriers.at(4)->coordinate_system};
            if(derived.size()!=3 || derived.contains(carriers.at(2)->coordinate_system))
                throw std::invalid_argument("distinct declared pattern coordinates required");
            for(const auto& [key,c]:carriers) {
                (void)key;guard.step();guard.dimension(c->domain.logical_size());
                if(c->domain.rank()!=1) throw std::invalid_argument("linear pattern coordinate carriers required");
            }
            domain=storage::temporal_tensor_matrix(a);guard.matrix(domain);
            if(!domain.cols()) throw std::invalid_argument("nonempty common pattern input required");
            guard.dimension(w.tensor.domain().logical_size());guard.entries(w.tensor.domain().logical_size());
            for(const auto n:f.tensor.domain().extents())guard.dimension(n);
            guard.entries(f.tensor.values().nonzeros());
            weights=w.tensor.values().dense();directions=storage::temporal_tensor_matrix(f);
            guard.vector(weights);guard.matrix(directions);
            const auto full=[](const auto& t){return t.support().count()==t.domain().logical_size();};
            if(!full(a.tensor) || !full(w.tensor) || !full(f.tensor) ||
                !full(receipt.tensor) || receipt.tensor.domain()!=numeric::IndexDomain::linear(66))
                throw std::invalid_argument("complete declared pattern tensor support required");
            const auto exact_size=[&](std::size_t i) {
                const auto q=receipt.tensor.state(i).value();guard.value(q);
                if(q<0 || q.get_den()!=1 || q.get_num()>numeric::Integer(std::to_string(std::numeric_limits<std::size_t>::max())))
                    throw std::invalid_argument("invalid pattern provenance count");
                return machine::checked_size(std::stoull(q.get_num().get_str()));
            };
            for(std::size_t i=0;i<64;++i) {
                const auto byte=exact_size(i);
                if(byte>255) throw std::invalid_argument("invalid pattern provenance digest byte");
                (i<32?provenance.source_sha256[i]:provenance.evidence_sha256[i-32])=static_cast<std::byte>(byte);
            }
            provenance.observations=exact_size(64);provenance.peers=exact_size(65);
            if(!provenance.observations || !provenance.peers ||
                std::all_of(provenance.source_sha256.begin(),provenance.source_sha256.end(),[](auto b){return b==std::byte{0};}) ||
                std::all_of(provenance.evidence_sha256.begin(),provenance.evidence_sha256.end(),[](auto b){return b==std::byte{0};}))
                throw std::invalid_argument("nonempty external pattern provenance required");
            lower_at.push_back(0);
            for(std::size_t i=0;i<source.grade(core::Grade{0}).cell_count();++i) {
                const auto& c=*carriers.at(5+i);guard.step();
                if(c.kind!=storage::TemporalCarrierKind::CellStalk ||
                    c.cell!=storage::stable_cell_key(source,{core::Grade{0},static_cast<std::uint32_t>(i)}) ||
                    c.domain.rank()!=1 || derived.contains(c.coordinate_system))
                    throw std::invalid_argument("pattern lower stalk owner or coordinate mismatch");
                lower_at.push_back(add(lower_at.back(),c.domain.logical_size()));guard.dimension(lower_at.back());
            }
            const auto& arena=source.grade(core::Grade{1});weights_at.push_back(0);occurrence_at.push_back(0);
            std::set<std::size_t> touched;
            for(std::size_t p=0;p<arena.incidence_count();++p) {
                guard.step();touched.insert(arena.lower_index[p]);
                const auto n=lower_at[arena.lower_index[p]+1]-lower_at[arena.lower_index[p]];
                weights_at.push_back(add(weights_at.back(),mul(n,domain.cols())));
                occurrence_at.push_back(add(occurrence_at.back(),n));
                guard.dimension(weights_at.back());guard.dimension(occurrence_at.back());
            }
            if(touched.size()!=source.grade(core::Grade{0}).cell_count() || weights_at.back()!=weights.size() ||
                directions.rows()!=weights.size()) throw std::invalid_argument("complete pattern occurrence weight dimensions required");
            retained=add(algebra::owned_gluing_detail::words(source),add(weights.size(),add(domain.nonzeros(),directions.nonzeros())));
            retained=add(retained,add(66,add(lower_at.size(),add(weights_at.size(),occurrence_at.size()))));
            for(const auto& t:endpoint.tensors) {
                retained=add(retained,add(10,add(t.interpretation.size(),add(t.tensor.values().nonzeros(),t.tensor.domain().rank()))));
                guard.entries(retained);
            }
            retained=add(retained,mul(endpoint.carriers.size(),8));
            if(endpoint.tensors.size()==5) {
                const auto& labelled=*tensors.at(5);
                const std::string prefix="rfc.context-transport-pattern.frames.v1:";
                if(labelled.role!=storage::TemporalTensorRole::OpaqueExact || labelled.owner!=global ||
                    !labelled.interpretation.starts_with(prefix) || !full(labelled.tensor) ||
                    labelled.tensor.domain()!=numeric::IndexDomain::linear(0))
                    throw std::invalid_argument("declared pattern frame labels required");
                const auto bytes=std::as_bytes(std::span(labelled.interpretation)).subspan(prefix.size());
                const auto n=bytes.size();guard.entries(add(retained,n));
                storage::TemporalReadLimits limits;limits.max_records=guard.limits().entries;limits.max_file_bytes=bytes.size();
                storage::temporal_codec_detail::Reader reader(bytes,limits);
                const auto count=reader.count(24);std::uint64_t previous{};std::set<std::string> names;
                std::map<std::uint64_t,std::size_t> wanted;
                for(const auto& [key,c]:carriers) if(key==2 || key>=5) {
                    const auto [it,inserted]=wanted.emplace(c->coordinate_system,c->domain.logical_size());
                    if(!inserted && it->second!=c->domain.logical_size()) throw std::invalid_argument("pattern frame dimensions conflict");
                }
                if(count!=wanted.size()) throw std::invalid_argument("frame labels must cover exactly the used common carriers");
                for(std::size_t i=0;i<count;++i) {
                    guard.step();TransportPatternFrameLabel label;label.frame=reader.u64();label.name=reader.string();
                    if(label.frame<=previous || !wanted.contains(label.frame) || label.name.empty() || label.name.size()>256 ||
                        !names.insert(label.name).second) throw std::invalid_argument("distinct bounded pattern frame declarations required");
                    previous=label.frame;const auto axes=reader.count(8);
                    if(axes!=wanted.at(label.frame)) throw std::invalid_argument("pattern frame labels disagree with their coordinate domain");
                    std::set<std::string> keys;
                    for(std::size_t j=0;j<axes;++j) {
                        guard.step();auto name=reader.string();
                        if(name.empty() || name.size()>256 || !keys.insert(name).second) throw std::invalid_argument("distinct bounded pattern axis labels required");
                        retained=add(retained,add(1,name.size()));guard.entries(add(retained,n));label.axes.push_back(std::move(name));
                    }
                    retained=add(retained,add(3,label.name.size()));guard.entries(add(retained,n));labels.push_back(std::move(label));
                }
                if(reader.remaining()) throw std::invalid_argument("trailing pattern frame declarations");
            }
            guard.entries(retained);
            // A Gram solve supplies exact domain coordinates and membership.
            // The common image may be rectangular and need not be orthonormal.
            const auto gram=product(domain.transpose(),domain,guard,retained);
            numeric::PreparedExactSolve solve(gram,guard);
            reconstruction=field::hodge_frame_detail::solve_columns(solve,domain.transpose(),guard);
            if(product(reconstruction,domain,guard,retained)!=M::identity(domain.cols()))
                throw std::invalid_argument("independent pattern input frame required");
            if(transport_parameter_basis_rank(directions,guard).rank!=directions.cols())
                throw std::invalid_argument("independent observable parameter frame required");
            retained=add(retained,reconstruction.nonzeros());guard.entries(retained);
        }
    };
    std::shared_ptr<const State> state_;
    explicit PreparedTransportPattern(storage::TemporalEndpoint endpoint,G& guard)
        :state_(std::make_shared<const State>(std::move(endpoint),guard)) {}
public:
    // Loading admits the inference profile and its declared provenance references.
    // Replaying the referenced training proof requires its external archive.
    static PreparedTransportPattern from_endpoint(storage::TemporalEndpoint endpoint,G& guard) {
        return PreparedTransportPattern(std::move(endpoint),guard);
    }

    static PreparedTransportPattern learn(const TransportContextSamples& samples,const std::vector<std::size_t>& training,
        std::size_t origin,const std::vector<std::vector<TransportContextView>>& atlas,
        std::size_t minimum_peers,TransportPatternCoordinates coordinates,
        TransportPatternProvenance provenance,G& guard,
        TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
        using namespace context_view_detail;
        const auto& observations=samples.model();
        const auto acquired=acquire_context_transport_view_rules(samples,training,{},atlas,minimum_peers,guard,scope);
        const auto rule=std::find_if(acquired.rules.begin(),acquired.rules.end(),[&](const auto& r){return r.origin==origin;});
        if(rule==acquired.rules.end() || !rule->admitted || !acquired.proof)
            throw std::invalid_argument("independently admitted training origin required");
        if((provenance.observations && provenance.observations!=samples.size()) ||
            (provenance.peers && provenance.peers!=rule->forced.size()))
            throw std::invalid_argument("pattern provenance population differs from training proof");
        provenance.observations=samples.size();provenance.peers=rule->forced.size();
        return compile(observations,acquired,origin,atlas,input_domain(acquired,origin,atlas,guard),coordinates,provenance,guard);
    }
    static PreparedTransportPattern learn(const PreparedTransportObservations& observations,
        const std::vector<TransportObservation>& samples,const std::vector<std::size_t>& training,
        std::size_t origin,const std::vector<std::vector<TransportContextView>>& atlas,
        std::size_t minimum_peers,TransportPatternCoordinates coordinates,TransportPatternProvenance provenance,G& guard,
        TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
        return learn(TransportContextSamples(observations,samples,guard),training,origin,atlas,minimum_peers,coordinates,provenance,guard,scope);
    }
    static PreparedTransportPattern learn(const PreparedTransportEvidence& evidence,const std::vector<std::size_t>& training,
        std::size_t origin,const std::vector<std::vector<TransportContextView>>& atlas,
        std::size_t minimum_peers,TransportPatternCoordinates coordinates,TransportPatternProvenance provenance,G& guard,
        TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
        return learn(TransportContextSamples(evidence),training,origin,atlas,minimum_peers,coordinates,provenance,guard,scope);
    }
private:
    static M input_domain(const TransportContextViewRuleAcquisition& acquired,std::size_t origin,
        const std::vector<std::vector<TransportContextView>>& atlas,G& guard) {
        using namespace context_view_detail;
        const auto rule=std::find_if(acquired.rules.begin(),acquired.rules.end(),[&](const auto& r){return r.origin==origin;});
        if(rule==acquired.rules.end() || !rule->admitted || !acquired.proof)
            throw std::invalid_argument("independently admitted training origin required");
        M domain=atlas.at(1).at(origin).accession;
        for(const auto peer:rule->forced) {
            guard.step();const auto& other=acquired.comparisons.witnesses.at(peer).common_input;
            const numeric::PreparedExactFrames left(domain,guard),right(other,guard);
            const auto chart=[&](const M& frame) {
                numeric::PreparedExactSolve gram(product(frame.transpose(),frame,guard,acquired.retained),guard);
                return TransportContextView{atlas[1][origin].frame,frame,field::hodge_frame_detail::solve_columns(gram,frame.transpose(),guard)};
            };
            domain=intersection(chart(left.image()),chart(right.image()),guard,acquired.retained).common_input;
        }
        return domain;
    }
    static PreparedTransportPattern compile(const PreparedTransportObservations& observations,
        const TransportContextViewRuleAcquisition& acquired,std::size_t origin,
        const std::vector<std::vector<TransportContextView>>& atlas,M domain,
        TransportPatternCoordinates coordinates,TransportPatternProvenance provenance,G& guard,
        M* unreduced_directions=nullptr) {
        using namespace context_view_detail;
        const numeric::PreparedExactFrames input_frame(domain,guard);domain=input_frame.image();
        if(!domain.cols()) throw std::invalid_argument("agreeing peers have no common pattern input");
        const auto& fibers=observations.owner().fibers();const auto& source=fibers.complex();const core::Grade up{1};
        algebra::SubcomplexMask patch(source);patch.select({up,static_cast<std::uint32_t>(origin)});patch.close_boundaries(source);
        auto selected=algebra::materialize_subcomplex(source,patch);
        storage::TemporalEndpoint endpoint(std::move(selected.complex));
        const auto add_carrier=[&](std::uint64_t key,std::uint64_t frame,std::size_t n,std::optional<storage::StableCellKey> cell={}) {
            storage::TemporalCarrier carrier;carrier.key=key;carrier.coordinate_system=frame;carrier.domain=numeric::IndexDomain::linear(n);
            if(cell) {carrier.kind=storage::TemporalCarrierKind::CellStalk;carrier.cell=cell;carrier.grade=cell->grade;}
            endpoint.carriers.push_back(std::move(carrier));
        };
        add_carrier(1,coordinates.domain,domain.cols(),storage::stable_cell_key(endpoint.source,{up,0}));
        add_carrier(2,atlas[1][origin].frame,domain.rows());
        const auto& arena=source.grade(up);const auto [begin,end]=arena.incidence_range(origin);
        const auto local_input=product(atlas[1][origin].reconstruction,domain,guard,acquired.retained);
        std::vector<std::size_t> offsets{0};
        for(std::size_t p=begin;p<end;++p) {
            guard.step();offsets.push_back(add(offsets.back(),mul(atlas[0][arena.lower_index[p]].accession.rows(),domain.cols())));
            guard.dimension(offsets.back());
        }
        M entry_readout(offsets.back(),observations.variable_coordinates().size());V fixed(offsets.back());
        for(std::size_t p=begin;p<end;++p) {
            guard.step();const auto& lower=atlas[0][arena.lower_index[p]].accession;
            const auto coefficient=algebra::owned_gluing_detail::coefficient(source,up,p,guard);
            const auto realized=product(product(lower,*fibers.transport_override(up,p),guard,acquired.retained),local_input,guard,acquired.retained);
            if(coefficient==0 && realized.nonzeros()) throw std::logic_error("inactive fixed pattern map");
            for(const auto& [key,q]:realized.entries()) fixed[offsets[p-begin]+key.first*domain.cols()+key.second]=guard.div(q,coefficient);
        }
        // Entry maps are projected before the evidence kernel is reduced.
        // This retains correlations, including cancelled occurrence directions.
        for(std::size_t e=0;e<observations.variable_coordinates().size();++e) {
            const auto& c=observations.variable_coordinates()[e];guard.step();
            if(c.occurrence<begin || c.occurrence>=end) continue;
            const auto& lower=atlas[0][arena.lower_index[c.occurrence]].accession;
            for(const auto& [lo,lq]:lower.entries()) if(lo.second==c.row)
                for(const auto& [hi,hq]:local_input.entries()) if(hi.first==c.column) {
                    const auto row=offsets[c.occurrence-begin]+lo.first*domain.cols()+hi.second;
                    entry_readout.set(row,e,guard.add(entry_readout.get(row,e),guard.mul(lq,hq)));
                    guard.entries(add(acquired.retained,add(entry_readout.nonzeros(),fixed.size())));
                }
        }
        auto readout=observations.parameter_basis() ?
            product(entry_readout,*observations.parameter_basis(),guard,add(acquired.retained,entry_readout.nonzeros())) :
            std::move(entry_readout);
        auto weights=numeric::checked_apply(readout,acquired.proof->evidence_origin(),guard);
        for(std::size_t i=0;i<weights.size();++i) weights[i]=guard.add(weights[i],fixed[i]);
        auto projected=product(readout,acquired.proof->evidence_kernel(),guard,acquired.retained);
        if(unreduced_directions) *unreduced_directions=projected;
        const auto directions=transport_observable_image(projected,guard,acquired.retained);
        add_carrier(3,coordinates.weights,weights.size());add_carrier(4,coordinates.parameters,directions.cols());
        for(std::size_t i=0;i<selected.to_ambient[0].size();++i) {
            const auto& lower=atlas[0][selected.to_ambient[0][i]];
            add_carrier(5+i,lower.frame,lower.accession.rows(),storage::stable_cell_key(endpoint.source,{core::Grade{0},static_cast<std::uint32_t>(i)}));
        }
        storage::TemporalTensor a;a.key=1;a.role=storage::TemporalTensorRole::Accession;a.input=1;a.output=2;
        a.interpretation="shared-input-image";a.tensor=storage::temporal_matrix_tensor(domain);endpoint.tensors.push_back(std::move(a));
        storage::TemporalTensor w;w.key=2;w.role=storage::TemporalTensorRole::Field;w.output=3;w.interpretation="ordered-occurrence-weights";
        std::vector<numeric::SparseRationalEntry> entries;
        for(std::size_t i=0;i<weights.size();++i) if(weights[i]!=0) entries.push_back({i,weights[i]});
        w.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(weights.size()),numeric::StructuralSupport::full(weights.size()),
            numeric::SparseRationalVector(weights.size(),std::move(entries)));endpoint.tensors.push_back(std::move(w));
        storage::TemporalTensor f;f.key=3;f.role=storage::TemporalTensorRole::PhysicalRealization;f.input=4;f.output=3;
        f.interpretation="observable-parameter-directions";f.tensor=storage::temporal_matrix_tensor(directions);endpoint.tensors.push_back(std::move(f));
        storage::TemporalTensor receipt;receipt.key=4;receipt.role=storage::TemporalTensorRole::OpaqueExact;
        receipt.interpretation="rfc.context-transport-pattern.v1";entries.clear();
        for(std::size_t i=0;i<64;++i) {
            const auto b=std::to_integer<unsigned>(i<32?provenance.source_sha256[i]:provenance.evidence_sha256[i-32]);
            if(b) entries.push_back({i,Q{b}});
        }
        entries.push_back({64,Q{numeric::Integer(std::to_string(provenance.observations))}});
        entries.push_back({65,Q{numeric::Integer(std::to_string(provenance.peers))}});
        receipt.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(66),numeric::StructuralSupport::full(66),
            numeric::SparseRationalVector(66,std::move(entries)));endpoint.tensors.push_back(std::move(receipt));
        return PreparedTransportPattern(std::move(endpoint),guard);
    }
public:
    const storage::TemporalEndpoint& endpoint() const & {return state_->endpoint;}
    const M& input_image() const & {return state_->domain;}
    const V& weights() const & {return state_->weights;}
    const M& parameter_directions() const & {return state_->directions;}
    const TransportPatternProvenance& provenance() const & {return state_->provenance;}
    const storage::TemporalEndpoint& endpoint() const &&=delete;
    const M& input_image() const &&=delete;
    const V& weights() const &&=delete;
    const M& parameter_directions() const &&=delete;
    const TransportPatternProvenance& provenance() const &&=delete;
    std::size_t retained_entries() const noexcept {return state_->retained;}
    const std::vector<TransportPatternFrameLabel>& frame_labels() const & {return state_->labels;}
    const std::vector<TransportPatternFrameLabel>& frame_labels() const &&=delete;
    PreparedTransportPattern with_frame_labels(std::vector<TransportPatternFrameLabel> labels,G& guard) const {
        using namespace context_view_detail;
        auto endpoint=state_->endpoint;
        if(endpoint.tensors.size()!=4) throw std::invalid_argument("pattern frame labels are immutable");
        std::sort(labels.begin(),labels.end(),[](const auto& a,const auto& b){return a.frame<b.frame;});
        storage::temporal_codec_detail::Writer writer;writer.u64(labels.size());
        for(const auto& label:labels) {
            guard.step();writer.u64(label.frame);storage::temporal_codec_detail::string(writer,label.name);writer.u64(label.axes.size());
            for(const auto& name:label.axes) {guard.step();storage::temporal_codec_detail::string(writer,name);}
            guard.entries(add(state_->retained,writer.size()));
        }
        storage::TemporalTensor tensor;tensor.key=5;tensor.role=storage::TemporalTensorRole::OpaqueExact;
        tensor.interpretation="rfc.context-transport-pattern.frames.v1:";
        tensor.interpretation.append(reinterpret_cast<const char*>(writer.bytes().data()),writer.size());
        tensor.tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(0),numeric::StructuralSupport::full(0));
        endpoint.tensors.push_back(std::move(tensor));
        return PreparedTransportPattern(std::move(endpoint),guard);
    }
    std::vector<TransportCoordinate> weight_coordinates() const {
        std::vector<TransportCoordinate> result;
        for(std::size_t p=0;p+1<state_->weights_at.size();++p)
            for(std::size_t i=0;i<state_->occurrence_at[p+1]-state_->occurrence_at[p];++i)
                for(std::size_t j=0;j<state_->domain.cols();++j) result.push_back({p,i,j});
        return result;
    }

    // Return the exact response family in the declared common lower frames.
    // Shared boundaries sum original signed coefficients once per occurrence.
    TransportPatternResponse response(const V& common_input,G& guard,bool shared=true) const {
        using namespace context_view_detail;const auto pin=state_;guard.step();
        guard.entries(pin->retained);const auto input=guard.canonical_input(common_input);
        if(input.size()!=pin->domain.rows()) throw std::invalid_argument("declared common pattern input required");
        TransportPatternResponse out;out.coordinates=numeric::checked_apply(pin->reconstruction,input,guard);
        if(numeric::checked_apply(pin->domain,out.coordinates,guard)!=input)
            throw std::invalid_argument("input outside independently evidenced pattern domain");
        out.offsets=shared?pin->lower_at:pin->occurrence_at;
        M readout(out.offsets.back(),pin->weights.size());const auto& source=pin->endpoint.source;
        const auto& arena=source.grade(core::Grade{1});
        for(std::size_t p=0;p<arena.incidence_count();++p) {
            guard.step();const auto n=pin->occurrence_at[p+1]-pin->occurrence_at[p];
            const auto q=shared?algebra::owned_gluing_detail::coefficient(source,core::Grade{1},p,guard):Q{1};
            const auto row=shared?pin->lower_at[arena.lower_index[p]]:pin->occurrence_at[p];
            for(std::size_t i=0;i<n;++i) for(std::size_t j=0;j<out.coordinates.size();++j) {
                guard.step();readout.set(row+i,pin->weights_at[p]+i*out.coordinates.size()+j,guard.mul(q,out.coordinates[j]));
                guard.entries(add(pin->retained,add(readout.nonzeros(),out.coordinates.size())));
            }
        }
        out.origin=numeric::checked_apply(readout,pin->weights,guard);
        const numeric::PreparedExactFrames family(product(readout,pin->directions,guard,pin->retained),guard);
        out.directions=family.image();return out;
    }
};
} // namespace rfc::learning
