#pragma once
#include "rfc/local/covering.hpp"
#include "rfc/algebra/owned_gluing.hpp"

namespace rfc::local {
enum class CoveringChannel { Cover, Base };
enum class CoveringVariance { Chain, ScalarSection };
namespace owned_covering_detail {
using algebra::owned_gluing_detail::add;
using algebra::owned_gluing_detail::mul;
using algebra::owned_gluing_detail::grade;
using algebra::owned_gluing_detail::require;
using algebra::owned_gluing_detail::words;
using Guard = numeric::ExactActionGuard;
using Q = numeric::Rational;
using Values = std::map<std::uint32_t,Q>;
struct State {
    core::RelationalComplex cover,base;
    algebra::RetainedSourceMap projection;
    std::uint64_t key,revision;
    std::size_t retained,envelope;
    std::vector<std::vector<std::vector<std::uint32_t>>> fibers;
    // Adjacent upward flags: actual cover lower cell -> base slot -> cover slot.
    std::vector<std::vector<std::map<std::size_t,std::size_t>>> upward;
    StrictRelationalCovering reference;
    State(core::RelationalComplex a,core::RelationalComplex b,algebra::RetainedSourceMap p,
          std::uint64_t k,std::uint64_t r,std::size_t w,std::size_t e,
          decltype(fibers) f,decltype(upward) u)
        :cover(std::move(a)),base(std::move(b)),projection(std::move(p)),key(k),revision(r),
         retained(w),envelope(e),fibers(std::move(f)),upward(std::move(u)),reference(cover,base,projection.cells) {}
};
inline const core::RelationalComplex& source(const State& s,CoveringChannel c) {
    require(c==CoveringChannel::Cover || c==CoveringChannel::Base,"unknown covering channel");
    return c==CoveringChannel::Cover?s.cover:s.base;
}
inline void validate(const State& s,Guard& guard) {
    guard.entries(s.envelope); guard.value(Q{0}); guard.value(Q{1});
    algebra::owned_gluing_detail::source(s.cover,guard); algebra::owned_gluing_detail::source(s.base,guard);
    for(std::size_t n=0;n<words(s.projection);++n)guard.step();
    for(const auto& row:s.fibers)for(const auto& f:row)for(const auto i:f){(void)i;guard.step();}
    for(const auto& row:s.upward)for(const auto& f:row)for(const auto& [a,b]:f){(void)a;(void)b;guard.step();}
}
inline std::size_t cells(const State& s) {
    std::size_t n{};
    for(std::size_t k=0;k<s.cover.grade_count();++k)n=add(n,add(s.cover.grade(grade(k)).cell_count(),s.base.grade(grade(k)).cell_count()));
    return n;
}
inline std::size_t action_envelope(const State& s,std::size_t n) {return add(s.envelope,add(mul(cells(s),8),mul(n,4)));}
} // namespace owned_covering_detail

class OwnedCoveringField;
// An owned strengthening of the canonical active-flag covering: the declared
// projection additionally bijects every retained upward/downward occurrence.
class OwnedRelationalCovering {
public:
    using Values=owned_covering_detail::Values;
    static OwnedRelationalCovering prepare(core::RelationalComplex source,core::RelationalComplex base,
        algebra::RetainedSourceMap input,std::uint64_t key,std::uint64_t revision,numeric::ExactActionGuard& guard) {
        using namespace owned_covering_detail; auto a=source,b=base; auto p=input;
        require(key && revision,"owned covering needs a declared identity and revision");
        require(a.grade_count()==b.grade_count(),"covering must preserve the complete grade inventory");
        const auto w=add(words(a),add(words(b),words(p))),e=add(1,mul(w,32)); guard.entries(e);guard.value(Q{0});guard.value(Q{1});
        algebra::owned_gluing_detail::source(a,guard); algebra::owned_gluing_detail::source(b,guard);
        algebra::owned_gluing_detail::chain(a,guard,e); algebra::owned_gluing_detail::chain(b,guard,e);
        algebra::owned_gluing_detail::map(a,b,p,false,guard);
        decltype(State::fibers) fibers; decltype(State::upward) upward;
        for(std::size_t k=0;k<a.grade_count();++k) {
            const auto g=grade(k); const auto& x=a.grade(g); const auto& y=b.grade(g);
            fibers.emplace_back(y.cell_count()); upward.emplace_back(x.cell_count());
            for(std::size_t i=0;i<x.cell_count();++i){guard.step();fibers[k][p.cells[k][i]].push_back(static_cast<std::uint32_t>(i));}
            for(const auto& f:fibers[k]){guard.step();require(!f.empty(),"covering is not surjective on retained cells");}
            if(!k)continue;
            std::vector<std::size_t> target_count(b.grade(grade(k-1)).cell_count());
            for(std::size_t q=0;q<y.incidence_count();++q){guard.step();++target_count[y.lower_index[q]];}
            for(std::size_t slot=0;slot<x.incidence_count();++slot) {
                guard.step();auto& star=upward[k-1][x.lower_index[slot]];
                require(star.emplace(p.occurrences[k][slot],slot).second,"covering repeats a retained upward flag in one star");
            }
            for(std::size_t i=0;i<upward[k-1].size();++i){guard.step();require(upward[k-1][i].size()==target_count[p.cells[k-1][i]],"covering omits a retained upward flag");}
        }
        // Account the canonical reference's active-star search before its
        // synchronous construction. The original explicit flag map stays primary.
        for(std::size_t k=0;k<a.grade_count();++k){
            const auto g=grade(k);const auto sa=a.grade(g).incidence_count(),tb=b.grade(g).incidence_count();
            const auto owners=add(a.grade(g).cell_count(),k?a.grade(grade(k-1)).cell_count():0);
            const auto visits=mul(add(owners,1),mul(add(sa,1),mul(add(tb,1),add(a.signature().aspects().size(),3))));
            for(std::size_t n=0;n<visits;++n)guard.step();
        }
        auto state=std::make_shared<const State>(std::move(a),std::move(b),std::move(p),key,revision,w,e,std::move(fibers),std::move(upward));
        return OwnedRelationalCovering(std::move(state));
    }
    const core::RelationalComplex& source(CoveringChannel c=CoveringChannel::Cover) const & {return owned_covering_detail::source(*state_,c);}
    const core::RelationalComplex& source(CoveringChannel=CoveringChannel::Cover) const &&=delete;
    const StrictRelationalCovering& reference() const & {return state_->reference;}
    const StrictRelationalCovering& reference() const &&=delete;
    const algebra::RetainedSourceMap& projection() const & {return state_->projection;}
    const algebra::RetainedSourceMap& projection() const &&=delete;
    std::uint64_t key() const{return state_->key;} std::uint64_t revision() const{return state_->revision;}
    std::size_t retained_words() const{return state_->retained;} std::size_t entry_envelope() const{return state_->envelope;}
    std::size_t action_envelope(std::size_t n=0) const{return owned_covering_detail::action_envelope(*state_,n);}
    bool same_owner(const OwnedRelationalCovering& other) const{return state_==other.state_;}
    void validate(numeric::ExactActionGuard& guard) const{const auto pin=state_;owned_covering_detail::validate(*pin,guard);}
    std::vector<core::CellId> fiber(core::CellId base_cell,numeric::ExactActionGuard& guard) const {
        using namespace owned_covering_detail;const auto pin=state_;validate(guard);std::vector<core::CellId> out;
        for(const auto i:pin->fibers.at(base_cell.grade.value).at(base_cell.index)){guard.step();out.push_back({base_cell.grade,i});}
        return out;
    }
    std::size_t lift_flag(core::CellId current,core::Grade upper,std::size_t base_slot,bool upward,numeric::ExactActionGuard& guard) const {
        using namespace owned_covering_detail;const auto pin=state_;
        owned_covering_detail::validate(*pin,guard);
        require(upper.value>0 && upper.value<pin->base.grade_count(),"covering flag upper grade");
        const auto& a=pin->cover.grade(upper);const auto& b=pin->base.grade(upper);require(base_slot<b.incidence_count(),"covering base flag out of range");guard.step();
        require(current.grade==grade(upper.value-(upward?1u:0u)),"covering lift changes the current grade");
        require(current.index<pin->cover.grade(current.grade).cell_count(),"covering lift current cell out of range");
        const auto projected=pin->projection.cells[current.grade.value][current.index];
        require(projected==(upward?b.lower_index[base_slot]:b.upper_index[base_slot]),"covering lift starts outside the declared flag fiber");
        if(upward)return pin->upward[current.grade.value][current.index].at(base_slot);
        const auto [begin,end]=a.incidence_range(current.index);
        for(auto p=begin;p<end;++p){guard.step();if(pin->projection.occurrences[upper.value][p]==base_slot)return p;}
        throw std::logic_error("owned covering lost its downward flag inverse");
    }
    OwnedCoveringField chain(CoveringChannel,core::Grade,Values,numeric::ExactActionGuard&) const;
    OwnedCoveringField section(CoveringChannel,std::uint64_t,core::Grade,Values,numeric::ExactActionGuard&) const;
    OwnedCoveringField boundary(OwnedCoveringField,numeric::ExactActionGuard&) const;
    OwnedCoveringField pushforward(OwnedCoveringField,numeric::ExactActionGuard&) const;
    OwnedCoveringField transfer(OwnedCoveringField,numeric::ExactActionGuard&) const;
    OwnedCoveringField pullback(OwnedCoveringField,numeric::ExactActionGuard&) const;
    OwnedCoveringField descend(OwnedCoveringField,numeric::ExactActionGuard&) const;
private:
    friend class OwnedCoveringField;
    explicit OwnedRelationalCovering(std::shared_ptr<const owned_covering_detail::State> s):state_(std::move(s)){}
    OwnedCoveringField field(CoveringChannel,CoveringVariance,std::uint64_t,core::Grade,Values,numeric::ExactActionGuard&) const;
    OwnedCoveringField duplicate(OwnedCoveringField,CoveringVariance,numeric::ExactActionGuard&) const;
    std::shared_ptr<const owned_covering_detail::State> state_;
};
class OwnedCoveringField {
public:
    OwnedRelationalCovering covering() const{return OwnedRelationalCovering(owner_);}
    CoveringChannel channel() const{return channel_;} CoveringVariance variance() const{return variance_;}
    std::uint64_t field_identity() const{return identity_;} core::Grade grade() const{return grade_;}
    const owned_covering_detail::Values& values() const &{return values_;}
    const owned_covering_detail::Values& values() const &&=delete;
private:
    friend class OwnedRelationalCovering;
    OwnedCoveringField(std::shared_ptr<const owned_covering_detail::State> s,CoveringChannel c,CoveringVariance v,std::uint64_t id,core::Grade g,owned_covering_detail::Values x)
        :owner_(std::move(s)),channel_(c),variance_(v),identity_(id),grade_(g),values_(std::move(x)){}
    std::shared_ptr<const owned_covering_detail::State> owner_;CoveringChannel channel_;CoveringVariance variance_;
    std::uint64_t identity_;core::Grade grade_;owned_covering_detail::Values values_;
};
inline OwnedCoveringField OwnedRelationalCovering::field(CoveringChannel channel,CoveringVariance variance,std::uint64_t identity,core::Grade g,Values input,numeric::ExactActionGuard& guard) const {
    using namespace owned_covering_detail;const auto pin=state_;auto values=input;owned_covering_detail::validate(*pin,guard);guard.entries(owned_covering_detail::action_envelope(*pin,values.size()));
    algebra::owned_gluing_detail::values(owned_covering_detail::source(*pin,channel),g,values,guard);
    for(auto& [i,q]:values){(void)i;guard.step();q=numeric::canonical_rational(q);guard.value(q);}
    return {pin,channel,variance,identity,g,std::move(values)};
}
inline OwnedCoveringField OwnedRelationalCovering::chain(CoveringChannel c,core::Grade g,Values x,numeric::ExactActionGuard& guard) const{return field(c,CoveringVariance::Chain,0,g,std::move(x),guard);}
inline OwnedCoveringField OwnedRelationalCovering::section(CoveringChannel c,std::uint64_t id,core::Grade g,Values x,numeric::ExactActionGuard& guard) const {
    owned_covering_detail::require(id!=0,"covering section needs an explicit scalar field identity");return field(c,CoveringVariance::ScalarSection,id,g,std::move(x),guard);
}
inline OwnedCoveringField OwnedRelationalCovering::boundary(OwnedCoveringField input,numeric::ExactActionGuard& guard) const {
    using namespace owned_covering_detail;const auto pin=state_;require(input.owner_==pin && input.variance_==CoveringVariance::Chain,"covering boundary needs its actual Chain owner");
    require(input.grade_.value>0,"covering grade zero has no chain boundary");owned_covering_detail::validate(*pin,guard);guard.entries(owned_covering_detail::action_envelope(*pin,input.values_.size()));
    const auto& c=owned_covering_detail::source(*pin,input.channel_);algebra::owned_gluing_detail::values(c,input.grade_,input.values_,guard);Values out;
    const auto& a=c.grade(input.grade_);
    for(const auto& [i,q]:input.values_)for(auto p=a.incidence_range(i).first;p<a.incidence_range(i).second;++p){
        guard.step();auto& value=out.try_emplace(a.lower_index[p],Q{0}).first->second;
        if(a.aspects.get(c.signature().existence_aspect(),p))value=guard.add(value,guard.mul(q,algebra::owned_gluing_detail::coefficient(c,input.grade_,p,guard)));
    }
    return {pin,input.channel_,CoveringVariance::Chain,0,grade(input.grade_.value-1u),std::move(out)};
}
inline OwnedCoveringField OwnedRelationalCovering::pushforward(OwnedCoveringField input,numeric::ExactActionGuard& guard) const {
    using namespace owned_covering_detail;const auto pin=state_;require(input.owner_==pin && input.channel_==CoveringChannel::Cover && input.variance_==CoveringVariance::Chain,"covering pushforward needs its actual cover Chain");
    owned_covering_detail::validate(*pin,guard);guard.entries(owned_covering_detail::action_envelope(*pin,input.values_.size()));Values out;
    for(const auto& [i,q]:input.values_){guard.step();guard.value(q);auto& v=out.try_emplace(pin->projection.cells[input.grade_.value][i],Q{0}).first->second;v=guard.add(v,q);}
    return {pin,CoveringChannel::Base,CoveringVariance::Chain,0,input.grade_,std::move(out)};
}
inline OwnedCoveringField OwnedRelationalCovering::duplicate(OwnedCoveringField input,CoveringVariance variance,numeric::ExactActionGuard& guard) const {
    using namespace owned_covering_detail;const auto pin=state_;require(input.owner_==pin && input.channel_==CoveringChannel::Base && input.variance_==variance,"covering fiber duplication needs its actual declared base channel");
    owned_covering_detail::validate(*pin,guard);guard.entries(owned_covering_detail::action_envelope(*pin,input.values_.size()));Values out;
    for(const auto& [i,q]:input.values_){guard.value(q);for(const auto j:pin->fibers[input.grade_.value][i]){guard.step();out.emplace(j,q);}}
    return {pin,CoveringChannel::Cover,variance,input.identity_,input.grade_,std::move(out)};
}
inline OwnedCoveringField OwnedRelationalCovering::transfer(OwnedCoveringField x,numeric::ExactActionGuard& guard) const{return duplicate(std::move(x),CoveringVariance::Chain,guard);}
inline OwnedCoveringField OwnedRelationalCovering::pullback(OwnedCoveringField x,numeric::ExactActionGuard& guard) const{return duplicate(std::move(x),CoveringVariance::ScalarSection,guard);}
inline OwnedCoveringField OwnedRelationalCovering::descend(OwnedCoveringField input,numeric::ExactActionGuard& guard) const {
    using namespace owned_covering_detail;const auto pin=state_;require(input.owner_==pin && input.channel_==CoveringChannel::Cover && input.variance_==CoveringVariance::ScalarSection,"covering descent needs its actual scalar cover section");
    owned_covering_detail::validate(*pin,guard);guard.entries(owned_covering_detail::action_envelope(*pin,input.values_.size()));Values out;
    for(std::size_t i=0;i<pin->fibers[input.grade_.value].size();++i){
        const auto& f=pin->fibers[input.grade_.value][i];std::optional<Q> value;std::size_t present{};
        for(const auto j:f){guard.step();const auto it=input.values_.find(j);if(it==input.values_.end())continue;guard.value(it->second);++present;require(!value || *value==it->second,"scalar section does not descend across its fiber");value=it->second;}
        require(!present || present==f.size(),"scalar section support is not a union of complete fibers");if(value)out.emplace(static_cast<std::uint32_t>(i),*value);
    }
    return {pin,CoveringChannel::Base,CoveringVariance::ScalarSection,input.identity_,input.grade_,std::move(out)};
}
} // namespace rfc::local
