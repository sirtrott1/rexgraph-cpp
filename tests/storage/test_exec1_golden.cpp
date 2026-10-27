#include "../../examples/prepared_binary_source_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
using namespace prepared_binary_source_example;
std::vector<std::byte> read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("missing golden");
std::vector<char> chars{std::istreambuf_iterator<char>(f),{}};std::vector<std::byte> b;for(unsigned char x:chars)b.push_back(std::byte(x));return b;}
void write(const std::filesystem::path& path,const std::vector<std::byte>& bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!f)throw std::runtime_error("golden write");}
int main(int argc,char** argv){try{
#ifdef RFC_EXEC1_EMIT
    if(argc!=2)return 2;
    const std::filesystem::path dir=argv[1];
    std::filesystem::create_directories(dir);
#else
    (void)argc;(void)argv;
    const std::filesystem::path dir=RFC_EXEC1_GOLDENS;
#endif
    unsigned checks=0;
    for(unsigned version=1;version<=2;++version){auto f=prepared_binary_source_example::make();f.endpoint.declarations->profile=static_cast<BooleanLayoutProfile>(version);
        const auto source=encode_rfcx(f.endpoint.source);const auto endpoint=encode_temporal_endpoint(f.endpoint);
        temporal_codec_detail::Writer w;temporal_codec_detail::binary_program(w,*f.endpoint.codes->operations.front().binary);
        const std::vector<std::pair<std::string,std::vector<std::byte>>> files{{"source.rfcx",source.bytes},{"endpoint-v"+std::to_string(version)+".rfcxe",endpoint},{"program.bin",w.bytes()}};
        for(const auto& [name,bytes]:files){
#ifdef RFC_EXEC1_EMIT
            write(dir/name,bytes);
#else
            if(read(dir/name)!=bytes)throw std::runtime_error("predecessor bytes changed: "+name);
        ++checks;
#endif
        }
        const auto reopened=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),endpoint);
        if(encode_temporal_endpoint(reopened)!=endpoint)throw std::runtime_error("noncanonical endpoint");
        ++checks;
    }
    std::cout<<"exec1_golden checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
