#pragma once
#include "rfc/storage/glued_local_fields.hpp"
#include "rfc/inference/native_action_moments.hpp"

namespace rfc::inference {
struct NativeLocalFieldPatch {
    std::string label;
    storage::PinnedNativeAccession anchor;
    storage::DeclaredLocalFieldContract contract;
};
// An explicit correspondence between the quotient's ordered, typed cell ranges
// and an actual stored Explicit carrier. A matching dimension alone is insufficient.
struct NativeGluedCarrierBinding {
    NativeCarrier carrier;
    std::vector<storage::GluedStalkRange> expected_layout;
};
class PreparedNativeLocalGluing {
    struct State {
        std::vector<NativeLocalFieldPatch> patches;
        storage::GluedLocalFieldView view;
        std::vector<NativeGluedCarrierBinding> carriers;
        std::size_t dimension_limit;
    };
    std::shared_ptr<const State> state_;
    static void require(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
public:
    explicit PreparedNativeLocalGluing(std::shared_ptr<const State> state):state_(std::move(state)){}
public:
    static PreparedNativeLocalGluing prepare(std::vector<NativeLocalFieldPatch> input,
        std::vector<storage::LocalFieldInterface> interfaces,numeric::ExactActionLimits limits={}){
        return prepare_with_charts(std::move(input),std::move(interfaces),{},limits);
    }
    static PreparedNativeLocalGluing prepare_with_charts(std::vector<NativeLocalFieldPatch> input,
        std::vector<storage::LocalFieldInterface> interfaces,std::vector<storage::LocalStalkChartRecords> requests,
        numeric::ExactActionLimits limits={}){
        auto patches=input; // Detach moved-in mutable declaration buffers.
        std::set<std::string> labels;std::vector<storage::DeclaredLocalFieldBinding> bindings;
        for(const auto& p:patches){
            require(!p.label.empty() && labels.insert(p.label).second,"local patch names must be unique");
            require(p.anchor.kind()==storage::NativeAccessionKind::Chain && p.anchor.support_count()==0,"local patch needs an empty owning source anchor");
            bindings.emplace_back(*p.anchor.endpoint(),p.contract,limits);
        }
        numeric::ExactActionGuard guard(limits);std::vector<storage::LocalStalkChart> charts;
        for(const auto& r:requests){
            require(r.patch<bindings.size(),"native chart patch missing");
            charts.push_back(storage::local_stalk_chart_from_records(bindings[r.patch],r,guard));
        }
        storage::GluedLocalFieldView view(std::move(bindings),std::move(interfaces),std::move(charts),limits);
        return PreparedNativeLocalGluing(std::make_shared<const State>(State{std::move(patches),std::move(view),{},limits.dimension}));
    }
    // The caller explicitly declares the quotient-coordinate carrier. This creates
    // no source publication, and adopts the actual view produced by prepare().
    PreparedNativeLocalGluing bind_carriers(std::vector<NativeGluedCarrierBinding> carriers)const{
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
        return PreparedNativeLocalGluing(std::make_shared<const State>(State{pin->patches,view,carriers,dimension_limit}));
    }
    const storage::GluedLocalFieldView& view()const &{return state_->view;}
    const storage::GluedLocalFieldView& view()const &&=delete;
    NativeFactoredAction boundary(core::Grade grade,std::optional<std::size_t> component={})const{
        const auto pin=state_;require(grade.value>0 && grade.value<pin->carriers.size(),"native glued boundary grade");
        std::vector<storage::PinnedNativeAccession> dependencies;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
        const auto in=pin->carriers[grade.value].carrier,out=pin->carriers[grade.value-1].carrier;
        native_action_detail::append_pin(dependencies,in.publication);native_action_detail::append_pin(dependencies,out.publication);
        return NativeFactoredAction({in,out,pin->view.boundary(grade,component),std::move(dependencies),{pin},
            pin->view.charts().empty()?"native-coordinate-identical-glued-boundary-v1":"native-chart-glued-boundary-v1",pin->dimension_limit});
    }
    NativeFieldValue glue_field(core::Grade grade,const std::vector<std::vector<storage::DeclaredFieldInput>>& inputs,
        const NativeActionAvailability& available,const runtime::ExecutionLease& lease=runtime::ExecutionLease{})const{
        const auto pin=state_;lease.check_cancelled();require(inputs.size()==pin->patches.size(),"one field input list per local patch");
        const auto carrier=pin->carriers.at(grade.value).carrier;
        std::vector<storage::PinnedNativeAccession> dependencies,roots;
        for(const auto& p:pin->patches)native_action_detail::append_pin(dependencies,p.anchor);
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
        return NativeFieldValue({carrier,std::move(values),std::move(roots),std::move(dependencies),{pin},{"native-glued-total-stalk-field-v1"}});
    }
};
} // namespace rfc::inference
