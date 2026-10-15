#pragma once
#include "rfc/local/owned_covering.hpp"

namespace rfc::local {
class OwnedCoveringPullbackMediator;
// The supplied source P and its maps declare the compatible-pair carrier.
// No equal raw key creates a pair or materializes a new relational source.
class OwnedCoveringPullback {
    struct State {
        OwnedRelationalCovering original,pulled;
        algebra::RetainedSourceMap into_cover,base_map;
        std::vector<std::map<std::pair<std::uint32_t,std::uint32_t>,std::uint32_t>> cells;
        std::vector<std::map<std::pair<std::size_t,std::size_t>,std::size_t>> occurrences;
        std::uint64_t key,revision;std::size_t retained,envelope;
    };
public:
    static OwnedCoveringPullback prepare(OwnedRelationalCovering original,OwnedRelationalCovering pulled,
        algebra::RetainedSourceMap input_into,algebra::RetainedSourceMap input_base,std::uint64_t key,std::uint64_t revision,numeric::ExactActionGuard& guard) {
        using namespace owned_covering_detail;auto into=input_into,base=input_base;
        require(key && revision,"covering pullback needs an explicit declaration identity/revision");original.validate(guard);pulled.validate(guard);
        const auto& e=original.source();const auto& b=original.source(CoveringChannel::Base);const auto& p=pulled.source();const auto& y=pulled.source(CoveringChannel::Base);
        require(e.grade_count()==p.grade_count(),"covering pullback changes the declared grade inventory");
        const auto w=add(original.retained_words(),add(pulled.retained_words(),add(words(into),words(base)))),envelope=add(add(original.entry_envelope(),pulled.entry_envelope()),mul(w,32));guard.entries(envelope);
        algebra::owned_gluing_detail::map(p,e,into,false,guard);algebra::owned_gluing_detail::map(y,b,base,false,guard);
        decltype(State::cells) cells;decltype(State::occurrences) occurrences;
        const auto& pm=original.projection();const auto& qm=pulled.projection();
        for(std::size_t k=0;k<e.grade_count();++k) {
            cells.emplace_back();occurrences.emplace_back();const auto g=grade(k);
            for(std::size_t i=0;i<p.grade(g).cell_count();++i){
                guard.step();const auto a=into.cells[k][i],z=qm.cells[k][i];
                require(pm.cells[k][a]==base.cells[k][z],"covering pullback square does not commute on cells");
                require(cells[k].emplace(std::pair{a,z},static_cast<std::uint32_t>(i)).second,"covering pullback repeats a compatible cell pair");
            }
            for(std::size_t i=0;i<p.grade(g).incidence_count();++i){
                guard.step();const auto a=into.occurrences[k][i],z=qm.occurrences[k][i];
                require(pm.occurrences[k][a]==base.occurrences[k][z],"covering pullback square does not commute on occurrences");
                require(occurrences[k].emplace(std::pair{a,z},i).second,"covering pullback repeats a compatible occurrence pair");
            }
            for(std::size_t a=0;a<e.grade(g).cell_count();++a)for(std::size_t z=0;z<y.grade(g).cell_count();++z){
                guard.step();if(pm.cells[k][a]==base.cells[k][z])require(cells[k].contains({static_cast<std::uint32_t>(a),static_cast<std::uint32_t>(z)}),"covering pullback omits a compatible cell pair");
            }
            for(std::size_t a=0;a<e.grade(g).incidence_count();++a)for(std::size_t z=0;z<y.grade(g).incidence_count();++z){
                guard.step();if(pm.occurrences[k][a]==base.occurrences[k][z])require(occurrences[k].contains({a,z}),"covering pullback omits a compatible occurrence pair");
            }
        }
        return OwnedCoveringPullback(std::make_shared<const State>(State{std::move(original),std::move(pulled),std::move(into),std::move(base),std::move(cells),std::move(occurrences),key,revision,w,envelope}));
    }
    OwnedRelationalCovering original() const{return state_->original;} OwnedRelationalCovering pulled() const{return state_->pulled;}
    const algebra::RetainedSourceMap& map(bool base=false) const &{return base?state_->base_map:state_->into_cover;}
    const algebra::RetainedSourceMap& map(bool=false) const &&=delete;
    std::uint64_t key() const{return state_->key;} std::uint64_t revision() const{return state_->revision;}
    std::size_t retained_words() const{return state_->retained;} std::size_t entry_envelope() const{return state_->envelope;}
    void validate(numeric::ExactActionGuard& guard) const {
        const auto pin=state_;guard.entries(pin->envelope);pin->original.validate(guard);pin->pulled.validate(guard);
        for(std::size_t n=0;n<owned_covering_detail::add(owned_covering_detail::words(pin->into_cover),owned_covering_detail::words(pin->base_map));++n)guard.step();
        for(const auto& row:pin->cells)for(const auto& [pair,cell]:row){(void)pair;(void)cell;guard.step();}
        for(const auto& row:pin->occurrences)for(const auto& [pair,slot]:row){(void)pair;(void)slot;guard.step();}
    }
    OwnedCoveringPullbackMediator mediate(core::RelationalComplex,algebra::RetainedSourceMap,algebra::RetainedSourceMap,numeric::ExactActionGuard&) const;
private:
    friend class OwnedCoveringPullbackMediator;
    explicit OwnedCoveringPullback(std::shared_ptr<const State> s):state_(std::move(s)){}
    std::shared_ptr<const State> state_;
};
class OwnedCoveringPullbackMediator {
public:
    OwnedCoveringPullback pullback() const{return OwnedCoveringPullback(owner_);}
    const core::RelationalComplex& source() const &{return source_;} const core::RelationalComplex& source() const &&=delete;
    const algebra::RetainedSourceMap& map() const &{return mediator_;} const algebra::RetainedSourceMap& map() const &&=delete;
    const algebra::RetainedSourceMap& cone_map(bool base=false) const &{return base?into_base_:into_cover_;}
    const algebra::RetainedSourceMap& cone_map(bool=false) const &&=delete;
private:
    friend class OwnedCoveringPullback;
    OwnedCoveringPullbackMediator(std::shared_ptr<const OwnedCoveringPullback::State> s,core::RelationalComplex source,
        algebra::RetainedSourceMap into,algebra::RetainedSourceMap base,algebra::RetainedSourceMap mediator)
        :owner_(std::move(s)),source_(std::move(source)),into_cover_(std::move(into)),into_base_(std::move(base)),mediator_(std::move(mediator)){}
    std::shared_ptr<const OwnedCoveringPullback::State> owner_;core::RelationalComplex source_;
    algebra::RetainedSourceMap into_cover_,into_base_,mediator_;
};
inline OwnedCoveringPullbackMediator OwnedCoveringPullback::mediate(core::RelationalComplex input_source,
    algebra::RetainedSourceMap input_into,algebra::RetainedSourceMap input_base,numeric::ExactActionGuard& guard) const {
    using namespace owned_covering_detail;const auto pin=state_;auto source=input_source;auto into=input_into,base=input_base;
    const auto fixed=OwnedCoveringPullback(pin);fixed.validate(guard);
    const auto envelope=add(pin->envelope,mul(add(words(source),add(words(into),words(base))),32));guard.entries(envelope);
    algebra::owned_gluing_detail::source(source,guard);algebra::owned_gluing_detail::chain(source,guard,envelope);
    const auto original=pin->original,pulled=pin->pulled;algebra::owned_gluing_detail::map(source,original.source(),into,false,guard);algebra::owned_gluing_detail::map(source,pulled.source(CoveringChannel::Base),base,false,guard);
    algebra::RetainedSourceMap mediator;
    for(std::size_t k=0;k<source.grade_count();++k){
        mediator.cells.emplace_back();mediator.occurrences.emplace_back();
        for(std::size_t i=0;i<into.cells[k].size();++i){guard.step();const auto it=pin->cells[k].find({into.cells[k][i],base.cells[k][i]});require(it!=pin->cells[k].end(),"covering pullback cone disagrees on a retained cell");mediator.cells[k].push_back(it->second);}
        for(std::size_t i=0;i<into.occurrences[k].size();++i){guard.step();const auto it=pin->occurrences[k].find({into.occurrences[k][i],base.occurrences[k][i]});require(it!=pin->occurrences[k].end(),"covering pullback cone disagrees on a retained occurrence");mediator.occurrences[k].push_back(it->second);}
    }
    algebra::owned_gluing_detail::map(source,pulled.source(),mediator,false,guard);
    return {pin,std::move(source),std::move(into),std::move(base),std::move(mediator)};
}
} // namespace rfc::local
