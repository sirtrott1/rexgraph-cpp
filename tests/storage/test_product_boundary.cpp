#include "../../benchmarks/product_boundary_fixture.hpp"
int main(int argc,char** argv) {
    try {
        if(argc>2)throw std::invalid_argument("usage: rfc_product_boundary_tests [NEW_DIRECTORY]");
        std::optional<std::filesystem::path> out;
        if(argc==2){out=argv[1];if(!std::filesystem::create_directory(*out))throw std::invalid_argument("fresh directory required");}
        rfc_product_boundary::Verify verify;
        rfc_product_boundary::run(9,out,verify,[](const char*,std::size_t,auto&& f){return f();});
        std::cout<<verify.count<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
