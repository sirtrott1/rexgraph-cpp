#pragma once
#include "rfc/learning/context_transport_pattern.hpp"

namespace rfc::learning {
struct TransportPatternBankCoordinates {
    std::uint64_t weights{}, parameters{};
    std::vector<std::uint64_t> domains;
};
struct TransportPatternInput {
    std::size_t pattern{};
    numeric::RationalVector value;
};
struct TransportPatternSparseInput {
    std::size_t pattern{};
    std::uint64_t frame{};
    numeric::SparseRationalTensor value;
};
struct TransportPatternComparison {
    std::string name;
    std::size_t left{}, right{};
    std::uint64_t left_frame{}, right_frame{}, interface_frame{};
    numeric::ExactMatrix left_map, right_map;
};
struct TransportPatternBankResponse {
    numeric::RationalVector origin;
    numeric::ExactMatrix directions;
    std::vector<numeric::RationalVector> coordinates;
    std::vector<std::size_t> offsets;
};
struct TransportPatternGluing {
    bool consistent{}, forced{};
    numeric::RationalVector defect_origin, parameter_origin, contradiction;
    numeric::ExactMatrix defect_directions, parameter_directions;
    std::vector<std::size_t> offsets, section_cells;
    std::vector<std::string> comparison_names;
    std::optional<TransportPatternBankResponse> boundary_response, section;
};

// One retained complex and one joint observable family for its learned stars.
// Shared source cells retain one identity; free parameters remain correlated.
class PreparedTransportPatternBank {
    using M=numeric::ExactMatrix;
    using V=numeric::RationalVector;
    using Q=numeric::Rational;
    using G=numeric::ExactActionGuard;
    struct State {
        storage::TemporalEndpoint endpoint;
        std::vector<M> domains, reconstructions;
        std::vector<std::uint64_t> input_frames;
        V weights;
        M directions;
        TransportPatternProvenance provenance;
        std::vector<TransportPatternFrameLabel> labels;
        std::vector<std::size_t> lower_at{0}, occurrence_at{0}, weights_at{0}, owners;
        std::size_t retained{};
        State(storage::TemporalEndpoint value,G& guard):endpoint(std::move(value)) {
            using namespace context_view_detail;
            guard.step();storage::validate_temporal_endpoint(endpoint);
            const auto& source=endpoint.source;
            if(endpoint.sequence || source.grade_count()<2 || !endpoint.local_blocks.empty() ||
                !endpoint.finite_systems.empty() || endpoint.codes || endpoint.declarations)
                throw std::invalid_argument("complete relation pattern bank required");
            for(std::size_t k=2;k<source.grade_count();++k)
                if(source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count())
                    throw std::invalid_argument("pattern bank has a nonempty higher grade");
            const auto& arena=source.grade(core::Grade{1});const auto count=arena.cell_count();
            const auto lower=source.grade(core::Grade{0}).cell_count();guard.dimension(count);
            if(!count || (endpoint.tensors.size()!=add(count,3) && endpoint.tensors.size()!=add(count,4)) ||
                endpoint.carriers.size()!=add(add(mul(count,2),lower),2))
                throw std::invalid_argument("complete bank tensor and carrier families required");
            const auto tensors=storage::temporal_endpoint_detail::tensors(endpoint);
            const auto carriers=storage::temporal_endpoint_detail::carriers(endpoint);
            for(std::uint64_t i=1;i<=endpoint.carriers.size();++i)
                if(!carriers.contains(i)) throw std::invalid_argument("bank carrier key missing");
            for(const auto key:{1u,2u,3u}) if(!tensors.contains(key)) throw std::invalid_argument("bank tensor key missing");
            const storage::TemporalTensorOwner global{};
            const auto& w=*tensors.at(1);const auto& f=*tensors.at(2);const auto& receipt=*tensors.at(3);
            if(w.role!=storage::TemporalTensorRole::Field || w.owner!=global || w.input || w.output!=1 ||
                w.interpretation!="ordered-bank-occurrence-weights" || f.role!=storage::TemporalTensorRole::PhysicalRealization ||
                f.owner!=global || f.input!=2 || f.output!=1 || f.interpretation!="joint-observable-parameter-directions" ||
                receipt.role!=storage::TemporalTensorRole::OpaqueExact || receipt.owner!=global || receipt.input || receipt.output ||
                receipt.interpretation!="rfc.context-transport-pattern-bank.v1")
                throw std::invalid_argument("invalid bank tensor roles");
            const auto full=[](const auto& t){return t.support().count()==t.domain().logical_size();};
            if(!full(w.tensor) || !full(f.tensor) || !full(receipt.tensor) ||
                receipt.tensor.domain()!=numeric::IndexDomain::linear(66))
                throw std::invalid_argument("complete bank tensor support required");
            weights=w.tensor.values().dense();directions=storage::temporal_tensor_matrix(f);
            guard.vector(weights);guard.matrix(directions);
            if(directions.rows()!=weights.size()) throw std::invalid_argument("joint bank weight dimensions");
            std::set<std::uint64_t> derived;
            for(std::size_t i=1;i<=2;++i) {
                const auto& c=*carriers.at(i);
                if(c.kind!=storage::TemporalCarrierKind::Explicit || !derived.insert(c.coordinate_system).second)
                    throw std::invalid_argument("distinct bank weight and parameter coordinates required");
            }
            std::map<std::uint64_t,std::size_t> common;
            retained=add(algebra::owned_gluing_detail::words(source),add(weights.size(),directions.nonzeros()));
            for(const auto& [key,c]:carriers) {
                guard.step();guard.dimension(c->domain.logical_size());
                if(c->domain.rank()!=1) throw std::invalid_argument("linear bank carriers required");
                retained=add(retained,8);
                if(key>=3 && key<3+count) {
                    if(c->kind!=storage::TemporalCarrierKind::CellStalk ||
                        c->cell!=storage::stable_cell_key(source,{core::Grade{1},static_cast<std::uint32_t>(key-3)}) ||
                        !derived.insert(c->coordinate_system).second)
                        throw std::invalid_argument("distinct original bank input owners required");
                } else if(key>=3+count) {
                    const auto [it,inserted]=common.emplace(c->coordinate_system,c->domain.logical_size());
                    if(!inserted && it->second!=c->domain.logical_size()) throw std::invalid_argument("bank common frame dimensions conflict");
                    if(key<3+2*count && c->kind!=storage::TemporalCarrierKind::Explicit)
                        throw std::invalid_argument("explicit bank common input carrier required");
                    if(key>=3+2*count && (c->kind!=storage::TemporalCarrierKind::CellStalk ||
                        c->cell!=storage::stable_cell_key(source,{core::Grade{0},static_cast<std::uint32_t>(key-3-2*count)})))
                        throw std::invalid_argument("original bank lower stalk owner required");
                }
            }
            for(const auto& [frame,n]:common) {(void)n;if(derived.contains(frame)) throw std::invalid_argument("derived and common bank coordinates overlap");}
            for(std::size_t i=0;i<lower;++i) {
                lower_at.push_back(add(lower_at.back(),carriers.at(3+2*count+i)->domain.logical_size()));
                guard.dimension(lower_at.back());
            }
            std::set<std::size_t> touched;
            for(std::size_t i=0;i<count;++i) {
                if(!tensors.contains(5+i)) throw std::invalid_argument("bank input image missing");
                const auto& a=*tensors.at(5+i);
                if(a.role!=storage::TemporalTensorRole::Accession || a.owner!=global || a.input!=3+i ||
                    a.output!=3+count+i || a.interpretation!="shared-bank-input-image" || !full(a.tensor))
                    throw std::invalid_argument("bank input image roles or support");
                auto domain=storage::temporal_tensor_matrix(a);guard.matrix(domain);
                if(!domain.cols()) throw std::invalid_argument("nonempty bank input image required");
                retained=add(retained,domain.nonzeros());guard.entries(retained);
                numeric::PreparedExactSolve solve(product(domain.transpose(),domain,guard,retained),guard);
                auto reconstruction=field::hodge_frame_detail::solve_columns(solve,domain.transpose(),guard);
                if(product(reconstruction,domain,guard,retained)!=M::identity(domain.cols()))
                    throw std::invalid_argument("independent bank input image required");
                retained=add(retained,reconstruction.nonzeros());
                const auto [begin,end]=arena.incidence_range(i);
                for(std::size_t pos=begin;pos<end;++pos) {
                    guard.step();touched.insert(arena.lower_index[pos]);owners.push_back(i);
                    const auto n=lower_at[arena.lower_index[pos]+1]-lower_at[arena.lower_index[pos]];
                    occurrence_at.push_back(add(occurrence_at.back(),n));weights_at.push_back(add(weights_at.back(),mul(n,domain.cols())));
                    guard.dimension(occurrence_at.back());guard.dimension(weights_at.back());
                }
                domains.push_back(std::move(domain));reconstructions.push_back(std::move(reconstruction));
                input_frames.push_back(carriers.at(3+count+i)->coordinate_system);
            }
            if(touched.size()!=lower || weights_at.back()!=weights.size()) throw std::invalid_argument("complete bank occurrence weights required");
            const numeric::PreparedExactFrames unknown(directions,guard);
            if(unknown.rank()!=directions.cols()) throw std::invalid_argument("independent joint bank parameter image required");
            const auto number=[&](std::size_t i) {
                const auto q=receipt.tensor.state(i).value();guard.value(q);
                if(q<0 || q.get_den()!=1 || q.get_num()>numeric::Integer(std::to_string(std::numeric_limits<std::size_t>::max())))
                    throw std::invalid_argument("invalid bank provenance count");
                return machine::checked_size(std::stoull(q.get_num().get_str()));
            };
            for(std::size_t i=0;i<64;++i) {
                const auto v=number(i);if(v>255) throw std::invalid_argument("invalid bank provenance digest byte");
                (i<32?provenance.source_sha256[i]:provenance.evidence_sha256[i-32])=static_cast<std::byte>(v);
            }
            provenance.observations=number(64);provenance.peers=number(65);
            if(!provenance.observations || !provenance.peers ||
                std::all_of(provenance.source_sha256.begin(),provenance.source_sha256.end(),[](auto v){return v==std::byte{0};}) ||
                std::all_of(provenance.evidence_sha256.begin(),provenance.evidence_sha256.end(),[](auto v){return v==std::byte{0};}))
                throw std::invalid_argument("nonempty external bank provenance required");
            for(const auto& t:endpoint.tensors) {
                retained=add(retained,add(10,add(t.interpretation.size(),add(t.tensor.values().nonzeros(),t.tensor.domain().rank()))));guard.entries(retained);
            }
            retained=add(retained,add(input_frames.size(),add(66,add(lower_at.size(),add(owners.size(),add(weights_at.size(),occurrence_at.size()))))));
            if(endpoint.tensors.size()==count+4) {
                if(!tensors.contains(4)) throw std::invalid_argument("bank frame declarations missing");
                const auto& t=*tensors.at(4);const std::string prefix="rfc.context-transport-pattern-bank.frames.v1:";
                if(t.role!=storage::TemporalTensorRole::OpaqueExact || t.owner!=global || t.input || t.output ||
                    !t.interpretation.starts_with(prefix) || t.tensor.domain()!=numeric::IndexDomain::linear(0) || !full(t.tensor))
                    throw std::invalid_argument("invalid bank frame declarations");
                const auto bytes=std::as_bytes(std::span(t.interpretation)).subspan(prefix.size());
                storage::TemporalReadLimits limits;limits.max_records=guard.limits().entries;limits.max_file_bytes=bytes.size();
                storage::temporal_codec_detail::Reader reader(bytes,limits);const auto n=reader.count(24);
                if(n!=common.size()) throw std::invalid_argument("bank names must cover exactly the common frames");
                std::uint64_t previous{};std::set<std::string> names;
                for(std::size_t i=0;i<n;++i) {
                    guard.step();TransportPatternFrameLabel label;label.frame=reader.u64();label.name=reader.string();
                    if(label.frame<=previous || !common.contains(label.frame) || label.name.empty() || label.name.size()>256 || !names.insert(label.name).second)
                        throw std::invalid_argument("distinct bounded bank frame names required");
                    previous=label.frame;const auto axes=reader.count(8);
                    if(axes!=common.at(label.frame)) throw std::invalid_argument("bank axis names disagree with their carrier");
                    std::set<std::string> axis_names;
                    for(std::size_t j=0;j<axes;++j) {
                        guard.step();auto name=reader.string();
                        if(name.empty() || name.size()>256 || !axis_names.insert(name).second) throw std::invalid_argument("distinct bounded bank axis names required");
                        retained=add(retained,add(1,name.size()));guard.entries(retained);label.axes.push_back(std::move(name));
                    }
                    retained=add(retained,add(3,label.name.size()));labels.push_back(std::move(label));
                }
                if(reader.remaining()) throw std::invalid_argument("trailing bank frame declarations");
            }
            guard.entries(retained);
        }
    };
    std::shared_ptr<const State> state_;
    explicit PreparedTransportPatternBank(storage::TemporalEndpoint endpoint,G& guard)
        :state_(std::make_shared<const State>(std::move(endpoint),guard)) {}
    V input_coordinates(const TransportPatternInput& request,G& guard) const {
        const auto& s=*state_;const auto input=guard.canonical_input(request.value);
        const auto& domain=s.domains[request.pattern];
        if(input.size()!=domain.rows()) throw std::invalid_argument("declared bank common input required");
        auto c=numeric::checked_apply(s.reconstructions[request.pattern],input,guard);
        if(numeric::checked_apply(domain,c,guard)!=input) throw std::invalid_argument("input outside evidenced bank domain");
        return c;
    }
    numeric::SparseRationalVector sparse_action(const M& map,const numeric::SparseRationalVector& input,G& guard) const {
        using namespace context_view_detail;
        if(map.cols()!=input.size()) throw std::invalid_argument("sparse bank action dimensions");
        guard.dimension(map.rows());guard.dimension(map.cols());
        const auto& values=input.entries();std::map<std::size_t,Q> sums;
        for(const auto& [key,q]:map.entries()) {
            guard.step();const auto it=std::lower_bound(values.begin(),values.end(),key.second,
                [](const auto& entry,std::size_t index){return entry.index<index;});
            if(it==values.end() || it->index!=key.second) continue;
            const auto value=guard.add(sums[key.first],guard.mul(q,it->value));
            if(value==0) sums.erase(key.first);else sums[key.first]=value;
            guard.entries(add(state_->retained,add(input.nonzeros(),sums.size())));
        }
        std::vector<numeric::SparseRationalEntry> out;out.reserve(sums.size());
        for(auto& [index,q]:sums) {guard.step();out.push_back({index,std::move(q)});}
        return numeric::SparseRationalVector(map.rows(),std::move(out));
    }
    V input_coordinates(const TransportPatternSparseInput& request,G& guard) const {
        using namespace context_view_detail;const auto& s=*state_;const auto& domain=s.domains[request.pattern];
        const auto& tensor=request.value;
        if(request.frame!=s.input_frames[request.pattern] || tensor.domain()!=numeric::IndexDomain::linear(domain.rows()) ||
            tensor.support().count()!=domain.rows())
            throw std::invalid_argument("complete sparse field on the declared bank common frame required");
        guard.dimension(domain.rows());guard.entries(add(s.retained,tensor.values().nonzeros()));
        for(const auto& e:tensor.values().entries()) {guard.step();guard.value(e.value);}
        const auto c=sparse_action(s.reconstructions[request.pattern],tensor.values(),guard);
        const auto restored=sparse_action(domain,c,guard);
        if(restored.entries()!=tensor.values().entries()) throw std::invalid_argument("input outside evidenced bank domain");
        guard.entries(add(s.retained,add(tensor.values().nonzeros(),c.size())));
        return c.dense();
    }
    template<class Input>
    M readout(const std::vector<Input>& inputs,G& guard,bool shared,
        std::vector<V>& coordinates) const {
        using namespace context_view_detail;const auto& s=*state_;guard.step();guard.entries(s.retained);
        if(inputs.empty()) throw std::invalid_argument("nonempty bank input selection required");
        coordinates.resize(s.domains.size());std::set<std::size_t> selected;
        const auto& arena=s.endpoint.source.grade(core::Grade{1});
        M out(shared?s.lower_at.back():s.occurrence_at.back(),s.weights.size());
        for(const auto& request:inputs) {
            guard.step();if(request.pattern>=s.domains.size() || !selected.insert(request.pattern).second)
                throw std::invalid_argument("distinct original bank pattern indices required");
            auto& c=coordinates[request.pattern];c=input_coordinates(request,guard);
            const auto [begin,end]=arena.incidence_range(request.pattern);
            for(std::size_t p=begin;p<end;++p) {
                const auto n=s.occurrence_at[p+1]-s.occurrence_at[p];
                const auto row=shared?s.lower_at[arena.lower_index[p]]:s.occurrence_at[p];
                const auto q=shared?algebra::owned_gluing_detail::coefficient(s.endpoint.source,core::Grade{1},p,guard):Q{1};
                for(std::size_t i=0;i<n;++i) for(std::size_t j=0;j<c.size();++j) {
                    guard.step();out.set(row+i,s.weights_at[p]+i*c.size()+j,guard.mul(q,c[j]));
                    guard.entries(add(s.retained,add(out.nonzeros(),s.weights.size())));
                }
            }
        }
        return out;
    }
    TransportPatternBankResponse apply(const M& map,const V& weights,const M& directions,
        std::vector<V> coordinates,G& guard,bool shared) const {
        TransportPatternBankResponse out;out.origin=numeric::checked_apply(map,weights,guard);
        numeric::PreparedExactFrames image(context_view_detail::product(map,directions,guard,state_->retained),guard);
        out.directions=image.image();out.coordinates=std::move(coordinates);
        out.offsets=shared?state_->lower_at:state_->occurrence_at;return out;
    }
public:
    static PreparedTransportPatternBank from_endpoint(storage::TemporalEndpoint endpoint,G& guard) {
        return PreparedTransportPatternBank(std::move(endpoint),guard);
    }
    static PreparedTransportPatternBank learn(const PreparedTransportObservations& observations,
        const std::vector<TransportObservation>& samples,const std::vector<std::size_t>& training,
        const std::vector<std::size_t>& origins,const std::vector<std::vector<TransportContextView>>& atlas,
        std::size_t minimum_peers,TransportPatternBankCoordinates coordinates,TransportPatternProvenance provenance,G& guard,
        TransportContextInputScope scope=TransportContextInputScope::FullCarrier) {
        using namespace context_view_detail;
        if(origins.empty() || origins.size()!=coordinates.domains.size() ||
            !std::is_sorted(origins.begin(),origins.end()) || std::adjacent_find(origins.begin(),origins.end())!=origins.end())
            throw std::invalid_argument("ordered distinct bank origins and domain identities required");
        const auto acquired=acquire_context_transport_view_rules(observations,samples,training,{},atlas,minimum_peers,guard,scope);
        if(!acquired.proof) throw std::invalid_argument("independently admitted bank training required");
        const auto& source=observations.owner().fibers().complex();algebra::SubcomplexMask patch(source);
        for(const auto origin:origins) patch.select({core::Grade{1},static_cast<std::uint32_t>(origin)});
        patch.close_boundaries(source);auto selected=algebra::materialize_subcomplex(source,patch);
        storage::TemporalEndpoint endpoint(std::move(selected.complex));
        const auto carrier=[&](std::uint64_t key,std::uint64_t frame,std::size_t n,std::optional<storage::StableCellKey> cell={}) {
            storage::TemporalCarrier c;c.key=key;c.coordinate_system=frame;c.domain=numeric::IndexDomain::linear(n);
            if(cell) {c.kind=storage::TemporalCarrierKind::CellStalk;c.cell=cell;c.grade=cell->grade;}
            endpoint.carriers.push_back(std::move(c));
        };
        std::vector<V> weights;std::vector<M> directions;std::size_t total{},peers{},resident=acquired.retained;
        for(std::size_t i=0;i<origins.size();++i) {
            const auto origin=origins[i];M projected;
            auto local_provenance=provenance;local_provenance.observations=samples.size();
            const auto rule=std::find_if(acquired.rules.begin(),acquired.rules.end(),[&](const auto& r){return r.origin==origin;});
            if(rule==acquired.rules.end() || !rule->admitted) throw std::invalid_argument("independently admitted bank origin required");
            local_provenance.peers=rule->forced.size();peers=add(peers,local_provenance.peers);
            const auto local=PreparedTransportPattern::compile(observations,acquired,origin,atlas,
                PreparedTransportPattern::input_domain(acquired,origin,atlas,guard),
                {coordinates.domains[i],coordinates.weights,coordinates.parameters},local_provenance,guard,&projected);
            resident=add(resident,add(local.retained_entries(),projected.nonzeros()));guard.entries(resident);
            const auto& domain=local.input_image();total=add(total,local.weights().size());guard.dimension(total);
            weights.push_back(local.weights());directions.push_back(std::move(projected));
            carrier(3+i,coordinates.domains[i],domain.cols(),storage::stable_cell_key(endpoint.source,{core::Grade{1},static_cast<std::uint32_t>(i)}));
            carrier(3+origins.size()+i,atlas[1][origin].frame,domain.rows());
            storage::TemporalTensor a;a.key=5+i;a.role=storage::TemporalTensorRole::Accession;a.input=3+i;a.output=3+origins.size()+i;
            a.interpretation="shared-bank-input-image";a.tensor=storage::temporal_matrix_tensor(domain);endpoint.tensors.push_back(std::move(a));
        }
        if((provenance.observations && provenance.observations!=samples.size()) || (provenance.peers && provenance.peers!=peers))
            throw std::invalid_argument("bank provenance population differs from training proof");
        provenance.observations=samples.size();provenance.peers=peers;
        V origin;origin.reserve(total);M projected(total,acquired.proof->evidence_kernel().cols());std::size_t at{};
        for(std::size_t i=0;i<origins.size();++i) {
            origin.insert(origin.end(),weights[i].begin(),weights[i].end());
            for(const auto& [key,q]:directions[i].entries()) {guard.step();projected.set(at+key.first,key.second,q);guard.entries(add(resident,projected.nonzeros()));}
            at=add(at,weights[i].size());
        }
        numeric::PreparedExactFrames joint(std::move(projected),guard);
        carrier(1,coordinates.weights,origin.size());carrier(2,coordinates.parameters,joint.rank());
        for(std::size_t i=0;i<selected.to_ambient[0].size();++i) {
            const auto& view=atlas[0][selected.to_ambient[0][i]];
            carrier(3+2*origins.size()+i,view.frame,view.accession.rows(),storage::stable_cell_key(endpoint.source,{core::Grade{0},static_cast<std::uint32_t>(i)}));
        }
        storage::TemporalTensor w;w.key=1;w.role=storage::TemporalTensorRole::Field;w.output=1;w.interpretation="ordered-bank-occurrence-weights";
        std::vector<numeric::SparseRationalEntry> entries;
        for(std::size_t i=0;i<origin.size();++i) if(origin[i]!=0) entries.push_back({i,origin[i]});
        w.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(origin.size()),numeric::StructuralSupport::full(origin.size()),
            numeric::SparseRationalVector(origin.size(),std::move(entries)));endpoint.tensors.push_back(std::move(w));
        storage::TemporalTensor f;f.key=2;f.role=storage::TemporalTensorRole::PhysicalRealization;f.input=2;f.output=1;
        f.interpretation="joint-observable-parameter-directions";f.tensor=storage::temporal_matrix_tensor(joint.image());endpoint.tensors.push_back(std::move(f));
        storage::TemporalTensor receipt;receipt.key=3;receipt.role=storage::TemporalTensorRole::OpaqueExact;
        receipt.interpretation="rfc.context-transport-pattern-bank.v1";entries.clear();
        for(std::size_t i=0;i<64;++i) {
            const auto byte=std::to_integer<unsigned>(i<32?provenance.source_sha256[i]:provenance.evidence_sha256[i-32]);
            if(byte) entries.push_back({i,Q{byte}});
        }
        entries.push_back({64,Q{numeric::Integer(std::to_string(provenance.observations))}});entries.push_back({65,Q{numeric::Integer(std::to_string(provenance.peers))}});
        receipt.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(66),numeric::StructuralSupport::full(66),
            numeric::SparseRationalVector(66,std::move(entries)));endpoint.tensors.push_back(std::move(receipt));
        std::sort(endpoint.carriers.begin(),endpoint.carriers.end(),[](const auto& a,const auto& b){return a.key<b.key;});
        std::sort(endpoint.tensors.begin(),endpoint.tensors.end(),[](const auto& a,const auto& b){return a.key<b.key;});
        return PreparedTransportPatternBank(std::move(endpoint),guard);
    }
    const storage::TemporalEndpoint& endpoint() const & {return state_->endpoint;}
    const V& weights() const & {return state_->weights;}
    const M& parameter_directions() const & {return state_->directions;}
    const std::vector<M>& input_images() const & {return state_->domains;}
    const TransportPatternProvenance& provenance() const & {return state_->provenance;}
    const std::vector<TransportPatternFrameLabel>& frame_labels() const & {return state_->labels;}
    const storage::TemporalEndpoint& endpoint() const &&=delete;
    const V& weights() const &&=delete;
    const M& parameter_directions() const &&=delete;
    const std::vector<M>& input_images() const &&=delete;
    const TransportPatternProvenance& provenance() const &&=delete;
    const std::vector<TransportPatternFrameLabel>& frame_labels() const &&=delete;
    std::size_t retained_entries() const noexcept {return state_->retained;}
    PreparedTransportPatternBank with_frame_labels(std::vector<TransportPatternFrameLabel> labels,G& guard) const {
        using namespace context_view_detail;auto endpoint=state_->endpoint;
        if(endpoint.tensors.size()!=state_->domains.size()+3) throw std::invalid_argument("bank frame labels are immutable");
        std::sort(labels.begin(),labels.end(),[](const auto& a,const auto& b){return a.frame<b.frame;});
        storage::temporal_codec_detail::Writer writer;writer.u64(labels.size());
        for(const auto& label:labels) {
            guard.step();if(label.name.size()>256) throw std::invalid_argument("bounded bank frame names required");
            writer.u64(label.frame);storage::temporal_codec_detail::string(writer,label.name);writer.u64(label.axes.size());
            for(const auto& name:label.axes) {guard.step();if(name.size()>256) throw std::invalid_argument("bounded bank axis names required");storage::temporal_codec_detail::string(writer,name);}
            guard.entries(add(state_->retained,writer.size()));
        }
        storage::TemporalTensor t;t.key=4;t.role=storage::TemporalTensorRole::OpaqueExact;t.interpretation="rfc.context-transport-pattern-bank.frames.v1:";
        t.interpretation.append(reinterpret_cast<const char*>(writer.bytes().data()),writer.size());
        t.tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(0),numeric::StructuralSupport::full(0));endpoint.tensors.push_back(std::move(t));
        std::sort(endpoint.tensors.begin(),endpoint.tensors.end(),[](const auto& a,const auto& b){return a.key<b.key;});
        return PreparedTransportPatternBank(std::move(endpoint),guard);
    }
    std::vector<TransportCoordinate> weight_coordinates() const {
        std::vector<TransportCoordinate> result;
        for(std::size_t p=0;p<state_->owners.size();++p)
            for(std::size_t i=0;i<state_->occurrence_at[p+1]-state_->occurrence_at[p];++i)
                for(std::size_t j=0;j<state_->domains[state_->owners[p]].cols();++j) result.push_back({p,i,j});
        return result;
    }
    TransportPatternBankResponse response(const std::vector<TransportPatternInput>& inputs,G& guard,bool shared=true) const {
        std::vector<V> coordinates;const auto map=readout(inputs,guard,shared,coordinates);
        return apply(map,state_->weights,state_->directions,std::move(coordinates),guard,shared);
    }
    TransportPatternBankResponse response_sparse(const std::vector<TransportPatternSparseInput>& inputs,G& guard,bool shared=true) const {
        std::vector<V> coordinates;const auto map=readout(inputs,guard,shared,coordinates);
        return apply(map,state_->weights,state_->directions,std::move(coordinates),guard,shared);
    }
    // One action budget covers the complete batch and its retained responses.
    std::vector<TransportPatternBankResponse> response_sparse_batch(
        const std::vector<std::vector<TransportPatternSparseInput>>& batch,G& guard,bool shared=true) const {
        using namespace context_view_detail;guard.dimension(batch.size());
        if(batch.empty()) throw std::invalid_argument("nonempty sparse bank batch required");
        std::vector<TransportPatternBankResponse> out;std::size_t retained=add(state_->retained,batch.size());
        for(const auto& inputs:batch) {
            guard.step();retained=add(retained,inputs.size());
            for(const auto& input:inputs) {guard.step();retained=add(retained,input.value.values().nonzeros());}
            guard.entries(retained);
        }
        for(const auto& inputs:batch) {
            auto response=response_sparse(inputs,guard,shared);
            retained=add(retained,add(response.origin.size(),add(response.directions.nonzeros(),response.offsets.size())));
            for(const auto& c:response.coordinates) retained=add(retained,c.size());
            guard.entries(retained);out.push_back(std::move(response));
        }
        return out;
    }
    // Restriction maps compare unscaled occurrence fields in declared interfaces.
    // Shared source cells require equal complete local fields and appear once
    // in the glued section. Signed boundary assembly is a separate response.
    // Inconsistent comparisons return an exact left-kernel witness.
    TransportPatternGluing glue(const std::vector<TransportPatternInput>& inputs,
        const std::vector<TransportPatternComparison>& comparisons,G& guard,bool shared=true) const {
        return glue_impl(inputs,comparisons,guard,shared);
    }
    TransportPatternGluing glue_sparse(const std::vector<TransportPatternSparseInput>& inputs,
        const std::vector<TransportPatternComparison>& comparisons,G& guard,bool shared=true) const {
        return glue_impl(inputs,comparisons,guard,shared);
    }
private:
    template<class Input>
    TransportPatternGluing glue_impl(const std::vector<Input>& inputs,
        const std::vector<TransportPatternComparison>& comparisons,G& guard,bool shared) const {
        using namespace context_view_detail;const auto& s=*state_;
        std::vector<V> coordinates;const auto unscaled=readout(inputs,guard,false,coordinates);
        std::set<std::size_t> selected;for(const auto& i:inputs) selected.insert(i.pattern);
        const auto& arena=s.endpoint.source.grade(core::Grade{1});
        const auto carriers=storage::temporal_endpoint_detail::carriers(s.endpoint);
        std::vector<TransportPatternComparison> conditions;
        std::vector<std::size_t> first(s.lower_at.size()-1,s.owners.size());
        TransportPatternGluing out;out.offsets.push_back(0);
        M section_map(s.lower_at.back(),s.occurrence_at.back());
        for(std::size_t p=0;p<s.owners.size();++p) if(selected.contains(s.owners[p])) {
            guard.step();const auto cell=arena.lower_index[p];const auto& carrier=*carriers.at(3+2*s.domains.size()+cell);
            if(first[cell]==s.owners.size()) {
                first[cell]=p;out.section_cells.push_back(cell);
                for(std::size_t i=0;i<carrier.domain.logical_size();++i) {guard.step();section_map.set(s.lower_at[cell]+i,s.occurrence_at[p]+i,1);}
            } else {
                conditions.push_back({"shared-cell-occurrence-"+std::to_string(p),first[cell],p,
                    carrier.coordinate_system,carrier.coordinate_system,carrier.coordinate_system,
                    M::identity(carrier.domain.logical_size()),M::identity(carrier.domain.logical_size())});
            }
            guard.entries(add(s.retained,add(unscaled.nonzeros(),section_map.nonzeros())));
        }
        conditions.insert(conditions.end(),comparisons.begin(),comparisons.end());
        std::set<std::string> names;std::map<std::uint64_t,std::size_t> frames;
        for(const auto& c:s.endpoint.carriers) frames.emplace(c.coordinate_system,c.domain.logical_size());
        for(const auto& c:conditions) {
            guard.step();guard.matrix(c.left_map);guard.matrix(c.right_map);
            if(c.name.empty() || c.name.size()>256 || !names.insert(c.name).second || c.left>=s.owners.size() || c.right>=s.owners.size() ||
                !selected.contains(s.owners[c.left]) || !selected.contains(s.owners[c.right]))
                throw std::invalid_argument("named comparisons of selected original occurrences required");
            const auto lower=[&](std::size_t p){return carriers.at(3+2*s.domains.size()+arena.lower_index[p]);};
            if(c.left_frame!=lower(c.left)->coordinate_system || c.right_frame!=lower(c.right)->coordinate_system || !c.interface_frame ||
                !c.left_map.rows() || c.left_map.rows()!=c.right_map.rows() ||
                c.left_map.cols()!=lower(c.left)->domain.logical_size() || c.right_map.cols()!=lower(c.right)->domain.logical_size())
                throw std::invalid_argument("declared bank interface carriers and complete restrictions required");
            const auto [it,inserted]=frames.emplace(c.interface_frame,c.left_map.rows());
            if(!inserted && it->second!=c.left_map.rows()) throw std::invalid_argument("bank interface frame dimensions conflict");
            out.offsets.push_back(add(out.offsets.back(),c.left_map.rows()));guard.dimension(out.offsets.back());
            out.comparison_names.push_back(c.name);
        }
        M restrictions(out.offsets.back(),s.occurrence_at.back());
        for(std::size_t i=0;i<conditions.size();++i) {
            const auto& c=conditions[i];
            for(const auto& [key,q]:c.left_map.entries()) {guard.step();restrictions.set(out.offsets[i]+key.first,s.occurrence_at[c.left]+key.second,q);}
            for(const auto& [key,q]:c.right_map.entries()) {guard.step();const auto row=out.offsets[i]+key.first,col=s.occurrence_at[c.right]+key.second;
                restrictions.set(row,col,guard.sub(restrictions.get(row,col),q));}
            guard.entries(add(s.retained,add(unscaled.nonzeros(),restrictions.nonzeros())));
        }
        const auto comparison=product(restrictions,unscaled,guard,s.retained);
        out.defect_origin=numeric::checked_apply(comparison,s.weights,guard);
        out.defect_directions=product(comparison,s.directions,guard,s.retained);
        const auto d=s.directions.cols();M augmented(out.defect_origin.size(),add(d,1));
        for(const auto& [key,q]:out.defect_directions.entries()) augmented.set(key.first,key.second,q);
        for(std::size_t i=0;i<out.defect_origin.size();++i) augmented.set(i,d,guard.sub(Q{0},out.defect_origin[i]));
        const numeric::PreparedExactFrames equations(out.defect_directions,guard),target(augmented,guard);
        out.consistent=equations.rank()==target.rank();
        out.forced=out.defect_directions.nonzeros()==0 && std::all_of(out.defect_origin.begin(),out.defect_origin.end(),[](const auto& q){return q==0;});
        if(!out.consistent) {
            const numeric::PreparedExactFrames annihilator(out.defect_directions.transpose(),guard);const auto& kernel=annihilator.kernel();
            for(std::size_t j=0;j<kernel.cols();++j) {
                V witness(kernel.rows());Q pairing;
                for(std::size_t i=0;i<witness.size();++i) {guard.step();witness[i]=kernel.get(i,j);pairing=guard.add(pairing,guard.mul(witness[i],out.defect_origin[i]));}
                if(pairing!=0) {out.contradiction=std::move(witness);break;}
            }
            if(out.contradiction.empty() || numeric::checked_transpose_apply(out.defect_directions,out.contradiction,guard)!=V(d))
                throw std::logic_error("bank context contradiction certificate");
            return out;
        }
        out.parameter_origin=V(d);out.parameter_directions=equations.kernel();
        for(std::size_t i=0;i<target.rank();++i) out.parameter_origin[target.pivots()[i]]=target.reduced().get(i,d);
        const auto residual=numeric::checked_apply(out.defect_directions,out.parameter_origin,guard);
        for(std::size_t i=0;i<residual.size();++i) if(guard.add(residual[i],out.defect_origin[i])!=0) throw std::logic_error("bank context origin certificate");
        auto weights=numeric::checked_apply(s.directions,out.parameter_origin,guard);
        for(std::size_t i=0;i<weights.size();++i) weights[i]=guard.add(weights[i],s.weights[i]);
        const auto directions=product(s.directions,out.parameter_directions,guard,s.retained);
        std::vector<V> selected_coordinates;const auto map=shared?readout(inputs,guard,true,selected_coordinates):unscaled;
        out.section=apply(product(section_map,unscaled,guard,s.retained),weights,directions,coordinates,guard,true);
        out.boundary_response=apply(map,weights,directions,std::move(coordinates),guard,shared);return out;
    }
};
} // namespace rfc::learning
