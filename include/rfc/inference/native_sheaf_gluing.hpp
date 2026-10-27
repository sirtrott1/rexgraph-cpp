#pragma once
#include "rfc/storage/sheaf_cover_gluing.hpp"
#include "rfc/inference/native_local_gluing.hpp"
#include "rfc/inference/native_action_moments.hpp"

namespace rfc::inference {
// A selector is admission data, not authority restored from serialized bytes.
struct ObservationMapRecord {
    std::uint64_t key{};
    storage::TemporalCarrier input,output;
    storage::TemporalTensorRole role{storage::TemporalTensorRole::OpaqueExact};
    storage::TemporalTensorOwner owner;
};
inline numeric::ExactMatrix observation_map_from_record(const storage::TemporalEndpoint& e,
    const ObservationMapRecord& r,numeric::ExactActionGuard& g){
    const auto require=[](bool b,const char* m){if(!b)throw std::invalid_argument(m);};
    const auto cs=storage::temporal_endpoint_detail::carriers(e);const auto ts=storage::temporal_endpoint_detail::tensors(e);
    require(cs.contains(r.input.key) && *cs.at(r.input.key)==r.input && cs.contains(r.output.key) && *cs.at(r.output.key)==r.output,"observation map carrier differs");
    require(ts.contains(r.key),"observation map record absent");const auto& t=*ts.at(r.key);
    require(t.role==r.role && t.owner==r.owner && t.input==r.input.key && t.output==r.output.key,"observation map owner/role/direction differs");
    require(t.tensor.support().count()==t.tensor.domain().logical_size(),"observation map requires total structural support");
    g.dimension(r.input.domain.logical_size());g.dimension(r.output.domain.logical_size());g.entries(t.tensor.values().nonzeros());
    auto m=storage::temporal_tensor_matrix(t);g.matrix(m);return m;
}
struct NativeObservationInterfaceStalkRecords {
    storage::DeclaredStalkBinding common;
    std::array<ObservationMapRecord,2> restrictions; // local -> common observations
};
struct NativeObservationInterfaceRecords {
    NativeLocalFieldPatch common;
    std::size_t left{},right{};
    std::array<algebra::RetainedSourceMap,2> maps;
    std::vector<NativeObservationInterfaceStalkRecords> stalks;
};
struct NativeStalkObservationRecords {
    std::vector<storage::ObservationStalkOrigin> origins;
    storage::ObservationCoordinateSpace target;
    ObservationMapRecord accession,reconstruction,compatibility_lift;
};
struct NativeObservationCarrierBinding {
    NativeCarrier carrier;
    std::vector<storage::ObservationStalkRange> expected_layout;
};
class PreparedNativeSheafGluing {
    friend class PreparedNativeFiniteSheafSections;
    struct State {
        std::vector<NativeLocalFieldPatch> patches;
        std::vector<NativeLocalFieldPatch> common;
        storage::PinnedNativeAccession certificate;
        storage::SheafCoverLocalFieldView view;
        std::vector<NativeObservationCarrierBinding> carriers;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    static void require(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
    explicit PreparedNativeSheafGluing(std::shared_ptr<const State> state):state_(std::move(state)){}
public:
    static PreparedNativeSheafGluing prepare(std::vector<NativeLocalFieldPatch> patches,
        std::vector<NativeObservationInterfaceRecords> requests,
        storage::PinnedNativeAccession certificate,std::vector<NativeStalkObservationRecords> records,
        numeric::ExactActionLimits limits={}){
        require(!patches.empty(),"sheaf cover requires patches");
        require(certificate.kind()==storage::NativeAccessionKind::Chain && certificate.support_count()==0,"observation witness requires an owning source anchor");
        std::set<std::string> labels;std::vector<storage::DeclaredLocalFieldBinding> bindings;
        for(const auto& p:patches){
            require(!p.label.empty() && labels.insert(p.label).second,"patch names must be unique");
            require(p.anchor.kind()==storage::NativeAccessionKind::Chain && p.anchor.support_count()==0,"patch needs empty owning source anchor");
            bindings.emplace_back(*p.anchor.endpoint(),p.contract,limits);
        }
        numeric::ExactActionGuard guard(limits);
        std::vector<NativeLocalFieldPatch> commons;std::vector<storage::ObservationFieldInterface> interfaces;
        for(const auto& r:requests){
            require(r.left<patches.size() && r.right<patches.size() && r.left!=r.right,"observation interface patch indices");
            require(!r.common.label.empty() && labels.insert(r.common.label).second && r.common.anchor.kind()==storage::NativeAccessionKind::Chain && r.common.anchor.support_count()==0,"common source anchor/name differs");
            storage::DeclaredLocalFieldBinding cb(*r.common.anchor.endpoint(),r.common.contract,limits);
            std::vector<storage::ObservationInterfaceStalk> ss;
            for(const auto& p:r.stalks){
                const auto cell=storage::sheaf_cover_detail::matched(cb,p.common);
                const std::array<std::size_t,2> ps{r.left,r.right};std::array<numeric::ExactMatrix,2> maps;
                for(std::size_t side=0;side<2;++side){
                    const auto& b=bindings[ps[side]];
                    const auto& local=storage::sheaf_cover_detail::stalk(b,{cell.grade,r.maps[side].cells.at(cell.grade.value).at(cell.index)});
                    const auto& o=p.restrictions[side];
                    require(b.endpoint().declarations && b.endpoint().declarations->registry.at(p.common.declaration.id)==p.common.declaration,"common observation type not retained locally");
                    require(o.output.kind==storage::TemporalCarrierKind::Explicit && o.output.coordinate_system==p.common.carrier.coordinate_system && o.output.domain==p.common.carrier.domain && o.input==local.carrier,"observation restriction endpoints differ from local/common stalks");
                    require(o.role==storage::TemporalTensorRole::Observation,"interface observation role");
                    require(o.owner.kind==storage::TemporalOwnerKind::Cell && o.owner.cell==*local.carrier.cell,"observation interface owner");
                    maps[side]=observation_map_from_record(b.endpoint(),o,guard);
                }
                ss.push_back({p.common,std::move(maps)});
            }
            interfaces.push_back({cb,r.left,r.right,r.maps,std::move(ss)});commons.push_back(r.common);
        }
        const auto e=certificate.endpoint();std::vector<storage::ObservationCellWitness> witnesses;
        for(const auto& r:records){
            require(e->declarations && e->declarations->registry.at(r.target.declaration.id)==r.target.declaration,"observation target definition differs");
            require(r.target.declaration.kind==composite::DeclarationKind::RationalTensor && r.target.declaration.axes.size()==1,"observation target type");
            require(r.accession.input.kind==storage::TemporalCarrierKind::Explicit && r.accession.input.coordinate_system==r.target.coordinates && r.accession.input.domain==numeric::IndexDomain::linear(r.target.declaration.axes.at(0).extent),"observation target coordinates differ");
            const auto load=[&](const ObservationMapRecord& m,storage::TemporalTensorRole role){
                require(m.role==role && m.owner.kind==storage::TemporalOwnerKind::Global && m.input.kind==storage::TemporalCarrierKind::Explicit && m.output.kind==storage::TemporalCarrierKind::Explicit,"observation witness map profile");
                return observation_map_from_record(*e,m,guard);
            };
            require(r.reconstruction.input==r.accession.output && r.reconstruction.output==r.accession.input && r.compatibility_lift.output==r.accession.output,"observation witness carrier correspondence");
            for(const auto& origin:r.origins)
                require(e->declarations->registry.at(origin.local.declaration.id)==origin.local.declaration,"local observation coordinate definition not retained");
            witnesses.push_back({r.origins,{r.target,load(r.accession,storage::TemporalTensorRole::Observation),load(r.reconstruction,storage::TemporalTensorRole::Reconstruction),load(r.compatibility_lift,storage::TemporalTensorRole::Reconstruction)}});
        }
        storage::SheafCoverLocalFieldView view(std::move(bindings),std::move(witnesses),std::move(interfaces),limits);
        return PreparedNativeSheafGluing(std::make_shared<const State>(State{patches,commons,certificate,std::move(view),{},limits.dimension}));
    }
    // The caller explicitly declares the compatible-observation carrier. This creates
    // no source publication, and adopts the actual view produced by prepare().
    PreparedNativeSheafGluing bind_carriers(std::vector<NativeObservationCarrierBinding> carriers)const{
        const auto pin=state_;const auto& view=pin->view;const auto dimension_limit=pin->dimension_limit;
        require(carriers.size()==view.grade_count(),"glued grade carrier inventory");
        for(std::size_t k=0;k<carriers.size();++k){
            const auto& c=carriers[k].carrier;
            require(c.same(NativeCarrier::bind(c.publication,c.key)),"foreign glued carrier declaration");
            require(c.declaration->kind==storage::TemporalCarrierKind::Explicit && c.declaration->coordinate_system!=0 &&
                    c.size()==view.dimension({static_cast<std::uint16_t>(k)}) && c.size()<=dimension_limit,
                    "glued carrier kind/grade/coordinate identity/extent");
            require(carriers[k].expected_layout==view.layout({static_cast<std::uint16_t>(k)}),"glued carrier layout must match every typed local range");
        }
        return PreparedNativeSheafGluing(std::make_shared<const State>(State{pin->patches,pin->common,pin->certificate,view,carriers,dimension_limit}));
    }
    const storage::SheafCoverLocalFieldView& view()const &{return state_->view;}
    const storage::SheafCoverLocalFieldView& view()const &&=delete;
    NativeFactoredAction coboundary(core::Grade grade,std::optional<std::size_t> component={})const{
        const auto pin=state_;require(std::size_t(grade.value)+1<pin->carriers.size(),"native glued coboundary grade");
        std::vector<storage::PinnedNativeAccession> dependencies;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        for(const auto& c:pin->common)native_action_detail::append_pin(dependencies,c.anchor);
        native_action_detail::append_pin(dependencies,pin->certificate);
        const auto in=pin->carriers[grade.value].carrier,out=pin->carriers[grade.value+1].carrier;
        native_action_detail::append_pin(dependencies,in.publication);native_action_detail::append_pin(dependencies,out.publication);
        return NativeFactoredAction({in,out,pin->view.coboundary(grade,component),std::move(dependencies),{pin},
            "native-declared-sheaf-cover-coboundary-v1",pin->dimension_limit});
    }
    NativeFieldValue glue_field(core::Grade grade,const std::vector<std::vector<storage::DeclaredFieldInput>>& inputs,
        const NativeActionAvailability& available,const runtime::ExecutionLease& lease=runtime::ExecutionLease{})const{
        const auto pin=state_;lease.check_cancelled();require(inputs.size()==pin->patches.size(),"one field input list per local patch");
        const auto carrier=pin->carriers.at(grade.value).carrier;
        std::vector<storage::PinnedNativeAccession> dependencies,roots;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        for(const auto& c:pin->common)native_action_detail::append_pin(dependencies,c.anchor);
        native_action_detail::append_pin(dependencies,pin->certificate);
        native_action_detail::append_pin(dependencies,carrier.publication);
        for(const auto& d:dependencies)native_action_detail::available(d,available);
        const auto limits=native_action_detail::limits(lease,dependencies,pin->dimension_limit);
        numeric::ExactActionGuard guard(limits,[&lease]{lease.check_cancelled();});guard.entries(pin->view.retained_records());
        // Bound the aggregate selected numerical payload before constructing all
        // local copies. Local typed admission and quotient checks remain separate
        // bounded stages; this is not a process-wide memory quota.
        std::size_t payload=pin->view.retained_records();
        for(std::size_t p=0;p<inputs.size();++p)for(const auto& i:inputs[p]){
            guard.step();const auto endpoint=pin->patches[p].anchor.endpoint();
            const auto found=std::find_if(endpoint->tensors.begin(),endpoint->tensors.end(),[&](const auto& t){return t.key==i.tensor;});
            require(found!=endpoint->tensors.end(),"selected local input field missing");
            payload=machine::checked_size_add(payload,machine::checked_size_mul(machine::checked_size_add(1,found->tensor.values().nonzeros()),4));guard.entries(payload);
        }
        runtime::admit_execution({pin->view.retained_records(),payload,payload,0,0},lease.policy());
        std::vector<local::SparseSheafCochain> locals;
        for(std::size_t p=0;p<inputs.size();++p){
            lease.check_cancelled();locals.push_back(pin->view.patches()[p].bind_upward(grade,inputs[p],limits));
            for(const auto& i:inputs[p]){
                storage::NativeAccessionRequest request;request.kind=storage::NativeAccessionKind::Field;request.subject=i.tensor;
                const auto root=pin->patches[p].anchor.access(request);native_action_detail::append_pin(roots,root);native_action_detail::append_pin(dependencies,root);
            }
        }
        auto values=pin->view.glue_fields(grade,locals,guard);lease.check_cancelled();
        runtime::admit_execution({pin->view.retained_records(),values.nonzeros(),values.nonzeros(),0,0},lease.policy());
        return NativeFieldValue({carrier,std::move(values),std::move(roots),std::move(dependencies),{pin},{"native-declared-sheaf-cover-cochain-v1"}});
    }
};
} // namespace rfc::inference
