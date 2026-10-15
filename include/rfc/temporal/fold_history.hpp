#pragma once
#include "rfc/geometry/panel_assembly.hpp"
#include "rfc/temporal/phase_history.hpp"

namespace rfc::temporal {
struct RationalFoldOperation {
    core::IdentityKey identity{};
    std::size_t crease{};
    std::vector<std::uint32_t> moving_panels;
    numeric::RationalPhase phase;
    bool active{true};
};

// Piecewise rigid exact kinematics over the owned panel assembly. The crease
// frame follows its fixed panel; the declared moving group is rotated in world
// coordinates. Every shared source vertex must still glue after each operation.
// This does not assert collision freedom, material laws, or physical time.
class RationalFoldHistory {
    using Pose=geometry::RationalRigidFrame;
    using M=numeric::ExactMatrix;
    struct Carrier {geometry::RationalPanelAssembly assembly; ExactHistoryLimits limits;};
    struct State {
        std::shared_ptr<const Carrier> carrier;
        std::vector<RationalFoldOperation> operations;
        std::vector<std::vector<Pose>> endpoints;
    };
public:
    class Edit {
        friend class RationalFoldHistory;
        Edit(std::shared_ptr<const State> base,std::size_t position,numeric::RationalPhase phase,bool active)
            :base_(std::move(base)),position_(position),phase_(std::move(phase)),active_(active) {}
        std::shared_ptr<const State> base_;
        std::size_t position_;
        numeric::RationalPhase phase_;
        bool active_;
    };
    class Endpoint {
        friend class RationalFoldHistory;
        Endpoint(std::shared_ptr<const Carrier> carrier,std::size_t count,std::vector<Pose> poses)
            :carrier_(std::move(carrier)),count_(count),poses_(std::move(poses)) {}
    public:
        const geometry::RationalPanelAssembly& assembly() const & noexcept {return carrier_->assembly;}
        const geometry::RationalPanelAssembly& assembly() const && = delete;
        const std::vector<Pose>& poses() const & noexcept {return poses_;}
        const std::vector<Pose>& poses() const && = delete;
        std::size_t operation_count() const noexcept {return count_;}
    private:
        std::shared_ptr<const Carrier> carrier_;
        std::size_t count_;
        std::vector<Pose> poses_;
    };
    RationalFoldHistory(const RationalFoldHistory&)=default;
    RationalFoldHistory& operator=(const RationalFoldHistory&)=default;
    static RationalFoldHistory begin(geometry::RationalPanelAssembly assembly,ExactHistoryLimits limits={}) {
        history_detail::admit_source(assembly.source(),limits);
        const auto n=assembly.frames().size();
        if (machine::checked_size_mul(n,16)>limits.max_coordinate_slots)
            throw std::length_error("fold history initial coordinate budget");
        std::vector<Pose> poses(n,Pose::identity());
        for (const auto& pose:poses) history_detail::admit(pose.affine(),limits);
        auto carrier=std::make_shared<const Carrier>(Carrier{std::move(assembly),limits});
        return RationalFoldHistory(std::make_shared<const State>(State{std::move(carrier),{},{std::move(poses)}}));
    }
    const geometry::RationalPanelAssembly& assembly() const & noexcept {return state_->carrier->assembly;}
    const geometry::RationalPanelAssembly& assembly() const && = delete;
    const std::vector<RationalFoldOperation>& operations() const & noexcept {return state_->operations;}
    const std::vector<RationalFoldOperation>& operations() const && = delete;
    const std::vector<Pose>& poses() const & noexcept {return state_->endpoints.back();}
    const std::vector<Pose>& poses() const && = delete;
    const std::vector<Pose>& poses_at(std::size_t prefix) const & {return state_->endpoints.at(prefix);}
    const std::vector<Pose>& poses_at(std::size_t) const && = delete;
    bool same_carrier(const RationalFoldHistory& other) const noexcept {return state_->carrier==other.state_->carrier;}
    bool same_history(const RationalFoldHistory& other) const noexcept {return state_==other.state_;}
    Endpoint observe() const {return Endpoint(state_->carrier,state_->operations.size(),poses());}
    numeric::RationalVector vertex(std::size_t panel,std::uint32_t index) const {
        if (!assembly().panel_contains(panel,index)) throw std::invalid_argument("vertex is not in the declared panel");
        return poses().at(panel).apply(assembly().vertices().at(index));
    }
    Pose frame(std::size_t panel) const {return poses().at(panel).compose(assembly().frames().at(panel));}
    RationalFoldHistory append(RationalFoldOperation operation) const {
        const auto& limits=state_->carrier->limits;
        if (state_->operations.size()>=limits.max_steps) throw std::length_error("fold history step budget");
        const auto slots=machine::checked_size_mul(machine::checked_size_mul(state_->operations.size()+2,poses().size()),16);
        if (slots>limits.max_coordinate_slots) throw std::length_error("fold history endpoint coordinate budget");
        const auto& crease=assembly().creases().at(operation.crease);
        if (operation.moving_panels.empty()) throw std::invalid_argument("fold requires an explicit nonempty moving panel group");
        auto ordered=operation.moving_panels; std::sort(ordered.begin(),ordered.end());
        if (std::adjacent_find(ordered.begin(),ordered.end())!=ordered.end() || ordered.back()>=poses().size() ||
            std::binary_search(ordered.begin(),ordered.end(),crease.fixed_panel) ||
            !std::binary_search(ordered.begin(),ordered.end(),crease.moving_panel))
            throw std::invalid_argument("fold moving group omits the moving panel, repeats a panel or contains the fixed panel");
        if (operation.moving_panels.size()>limits.max_source_cells)
            throw std::length_error("fold moving panel group budget");
        history_detail::admit(operation.phase.real_action(),limits);
        auto next=poses();
        if (operation.active) {
            const auto world_frame=poses()[crease.fixed_panel].compose(crease.reference_frame);
            const Pose rotation(geometry::coordinate_rotation(geometry::CoordinateAxis::X,operation.phase),{0,0,0});
            const auto world_action=world_frame.compose(rotation).compose(world_frame.inverse());
            history_detail::admit(world_action.affine(),limits);
            for (const auto panel:operation.moving_panels) next[panel]=world_action.compose(next[panel]);
        }
        for (const auto& pose:next) {
            history_detail::admit(pose.affine(),limits);
            geometry::panel_detail::admit(pose.affine(),assembly().limits().max_integer_bits);
        }
        // Retained vertex identity is the gluing condition, including shared
        // vertices outside a selected crease. Cancellation never removes it.
        std::vector<numeric::RationalVector> first(assembly().vertices().size());
        std::vector<bool> seen(first.size(),false);
        for (std::size_t panel=0;panel<next.size();++panel) for (std::size_t v=0;v<first.size();++v) {
            const auto index=static_cast<std::uint32_t>(v);
            if (!assembly().panel_contains(panel,index)) continue;
            const auto value=next[panel].apply(assembly().vertices()[v]);
            for (const auto& q:value) {
                geometry::panel_detail::admit(q,limits.max_integer_bits);
                geometry::panel_detail::admit(q,assembly().limits().max_integer_bits);
            }
            if (seen[v] && value!=first[v]) throw std::invalid_argument("fold tears a shared source vertex; declare the complete moving group");
            first[v]=value; seen[v]=true;
        }
        auto state=std::make_shared<State>(*state_);
        state->operations.push_back(std::move(operation)); state->endpoints.push_back(std::move(next));
        return RationalFoldHistory(std::move(state));
    }
    RationalFoldHistory prefix(std::size_t count) const {
        if (count>state_->operations.size()) throw std::out_of_range("fold history prefix");
        auto state=std::make_shared<State>(State{state_->carrier,{},{}});
        state->operations.assign(state_->operations.begin(),state_->operations.begin()+static_cast<std::ptrdiff_t>(count));
        state->endpoints.assign(state_->endpoints.begin(),state_->endpoints.begin()+static_cast<std::ptrdiff_t>(count+1));
        return RationalFoldHistory(std::move(state));
    }
    Edit propose(std::size_t position,numeric::RationalPhase phase,bool active) const {
        (void)state_->operations.at(position); history_detail::admit(phase.real_action(),state_->carrier->limits);
        return Edit(state_,position,std::move(phase),active);
    }
    RationalFoldHistory apply(const Edit& edit) const {
        if (edit.base_!=state_) throw std::invalid_argument("fold edit base history differs");
        auto next=prefix(edit.position_);
        for (std::size_t i=edit.position_;i<state_->operations.size();++i) {
            auto operation=state_->operations[i];
            if (i==edit.position_) {operation.phase=edit.phase_; operation.active=edit.active_;}
            next=next.append(std::move(operation));
        }
        return next;
    }
    std::vector<M> actions(std::size_t panel) const {
        if (panel>=poses().size()) throw std::out_of_range("fold action panel");
        history_detail::admit_word(state_->operations.size(),4,state_->carrier->limits);
        std::vector<M> out; out.reserve(state_->operations.size());
        for (std::size_t i=0;i<state_->operations.size();++i)
            out.push_back(state_->endpoints[i+1][panel].affine().multiply(state_->endpoints[i][panel].affine().inverse()));
        return out;
    }
    std::vector<OrderedFactorInteraction> attribution(const RationalFoldHistory& edited,std::size_t panel) const {
        aligned(edited); history_detail::admit_interactions(state_->operations.size(),4,state_->carrier->limits);
        const auto old_actions=actions(panel),new_actions=edited.actions(panel);
        if (old_actions.empty()) return {};
        auto terms=finite_ordered_factor_attribution(old_actions,new_actions,FactorOrder::AppliedFirst);
        for (const auto& term:terms) history_detail::admit(term.contribution,state_->carrier->limits);
        return terms;
    }
    OperationWordDefect defect(const RationalFoldHistory& edited,std::size_t panel) const {
        aligned(edited); const auto old_actions=actions(panel),new_actions=edited.actions(panel);
        if (old_actions.empty()) return {M(4,4),M(4,4),{},{}};
        auto result=finite_word_defect(OperationWord(old_actions),OperationWord(new_actions),std::vector<M>(old_actions.size()+1,M::identity(4)));
        history_detail::admit(result.direct,state_->carrier->limits); history_detail::admit(result.expanded,state_->carrier->limits);
        for (const auto& term:result.contributions) history_detail::admit(term,state_->carrier->limits);
        return result;
    }
private:
    explicit RationalFoldHistory(std::shared_ptr<const State> state):state_(std::move(state)) {}
    void aligned(const RationalFoldHistory& other) const {
        if (!same_carrier(other) || state_->operations.size()!=other.state_->operations.size())
            throw std::invalid_argument("fold attribution carrier or length differs");
        for (std::size_t i=0;i<state_->operations.size();++i) {
            const auto& a=state_->operations[i]; const auto& b=other.state_->operations[i];
            if (a.identity!=b.identity || a.crease!=b.crease || a.moving_panels!=b.moving_panels)
                throw std::invalid_argument("fold attribution operation identity or interface differs");
        }
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::temporal
