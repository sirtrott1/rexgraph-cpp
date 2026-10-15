#pragma once
#include "rfc/local/owned_covering.hpp"
#include "rfc/topology/monodromy.hpp"

namespace rfc::topology {
// A structural walk uses the actual retained flag, including inactive flags.
// It is not a pairwise graph edge or an automatically homotopy-quotiented path.
struct CoveringFlagStep {
    core::Grade upper_grade;
    std::size_t occurrence{};
    bool upward{};
    constexpr auto operator<=>(const CoveringFlagStep&) const=default;
};
class OwnedCoveringWalk {
public:
    static OwnedCoveringWalk prepare(local::OwnedRelationalCovering owner,local::CoveringChannel channel,
        core::CellId start,std::vector<CoveringFlagStep> input,numeric::ExactActionGuard& guard) {
        using namespace local::owned_covering_detail;auto steps=input;owner.validate(guard);
        guard.entries(add(owner.entry_envelope(),mul(add(steps.size(),1),16)));
        const auto& c=owner.source(channel);require(start.index<c.grade(start.grade).cell_count(),"covering walk start out of range");
        std::vector<core::CellId> visits{start};auto current=start;
        for(const auto& step:steps){
            guard.step();require(step.upper_grade.value>0 && step.upper_grade.value<c.grade_count(),"covering walk flag grade");
            const auto& a=c.grade(step.upper_grade);require(step.occurrence<a.incidence_count(),"covering walk flag out of range");
            const core::CellId lower{grade(step.upper_grade.value-1u),a.lower_index[step.occurrence]},upper{step.upper_grade,a.upper_index[step.occurrence]};
            require(current==(step.upward?lower:upper),"covering walk is not incident to its current cell");
            current=step.upward?upper:lower;visits.push_back(current);
        }
        return {std::move(owner),channel,start,std::move(steps),std::move(visits)};
    }
    local::OwnedRelationalCovering covering() const{return owner_;}
    local::CoveringChannel channel() const{return channel_;}
    core::CellId start() const{return start_;} core::CellId finish() const{return visits_.back();}
    bool is_loop() const{return start()==finish();}
    const std::vector<CoveringFlagStep>& steps() const &{return steps_;}
    const std::vector<CoveringFlagStep>& steps() const &&=delete;
    const std::vector<core::CellId>& visits() const &{return visits_;}
    const std::vector<core::CellId>& visits() const &&=delete;
    OwnedCoveringWalk inverse(numeric::ExactActionGuard& guard) const {
        auto copy=*this;std::vector<CoveringFlagStep> out;
        guard.entries(local::owned_covering_detail::add(copy.owner_.entry_envelope(),local::owned_covering_detail::mul(copy.steps_.size()+1,24)));
        for(auto it=copy.steps_.rbegin();it!=copy.steps_.rend();++it){guard.step();out.push_back({it->upper_grade,it->occurrence,!it->upward});}
        return prepare(copy.owner_,copy.channel_,copy.finish(),std::move(out),guard);
    }
    OwnedCoveringWalk prefix(std::size_t n,numeric::ExactActionGuard& guard) const {
        const auto copy=*this;local::owned_covering_detail::require(n<=copy.steps_.size(),"covering walk prefix length");
        return prepare(copy.owner_,copy.channel_,copy.start_,{copy.steps_.begin(),copy.steps_.begin()+static_cast<std::ptrdiff_t>(n)},guard);
    }
    static OwnedCoveringWalk compose(OwnedCoveringWalk first,OwnedCoveringWalk second,numeric::ExactActionGuard& guard) {
        using namespace local::owned_covering_detail;
        require(first.owner_.same_owner(second.owner_) && first.channel_==second.channel_ && first.finish()==second.start(),"covering walk composition owner/channel/endpoint mismatch");
        guard.entries(add(first.owner_.entry_envelope(),mul(add(add(first.steps_.size(),second.steps_.size()),1),24)));
        auto steps=first.steps_;for(const auto& step:second.steps_){guard.step();steps.push_back(step);}
        return prepare(first.owner_,first.channel_,first.start_,std::move(steps),guard);
    }
private:
    OwnedCoveringWalk(local::OwnedRelationalCovering owner,local::CoveringChannel channel,core::CellId start,
        std::vector<CoveringFlagStep> steps,std::vector<core::CellId> visits)
        :owner_(std::move(owner)),channel_(channel),start_(start),steps_(std::move(steps)),visits_(std::move(visits)){}
    local::OwnedRelationalCovering owner_;local::CoveringChannel channel_;core::CellId start_;
    std::vector<CoveringFlagStep> steps_;std::vector<core::CellId> visits_;
};
struct OwnedCoveringLift {
    OwnedCoveringWalk base,lifted;
};
inline OwnedCoveringLift lift_covering_walk(OwnedCoveringWalk base,core::CellId start,numeric::ExactActionGuard& guard) {
    using namespace local::owned_covering_detail;const auto owner=base.covering();
    require(base.channel()==local::CoveringChannel::Base,"covering lift requires an actual base walk");owner.validate(guard);
    const auto& c=owner.source();require(start.grade==base.start().grade && start.index<c.grade(start.grade).cell_count(),"covering lift initial cell grade/range");
    require(owner.projection().cells[start.grade.value][start.index]==base.start().index,"covering lift initial cell is in a different fiber");
    guard.entries(add(owner.entry_envelope(),mul(add(base.steps().size(),1),32)));
    std::vector<CoveringFlagStep> out;auto current=start;
    for(const auto& step:base.steps()) {
        const auto p=owner.lift_flag(current,step.upper_grade,step.occurrence,step.upward,guard);const auto& a=c.grade(step.upper_grade);
        out.push_back({step.upper_grade,p,step.upward});current=step.upward?core::CellId{step.upper_grade,a.upper_index[p]}:core::CellId{grade(step.upper_grade.value-1u),a.lower_index[p]};
    }
    auto lifted=OwnedCoveringWalk::prepare(owner,local::CoveringChannel::Cover,start,std::move(out),guard);
    for(std::size_t i=0;i<lifted.visits().size();++i){guard.step();const auto v=lifted.visits()[i];require(v.grade==base.visits()[i].grade && owner.projection().cells[v.grade.value][v.index]==base.visits()[i].index,"covering walk lift projection failed");}
    return {std::move(base),std::move(lifted)};
}
class OwnedCoveringMonodromy {
public:
    static OwnedCoveringMonodromy prepare(OwnedCoveringWalk loop,numeric::ExactActionGuard& guard) {
        using namespace local::owned_covering_detail;require(loop.channel()==local::CoveringChannel::Base && loop.is_loop(),"covering monodromy needs a closed actual base walk");
        const auto owner=loop.covering();auto fiber=owner.fiber(loop.start(),guard);
        guard.entries(add(owner.entry_envelope(),mul(mul(add(fiber.size(),1),add(loop.steps().size(),1)),64)));
        std::map<std::uint32_t,std::size_t> position;for(std::size_t i=0;i<fiber.size();++i){guard.step();position.emplace(fiber[i].index,i);}
        std::vector<OwnedCoveringLift> lifts;std::vector<std::size_t> image;std::set<std::size_t> hit;
        for(const auto v:fiber){auto lift=lift_covering_walk(loop,v,guard);const auto it=position.find(lift.lifted.finish().index);require(it!=position.end() && hit.insert(it->second).second,"covering loop lift does not permute its entire fiber");image.push_back(it->second);lifts.push_back(std::move(lift));}
        return {std::move(loop),std::move(fiber),std::move(image),std::move(lifts)};
    }
    const OwnedCoveringWalk& loop() const &{return loop_;} const OwnedCoveringWalk& loop() const &&=delete;
    const std::vector<core::CellId>& fiber() const &{return fiber_;} const std::vector<core::CellId>& fiber() const &&=delete;
    const std::vector<std::size_t>& image() const &{return image_;} const std::vector<std::size_t>& image() const &&=delete;
    const std::vector<OwnedCoveringLift>& lifts() const &{return lifts_;} const std::vector<OwnedCoveringLift>& lifts() const &&=delete;
private:
    OwnedCoveringMonodromy(OwnedCoveringWalk loop,std::vector<core::CellId> f,std::vector<std::size_t> i,std::vector<OwnedCoveringLift> lifts)
        :loop_(std::move(loop)),fiber_(std::move(f)),image_(std::move(i)),lifts_(std::move(lifts)){}
    OwnedCoveringWalk loop_;std::vector<core::CellId> fiber_;std::vector<std::size_t> image_;std::vector<OwnedCoveringLift> lifts_;
};
// Explicit adapter from the existing pairwise profile; all other retained
// structure remains in the covering. No branch is silently made pairwise.
inline OwnedCoveringWalk covering_pairwise_walk(local::OwnedRelationalCovering owner,std::uint32_t start,
    std::vector<EdgeStep> input,numeric::ExactActionGuard& guard) {
    using namespace local::owned_covering_detail;auto steps=input;owner.validate(guard);const auto& c=owner.source(local::CoveringChannel::Base);
    guard.entries(add(owner.entry_envelope(),mul(add(steps.size(),1),32)));std::vector<CoveringFlagStep> flags;
    for(const auto& step:steps){
        guard.step();const auto endpoints=pairwise_endpoints(c,step.relation);(void)endpoints;const auto& a=c.grade(grade(1));
        std::optional<std::size_t> negative,positive;const auto [begin,end]=a.incidence_range(step.relation);
        for(auto p=begin;p<end;++p){guard.step();if(!a.aspects.get(c.signature().existence_aspect(),p))continue;const auto q=algebra::owned_gluing_detail::coefficient(c,grade(1),p,guard);if(q<0)negative=p;if(q>0)positive=p;}
        require(negative && positive,"pairwise covering walk has no declared endpoint flags");
        flags.push_back({grade(1),step.forward?*negative:*positive,true});flags.push_back({grade(1),step.forward?*positive:*negative,false});
    }
    return OwnedCoveringWalk::prepare(std::move(owner),local::CoveringChannel::Base,{grade(0),start},std::move(flags),guard);
}
class OwnedCoveringDeck {
    struct State{local::OwnedRelationalCovering cover;algebra::RetainedSourceMap forward,inverse;std::uint64_t key;std::size_t envelope;};
public:
    static OwnedCoveringDeck prepare(local::OwnedRelationalCovering cover,algebra::RetainedSourceMap input,std::uint64_t key,numeric::ExactActionGuard& guard) {
        using namespace local::owned_covering_detail;auto map=input;require(key!=0,"deck transformation needs an explicit identity");cover.validate(guard);const auto& c=cover.source();
        const auto envelope=add(cover.entry_envelope(),mul(words(map),16));guard.entries(envelope);algebra::owned_gluing_detail::map(c,c,map,true,guard);algebra::RetainedSourceMap inverse;
        for(std::size_t k=0;k<c.grade_count();++k){
            inverse.cells.emplace_back(map.cells[k].size());inverse.occurrences.emplace_back(map.occurrences[k].size());
            for(std::size_t i=0;i<map.cells[k].size();++i){guard.step();const auto j=map.cells[k][i];require(cover.projection().cells[k][i]==cover.projection().cells[k][j],"deck transformation changes a projected cell");inverse.cells[k][j]=static_cast<std::uint32_t>(i);}
            for(std::size_t p=0;p<map.occurrences[k].size();++p){guard.step();const auto q=map.occurrences[k][p];require(cover.projection().occurrences[k][p]==cover.projection().occurrences[k][q],"deck transformation changes a projected occurrence");inverse.occurrences[k][q]=p;}
        }
        algebra::owned_gluing_detail::map(c,c,inverse,true,guard);
        return OwnedCoveringDeck(std::make_shared<const State>(State{std::move(cover),std::move(map),std::move(inverse),key,envelope}));
    }
    local::OwnedRelationalCovering covering() const{return state_->cover;}
    const algebra::RetainedSourceMap& map(bool inverse=false) const &{return inverse?state_->inverse:state_->forward;}
    const algebra::RetainedSourceMap& map(bool=false) const &&=delete;
    std::uint64_t key() const{return state_->key;}
    void validate(numeric::ExactActionGuard& guard) const{const auto pin=state_;guard.entries(pin->envelope);pin->cover.validate(guard);for(std::size_t n=0;n<local::owned_covering_detail::add(local::owned_covering_detail::words(pin->forward),local::owned_covering_detail::words(pin->inverse));++n)guard.step();}
private:
    explicit OwnedCoveringDeck(std::shared_ptr<const State> s):state_(std::move(s)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::topology
