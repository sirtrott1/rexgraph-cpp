#pragma once

#include "rfc/numeric/prepared_exact_frames.hpp"
#include <map>
#include <memory>

namespace rfc::topology {
struct RationalZigzagArrow {
    bool forward{true};
    numeric::ExactMatrix map;
};
struct RationalInterval {
    std::size_t begin{}, end{};
    // One matrix per supported vertex, with one column per copy of the interval.
    std::vector<numeric::ExactMatrix> generators;
};
namespace zigzag_detail {
using Matrix = numeric::ExactMatrix;
using Guard = numeric::ExactActionGuard;
using Frames = numeric::PreparedExactFrames;
inline std::size_t add(std::size_t a, std::size_t b) { return machine::checked_size_add(a,b); }
inline void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
inline Matrix columns(const Matrix& a, const std::vector<std::size_t>& selected, Guard& guard) {
    guard.dimension(selected.size()); Matrix out(a.rows(),selected.size());
    std::map<std::size_t,std::size_t> positions;
    for (std::size_t j=0;j<selected.size();++j) {
        guard.step();require(selected[j]<a.cols() && positions.emplace(selected[j],j).second,"zigzag column selection");
    }
    guard.entries(positions.size());
    for(const auto& [ij,q]:a.entries()){guard.step();const auto p=positions.find(ij.second);
        if(p!=positions.end()){guard.entries(add(positions.size(),add(out.nonzeros(),1)));out.set(ij.first,p->second,q);}}
    return out;
}
inline Matrix block(const Matrix& a, std::size_t row, std::size_t nr, std::size_t col, std::size_t nc, Guard& guard) {
    require(row<=a.rows() && nr<=a.rows()-row && col<=a.cols() && nc<=a.cols()-col,"zigzag matrix block");
    Matrix out(nr,nc);
    for (const auto& [ij,q]:a.entries()) { guard.step(); if(ij.first>=row && ij.first-row<nr && ij.second>=col && ij.second-col<nc)
        out.set(ij.first-row,ij.second-col,q); }
    return out;
}
inline Matrix join(const Matrix& a,const Matrix& b,Guard& guard) {
    require(a.rows()==b.rows(),"zigzag frame rows"); const auto n=add(a.cols(),b.cols());guard.dimension(n);
    guard.entries(add(a.nonzeros(),b.nonzeros()));Matrix out(a.rows(),n);
    for(const auto& [ij,q]:a.entries()){guard.step();out.set(ij.first,ij.second,q);}
    for(const auto& [ij,q]:b.entries()){guard.step();out.set(ij.first,add(a.cols(),ij.second),q);}
    return out;
}
inline Matrix solve_columns(const numeric::PreparedExactSolve& solve,const Matrix& rhs,Guard& guard) {
    Matrix out(rhs.rows(),rhs.cols());
    std::map<std::size_t,std::vector<numeric::SparseRationalEntry>> columns;
    for(const auto& [ij,q]:rhs.entries()){guard.step();columns[ij.second].push_back({ij.first,q});}
    guard.entries(machine::checked_size_mul(rhs.nonzeros(),2));
    for(auto& [j,entries]:columns){
        const auto x=solve.apply(numeric::SparseRationalVector(rhs.rows(),std::move(entries)),guard);
        for(const auto& e:x.entries()){guard.step();guard.entries(add(out.nonzeros(),1));out.set(e.index,j,e.value);}
    }
    return out;
}
// Algebraic coordinates in an injective frame. The temporary Gram is only an
// exact coordinate solve; it does not declare a physical metric on either carrier.
inline Matrix coordinates(const Matrix& frame,const Matrix& values,Guard& guard) {
    require(frame.rows()==values.rows(),"zigzag coordinate carrier");
    if(!frame.cols()){require(!values.nonzeros(),"zigzag value outside zero frame");return Matrix(0,values.cols());}
    const auto dual=frame.transpose();
    numeric::PreparedExactSolve solve(numeric::checked_multiply(dual,frame,guard),guard);
    auto out=solve_columns(solve,numeric::checked_multiply(dual,values,guard),guard);
    require(numeric::checked_multiply(frame,out,guard)==values,"zigzag value outside declared frame");return out;
}
inline Matrix extend(const Matrix& initial,std::size_t dimension,Guard& guard) {
    guard.dimension(dimension);guard.entries(dimension);
    if(initial.cols()==dimension)return initial;
    Frames all(join(initial,Matrix::identity(dimension),guard),guard);
    require(all.rank()==dimension,"zigzag incomplete coordinate frame");return all.image();
}
using Multiplicities=std::map<std::pair<std::size_t,std::size_t>,std::size_t>;
// Right filtration: forward images preserve birth order; backward preimages
// insert the new kernel first. Only the current filtration frame is retained.
inline Multiplicities intervals(const std::vector<std::size_t>& dimensions,
    const std::vector<RationalZigzagArrow>& arrows,Guard& guard) {
    Multiplicities result;Matrix basis=Matrix::identity(dimensions.front());
    std::vector<std::size_t> births(dimensions.front(),0);
    for(std::size_t k=0;k<arrows.size();++k){
        guard.step();const auto& arrow=arrows[k];std::vector<std::size_t> next;
        Matrix new_basis(dimensions[k+1],0);
        if(arrow.forward){
            Frames image(numeric::checked_multiply(arrow.map,basis,guard),guard);
            for(auto j:image.pivots())next.push_back(births[j]);
            new_basis=extend(image.image(),dimensions[k+1],guard);
            next.resize(dimensions[k+1],k+1);
        }else{
            const auto map=coordinates(basis,arrow.map,guard);
            Frames whole(map,guard);new_basis=whole.kernel();next.resize(new_basis.cols(),k+1);
            // A suffix kernel is the exact preimage of the corresponding flag
            // prefix. Equal birth blocks are processed together, not per scalar.
            for(std::size_t p=0;p<births.size();){
                std::size_t end=p+1;while(end<births.size() && births[end]==births[p])++end;
                Frames preimage(block(map,end,map.rows()-end,0,map.cols(),guard),guard);
                Frames combined(join(new_basis,preimage.kernel(),guard),guard);
                const auto old=new_basis.cols();new_basis=combined.image();
                for(std::size_t j=old;j<new_basis.cols();++j)next.push_back(births[p]);
                p=end;
            }
            require(new_basis.cols()==dimensions[k+1],"zigzag preimage flag incomplete");
        }
        std::map<std::size_t,std::size_t> before,after;
        for(auto b:births)++before[b];
        for(auto b:next)if(b!=k+1)++after[b];
        for(const auto& [birth,count]:before){require(after[birth]<=count,"zigzag filtration grew an old quotient");
            if(count>after[birth])result[{birth,k}]=count-after[birth];}
        basis=std::move(new_basis);births=std::move(next);
    }
    for(auto birth:births)++result[{birth,dimensions.size()-1}];
    return result;
}
struct HomFrame { std::vector<std::size_t> offsets;Matrix values; };
// Sparse commutativity equations for Hom(I,V) or Hom(V,I). Only an interval
// already found by the right filtration is solved, and all copies split at once.
inline HomFrame hom(std::size_t begin,std::size_t end,const std::vector<std::size_t>& dimensions,
    const std::vector<RationalZigzagArrow>& arrows,bool into,Guard& guard) {
    std::vector<std::size_t> offsets(dimensions.size());std::size_t variables=0,equations=0;
    for(std::size_t i=begin;i<=end;++i){offsets[i]=variables;variables=add(variables,dimensions[i]);}
    for(std::size_t k=0;k<arrows.size();++k){const auto s=arrows[k].forward?k:k+1,t=arrows[k].forward?k+1:k;
        if(into?(s>=begin && s<=end):(t>=begin && t<=end))equations=add(equations,dimensions[into?t:s]);}
    guard.dimension(variables);guard.dimension(equations);guard.entries(add(offsets.size(),variables));
    Matrix constraints(equations,variables);std::size_t row=0;
    const auto put=[&](std::size_t i,std::size_t j,const numeric::Rational& q){
        guard.step();auto value=guard.add(constraints.get(i,j),q);guard.entries(add(constraints.nonzeros(),value!=0?1:0));constraints.set(i,j,value);};
    for(std::size_t k=0;k<arrows.size();++k){
        const auto s=arrows[k].forward?k:k+1,t=arrows[k].forward?k+1:k;
        const bool si=s>=begin && s<=end,ti=t>=begin && t<=end;
        if(into && si){
            for(const auto& [ij,q]:arrows[k].map.entries())put(add(row,ij.first),add(offsets[s],ij.second),q);
            if(ti)for(std::size_t i=0;i<dimensions[t];++i)put(add(row,i),add(offsets[t],i),-1);
            row=add(row,dimensions[t]);
        }else if(!into && ti){
            for(const auto& [ij,q]:arrows[k].map.entries())put(add(row,ij.second),add(offsets[t],ij.first),q);
            if(si)for(std::size_t i=0;i<dimensions[s];++i)put(add(row,i),add(offsets[s],i),-1);
            row=add(row,dimensions[s]);
        }
    }
    Frames frame(std::move(constraints),guard);return {std::move(offsets),frame.kernel()};
}
} // namespace zigzag_detail

class PreparedRationalZigzag {
    struct State {std::vector<std::size_t> dimensions;std::vector<RationalZigzagArrow> arrows;std::vector<RationalInterval> intervals;};
    std::shared_ptr<const State> state_;
public:
    // Explicit matrices have complete rectangular support, including zero coefficients.
    // This vector-space provider infers no source/carrier identity or chain map.
    PreparedRationalZigzag(std::vector<std::size_t> dimensions,std::vector<RationalZigzagArrow> arrows,
        numeric::ExactActionGuard& guard) {
        using namespace zigzag_detail;
        guard.step();
        require(!dimensions.empty() && arrows.size()==dimensions.size()-1,"zigzag requires consecutive oriented arrows");
        guard.dimension(dimensions.size());for(auto n:dimensions){guard.dimension(n);guard.entries(n);}
        std::size_t retained=0;
        for(std::size_t k=0;k<arrows.size();++k){const auto& a=arrows[k];guard.matrix(a.map);
            require(a.map.cols()==dimensions[a.forward?k:k+1] && a.map.rows()==dimensions[a.forward?k+1:k],"zigzag arrow shape");
            retained=add(retained,a.map.nonzeros());guard.entries(retained);}
        auto state=std::make_shared<State>();state->dimensions=std::move(dimensions);state->arrows=std::move(arrows);
        if(state->dimensions.size()==1){const auto n=state->dimensions.front();
            if(n)state->intervals.push_back({0,0,{Matrix::identity(n)}});
            state_=std::move(state);validate(guard);return;}
        auto dims=state->dimensions;auto residual=state->arrows;
        std::vector<Matrix> lifts;for(auto n:dims)lifts.push_back(Matrix::identity(n));
        const auto counts=zigzag_detail::intervals(dims,residual,guard);
        for(const auto& [interval,copies]:counts){
            guard.step();const auto [begin,end]=interval;
            const auto f=hom(begin,end,dims,residual,true,guard),g=hom(begin,end,dims,residual,false,guard);
            const auto fa=block(f.values,f.offsets[begin],dims[begin],0,f.values.cols(),guard);
            const auto ga=block(g.values,g.offsets[begin],dims[begin],0,g.values.cols(),guard).transpose();
            const auto pairing=numeric::checked_multiply(ga,fa,guard);Frames paired(pairing,guard);
            require(paired.rank()==copies,"zigzag interval multiplicity certificate failed");
            const auto chosen=columns(f.values,paired.pivots(),guard);
            const auto selected_pair=columns(pairing,paired.pivots(),guard);
            const auto transpose=selected_pair.transpose();
            numeric::PreparedExactSolve pair_solve(numeric::checked_multiply(transpose,selected_pair,guard),guard);
            const auto dual=solve_columns(pair_solve,transpose,guard);
            RationalInterval output{begin,end,{}};
            std::vector<Matrix> complement;complement.reserve(dims.size());
            for(std::size_t i=0;i<dims.size();++i){
                if(i<begin || i>end){complement.push_back(Matrix::identity(dims[i]));continue;}
                const auto inclusion=block(chosen,f.offsets[i],dims[i],0,copies,guard);
                const auto projection=numeric::checked_multiply(dual,block(g.values,g.offsets[i],dims[i],0,g.values.cols(),guard).transpose(),guard);
                require(numeric::checked_multiply(projection,inclusion,guard)==Matrix::identity(copies),"zigzag retraction certificate");
                output.generators.push_back(numeric::checked_multiply(lifts[i],inclusion,guard));
                Frames kernel(projection,guard);complement.push_back(kernel.kernel());
            }
            std::vector<RationalZigzagArrow> next;next.reserve(residual.size());
            for(std::size_t k=0;k<residual.size();++k){const auto& a=residual[k];const auto s=a.forward?k:k+1,t=a.forward?k+1:k;
                next.push_back({a.forward,coordinates(complement[t],numeric::checked_multiply(a.map,complement[s],guard),guard)});}
            for(std::size_t i=0;i<dims.size();++i){lifts[i]=numeric::checked_multiply(lifts[i],complement[i],guard);dims[i]=complement[i].cols();}
            residual=std::move(next);state->intervals.push_back(std::move(output));
            std::size_t used=retained;
            for(const auto& a:residual)used=add(used,a.map.nonzeros());
            for(const auto& a:lifts)used=add(used,a.nonzeros());
            for(const auto& q:state->intervals)for(const auto& a:q.generators)used=add(used,a.nonzeros());
            guard.entries(used);
        }
        for(auto n:dims)require(n==0,"zigzag decomposition incomplete");
        state_=std::move(state);validate(guard);
    }
    const std::vector<std::size_t>& dimensions() const & {return state_->dimensions;}
    const std::vector<RationalZigzagArrow>& arrows() const & {return state_->arrows;}
    const std::vector<RationalInterval>& intervals() const & {return state_->intervals;}
    const std::vector<std::size_t>& dimensions() const &&=delete;
    const std::vector<RationalZigzagArrow>& arrows() const &&=delete;
    const std::vector<RationalInterval>& intervals() const &&=delete;
    void validate(numeric::ExactActionGuard& guard) const {
        using namespace zigzag_detail;std::vector<Matrix> frames;std::vector<std::vector<std::size_t>> ids;
        for(auto n:state_->dimensions){frames.emplace_back(n,0);ids.emplace_back();}
        std::size_t id=0;
        for(const auto& interval:state_->intervals){const auto copies=interval.generators.front().cols();
            for(std::size_t i=interval.begin;i<=interval.end;++i){frames[i]=join(frames[i],interval.generators[i-interval.begin],guard);
                for(std::size_t j=0;j<copies;++j)ids[i].push_back(add(id,j));}id=add(id,copies);}
        for(std::size_t i=0;i<frames.size();++i){
            require(frames[i].cols()==state_->dimensions[i],"zigzag full generator basis shape");
            const numeric::PreparedExactSolve full(frames[i],guard);(void)full;}
        for(std::size_t k=0;k<state_->arrows.size();++k){const auto& a=state_->arrows[k];const auto s=a.forward?k:k+1,t=a.forward?k+1:k;
            Matrix d(ids[t].size(),ids[s].size());std::map<std::size_t,std::size_t> rows;
            for(std::size_t i=0;i<ids[t].size();++i)rows[ids[t][i]]=i;
            for(std::size_t j=0;j<ids[s].size();++j){guard.step();const auto found=rows.find(ids[s][j]);if(found!=rows.end())d.set(found->second,j,1);}
            require(numeric::checked_multiply(a.map,frames[s],guard)==numeric::checked_multiply(frames[t],d,guard),"zigzag original arrow reconstruction failed");}
    }
};
} // namespace rfc::topology
