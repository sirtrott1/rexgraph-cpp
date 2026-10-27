#pragma once
#include "rfc/storage/certified_stalk_observations.hpp"
#include "rfc/storage/injective_local_gluing.hpp"

namespace rfc::storage {
struct ObservationInterfaceStalk {
    DeclaredStalkBinding common;
    std::array<numeric::ExactMatrix,2> restrictions;
};
struct ObservationFieldInterface {
    DeclaredLocalFieldBinding common;
    std::size_t left{},right{};
    std::array<algebra::RetainedSourceMap,2> maps;
    std::vector<ObservationInterfaceStalk> stalks;
};
struct ObservationStalkOrigin {
    std::size_t patch{};
    DeclaredStalkBinding local;
    bool operator==(const ObservationStalkOrigin& other)const {
        return patch==other.patch && local.carrier==other.local.carrier && local.declaration==other.local.declaration;
    }
};
struct ObservationCellWitness {
    std::vector<ObservationStalkOrigin> origins;
    StalkObservationWitness witness;
    bool operator==(const ObservationCellWitness&) const = default;
};
struct ObservationStalkRange {
    std::size_t offset{},dimension{};
    ObservationCellWitness certificate;
    std::vector<ObservationLinearLink> interfaces;
    bool operator==(const ObservationStalkRange&) const = default;
};
namespace sheaf_cover_detail {
using namespace local_gluing_detail;
using injective_gluing_detail::source;
inline const DeclaredStalkBinding& stalk(const DeclaredLocalFieldBinding& b,core::CellId cell){
    const auto key=stable_cell_key(source(b),cell);
    for(const auto& s:b.contract().upward.stalks)if(s.carrier.cell==key)return s;
    throw std::invalid_argument("upward stalk declaration absent");
}
using injective_gluing_detail::same_stalk;
// This is the DECLARED upward restriction. The signed incidence coefficient
// is applied once by the cochain action, just as in SparseSheafActions.
inline StalkChartMap raw_map(const DeclaredLocalFieldBinding& b,core::Grade k,std::size_t slot){
    const auto& sh=b.sheaf().sheaf();const auto& a=source(b).grade(k);
    const auto& m=sh.restriction_override(k,slot);
    if(m)return StalkChartMap::explicit_map(*m);
    const auto n=sh.stalks().dimension({{static_cast<std::uint16_t>(k.value-1)},a.lower_index[slot]});
    return StalkChartMap::identity(n,1);
}
struct Cell {
    std::vector<Origin> origins;
    std::vector<ObservationCoordinateSpace> parts;
    std::vector<ObservationStalkOrigin> typed_origins;
    std::vector<ObservationLinearLink> links;
    std::optional<CertifiedStalkObservations> cover;
};
struct Route {std::size_t patch{},slot{},upper{},lower{},upper_part{},lower_part{};};
struct Grade {
    std::vector<Cell> cells;
    std::vector<ObservationStalkRange> layout;
    std::vector<std::vector<std::size_t>> images,parts;
    std::vector<Route> routes;
    std::vector<std::vector<std::size_t>> from_upper,from_lower;
    std::vector<std::size_t> offsets{0};
    std::size_t occurrences{};
};
struct State {
    std::vector<DeclaredLocalFieldBinding> patches;
    std::vector<ObservationCellWitness> certificates;
    std::vector<ObservationFieldInterface> interfaces;
    std::vector<std::map<core::CellId,std::size_t>> interface_index;
    std::vector<Grade> grades;
    std::size_t records{};
};
inline core::CellId matched(const DeclaredLocalFieldBinding& b,const DeclaredStalkBinding& p){
    require(p.carrier.cell.has_value(),"colimit must name a local cell");
    const auto cell=StableSourceIndex(source(b)).cell(*p.carrier.cell);
    require(cell.has_value() && same_stalk(p,stalk(b,*cell)),"foreign observation cell/type/coordinate declaration");return *cell;
}
inline ObservationCoordinateSpace space(std::string name,const DeclaredStalkBinding& p){return {std::move(name),p.carrier.coordinate_system,p.declaration};}
inline S action(const State& s,core::Grade upper_grade,const S& x,bool transpose,std::optional<std::size_t> component,Guard& g){
    const auto& routes=s.grades.at(upper_grade.value);
    const auto& input=s.grades.at(upper_grade.value-(transpose?0u:1u));
    const auto& output=s.grades.at(upper_grade.value-(transpose?1u:0u));
    require(x.size()==input.offsets.back(),"sheaf cover action input carrier");
    g.sparse_vector(x);g.dimension(output.offsets.back());g.entries(s.records);std::map<std::size_t,Q> out;
    for(auto it=x.entries().begin();it!=x.entries().end();){
        g.step();const auto ci=std::size_t(std::upper_bound(input.offsets.begin(),input.offsets.end(),it->index)-input.offsets.begin()-1);
        const auto value=part(x,input.offsets[ci],input.cells[ci].cover->dimension(),g);
        for(const auto ri:(transpose?routes.from_upper.at(ci):routes.from_lower.at(ci))){
            const auto& r=routes.routes[ri];g.step();if(component && r.patch!=*component)continue;
            const auto& b=s.patches[r.patch];const auto& c=source(b);
            if(!c.grade(upper_grade).aspects.get(c.signature().existence_aspect(),r.slot))continue;
            const auto coefficient=c.incidence_coefficient(upper_grade,r.slot);g.value(coefficient);if(coefficient==0)continue;
            const auto& from=*input.cells[ci].cover;
            const auto local=transpose?from.contribution_transpose(r.upper_part,value,g):from.restrict(r.lower_part,value,g);
            const auto acted=raw_map(b,upper_grade,r.slot).apply(local,g,transpose);
            const auto target=transpose?r.lower:r.upper;const auto& to=*output.cells[target].cover;
            const auto mapped=transpose?to.restrict_transpose(r.lower_part,acted,g):to.contribution(r.upper_part,acted,g);
            const auto held=add(s.records,add(x.nonzeros(),add(value.nonzeros(),add(local.nonzeros(),add(acted.nonzeros(),mapped.nonzeros())))));
            for(const auto& e:mapped.entries())accumulate(out,add(output.offsets[target],e.index),g.mul(coefficient,e.value),g,held);
        }
        it=std::lower_bound(it,x.entries().end(),input.offsets[ci+1],[](const auto& e,auto n){return e.index<n;});
    }
    return numeric::checked_sparse_detail::vector(output.offsets.back(),std::move(out),g);
}
} // namespace sheaf_cover_detail

// Resident finite observation limit with explicit upward incidence restrictions.
// The target stalk is exactly the compatible local observation family. This is
// not the downward chain colimit, and its coboundary is not inferred by transpose.
// Arbitrary cochains can be glued; ker(delta^0) and delta^2 need separate checks.
class SheafCoverLocalFieldView {
    using State=sheaf_cover_detail::State;using G=numeric::ExactActionGuard;using S=numeric::SparseRationalVector;
    std::shared_ptr<const State> state_;
public:
    SheafCoverLocalFieldView(std::vector<DeclaredLocalFieldBinding> patches,std::vector<ObservationCellWitness> certificates,
        std::vector<ObservationFieldInterface> interfaces,numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}){
        using namespace sheaf_cover_detail;
        auto s=std::make_shared<State>(State{std::move(patches),std::move(certificates),std::move(interfaces),{},{},0});
        G g(limits,std::move(checkpoint));g.step();require(!s->patches.empty(),"finite cover needs patches");
        const auto count=source(s->patches[0]).grade_count();std::size_t cells_needed=0;
        for(const auto& p:s->patches){require(source(p).grade_count()==count,"cover grade inventory differs");s->records=add(s->records,algebra::owned_gluing_detail::words(source(p)));for(std::size_t k=0;k<count;++k)cells_needed=add(cells_needed,source(p).grade(grade(k)).cell_count());}
        // Account for retained certificate copies in the view, per-cell plan and
        // public layout before constructing those derived objects.
        for(const auto& c:s->certificates){
            const auto keep=[&](const numeric::ExactMatrix& m){g.matrix(m);s->records=add(s->records,machine::checked_size_mul(add(m.nonzeros(),1),4));g.entries(s->records);};
            keep(c.witness.accession);keep(c.witness.reconstruction);keep(c.witness.compatibility_lift);
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
                const auto& v=e.stalks[i];const auto kc=matched(e.common,v.common);require(index.emplace(kc,i).second,"common observation identity/repetition");
                const auto kd=v.common.carrier.domain.logical_size();
                for(std::size_t p=0;p<2;++p){
                    const auto& local=stalk(s->patches[sides[p]],{kc.grade,e.maps[p].cells.at(kc.grade.value).at(kc.index)});
                    const auto n=local.carrier.domain.logical_size();const auto& f=v.restrictions[p];
                    require(f.rows()==kd && f.cols()==n,"observation interface carrier mismatch");g.matrix(f);
                    s->records=add(s->records,f.nonzeros());g.entries(s->records);
                }
            }
            std::size_t need=0;for(std::size_t k=0;k<common.grade_count();++k)need=add(need,common.grade(grade(k)).cell_count());require(need==index.size(),"common observation coverage incomplete");s->interface_index.push_back(std::move(index));
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
                    g.step();cell.links.push_back({"interface-"+std::to_string(eidx),dst.parts[e.left][l],dst.parts[e.right][r],space("common-"+std::to_string(eidx),v.common),v.restrictions});
                }
            }
            for(auto& cell:dst.cells){
                const ObservationCellWitness* certificate=nullptr;
                for(const auto& c:s->certificates){g.step();if(c.origins==cell.typed_origins){require(!certificate,"duplicate observation cell certificate");certificate=&c;}}
                require(certificate!=nullptr,"missing complete ordered observation origin certificate");
                cell.cover.emplace(cell.parts,cell.links,certificate->witness,g);s->records=add(s->records,cell.cover->stored_entries());g.entries(s->records);
                ObservationStalkRange layout{dst.offsets.back(),cell.cover->dimension(),*certificate,cell.links};
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
                        const auto left=StalkChartMap::explicit_map(u.restrictions[p]).multiply(lm,g);
                        const auto right=km.multiply(StalkChartMap::explicit_map(l.restrictions[p]),g);
                        require(left.equals(right,g),"local sheaf fails whole observation interface naturality");
                    }
                }
            }
        }
        std::size_t quotient_cells=0;for(const auto& k:s->grades)quotient_cells=add(quotient_cells,k.cells.size());
        require(quotient_cells==s->certificates.size(),"extra or missing observation certificates");
        state_=std::move(s);
    }
    std::size_t grade_count()const{return state_->grades.size();}
    std::size_t patch_count()const{return state_->patches.size();}
    std::size_t dimension(core::Grade k)const{return state_->grades.at(k.value).offsets.back();}
    std::size_t cell_count(core::Grade k)const{return state_->grades.at(k.value).cells.size();}
    std::size_t occurrence_count(core::Grade k)const{return state_->grades.at(k.value).occurrences;}
    std::size_t retained_records()const{return state_->records;}
    std::size_t image(std::size_t p,core::CellId c)const{return state_->grades.at(c.grade.value).images.at(p).at(c.index);}
    const std::vector<ObservationStalkRange>& layout(core::Grade k)const &{return state_->grades.at(k.value).layout;}
    const std::vector<ObservationStalkRange>& layout(core::Grade)const &&=delete;
    const std::vector<DeclaredLocalFieldBinding>& patches()const &{return state_->patches;}
    const std::vector<DeclaredLocalFieldBinding>& patches()const &&=delete;
    S glue_fields(core::Grade k,const std::vector<local::SparseSheafCochain>& values,G& g)const {
        using namespace sheaf_cover_detail;const auto pin=state_;require(values.size()==pin->patches.size(),"one total input per patch required");std::size_t held=pin->records;
        for(std::size_t p=0;p<values.size();++p){require(values[p].grade()==k && values[p].owner().same_owner(pin->patches[p].sheaf()),"foreign cover input owner/grade");held=add(held,values[p].nonzeros());g.entries(held);}
        const auto& dst=pin->grades.at(k.value);std::map<std::size_t,Q> out;
        for(std::size_t c=0;c<dst.cells.size();++c){g.step();const auto& cell=dst.cells[c];std::vector<S> xs;
            for(const auto& o:cell.origins){const auto it=values[o.patch].values().find(static_cast<std::uint32_t>(o.local));const auto n=stalk(pin->patches[o.patch],{k,static_cast<std::uint32_t>(o.local)}).carrier.domain.logical_size();xs.push_back(it==values[o.patch].values().end()?S(n):it->second);}
            const auto v=cell.cover->glue(xs,g);for(const auto& e:v.entries())accumulate(out,add(dst.offsets[c],e.index),e.value,g,add(held,v.nonzeros()));
        }
        return numeric::checked_sparse_detail::vector(dst.offsets.back(),std::move(out),g);
    }
    local::SparseSheafCochain restrict_field(core::Grade k,const S& x,std::size_t p,G& g)const {
        using namespace sheaf_cover_detail;const auto pin=state_;require(p<pin->patches.size() && x.size()==pin->grades.at(k.value).offsets.back(),"cover restriction carrier");g.sparse_vector(x);const auto& dst=pin->grades[k.value];local::SparseSheafValues out;auto held=add(pin->records,x.nonzeros());
        for(std::size_t i=0;i<dst.images[p].size();++i){g.step();const auto ci=dst.images[p][i];const auto& c=dst.cells[ci];const auto y=c.cover->restrict(dst.parts[p][i],part(x,dst.offsets[ci],c.cover->dimension(),g),g);if(y.nonzeros()){held=add(held,add(1,y.nonzeros()));g.entries(held);out.emplace(static_cast<std::uint32_t>(i),y);}}
        return local::SparseSheafActions(pin->patches[p].sheaf(),g.limits()).bind(k,std::move(out));
    }
    // q denotes the input cochain grade. All shared coface fibers must be complete.
    // Readout naturality, rather than an injection square, makes restriction to a
    // boundary-closed patch commute with this assembled coboundary.
    field::FactoredAction coboundary(core::Grade q,std::optional<std::size_t> component={})const {
        using namespace sheaf_cover_detail;const auto pin=state_;
        require(std::size_t(q.value)+1<pin->grades.size() && (!component || *component<pin->patches.size()),"cover coboundary selection");
        const auto k=grade(std::size_t(q.value)+1);
        const auto make=[k,component](bool transpose){return [k,component,transpose](const auto& s,const auto& x,G& g){
            if constexpr(std::is_same_v<std::decay_t<decltype(x)>,S>)return sheaf_cover_detail::action(*s,k,x,transpose,component,g);
            else return sheaf_cover_detail::action(*s,k,sparse_input(x,g),transpose,component,g).dense();};};
        return field::FactoredAction::owned(pin,pin->grades[k.value].offsets.back(),pin->grades[q.value].offsets.back(),pin->records,make(false),make(true));
    }
};
} // namespace rfc::storage
