#pragma once
#include "rfc/local/covering_pullback.hpp"
#include "rfc/topology/covering_walk.hpp"
#include "rfc/inference/native_gluing.hpp"

namespace rfc::inference {
struct NativeCoveringAvailability {runtime::WorkAvailability cover,base;};
struct NativeCoveringRequest {
    storage::PinnedNativeAccession cover,base;
    std::uint64_t key{},revision{};
    NativeGluingSourceMap projection;
    std::size_t dimension_limit{4096};
};
struct NativeCoveringFlagStep {storage::StableOccurrenceKey occurrence;bool upward{};};
struct NativeCoveringWalkRequest {storage::StableCellKey start;std::vector<NativeCoveringFlagStep> steps;};
struct NativeCoveringEdgeStep {storage::StableCellKey relation;bool forward{true};};
enum class NativeCoveringAction {DeclaredChain,NativeChain,Section,Boundary,Pushforward,Transfer,Pullback,Descent,Walk,PairwiseWalk,Lift,Monodromy,Deck};
struct NativeCoveringProvenance {
    NativeCoveringAction action;
    std::uint64_t key{},revision{};
    std::array<std::uint64_t,2> sources,namespaces,sequences;
    NativeCoveringAvailability preparation_availability,execution_availability;
    runtime::ExecutionRequirements requirements;numeric::ExactActionLimits limits;
    std::size_t preparation_steps{},execution_steps{};
    std::string preparation_policy,execution_policy;bool fallback{};
};
namespace native_covering_detail {
using native_gluing_detail::Pin;using native_gluing_detail::Guard;
using native_gluing_detail::requirements;using native_gluing_detail::add;using native_gluing_detail::mul;
using native_gluing_detail::require;using native_gluing_detail::grade;
using Channel=local::CoveringChannel;using Variance=local::CoveringVariance;
struct State {
    NativeCoveringRequest request;local::OwnedRelationalCovering covering;
    NativeCoveringAvailability preparation;std::size_t steps;std::string policy;bool fallback;
};
inline void available(const State& s,NativeCoveringAvailability a) {
    for(const auto& w:{s.preparation,a}){
        runtime::admit_work_availability(s.request.cover.source_namespace(),s.request.cover.sequence(),w.cover);
        runtime::admit_work_availability(s.request.base.source_namespace(),s.request.base.sequence(),w.base);
    }
}
inline std::array<Pin,2> pins(const State& s){return {s.request.cover,s.request.base};}
inline numeric::ExactActionLimits limits(const State& s,const runtime::ExecutionLease& lease){return native_gluing_detail::limits(lease,pins(s),s.request.dimension_limit);}
inline NativeCoveringProvenance report(const State& s,NativeCoveringAction action,NativeCoveringAvailability a,
    const runtime::ExecutionLease& lease,const Guard& guard,runtime::ExecutionRequirements r) {
    return {action,s.request.key,s.request.revision,{s.request.projection.from,s.request.projection.to},
        {s.request.cover.source_namespace(),s.request.base.source_namespace()},{s.request.cover.sequence(),s.request.base.sequence()},s.preparation,a,r,
        guard.limits(),s.steps,guard.steps(),s.policy,lease.policy().name,s.fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu};
}
inline NativeCoveringWalkRequest request(const topology::OwnedCoveringWalk& walk) {
    const auto owner=walk.covering();const auto& c=owner.source(walk.channel());NativeCoveringWalkRequest r{storage::stable_cell_key(c,walk.start()),{}};
    for(const auto& step:walk.steps())r.steps.push_back({{step.upper_grade,c.occurrence_identity(step.upper_grade,step.occurrence)},step.upward});
    return r;
}
}
class DerivedNativeCoveringField;
class DerivedNativeCoveringWalk;
class NativeCoveringLift;
class NativeCoveringMonodromy;
class NativeCoveringDeck;
class PreparedNativeCoveringPullback;
class PreparedNativeCovering {
public:
    using Values=local::OwnedRelationalCovering::Values;
    static PreparedNativeCovering prepare(NativeCoveringRequest input,NativeCoveringAvailability a,runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_covering_detail;auto r=input;lease.check_cancelled();
        require(r.key && r.revision && r.dimension_limit && r.projection.key && r.projection.from && r.projection.to && r.projection.from!=r.projection.to,"native covering needs complete declared source identities");
        for(const auto* p:{&r.cover,&r.base})require(p->kind()==storage::NativeAccessionKind::Chain && !p->support_count(),"covering needs actual empty owning Chain anchors");
        runtime::admit_work_availability(r.cover.source_namespace(),r.cover.sequence(),a.cover);runtime::admit_work_availability(r.base.source_namespace(),r.base.sequence(),a.base);
        const auto w=add(algebra::owned_gluing_detail::words(r.cover.endpoint()->source),add(algebra::owned_gluing_detail::words(r.base.endpoint()->source),add(r.projection.cells.size(),r.projection.occurrences.size()))),envelope=add(1,mul(w,64));
        runtime::admit_execution(requirements(w,envelope),lease.policy());const std::array<Pin,2> anchors{r.cover,r.base};
        Guard guard(native_gluing_detail::limits(lease,anchors,r.dimension_limit),[&lease]{lease.check_cancelled();});guard.entries(envelope);
        algebra::owned_gluing_detail::source(r.cover.endpoint()->source,guard);algebra::owned_gluing_detail::source(r.base.endpoint()->source,guard);
        auto map=native_gluing_detail::indexed(r.cover.endpoint()->source,r.base.endpoint()->source,r.projection.cells,r.projection.occurrences,guard);
        auto cover=local::OwnedRelationalCovering::prepare(r.cover.endpoint()->source,r.base.endpoint()->source,std::move(map),r.key,r.revision,guard);
        lease.check_cancelled();return PreparedNativeCovering(std::make_shared<const State>(State{std::move(r),std::move(cover),a,guard.steps(),lease.policy().name,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    local::OwnedRelationalCovering numeric_plan() const{return state_->covering;}
    const NativeCoveringRequest& declaration() const &{return state_->request;} const NativeCoveringRequest& declaration() const &&=delete;
    storage::PinnedNativeAccession anchor(local::CoveringChannel channel) const {
        native_covering_detail::require(channel==local::CoveringChannel::Cover || channel==local::CoveringChannel::Base,"unknown native covering channel");return channel==local::CoveringChannel::Cover?state_->request.cover:state_->request.base;
    }
    NativeCoveringAvailability preparation_availability() const{return state_->preparation;}
    bool same_owner(const PreparedNativeCovering& other) const{return state_==other.state_;}
    DerivedNativeCoveringField chain(local::CoveringChannel,core::Grade,Values,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringField chain(local::CoveringChannel,storage::PinnedNativeAccession,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringField section(local::CoveringChannel,std::uint64_t,core::Grade,Values,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringField boundary(DerivedNativeCoveringField,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringField pushforward(DerivedNativeCoveringField,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringField transfer(DerivedNativeCoveringField,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringField pullback(DerivedNativeCoveringField,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringField descend(DerivedNativeCoveringField,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringWalk walk(NativeCoveringWalkRequest,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringWalk pairwise_walk(storage::StableCellKey,std::vector<NativeCoveringEdgeStep>,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeCoveringWalk prefix(DerivedNativeCoveringWalk,std::size_t,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeCoveringLift lift(DerivedNativeCoveringWalk,storage::StableCellKey,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeCoveringMonodromy monodromy(DerivedNativeCoveringWalk,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeCoveringDeck deck(NativeGluingSourceMap,NativeCoveringAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
private:
    friend class PreparedNativeCoveringPullback;
    explicit PreparedNativeCovering(std::shared_ptr<const native_covering_detail::State> s):state_(std::move(s)){}
    DerivedNativeCoveringField field(local::CoveringChannel,local::CoveringVariance,std::uint64_t,core::Grade,Values,NativeCoveringAvailability,runtime::ExecutionLease) const;
    DerivedNativeCoveringField transform(DerivedNativeCoveringField,NativeCoveringAction,NativeCoveringAvailability,runtime::ExecutionLease) const;
    std::shared_ptr<const native_covering_detail::State> state_;
};
class DerivedNativeCoveringField {
public:
    PreparedNativeCovering plan() const{return owner_;}
    const local::OwnedCoveringField& field() const &{return field_;} const local::OwnedCoveringField& field() const &&=delete;
    const NativeCoveringProvenance& provenance() const &{return report_;} const NativeCoveringProvenance& provenance() const &&=delete;
    std::shared_ptr<const DerivedNativeCoveringField> input() const{return input_;}
    std::optional<storage::PinnedNativeAccession> native_input() const{return native_;}
private:
    friend class PreparedNativeCovering;
    DerivedNativeCoveringField(PreparedNativeCovering owner,local::OwnedCoveringField field,NativeCoveringProvenance report,
        std::shared_ptr<const DerivedNativeCoveringField> input={},std::optional<storage::PinnedNativeAccession> native={})
        :owner_(std::move(owner)),field_(std::move(field)),report_(std::move(report)),input_(std::move(input)),native_(std::move(native)){}
    PreparedNativeCovering owner_;local::OwnedCoveringField field_;NativeCoveringProvenance report_;
    std::shared_ptr<const DerivedNativeCoveringField> input_;std::optional<storage::PinnedNativeAccession> native_;
};
inline DerivedNativeCoveringField PreparedNativeCovering::field(local::CoveringChannel channel,local::CoveringVariance variance,std::uint64_t id,
    core::Grade g,Values input,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;auto values=input;lease.check_cancelled();available(*pin,a);
    const auto required=requirements(pin->covering.retained_words(),pin->covering.action_envelope(values.size()));runtime::admit_execution(required,lease.policy());
    Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});auto out=variance==Variance::Chain?pin->covering.chain(channel,g,std::move(values),guard):pin->covering.section(channel,id,g,std::move(values),guard);
    lease.check_cancelled();return {PreparedNativeCovering(pin),std::move(out),report(*pin,variance==Variance::Chain?NativeCoveringAction::DeclaredChain:NativeCoveringAction::Section,a,lease,guard,required)};
}
inline DerivedNativeCoveringField PreparedNativeCovering::chain(local::CoveringChannel c,core::Grade g,Values v,NativeCoveringAvailability a,runtime::ExecutionLease lease) const{return field(c,local::CoveringVariance::Chain,0,g,std::move(v),a,std::move(lease));}
inline DerivedNativeCoveringField PreparedNativeCovering::section(local::CoveringChannel c,std::uint64_t id,core::Grade g,Values v,NativeCoveringAvailability a,runtime::ExecutionLease lease) const{return field(c,local::CoveringVariance::ScalarSection,id,g,std::move(v),a,std::move(lease));}
inline DerivedNativeCoveringField PreparedNativeCovering::chain(local::CoveringChannel channel,storage::PinnedNativeAccession input,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;const auto fixed=PreparedNativeCovering(pin);lease.check_cancelled();available(*pin,a);const auto anchor=fixed.anchor(channel);
    require(input.kind()==storage::NativeAccessionKind::Chain && input.endpoint()==anchor.endpoint() && input.same_execution_authority(anchor),"covering Chain needs its actual channel publication and execution authority");
    const auto required=requirements(pin->covering.retained_words(),pin->covering.action_envelope(input.support_count()));runtime::admit_execution(required,lease.policy());
    const std::array<Pin,3> anchors{pin->request.cover,pin->request.base,input};Guard guard(native_gluing_detail::limits(lease,anchors,pin->request.dimension_limit),[&lease]{lease.check_cancelled();});pin->covering.validate(guard);
    const auto descriptor=input.descriptor();const auto g=descriptor.request().grade;storage::StableSourceIndex index(anchor.endpoint()->source);Values values;
    input.for_each_cell([&](auto key){guard.step();const auto cell=index.cell({g,key});const auto q=input.state(key);require(cell && q,"native covering Chain support changed");guard.value(*q);values.emplace(cell->index,*q);});
    auto out=pin->covering.chain(channel,g,std::move(values),guard);lease.check_cancelled();return {fixed,std::move(out),report(*pin,NativeCoveringAction::NativeChain,a,lease,guard,required),{},std::move(input)};
}
inline DerivedNativeCoveringField PreparedNativeCovering::transform(DerivedNativeCoveringField input,NativeCoveringAction action,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;lease.check_cancelled();available(*pin,a);require(input.owner_.state_==pin,"covering action needs its actual field owner");
    const auto required=requirements(pin->covering.retained_words(),pin->covering.action_envelope(input.field_.values().size()));runtime::admit_execution(required,lease.policy());Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});
    auto out=[&]{switch(action){
        case NativeCoveringAction::Boundary:return pin->covering.boundary(input.field_,guard);
        case NativeCoveringAction::Pushforward:return pin->covering.pushforward(input.field_,guard);
        case NativeCoveringAction::Transfer:return pin->covering.transfer(input.field_,guard);
        case NativeCoveringAction::Pullback:return pin->covering.pullback(input.field_,guard);
        case NativeCoveringAction::Descent:return pin->covering.descend(input.field_,guard);
        default:throw std::invalid_argument("unknown native covering field action");
    }}();lease.check_cancelled();return {PreparedNativeCovering(pin),std::move(out),report(*pin,action,a,lease,guard,required),std::make_shared<const DerivedNativeCoveringField>(std::move(input))};
}
inline DerivedNativeCoveringField PreparedNativeCovering::boundary(DerivedNativeCoveringField v,NativeCoveringAvailability a,runtime::ExecutionLease l) const{return transform(std::move(v),NativeCoveringAction::Boundary,a,std::move(l));}
inline DerivedNativeCoveringField PreparedNativeCovering::pushforward(DerivedNativeCoveringField v,NativeCoveringAvailability a,runtime::ExecutionLease l) const{return transform(std::move(v),NativeCoveringAction::Pushforward,a,std::move(l));}
inline DerivedNativeCoveringField PreparedNativeCovering::transfer(DerivedNativeCoveringField v,NativeCoveringAvailability a,runtime::ExecutionLease l) const{return transform(std::move(v),NativeCoveringAction::Transfer,a,std::move(l));}
inline DerivedNativeCoveringField PreparedNativeCovering::pullback(DerivedNativeCoveringField v,NativeCoveringAvailability a,runtime::ExecutionLease l) const{return transform(std::move(v),NativeCoveringAction::Pullback,a,std::move(l));}
inline DerivedNativeCoveringField PreparedNativeCovering::descend(DerivedNativeCoveringField v,NativeCoveringAvailability a,runtime::ExecutionLease l) const{return transform(std::move(v),NativeCoveringAction::Descent,a,std::move(l));}
class DerivedNativeCoveringWalk {
public:
    PreparedNativeCovering plan() const{return owner_;}
    const topology::OwnedCoveringWalk& walk() const &{return walk_;} const topology::OwnedCoveringWalk& walk() const &&=delete;
    const NativeCoveringWalkRequest& request() const &{return request_;} const NativeCoveringWalkRequest& request() const &&=delete;
    const std::optional<std::vector<NativeCoveringEdgeStep>>& pairwise_input() const &{return pairwise_;}
    const std::optional<std::vector<NativeCoveringEdgeStep>>& pairwise_input() const &&=delete;
    const NativeCoveringProvenance& provenance() const &{return report_;} const NativeCoveringProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeCovering;
    DerivedNativeCoveringWalk(PreparedNativeCovering owner,topology::OwnedCoveringWalk w,NativeCoveringWalkRequest r,NativeCoveringProvenance p,
        std::optional<std::vector<NativeCoveringEdgeStep>> pairwise={})
        :owner_(std::move(owner)),walk_(std::move(w)),request_(std::move(r)),report_(std::move(p)),pairwise_(std::move(pairwise)){}
    PreparedNativeCovering owner_;topology::OwnedCoveringWalk walk_;NativeCoveringWalkRequest request_;NativeCoveringProvenance report_;
    std::optional<std::vector<NativeCoveringEdgeStep>> pairwise_;
};
inline DerivedNativeCoveringWalk PreparedNativeCovering::walk(NativeCoveringWalkRequest input,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;auto request=input;lease.check_cancelled();available(*pin,a);
    const auto required=requirements(pin->covering.retained_words(),add(pin->covering.entry_envelope(),mul(add(request.steps.size(),1),32)));runtime::admit_execution(required,lease.policy());
    Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});pin->covering.validate(guard);storage::StableSourceIndex index(pin->request.base.endpoint()->source);const auto start=index.cell(request.start);require(start.has_value(),"covering walk names an absent initial cell");
    std::vector<topology::CoveringFlagStep> steps;for(const auto& s:request.steps){guard.step();const auto p=index.occurrence(s.occurrence);require(p.has_value(),"covering walk names an absent occurrence");steps.push_back({s.occurrence.upper_grade,*p,s.upward});}
    auto out=topology::OwnedCoveringWalk::prepare(pin->covering,Channel::Base,*start,std::move(steps),guard);lease.check_cancelled();return {PreparedNativeCovering(pin),std::move(out),std::move(request),report(*pin,NativeCoveringAction::Walk,a,lease,guard,required)};
}
inline DerivedNativeCoveringWalk PreparedNativeCovering::pairwise_walk(storage::StableCellKey start,std::vector<NativeCoveringEdgeStep> input,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;auto captured=input;lease.check_cancelled();available(*pin,a);
    const auto required=requirements(pin->covering.retained_words(),add(pin->covering.entry_envelope(),mul(add(captured.size(),1),64)));runtime::admit_execution(required,lease.policy());
    Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});pin->covering.validate(guard);storage::StableSourceIndex index(pin->request.base.endpoint()->source);const auto first=index.cell(start);require(first && first->grade.value==0,"pairwise covering walk needs an actual base vertex");
    std::vector<topology::EdgeStep> steps;for(const auto& s:captured){guard.step();const auto cell=index.cell(s.relation);require(cell && cell->grade.value==1,"pairwise covering step needs a primary grade-one relation");steps.push_back({cell->index,s.forward});}
    auto out=topology::covering_pairwise_walk(pin->covering,first->index,std::move(steps),guard);auto declaration=native_covering_detail::request(out);lease.check_cancelled();
    return {PreparedNativeCovering(pin),std::move(out),std::move(declaration),report(*pin,NativeCoveringAction::PairwiseWalk,a,lease,guard,required),std::move(captured)};
}
inline DerivedNativeCoveringWalk PreparedNativeCovering::prefix(DerivedNativeCoveringWalk input,std::size_t n,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;require(input.owner_.state_==pin && n<=input.request_.steps.size(),"covering prefix owner/length mismatch");
    NativeCoveringWalkRequest request{input.request_.start,{input.request_.steps.begin(),input.request_.steps.begin()+static_cast<std::ptrdiff_t>(n)}};
    // The prefix owns only its selected word. No parent exposes the later suffix.
    return PreparedNativeCovering(pin).walk(std::move(request),a,std::move(lease));
}
class NativeCoveringLift {
public:
    std::shared_ptr<const DerivedNativeCoveringWalk> input() const{return input_;}
    const topology::OwnedCoveringLift& lift() const &{return lift_;} const topology::OwnedCoveringLift& lift() const &&=delete;
    const NativeCoveringProvenance& provenance() const &{return report_;} const NativeCoveringProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeCovering;
    NativeCoveringLift(std::shared_ptr<const DerivedNativeCoveringWalk> input,topology::OwnedCoveringLift lift,NativeCoveringProvenance p)
        :input_(std::move(input)),lift_(std::move(lift)),report_(std::move(p)){}
    std::shared_ptr<const DerivedNativeCoveringWalk> input_;topology::OwnedCoveringLift lift_;NativeCoveringProvenance report_;
};
inline NativeCoveringLift PreparedNativeCovering::lift(DerivedNativeCoveringWalk input,storage::StableCellKey start,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;lease.check_cancelled();available(*pin,a);require(input.owner_.state_==pin,"covering lift needs its actual walk owner");
    const auto required=requirements(pin->covering.retained_words(),add(pin->covering.entry_envelope(),mul(add(input.walk_.steps().size(),1),64)));runtime::admit_execution(required,lease.policy());Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});pin->covering.validate(guard);
    storage::StableSourceIndex index(pin->request.cover.endpoint()->source);const auto cell=index.cell(start);require(cell.has_value(),"covering lift names an absent start");auto out=topology::lift_covering_walk(input.walk_,*cell,guard);lease.check_cancelled();
    return {std::make_shared<const DerivedNativeCoveringWalk>(std::move(input)),std::move(out),report(*pin,NativeCoveringAction::Lift,a,lease,guard,required)};
}
class NativeCoveringMonodromy {
public:
    std::shared_ptr<const DerivedNativeCoveringWalk> input() const{return input_;}
    const topology::OwnedCoveringMonodromy& action() const &{return action_;} const topology::OwnedCoveringMonodromy& action() const &&=delete;
    const NativeCoveringProvenance& provenance() const &{return report_;} const NativeCoveringProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeCovering;
    NativeCoveringMonodromy(std::shared_ptr<const DerivedNativeCoveringWalk> input,topology::OwnedCoveringMonodromy action,NativeCoveringProvenance p)
        :input_(std::move(input)),action_(std::move(action)),report_(std::move(p)){}
    std::shared_ptr<const DerivedNativeCoveringWalk> input_;topology::OwnedCoveringMonodromy action_;NativeCoveringProvenance report_;
};
inline NativeCoveringMonodromy PreparedNativeCovering::monodromy(DerivedNativeCoveringWalk input,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;lease.check_cancelled();available(*pin,a);require(input.owner_.state_==pin,"monodromy needs its actual native walk owner");
    const auto cells=pin->covering.source().grade(input.walk_.start().grade).cell_count();const auto required=requirements(pin->covering.retained_words(),add(pin->covering.entry_envelope(),mul(mul(add(cells,1),add(input.walk_.steps().size(),1)),64)));
    runtime::admit_execution(required,lease.policy());Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});auto out=topology::OwnedCoveringMonodromy::prepare(input.walk_,guard);lease.check_cancelled();
    return {std::make_shared<const DerivedNativeCoveringWalk>(std::move(input)),std::move(out),report(*pin,NativeCoveringAction::Monodromy,a,lease,guard,required)};
}
class NativeCoveringDeck {
public:
    PreparedNativeCovering plan() const{return owner_;}
    const topology::OwnedCoveringDeck& transformation() const &{return deck_;} const topology::OwnedCoveringDeck& transformation() const &&=delete;
    const NativeGluingSourceMap& declaration() const &{return request_;} const NativeGluingSourceMap& declaration() const &&=delete;
    const NativeCoveringProvenance& provenance() const &{return report_;} const NativeCoveringProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeCovering;
    NativeCoveringDeck(PreparedNativeCovering p,NativeGluingSourceMap r,topology::OwnedCoveringDeck d,NativeCoveringProvenance report)
        :owner_(std::move(p)),request_(std::move(r)),deck_(std::move(d)),report_(std::move(report)){}
    PreparedNativeCovering owner_;NativeGluingSourceMap request_;topology::OwnedCoveringDeck deck_;NativeCoveringProvenance report_;
};
inline NativeCoveringDeck PreparedNativeCovering::deck(NativeGluingSourceMap input,NativeCoveringAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;auto request=input;lease.check_cancelled();available(*pin,a);
    require(request.key && request.from==pin->request.projection.from && request.to==request.from,"deck transformation needs its actual declared cover source");
    const auto w=add(pin->covering.retained_words(),add(request.cells.size(),request.occurrences.size())),envelope=add(pin->covering.entry_envelope(),mul(w,32));const auto required=requirements(w,envelope);runtime::admit_execution(required,lease.policy());Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});pin->covering.validate(guard);
    const auto& c=pin->request.cover.endpoint()->source;auto map=native_gluing_detail::indexed(c,c,request.cells,request.occurrences,guard);auto out=topology::OwnedCoveringDeck::prepare(pin->covering,std::move(map),request.key,guard);lease.check_cancelled();
    return {PreparedNativeCovering(pin),std::move(request),std::move(out),report(*pin,NativeCoveringAction::Deck,a,lease,guard,required)};
}
struct NativeCoveringPullbackAvailability {NativeCoveringAvailability original,pulled;};
struct NativeCoveringPullbackRequest {std::uint64_t key{},revision{};NativeGluingSourceMap into_cover,base_map;};
struct NativeCoveringCone {storage::PinnedNativeAccession source;std::uint64_t source_key{};NativeGluingSourceMap into_cover,into_base;};
struct NativeCoveringPullbackProvenance {
    std::uint64_t key{},revision{};std::array<std::uint64_t,4> namespaces,sequences;
    NativeCoveringPullbackAvailability preparation_availability,execution_availability;
    runtime::ExecutionRequirements requirements;numeric::ExactActionLimits limits;
    std::size_t preparation_steps{},execution_steps{};std::string preparation_policy,execution_policy;bool fallback{};
};
namespace native_covering_detail {
struct PullbackState {
    PreparedNativeCovering original,pulled;NativeCoveringPullbackRequest request;
    local::OwnedCoveringPullback pullback;NativeCoveringPullbackAvailability preparation;
    std::size_t steps;std::string policy;bool fallback;
};
inline std::array<Pin,4> pins(const PullbackState& s){return {s.original.anchor(Channel::Cover),s.original.anchor(Channel::Base),s.pulled.anchor(Channel::Cover),s.pulled.anchor(Channel::Base)};}
inline void available(const PullbackState& s,NativeCoveringPullbackAvailability a) {
    const auto check=[](const PreparedNativeCovering& p,NativeCoveringAvailability w){
        runtime::admit_work_availability(p.anchor(Channel::Cover).source_namespace(),p.anchor(Channel::Cover).sequence(),w.cover);
        runtime::admit_work_availability(p.anchor(Channel::Base).source_namespace(),p.anchor(Channel::Base).sequence(),w.base);
    };
    check(s.original,s.original.preparation_availability());check(s.pulled,s.pulled.preparation_availability());
    for(const auto& w:{s.preparation,a}){check(s.original,w.original);check(s.pulled,w.pulled);}
}
inline std::size_t dimension(const PullbackState& s){return std::min(s.original.declaration().dimension_limit,s.pulled.declaration().dimension_limit);}
inline NativeCoveringPullbackProvenance report(const PullbackState& s,NativeCoveringPullbackAvailability a,
    const runtime::ExecutionLease& lease,const Guard& guard,runtime::ExecutionRequirements required) {
    const auto anchors=pins(s);return {s.request.key,s.request.revision,
        {anchors[0].source_namespace(),anchors[1].source_namespace(),anchors[2].source_namespace(),anchors[3].source_namespace()},
        {anchors[0].sequence(),anchors[1].sequence(),anchors[2].sequence(),anchors[3].sequence()},s.preparation,a,required,guard.limits(),s.steps,guard.steps(),s.policy,lease.policy().name,
        s.fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu};
}
}
class NativeCoveringPullbackWitness;
class NativeCoveringPullbackMediator;
class PreparedNativeCoveringPullback {
public:
    static PreparedNativeCoveringPullback prepare(PreparedNativeCovering original,PreparedNativeCovering pulled,NativeCoveringPullbackRequest input,
        NativeCoveringPullbackAvailability a,runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_covering_detail;auto r=input;lease.check_cancelled();available(*original.state_,a.original);available(*pulled.state_,a.pulled);
        require(r.key && r.revision && r.into_cover.key && r.base_map.key &&
            r.into_cover.from==pulled.declaration().projection.from && r.into_cover.to==original.declaration().projection.from &&
            r.base_map.from==pulled.declaration().projection.to && r.base_map.to==original.declaration().projection.to,"covering pullback needs all actual declared source maps");
        const auto w=add(original.numeric_plan().retained_words(),add(pulled.numeric_plan().retained_words(),add(add(r.into_cover.cells.size(),r.into_cover.occurrences.size()),add(r.base_map.cells.size(),r.base_map.occurrences.size()))));
        const auto required=requirements(w,add(add(original.numeric_plan().entry_envelope(),pulled.numeric_plan().entry_envelope()),mul(w,64)));runtime::admit_execution(required,lease.policy());
        const std::array<Pin,4> anchors{original.anchor(Channel::Cover),original.anchor(Channel::Base),pulled.anchor(Channel::Cover),pulled.anchor(Channel::Base)};
        Guard guard(native_gluing_detail::limits(lease,anchors,std::min(original.declaration().dimension_limit,pulled.declaration().dimension_limit)),[&lease]{lease.check_cancelled();});original.numeric_plan().validate(guard);pulled.numeric_plan().validate(guard);
        auto into=native_gluing_detail::indexed(anchors[2].endpoint()->source,anchors[0].endpoint()->source,r.into_cover.cells,r.into_cover.occurrences,guard);
        auto base=native_gluing_detail::indexed(anchors[3].endpoint()->source,anchors[1].endpoint()->source,r.base_map.cells,r.base_map.occurrences,guard);
        auto square=local::OwnedCoveringPullback::prepare(original.numeric_plan(),pulled.numeric_plan(),std::move(into),std::move(base),r.key,r.revision,guard);lease.check_cancelled();
        const auto fallback=original.state_->fallback || pulled.state_->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        return PreparedNativeCoveringPullback(std::make_shared<const PullbackState>(PullbackState{std::move(original),std::move(pulled),std::move(r),std::move(square),a,guard.steps(),lease.policy().name,fallback}));
    }
    PreparedNativeCovering original() const{return state_->original;} PreparedNativeCovering pulled() const{return state_->pulled;}
    local::OwnedCoveringPullback numeric_plan() const{return state_->pullback;}
    const NativeCoveringPullbackRequest& declaration() const &{return state_->request;} const NativeCoveringPullbackRequest& declaration() const &&=delete;
    NativeCoveringPullbackAvailability preparation_availability() const{return state_->preparation;}
    NativeCoveringPullbackWitness witness(NativeCoveringPullbackAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeCoveringPullbackMediator mediate(NativeCoveringCone,runtime::WorkAvailability,NativeCoveringPullbackAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
private:
    friend class NativeCoveringPullbackWitness;friend class NativeCoveringPullbackMediator;
    explicit PreparedNativeCoveringPullback(std::shared_ptr<const native_covering_detail::PullbackState> s):state_(std::move(s)){}
    std::shared_ptr<const native_covering_detail::PullbackState> state_;
};
class NativeCoveringPullbackWitness {
public:
    PreparedNativeCoveringPullback plan() const{return PreparedNativeCoveringPullback(owner_);}
    const NativeCoveringPullbackProvenance& provenance() const &{return report_;}const NativeCoveringPullbackProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeCoveringPullback;
    NativeCoveringPullbackWitness(std::shared_ptr<const native_covering_detail::PullbackState> owner,NativeCoveringPullbackProvenance p):owner_(std::move(owner)),report_(std::move(p)){}
    std::shared_ptr<const native_covering_detail::PullbackState> owner_;NativeCoveringPullbackProvenance report_;
};
inline NativeCoveringPullbackWitness PreparedNativeCoveringPullback::witness(NativeCoveringPullbackAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;lease.check_cancelled();available(*pin,a);
    const auto required=requirements(pin->pullback.retained_words(),pin->pullback.entry_envelope());runtime::admit_execution(required,lease.policy());Guard guard(native_gluing_detail::limits(lease,pins(*pin),dimension(*pin)),[&lease]{lease.check_cancelled();});pin->pullback.validate(guard);lease.check_cancelled();
    return {pin,report(*pin,a,lease,guard,required)};
}
class NativeCoveringPullbackMediator {
public:
    PreparedNativeCoveringPullback plan() const{return PreparedNativeCoveringPullback(owner_);}
    const local::OwnedCoveringPullbackMediator& mediator() const &{return mediator_;}const local::OwnedCoveringPullbackMediator& mediator() const &&=delete;
    const NativeCoveringCone& request() const &{return request_;}const NativeCoveringCone& request() const &&=delete;
    runtime::WorkAvailability source_availability() const{return availability_;}
    const NativeCoveringPullbackProvenance& provenance() const &{return report_;}const NativeCoveringPullbackProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeCoveringPullback;
    NativeCoveringPullbackMediator(std::shared_ptr<const native_covering_detail::PullbackState> owner,NativeCoveringCone r,
        local::OwnedCoveringPullbackMediator m,runtime::WorkAvailability a,NativeCoveringPullbackProvenance report)
        :owner_(std::move(owner)),request_(std::move(r)),mediator_(std::move(m)),availability_(a),report_(std::move(report)){}
    std::shared_ptr<const native_covering_detail::PullbackState> owner_;NativeCoveringCone request_;local::OwnedCoveringPullbackMediator mediator_;
    runtime::WorkAvailability availability_;NativeCoveringPullbackProvenance report_;
};
inline NativeCoveringPullbackMediator PreparedNativeCoveringPullback::mediate(NativeCoveringCone input,runtime::WorkAvailability cone,
    NativeCoveringPullbackAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_covering_detail;const auto pin=state_;auto request=input;lease.check_cancelled();available(*pin,a);
    require(request.source_key && request.into_cover.key && request.into_base.key && request.into_cover.from==request.source_key && request.into_base.from==request.source_key &&
        request.into_cover.to==pin->original.declaration().projection.from && request.into_base.to==pin->pulled.declaration().projection.to,"pullback cone needs its complete declared source identities");
    require(request.source.kind()==storage::NativeAccessionKind::Chain && !request.source.support_count(),"pullback cone needs an actual empty owning Chain anchor");runtime::admit_work_availability(request.source.source_namespace(),request.source.sequence(),cone);
    const auto w=add(pin->pullback.retained_words(),add(algebra::owned_gluing_detail::words(request.source.endpoint()->source),add(add(request.into_cover.cells.size(),request.into_cover.occurrences.size()),add(request.into_base.cells.size(),request.into_base.occurrences.size()))));
    const auto required=requirements(w,add(pin->pullback.entry_envelope(),mul(w,64)));runtime::admit_execution(required,lease.policy());const auto four=pins(*pin);
    const std::array<Pin,5> anchors{four[0],four[1],four[2],four[3],request.source};Guard guard(native_gluing_detail::limits(lease,anchors,dimension(*pin)),[&lease]{lease.check_cancelled();});pin->pullback.validate(guard);algebra::owned_gluing_detail::source(request.source.endpoint()->source,guard);
    auto into=native_gluing_detail::indexed(request.source.endpoint()->source,four[0].endpoint()->source,request.into_cover.cells,request.into_cover.occurrences,guard);
    auto base=native_gluing_detail::indexed(request.source.endpoint()->source,four[3].endpoint()->source,request.into_base.cells,request.into_base.occurrences,guard);
    auto out=pin->pullback.mediate(request.source.endpoint()->source,std::move(into),std::move(base),guard);lease.check_cancelled();
    return {pin,std::move(request),std::move(out),cone,report(*pin,a,lease,guard,required)};
}
} // namespace rfc::inference
