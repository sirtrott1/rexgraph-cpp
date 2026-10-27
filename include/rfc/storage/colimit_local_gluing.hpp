#pragma once
#include "rfc/storage/certified_stalk_colimit.hpp"
#include "rfc/storage/injective_local_gluing.hpp"

namespace rfc::storage {
struct ColimitInterfaceStalk {
    DeclaredStalkBinding common;
    std::array<ColimitInjection,2> sides;
};
struct ColimitFieldInterface {
    DeclaredLocalFieldBinding common;
    std::size_t left{},right{};
    std::array<algebra::RetainedSourceMap,2> maps;
    std::vector<ColimitInterfaceStalk> stalks;
};
struct ColimitStalkOrigin {
    std::size_t patch{};
    DeclaredStalkBinding local;
    bool operator==(const ColimitStalkOrigin& other)const {
        return patch==other.patch && local.carrier==other.local.carrier && local.declaration==other.local.declaration;
    }
};
struct ColimitCellWitness {
    std::vector<ColimitStalkOrigin> origins;
    StalkColimitWitness witness;
    bool operator==(const ColimitCellWitness&) const = default;
};
struct ColimitStalkRange {
    std::size_t offset{},dimension{};
    ColimitCellWitness certificate;
    std::vector<ColimitLinearLink> interfaces;
    bool operator==(const ColimitStalkRange&) const = default;
};
namespace colimit_gluing_detail {
using namespace local_gluing_detail;
using injective_gluing_detail::source;
using injective_gluing_detail::stalk;
using injective_gluing_detail::same_stalk;
using injective_gluing_detail::raw_map;
struct Cell {
    std::vector<Origin> origins;
    std::vector<ColimitCoordinateSpace> parts;
    std::vector<ColimitStalkOrigin> typed_origins;
    std::vector<ColimitLinearLink> links;
    std::optional<CertifiedStalkColimit> cover;
};
struct Route {std::size_t patch{},slot{},upper{},lower{},upper_part{},lower_part{};};
struct Grade {
    std::vector<Cell> cells;
    std::vector<ColimitStalkRange> layout;
    std::vector<std::vector<std::size_t>> images,parts;
    std::vector<Route> routes;
    std::vector<std::vector<std::size_t>> from_upper,from_lower;
    std::vector<std::size_t> offsets{0};
    std::size_t occurrences{};
};
struct State {
    std::vector<DeclaredLocalFieldBinding> patches;
    std::vector<ColimitCellWitness> certificates;
    std::vector<ColimitFieldInterface> interfaces;
    std::vector<std::map<core::CellId,std::size_t>> interface_index;
    std::vector<Grade> grades;
    std::size_t records{};
};
inline core::CellId matched(const DeclaredLocalFieldBinding& b,const DeclaredStalkBinding& p){
    require(p.carrier.cell.has_value(),"colimit must name a local cell");
    const auto cell=StableSourceIndex(source(b)).cell(*p.carrier.cell);
    require(cell.has_value() && same_stalk(p,stalk(b,*cell)),"foreign colimit cell/type/coordinate declaration");return *cell;
}
inline ColimitCoordinateSpace space(std::string name,const DeclaredStalkBinding& p){return {std::move(name),p.carrier.coordinate_system,p.declaration};}
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
} // namespace colimit_gluing_detail

// Resident unsplit finite cover with supplied exact quotient and observation
// witnesses. Local injections must survive the entire diagram. Nothing here
// discovers a complement or identifies a readout with an adjoint/sheaf map.
class ColimitLocalFieldView {
    using State=colimit_gluing_detail::State;using G=numeric::ExactActionGuard;using S=numeric::SparseRationalVector;
    std::shared_ptr<const State> state_;
public:
    ColimitLocalFieldView(std::vector<DeclaredLocalFieldBinding> patches,std::vector<ColimitCellWitness> certificates,
        std::vector<ColimitFieldInterface> interfaces,numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}){
        using namespace colimit_gluing_detail;
        auto s=std::make_shared<State>(State{std::move(patches),std::move(certificates),std::move(interfaces),{},{},0});
        G g(limits,std::move(checkpoint));g.step();require(!s->patches.empty(),"finite cover needs patches");
        const auto count=source(s->patches[0]).grade_count();std::size_t cells_needed=0;
        for(const auto& p:s->patches){require(source(p).grade_count()==count,"cover grade inventory differs");s->records=add(s->records,algebra::owned_gluing_detail::words(source(p)));for(std::size_t k=0;k<count;++k)cells_needed=add(cells_needed,source(p).grade(grade(k)).cell_count());}
        // Account for retained certificate copies in the view, per-cell plan and
        // public layout before constructing those derived objects.
        for(const auto& c:s->certificates){
            const auto keep=[&](const numeric::ExactMatrix& m){g.matrix(m);s->records=add(s->records,machine::checked_size_mul(add(m.nonzeros(),1),4));g.entries(s->records);};
            keep(c.witness.quotient);keep(c.witness.section);keep(c.witness.relation_lift);keep(c.witness.reconstruct_observations);
            for(const auto& r:c.witness.readouts)keep(r);
            s->records=add(s->records,machine::checked_size_mul(add(c.origins.size(),1),4));g.entries(s->records);
        }
        // Each certificate names the complete ordered list of its local origins.
        // The source quotient below determines that list; callers cannot substitute
        // equal-dimensional unrelated sources or omit an identified local copy.
        for(const auto& e:s->interfaces){
            require(e.left<s->patches.size() && e.right<s->patches.size() && e.left!=e.right,"cover interface endpoints differ");const auto& common=source(e.common);
            s->records=add(s->records,algebra::owned_gluing_detail::words(common));std::array<std::size_t,2> sides{e.left,e.right};
            for(std::size_t p=0;p<2;++p){s->records=add(s->records,algebra::owned_gluing_detail::words(e.maps[p]));algebra::owned_gluing_detail::map(common,source(s->patches[sides[p]]),e.maps[p],true,g);}
            std::map<core::CellId,std::size_t> index;
            for(std::size_t i=0;i<e.stalks.size();++i){
                const auto& v=e.stalks[i];const auto kc=matched(e.common,v.common);require(index.emplace(kc,i).second,"common colimit identity/repetition");
                const auto kd=v.common.carrier.domain.logical_size();
                for(std::size_t p=0;p<2;++p){
                    const auto& local=stalk(s->patches[sides[p]],{kc.grade,e.maps[p].cells.at(kc.grade.value).at(kc.index)});
                    const auto n=local.carrier.domain.logical_size();const auto& f=v.sides[p];
                    require(f.map.rows()==n && f.map.cols()==kd && f.left_inverse.rows()==kd && f.left_inverse.cols()==n,"colimit interface carrier mismatch");
                    require(certified_colimit_detail::identity(certified_colimit_detail::product(f.left_inverse,f.map,g,s->records),g),"colimit interface not injective");
                    s->records=add(s->records,add(f.map.nonzeros(),f.left_inverse.nonzeros()));g.entries(s->records);
                }
            }
            std::size_t need=0;for(std::size_t k=0;k<common.grade_count();++k)need=add(need,common.grade(grade(k)).cell_count());require(need==index.size(),"common colimit coverage incomplete");s->interface_index.push_back(std::move(index));
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
                dst.images[p].push_back(at->second);dst.parts[p].push_back(cell.parts.size());cell.origins.push_back({p,i});const auto& sb=stalk(s->patches[p],{kg,static_cast<std::uint32_t>(i)});cell.typed_origins.push_back({p,sb});cell.parts.push_back(space("patch-"+std::to_string(p),sb));
            }
            for(std::size_t eidx=0;eidx<s->interfaces.size();++eidx){const auto& e=s->interfaces[eidx];if(k>=source(e.common).grade_count())continue;
                for(std::size_t i=0;i<e.maps[0].cells[k].size();++i){const auto& v=e.stalks.at(s->interface_index[eidx].at({kg,static_cast<std::uint32_t>(i)}));
                    const auto l=e.maps[0].cells[k][i],r=e.maps[1].cells[k][i];auto& cell=dst.cells[dst.images[e.left][l]];
                    g.step();cell.links.push_back({"interface-"+std::to_string(eidx),dst.parts[e.left][l],dst.parts[e.right][r],space("common-"+std::to_string(eidx),v.common),v.sides});
                }
            }
            for(auto& cell:dst.cells){
                const ColimitCellWitness* certificate=nullptr;
                for(const auto& c:s->certificates){g.step();if(c.origins==cell.typed_origins){require(!certificate,"duplicate colimit cell certificate");certificate=&c;}}
                require(certificate!=nullptr,"missing complete ordered colimit origin certificate");
                cell.cover.emplace(cell.parts,cell.links,certificate->witness,g);s->records=add(s->records,cell.cover->stored_entries());g.entries(s->records);
                ColimitStalkRange layout{dst.offsets.back(),cell.cover->dimension(),*certificate,cell.links};
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
                    const auto km=raw_map(e.common,kg,slot);const std::array<std::size_t,2> sides{e.left,e.right};
                    for(std::size_t p=0;p<2;++p){
                        const auto lm=raw_map(s->patches[sides[p]],kg,e.maps[p].occurrences[k][slot]);
                        const auto left=lm.multiply(StalkChartMap::explicit_map(u.sides[p].map),g);
                        const auto right=StalkChartMap::explicit_map(l.sides[p].map).multiply(km,g);
                        require(left.equals(right,g),"local boundary fails whole unsplit interface descent");
                    }
                }
            }
        }
        std::size_t quotient_cells=0;for(const auto& k:s->grades)quotient_cells=add(quotient_cells,k.cells.size());
        require(quotient_cells==s->certificates.size(),"extra or missing colimit certificates");
        state_=std::move(s);
    }
    std::size_t grade_count()const{return state_->grades.size();}
    std::size_t patch_count()const{return state_->patches.size();}
    std::size_t dimension(core::Grade k)const{return state_->grades.at(k.value).offsets.back();}
    std::size_t cell_count(core::Grade k)const{return state_->grades.at(k.value).cells.size();}
    std::size_t occurrence_count(core::Grade k)const{return state_->grades.at(k.value).occurrences;}
    std::size_t retained_records()const{return state_->records;}
    std::size_t image(std::size_t p,core::CellId c)const{return state_->grades.at(c.grade.value).images.at(p).at(c.index);}
    const std::vector<ColimitStalkRange>& layout(core::Grade k)const &{return state_->grades.at(k.value).layout;}
    const std::vector<ColimitStalkRange>& layout(core::Grade)const &&=delete;
    const std::vector<DeclaredLocalFieldBinding>& patches()const &{return state_->patches;}
    const std::vector<DeclaredLocalFieldBinding>& patches()const &&=delete;
    S glue_fields(core::Grade k,const std::vector<field::SparseStalkField>& values,G& g)const {
        using namespace colimit_gluing_detail;const auto pin=state_;require(values.size()==pin->patches.size(),"one total input per patch required");std::size_t held=pin->records;
        for(std::size_t p=0;p<values.size();++p){require(values[p].grade()==k && values[p].owner().same_owner(pin->patches[p].fibers()),"foreign cover input owner/grade");held=add(held,values[p].nonzeros());g.entries(held);}
        const auto& dst=pin->grades.at(k.value);std::map<std::size_t,Q> out;
        for(std::size_t c=0;c<dst.cells.size();++c){g.step();const auto& cell=dst.cells[c];std::vector<S> xs;
            for(const auto& o:cell.origins){const auto it=values[o.patch].values().find(static_cast<std::uint32_t>(o.local));const auto n=stalk(pin->patches[o.patch],{k,static_cast<std::uint32_t>(o.local)}).carrier.domain.logical_size();xs.push_back(it==values[o.patch].values().end()?S(n):it->second);}
            const auto v=cell.cover->glue(xs,g);for(const auto& e:v.entries())accumulate(out,add(dst.offsets[c],e.index),e.value,g,add(held,v.nonzeros()));
        }
        return numeric::checked_sparse_detail::vector(dst.offsets.back(),std::move(out),g);
    }
    S include_chain_field(core::Grade k,const field::SparseStalkField& x,std::size_t p,G& g)const {
        using namespace colimit_gluing_detail;const auto pin=state_;require(p<pin->patches.size() && x.grade()==k && x.owner().same_owner(pin->patches[p].fibers()),"foreign cover chain injection");
        const auto& dst=pin->grades.at(k.value);g.entries(add(pin->records,x.nonzeros()));std::map<std::size_t,Q> out;
        for(const auto& [i,v]:x.values()){g.step();const auto ci=dst.images[p].at(i);const auto y=dst.cells[ci].cover->include(dst.parts[p][i],v,g);for(const auto& e:y.entries())accumulate(out,add(dst.offsets[ci],e.index),e.value,g,add(pin->records,add(x.nonzeros(),y.nonzeros())));}
        return numeric::checked_sparse_detail::vector(dst.offsets.back(),std::move(out),g);
    }
    field::SparseStalkField restrict_field(core::Grade k,const S& x,std::size_t p,G& g)const {
        using namespace colimit_gluing_detail;const auto pin=state_;require(p<pin->patches.size() && x.size()==pin->grades.at(k.value).offsets.back(),"cover restriction carrier");g.sparse_vector(x);const auto& dst=pin->grades[k.value];field::SparseStalkValues out;auto held=add(pin->records,x.nonzeros());
        for(std::size_t i=0;i<dst.images[p].size();++i){g.step();const auto ci=dst.images[p][i];const auto& c=dst.cells[ci];const auto y=c.cover->restrict(dst.parts[p][i],part(x,dst.offsets[ci],c.cover->dimension(),g),g);if(y.nonzeros()){held=add(held,add(1,y.nonzeros()));g.entries(held);out.emplace(static_cast<std::uint32_t>(i),y);}}
        return field::SparseStalkFieldActions(pin->patches[p].fibers(),{},g.limits()).bind(k,std::move(out));
    }
    field::FactoredAction boundary(core::Grade k,std::optional<std::size_t> component={})const {
        using namespace colimit_gluing_detail;const auto pin=state_;require(k.value>0 && k.value<pin->grades.size() && (!component || *component<pin->patches.size()),"cover boundary selection");
        const auto make=[k,component](bool transpose){return [k,component,transpose](const auto& s,const auto& x,G& g){if constexpr(std::is_same_v<std::decay_t<decltype(x)>,S>)return colimit_gluing_detail::action(*s,k,x,transpose,component,g);else return colimit_gluing_detail::action(*s,k,sparse_input(x,g),transpose,component,g).dense();};};
        return field::FactoredAction::owned(pin,pin->grades[k.value-1].offsets.back(),pin->grades[k.value].offsets.back(),pin->records,make(false),make(true));
    }
};
} // namespace rfc::storage
