#include "rfc/numeric/prepared_exact_frames.hpp"
#include <iostream>
#include <limits>
using namespace rfc::numeric;
namespace {
std::size_t checks{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void rejects(F f,const char* reason){bool hit=false;try{f();}catch(const std::exception&){hit=true;}check(hit,reason);}
}
int main(){try{
    for(std::size_t rows=0;rows<9;++rows)for(std::size_t cols=0;cols<8;++cols)for(unsigned pattern=0;pattern<3;++pattern){
        ExactMatrix a(rows,cols);
        for(std::size_t i=0;i<rows;++i)for(std::size_t j=0;j<cols;++j)
            if((i+2*j+pattern)%4==0)a.set(i,j,Rational{static_cast<long>((i+1)*(j+3)%7)-3,static_cast<long>(j+1)});
        std::vector<std::size_t> pivots;const auto expected=a.rref(pivots);
        const PreparedExactFrames frames(a);
        check(frames.original()==a && frames.pivots()==pivots,"source map and deterministic pivot columns remain exact");
        check(frames.reduced()==expected,"support-row reduction equals independent complete exact RREF");
        check(frames.image().multiply(frames.coordinates())==a,"original map factors through exact image coordinates");
        check(frames.kernel()==a.kernel_frame() && a.multiply(frames.kernel()).nonzeros()==0,"complete free family equals original nullspace");
    }
    if(std::numeric_limits<std::size_t>::digits>35){
        const auto n=std::size_t{1}<<35;ExactMatrix a(n,3);
        a.set(n-1,0,2);a.set(n-1,2,6);a.set(n-12,1,Rational{1,7});
        ExactActionLimits limits;limits.dimension=n;limits.entries=80;limits.steps=500;
        ExactActionGuard guard(limits);const PreparedExactFrames frames(a,guard);
        check(frames.rank()==2 && frames.image().rows()==n && frames.original().rows()==n,"huge original row carrier is retained without full row scratch");
        check(frames.kernel().rows()==3 && frames.kernel().cols()==1 && frames.kernel().get(0,0)==-3 && frames.kernel().get(2,0)==1,"huge carrier retains exact free response direction");
        check(frames.image().multiply(frames.coordinates())==a && frames.preparation_steps()<500,"huge support-row work stays bounded by represented data");
        rejects([&]{ExactMatrix wide(1,n);(void)PreparedExactFrames(wide,limits);},"explicit complete nullspace request must admit its full free-column family before allocation");
    }
    rejects([]{PreparedExactFrames x(ExactMatrix(1,1),{},[]{throw std::runtime_error("cancel");});},"frame preparation cancellation remains active");
    std::cout<<checks<<" sparse exact frame checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
