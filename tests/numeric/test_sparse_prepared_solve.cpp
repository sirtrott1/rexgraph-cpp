#include "rfc/numeric/checked_exact_actions.hpp"
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

using namespace rfc::numeric;
namespace {
std::size_t checks{};
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F fn,const char* why){bool hit=false;try{fn();}catch(const std::exception&){hit=true;}check(hit,why);}
SparseRationalVector sparse(const RationalVector& x){std::vector<SparseRationalEntry> out;for(std::size_t i=0;i<x.size();++i)if(x[i]!=0)out.push_back({i,x[i]});return SparseRationalVector(x.size(),std::move(out));}
}
int main(){try{
    for(std::size_t n=1;n<=9;++n)for(unsigned pattern=0;pattern<4;++pattern){
        auto lower=ExactMatrix::identity(n),upper=ExactMatrix::identity(n);
        for(std::size_t i=0;i<n;++i){
            upper.set(i,i,Rational{static_cast<long>(i+2),static_cast<long>(i+1)});
            for(std::size_t j=0;j<i;++j){
                if((i+j+pattern)%3==0)lower.set(i,j,Rational{static_cast<long>(i+pattern)-2,static_cast<long>(j+2)});
                if((2*i+j+pattern)%3==0)upper.set(j,i,Rational{static_cast<long>(j+pattern)-1,static_cast<long>(i+1)});
            }
        }
        const auto product=lower.multiply(upper);ExactMatrix a(n,n);
        for(const auto& [key,q]:product.entries())a.set((key.first+pattern)%n,n-1-key.second,q);
        ExactActionGuard guard;const PreparedExactSolve solve(a,guard);
        ExactSolveOrdering order;order.rows.resize(n);order.columns.resize(n);
        for(std::size_t i=0;i<n;++i){order.rows[i]=n-1-i;order.columns[i]=(i+1)%n;}
        const auto ordered=PreparedExactSolve::prepare_ordered(a,order,guard);
        const auto selected=solve.prepare_selected({n-1,0,n-1},guard);
        for(std::size_t i=0;i<n;++i){
            RationalVector dense(n);dense[i]=Rational{3,2};if(i+1<n)dense[i+1]=Rational{-7,3};
            const auto x=sparse(dense);
            check(checked_apply(a,x,guard).dense()==a.apply(dense),"checked sparse action matches original map");
            check(checked_transpose_apply(a,x,guard).dense()==a.transpose().apply(dense),"checked sparse transpose matches original map");
            const auto expected=a.solve(dense),transpose=a.transpose().solve(dense);
            check(solve.apply(x,guard).dense()==expected,"sparse prepared solve matches independent exact elimination");
            check(solve.apply_transpose(x,guard).dense()==transpose,"sparse transpose solve reverses actual elimination and row swaps");
            check(ordered.apply(x,guard).dense()==expected,"sparse solve preserves declared row and column ordering");
            check(ordered.apply_transpose(x,guard).dense()==transpose,"sparse transpose preserves declared orderings");
            check(selected.apply(x,guard)==RationalVector{expected[n-1],expected[0],expected[n-1]},"selected sparse solve preserves repeated coordinate requests");
        }
        check(solve.apply(SparseRationalVector(n),guard).empty(),"zero sparse solve remains implicit");
        check(solve.apply_transpose(SparseRationalVector(n),guard).empty(),"zero sparse transpose remains implicit");
        std::vector<std::size_t> outputs(n);std::iota(outputs.begin(),outputs.end(),0);
        const auto full=solve.prepare_selected(outputs,guard);
        check(full.uses_full_solve(),"full requested output reuses primal factors rather than constructing inverse");
        const auto unit=SparseRationalVector::basis(n,n-1);
        check(full.apply(unit,guard)==a.solve(unit.dense()),"explicit full selected readout agrees");
    }
    const PreparedExactSolve empty(ExactMatrix(0,0));
    check(empty.apply(SparseRationalVector(0)).empty() && empty.apply_transpose(SparseRationalVector(0)).empty(),"zero-carrier sparse systems");
    check(empty.apply({}).empty() && empty.apply_transpose({}).empty(),"empty initializer lists retain the existing complete-vector API");
    const PreparedExactSolve scalar(ExactMatrix::diagonal(RationalVector{2}));
    check(scalar.apply({1})==RationalVector{Rational{1,2}} && scalar.apply_transpose({1})==RationalVector{Rational{1,2}},"single-coordinate initializer lists are values rather than sparse carrier dimensions");
    check(scalar.prepare_selected({0}).apply({1})==RationalVector{Rational{1,2}},"selected solve preserves initializer-list source compatibility");
    const auto repeated=scalar.prepare_selected(std::vector<std::size_t>(100,0));
    rejects([&]{ExactActionLimits limits;limits.dimension=100;limits.entries=10;
        (void)repeated.apply(SparseRationalVector::basis(1,0),limits);},
        "sparse input cannot bypass the entry budget of a repeated complete readout");
    const auto diagonal=ExactMatrix::diagonal(RationalVector(1000,Rational{2}));
    ExactActionGuard guard;const PreparedExactSolve retained(diagonal,guard);
    const auto single=retained.apply(SparseRationalVector::basis(1000,987,3),guard);
    check(single.nonzeros()==1 && single.entries()[0].value==Rational{3,2},"large diagonal solve stores one result coordinate");
    if(std::numeric_limits<std::size_t>::digits>34){
        const auto n=std::size_t{1}<<34;ExactMatrix a(n,n);a.set(n-1,n-3,Rational{2,5});
        ExactActionLimits limits;limits.dimension=n;limits.entries=16;limits.steps=40;ExactActionGuard limited(limits);
        const auto out=checked_apply(a,SparseRationalVector::basis(n,n-3,3),limited);
        check(out.nonzeros()==1 && out.entries()[0]==SparseRationalEntry{n-1,Rational{6,5}},"sparse rectangular action never allocates the declared zero coordinates");
        check(checked_transpose_apply(a,out,limited).entries()==std::vector<SparseRationalEntry>{{n-3,Rational{12,25}}},"huge sparse transpose action");
        rejects([&]{(void)PreparedExactSolve(a,limits);},
            "impossible huge sparse square solve is rejected before coordinate index allocation");
    }
    rejects([&]{(void)retained.apply(SparseRationalVector(1),guard);},"sparse RHS carrier mismatch rejects");
    rejects([&]{auto limits=ExactActionLimits{};limits.integer_bits=1;(void)retained.apply(SparseRationalVector::basis(1000,1,3),limits);},"current bit budget applies to retained factors and RHS");
    rejects([&]{(void)retained.apply(SparseRationalVector(1000),ExactActionLimits{},[]{throw std::runtime_error("cancel");});},"sparse solve cancellation remains active");
    std::cout<<"sparse prepared solve: "<<checks<<" checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
