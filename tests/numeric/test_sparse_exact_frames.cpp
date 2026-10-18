#include "rfc/numeric/prepared_exact_frames.hpp"
#include <iostream>
#include <limits>
#include <future>
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
    std::uint32_t state=37;
    const auto next=[&](){state=state*1664525u+1013904223u;return state;};
    for(std::size_t trial=0;trial<160;++trial){
        const auto rows=1+next()%24,cols=1+next()%29;ExactMatrix a(rows,cols);
        for(std::size_t i=0;i<rows;++i)for(std::size_t j=0;j<cols;++j){
            if(next()%5==0)a.set(i,j,Rational{static_cast<long>(next()%15)-7,static_cast<long>(1+next()%9)});
        }
        std::vector<std::size_t> pivots;const auto expected=a.rref(pivots);const PreparedExactFrames frames(a);
        check(frames.pivots()==pivots && frames.reduced()==expected,"indexed elimination preserves exact reference pivots and RREF under swaps and fill");
        check(frames.kernel()==a.kernel_frame(),"indexed kernel preserves every canonical free coordinate");
        check(frames.image().multiply(frames.coordinates())==a,"indexed image factorization retains original rows");
    }
    {
        constexpr std::size_t n=12000;ExactMatrix a(2*n,3*n),expected(3*n,n);
        for(std::size_t i=0;i<n;++i){
            a.set(i,i,1);a.set(i,n+i,-1);a.set(n+i,i,1);a.set(n+i,2*n+i,-1);
            for(std::size_t member=0;member<3;++member)expected.set(member*n+i,i,1);
        }
        ExactActionLimits limits{3*n,1000000,3000000,16384};const PreparedExactFrames frames(a,limits);
        check(frames.rank()==2*n && frames.kernel()==expected,"complete sparse family agreement kernel retains all common directions");
        check(frames.preparation_steps()<200*n,"sparse family reduction avoids scans across unrelated rows and free coefficients");
        auto task=[frames,limits]{ExactActionGuard guard(limits);frames.validate(guard);return frames.kernel().nonzeros();};
        auto first=std::async(std::launch::async,task),second=std::async(std::launch::async,task);
        check(first.get()==3*n && second.get()==3*n,"immutable frame owner supports concurrent validation");
        a.set(0,0,2);check(frames.original().get(0,0)==1,"caller changes do not alter owned frame source");
        auto tiny=limits;tiny.entries=3*n-1;rejects([&]{PreparedExactFrames refused(a,tiny);},"complete free-axis and sparse index resource admission remains enforced");
        std::size_t checkpoints{};rejects([&]{PreparedExactFrames stopped(a,limits,[&]{if(++checkpoints==100000)throw std::runtime_error("stop");});},"sparse index construction and elimination remain cancellable");
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
