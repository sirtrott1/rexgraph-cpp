#include "rfc/storage/shared_packet_codec.hpp"
#include <atomic>
#include <iostream>
#include <thread>

namespace {
using Bytes=std::vector<std::byte>;
using Span=std::span<const std::byte>;
using Limits=rfc::storage::SharedPacketLimits;
std::size_t checks{};
void require(bool okay,const char* message){++checks;if(!okay){throw std::runtime_error(message);}}
template<class Function> void rejects(Function&& function,const char* message){bool caught{};try{function();}catch(const std::exception&){caught=true;}require(caught,message);}
Bytes image(std::size_t size,std::uint64_t seed){Bytes out(size);for(auto& byte:out){seed=seed*6364136223846793005ULL+1;byte=static_cast<std::byte>(seed>>56u);}return out;}
Bytes packet(std::uint64_t expanded,const std::vector<Bytes>& chunks,const std::vector<std::size_t>& roots){
    rfc::storage::rfcx_detail::Writer w;w.raw(rfc::storage::shared_packet_detail::magic);w.u32(1);w.u32(0);w.u64(0);w.u64(0);w.u64(expanded);w.u64(chunks.size());
    for(const auto& chunk:chunks){w.u64(chunk.size());w.raw(chunk);}w.u64(roots.size());for(const auto id:roots){w.u64(id);}
    w.patch_u64(16,w.size());w.patch_u64(24,rfc::storage::rfcx_detail::fnv1a64(Span(w.bytes()).subspan(32)));return std::move(w).take_bytes();
}
void roundtrip(const Bytes& source,std::size_t step){
    using namespace rfc::storage;
    const auto expected=encode_shared_packet(source);std::size_t calls{};
    const auto streamed=encode_shared_packet_stream([&](auto consume){++calls;consume(Span{});for(std::size_t pos=0;pos<source.size();){const auto n=std::min(step,source.size()-pos);consume(Span(source).subspan(pos,n));pos+=n;}});
    require(calls==1&&streamed==expected,"fragmentation cannot change canonical shared bytes");
    Bytes restored;std::size_t largest{};
    const auto stats=stream_shared_packet(streamed,[&](Span chunk){largest=std::max(largest,chunk.size());restored.insert(restored.end(),chunk.begin(),chunk.end());});
    const auto inspected=inspect_shared_packet(streamed);
    require(restored==source&&decode_shared_packet(streamed)==source,"all original bytes reconstruct exactly");
    require(largest<=4096&&stats.expanded_bytes==source.size()&&stats.stored_bytes==streamed.size()&&stats.unique_chunks==inspected.unique_chunks&&stats.chunk_references==inspected.chunk_references&&stats.literal_bytes==inspected.literal_bytes,"bounded callbacks and passive inspection agree");
}
void run(){
    using namespace rfc::storage;
    for(const auto size:{0u,1u,255u,256u,257u,4095u,4096u,4097u,65537u}){const auto source=image(size,size+123u);for(const auto step:{1u,7u,255u,256u,4095u,65536u}){roundtrip(source,step);}}
    for(std::uint64_t seed=0;seed<32;++seed){roundtrip(image(static_cast<std::size_t>(seed)*719+23,seed),static_cast<std::size_t>(seed)*73+1);}
    Bytes repeated(65536,std::byte{0});roundtrip(repeated,3);const auto good=encode_shared_packet(repeated);
    const auto a=Bytes{std::byte{'a'}},b=Bytes{std::byte{'b'}};
    const std::vector<Bytes> bad{packet(1,{a},{1}),packet(0,{{}},{}),packet(1,{a},{0,0}),packet(2,{a,a},{0,1}),packet(1,{a,b},{0}),packet(2,{b,a},{1,0}),packet(2,{a,b},{0,1}),packet(4097,{Bytes(4096,std::byte{0}),a},{0,0}),packet(8192,{Bytes(4096,std::byte{0}),Bytes(4096,std::byte{0})},{0,1})};
    for(const auto& bytes:bad){std::size_t emitted{};rejects([&]{stream_shared_packet(bytes,[&](Span){++emitted;});},"noncanonical streams reject");require(!emitted,"full malformed packet validation precedes output");rejects([&]{inspect_shared_packet(bytes);},"inspection applies complete canonical validation");}
    auto corrupt=good;corrupt.back()^=std::byte{1};std::size_t emitted{};
    rejects([&]{stream_shared_packet(corrupt,[&](Span){++emitted;});},"integrity rejects");require(!emitted,"integrity failure emits nothing");
    auto mutable_packet=good;auto policy=Limits{};bool changed{};Bytes restored;
    stream_shared_packet(mutable_packet,[&](Span chunk){mutable_packet.clear();policy.max_expanded_bytes=0;restored.insert(restored.end(),chunk.begin(),chunk.end());},policy,[&]{if(!changed){changed=true;mutable_packet.clear();policy.max_records=0;}});
    require(changed&&restored==repeated,"stored input and policy pinned before reentrant callbacks");
    auto limits=Limits{};limits.max_expanded_bytes=repeated.size()-1;
    rejects([&]{stream_shared_packet(good,[](Span){},limits);},"logical expansion admission remains mandatory without allocation");
    rejects([&]{encode_shared_packet_stream([&](auto consume){consume(Span(repeated));},limits);},"stream encoder admits total expanded bytes");
    limits=Limits{};limits.max_records=1;rejects([&]{encode_shared_packet_stream([&](auto consume){consume(Span(repeated));},limits);},"stream cumulative record admission");
    limits=Limits{};limits.max_stored_bytes=56;rejects([&]{encode_shared_packet_stream([&](auto consume){consume(Span(repeated));},limits);},"stream stored admission");
    limits=Limits{};limits.max_work_bytes=16;rejects([&]{encode_shared_packet_stream([&](auto consume){consume(Span(repeated));},limits);},"stream encoder counted work admission");
    rejects([&]{stream_shared_packet(good,[](Span){}, {},[]{throw std::runtime_error("cancel");});},"validation cancellation emits nothing");
    rejects([&]{stream_shared_packet(good,[](Span){throw std::runtime_error("sink");});},"sink failure propagates");
    rejects([&]{encode_shared_packet_stream([&](auto consume){consume(Span(repeated));throw std::runtime_error("producer");});},"producer failure returns no packet");
    rejects([&]{encode_shared_packet_stream([&](auto consume){consume(Span(repeated));},{},[]{throw std::runtime_error("cancel");});},"encoder cancellation returns no packet");
    std::atomic<bool> okay{true};std::vector<std::thread> readers;
    for(std::size_t i=0;i<3;++i){readers.emplace_back([&]{try{std::size_t size{};stream_shared_packet(good,[&](Span chunk){for(const auto byte:chunk){if(byte!=std::byte{0})okay=false;}size+=chunk.size();});if(size!=repeated.size())okay=false;}catch(...){okay=false;}});}
    for(auto& reader:readers){reader.join();}require(okay,"three independent concurrent streaming readers");
}
}
int main(){try{run();std::cout<<checks<<" shared stream checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
