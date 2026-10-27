#pragma once
#include "rfc/storage/glued_local_fields.hpp"

namespace rfc::storage {
// A complete supplied simultaneous splitting. A block is a typed coordinate
// space, NOT a source identity. Equal block names/types alone never glue it.
struct StalkCoordinateBlock {
    std::string name;
    composite::Declaration declaration;
    bool operator==(const StalkCoordinateBlock&) const = default;
};
struct LocalStalkPartition {
    LocalStalkChart chart;
    std::vector<StalkCoordinateBlock> blocks;
};
struct LocalStalkPartitionRecords {
    LocalStalkChartRecords chart;
    std::vector<StalkCoordinateBlock> blocks;
};
struct StalkBlockLink { std::size_t left{}, left_block{}, right{}, right_block{}; };
struct CoverBlockRange {
    std::size_t part{}, block{}, offset{}, dimension{};
    StalkCoordinateBlock coordinates;
    bool operator==(const CoverBlockRange&) const = default;
};
namespace split_cover_detail {
using namespace local_gluing_detail;
inline std::vector<std::size_t> block_offsets(const LocalStalkPartition& p,Guard& g){
    std::vector<std::size_t> out{0};std::set<std::string> names;
    std::vector<composite::Declaration> definitions{p.chart.local.declaration,p.chart.common_declaration};
    for(const auto& b:p.blocks){
        g.step();require(!b.name.empty() && names.insert(b.name).second,"split block names must be nonempty and unique locally");
        require(b.declaration.kind==composite::DeclarationKind::RationalTensor && b.declaration.axes.size()==1 && b.declaration.axes[0].extent>0,"cover blocks require positive one-axis rational types");
        out.push_back(add(out.back(),b.declaration.axes[0].extent));g.dimension(out.back());definitions.push_back(b.declaration);
    }
    (void)composite::DeclarationRegistry(std::move(definitions));
    const auto n=p.chart.local.carrier.domain.logical_size();
    require(out.back()==n && n>0 && p.chart.common_coordinates!=0,"partition must cover the entire local stalk");
    require(p.chart.common_declaration.kind==composite::DeclarationKind::RationalTensor && p.chart.common_declaration.axes.size()==1 && p.chart.common_declaration.axes[0].extent==n,"partition target declaration differs");
    require(p.chart.to_common.rows()==n && p.chart.to_common.cols()==n && p.chart.from_common.rows()==n && p.chart.from_common.cols()==n,"partition chart dimensions");
    const auto identity_map=StalkChartMap::identity(n);
    require(p.chart.to_common.multiply(p.chart.from_common,g).equals(identity_map,g) && p.chart.from_common.multiply(p.chart.to_common,g).equals(identity_map,g),"partition charts are not two-sided inverses");
    const auto& names_full=p.chart.common_declaration.axes[0].coordinates;
    if(!names_full.empty()){
        std::size_t cursor=0;
        for(const auto& b:p.blocks){
            const auto& names_block=b.declaration.axes[0].coordinates;
            require(names_block.size()==b.declaration.axes[0].extent,"named split coordinates require complete named blocks");
            for(const auto id:names_block){g.step();require(names_full.at(cursor++)==id,"block coordinate order differs from named full split");}
        }
    }
    return out;
}
inline std::size_t block_at(const std::vector<std::size_t>& offsets,std::size_t i){
    require(i<offsets.back(),"block coordinate outside carrier");
    return std::size_t(std::upper_bound(offsets.begin(),offsets.end(),i)-offsets.begin()-1);
}
// E embeds complete common blocks into selected local blocks. This is explicit
// routing in supplied split coordinates, not an inferred subspace intersection.
struct BlockEmbedding {
    std::vector<std::size_t> common,local,images;
    std::size_t image(std::size_t i)const {const auto b=block_at(common,i);return add(local.at(images.at(b)),i-common[b]);}
    std::optional<std::size_t> preimage(std::size_t i)const {
        const auto b=block_at(local,i);const auto at=std::find(images.begin(),images.end(),b);
        if(at==images.end())return {};
        const auto c=std::size_t(at-images.begin());return add(common[c],i-local[b]);
    }
};
// Test L E_upper = E_lower K over the WHOLE carrier. Implicit diagonals
// remain intervals; their length is never an enumeration request. Corrections
// that cancel an explicit interval are checked exactly under the entry budget.
inline bool descends(const StalkChartMap& L,const StalkChartMap& K,
                     const BlockEmbedding& low,const BlockEmbedding& up,Guard& g){
    require(L.rows()==low.local.back() && L.cols()==up.local.back() && K.rows()==low.common.back() && K.cols()==up.common.back(),"cover descent dimensions");
    L.validate(g);K.validate(g);
    using Shift=std::pair<bool,std::size_t>;
    std::map<Shift,std::map<std::size_t,Q>> events;
    std::map<std::pair<std::size_t,std::size_t>,Q> correction;
    const auto held=add(L.stored_entries(),K.stored_entries());std::size_t records=held;
    const auto diagonal=[&](std::size_t row,std::size_t col,std::size_t n,const Q& q){
        g.step();if(q==0 || n==0)return;
        const Shift shift{row>=col,row>=col?row-col:col-row};
        records=add(records,2);g.entries(records);auto& ev=events[shift];
        ev[col]=g.add(ev[col],q);const auto end=add(col,n);ev[end]=g.sub(ev[end],q);
    };
    if(L.identity_coefficient()!=0)for(std::size_t b=0;b+1<up.common.size();++b)
        diagonal(up.local.at(up.images.at(b)),up.common[b],up.common[b+1]-up.common[b],L.identity_coefficient());
    if(K.identity_coefficient()!=0)for(std::size_t b=0;b+1<low.common.size();++b)
        diagonal(low.local.at(low.images.at(b)),low.common[b],low.common[b+1]-low.common[b],-K.identity_coefficient());
    const auto update=[&](std::size_t i,std::size_t j,const Q& q){
        g.entries(add(records,correction.size()+1));const auto key=std::make_pair(i,j);const auto at=correction.find(key);
        auto value=g.add(at==correction.end()?Q{0}:at->second,q);
        if(value==0)correction.erase(key);else correction[key]=std::move(value);
    };
    for(const auto& [ij,q]:L.correction().entries()){g.step();if(const auto j=up.preimage(ij.second))update(ij.first,*j,q);}
    for(const auto& [ij,q]:K.correction().entries()){g.step();update(low.image(ij.first),ij.second,-q);}
    for(const auto& [shift,ev]:events){
        Q value=0;std::size_t previous=0;
        for(const auto& [end,delta]:ev){
            g.step();if(value!=0 && end>previous){
                const auto n=end-previous;if(n>correction.size())return false;
                std::size_t matched=0;
                for(auto it=correction.begin();it!=correction.end();){
                    g.step();const auto row=it->first.first,col=it->first.second;
                    const Shift at{row>=col,row>=col?row-col:col-row};
                    if(at==shift && col>=previous && col<end){if(it->second!=-value)return false;++matched;it=correction.erase(it);}else ++it;
                }
                if(matched!=n)return false;
            }
            value=g.add(value,delta);previous=end;
        }
        require(value==0,"unclosed symbolic diagonal interval");
    }
    return correction.empty();
}
} // namespace split_cover_detail

// Finite colimit of supplied block identifications. All interfaces must be
// simultaneously split by these charts. Cycles may be redundant, but may not
// identify two distinct blocks of one local stalk. This is NOT arbitrary
// non-distributive subspace gluing and never silently quotients a local direction.
class SplitStalkCover {
    using S=numeric::SparseRationalVector;using G=numeric::ExactActionGuard;
    std::vector<LocalStalkPartition> parts_;
    std::vector<std::vector<std::size_t>> offsets_,images_;
    std::vector<CoverBlockRange> blocks_;
    std::size_t dimension_{},records_{};
    S scatter(std::size_t p,const S& x,bool representative,G& g)const {
        using namespace split_cover_detail;require(p<parts_.size() && x.size()==offsets_[p].back(),"cover scatter carrier");g.sparse_vector(x);
        std::vector<numeric::SparseRationalEntry> out;
        for(const auto& e:x.entries()){
            g.step();const auto b=block_at(offsets_[p],e.index);const auto& dst=blocks_[images_[p][b]];
            if(representative && dst.part!=p)continue;
            g.entries(add(records_,add(x.nonzeros(),out.size()+1)));out.push_back({add(dst.offset,e.index-offsets_[p][b]),e.value});
        }
        return S(dimension_,std::move(out));
    }
    S gather(std::size_t p,const S& x,bool representative,G& g)const {
        using namespace split_cover_detail;require(p<parts_.size() && x.size()==dimension_,"cover gather carrier");g.sparse_vector(x);
        std::vector<numeric::SparseRationalEntry> out;
        for(std::size_t b=0;b<images_[p].size();++b){
            g.step();const auto& from=blocks_[images_[p][b]];if(representative && from.part!=p)continue;
            const auto v=part(x,from.offset,from.dimension,g);
            for(const auto& e:v.entries()){g.entries(add(records_,add(x.nonzeros(),out.size()+1)));out.push_back({add(offsets_[p][b],e.index),e.value});}
        }
        return S(offsets_[p].back(),std::move(out));
    }
public:
    SplitStalkCover(std::vector<LocalStalkPartition> parts,std::vector<StalkBlockLink> links,G& g):parts_(std::move(parts)){
        using namespace split_cover_detail;require(!parts_.empty(),"cover needs local parts");
        std::vector<std::size_t> starts{0};records_=add(parts_.size(),links.size());g.entries(records_);
        for(const auto& p:parts_){offsets_.push_back(block_offsets(p,g));starts.push_back(add(starts.back(),p.blocks.size()));records_=add(records_,add(p.blocks.size(),add(p.chart.to_common.stored_entries(),p.chart.from_common.stored_entries())));g.entries(records_);}
        g.entries(add(records_,starts.back()));Union dsu(starts.back());
        for(const auto& l:links){
            g.step();require(l.left<parts_.size() && l.right<parts_.size() && l.left!=l.right,"invalid cover link endpoints");
            require(l.left_block<parts_[l.left].blocks.size() && l.right_block<parts_[l.right].blocks.size(),"invalid cover block selection");
            require(parts_[l.left].blocks[l.left_block].declaration==parts_[l.right].blocks[l.right_block].declaration,"shared block definitions or ordered coordinates differ");
            dsu.join(add(starts[l.left],l.left_block),add(starts[l.right],l.right_block),g);
        }
        std::map<std::size_t,std::size_t> roots;std::set<std::pair<std::size_t,std::size_t>> injective;images_.resize(parts_.size());
        for(std::size_t p=0;p<parts_.size();++p)for(std::size_t b=0;b<parts_[p].blocks.size();++b){
            g.step();const auto r=dsu.root(add(starts[p],b),g);require(injective.emplace(r,p).second,"interface cycle collapses distinct directions in one local stalk");
            const auto [at,fresh]=roots.emplace(r,blocks_.size());images_[p].push_back(at->second);
            if(fresh){const auto n=offsets_[p][b+1]-offsets_[p][b];blocks_.push_back({p,b,dimension_,n,parts_[p].blocks[b]});dimension_=add(dimension_,n);g.dimension(dimension_);}
        }
        records_=machine::checked_size_mul(add(records_,starts.back()+1),8);g.entries(records_);
    }
    std::size_t dimension()const{return dimension_;}
    std::size_t part_count()const{return parts_.size();}
    std::size_t stored_entries()const{return records_;}
    const std::vector<CoverBlockRange>& layout()const &{return blocks_;}
    const std::vector<CoverBlockRange>& layout()const &&=delete;
    S include(std::size_t p,const S& x,G& g)const{return scatter(p,parts_.at(p).chart.to_common.apply(x,g),false,g);}
    S include_transpose(std::size_t p,const S& x,G& g)const{return parts_.at(p).chart.to_common.apply(gather(p,x,false,g),g,true);}
    S restrict(std::size_t p,const S& x,G& g)const{return parts_.at(p).chart.from_common.apply(gather(p,x,false,g),g);}
    S representative(std::size_t p,const S& x,G& g)const{return parts_.at(p).chart.from_common.apply(gather(p,x,true,g),g);}
    S representative_transpose(std::size_t p,const S& x,G& g)const{return scatter(p,parts_.at(p).chart.from_common.apply(x,g,true),true,g);}
    S glue(const std::vector<S>& values,G& g)const{
        using namespace split_cover_detail;require(values.size()==parts_.size(),"one total local value required per cover part");
        std::vector<S> split;std::size_t held=records_;
        for(std::size_t p=0;p<parts_.size();++p){g.sparse_vector(values[p]);held=add(held,values[p].nonzeros());g.entries(held);split.push_back(parts_[p].chart.to_common.apply(values[p],g));held=add(held,split.back().nonzeros());g.entries(held);}
        std::map<std::size_t,Q> out;
        for(std::size_t p=0;p<parts_.size();++p)for(std::size_t b=0;b<images_[p].size();++b){
            g.step();const auto& dst=blocks_[images_[p][b]];const auto v=part(split[p],offsets_[p][b],dst.dimension,g);
            const auto ref=part(split[dst.part],offsets_[dst.part][dst.block],dst.dimension,g);
            require(v.entries()==ref.entries(),"local fields disagree on a cover overlap");
            if(dst.part==p)for(const auto& e:v.entries())accumulate(out,add(dst.offset,e.index),e.value,g,add(held,add(v.nonzeros(),ref.nonzeros())));
        }
        return numeric::checked_sparse_detail::vector(dimension_,std::move(out),g);
    }
};
} // namespace rfc::storage
