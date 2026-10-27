// Compile with the original typed-field headers to generate golden fixtures and
// verify that that reader refuses the new, explicitly tagged declaration profile.
#include "compact_tensor_fixture.hpp"
#include <iostream>
using namespace compact_fixture;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("usage: old-codec emit DIRECTORY | reject ARCHIVE");
    if(std::string(argv[1])=="reject"){
        const auto b=load(argv[2]);bool rejected=false;
        try{(void)storage::decode_declared_archive(b);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("old reader accepted unsupported profile");
        std::cout<<"predecessor reader rejected new profile\n";return 0;
    }
    if(std::string(argv[1])!="emit")throw std::runtime_error("unknown mode");
    std::filesystem::create_directories(argv[2]);
    for(const auto& name:{"full","partial","zero","nested","empty","large"}){
        const std::string n=name;const std::size_t size=n=="empty"?0:n=="large"?1000003:3;
        const auto v=n=="nested"?nested():tensor(size,n!="partial",n=="zero");
        const CompiledLayout l(registry(size),id(n=="nested"?6:2));
        const auto raw=storage::encode_declared_bits(l,v);
        save(std::filesystem::path(argv[2])/(n+".bits"),raw.bytes);
        std::ofstream(std::filesystem::path(argv[2])/(n+".count"))<<raw.bit_count<<'\n';
        save(std::filesystem::path(argv[2])/(n+".rfcxv"),storage::encode_declared_archive(l,std::span(&v,1)));
        std::cout<<n<<" "<<raw.bit_count<<" bits "<<raw.bytes.size()<<" bytes\n";
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
