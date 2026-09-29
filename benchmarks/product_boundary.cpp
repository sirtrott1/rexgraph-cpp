#include "product_boundary_fixture.hpp"
#include "product_allocation_overrides.hpp"
#include <charconv>
int main(int argc,char** argv) {
    try {
        if(argc<2 || argc>3)throw std::invalid_argument("usage: rfc_product_boundary NEW_DIRECTORY [ASSERTIONS=64 (3..256)]");
        std::size_t n=64;
        if(argc==3){const std::string_view text(argv[2]);const auto [p,ec]=std::from_chars(text.data(),text.data()+text.size(),n);if(ec!=std::errc{} || p!=text.data()+text.size() || n<3 || n>256)throw std::invalid_argument("assertion count 3..256");}
        const std::filesystem::path out(argv[1]);if(!std::filesystem::create_directory(out))throw std::invalid_argument("fresh directory required");
        std::ofstream timings(out/"timings.csv");timings.exceptions(std::ios::badbit|std::ios::failbit);
        timings<<"assertions,trial,step,operation,nanoseconds,cpp_allocations,cpp_requested_bytes\n";
        rfc_temporal_product::Measurements time(timings,n,0);rfc_product_boundary::Verify verify;
        rfc_product_boundary::run(n,out,verify,[&](const char* op,std::size_t step,auto&& f){return time.run(op,step,std::forward<decltype(f)>(f));});
        timings.close();
        std::cout<<"assertions="<<n<<" boundary_checks="<<verify.count<<" exact_export_and_replay=true authorization_isolated=true proposal_uncommitted=true\n";return 0;
    }catch(const std::exception& e){product_allocations::active=false;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
