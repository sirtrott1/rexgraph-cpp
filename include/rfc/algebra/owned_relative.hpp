#pragma once
#include "rfc/algebra/relative.hpp"
#include "rfc/algebra/owned_gluing.hpp"

namespace rfc::algebra {
// A coordinate space, not a new set of primary relations.
enum class RelativeSpace { Whole, Sub, Quotient };
namespace owned_relative_detail {
using owned_gluing_detail::require;
using owned_gluing_detail::add;
using owned_gluing_detail::mul;
using owned_gluing_detail::grade;
using Guard=numeric::ExactActionGuard;
using Q=numeric::Rational;
using Values=std::map<std::uint32_t,Q>;
inline constexpr auto absent=std::numeric_limits<std::uint32_t>::max();
inline std::size_t words(const SubcomplexMask& mask) {
    auto n=mask.grade_count();
    for(std::size_t k=0;k<mask.grade_count();++k) n=add(n,mask.grade(grade(k)).size());
    return n;
}
inline void mask(const core::RelationalComplex& c,const SubcomplexMask& m,Guard& guard) {
    require(m.grade_count()==c.grade_count(),"relative mask changes declared grade inventory");
    for(std::size_t k=0;k<c.grade_count();++k) {
        const auto g=grade(k); const auto& a=c.grade(g);
        require(m.grade(g).size()==a.cell_count(),"relative mask changes ambient cell inventory");
        for(std::size_t i=0;i<a.cell_count();++i) {
            guard.step(); if(!m.contains({g,static_cast<std::uint32_t>(i)}) || !k) continue;
            const auto [b,e]=a.incidence_range(i);
            for(auto p=b;p<e;++p) { guard.step(); require(m.contains({grade(k-1),a.lower_index[p]}),"relative mask is not closed on every retained occurrence"); }
        }
    }
}
struct State {
    core::RelationalComplex ambient;
    SubcomplexMask whole,sub;
    // The existing canonical quotient view borrows only this stable owner.
    RelativeChainComplex reference;
    std::array<std::vector<std::vector<std::uint32_t>>,3> cells,inverse;
    std::size_t retained,envelope;
    State(core::RelationalComplex c,SubcomplexMask a,SubcomplexMask b,std::size_t w,std::size_t e)
        :ambient(std::move(c)),whole(std::move(a)),sub(std::move(b)),reference(ambient,whole,sub),retained(w),envelope(e) {
        for(std::size_t s=0;s<3;++s) {
            cells[s].resize(ambient.grade_count()); inverse[s].resize(ambient.grade_count());
            for(std::size_t k=0;k<ambient.grade_count();++k) {
                const auto g=grade(k); auto& row=cells[s][k]; auto& back=inverse[s][k];
                back.assign(ambient.grade(g).cell_count(),absent);
                for(std::size_t i=0;i<back.size();++i) {
                    const core::CellId id{g,static_cast<std::uint32_t>(i)};
                    const bool included=s==0?whole.contains(id):(s==1?sub.contains(id):whole.contains(id)&&!sub.contains(id));
                    if(included) { back[i]=static_cast<std::uint32_t>(row.size()); row.push_back(static_cast<std::uint32_t>(i)); }
                }
            }
        }
    }
};
inline std::size_t space(RelativeSpace s) { const auto i=static_cast<std::size_t>(s); require(i<3,"unknown relative coordinate space"); return i; }
inline void validate(const State& s,Guard& guard) {
    guard.entries(s.envelope); guard.value(Q{0}); guard.value(Q{1}); owned_gluing_detail::source(s.ambient,guard);
    for(std::size_t n=0;n<add(words(s.whole),words(s.sub));++n) guard.step();
}
inline Values values(const State& s,RelativeSpace which,core::Grade g,const Values& input,Guard& guard) {
    const auto& row=s.cells.at(space(which)).at(g.value); guard.dimension(row.size());
    Values result;
    for(const auto& [i,q]:input) { guard.step(); require(i<row.size(),"relative chain coordinate out of range"); guard.value(q); auto v=numeric::canonical_rational(q); guard.value(v); result.emplace(i,std::move(v)); }
    return result;
}
}
class OwnedRelativeChain;
class RelativeConnectingWitness;
class OwnedRelativePairMap;
class OwnedRelativePair {
public:
    using Values=owned_relative_detail::Values;
    static OwnedRelativePair prepare(core::RelationalComplex ambient,SubcomplexMask whole,SubcomplexMask sub,numeric::ExactActionGuard& guard) {
        using namespace owned_relative_detail;
        // Copies precede checkpoints: references into a moved mask cannot alter
        // the captured declaration during preparation.
        auto a=whole,b=sub;
        const auto retained=add(owned_gluing_detail::words(ambient),add(words(a),words(b)));
        const auto envelope=add(1,mul(retained,16)); guard.entries(envelope);
        owned_gluing_detail::source(ambient,guard); owned_relative_detail::mask(ambient,a,guard); owned_relative_detail::mask(ambient,b,guard);
        for(std::size_t k=0;k<ambient.grade_count();++k) for(std::size_t i=0;i<ambient.grade(grade(k)).cell_count();++i) {
            guard.step(); const core::CellId id{grade(k),static_cast<std::uint32_t>(i)};
            require(!b.contains(id)||a.contains(id),"relative subcomplex is not contained in whole");
        }
        owned_gluing_detail::chain(ambient,guard,envelope);
        // Precharge the existing constructor and complete coordinate tables.
        // The canonical constructor is synchronous; no new quotient builder.
        for(std::size_t n=0;n<mul(retained,8);++n) guard.step();
        return OwnedRelativePair(std::make_shared<const owned_relative_detail::State>(std::move(ambient),std::move(a),std::move(b),retained,envelope));
    }
    const core::RelationalComplex& ambient() const & { return state_->ambient; }
    const core::RelationalComplex& ambient() const &&=delete;
    const SubcomplexMask& mask(bool sub) const & { return sub?state_->sub:state_->whole; }
    const SubcomplexMask& mask(bool) const &&=delete;
    const RelativeChainComplex& reference() const & { return state_->reference; }
    const RelativeChainComplex& reference() const &&=delete;
    const std::vector<std::uint32_t>& coordinates(RelativeSpace s,core::Grade g) const & { return state_->cells.at(owned_relative_detail::space(s)).at(g.value); }
    const std::vector<std::uint32_t>& coordinates(RelativeSpace,core::Grade) const &&=delete;
    std::optional<std::uint32_t> local_index(RelativeSpace s,core::CellId ambient) const {
        const auto i=state_->inverse.at(owned_relative_detail::space(s)).at(ambient.grade.value).at(ambient.index);
        return i==owned_relative_detail::absent?std::nullopt:std::optional<std::uint32_t>{i};
    }
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    bool same_owner(const OwnedRelativePair& other) const { return state_==other.state_; }
    void validate(numeric::ExactActionGuard& g) const { const auto pin=state_; owned_relative_detail::validate(*pin,g); }
    OwnedRelativeChain chain(RelativeSpace,core::Grade,Values,numeric::ExactActionGuard&) const;
    OwnedRelativeChain project(OwnedRelativeChain,numeric::ExactActionGuard&) const;
    OwnedRelativeChain inject(OwnedRelativeChain,numeric::ExactActionGuard&) const;
    OwnedRelativeChain representative(OwnedRelativeChain,numeric::ExactActionGuard&) const;
    OwnedRelativeChain boundary(OwnedRelativeChain,numeric::ExactActionGuard&) const;
    RelativeConnectingWitness connecting_witness(OwnedRelativeChain,numeric::ExactActionGuard&) const;
private:
    friend class OwnedRelativeChain; friend class OwnedRelativePairMap;
    explicit OwnedRelativePair(std::shared_ptr<const owned_relative_detail::State> s):state_(std::move(s)){}
    OwnedRelativeChain convert(OwnedRelativeChain,RelativeSpace,RelativeSpace,numeric::ExactActionGuard&) const;
    std::shared_ptr<const owned_relative_detail::State> state_;
};
class OwnedRelativeChain {
public:
    OwnedRelativePair pair() const { return OwnedRelativePair(owner_); }
    RelativeSpace space() const { return space_; }
    core::Grade grade() const { return grade_; }
    const owned_relative_detail::Values& values() const & { return values_; }
    const owned_relative_detail::Values& values() const &&=delete;
private:
    friend class OwnedRelativePair; friend class OwnedRelativePairMap;
    OwnedRelativeChain(std::shared_ptr<const owned_relative_detail::State> s,RelativeSpace which,core::Grade g,owned_relative_detail::Values v)
        :owner_(std::move(s)),space_(which),grade_(g),values_(std::move(v)){}
    std::shared_ptr<const owned_relative_detail::State> owner_; RelativeSpace space_; core::Grade grade_; owned_relative_detail::Values values_;
};
inline OwnedRelativeChain OwnedRelativePair::chain(RelativeSpace which,core::Grade g,Values input,numeric::ExactActionGuard& guard) const {
    using namespace owned_relative_detail; const auto pin=state_; auto captured=input;
    owned_relative_detail::validate(*pin,guard); guard.entries(add(pin->envelope,mul(input.size(),3)));
    return {pin,which,g,values(*pin,which,g,captured,guard)};
}
inline OwnedRelativeChain OwnedRelativePair::convert(OwnedRelativeChain input,RelativeSpace from,RelativeSpace to,numeric::ExactActionGuard& guard) const {
    using namespace owned_relative_detail; const auto pin=state_; require(input.owner_==pin && input.space_==from,"relative coordinate map needs its actual owner and domain");
    owned_relative_detail::validate(*pin,guard); Values out;
    for(const auto& [i,q]:input.values_) {
        guard.step(); guard.value(q); guard.entries(add(pin->envelope,add(mul(input.values_.size(),2),add(out.size(),1))));
        const auto ambient=pin->cells.at(space(from)).at(input.grade_.value).at(i);
        const auto local=pin->inverse.at(space(to)).at(input.grade_.value).at(ambient);
        if(local!=absent) out.emplace(local,q);
    }
    return {pin,to,input.grade_,std::move(out)};
}
inline OwnedRelativeChain OwnedRelativePair::project(OwnedRelativeChain input,numeric::ExactActionGuard& guard) const { return convert(std::move(input),RelativeSpace::Whole,RelativeSpace::Quotient,guard); }
inline OwnedRelativeChain OwnedRelativePair::inject(OwnedRelativeChain input,numeric::ExactActionGuard& guard) const { return convert(std::move(input),RelativeSpace::Sub,RelativeSpace::Whole,guard); }
inline OwnedRelativeChain OwnedRelativePair::representative(OwnedRelativeChain input,numeric::ExactActionGuard& guard) const { return convert(std::move(input),RelativeSpace::Quotient,RelativeSpace::Whole,guard); }
inline OwnedRelativeChain OwnedRelativePair::boundary(OwnedRelativeChain input,numeric::ExactActionGuard& guard) const {
    using namespace owned_relative_detail; const auto pin=state_; require(input.owner_==pin,"relative boundary needs its actual owner"); require(input.grade_.value>0,"relative grade zero has no boundary");
    owned_relative_detail::validate(*pin,guard); Values out; const auto lower=grade(input.grade_.value-1u); const auto s=space(input.space_);
    const auto& arena=pin->ambient.grade(input.grade_);
    for(const auto& [i,q]:input.values_) {
        guard.value(q); const auto ambient=pin->cells[s].at(input.grade_.value).at(i); const auto [b,e]=arena.incidence_range(ambient);
        for(auto p=b;p<e;++p) {
            guard.step(); const auto local=pin->inverse[s].at(lower.value).at(arena.lower_index[p]);
            if(local==absent) { require(input.space_==RelativeSpace::Quotient,"retained boundary escaped a closed relative subcomplex"); continue; }
            guard.entries(add(pin->envelope,add(mul(input.values_.size(),2),add(out.size(),1))));
            auto& value=out.try_emplace(local,Q{0}).first->second;
            if(arena.aspects.get(pin->ambient.signature().existence_aspect(),p))
                value=guard.add(value,guard.mul(owned_gluing_detail::coefficient(pin->ambient,input.grade_,p,guard),q));
        }
    }
    return {pin,input.space_,lower,std::move(out)};
}
class RelativeConnectingWitness {
public:
    const OwnedRelativeChain& relative() const & { return relative_; }
    const OwnedRelativeChain& relative() const &&=delete;
    const OwnedRelativeChain& representative() const & { return representative_; }
    const OwnedRelativeChain& representative() const &&=delete;
    const OwnedRelativeChain& whole_boundary() const & { return whole_boundary_; }
    const OwnedRelativeChain& whole_boundary() const &&=delete;
    const OwnedRelativeChain& sub_boundary() const & { return sub_boundary_; }
    const OwnedRelativeChain& sub_boundary() const &&=delete;
private:
    friend class OwnedRelativePair;
    RelativeConnectingWitness(OwnedRelativeChain r,OwnedRelativeChain w,OwnedRelativeChain d,OwnedRelativeChain b)
        :relative_(std::move(r)),representative_(std::move(w)),whole_boundary_(std::move(d)),sub_boundary_(std::move(b)){}
    OwnedRelativeChain relative_,representative_,whole_boundary_,sub_boundary_;
};
inline RelativeConnectingWitness OwnedRelativePair::connecting_witness(OwnedRelativeChain input,numeric::ExactActionGuard& guard) const {
    using namespace owned_relative_detail; const auto pin=state_; require(input.owner_==pin && input.space_==RelativeSpace::Quotient,"connecting witness needs its actual quotient chain");
    std::size_t count{}; for(const auto& row:pin->cells[0]) { guard.step(); count=add(count,row.size()); }
    guard.entries(add(pin->envelope,mul(count,12)));
    const auto projected=OwnedRelativePair(pin).boundary(input,guard);
    for(const auto& [i,q]:projected.values_) { (void)i; guard.step(); require(q==0,"connecting witness needs a relative cycle"); }
    auto lifted=OwnedRelativePair(pin).representative(input,guard); auto d=OwnedRelativePair(pin).boundary(lifted,guard); Values sub;
    const auto fixed=add(pin->envelope,add(mul(input.values_.size(),2),add(projected.values_.size(),add(lifted.values_.size(),d.values_.size()))));
    for(const auto& [i,q]:d.values_) {
        guard.step(); guard.value(q); guard.entries(add(fixed,add(sub.size(),1)));
        const auto ambient=pin->cells[0].at(d.grade_.value).at(i); const auto local=pin->inverse[1].at(d.grade_.value).at(ambient);
        if(local==absent) require(q==0,"relative cycle boundary escaped the subcomplex"); else sub.emplace(local,q);
    }
    auto b=OwnedRelativeChain(pin,RelativeSpace::Sub,d.grade_,std::move(sub));
    if(b.grade_.value) { const auto check=OwnedRelativePair(pin).boundary(b,guard); for(const auto& [i,q]:check.values_) { (void)i; guard.step(); require(q==0,"connecting witness sub-boundary is not a cycle"); } }
    return {std::move(input),std::move(lifted),std::move(d),std::move(b)};
}
class OwnedRelativePairMap {
    struct State { OwnedRelativePair source,target; RetainedSourceMap map; std::size_t retained,envelope; };
public:
    static OwnedRelativePairMap prepare(OwnedRelativePair source,OwnedRelativePair target,RetainedSourceMap declaration,numeric::ExactActionGuard& guard) {
        using namespace owned_relative_detail; auto map=declaration; source.validate(guard); target.validate(guard);
        const auto retained=add(source.retained_words(),add(target.retained_words(),owned_gluing_detail::words(map)));
        const auto envelope=add(source.entry_envelope(),add(target.entry_envelope(),mul(owned_gluing_detail::words(map),16))); guard.entries(envelope);
        owned_gluing_detail::map(source.ambient(),target.ambient(),map,false,guard);
        for(std::size_t k=0;k<source.ambient().grade_count();++k) for(std::size_t i=0;i<map.cells[k].size();++i) {
            guard.step(); const core::CellId from{grade(k),static_cast<std::uint32_t>(i)},to{grade(k),map.cells[k][i]};
            require(!source.mask(false).contains(from)||target.mask(false).contains(to),"relative map does not preserve whole subcomplex");
            require(!source.mask(true).contains(from)||target.mask(true).contains(to),"relative map does not preserve quotient subcomplex");
        }
        return OwnedRelativePairMap(std::make_shared<const State>(State{std::move(source),std::move(target),std::move(map),retained,envelope}));
    }
    OwnedRelativePair source() const { return state_->source; }
    OwnedRelativePair target() const { return state_->target; }
    const RetainedSourceMap& declaration() const & { return state_->map; }
    const RetainedSourceMap& declaration() const &&=delete;
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    OwnedRelativeChain apply(OwnedRelativeChain input,numeric::ExactActionGuard& guard) const {
        using namespace owned_relative_detail; const auto pin=state_; require(pin->source.same_owner(input.pair()),"relative map needs its actual source pair");
        guard.entries(pin->envelope); pin->source.validate(guard); pin->target.validate(guard);
        for(std::size_t n=0;n<owned_gluing_detail::words(pin->map);++n) guard.step();
        Values out; const auto s=space(input.space_);
        for(const auto& [i,q]:input.values_) {
            guard.step(); guard.value(q); guard.entries(add(pin->envelope,add(mul(input.values_.size(),2),add(out.size(),1))));
            const auto from=pin->source.state_->cells[s].at(input.grade_.value).at(i);
            const auto to=pin->map.cells.at(input.grade_.value).at(from);
            const auto local=pin->target.state_->inverse[s].at(input.grade_.value).at(to);
            if(local==absent) { require(input.space_==RelativeSpace::Quotient,"relative map escaped its target subcomplex"); continue; }
            auto& v=out.try_emplace(local,Q{0}).first->second; v=guard.add(v,q);
        }
        return {pin->target.state_,input.space_,input.grade_,std::move(out)};
    }
private:
    explicit OwnedRelativePairMap(std::shared_ptr<const State> s):state_(std::move(s)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::algebra
