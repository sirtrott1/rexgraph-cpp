#pragma once
#include "rfc/storage/finite_cover_gluing.hpp"
#include "rfc/inference/native_local_gluing.hpp"
#include "rfc/inference/native_action_moments.hpp"

namespace rfc::inference {
struct NativeCoverInterfaceStalkRecords {
    storage::LocalStalkPartitionRecords common;
    std::array<std::vector<std::size_t>,2> block_images;
};
struct NativeCoverInterfaceRecords {
    NativeLocalFieldPatch common;
    std::size_t left{},right{};
    std::array<algebra::RetainedSourceMap,2> maps;
    std::vector<NativeCoverInterfaceStalkRecords> stalks;
};
struct NativeCoverCarrierBinding {
    NativeCarrier carrier;
    std::vector<storage::CoverStalkRange> expected_layout;
};
class PreparedNativeFiniteCoverGluing {
    struct State {
        std::vector<NativeLocalFieldPatch> patches;
        std::vector<NativeLocalFieldPatch> common;
        storage::FiniteCoverLocalFieldView view;
        std::vector<NativeCoverCarrierBinding> carriers;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    static void require(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
    explicit PreparedNativeFiniteCoverGluing(std::shared_ptr<const State> state):state_(std::move(state)){}
public:
    static PreparedNativeFiniteCoverGluing prepare(std::vector<NativeLocalFieldPatch> input,
        std::vector<storage::LocalStalkPartitionRecords> partitions,
        std::vector<NativeCoverInterfaceRecords> requests,
        numeric::ExactActionLimits limits={}){
        auto patches=std::move(input);
        require(!patches.empty(),"finite cover requires patches");
        std::set<std::string> labels;std::vector<storage::DeclaredLocalFieldBinding> bindings;
        for(const auto& p:patches){
            require(!p.label.empty() && labels.insert(p.label).second,"patch names must be unique");
            require(p.anchor.kind()==storage::NativeAccessionKind::Chain && p.anchor.support_count()==0,"patch needs empty owning source anchor");
            bindings.emplace_back(*p.anchor.endpoint(),p.contract,limits);
        }
        numeric::ExactActionGuard guard(limits);std::vector<storage::LocalStalkPartition> parts;
        for(const auto& p:partitions){require(p.chart.patch<bindings.size(),"partition patch index");for(const auto& b:p.blocks)require(bindings[p.chart.patch].endpoint().declarations && bindings[p.chart.patch].endpoint().declarations->registry.at(b.declaration.id)==b.declaration,"local block definition absent or different");parts.push_back({storage::local_stalk_chart_from_records(bindings[p.chart.patch],p.chart,guard),p.blocks});}
        std::vector<NativeLocalFieldPatch> commons;std::vector<storage::FiniteCoverInterface> interfaces;
        for(const auto& r:requests){
            require(!r.common.label.empty() && labels.insert(r.common.label).second && r.common.anchor.kind()==storage::NativeAccessionKind::Chain && r.common.anchor.support_count()==0,"common source anchor/name differs");
            storage::DeclaredLocalFieldBinding cb(*r.common.anchor.endpoint(),r.common.contract,limits);
            std::vector<storage::CoverInterfaceStalk> ss;
            for(const auto& p:r.stalks){require(p.common.chart.patch==0,"common chart side must be zero");for(const auto& b:p.common.blocks)require(cb.endpoint().declarations && cb.endpoint().declarations->registry.at(b.declaration.id)==b.declaration,"common block definition absent or different");ss.push_back({{storage::local_stalk_chart_from_records(cb,p.common.chart,guard),p.common.blocks},p.block_images});}
            interfaces.push_back({cb,r.left,r.right,r.maps,std::move(ss)});commons.push_back(r.common);
        }
        storage::FiniteCoverLocalFieldView view(std::move(bindings),std::move(parts),std::move(interfaces),limits);
        return PreparedNativeFiniteCoverGluing(std::make_shared<const State>(State{patches,commons,std::move(view),{},limits.dimension}));
    }
    // The caller explicitly declares the quotient-coordinate carrier. This creates
    // no source publication, and adopts the actual view produced by prepare().
    PreparedNativeFiniteCoverGluing bind_carriers(std::vector<NativeCoverCarrierBinding> carriers)const{
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
        return PreparedNativeFiniteCoverGluing(std::make_shared<const State>(State{pin->patches,pin->common,view,carriers,dimension_limit}));
    }
    const storage::FiniteCoverLocalFieldView& view()const &{return state_->view;}
    const storage::FiniteCoverLocalFieldView& view()const &&=delete;
    NativeFactoredAction boundary(core::Grade grade,std::optional<std::size_t> component={})const{
        const auto pin=state_;require(grade.value>0 && grade.value<pin->carriers.size(),"native glued boundary grade");
        std::vector<storage::PinnedNativeAccession> dependencies;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        for(const auto& c:pin->common)native_action_detail::append_pin(dependencies,c.anchor);
        const auto in=pin->carriers[grade.value].carrier,out=pin->carriers[grade.value-1].carrier;
        native_action_detail::append_pin(dependencies,in.publication);native_action_detail::append_pin(dependencies,out.publication);
        return NativeFactoredAction({in,out,pin->view.boundary(grade,component),std::move(dependencies),{pin},
            "native-split-finite-cover-boundary-v1",pin->dimension_limit});
    }
    NativeFieldValue glue_field(core::Grade grade,const std::vector<std::vector<storage::DeclaredFieldInput>>& inputs,
        const NativeActionAvailability& available,const runtime::ExecutionLease& lease=runtime::ExecutionLease{})const{
        const auto pin=state_;lease.check_cancelled();require(inputs.size()==pin->patches.size(),"one field input list per local patch");
        const auto carrier=pin->carriers.at(grade.value).carrier;
        std::vector<storage::PinnedNativeAccession> dependencies,roots;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        for(const auto& c:pin->common)native_action_detail::append_pin(dependencies,c.anchor);
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
        std::vector<field::SparseStalkField> locals;
        for(std::size_t p=0;p<inputs.size();++p){
            lease.check_cancelled();locals.push_back(pin->view.patches()[p].bind_downward(grade,inputs[p],limits));
            for(const auto& i:inputs[p]){
                storage::NativeAccessionRequest request;request.kind=storage::NativeAccessionKind::Field;request.subject=i.tensor;
                const auto root=pin->patches[p].anchor.access(request);native_action_detail::append_pin(roots,root);native_action_detail::append_pin(dependencies,root);
            }
        }
        auto values=pin->view.glue_fields(grade,locals,guard);lease.check_cancelled();
        runtime::admit_execution({pin->view.retained_records(),values.nonzeros(),values.nonzeros(),0,0},lease.policy());
        return NativeFieldValue({carrier,std::move(values),std::move(roots),std::move(dependencies),{pin},{"native-split-finite-cover-total-field-v1"}});
    }
};
} // namespace rfc::inference
