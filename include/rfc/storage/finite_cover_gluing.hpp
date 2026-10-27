#pragma once
#include "rfc/storage/split_stalk_cover.hpp"
#include "rfc/storage/injective_local_gluing.hpp"

namespace rfc::storage {
struct CoverInterfaceStalk {
    LocalStalkPartition common;
    std::array<std::vector<std::size_t>,2> block_images;
};
struct FiniteCoverInterface {
    DeclaredLocalFieldBinding common;
    std::size_t left{},right{};
    std::array<algebra::RetainedSourceMap,2> maps;
    std::vector<CoverInterfaceStalk> stalks;
};
struct CoverStalkOrigin {
    std::size_t patch{};
    TemporalCarrier local;
    composite::Declaration local_declaration,split_declaration;
    std::uint64_t split_coordinates{};
    std::vector<StalkCoordinateBlock> blocks;
    bool operator==(const CoverStalkOrigin&) const = default;
};
struct CoverStalkRange {
    std::size_t offset{},dimension{};
    std::vector<CoverStalkOrigin> origins;
    std::vector<CoverBlockRange> blocks;
    bool operator==(const CoverStalkRange&) const = default;
};
namespace finite_cover_detail {
using namespace local_gluing_detail;
using injective_gluing_detail::source;
using injective_gluing_detail::stalk;
using injective_gluing_detail::same_stalk;
using injective_gluing_detail::raw_map;
struct Cell {
    std::vector<Origin> origins;
    std::vector<LocalStalkPartition> parts;
    std::vector<StalkBlockLink> links;
    std::optional<SplitStalkCover> cover;
};
struct Route {std::size_t patch{},slot{},upper{},lower{},upper_part{},lower_part{};};
struct Grade {
    std::vector<Cell> cells;
    std::vector<CoverStalkRange> layout;
    std::vector<std::vector<std::size_t>> images,parts;
    std::vector<Route> routes;
    std::vector<std::vector<std::size_t>> from_upper,from_lower;
    std::vector<std::size_t> offsets{0};
    std::size_t occurrences{};
};
struct State {
    std::vector<DeclaredLocalFieldBinding> patches;
    std::vector<LocalStalkPartition> partitions;
    std::vector<FiniteCoverInterface> interfaces;
    std::map<std::pair<std::size_t,core::CellId>,std::size_t> partition_index;
    std::vector<std::map<core::CellId,std::size_t>> interface_index;
    std::vector<Grade> grades;
    std::size_t records{};
};
inline core::CellId matched(const DeclaredLocalFieldBinding& b,const LocalStalkPartition& p){
    require(p.chart.local.carrier.cell.has_value(),"partition must name a local cell");
    const auto cell=StableSourceIndex(source(b)).cell(*p.chart.local.carrier.cell);
    require(cell.has_value() && same_stalk(p.chart.local,stalk(b,*cell)),"foreign partition cell/type/coordinate declaration");return *cell;
}
inline const LocalStalkPartition& partition(const State& s,std::size_t p,core::CellId c){return s.partitions.at(s.partition_index.at({p,c}));}
inline split_cover_detail::BlockEmbedding embedding(const LocalStalkPartition& k,const LocalStalkPartition& local,
                                                   const std::vector<std::size_t>& images,Guard& g){
    const auto a=split_cover_detail::block_offsets(k,g),b=split_cover_detail::block_offsets(local,g);
    require(images.size()==k.blocks.size(),"common block inventory must be complete");std::set<std::size_t> used;
    for(std::size_t i=0;i<images.size();++i){g.step();require(images[i]<local.blocks.size() && used.insert(images[i]).second,"common block map is not injective");require(k.blocks[i].declaration==local.blocks[images[i]].declaration,"common block type/order differs");}
    return {a,b,images};
}
inline StalkChartMap split_map(const DeclaredLocalFieldBinding& b,core::Grade k,std::size_t slot,
                              const LocalStalkPartition& low,const LocalStalkPartition& up,Guard& g){
    return low.chart.to_common.multiply(raw_map(b,k,slot),g).multiply(up.chart.from_common,g);
}
inline S action(const State& s,core::Grade k,const S& x,bool transpose,std::optional<std::size_t> component,Guard& g){
    const auto& routes=s.grades.at(k.value);const auto& input=s.grades.at(k.value-(transpose?1u:0u));
    const auto& output=s.grades.at(k.value-(transpose?0u:1u));require(x.size()==input.offsets.back(),"cover action input carrier");
    g.sparse_vector(x);g.dimension(output.offsets.back());g.entries(s.records);std::map<std::size_t,Q> out;
    for(auto it=x.entries().begin();it!=x.entries().end();){
        g.step();const auto ci=std::size_t(std::upper_bound(input.offsets.begin(),input.offsets.end(),it->index)-input.offsets.begin()-1);
        const auto value=part(x,input.offsets[ci],input.cells[ci].cover->dimension(),g);
        for(const auto ri:(transpose?routes.from_lower.at(ci):routes.from_upper.at(ci))){
            const auto& r=routes.routes[ri];g.step();if(component && r.patch!=*component)continue;
            const auto& b=s.patches[r.patch];const auto& c=source(b);if(!c.grade(k).aspects.get(c.signature().existence_aspect(),r.slot))continue;
            const auto& from=*input.cells[ci].cover;
            const auto local=transpose?from.include_transpose(r.lower_part,value,g):from.representative(r.upper_part,value,g);
            const auto acted=raw_map(b,k,r.slot).apply(local,g,transpose);const auto target=transpose?r.upper:r.lower;
            const auto& to=*output.cells[target].cover;
            const auto mapped=transpose?to.representative_transpose(r.upper_part,acted,g):to.include(r.lower_part,acted,g);
            const auto held=add(s.records,add(x.nonzeros(),add(value.nonzeros(),add(local.nonzeros(),add(acted.nonzeros(),mapped.nonzeros())))));
            for(const auto& e:mapped.entries())accumulate(out,add(output.offsets[target],e.index),e.value,g,held);
        }
        it=std::lower_bound(it,x.entries().end(),input.offsets[ci+1],[](const auto& e,auto n){return e.index<n;});
    }
    return numeric::checked_sparse_detail::vector(output.offsets.back(),std::move(out),g);
}
} // namespace finite_cover_detail

// A resident finite cover with supplied simultaneous block splittings. Multiple
// common sources and cyclic interfaces are checked globally at cells, occurrences
// and stalk blocks. No local source/direction may collapse in the colimit. The
// complete injection squares give boundary descent, not sheaf/metric certificates.
class FiniteCoverLocalFieldView {
    using State=finite_cover_detail::State;using G=numeric::ExactActionGuard;using S=numeric::SparseRationalVector;
    std::shared_ptr<const State> state_;
public:
    FiniteCoverLocalFieldView(std::vector<DeclaredLocalFieldBinding> patches,std::vector<LocalStalkPartition> partitions,
        std::vector<FiniteCoverInterface> interfaces,numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}){
        using namespace finite_cover_detail;
        auto s=std::make_shared<State>(State{std::move(patches),std::move(partitions),std::move(interfaces),{},{},{},0});
        G g(limits,std::move(checkpoint));g.step();require(!s->patches.empty(),"finite cover needs patches");
        const auto count=source(s->patches[0]).grade_count();std::size_t cells_needed=0;
        for(const auto& p:s->patches){require(source(p).grade_count()==count,"cover grade inventory differs");s->records=add(s->records,algebra::owned_gluing_detail::words(source(p)));for(std::size_t k=0;k<count;++k)cells_needed=add(cells_needed,source(p).grade(grade(k)).cell_count());}
        for(std::size_t i=0;i<s->partitions.size();++i){
            const auto& p=s->partitions[i];require(p.chart.patch<s->patches.size(),"foreign partition patch");const auto cell=matched(s->patches[p.chart.patch],p);
            require(s->partition_index.emplace(std::make_pair(p.chart.patch,cell),i).second,"repeated local partition");
            (void)split_cover_detail::block_offsets(p,g);s->records=add(s->records,add(p.blocks.size(),add(p.chart.to_common.stored_entries(),p.chart.from_common.stored_entries())));
        }
        require(cells_needed==s->partition_index.size(),"one complete partition per local cell required");
        for(const auto& e:s->interfaces){
            require(e.left<s->patches.size() && e.right<s->patches.size() && e.left!=e.right,"cover interface endpoints differ");const auto& common=source(e.common);
            s->records=add(s->records,algebra::owned_gluing_detail::words(common));std::array<std::size_t,2> sides{e.left,e.right};
            for(std::size_t p=0;p<2;++p){s->records=add(s->records,algebra::owned_gluing_detail::words(e.maps[p]));algebra::owned_gluing_detail::map(common,source(s->patches[sides[p]]),e.maps[p],true,g);}
            std::map<core::CellId,std::size_t> index;
            for(std::size_t i=0;i<e.stalks.size();++i){
                const auto& v=e.stalks[i];const auto kc=matched(e.common,v.common);require(v.common.chart.patch==0 && index.emplace(kc,i).second,"common partition identity/repetition");
                s->records=add(s->records,add(v.common.blocks.size(),add(v.common.chart.to_common.stored_entries(),v.common.chart.from_common.stored_entries())));
                for(std::size_t p=0;p<2;++p)(void)embedding(v.common,partition(*s,sides[p],{kc.grade,e.maps[p].cells.at(kc.grade.value).at(kc.index)}),v.block_images[p],g);
            }
            std::size_t need=0;for(std::size_t k=0;k<common.grade_count();++k)need=add(need,common.grade(grade(k)).cell_count());require(need==index.size(),"common partition coverage incomplete");s->interface_index.push_back(std::move(index));
        }
        s->records=machine::checked_size_mul(add(s->records,add(cells_needed,1)),16);g.entries(s->records);g.dimension(count);s->grades.resize(count);
        for(std::size_t k=0;k<count;++k){
            const auto kg=grade(k);auto& dst=s->grades[k];std::vector<std::size_t> start{0},ostart{0};
            for(const auto& p:s->patches){start.push_back(add(start.back(),source(p).grade(kg).cell_count()));ostart.push_back(add(ostart.back(),source(p).grade(kg).incidence_count()));}
            g.entries(add(s->records,add(start.back(),ostart.back())));Union cells(start.back()),occurrences(ostart.back());
            for(const auto& e:s->interfaces){if(k>=source(e.common).grade_count())continue;
                for(std::size_t i=0;i<e.maps[0].cells[k].size();++i){g.step();cells.join(add(start[e.left],e.maps[0].cells[k][i]),add(start[e.right],e.maps[1].cells[k][i]),g);}
                for(std::size_t i=0;i<e.maps[0].occurrences[k].size();++i){g.step();occurrences.join(add(ostart[e.left],e.maps[0].occurrences[k][i]),add(ostart[e.right],e.maps[1].occurrences[k][i]),g);}
            }
            dst.images.resize(s->patches.size());dst.parts.resize(s->patches.size());std::map<std::size_t,std::size_t> roots;std::set<std::pair<std::size_t,std::size_t>> injective;
            for(std::size_t p=0;p<s->patches.size();++p)for(std::size_t i=0;i<start[p+1]-start[p];++i){
                g.step();const auto root=cells.root(start[p]+i,g);require(injective.emplace(root,p).second,"cover cycle collapses distinct local cells");
                const auto [at,fresh]=roots.emplace(root,dst.cells.size());if(fresh)dst.cells.emplace_back();auto& cell=dst.cells[at->second];
                dst.images[p].push_back(at->second);dst.parts[p].push_back(cell.parts.size());cell.origins.push_back({p,i});cell.parts.push_back(partition(*s,p,{kg,static_cast<std::uint32_t>(i)}));
            }
            for(std::size_t eidx=0;eidx<s->interfaces.size();++eidx){const auto& e=s->interfaces[eidx];if(k>=source(e.common).grade_count())continue;
                for(std::size_t i=0;i<e.maps[0].cells[k].size();++i){const auto& v=e.stalks.at(s->interface_index[eidx].at({kg,static_cast<std::uint32_t>(i)}));
                    const auto l=e.maps[0].cells[k][i],r=e.maps[1].cells[k][i];auto& cell=dst.cells[dst.images[e.left][l]];
                    for(std::size_t b=0;b<v.common.blocks.size();++b){g.step();cell.links.push_back({dst.parts[e.left][l],v.block_images[0][b],dst.parts[e.right][r],v.block_images[1][b]});}
                }
            }
            for(auto& cell:dst.cells){
                cell.cover.emplace(cell.parts,cell.links,g);s->records=add(s->records,cell.cover->stored_entries());g.entries(s->records);
                CoverStalkRange layout;layout.offset=dst.offsets.back();layout.dimension=cell.cover->dimension();layout.blocks=cell.cover->layout();
                for(const auto& p:cell.parts)layout.origins.push_back({p.chart.patch,p.chart.local.carrier,p.chart.local.declaration,p.chart.common_declaration,p.chart.common_coordinates,p.blocks});
                dst.offsets.push_back(add(dst.offsets.back(),layout.dimension));g.dimension(dst.offsets.back());dst.layout.push_back(std::move(layout));
            }
            if(!k)continue;
            dst.from_upper.resize(dst.cells.size());dst.from_lower.resize(s->grades[k-1].cells.size());
            std::map<std::size_t,std::vector<std::size_t>> fibers;std::set<std::pair<std::size_t,std::size_t>> oinjective;std::set<std::size_t> oroots;
            for(std::size_t p=0;p<s->patches.size();++p){const auto& a=source(s->patches[p]).grade(kg);
                for(std::size_t i=0;i<a.cell_count();++i){const auto [begin,end]=a.incidence_range(i);std::vector<std::size_t> fiber;const auto u=dst.images[p][i];
                    for(auto slot=begin;slot<end;++slot){g.step();const auto root=occurrences.root(ostart[p]+slot,g);require(oinjective.emplace(root,p).second,"cover cycle collapses distinct local occurrences");fiber.push_back(root);oroots.insert(root);
                        const auto l=s->grades[k-1].images[p][a.lower_index[slot]],id=dst.routes.size();
                        dst.routes.push_back({p,slot,u,l,dst.parts[p][i],s->grades[k-1].parts[p][a.lower_index[slot]]});dst.from_upper[u].push_back(id);dst.from_lower[l].push_back(id);
                        const auto map=raw_map(s->patches[p],kg,slot);map.validate(g);s->records=add(s->records,map.stored_entries());g.entries(s->records);
                    }
                    std::sort(fiber.begin(),fiber.end());const auto [at,fresh]=fibers.emplace(u,fiber);require(fresh || at->second==fiber,"cover lost complete shared occurrence fiber");
                }
            }
            dst.occurrences=oroots.size();
            for(std::size_t eidx=0;eidx<s->interfaces.size();++eidx){const auto& e=s->interfaces[eidx];if(k>=source(e.common).grade_count())continue;const auto& a=source(e.common).grade(kg);
                for(std::size_t slot=0;slot<a.incidence_count();++slot){
                    const core::CellId ku{kg,a.upper_index[slot]},kl{grade(k-1),a.lower_index[slot]};
                    const auto& u=e.stalks.at(s->interface_index[eidx].at(ku));const auto& l=e.stalks.at(s->interface_index[eidx].at(kl));
                    const auto km=split_map(e.common,kg,slot,l.common,u.common,g);const std::array<std::size_t,2> sides{e.left,e.right};
                    for(std::size_t p=0;p<2;++p){
                        const auto& lp=partition(*s,sides[p],{kl.grade,e.maps[p].cells[k-1][kl.index]});const auto& up=partition(*s,sides[p],{ku.grade,e.maps[p].cells[k][ku.index]});
                        const auto lm=split_map(s->patches[sides[p]],kg,e.maps[p].occurrences[k][slot],lp,up,g);
                        require(split_cover_detail::descends(lm,km,embedding(l.common,lp,l.block_images[p],g),embedding(u.common,up,u.block_images[p],g),g),"local boundary does not descend through the complete cover interface");
                    }
                }
            }
        }
        state_=std::move(s);
    }
    std::size_t grade_count()const{return state_->grades.size();}
    std::size_t patch_count()const{return state_->patches.size();}
    std::size_t dimension(core::Grade k)const{return state_->grades.at(k.value).offsets.back();}
    std::size_t cell_count(core::Grade k)const{return state_->grades.at(k.value).cells.size();}
    std::size_t occurrence_count(core::Grade k)const{return state_->grades.at(k.value).occurrences;}
    std::size_t retained_records()const{return state_->records;}
    std::size_t image(std::size_t p,core::CellId c)const{return state_->grades.at(c.grade.value).images.at(p).at(c.index);}
    const std::vector<CoverStalkRange>& layout(core::Grade k)const &{return state_->grades.at(k.value).layout;}
    const std::vector<CoverStalkRange>& layout(core::Grade)const &&=delete;
    const std::vector<DeclaredLocalFieldBinding>& patches()const &{return state_->patches;}
    const std::vector<DeclaredLocalFieldBinding>& patches()const &&=delete;
    S glue_fields(core::Grade k,const std::vector<field::SparseStalkField>& values,G& g)const {
        using namespace finite_cover_detail;const auto pin=state_;require(values.size()==pin->patches.size(),"one total input per patch required");std::size_t held=pin->records;
        for(std::size_t p=0;p<values.size();++p){require(values[p].grade()==k && values[p].owner().same_owner(pin->patches[p].fibers()),"foreign cover input owner/grade");held=add(held,values[p].nonzeros());g.entries(held);}
        const auto& dst=pin->grades.at(k.value);std::map<std::size_t,Q> out;
        for(std::size_t c=0;c<dst.cells.size();++c){g.step();const auto& cell=dst.cells[c];std::vector<S> xs;
            for(const auto& o:cell.origins){const auto it=values[o.patch].values().find(static_cast<std::uint32_t>(o.local));const auto n=stalk(pin->patches[o.patch],{k,static_cast<std::uint32_t>(o.local)}).carrier.domain.logical_size();xs.push_back(it==values[o.patch].values().end()?S(n):it->second);}
            const auto v=cell.cover->glue(xs,g);for(const auto& e:v.entries())accumulate(out,add(dst.offsets[c],e.index),e.value,g,add(held,v.nonzeros()));
        }
        return numeric::checked_sparse_detail::vector(dst.offsets.back(),std::move(out),g);
    }
    S include_chain_field(core::Grade k,const field::SparseStalkField& x,std::size_t p,G& g)const {
        using namespace finite_cover_detail;const auto pin=state_;require(p<pin->patches.size() && x.grade()==k && x.owner().same_owner(pin->patches[p].fibers()),"foreign cover chain injection");
        const auto& dst=pin->grades.at(k.value);g.entries(add(pin->records,x.nonzeros()));std::map<std::size_t,Q> out;
        for(const auto& [i,v]:x.values()){g.step();const auto ci=dst.images[p].at(i);const auto y=dst.cells[ci].cover->include(dst.parts[p][i],v,g);for(const auto& e:y.entries())accumulate(out,add(dst.offsets[ci],e.index),e.value,g,add(pin->records,add(x.nonzeros(),y.nonzeros())));}
        return numeric::checked_sparse_detail::vector(dst.offsets.back(),std::move(out),g);
    }
    field::SparseStalkField restrict_field(core::Grade k,const S& x,std::size_t p,G& g)const {
        using namespace finite_cover_detail;const auto pin=state_;require(p<pin->patches.size() && x.size()==pin->grades.at(k.value).offsets.back(),"cover restriction carrier");g.sparse_vector(x);const auto& dst=pin->grades[k.value];field::SparseStalkValues out;auto held=add(pin->records,x.nonzeros());
        for(std::size_t i=0;i<dst.images[p].size();++i){g.step();const auto ci=dst.images[p][i];const auto& c=dst.cells[ci];const auto y=c.cover->restrict(dst.parts[p][i],part(x,dst.offsets[ci],c.cover->dimension(),g),g);if(y.nonzeros()){held=add(held,add(1,y.nonzeros()));g.entries(held);out.emplace(static_cast<std::uint32_t>(i),y);}}
        return field::SparseStalkFieldActions(pin->patches[p].fibers(),{},g.limits()).bind(k,std::move(out));
    }
    field::FactoredAction boundary(core::Grade k,std::optional<std::size_t> component={})const {
        using namespace finite_cover_detail;const auto pin=state_;require(k.value>0 && k.value<pin->grades.size() && (!component || *component<pin->patches.size()),"cover boundary selection");
        const auto make=[k,component](bool transpose){return [k,component,transpose](const auto& s,const auto& x,G& g){if constexpr(std::is_same_v<std::decay_t<decltype(x)>,S>)return finite_cover_detail::action(*s,k,x,transpose,component,g);else return finite_cover_detail::action(*s,k,sparse_input(x,g),transpose,component,g).dense();};};
        return field::FactoredAction::owned(pin,pin->grades[k.value-1].offsets.back(),pin->grades[k.value].offsets.back(),pin->records,make(false),make(true));
    }
};
} // namespace rfc::storage
