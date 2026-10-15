#pragma once
#include "rfc/algebra/morphism.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>

namespace rfc::algebra {
// Complete maps of retained cells AND occurrences. Occurrence indices refer to
// the actual source arenas, never to aggregated numerical boundary entries.
struct RetainedSourceMap {
    std::vector<std::vector<std::uint32_t>> cells;
    std::vector<std::vector<std::size_t>> occurrences;
};
enum class GluingSource { Common, Left, Right, Glued };
struct RelationalGluingDeclaration {
    std::uint64_t key{}, revision{};
    RetainedSourceMap common_left, common_right, left_glued, right_glued;
};
namespace owned_gluing_detail {
using Q = numeric::Rational;
using Guard = numeric::ExactActionGuard;
using Values = std::map<std::uint32_t, Q>;
using Matrix = numeric::ExactMatrix;
inline void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
inline std::size_t add(std::size_t a, std::size_t b) { return machine::checked_size_add(a,b); }
inline std::size_t mul(std::size_t a, std::size_t b) { return machine::checked_size_mul(a,b); }
inline core::Grade grade(std::size_t k) {
    if (k > 65535u) throw std::overflow_error("owned gluing grade overflow");
    return core::Grade{static_cast<std::uint16_t>(k)};
}
inline std::size_t words(const core::RelationalComplex& c) {
    auto n = c.grade_count();
    for (std::size_t i=0;i<c.signature().aspects().size();++i)
        n=add(n,add(1,c.signature().aspects().at(static_cast<composite::AspectId>(i)).name.size()));
    for (std::size_t t=0;t<c.signature().type_count();++t) {
        const auto& d=c.signature().type(static_cast<core::RelationTypeId>(t));
        n=add(n,add(add(1,d.name.size()),d.constraints.rules.size()));
    }
    for (std::size_t k=0;k<c.grade_count();++k) {
        const auto& a=c.grade(grade(k));
        n=add(n,add(mul(a.cell_count(),4),mul(a.incidence_count(),add(7,c.signature().aspects().size()))));
    }
    return n;
}
inline std::size_t words(const RetainedSourceMap& m) {
    auto n=add(m.cells.size(),m.occurrences.size());
    for (const auto& a:m.cells) n=add(n,a.size());
    for (const auto& a:m.occurrences) n=add(n,a.size());
    return n;
}
inline Q coefficient(const core::RelationalComplex& c, core::Grade g, std::size_t p, Guard& guard) {
    const auto& a=c.grade(g); const auto& lower=c.grade(grade(g.value-1u));
    const Q lattice{a.lattice_coefficient.at(p)}; guard.value(lattice);
    return guard.div(guard.mul(lower.cell_scales.at(a.lower_index.at(p)),lattice),a.cell_scales.at(a.upper_index.at(p)));
}
inline void source(const core::RelationalComplex& c, Guard& guard) {
    guard.dimension(c.grade_count()); guard.dimension(c.signature().type_count()); guard.dimension(c.signature().aspects().size());
    for (std::size_t t=0;t<c.signature().type_count();++t)
        for (const auto& r:c.signature().type(static_cast<core::RelationTypeId>(t)).constraints.rules) { guard.step(); guard.value(r.value); }
    for (std::size_t k=0;k<c.grade_count();++k) {
        const auto g=grade(k); const auto& a=c.grade(g); guard.dimension(a.cell_count());
        for (const auto& q:a.cell_scales) { guard.step(); guard.value(q); }
        for (std::size_t p=0;p<a.incidence_count();++p) {
            guard.step(); (void)coefficient(c,g,p,guard);
            for (std::size_t j=0;j<c.signature().aspects().size();++j) { guard.step(); (void)a.aspects.get(static_cast<composite::AspectId>(j),p); }
        }
    }
}
inline Matrix boundary(const core::RelationalComplex& c,std::size_t k,Guard& guard) {
    const auto g=grade(k); const auto& a=c.grade(g);
    Matrix out(c.grade(grade(k-1)).cell_count(),a.cell_count());
    for (std::size_t p=0;p<a.incidence_count();++p) {
        guard.step(); if (!a.aspects.get(c.signature().existence_aspect(),p)) continue;
        out.set(a.lower_index[p],a.upper_index[p],guard.add(out.get(a.lower_index[p],a.upper_index[p]),coefficient(c,g,p,guard)));
        guard.entries(out.nonzeros());
    }
    return out;
}
inline void chain(const core::RelationalComplex& c,Guard& guard,std::size_t resident) {
    for (std::size_t k=2;k<c.grade_count();++k) {
        const auto a=boundary(c,k-1,guard),b=boundary(c,k,guard);
        const auto fixed=add(resident,add(a.nonzeros(),b.nonzeros())); guard.entries(fixed);
        Matrix out(a.rows(),b.cols());
        for (const auto& [ij,x]:a.entries()) for (const auto& [kl,y]:b.entries()) {
            guard.step(); if (ij.second!=kl.first) continue;
            guard.entries(add(fixed,add(out.nonzeros(),1)));
            out.set(ij.first,kl.second,guard.add(out.get(ij.first,kl.second),guard.mul(x,y)));
        }
        require(!out.nonzeros(),"owned gluing requires the complete strict chain family");
    }
}
inline void map(const core::RelationalComplex& a,const core::RelationalComplex& b,const RetainedSourceMap& m,
                bool injective,Guard& guard) {
    require(a.semantic_scale()==b.semantic_scale(),"gluing source map changes semantic scale");
    require(m.cells.size()==a.grade_count() && m.occurrences.size()==a.grade_count() && b.grade_count()>=a.grade_count(),
            "gluing source map must include all declared grades");
    for (std::size_t k=0;k<a.grade_count();++k) {
        const auto g=grade(k); const auto& x=a.grade(g); const auto& y=b.grade(g);
        require(m.cells[k].size()==x.cell_count() && m.occurrences[k].size()==x.incidence_count(),"gluing source map omits retained data");
        std::set<std::uint32_t> cells; std::set<std::size_t> occurrences;
        for (std::size_t i=0;i<x.cell_count();++i) {
            guard.step(); const auto j=m.cells[k][i]; require(j<y.cell_count(),"gluing source cell map out of range");
            if (injective) require(cells.insert(j).second,"gluing source embedding is not injective");
            if (!k) continue;
            require(x.relation_types[i]==y.relation_types[j],"gluing source map changes primary type");
            const auto [xb,xe]=x.incidence_range(i); const auto [yb,ye]=y.incidence_range(j);
            require(xe-xb==ye-yb,"gluing source map loses a complete occurrence fiber");
            std::set<std::size_t> fiber;
            const bool ordered=a.signature().type(x.relation_types[i]).is_attaching_loop();
            for (auto p=xb;p<xe;++p) {
                guard.step(); const auto q=m.occurrences[k][p];
                require(q>=yb && q<ye && fiber.insert(q).second,"gluing source occurrence map changes ownership or fiber multiplicity");
                if (injective) require(occurrences.insert(q).second,"gluing occurrence embedding is not injective");
                require(!ordered || p-xb==q-yb,"gluing source map changes attaching order");
                require(m.cells[k-1].at(x.lower_index[p])==y.lower_index[q],"gluing source map changes boundary attachment");
                require(coefficient(a,g,p,guard)==coefficient(b,g,q,guard),"gluing source map changes exact coefficient");
                for (std::size_t t=0;t<a.signature().aspects().size();++t) {
                    guard.step(); const auto aspect=static_cast<composite::AspectId>(t);
                    require(x.aspects.get(aspect,p)==y.aspects.get(aspect,q),"gluing source map changes retained aspects");
                }
            }
            // Account the existing canonical morphism's unordered-fiber matching
            // before invoking it. The declared occurrence map remains distinct.
            const auto cost=mul(mul(xe-xb,xe-xb),add(a.signature().aspects().size(),2));
            for (std::size_t n=0;n<cost;++n) guard.step();
        }
    }
    // Reuse the canonical strict signature/type/morphism contract. No length or
    // current numerical response is accepted in place of that whole contract.
    for (std::size_t n=0;n<add(words(a),words(b));++n) guard.step();
    (void)RelationalMorphism(a,b,m.cells);
}
struct State {
    std::array<core::RelationalComplex,4> sources;
    RelationalGluingDeclaration declaration;
    std::size_t retained{}, envelope{};
};
inline const core::RelationalComplex& source(const State& s,GluingSource which) { return s.sources.at(static_cast<std::size_t>(which)); }
inline void validate(const State& s,Guard& guard) {
    guard.entries(s.envelope); guard.value(Q{0}); guard.value(Q{1});
    for (const auto& c:s.sources) source(c,guard);
    for (const auto* m:{&s.declaration.common_left,&s.declaration.common_right,&s.declaration.left_glued,&s.declaration.right_glued})
        for (std::size_t n=0;n<words(*m);++n) guard.step();
}
inline void values(const core::RelationalComplex& c,core::Grade g,const Values& v,Guard& guard,bool complete=false) {
    const auto n=c.grade(g).cell_count(); require(!complete || v.size()==n,"same-field gluing needs complete local sections");
    for (const auto& [i,q]:v) { guard.step(); require(i<n,"gluing field cell outside its actual source"); guard.value(q); }
}
} // namespace owned_gluing_detail

class OwnedRelationalGluing;
class OwnedGluingMorphism;
class OwnedGluedChain {
public:
    core::Grade grade() const noexcept { return grade_; }
    const owned_gluing_detail::Values& values() const & noexcept { return values_; }
    const owned_gluing_detail::Values& values() const && = delete;
    OwnedRelationalGluing gluing() const;
private:
    friend class OwnedRelationalGluing; friend class OwnedGluingMorphism;
    OwnedGluedChain(std::shared_ptr<const owned_gluing_detail::State> owner,core::Grade g,owned_gluing_detail::Values values)
        :owner_(std::move(owner)),grade_(g),values_(std::move(values)) {}
    std::shared_ptr<const owned_gluing_detail::State> owner_;
    core::Grade grade_;
    owned_gluing_detail::Values values_;
};
// Certifies the complete retained pushout constructed by a source constructor.
// The native atlas supplies that constructor; this layer owns its whole source
// square and validates the quotient, inclusions and universal source-map action.
class OwnedRelationalGluing {
public:
    using Values=owned_gluing_detail::Values;
    static OwnedRelationalGluing prepare(core::RelationalComplex common,core::RelationalComplex left,
        core::RelationalComplex right,core::RelationalComplex glued,RelationalGluingDeclaration input_declaration,
        numeric::ExactActionGuard& guard) {
        using namespace owned_gluing_detail;
        auto declaration=input_declaration; // Freeze mutable map words before checkpoints.
        require(declaration.key!=0,"gluing needs a declared constructor identity");
        auto retained=add(add(words(common),words(left)),add(words(right),words(glued)));
        for (const auto* m:{&declaration.common_left,&declaration.common_right,&declaration.left_glued,&declaration.right_glued}) retained=add(retained,words(*m));
        State s{{std::move(common),std::move(left),std::move(right),std::move(glued)},std::move(declaration),retained,add(1,mul(retained,8))};
        owned_gluing_detail::validate(s,guard);
        for (const auto& c:s.sources) owned_gluing_detail::chain(c,guard,s.envelope);
        const auto& k=s.sources[0]; const auto& a=s.sources[1]; const auto& b=s.sources[2]; const auto& p=s.sources[3]; const auto& d=s.declaration;
        map(k,a,d.common_left,true,guard); map(k,b,d.common_right,true,guard);
        map(a,p,d.left_glued,true,guard); map(b,p,d.right_glued,true,guard);
        require(p.grade_count()==std::max(a.grade_count(),b.grade_count()),"gluing must preserve exactly the participant grade inventory");
        // Coverage and intersection are checked on occurrence identities as well
        // as cells. No additional equal-key or equal-value identification occurs.
        for (std::size_t g=0;g<p.grade_count();++g) {
            const auto check=[&](const auto& ac,const auto& bc,const auto& ka,const auto& kb,std::size_t size) {
                std::set<std::size_t> ai,bi,common_image;
                for (const auto x:ac) { guard.step(); ai.insert(x); }
                for (const auto x:bc) { guard.step(); bi.insert(x); }
                require(ka.size()==kb.size(),"gluing common source map shape");
                for (std::size_t i=0;i<ka.size();++i) { guard.step(); const auto x=ac.at(ka[i]); require(x==bc.at(kb[i]),"gluing square fails on retained common data"); common_image.insert(x); }
                for (const auto x:ai) { guard.step(); require(bi.contains(x)==common_image.contains(x),"gluing identifies data outside the declared interface"); }
                const auto total=add(ai.size(),bi.size())-common_image.size();
                require(total==size,"gluing source contains data outside the two participant images");
            };
            const std::vector<std::uint32_t> empty_cells;
            const std::vector<std::size_t> empty_occ;
            const auto& kc=g<k.grade_count()?d.common_left.cells[g]:empty_cells;
            const auto& kr=g<k.grade_count()?d.common_right.cells[g]:empty_cells;
            const auto& ko=g<k.grade_count()?d.common_left.occurrences[g]:empty_occ;
            const auto& ro=g<k.grade_count()?d.common_right.occurrences[g]:empty_occ;
            check(g<a.grade_count()?d.left_glued.cells[g]:empty_cells,g<b.grade_count()?d.right_glued.cells[g]:empty_cells,kc,kr,p.grade(grade(g)).cell_count());
            check(g<a.grade_count()?d.left_glued.occurrences[g]:empty_occ,g<b.grade_count()?d.right_glued.occurrences[g]:empty_occ,ko,ro,p.grade(grade(g)).incidence_count());
        }
        return OwnedRelationalGluing(std::make_shared<const State>(std::move(s)));
    }
    const core::RelationalComplex& source(GluingSource which=GluingSource::Glued) const & { return owned_gluing_detail::source(*state_,which); }
    const core::RelationalComplex& source(GluingSource=GluingSource::Glued) const && = delete;
    const RelationalGluingDeclaration& declaration() const & { return state_->declaration; }
    const RelationalGluingDeclaration& declaration() const && = delete;
    bool same_owner(const OwnedRelationalGluing& other) const noexcept { return state_==other.state_; }
    std::size_t retained_words() const noexcept { return state_->retained; }
    std::size_t entry_envelope() const noexcept { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin=state_; owned_gluing_detail::validate(*pin,guard); }
    OwnedGluedChain chain(core::Grade g,Values input_values,numeric::ExactActionGuard& guard) const {
        using namespace owned_gluing_detail; const auto pin=state_; auto values=input_values; owned_gluing_detail::validate(*pin,guard);
        guard.entries(add(pin->envelope,mul(values.size(),2))); owned_gluing_detail::values(pin->sources[3],g,values,guard);
        for (auto& [i,q]:values) { (void)i; guard.step(); q=numeric::canonical_rational(q); guard.value(q); }
        return {pin,g,std::move(values)};
    }
    OwnedGluedChain include(bool right,core::Grade g,Values input_values,numeric::ExactActionGuard& guard) const {
        using namespace owned_gluing_detail; const auto pin=state_; auto values=input_values; owned_gluing_detail::validate(*pin,guard);
        owned_gluing_detail::values(pin->sources[right?2:1],g,values,guard); Values out;
        const auto& m=(right?pin->declaration.right_glued:pin->declaration.left_glued).cells.at(g.value);
        guard.entries(add(pin->envelope,mul(values.size(),3)));
        for (const auto& [i,q]:values) { guard.step(); out.emplace(m.at(i),numeric::canonical_rational(q)); }
        return {pin,g,std::move(out)};
    }
    OwnedGluedChain glue_sections(core::Grade g,Values input_left,Values input_right,numeric::ExactActionGuard& guard) const {
        using namespace owned_gluing_detail; const auto pin=state_; auto left=input_left,right=input_right; owned_gluing_detail::validate(*pin,guard);
        owned_gluing_detail::values(pin->sources[1],g,left,guard,true); owned_gluing_detail::values(pin->sources[2],g,right,guard,true);
        guard.entries(add(pin->envelope,mul(add(left.size(),right.size()),3))); Values out;
        for (const bool r:{false,true}) {
            const auto& input=r?right:left; const auto& m=(r?pin->declaration.right_glued:pin->declaration.left_glued).cells.at(g.value);
            for (const auto& [i,q]:input) {
                guard.step(); const auto value=numeric::canonical_rational(q); const auto [it,inserted]=out.emplace(m.at(i),value);
                require(inserted || it->second==value,"same-field gluing obstruction on the common interface");
            }
        }
        return {pin,g,std::move(out)};
    }
    OwnedGluedChain boundary(OwnedGluedChain input,numeric::ExactActionGuard& guard) const {
        using namespace owned_gluing_detail; const auto pin=state_; require(pin==input.owner_,"gluing boundary needs its actual chain owner");
        owned_gluing_detail::validate(*pin,guard); owned_gluing_detail::values(pin->sources[3],input.grade_,input.values_,guard);
        require(input.grade_.value>0,"gluing grade zero has no boundary"); Values out;
        const auto& a=pin->sources[3].grade(input.grade_);
        for (const auto& [i,q]:input.values_) {
            const auto [begin,end]=a.incidence_range(i);
            for (auto p=begin;p<end;++p) {
                guard.step(); guard.entries(add(pin->envelope,add(mul(input.values_.size(),2),add(out.size(),1))));
                auto& v=out.try_emplace(a.lower_index[p],Q{0}).first->second;
                if (a.aspects.get(pin->sources[3].signature().existence_aspect(),p)) v=guard.add(v,guard.mul(q,coefficient(pin->sources[3],input.grade_,p,guard)));
            }
        }
        return {pin,grade(input.grade_.value-1u),std::move(out)};
    }
private:
    friend class OwnedGluedChain; friend class OwnedGluingMorphism;
    explicit OwnedRelationalGluing(std::shared_ptr<const owned_gluing_detail::State> state):state_(std::move(state)) {}
    std::shared_ptr<const owned_gluing_detail::State> state_;
};
inline OwnedRelationalGluing OwnedGluedChain::gluing() const { return OwnedRelationalGluing(owner_); }

class OwnedGluingMorphism {
    struct State { OwnedRelationalGluing gluing; core::RelationalComplex target; RetainedSourceMap left,right,mediator; std::size_t words{},envelope{}; };
public:
    static OwnedGluingMorphism prepare(OwnedRelationalGluing gluing,core::RelationalComplex target,
        RetainedSourceMap input_left,RetainedSourceMap input_right,numeric::ExactActionGuard& guard) {
        using namespace owned_gluing_detail;
        auto left=input_left,right=input_right;
        gluing.validate(guard); source(target,guard);
        const auto retained=add(gluing.retained_words(),add(words(target),add(words(left),words(right))));
        const auto envelope=add(gluing.entry_envelope(),mul(retained,8)); guard.entries(envelope); chain(target,guard,envelope);
        owned_gluing_detail::map(gluing.source(GluingSource::Left),target,left,false,guard); owned_gluing_detail::map(gluing.source(GluingSource::Right),target,right,false,guard);
        const auto& p=gluing.source(); const auto& d=gluing.declaration(); RetainedSourceMap mediator;
        for (std::size_t k=0;k<p.grade_count();++k) {
            const auto g=grade(k); const auto absent=std::numeric_limits<std::size_t>::max();
            std::vector<std::optional<std::uint32_t>> cm(p.grade(g).cell_count());
            std::vector<std::size_t> om(p.grade(g).incidence_count(),absent);
            for (const bool r:{false,true}) {
                const auto& in=r?right:left; const auto& inclusion=r?d.right_glued:d.left_glued;
                if (k>=in.cells.size()) continue;
                for (std::size_t i=0;i<in.cells[k].size();++i) { guard.step(); auto& value=cm.at(inclusion.cells[k][i]); require(!value || *value==in.cells[k][i],"source maps disagree on a common cell"); value=in.cells[k][i]; }
                for (std::size_t i=0;i<in.occurrences[k].size();++i) { guard.step(); auto& value=om.at(inclusion.occurrences[k][i]); require(value==absent || value==in.occurrences[k][i],"source maps disagree on a common occurrence"); value=in.occurrences[k][i]; }
            }
            std::vector<std::uint32_t> row; row.reserve(cm.size());
            for (const auto i:cm) { guard.step(); require(i.has_value(),"uncovered gluing mediator cell"); row.push_back(*i); }
            for (const auto i:om) { guard.step(); require(i!=absent,"uncovered gluing mediator occurrence"); }
            mediator.cells.push_back(std::move(row)); mediator.occurrences.push_back(std::move(om));
        }
        owned_gluing_detail::map(p,target,mediator,false,guard);
        const auto stored=add(retained,words(mediator)); guard.entries(add(gluing.entry_envelope(),mul(stored,8)));
        const auto final_envelope=add(envelope,mul(words(mediator),8));
        return OwnedGluingMorphism(std::make_shared<const State>(State{std::move(gluing),std::move(target),std::move(left),std::move(right),std::move(mediator),stored,final_envelope}));
    }
    OwnedRelationalGluing gluing() const { return state_->gluing; }
    const core::RelationalComplex& target() const & { return state_->target; }
    const core::RelationalComplex& target() const && = delete;
    const RetainedSourceMap& map() const & { return state_->mediator; }
    const RetainedSourceMap& map() const && = delete;
    const RetainedSourceMap& participant_map(bool right) const & { return right?state_->right:state_->left; }
    const RetainedSourceMap& participant_map(bool) const && = delete;
    std::size_t retained_words() const noexcept { return state_->words; }
    std::size_t entry_envelope() const noexcept { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const {
        const auto pin=state_; guard.entries(pin->envelope); pin->gluing.validate(guard); owned_gluing_detail::source(pin->target,guard);
        for (const auto* m:{&pin->left,&pin->right,&pin->mediator}) for (std::size_t n=0;n<owned_gluing_detail::words(*m);++n) guard.step();
    }
    owned_gluing_detail::Values apply(OwnedGluedChain input,numeric::ExactActionGuard& guard) const {
        using namespace owned_gluing_detail; const auto pin=state_; require(pin->gluing.state_==input.owner_,"gluing mediator needs its actual chain owner");
        OwnedGluingMorphism(pin).validate(guard); values(pin->gluing.source(),input.grade_,input.values_,guard); Values out;
        for (const auto& [i,q]:input.values_) {
            guard.step(); guard.entries(add(pin->envelope,add(mul(input.values_.size(),2),add(out.size(),1))));
            auto& v=out.try_emplace(pin->mediator.cells.at(input.grade_.value).at(i),Q{0}).first->second;
            v=guard.add(v,q);
        }
        return out;
    }
private:
    explicit OwnedGluingMorphism(std::shared_ptr<const State> state):state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::algebra
