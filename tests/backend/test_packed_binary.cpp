#include "rfc/backend/packed_binary.hpp"
#include <iostream>
using namespace rfc;
namespace {
unsigned checks{};
void check(bool ok){++checks;if(!ok)throw std::runtime_error("packed owning preparation assertion");}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught);}
}
int main() {
    using namespace composite;using namespace backend;
    for(auto slots:{0u,1u,31u,32u,33u,63u,64u,65u,127u,129u,513u}) {
        AspectSchema schema;schema.add("a");schema.add("b");AspectPlanes planes(schema,slots);
        for(unsigned i=0;i<slots;++i){planes.set(0,i,i%3==0);planes.set(1,i,i%5==0);}
        BinaryProgram p(4);p.append({BinaryOpcode::LoadAspect,0,0,0,0,0});p.append({BinaryOpcode::LoadAspect,1,0,0,0,1});
        p.append({BinaryOpcode::Select,0,0,1,3,0});p.append({BinaryOpcode::Not,2,0});
        PreparedPackedBinary op(p,planes,{0,2,3,0});auto copy=std::move(op);
        check(copy.same_owner(op));auto out=op.empty_result(100000);
        runtime::CpuTileExecutor cpu({4,4,100});cpu.run_tiles(op.words(),[&](auto w,auto,const auto&){op.cpu_words(w,w+1,out);});
        for(unsigned i=0;i<slots;++i){check(out[0].get(i)==(i%3==0 && i%5==0));check(out[1].get(i)!=out[0].get(i));check(!out[2].get(i));}
        check(out[0].words()==out[3].words());
        if(slots){planes.set(0,0,false);auto frozen=op.empty_result(100000);op.cpu_words(0,op.words(),frozen);check(frozen[0].get(0));}
        rejects([&]{PreparedPackedBinary bad(p,planes,{4});});
        rejects([&]{PreparedPackedBinary bad(p,planes,{0},{0,1,1000});});
        rejects([&]{PreparedPackedBinary bad(p,planes,{0},{100,0,1000});});
        rejects([&]{PreparedPackedBinary bad(p,planes,{0},{100,4,0});});
        rejects([&]{op.cpu_words(0,op.words()+1,out);});
        if(slots)rejects([&]{(void)op.empty_result(0);});
        BinaryProgram absent(1);absent.append({BinaryOpcode::LoadAspect,0,0,0,0,2});
        rejects([&]{PreparedPackedBinary bad(absent,planes,{0});});
        if(slots){std::stop_source stop;stop.request_stop();rejects([&]{op.cpu_words(0,op.words(),out,stop.get_token());});}
        if(slots){planes.plane(0)=machine::PackedBits(slots-1);rejects([&]{PreparedPackedBinary bad(p,planes,{0});});}
    }
    std::cout<<checks<<" owning packed checks passed\n";
}
