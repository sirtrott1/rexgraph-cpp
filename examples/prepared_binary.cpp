#include "prepared_binary_data.hpp"
#include <fstream>
#include <iostream>
// Optional output file: one byte per (a,b) pair, in a-major order.
int main(int argc,char** argv){try{
    using namespace prepared_binary_example;
    const auto codes=mux();const PreparedRelationalBinaryOperation prepared(codes,166);
    const auto inputs=byte_pairs();BinaryExecutionWorkspace workspace;
    const auto result=prepared.materialize(2,65536,inputs,workspace);
    std::vector<unsigned char> bytes(65536);
    for(std::size_t row=0;row<bytes.size();++row){
        unsigned value=0;for(std::size_t bit=0;bit<8;++bit)value|=unsigned(result.outputs[bit].get(row))<<bit;
        const auto a=unsigned(row/256),b=unsigned(row%256);
        if(value!=((a&0xA5u)|(b&0x5Au)))throw std::runtime_error("byte mux mismatch");
        bytes[row]=static_cast<unsigned char>(value);
    }
    const auto growth=workspace.growth_events();
    const auto repeated=prepared.materialize(2,65536,inputs,workspace);
    if(growth!=workspace.growth_events() || repeated.outputs[0].words()!=result.outputs[0].words())throw std::runtime_error("workspace reuse");
    if(argc==2){std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("output write");}
    const auto& r=result.receipt;
    std::cout<<"byte_pairs=65536 outputs=8 mask=165 words="<<r.words
             <<" instructions="<<r.instructions<<" aspect_reads="<<r.aspect_reads
             <<" output_words="<<r.output_words<<" work="<<r.admitted_work
             <<" warm_growth="<<(workspace.growth_events()-growth)<<'\n';
    std::cout<<"a=15 b=240 output="<<unsigned(bytes[15*256+240])<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
