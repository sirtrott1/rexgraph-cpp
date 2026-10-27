#pragma once
#include "rfc/storage/local_stalk_chart.hpp"
#include "rfc/algebra/owned_gluing.hpp"
#include "rfc/field/factored_action.hpp"
#include <numeric>

namespace rfc::storage {
// Explicit embedded interface; local indices refer to the retained patch arenas.
// Equal source keys in different patches do not identify anything implicitly.
struct LocalFieldInterface {
    core::OwnedRelationalSource common;
    std::size_t left{}, right{};
    algebra::RetainedSourceMap common_left, common_right;
};
struct GluedStalkRange {
    std::size_t patch{};
    StableCellKey cell;
    std::uint64_t coordinates{};
    composite::Declaration declaration;
    std::size_t offset{}, dimension{};
    bool operator==(const GluedStalkRange&) const = default;
};
namespace local_gluing_detail {
using Q=numeric::Rational;
using S=numeric::SparseRationalVector;
using Guard=numeric::ExactActionGuard;
inline void require(bool b,const char* m){if(!b)throw std::invalid_argument(m);}
inline std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
inline core::Grade grade(std::size_t k){if(k>65535)throw std::length_error("gluing grade limit");return {static_cast<std::uint16_t>(k)};}
struct Union {
    std::vector<std::size_t> p;
    explicit Union(std::size_t n):p(n){std::iota(p.begin(),p.end(),0);}
    std::size_t root(std::size_t a,Guard& g){while(p[a]!=a){g.step();p[a]=p[p[a]];a=p[a];}return a;}
    void join(std::size_t a,std::size_t b,Guard& g){a=root(a,g);b=root(b,g);if(a!=b)p[std::max(a,b)]=std::min(a,b);}
};
struct Origin {std::size_t patch{}, local{};};
struct Route {std::size_t patch{}, slot{}, upper{}, lower{};};
struct Grade {
    std::vector<GluedStalkRange> layout;
    std::vector<Origin> cells;
    std::vector<std::vector<std::size_t>> images;
    std::vector<Route> routes;
    std::vector<std::vector<std::size_t>> from_upper, from_lower;
    std::vector<std::size_t> offsets;
};
struct State {
    std::vector<DeclaredLocalFieldBinding> patches;
    std::vector<LocalFieldInterface> interfaces;
    std::vector<Grade> grades;
    std::size_t records{};
    std::vector<LocalStalkChart> charts;
    std::map<std::pair<std::size_t,core::CellId>,std::size_t> chart_index;
    std::map<std::tuple<std::size_t,std::uint16_t,std::size_t>,StalkChartMap> transports;
};
inline const core::RelationalComplex& source(const State& s,std::size_t p){return s.patches.at(p).source_owner().source();}
inline const DeclaredStalkBinding& stalk(const State& s,std::size_t p,core::CellId cell){
    const auto key=stable_cell_key(source(s,p),cell);
    const auto& family=s.patches[p].contract().downward.stalks;
    for(const auto& b:family)if(b.carrier.cell==key)return b;
    throw std::logic_error("bound patch omitted a stalk");
}
inline const LocalStalkChart* chart(const State& s,std::size_t p,core::CellId c){
    const auto at=s.chart_index.find({p,c});return at==s.chart_index.end()?nullptr:&s.charts.at(at->second);
}
inline StalkChartMap raw_map(const State& s,std::size_t p,std::size_t slot,core::Grade k){
    const auto& f=s.patches[p].fibers().fibers();
    if(const auto& m=f.transport_override(k,slot))return StalkChartMap::explicit_map(*m);
    const auto& a=source(s,p).grade(k);
    return StalkChartMap::identity(f.stalks().dimension({k,a.upper_index[slot]}),source(s,p).incidence_coefficient(k,slot));
}
inline StalkChartMap realized_map(const State& s,std::size_t p,std::size_t slot,core::Grade k){
    const auto at=s.transports.find({p,k.value,slot});return at==s.transports.end()?raw_map(s,p,slot,k):at->second;
}
inline bool same_map(const State& s,std::size_t p,std::size_t a,std::size_t q,std::size_t b,core::Grade k,Guard& g){
    if(s.transports.contains({p,k.value,a}) || s.transports.contains({q,k.value,b}))
        return realized_map(s,p,a,k).equals(realized_map(s,q,b,k),g);
    const auto& f=s.patches[p].fibers().fibers();const auto& h=s.patches[q].fibers().fibers();
    const auto& x=f.transport_override(k,a);const auto& y=h.transport_override(k,b);
    if(x)g.matrix(*x);
    if(y)g.matrix(*y);
    if(x && y)return x->rows()==y->rows() && x->cols()==y->cols() && x->entries()==y->entries();
    const Q c=f.complex().incidence_coefficient(k,a),d=h.complex().incidence_coefficient(k,b);g.value(c);g.value(d);
    if(!x && !y)return c==d;
    const auto& m=x?*x:*y;const Q scalar=x?d:c;
    if(m.rows()!=m.cols())return false;
    if(scalar==0)return m.nonzeros()==0;
    if(m.nonzeros()!=m.rows())return false;
    for(const auto& [ij,v]:m.entries()){g.step();if(ij.first!=ij.second || v!=scalar)return false;}
    return true;
}
inline S sparse_input(const numeric::RationalVector& x,Guard& g){
    g.vector(x);std::vector<numeric::SparseRationalEntry> es;
    for(std::size_t i=0;i<x.size();++i){g.step();if(x[i]!=0)es.push_back({i,x[i]});g.entries(es.size());}
    return S(x.size(),std::move(es));
}
inline S part(const S& x,std::size_t offset,std::size_t n,Guard& g){
    const auto& es=x.entries();auto at=std::lower_bound(es.begin(),es.end(),offset,[](const auto& a,auto b){return a.index<b;});
    std::vector<numeric::SparseRationalEntry> out;
    for(;at!=es.end() && at->index-offset<n;++at){g.step();out.push_back({at->index-offset,at->value});g.entries(out.size());}
    return S(n,std::move(out));
}
inline void accumulate(std::map<std::size_t,Q>& out,std::size_t i,const Q& x,Guard& g,std::size_t held){
    g.entries(add(held,add(out.size(),1)));const auto it=out.find(i);auto q=g.add(it==out.end()?Q{0}:it->second,x);
    if(q==0)out.erase(i);else out[i]=std::move(q);
}
inline S action(const State& s,core::Grade k,const S& x,bool transpose,std::optional<std::size_t> selected,Guard& g){
    require(k.value>0 && k.value<s.grades.size(),"glued boundary grade");
    const auto& routes=s.grades[k.value];const auto& input=s.grades[k.value-(transpose?1u:0u)];const auto& output=s.grades[k.value-(transpose?0u:1u)];
    require(x.size()==input.offsets.back(),"glued action input carrier");g.sparse_vector(x);g.dimension(output.offsets.back());
    std::map<std::size_t,Q> out;
    for(auto it=x.entries().begin();it!=x.entries().end();){
        g.step();const auto cell=static_cast<std::size_t>(std::upper_bound(input.offsets.begin(),input.offsets.end(),it->index)-input.offsets.begin()-1);
        const auto local=part(x,input.offsets[cell],input.layout[cell].dimension,g);
        const auto& visits=transpose?routes.from_lower.at(cell):routes.from_upper.at(cell);
        for(const auto id:visits){
            g.step();const auto& route=routes.routes[id];if(selected && route.patch!=*selected)continue;
            const auto& fibers=s.patches[route.patch].fibers().fibers();const auto& c=fibers.complex();
            if(!c.grade(k).aspects.get(c.signature().existence_aspect(),route.slot))continue;
            const auto target=transpose?route.upper:route.lower;const auto off=output.offsets[target];
            const auto held=add(s.records,add(x.nonzeros(),local.nonzeros()));
            if(const auto mapped=s.transports.find({route.patch,k.value,route.slot});mapped!=s.transports.end()){
                const auto y=mapped->second.apply(local,g,transpose);
                for(const auto& e:y.entries())accumulate(out,add(off,e.index),e.value,g,add(held,y.nonzeros()));
            }else if(const auto& map=fibers.transport_override(k,route.slot)){
                // Already-realized overrides are never multiplied by the source coefficient again.
                for(const auto& [ij,v]:map->entries()){
                    g.step();const auto src=transpose?ij.first:ij.second,dst=transpose?ij.second:ij.first;
                    const auto at=std::lower_bound(local.entries().begin(),local.entries().end(),src,[](const auto& a,auto b){return a.index<b;});
                    if(at!=local.entries().end() && at->index==src)accumulate(out,add(off,dst),g.mul(v,at->value),g,held);
                }
            }else{
                const auto coefficient=c.incidence_coefficient(k,route.slot);g.value(coefficient);
                if(coefficient!=0)for(const auto& e:local.entries())accumulate(out,add(off,e.index),g.mul(coefficient,e.value),g,held);
            }
        }
        const auto end=input.offsets[cell+1];it=std::lower_bound(it,x.entries().end(),end,[](const auto& a,auto b){return a.index<b;});
    }
    return numeric::checked_sparse_detail::vector(output.offsets.back(),std::move(out),g);
}
} // namespace local_gluing_detail

// A resident embedded-interface view. Default: coordinate-identical.
// The explicit chart overload admits checked complete stalk isomorphisms. The result retains
// local source/map owners plus an identity quotient index, NOT a global source
// arena or assembled matrix. General linear stalk pushouts and disk residency are
// separate profiles. Complete source fibers and realized maps are checked on the
// interface, including inactive/zero occurrences; chain flatness is NOT inferred.
class GluedLocalFieldView {
    using State=local_gluing_detail::State;
    using S=local_gluing_detail::S;
    using Guard=local_gluing_detail::Guard;
    std::shared_ptr<const State> state_;
public:
    GluedLocalFieldView(std::vector<DeclaredLocalFieldBinding> patches,std::vector<LocalFieldInterface> interfaces,
                        numeric::ExactActionLimits limits={},std::function<void()> checkpoint={})
        :GluedLocalFieldView(std::move(patches),std::move(interfaces),std::vector<LocalStalkChart>{},limits,std::move(checkpoint)){}
    // Opt-in complete chart isomorphisms. Rectangular partial identifications
    // and observation pullbacks are NOT admitted by this constructor.
    GluedLocalFieldView(std::vector<DeclaredLocalFieldBinding> patches,std::vector<LocalFieldInterface> interfaces,
                        std::vector<LocalStalkChart> charts,numeric::ExactActionLimits limits,std::function<void()> checkpoint={}){
        using namespace local_gluing_detail;
        require(!patches.empty(),"gluing needs local patches");
        auto s=std::make_shared<State>(State{patches,interfaces,{},0,charts,{},{}});
        // Freeze all supplied declarations before invoking host checkpoints.
        Guard g(limits,std::move(checkpoint));g.step();g.entries(s->patches.size());
        std::size_t grades=0;
        for(std::size_t p=0;p<s->patches.size();++p){
            const auto& c=source(*s,p);require(c.grade_count()==source(*s,0).grade_count(),"declare the same grade inventory, including empty grades, in every patch");grades=std::max(grades,c.grade_count());
            s->records=add(s->records,algebra::owned_gluing_detail::words(c));
            s->records=add(s->records,s->patches[p].contract().downward.stalks.size());g.entries(s->records);
            for(std::size_t k=1;k<c.grade_count();++k)for(std::size_t i=0;i<c.grade(grade(k)).incidence_count();++i){
                g.step();const auto& f=s->patches[p].fibers().fibers();f.validate_transport(grade(k),i);
                if(const auto& m=f.transport_override(grade(k),i)){g.matrix(*m);s->records=add(s->records,m->nonzeros());g.entries(s->records);}
            }
        }
        for(std::size_t n=0;n<s->charts.size();++n){
            const auto& c=s->charts[n];g.step();require(c.patch<s->patches.size() && c.local.carrier.cell.has_value(),"chart patch/cell absent");
            StableSourceIndex index(source(*s,c.patch));const auto cell=index.cell(*c.local.carrier.cell);
            require(cell.has_value(),"chart local cell absent");const auto& local=stalk(*s,c.patch,*cell);
            require(local.carrier==c.local.carrier && local.declaration==c.local.declaration,"chart expected local definition/carrier differs");
            require(s->chart_index.emplace(std::make_pair(c.patch,*cell),n).second,"duplicate local chart");
            const auto dim=local.carrier.domain.logical_size();
            require(c.common_coordinates!=0 && c.common_declaration.kind==composite::DeclarationKind::RationalTensor &&
                    c.common_declaration.axes.size()==1 && c.common_declaration.axes[0].extent==dim,"chart target needs full positive one-axis rational coordinates");
            // Validate complete definitions together, rejecting same-ID conflicts.
            (void)composite::DeclarationRegistry({local.declaration,c.common_declaration});
            require(c.to_common.rows()==dim && c.to_common.cols()==dim && c.from_common.rows()==dim && c.from_common.cols()==dim,"rectangular charts require a separate interface profile");
            const auto identity=StalkChartMap::identity(dim);
            require(c.to_common.multiply(c.from_common,g).equals(identity,g) && c.from_common.multiply(c.to_common,g).equals(identity,g),"chart maps are not two-sided inverses");
            s->records=add(s->records,add(c.to_common.stored_entries(),c.from_common.stored_entries()));g.entries(s->records);
        }
        // Transform every incidence touching a changed chart, including inactive
        // and zero occurrences. Retain the original local map in its original owner.
        for(std::size_t p=0;p<s->patches.size();++p)for(std::size_t k=1;k<grades;++k){
            const auto kg=grade(k);const auto& a=source(*s,p).grade(kg);
            for(std::size_t slot=0;slot<a.incidence_count();++slot){
                const auto lower=chart(*s,p,{grade(k-1),a.lower_index[slot]}),upper=chart(*s,p,{kg,a.upper_index[slot]});
                if(!lower && !upper)continue;
                g.step();auto m=raw_map(*s,p,slot,kg);
                if(upper)m=m.multiply(upper->from_common,g);
                if(lower)m=lower->to_common.multiply(m,g);
                s->records=add(s->records,m.stored_entries());g.entries(s->records);
                s->transports.emplace(std::make_tuple(p,kg.value,slot),std::move(m));
            }
        }
        for(const auto& e:s->interfaces){
            require(e.left<s->patches.size() && e.right<s->patches.size() && e.left!=e.right,"invalid interface patch pair");
            s->records=add(s->records,add(algebra::owned_gluing_detail::words(e.common.source()),add(algebra::owned_gluing_detail::words(e.common_left),algebra::owned_gluing_detail::words(e.common_right))));g.entries(s->records);
            algebra::owned_gluing_detail::map(e.common.source(),source(*s,e.left),e.common_left,true,g);
            algebra::owned_gluing_detail::map(e.common.source(),source(*s,e.right),e.common_right,true,g);
        }
        s->records=machine::checked_size_mul(add(s->records,1),16);g.entries(s->records);g.dimension(grades);s->grades.resize(grades);
        for(std::size_t k=0;k<grades;++k){
            auto& dst=s->grades[k];const auto kg=grade(k);
            std::vector<std::size_t> start(s->patches.size()+1,0),ostart(start.size(),0);
            for(std::size_t p=0;p<s->patches.size();++p){
                const auto& c=source(*s,p);start[p+1]=add(start[p],k<c.grade_count()?c.grade(kg).cell_count():0);
                ostart[p+1]=add(ostart[p],k<c.grade_count()?c.grade(kg).incidence_count():0);
            }
            Union cells(start.back()),occurrences(ostart.back());
            for(const auto& e:s->interfaces){
                if(k>=e.common.source().grade_count())continue;
                for(std::size_t i=0;i<e.common_left.cells[k].size();++i){g.step();cells.join(start[e.left]+e.common_left.cells[k][i],start[e.right]+e.common_right.cells[k][i],g);}
                for(std::size_t i=0;i<e.common_left.occurrences[k].size();++i){g.step();occurrences.join(ostart[e.left]+e.common_left.occurrences[k][i],ostart[e.right]+e.common_right.occurrences[k][i],g);}
            }
            dst.images.resize(s->patches.size());dst.offsets.push_back(0);
            std::map<std::size_t,std::size_t> roots;std::set<std::pair<std::size_t,std::size_t>> injective;
            for(std::size_t p=0;p<s->patches.size();++p){
                for(std::size_t i=0;i<start[p+1]-start[p];++i){
                    g.step();const auto root=cells.root(start[p]+i,g);require(injective.emplace(root,p).second,"interface cycle identifies distinct cells in one patch");
                    const auto [at,fresh]=roots.emplace(root,dst.cells.size());const auto n=at->second;dst.images[p].push_back(n);
                    const auto& b=stalk(*s,p,{kg,static_cast<std::uint32_t>(i)});const auto dim=b.carrier.domain.logical_size();g.dimension(dim);
                    const auto charted=chart(*s,p,{kg,static_cast<std::uint32_t>(i)});
                    const auto coordinates=charted?charted->common_coordinates:b.carrier.coordinate_system;
                    const auto& declaration=charted?charted->common_declaration:b.declaration;
                    if(fresh){dst.cells.push_back({p,i});dst.layout.push_back({p,*b.carrier.cell,coordinates,declaration,dst.offsets.back(),dim});dst.offsets.push_back(add(dst.offsets.back(),dim));g.dimension(dst.offsets.back());}
                    else{const auto& previous=dst.layout[n];require(previous.dimension==dim && previous.coordinates==coordinates && previous.declaration==declaration,"shared stalk needs identical declared coordinates; recode explicitly first");}
                }
            }
            if(!k)continue;
            std::map<std::size_t,Origin> oroots;std::set<std::pair<std::size_t,std::size_t>> oinjective;
            std::vector<std::vector<std::size_t>> fibers(dst.cells.size());
            dst.from_upper.resize(dst.cells.size());dst.from_lower.resize(s->grades[k-1].cells.size());
            for(std::size_t p=0;p<s->patches.size();++p){
                if(k>=source(*s,p).grade_count())continue;
                const auto& a=source(*s,p).grade(kg);
                for(std::size_t i=0;i<a.cell_count();++i){
                    const auto upper=dst.images[p][i];const auto [begin,end]=a.incidence_range(i);std::vector<std::size_t> fiber;
                    for(auto slot=begin;slot<end;++slot){
                        g.step();const auto root=occurrences.root(ostart[p]+slot,g);fiber.push_back(root);
                        require(oinjective.emplace(root,p).second,"interface cycle identifies distinct occurrences in one patch");
                        const auto [at,fresh]=oroots.emplace(root,Origin{p,slot});
                        if(!fresh)require(same_map(*s,p,slot,at->second.patch,at->second.local,kg,g),"realized local maps do not descend on shared occurrence");
                        // Select each retained upper relation exactly once, not once per local copy.
                        if(dst.cells[upper].patch==p && dst.cells[upper].local==i){
                            const auto lower=s->grades[k-1].images[p].at(a.lower_index[slot]);const auto n=dst.routes.size();
                            dst.routes.push_back({p,slot,upper,lower});dst.from_upper[upper].push_back(n);dst.from_lower[lower].push_back(n);
                        }
                    }
                    std::sort(fiber.begin(),fiber.end());
                    if(dst.cells[upper].patch==p && dst.cells[upper].local==i)fibers[upper]=fiber;
                    else require(fibers[upper]==fiber,"shared relation lost a complete occurrence fiber");
                }
            }
        }
        state_=std::move(s);
    }
    const std::vector<GluedStalkRange>& layout(core::Grade k) const & {return state_->grades.at(k.value).layout;}
    const std::vector<GluedStalkRange>& layout(core::Grade) const &&=delete;
    std::size_t dimension(core::Grade k)const{return state_->grades.at(k.value).offsets.back();}
    std::size_t cell_count(core::Grade k)const{return layout(k).size();}
    std::size_t occurrence_count(core::Grade k)const{return state_->grades.at(k.value).routes.size();}
    std::size_t retained_records()const{return state_->records;}
    std::size_t patch_count()const{return state_->patches.size();}
    std::size_t grade_count()const{return state_->grades.size();}
    const std::vector<DeclaredLocalFieldBinding>& patches()const &{return state_->patches;}
    const std::vector<DeclaredLocalFieldBinding>& patches()const &&=delete;
    std::size_t image(std::size_t p,core::CellId c)const{return state_->grades.at(c.grade.value).images.at(p).at(c.index);}
    // Total local values, not observation masks. Missing numerical entries mean
    // zero only under the supplied field's existing total-value contract.
    S glue_fields(core::Grade k,const std::vector<field::SparseStalkField>& inputs,Guard& g)const{
        using namespace local_gluing_detail;const auto pin=state_;require(inputs.size()==patch_count(),"one total field per patch is required");
        const auto& target=pin->grades.at(k.value);g.dimension(target.offsets.back());g.entries(pin->records);std::map<std::size_t,S> chosen;
        std::size_t held=pin->records;
        for(std::size_t p=0;p<inputs.size();++p){const auto& f=inputs[p];require(f.owner().same_owner(pin->patches[p].fibers()) && f.grade()==k,"foreign local field owner or grade");held=add(held,f.nonzeros());}g.entries(held);
        for(std::size_t p=0;p<inputs.size();++p)for(std::size_t i=0;i<target.images[p].size();++i){
            g.step();const auto dst=target.images[p][i];const auto n=target.layout[dst].dimension;const auto at=inputs[p].values().find(static_cast<std::uint32_t>(i));
            auto value=at==inputs[p].values().end()?S(n):at->second;g.sparse_vector(value);
            if(const auto c=chart(*pin,p,{k,static_cast<std::uint32_t>(i)}))value=c->to_common.apply(value,g);
            const auto old=chosen.find(dst);
            if(old!=chosen.end())require(old->second.entries()==value.entries(),"local field disagreement on shared stalk");
            else{held=add(held,add(1,value.nonzeros()));g.entries(held);chosen.emplace(dst,value);}
        }
        std::vector<numeric::SparseRationalEntry> out;
        for(const auto& [cell,v]:chosen)for(const auto& e:v.entries()){g.step();g.entries(add(held,add(out.size(),1)));out.push_back({add(target.offsets[cell],e.index),e.value});}
        return S(target.offsets.back(),std::move(out));
    }
    field::SparseStalkField restrict_field(core::Grade k,const S& value,std::size_t p,Guard& g)const{
        using namespace local_gluing_detail;const auto pin=state_;require(value.size()==dimension(k),"glued field carrier mismatch");g.sparse_vector(value);
        field::SparseStalkValues out;const auto& target=pin->grades.at(k.value);std::size_t held=add(pin->records,value.nonzeros());
        for(std::size_t i=0;i<target.images.at(p).size();++i){g.step();const auto dst=target.images[p][i];auto v=part(value,target.offsets[dst],target.layout[dst].dimension,g);if(const auto c=chart(*pin,p,{k,static_cast<std::uint32_t>(i)}))v=c->from_common.apply(v,g);if(v.nonzeros()){held=add(held,add(1,v.nonzeros()));g.entries(held);out.emplace(static_cast<std::uint32_t>(i),std::move(v));}}
        return field::SparseStalkFieldActions(pin->patches.at(p).fibers(),{},g.limits()).bind(k,std::move(out));
    }
    const std::vector<LocalStalkChart>& charts()const &{return state_->charts;}
    const std::vector<LocalStalkChart>& charts()const &&=delete;
    // Linear inclusion of one local CHAIN contribution, not a gluing of section
    // copies. Do not add these on shared values and claim a same-field section.
    S include_chain_field(core::Grade k,const field::SparseStalkField& value,std::size_t p,Guard& g)const{
        using namespace local_gluing_detail;const auto pin=state_;
        require(value.grade()==k && value.owner().same_owner(pin->patches.at(p).fibers()),"foreign included chain field");
        g.entries(add(pin->records,value.nonzeros()));std::map<std::size_t,Q> out;
        const auto& target=pin->grades.at(k.value);
        for(const auto& [i,v]:value.values()){
            g.step();const auto dst=target.images.at(p).at(i);const auto c=chart(*pin,p,{k,i});
            const auto mapped=c?c->to_common.apply(v,g):v;
            for(const auto& e:mapped.entries())accumulate(out,add(target.offsets[dst],e.index),e.value,g,add(pin->records,add(value.nonzeros(),mapped.nonzeros())));
        }
        return numeric::checked_sparse_detail::vector(target.offsets.back(),std::move(out),g);
    }
    field::FactoredAction boundary(core::Grade k,std::optional<std::size_t> component={})const{
        using namespace local_gluing_detail;require(k.value>0 && k.value<grade_count(),"glued boundary grade");require(!component || *component<patch_count(),"glued component index");
        const auto make=[k,component](bool transpose){return [k,component,transpose](const auto& pin,const auto& x,Guard& g){
            if constexpr(std::is_same_v<std::decay_t<decltype(x)>,S>)return action(*pin,k,x,transpose,component,g);
            else return action(*pin,k,sparse_input(x,g),transpose,component,g).dense();
        };};
        return field::FactoredAction::owned(state_,dimension(grade(k.value-1)),dimension(k),state_->records,make(false),make(true));
    }
};
} // namespace rfc::storage
