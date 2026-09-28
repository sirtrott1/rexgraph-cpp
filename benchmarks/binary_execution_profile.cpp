// Allocation/timing comparison against the unchanged per-word interpreter.
// Build with -DRFC_EXECUTION_BASELINE and the F5 include root for an independent
// baseline binary. GMP/process allocations and OS page caches are NOT measured.
#include "rfc/composite/program.hpp"
#ifndef RFC_EXECUTION_BASELINE
#include "rfc/composite/execution.hpp"
#endif
#include <chrono>
#include <cstdlib>
#include <new>
#include <iostream>
#include <algorithm>
#include <array>
#include <string>

namespace allocations {thread_local bool active=false;thread_local std::size_t count=0,bytes=0;}
// Keep paired overrides out of caller inlining. This avoids GCC's false
// mismatched-new/delete diagnostic on our intentionally paired malloc/free
// implementation, and preserves an ordinary allocator call boundary.
#if defined(__GNUC__) || defined(__clang__)
#define RFC_PROFILE_NOINLINE __attribute__((noinline))
#else
#define RFC_PROFILE_NOINLINE
#endif
RFC_PROFILE_NOINLINE void* operator new(std::size_t n){if(void* p=std::malloc(n?n:1)){if(allocations::active){++allocations::count;allocations::bytes+=n;}return p;}throw std::bad_alloc();}
RFC_PROFILE_NOINLINE void* operator new[](std::size_t n){return ::operator new(n);}
RFC_PROFILE_NOINLINE void operator delete(void* p) noexcept{std::free(p);}RFC_PROFILE_NOINLINE void operator delete[](void* p) noexcept{std::free(p);}
RFC_PROFILE_NOINLINE void operator delete(void* p,std::size_t) noexcept{std::free(p);}RFC_PROFILE_NOINLINE void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#undef RFC_PROFILE_NOINLINE
namespace {
using namespace rfc;using namespace composite;using Clock=std::chrono::steady_clock;
using Word=BinaryProgram::word_type;
struct Fixture {std::string name;std::size_t slots;BinaryProgram program;std::vector<std::uint16_t> outputs;};
Word hash(const std::vector<machine::PackedBits>& values){Word h=1469598103934665603ULL;for(const auto& v:values)for(auto w:v.words()){h^=w;h*=1099511628211ULL;}return h;}
std::vector<machine::PackedBits> reference(const Fixture& f,const AspectPlanes& planes){
    std::vector<machine::PackedBits> out;out.reserve(f.outputs.size());
    for(auto r:f.outputs){machine::PackedBits bits(f.slots);for(std::size_t w=0;w<bits.word_count();++w)bits.assign_word(w,f.program.execute_domain_word_with_provider(f.slots,w,r,[&](auto a,auto wi){return planes.plane(a).word(wi);}));out.push_back(std::move(bits));}return out;
}
}
int main(int argc,char** argv){try{
    const auto repetitions=argc>1?std::stoul(argv[1]):9; if(repetitions<3 || repetitions>101)throw std::invalid_argument("repetitions 3..101");
    BinaryProgram masks=compile_active_orientation_query(0,1,false);
    BinaryProgram sparse(65535);sparse.append({BinaryOpcode::LoadAspect,60000,0,0,0,0});sparse.append({BinaryOpcode::LoadAspect,65534,0,0,0,1});sparse.append({BinaryOpcode::AndNot,65534,60000,65534});
    BinaryProgram multi(16);for(std::uint16_t r=0;r<8;++r)multi.append({BinaryOpcode::LoadAspect,r,0,0,0,static_cast<AspectId>(r%4)});
    for(std::uint16_t r=8;r<16;++r)multi.append({r%2?BinaryOpcode::Xor:BinaryOpcode::Select,r,static_cast<std::uint16_t>(r-8),static_cast<std::uint16_t>(r-7),static_cast<std::uint16_t>((r-6)%8)});
    const std::vector<Fixture> fixtures{{"small_tail",65,masks,{2}},{"mask_1m",1048589,masks,{2}},{"sparse_registers",65537,sparse,{65534}},{"eight_outputs",262145,multi,{0,2,4,6,8,10,12,14}}};
    std::cout<<"mode,fixture,slots,outputs,plan_ns,register_words,zero_words,median_ns,p95_ns,cpp_allocations,cpp_allocated_bytes,checksum,exact\n";
    for(const auto& f:fixtures){
        AspectSchema schema;for(int i=0;i<4;++i)schema.add("a"+std::to_string(i));AspectPlanes planes(schema,f.slots);Word random=42;
        for(std::size_t a=0;a<4;++a)for(std::size_t w=0;w<planes.plane(0).word_count();++w){random^=random<<13;random^=random>>7;random^=random<<17;planes.plane(static_cast<AspectId>(a)).assign_word(w,random);}
        const auto expected=reference(f,planes);const auto expected_hash=hash(expected);
        std::size_t scratch=f.program.register_count(),zeros=scratch;long long plan_ns=0;
#ifndef RFC_EXECUTION_BASELINE
        const auto start=Clock::now();const BinaryExecutionPlan plan(f.program,f.outputs);plan_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();BinaryExecutionWorkspace workspace;
        scratch=plan.register_words();zeros=plan.zeroed_register_words();
        const auto run=[&]{return plan.materialize(planes,workspace);};
        constexpr const char* mode="lowered";
#else
        const auto run=[&]{return reference(f,planes);};
        constexpr const char* mode="F5-reference";
#endif
        const auto check=run();for(std::size_t i=0;i<check.size();++i)if(check[i].words()!=expected[i].words())throw std::logic_error("non-exact execution");
        std::vector<long long> ns;std::size_t count=0,bytes=0;
        for(std::size_t r=0;r<repetitions;++r){allocations::count=allocations::bytes=0;allocations::active=true;const auto t=Clock::now();auto output=run();const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();allocations::active=false;
            if(output.size()!=expected.size())throw std::logic_error("output arity");
            for(std::size_t i=0;i<output.size();++i)if(output[i].words()!=expected[i].words())throw std::logic_error("full output mismatch");
            if(hash(output)!=expected_hash) { throw std::logic_error("output mismatch"); }
            count=allocations::count;bytes=allocations::bytes;ns.push_back(elapsed);}
        std::sort(ns.begin(),ns.end());std::cout<<mode<<','<<f.name<<','<<f.slots<<','<<f.outputs.size()<<','<<plan_ns<<','<<scratch<<','<<zeros<<','<<ns[ns.size()/2]<<','<<ns[(ns.size()*95+99)/100-1]<<','<<count<<','<<bytes<<','<<expected_hash<<",true\n";
#ifndef RFC_EXECUTION_BASELINE
        // No-output-allocation path. Each sink invocation is synchronous and
        // its work is explicit; reference hash in identical word-major order.
        Word reference_hash=0;for(std::size_t w=0;w<expected.front().word_count();++w)for(std::size_t o=0;o<expected.size();++o)reference_hash=reference_hash*33+expected[o].word(w);
        ns.clear();for(std::size_t r=0;r<repetitions;++r){Word h=0;allocations::count=allocations::bytes=0;allocations::active=true;const auto t=Clock::now();plan.for_each_word(f.slots,[&](auto a,auto w){return planes.plane(a).word(w);},[&](std::size_t word,std::span<const Word> outputs){for(std::size_t i=0;i<outputs.size();++i){if(outputs[i]!=expected[i].word(word))throw std::logic_error("stream word mismatch");h=h*33+outputs[i];}},workspace);const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();allocations::active=false;if(h!=reference_hash)throw std::logic_error("stream mismatch");count=allocations::count;bytes=allocations::bytes;ns.push_back(elapsed);}
        std::sort(ns.begin(),ns.end());std::cout<<"streamed"<<','<<f.name<<','<<f.slots<<','<<f.outputs.size()<<','<<plan_ns<<','<<scratch<<','<<zeros<<','<<ns[ns.size()/2]<<','<<ns[(ns.size()*95+99)/100-1]<<','<<count<<','<<bytes<<','<<reference_hash<<",true\n";
#endif
    }
    return 0;
}catch(const std::exception& e){allocations::active=false;std::cerr<<e.what()<<'\n';return 1;}}
