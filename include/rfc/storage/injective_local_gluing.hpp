#pragma once
#include "rfc/storage/glued_local_fields.hpp"
#include "rfc/storage/split_stalk_pushout.hpp"
#include <array>

namespace rfc::storage {
// One common-source cell has two supplied split coordinate charts. Their leading
// common_dimension coordinates are EXPLICITLY identified with the complete
// common stalk declaration, in its declared order; the tails stay independent.
struct InjectiveStalkInterface {
    DeclaredStalkBinding common;
    std::array<LocalStalkChart,2> split;
};
struct InjectiveStalkInterfaceRecords {
    DeclaredStalkBinding common;
    std::array<LocalStalkChartRecords,2> split;
};
struct InjectiveStalkRange {
    std::size_t offset{},dimension{},shared_dimension{};
    std::optional<TemporalCarrier> common;
    std::optional<composite::Declaration> common_declaration;
    std::array<std::optional<TemporalCarrier>,2> local;
    std::array<std::optional<composite::Declaration>,2> local_declarations,split_declarations;
    std::array<std::uint64_t,2> split_coordinates{};
    bool operator==(const InjectiveStalkRange&)const=default;
};
namespace injective_gluing_detail {
using namespace local_gluing_detail;
struct Cell {
    InjectiveStalkRange layout;
    std::array<std::optional<core::CellId>,2> local;
    std::optional<SplitStalkPushout> pushout;
};
struct Grade {
    std::vector<Cell> cells;
    std::vector<InjectiveStalkRange> layout;
    std::array<std::vector<std::size_t>,2> images;
    std::vector<Route> routes;
    std::vector<std::vector<std::size_t>> from_upper,from_lower;
    std::vector<std::size_t> offsets{0};
    std::size_t occurrences{};
};
struct State {
    DeclaredLocalFieldBinding common;
    std::array<DeclaredLocalFieldBinding,2> patches;
    std::array<algebra::RetainedSourceMap,2> maps;
    std::vector<InjectiveStalkInterface> interfaces;
    std::vector<Grade> grades;
    std::size_t records{};
};
inline const core::RelationalComplex& source(const DeclaredLocalFieldBinding& b){return b.source_owner().source();}
inline const DeclaredStalkBinding& stalk(const DeclaredLocalFieldBinding& b,core::CellId c){
    const auto key=stable_cell_key(source(b),c);
    for(const auto& s:b.contract().downward.stalks)if(s.carrier.cell==key)return s;
    throw std::logic_error("interface stalk missing from binding");
}
inline bool same_stalk(const DeclaredStalkBinding& a,const DeclaredStalkBinding& b){return a.carrier==b.carrier && a.declaration==b.declaration;}
inline StalkChartMap raw_map(const DeclaredLocalFieldBinding& b,core::Grade k,std::size_t slot){
    const auto& f=b.fibers().fibers();f.validate_transport(k,slot);
    if(const auto& m=f.transport_override(k,slot))return StalkChartMap::explicit_map(*m);
    const auto& a=source(b).grade(k);
    return StalkChartMap::identity(f.stalks().dimension({k,a.upper_index[slot]}),source(b).incidence_coefficient(k,slot));
}
inline S action(const State& s,core::Grade k,const S& x,bool transpose,std::optional<std::size_t> component,Guard& g){
    const auto& routes=s.grades.at(k.value);const auto& in=s.grades.at(k.value-(transpose?1u:0u));
    const auto& outgrade=s.grades.at(k.value-(transpose?0u:1u));
    require(x.size()==in.offsets.back(),"injective action input carrier");g.sparse_vector(x);g.entries(s.records);g.dimension(outgrade.offsets.back());
    std::map<std::size_t,Q> output;
    for(auto it=x.entries().begin();it!=x.entries().end();){
        g.step();const auto ci=std::size_t(std::upper_bound(in.offsets.begin(),in.offsets.end(),it->index)-in.offsets.begin()-1);
        const auto& cell=in.cells[ci];const auto value=part(x,cell.layout.offset,cell.layout.dimension,g);
        for(const auto ri:(transpose?routes.from_lower.at(ci):routes.from_upper.at(ci))){
            const auto& r=routes.routes[ri];g.step();if(component && *component!=r.patch)continue;
            const auto& b=s.patches[r.patch];const auto& c=source(b);
            if(!c.grade(k).aspects.get(c.signature().existence_aspect(),r.slot))continue;
            const auto input=cell.pushout?(transpose?cell.pushout->include_transpose(r.patch,value,g):cell.pushout->representative(r.patch,value,g)):value;
            const auto local=raw_map(b,k,r.slot).apply(input,g,transpose);
            const auto& target=outgrade.cells[transpose?r.upper:r.lower];
            const auto mapped=target.pushout?(transpose?target.pushout->representative_transpose(r.patch,local,g):target.pushout->include(r.patch,local,g)):local;
            const auto held=add(s.records,add(x.nonzeros(),add(value.nonzeros(),add(input.nonzeros(),add(local.nonzeros(),mapped.nonzeros())))));
            for(const auto& e:mapped.entries())accumulate(output,add(target.layout.offset,e.index),e.value,g,held);
        }
        it=std::lower_bound(it,x.entries().end(),in.offsets[ci+1],[](const auto& e,auto n){return e.index<n;});
    }
    return numeric::checked_sparse_detail::vector(outgrade.offsets.back(),std::move(output),g);
}
} // namespace injective_gluing_detail

// Two-patch resident pushout over one complete embedded common source. Supplied
// split witnesses admit ANY finite split injection, not only coordinate subsets.
// No pseudoinverse, inferred metric, or discarded complementary direction.
class InjectiveLocalFieldView {
    using State=injective_gluing_detail::State;using G=numeric::ExactActionGuard;using S=numeric::SparseRationalVector;
    std::shared_ptr<const State> state_;
public:
    InjectiveLocalFieldView(DeclaredLocalFieldBinding common,std::array<DeclaredLocalFieldBinding,2> patches,
        std::array<algebra::RetainedSourceMap,2> maps,std::vector<InjectiveStalkInterface> interfaces,
        numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}){
        using namespace injective_gluing_detail;
        auto s=std::make_shared<State>(State{common,patches,maps,interfaces,{},0}); // detach before callbacks
        G g(limits,std::move(checkpoint));g.step();
        const auto& ksource=source(s->common);const auto count=source(s->patches[0]).grade_count();
        require(count==source(s->patches[1]).grade_count() && ksource.grade_count()<=count,"injective gluing grade inventory");
        for(std::size_t p=0;p<2;++p)algebra::owned_gluing_detail::map(ksource,source(s->patches[p]),s->maps[p],true,g);
        s->records=algebra::owned_gluing_detail::words(ksource);
        for(std::size_t p=0;p<2;++p){s->records=add(s->records,algebra::owned_gluing_detail::words(source(s->patches[p])));s->records=add(s->records,algebra::owned_gluing_detail::words(s->maps[p]));}
        std::map<core::CellId,std::size_t> requests;
        StableSourceIndex common_index(ksource);
        for(std::size_t i=0;i<s->interfaces.size();++i){
            const auto& d=s->interfaces[i];require(d.common.carrier.cell.has_value(),"common cell declaration missing");
            const auto kc=common_index.cell(*d.common.carrier.cell);require(kc.has_value(),"foreign common cell");
            require(same_stalk(d.common,stalk(s->common,*kc)) && requests.emplace(*kc,i).second,"common stalk differs or repeated");
            for(std::size_t p=0;p<2;++p){
                const auto& ch=d.split[p];const auto lc=core::CellId{kc->grade,s->maps[p].cells.at(kc->grade.value).at(kc->index)};
                require(ch.patch==p && same_stalk(ch.local,stalk(s->patches[p],lc)),"split chart owner/cell/type differs");
                const auto n=ch.local.carrier.domain.logical_size();
                require(ch.common_coordinates!=0 && ch.common_declaration.kind==composite::DeclarationKind::RationalTensor && ch.common_declaration.axes.size()==1 && ch.common_declaration.axes[0].extent==n,"split needs a complete one-axis declaration");
                (void)composite::DeclarationRegistry({d.common.declaration,ch.local.declaration,ch.common_declaration});
                require(ch.to_common.cols()==n && ch.from_common.cols()==n,"split local dimension differs");
                s->records=add(s->records,add(ch.to_common.stored_entries(),ch.from_common.stored_entries()));
            }
        }
        std::size_t needed=0;for(std::size_t k=0;k<ksource.grade_count();++k)needed=add(needed,ksource.grade(grade(k)).cell_count());
        require(needed==requests.size(),"one split declaration required per common cell");
        s->records=machine::checked_size_mul(add(s->records,needed+1),16);g.entries(s->records);g.dimension(count);s->grades.resize(count);
        for(std::size_t k=0;k<count;++k){
            const auto kg=grade(k);auto& dst=s->grades[k];
            std::array<std::map<std::size_t,std::size_t>,2> inverse;
            if(k<ksource.grade_count())for(std::size_t j=0;j<ksource.grade(kg).cell_count();++j)for(std::size_t p=0;p<2;++p)inverse[p].emplace(s->maps[p].cells[k][j],j);
            std::map<std::size_t,std::size_t> common_cells;
            for(std::size_t p=0;p<2;++p)for(std::size_t i=0;i<source(s->patches[p]).grade(kg).cell_count();++i){
                g.step();const auto c=core::CellId{kg,static_cast<std::uint32_t>(i)};const auto& b=stalk(s->patches[p],c);
                if(p==1 && inverse[p].contains(i)){const auto n=common_cells.at(inverse[p].at(i));dst.images[p].push_back(n);dst.cells[n].local[p]=c;continue;}
                injective_gluing_detail::Cell cell;cell.local[p]=c;cell.layout.offset=dst.offsets.back();cell.layout.local[p]=b.carrier;cell.layout.local_declarations[p]=b.declaration;
                if(inverse[p].contains(i)){
                    const auto j=inverse[p].at(i);const auto& d=s->interfaces.at(requests.at({kg,static_cast<std::uint32_t>(j)}));
                    const auto r=d.common.carrier.domain.logical_size();cell.layout.common=d.common.carrier;cell.layout.common_declaration=d.common.declaration;cell.layout.shared_dimension=r;
                    cell.pushout.emplace(r,std::array{d.split[0].to_common,d.split[1].to_common},std::array{d.split[0].from_common,d.split[1].from_common},g);
                    for(std::size_t a=0;a<2;++a){cell.layout.local[a]=d.split[a].local.carrier;cell.layout.local_declarations[a]=d.split[a].local.declaration;cell.layout.split_declarations[a]=d.split[a].common_declaration;cell.layout.split_coordinates[a]=d.split[a].common_coordinates;}
                    cell.layout.dimension=cell.pushout->dimension();common_cells.emplace(j,dst.cells.size());
                }else cell.layout.dimension=b.carrier.domain.logical_size();
                dst.images[p].push_back(dst.cells.size());dst.offsets.push_back(add(dst.offsets.back(),cell.layout.dimension));g.dimension(dst.offsets.back());dst.cells.push_back(std::move(cell));
            }
            for(const auto& c:dst.cells)dst.layout.push_back(c.layout);
            if(!k)continue;
            dst.from_upper.resize(dst.cells.size());dst.from_lower.resize(s->grades[k-1].cells.size());
            std::size_t common_count=k<ksource.grade_count()?ksource.grade(kg).incidence_count():0;
            for(std::size_t p=0;p<2;++p){
                const auto& a=source(s->patches[p]).grade(kg);dst.occurrences=add(dst.occurrences,a.incidence_count());
                for(std::size_t slot=0;slot<a.incidence_count();++slot){
                    g.step();const auto m=raw_map(s->patches[p],kg,slot);m.validate(g);s->records=add(s->records,m.stored_entries());g.entries(s->records);
                    const auto u=dst.images[p].at(a.upper_index[slot]),l=s->grades[k-1].images[p].at(a.lower_index[slot]);const auto id=dst.routes.size();
                    dst.routes.push_back({p,slot,u,l});dst.from_upper[u].push_back(id);dst.from_lower[l].push_back(id);
                }
                for(std::size_t slot=0;slot<common_count;++slot){
                    g.step();const auto& ca=ksource.grade(kg);const auto u=dst.images[p].at(s->maps[p].cells[k].at(ca.upper_index[slot]));
                    const auto l=s->grades[k-1].images[p].at(s->maps[p].cells[k-1].at(ca.lower_index[slot]));
                    const auto& up=*dst.cells[u].pushout;const auto& low=*s->grades[k-1].cells[l].pushout;
                    const auto t=raw_map(s->patches[p],kg,s->maps[p].occurrences[k][slot]);
                    const auto split=low.to_split(p).multiply(t,g).multiply(up.from_split(p),g);
                    require(SplitStalkPushout::descends(split,raw_map(s->common,kg,slot),low.common_dimension(),up.common_dimension(),g),"local transport does not commute with common injection");
                }
            }
            dst.occurrences-=common_count;
        }
        state_=std::move(s);
    }
    std::size_t grade_count()const{return state_->grades.size();}
    std::size_t dimension(core::Grade k)const{return state_->grades.at(k.value).offsets.back();}
    std::size_t cell_count(core::Grade k)const{return state_->grades.at(k.value).cells.size();}
    std::size_t occurrence_count(core::Grade k)const{return state_->grades.at(k.value).occurrences;}
    std::size_t retained_records()const{return state_->records;}
    const std::vector<InjectiveStalkRange>& layout(core::Grade k)const &{return state_->grades.at(k.value).layout;}
    const std::vector<InjectiveStalkRange>& layout(core::Grade)const &&=delete;
    const std::array<DeclaredLocalFieldBinding,2>& patches()const &{return state_->patches;}
    const std::array<DeclaredLocalFieldBinding,2>& patches()const &&=delete;
    const DeclaredLocalFieldBinding& common()const &{return state_->common;}
    const DeclaredLocalFieldBinding& common()const &&=delete;
    std::size_t image(std::size_t p,core::CellId c)const{return state_->grades.at(c.grade.value).images.at(p).at(c.index);}
    S glue_fields(core::Grade k,const std::array<field::SparseStalkField,2>& xs,G& g)const{
        using namespace injective_gluing_detail;const auto pin=state_;const auto& dst=pin->grades.at(k.value);g.entries(pin->records);g.dimension(dst.offsets.back());
        for(std::size_t p=0;p<2;++p)require(xs[p].grade()==k && xs[p].owner().same_owner(pin->patches[p].fibers()),"foreign local section owner/grade");
        const auto value=[&](std::size_t p,core::CellId c){const auto n=stalk(pin->patches[p],c).carrier.domain.logical_size();const auto it=xs[p].values().find(c.index);return it==xs[p].values().end()?S(n):it->second;};
        std::vector<numeric::SparseRationalEntry> out;const auto held=add(pin->records,add(xs[0].nonzeros(),xs[1].nonzeros()));g.entries(held);
        for(const auto& c:dst.cells){g.step();const auto v=c.pushout?c.pushout->glue(value(0,*c.local[0]),value(1,*c.local[1]),g):c.local[0]?value(0,*c.local[0]):value(1,*c.local[1]);
            g.sparse_vector(v);for(const auto& e:v.entries()){g.entries(add(held,add(v.nonzeros(),out.size()+1)));out.push_back({add(c.layout.offset,e.index),e.value});}}
        return S(dst.offsets.back(),std::move(out));
    }
    field::SparseStalkField restrict_field(core::Grade k,const S& x,std::size_t p,G& g)const{
        using namespace injective_gluing_detail;const auto pin=state_;require(p<2 && x.size()==pin->grades.at(k.value).offsets.back(),"restriction carrier");g.sparse_vector(x);const auto& dst=pin->grades[k.value];field::SparseStalkValues out;auto held=add(pin->records,x.nonzeros());g.entries(held);
        for(std::size_t i=0;i<dst.images[p].size();++i){g.step();const auto& c=dst.cells[dst.images[p][i]];auto v=part(x,c.layout.offset,c.layout.dimension,g);if(c.pushout)v=c.pushout->restrict(p,v,g);if(v.nonzeros()){held=add(held,add(1,v.nonzeros()));g.entries(held);out.emplace(static_cast<std::uint32_t>(i),std::move(v));}}
        return field::SparseStalkFieldActions(pin->patches[p].fibers(),{},g.limits()).bind(k,std::move(out));
    }
    S include_chain_field(core::Grade k,const field::SparseStalkField& x,std::size_t p,G& g)const{
        using namespace injective_gluing_detail;const auto pin=state_;require(p<2 && x.grade()==k && x.owner().same_owner(pin->patches[p].fibers()),"foreign chain injection");const auto& dst=pin->grades.at(k.value);g.entries(add(pin->records,x.nonzeros()));g.dimension(dst.offsets.back());std::map<std::size_t,Q> out;
        for(const auto& [i,v]:x.values()){g.step();const auto& c=dst.cells[dst.images[p].at(i)];const auto mapped=c.pushout?c.pushout->include(p,v,g):v;
            for(const auto& e:mapped.entries())accumulate(out,add(c.layout.offset,e.index),e.value,g,add(pin->records,add(x.nonzeros(),mapped.nonzeros())));}
        return numeric::checked_sparse_detail::vector(dst.offsets.back(),std::move(out),g);
    }
    field::FactoredAction boundary(core::Grade k,std::optional<std::size_t> component={})const{
        using namespace injective_gluing_detail;const auto pin=state_;require(k.value>0 && k.value<pin->grades.size() && (!component || *component<2),"injective boundary selection");
        const auto make=[k,component](bool transpose){return [k,component,transpose](const auto& s,const auto& x,G& g){
            if constexpr(std::is_same_v<std::decay_t<decltype(x)>,S>)return injective_gluing_detail::action(*s,k,x,transpose,component,g);
            else return injective_gluing_detail::action(*s,k,sparse_input(x,g),transpose,component,g).dense();};};
        return field::FactoredAction::owned(pin,pin->grades[k.value-1].offsets.back(),pin->grades[k.value].offsets.back(),pin->records,make(false),make(true));
    }
};
} // namespace rfc::storage
