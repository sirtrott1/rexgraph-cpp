#pragma once
#include "rfc/storage/injective_local_gluing.hpp"
#include "rfc/inference/native_local_gluing.hpp"
#include "rfc/inference/native_action_moments.hpp"

namespace rfc::inference {
struct NativeInjectiveCarrierBinding {
    NativeCarrier carrier;
    std::vector<storage::InjectiveStalkRange> expected_layout;
};
class PreparedNativeInjectiveGluing {
    struct State {
        std::vector<NativeLocalFieldPatch> patches;
        NativeLocalFieldPatch common;
        storage::InjectiveLocalFieldView view;
        std::vector<NativeInjectiveCarrierBinding> carriers;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    static void require(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
public:
    explicit PreparedNativeInjectiveGluing(std::shared_ptr<const State> state):state_(std::move(state)){}
public:
    static PreparedNativeInjectiveGluing prepare(NativeLocalFieldPatch common,
        std::vector<NativeLocalFieldPatch> input,
        std::array<algebra::RetainedSourceMap,2> maps,
        std::vector<storage::InjectiveStalkInterfaceRecords> requests,
        numeric::ExactActionLimits limits={}){
        auto patches=input;
        require(patches.size()==2,"split-injective profile requires two patches");
        std::set<std::string> labels;std::vector<storage::DeclaredLocalFieldBinding> bindings;
        for(const auto& p:patches){
            require(!p.label.empty() && labels.insert(p.label).second,"local patch names must be unique");
            require(p.anchor.kind()==storage::NativeAccessionKind::Chain && p.anchor.support_count()==0,"local patch needs empty owning source anchor");
            bindings.emplace_back(*p.anchor.endpoint(),p.contract,limits);
        }
        require(!common.label.empty() && labels.insert(common.label).second && common.anchor.kind()==storage::NativeAccessionKind::Chain && common.anchor.support_count()==0,"common source anchor/name differs");
        storage::DeclaredLocalFieldBinding cb(*common.anchor.endpoint(),common.contract,limits);
        numeric::ExactActionGuard guard(limits);std::vector<storage::InjectiveStalkInterface> ds;
        for(const auto& r:requests){
            require(r.split[0].patch==0 && r.split[1].patch==1,"split side identities differ");
            ds.push_back({r.common,{storage::local_stalk_chart_from_records(bindings[0],r.split[0],guard),storage::local_stalk_chart_from_records(bindings[1],r.split[1],guard)}});
        }
        storage::InjectiveLocalFieldView view(cb,{bindings[0],bindings[1]},maps,std::move(ds),limits);
        return PreparedNativeInjectiveGluing(std::make_shared<const State>(State{patches,common,std::move(view),{},limits.dimension}));
    }
    // The caller explicitly declares the quotient-coordinate carrier. This creates
    // no source publication, and adopts the actual view produced by prepare().
    PreparedNativeInjectiveGluing bind_carriers(std::vector<NativeInjectiveCarrierBinding> carriers)const{
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
        return PreparedNativeInjectiveGluing(std::make_shared<const State>(State{pin->patches,pin->common,view,carriers,dimension_limit}));
    }
    const storage::InjectiveLocalFieldView& view()const &{return state_->view;}
    const storage::InjectiveLocalFieldView& view()const &&=delete;
    NativeFactoredAction boundary(core::Grade grade,std::optional<std::size_t> component={})const{
        const auto pin=state_;require(grade.value>0 && grade.value<pin->carriers.size(),"native glued boundary grade");
        std::vector<storage::PinnedNativeAccession> dependencies;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        native_action_detail::append_pin(dependencies,pin->common.anchor);
        const auto in=pin->carriers[grade.value].carrier,out=pin->carriers[grade.value-1].carrier;
        native_action_detail::append_pin(dependencies,in.publication);native_action_detail::append_pin(dependencies,out.publication);
        return NativeFactoredAction({in,out,pin->view.boundary(grade,component),std::move(dependencies),{pin},
            "native-split-injective-boundary-v1",pin->dimension_limit});
    }
    NativeFieldValue glue_field(core::Grade grade,const std::vector<std::vector<storage::DeclaredFieldInput>>& inputs,
        const NativeActionAvailability& available,const runtime::ExecutionLease& lease=runtime::ExecutionLease{})const{
        const auto pin=state_;lease.check_cancelled();require(inputs.size()==pin->patches.size(),"one field input list per local patch");
        const auto carrier=pin->carriers.at(grade.value).carrier;
        std::vector<storage::PinnedNativeAccession> dependencies,roots;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        native_action_detail::append_pin(dependencies,pin->common.anchor);
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
        auto values=pin->view.glue_fields(grade,{locals[0],locals[1]},guard);lease.check_cancelled();
        runtime::admit_execution({pin->view.retained_records(),values.nonzeros(),values.nonzeros(),0,0},lease.policy());
        return NativeFieldValue({carrier,std::move(values),std::move(roots),std::move(dependencies),{pin},{"native-split-injective-total-field-v1"}});
    }
};
} // namespace rfc::inference
