#pragma once
// Local benchmark instrumentation only. Does NOT count GMP, aligned allocations,
// mappings, peak live bytes, or OS activity. Ordinary new/delete overrides live
// in the benchmark executable, never in library headers or applications.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <utility>
namespace product_allocations {
inline thread_local bool active=false;
inline thread_local std::size_t count=0,bytes=0;
}
namespace rfc_temporal_product {
class Measurements {
public:
    Measurements(std::ostream& out,std::size_t n,std::size_t trial):out_(out),n_(n),trial_(trial){}
    template<class F> auto run(const char* operation,std::size_t step,F&& f) {
        using Clock=std::chrono::steady_clock;
        product_allocations::count=product_allocations::bytes=0;
        product_allocations::active=true;const auto start=Clock::now();
        try {
            auto result=f();
            const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
            product_allocations::active=false;
            out_<<n_<<','<<trial_<<','<<step<<','<<operation<<','<<ns<<','<<product_allocations::count<<','<<product_allocations::bytes<<'\n';
            return result;
        } catch(...) {product_allocations::active=false;throw;}
    }
private:
    std::ostream& out_;std::size_t n_,trial_;
};
}
