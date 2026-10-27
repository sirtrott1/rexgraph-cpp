#pragma once
#include <array>
#include "rfc/storage/glued_local_fields.hpp"

namespace rfc::storage {
// All coordinate spaces and interface arrows have explicit local identities.
struct ColimitCoordinateSpace {
    std::string name;
    std::uint64_t coordinates{};
    composite::Declaration declaration;
    bool operator==(const ColimitCoordinateSpace&) const = default;
};
struct ColimitInjection {
    numeric::ExactMatrix map, left_inverse;
    bool operator==(const ColimitInjection&) const = default;
};
struct ColimitLinearLink {
    std::string name;
    std::size_t left{}, right{};
    ColimitCoordinateSpace common;
    std::array<ColimitInjection,2> sides;
    bool operator==(const ColimitLinearLink&) const = default;
};
// W=sum(local stalks), K=sum(interface stalks), Delta:K->W is built from
// the declared difference injections. Q:W->X and H:X->W satisfy QH=I,
// Q Delta=0, Delta Z=I-HQ. These equations prove ker Q=im Delta and
// surjectivity, even with redundant cycles. They do not discover a complement.
// Readouts R_p:X->V_p and E:W->X are SEPARATE observation data:
// R_p J_p=I and E stack(R_p)=I. A tuple glues only if it belongs to the
// image of these readouts. H is a chain representative, not a restriction.
struct StalkColimitWitness {
    ColimitCoordinateSpace target;
    numeric::ExactMatrix quotient, section, relation_lift;
    std::vector<numeric::ExactMatrix> readouts;
    numeric::ExactMatrix reconstruct_observations;
    bool operator==(const StalkColimitWitness&) const = default;
};
namespace certified_colimit_detail {
using namespace local_gluing_detail;
using M=numeric::ExactMatrix;
inline std::size_t extent(const ColimitCoordinateSpace& c,Guard& g){
    require(!c.name.empty() && c.coordinates!=0,"colimit coordinate identity missing");
    require(c.declaration.kind==composite::DeclarationKind::RationalTensor && c.declaration.axes.size()==1 && c.declaration.axes[0].extent>0,"colimit needs positive one-axis rational coordinates");
    const auto n=c.declaration.axes[0].extent;g.dimension(n);return n;
}
// These helpers charge input, indexed copies, output fill and scalar work.
// Counts are conservative stored scalar/metadata entries, not GMP/RSS quotas.
inline void set(M& a,std::size_t i,std::size_t j,const Q& q,Guard& g,std::size_t held){
    g.step();g.value(q);g.entries(add(held,add(a.nonzeros(),1)));a.set(i,j,q);
}
inline M product(const M& a,const M& b,Guard& g,std::size_t held=0){
    require(a.cols()==b.rows(),"colimit product carriers");g.matrix(a);g.matrix(b);
    const auto retained=add(held,machine::checked_size_mul(add(a.nonzeros(),b.nonzeros()),4));g.entries(retained);
    std::map<std::size_t,std::vector<std::pair<std::size_t,Q>>> rows;
    for(const auto& [ij,q]:b.entries()){g.step();rows[ij.first].emplace_back(ij.second,q);}
    M out(a.rows(),b.cols());
    for(const auto& [ij,q]:a.entries()){
        g.step();const auto it=rows.find(ij.second);if(it==rows.end())continue;
        for(const auto& [j,v]:it->second)set(out,ij.first,j,g.add(out.get(ij.first,j),g.mul(q,v)),g,retained);
    }return out;
}
inline M block(const M& a,std::size_t ro,std::size_t nr,std::size_t co,std::size_t nc,Guard& g,std::size_t held=0){
    require(ro<=a.rows() && nr<=a.rows()-ro && co<=a.cols() && nc<=a.cols()-co,"colimit block bounds");
    M out(nr,nc);for(const auto& [ij,q]:a.entries()){g.step();if(ij.first>=ro && ij.first-ro<nr && ij.second>=co && ij.second-co<nc)set(out,ij.first-ro,ij.second-co,q,g,add(held,a.nonzeros()));}return out;
}
inline bool identity(const M& a,Guard& g){
    g.matrix(a);if(a.rows()!=a.cols() || a.nonzeros()!=a.rows())return false;
    for(const auto& [ij,q]:a.entries()){g.step();if(ij.first!=ij.second || q!=1)return false;}return true;
}
inline bool sum_is_identity(M a,const M& b,Guard& g,std::size_t held){
    if(a.rows()!=b.rows() || a.cols()!=b.cols())return false;
    for(const auto& [ij,q]:b.entries())set(a,ij.first,ij.second,g.add(a.get(ij.first,ij.second),q),g,add(held,b.nonzeros()));
    return identity(a,g);
}
inline S apply(const M& a,const S& x,Guard& g,std::size_t held,bool transpose=false){
    g.matrix(a);g.sparse_vector(x);require(x.size()==(transpose?a.rows():a.cols()),"colimit action carrier");
    held=add(held,add(a.nonzeros(),x.nonzeros()));std::map<std::size_t,Q> out;
    for(const auto& [ij,q]:a.entries()){
        g.step();const auto index=transpose?ij.first:ij.second;
        const auto it=std::lower_bound(x.entries().begin(),x.entries().end(),index,[](const auto& e,auto i){return e.index<i;});
        if(it!=x.entries().end() && it->index==index)accumulate(out,transpose?ij.second:ij.first,g.mul(q,it->value),g,held);
    }
    g.entries(add(held,machine::checked_size_mul(out.size(),2)));
    return numeric::checked_sparse_detail::vector(transpose?a.cols():a.rows(),std::move(out),g);
}
} // namespace certified_colimit_detail

class CertifiedStalkColimit {
    using M=numeric::ExactMatrix;using S=numeric::SparseRationalVector;using G=numeric::ExactActionGuard;
    struct State {
        std::vector<ColimitCoordinateSpace> spaces;
        std::vector<ColimitLinearLink> links;
        StalkColimitWitness witness;
        std::vector<std::size_t> offsets;
        M relations;
        std::vector<M> injections,representatives,observation_parts;
        std::size_t dimension{},records{};
    };
    std::shared_ptr<const State> state_;
public:
    CertifiedStalkColimit(std::vector<ColimitCoordinateSpace> spaces,std::vector<ColimitLinearLink> links,
                         StalkColimitWitness witness,G& g){
        using namespace certified_colimit_detail;
        g.step();require(!spaces.empty(),"colimit needs local spaces");
        auto s=std::make_shared<State>(State{std::move(spaces),std::move(links),std::move(witness),{0},M{}, {},{},{},0,0});
        auto& w=s->witness;std::set<std::string> names;std::vector<composite::Declaration> defs;
        s->records=add(s->spaces.size(),s->links.size());g.entries(s->records);
        for(const auto& c:s->spaces){g.step();require(names.insert(c.name).second,"duplicate local colimit identity");s->offsets.push_back(add(s->offsets.back(),extent(c,g)));g.dimension(s->offsets.back());defs.push_back(c.declaration);}
        s->dimension=extent(w.target,g);defs.push_back(w.target.declaration);const auto n=s->offsets.back(),m=s->dimension;
        std::size_t kd=0;names.clear();
        for(const auto& e:s->links){
            g.step();require(!e.name.empty() && names.insert(e.name).second,"duplicate interface identity");
            require(e.left<s->spaces.size() && e.right<s->spaces.size() && e.left!=e.right,"colimit link endpoints");
            kd=add(kd,extent(e.common,g));g.dimension(kd);defs.push_back(e.common.declaration);
        }
        (void)composite::DeclarationRegistry(std::move(defs));
        const auto retain=[&](const M& a){g.matrix(a);s->records=add(s->records,a.nonzeros());g.entries(s->records);};
        retain(w.quotient);retain(w.section);retain(w.relation_lift);retain(w.reconstruct_observations);
        require(w.quotient.rows()==m && w.quotient.cols()==n && w.section.rows()==n && w.section.cols()==m && w.relation_lift.rows()==kd && w.relation_lift.cols()==n,"colimit witness shapes");
        require(w.readouts.size()==s->spaces.size() && w.reconstruct_observations.rows()==m && w.reconstruct_observations.cols()==n,"colimit observation shapes");
        for(std::size_t p=0;p<s->spaces.size();++p){const auto d=s->offsets[p+1]-s->offsets[p];retain(w.readouts[p]);require(w.readouts[p].rows()==d && w.readouts[p].cols()==m,"local readout shape");}
        s->relations=M(n,kd);std::size_t col=0;
        for(const auto& e:s->links){
            const auto d=e.common.declaration.axes[0].extent;const std::array<std::size_t,2> ps{e.left,e.right};
            for(std::size_t side=0;side<2;++side){const auto p=ps[side],pd=s->offsets[p+1]-s->offsets[p];const auto& f=e.sides[side];
                retain(f.map);retain(f.left_inverse);require(f.map.rows()==pd && f.map.cols()==d && f.left_inverse.rows()==d && f.left_inverse.cols()==pd,"interface injection shape");
                require(identity(product(f.left_inverse,f.map,g,s->records),g),"interface map is not certified injective");
                for(const auto& [ij,q]:f.map.entries())set(s->relations,add(s->offsets[p],ij.first),add(col,ij.second),side==0?q:Q{-q},g,s->records);
            }col=add(col,d);
        }
        retain(s->relations);
        require(identity(product(w.quotient,w.section,g,s->records),g),"colimit quotient section is not a right inverse");
        require(product(w.quotient,s->relations,g,s->records).nonzeros()==0,"colimit quotient fails interface equality");
        const auto hq=product(w.section,w.quotient,g,s->records);
        const auto dz=product(s->relations,w.relation_lift,g,add(s->records,hq.nonzeros()));
        require(sum_is_identity(hq,dz,g,s->records),"colimit kernel differs from declared relation image");
        M ra(n,m);
        for(std::size_t p=0;p<s->spaces.size();++p){
            const auto off=s->offsets[p],d=s->offsets[p+1]-off;
            auto j=block(w.quotient,0,m,off,d,g,s->records);
            require(identity(product(w.readouts[p],j,g,s->records),g),"colimit collapses a local direction or readout is not its left inverse");
            for(const auto& [ij,q]:w.readouts[p].entries())set(ra,add(off,ij.first),ij.second,q,g,s->records);
            auto h=block(w.section,off,d,0,m,g,s->records);auto e=block(w.reconstruct_observations,0,m,off,d,g,s->records);
            retain(j);retain(h);retain(e);s->injections.push_back(std::move(j));s->representatives.push_back(std::move(h));s->observation_parts.push_back(std::move(e));
        }
        require(identity(product(w.reconstruct_observations,ra,g,s->records),g),"joint readouts are not completely reconstructible");
        state_=std::move(s);
    }
    std::size_t dimension()const{return state_->dimension;}
    std::size_t part_count()const{return state_->spaces.size();}
    std::size_t stored_entries()const{return state_->records;}
    const StalkColimitWitness& witness()const &{return state_->witness;}
    const StalkColimitWitness& witness()const &&=delete;
    const M& relations()const &{return state_->relations;}
    const M& relations()const &&=delete;
    const M& injection(std::size_t p)const &{return state_->injections.at(p);}
    const M& injection(std::size_t)const &&=delete;
    S include(std::size_t p,const S& x,G& g)const{return certified_colimit_detail::apply(state_->injections.at(p),x,g,state_->records);}
    S include_transpose(std::size_t p,const S& x,G& g)const{return certified_colimit_detail::apply(state_->injections.at(p),x,g,state_->records,true);}
    S representative(std::size_t p,const S& x,G& g)const{return certified_colimit_detail::apply(state_->representatives.at(p),x,g,state_->records);}
    S representative_transpose(std::size_t p,const S& x,G& g)const{return certified_colimit_detail::apply(state_->representatives.at(p),x,g,state_->records,true);}
    S restrict(std::size_t p,const S& x,G& g)const{return certified_colimit_detail::apply(state_->witness.readouts.at(p),x,g,state_->records);}
    S glue(const std::vector<S>& values,G& g)const{
        using namespace certified_colimit_detail;const auto pin=state_;require(values.size()==pin->spaces.size(),"one total value per local colimit space required");
        std::map<std::size_t,Q> out;std::size_t held=pin->records;
        for(std::size_t p=0;p<values.size();++p){require(values[p].size()==pin->offsets[p+1]-pin->offsets[p],"local colimit field shape");g.sparse_vector(values[p]);held=add(held,values[p].nonzeros());g.entries(held);}
        for(std::size_t p=0;p<values.size();++p){const auto v=apply(pin->observation_parts[p],values[p],g,held);for(const auto& e:v.entries())accumulate(out,e.index,e.value,g,add(held,v.nonzeros()));}
        auto result=numeric::checked_sparse_detail::vector(pin->dimension,std::move(out),g);
        for(std::size_t p=0;p<values.size();++p)require(apply(pin->witness.readouts[p],result,g,held).entries()==values[p].entries(),"local fields do not lie in the declared joint observation image");
        return result;
    }
};
} // namespace rfc::storage
