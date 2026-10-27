#pragma once
#include "rfc/storage/local_stalk_chart.hpp"
#include <array>

namespace rfc::storage {
// A supplied SPLITTING, not an inferred orthogonal complement. to_split maps
// V_p -> K + U_p and from_split maps back. The injection K -> V_p is
// from_split restricted to the leading r coordinates. Both full inverse
// identities certify injectivity and preserve every unshared direction.
// The pushout coordinates are (shared K, left U, right U). No quotient solve
// or ambient identity enumeration is required when these witnesses are supplied.
class SplitStalkPushout {
    using Q=numeric::Rational;using S=numeric::SparseRationalVector;using G=numeric::ExactActionGuard;
    std::size_t shared_,size_;
    std::array<StalkChartMap,2> to_,from_;
    static void require(bool b,const char* m){if(!b)throw std::invalid_argument(m);}
    std::size_t image(std::size_t p,std::size_t i)const {
        return i<shared_ || p==0?i:machine::checked_size_add(to_[0].cols()-shared_,i);
    }
    // Coordinate selection is bounded by represented entries, not by extent.
    S gather(std::size_t p,const S& x,bool representative,G& g)const {
        require(p<2 && x.size()==size_,"pushout gather carrier");g.sparse_vector(x);
        std::vector<numeric::SparseRationalEntry> out;
        const auto begin=p==0?shared_:to_[0].cols();
        const auto end=machine::checked_size_add(begin,to_[p].cols()-shared_);
        for(const auto& e:x.entries()){
            g.step();std::optional<std::size_t> i;
            if(e.index<shared_ && !(representative && p==1))i=e.index;
            else if(e.index>=begin && e.index<end)i=shared_+e.index-begin;
            if(i){g.entries(machine::checked_size_add(stored_entries(),machine::checked_size_add(x.nonzeros(),out.size()+1)));out.push_back({*i,e.value});}
        }
        return S(to_[p].cols(),std::move(out));
    }
    S scatter(std::size_t p,const S& x,bool representative,G& g)const {
        require(p<2 && x.size()==to_[p].cols(),"pushout scatter carrier");g.sparse_vector(x);
        std::vector<numeric::SparseRationalEntry> out;
        for(const auto& e:x.entries()){
            g.step();if(representative && p==1 && e.index<shared_)continue;
            g.entries(machine::checked_size_add(stored_entries(),machine::checked_size_add(x.nonzeros(),out.size()+1)));
            out.push_back({image(p,e.index),e.value});
        }
        return S(size_,std::move(out));
    }
public:
    SplitStalkPushout(std::size_t shared,std::array<StalkChartMap,2> to_split,
                     std::array<StalkChartMap,2> from_split,G& g)
        :shared_(shared),size_(0),to_(to_split),from_(from_split){
        require(shared_>0,"split interface has a positive declared common stalk");
        for(std::size_t p=0;p<2;++p){
            const auto n=to_[p].cols();require(n>=shared_ && to_[p].rows()==n && from_[p].rows()==n && from_[p].cols()==n,"split witness dimensions");
            const auto id=StalkChartMap::identity(n);
            require(to_[p].multiply(from_[p],g).equals(id,g) && from_[p].multiply(to_[p],g).equals(id,g),"split witnesses are not two-sided inverses");
        }
        size_=machine::checked_size_add(to_[0].cols(),to_[1].cols()-shared_);g.dimension(size_);g.entries(stored_entries());
    }
    std::size_t dimension()const{return size_;}
    std::size_t common_dimension()const{return shared_;}
    std::size_t local_dimension(std::size_t p)const{return to_.at(p).cols();}
    std::size_t stored_entries()const {std::size_t n=1;for(std::size_t p=0;p<2;++p){n=machine::checked_size_add(n,to_[p].stored_entries());n=machine::checked_size_add(n,from_[p].stored_entries());}return n;}
    const StalkChartMap& to_split(std::size_t p)const & {return to_.at(p);}
    const StalkChartMap& from_split(std::size_t p)const & {return from_.at(p);}
    const StalkChartMap& to_split(std::size_t)const &&=delete;
    const StalkChartMap& from_split(std::size_t)const &&=delete;
    // Chain injection j_p. Adding two such chains is a physical/algebraic sum,
    // NOT the gluing of two descriptions of the same shared value.
    S include(std::size_t p,const S& x,G& g)const {return scatter(p,to_.at(p).apply(x,g),false,g);}
    S include_transpose(std::size_t p,const S& x,G& g)const {return to_.at(p).apply(gather(p,x,false,g),g,true);}
    // Chosen observation restriction rho_p; depends on the supplied splitting.
    S restrict(std::size_t p,const S& x,G& g)const {return from_.at(p).apply(gather(p,x,false,g),g);}
    // A right inverse h of the quotient addition: shared K assigned to left,
    // right contributes only U_right. sum j_p h_p = identity on the pushout.
    S representative(std::size_t p,const S& x,G& g)const {return from_.at(p).apply(gather(p,x,true,g),g);}
    S representative_transpose(std::size_t p,const S& x,G& g)const {return scatter(p,from_.at(p).apply(x,g,true),true,g);}
    S glue(const S& left,const S& right,G& g)const {
        const auto a=to_[0].apply(left,g),b=to_[1].apply(right,g);
        auto ai=a.entries().begin(),bi=b.entries().begin();
        while((ai!=a.entries().end() && ai->index<shared_) || (bi!=b.entries().end() && bi->index<shared_)){
            g.step();require(ai!=a.entries().end() && bi!=b.entries().end() && ai->index==bi->index && ai->value==bi->value,"local fields disagree in common split coordinates");++ai;++bi;
        }
        const auto x=scatter(0,a,false,g),y=scatter(1,b,true,g);
        std::vector<numeric::SparseRationalEntry> out=x.entries();
        g.entries(machine::checked_size_add(stored_entries(),machine::checked_size_mul(machine::checked_size_add(a.nonzeros(),b.nonzeros()),4)));
        out.insert(out.end(),y.entries().begin(),y.entries().end());return S(size_,std::move(out));
    }
    // Full equation T_local i_upper = i_lower T_common, checked in split
    // coordinates. Sparse entries outside the common block are checked too.
    // Scalar identity differences are proved symbolically, including huge K.
    static bool descends(const StalkChartMap& split_map,const StalkChartMap& common,
                         std::size_t lower_common,std::size_t upper_common,G& g){
        require(common.rows()==lower_common && common.cols()==upper_common &&
                split_map.rows()>=lower_common && split_map.cols()>=upper_common,"descent carriers");
        split_map.validate(g);common.validate(g);numeric::ExactMatrix diff(split_map.rows(),upper_common);
        const auto held=machine::checked_size_add(split_map.stored_entries(),common.stored_entries());
        for(const auto& [ij,q]:split_map.correction().entries()){
            g.step();if(ij.second<upper_common){g.entries(machine::checked_size_add(held,diff.nonzeros()+1));diff.set(ij.first,ij.second,q);}
        }
        for(const auto& [ij,q]:common.correction().entries()){
            g.entries(machine::checked_size_add(held,diff.nonzeros()+1));diff.set(ij.first,ij.second,g.sub(diff.get(ij.first,ij.second),q));
        }
        const auto scalar=g.sub(split_map.identity_coefficient(),common.identity_coefficient());
        if(scalar==0)return diff.nonzeros()==0;
        if(diff.nonzeros()!=upper_common)return false;
        for(const auto& [ij,q]:diff.entries()){g.step();if(ij.first!=ij.second || q!=-scalar)return false;}
        return true;
    }
};
} // namespace rfc::storage
