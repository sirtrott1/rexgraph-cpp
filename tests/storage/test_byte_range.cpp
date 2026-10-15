#include "rfc/storage/shared_packet_codec.hpp"
#include "rfc/storage/temporal_codec_primitives.hpp"
#include <atomic>
#include <iostream>
#include <thread>

namespace {
using Bytes=std::vector<std::byte>;
using Span=std::span<const std::byte>;
using View=rfc::storage::ByteRange;
std::size_t checks{};
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
template<class F>void rejects(F&& f,const char* message){bool caught{};try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
View indexed(const Bytes& bytes,std::size_t width,std::size_t material,std::size_t work,std::function<void()> callback={}){
    auto owner=std::make_shared<const Bytes>(bytes);std::vector<View::Chunk> chunks;std::vector<std::size_t> roots;
    for(std::size_t i=0;i<bytes.size();i+=std::min(width,bytes.size()-i)){roots.push_back(chunks.size());chunks.push_back({i,std::min(width,bytes.size()-i)});}
    return View::indexed(std::move(owner),chunks,std::move(roots),bytes.size(),material,work,100000,std::move(callback));
}
void run(){
    const Bytes pattern{std::byte{1},std::byte{2},std::byte{3},std::byte{4},std::byte{5},std::byte{6},std::byte{7},std::byte{8}};
    auto owner=std::make_shared<const Bytes>(pattern);std::weak_ptr<const Bytes> lifetime=owner;
    auto repeated=View::indexed(owner,{{0,3},{3,5}},{0,1,0,1},16,16,64,16);owner.reset();
    Bytes expected=pattern;expected.insert(expected.end(),pattern.begin(),pattern.end());
    repeated.require_equal(expected);check(repeated.materialized_bytes()==0,"all ordered repeated references compared without expansion");
    check(!lifetime.expired(),"immutable literal owner retained");
    repeated.subspan(6,7).require_equal(Span(expected).subspan(6,7));check(repeated.materialized_bytes()==0,"cross-boundary subrange stays logical");
    const auto first=repeated.subspan(1,2).span();check(std::equal(first.begin(),first.end(),expected.begin()+1),"one-literal leaf borrows immutable storage");
    check(repeated.materialized_bytes()==0,"one-literal leaf makes no allocation");
    const auto leaf=repeated.subspan(2,9).span();check(std::equal(leaf.begin(),leaf.end(),expected.begin()+2),"cross-literal leaf complete");
    check(repeated.materialized_bytes()==9,"only requested leaf retained");
    check(repeated.subspan(2,9).span().data()==leaf.data()&&repeated.materialized_bytes()==9,"same physical range reused within one image");
    rejects([&]{(void)repeated.subspan(16,1);},"subrange bounds");
    rejects([&]{(void)repeated.subspan(17);},"offset bounds");
    auto bad=expected;bad.back()^=std::byte{1};rejects([&]{repeated.require_equal(bad);},"complete last-byte mismatch rejects");
    rejects([&]{repeated.require_equal(pattern);},"complete length mismatch rejects");
    repeated=View{};check(lifetime.expired(),"literal and leaf ownership release after last image");
    rejects([&]{(void)View::indexed(std::make_shared<const Bytes>(pattern),{{7,2}},{0},2,8,8,8);},"literal bounds before indexing");
    rejects([&]{(void)View::indexed(std::make_shared<const Bytes>(pattern),{{0,8}},{1},8,8,8,8);},"reference bounds before indexing");
    rejects([&]{(void)View::indexed(std::make_shared<const Bytes>(pattern),{{0,8}},{0},7,8,8,8);},"declared complete expansion enforced");
    auto bounded=indexed(expected,3,8,64);rejects([&]{(void)bounded.subspan(2,9).span();},"leaf materialization rejects before allocation");check(bounded.materialized_bytes()==0,"failed allocation admission leaves no retained buffer");
    auto work=indexed(expected,3,16,8);rejects([&]{(void)work.subspan(2,9).span();},"copy work admission before allocation");
    bool cancel=true;auto cancelled=indexed(expected,3,16,64,[&]{if(cancel)throw std::runtime_error("cancel");});
    rejects([&]{(void)cancelled.subspan(2,9).span();},"leaf copy cancellation propagates");check(cancelled.materialized_bytes()==0,"cancelled allocation reservation released");
    cancel=false;cancelled.subspan(2,9).require_equal(Span(expected).subspan(2,9));check(cancelled.subspan(2,9).span().size()==9,"retry after cancellation valid");
    rfc::storage::rfcx_detail::Writer writer;writer.u8(7);writer.u64(0x1020304050607080ULL);writer.u32(5);
    const Bytes name{std::byte{'h'},std::byte{'e'},std::byte{'l'},std::byte{'l'},std::byte{'o'}};writer.raw(name);writer.u64(expected.size());writer.raw(expected);writer.u16(0x7654);
    const auto bytes=std::move(writer).take_bytes();auto logical=indexed(bytes,3,bytes.size(),10*bytes.size());
    rfc::storage::rfcx_detail::Reader reader(logical);
    check(reader.u8()==7&&reader.u64()==0x1020304050607080ULL&&reader.string()=="hello","existing scalar and string parser crosses every literal boundary");
    const auto blob=reader.view_slice(static_cast<std::size_t>(reader.u64()));blob.require_equal(expected);
    check(reader.u16()==0x7654&&reader.remaining()==0,"view slicing preserves complete cursor order");
    rejects([&]{(void)reader.u8();},"cursor truncation rejects");
    check(rfc::storage::rfcx_detail::fnv1a64(logical)==rfc::storage::rfcx_detail::fnv1a64(bytes),"same complete envelope hash on logical bytes");
    rfc::numeric::Integer huge=1;huge<<=137;huge+=31;const rfc::numeric::Rational rational(huge,17);
    rfc::storage::rfcx_detail::Writer rational_writer;rfc::storage::temporal_codec_detail::rational(rational_writer,rational);
    const auto rational_bytes=std::move(rational_writer).take_bytes();auto rational_image=indexed(rational_bytes,2,1024,4096);
    const rfc::storage::TemporalReadLimits policy;rfc::storage::temporal_codec_detail::Reader rational_reader(rational_image,policy);
    check(rational_reader.rational()==rational&&rational_reader.remaining()==0,"existing GMP rational parser crosses numerator and denominator chunks exactly");
    check(rational_image.materialized_bytes()<rational_image.size(),"only noncontiguous integer magnitude materialized");
    const Bytes leading_zero{std::byte{0},std::byte{2},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{1}};
    auto malformed=indexed(leading_zero,2,32,64);rfc::storage::temporal_codec_detail::Reader invalid_integer(malformed,policy);
    rejects([&]{(void)invalid_integer.integer();},"noncanonical integer still rejects across chunk boundaries");
    Bytes large(1024*1024);for(std::size_t i=0;i<large.size();++i)large[i]=static_cast<std::byte>(i%251);
    auto packet=rfc::storage::encode_shared_packet(large);bool changed{};
    auto view=rfc::storage::view_shared_packet(packet,{},[&]{if(!changed){changed=true;std::fill(packet.begin(),packet.end(),std::byte{0});}});
    check(changed,"callback ran after stored source freeze");view.require_equal(large);check(view.materialized_bytes()==0,"validated shared image never allocates complete expansion");
    auto rebinding=view;bool rebound{};Bytes collected;
    rebinding.for_each_span([&](Span part){if(!rebound){rebound=true;rebinding=View{};}collected.insert(collected.end(),part.begin(),part.end());});
    check(rebound&&collected==large,"stream pins image before caller rebind");
    std::atomic<bool> okay{true};std::vector<std::thread> readers;
    for(std::size_t i=0;i<4;++i){readers.emplace_back([&]{try{view.require_equal(large);const auto part=view.subspan(100,511).span();if(!std::equal(part.begin(),part.end(),large.begin()+100))okay=false;}catch(...){okay=false;}});}
    for(auto& t:readers){t.join();}check(okay,"four concurrent logical/leaf readers");
    check(view.materialized_bytes()<=511,"concurrent readers retain only requested range");
    auto split=indexed(large,31,4096,8192);readers.clear();okay=true;
    for(std::size_t i=0;i<4;++i){readers.emplace_back([&]{try{const auto part=split.subspan(100,511).span();if(!std::equal(part.begin(),part.end(),large.begin()+100))okay=false;}catch(...){okay=false;}});}
    for(auto& t:readers){t.join();}check(okay,"four concurrent materializations across actual literal boundaries");
    check(split.materialized_bytes()==511,"concurrent cross-literal requests retain one immutable leaf");
    auto invalid=rfc::storage::encode_shared_packet(large);invalid.back()^=std::byte{1};rejects([&]{(void)rfc::storage::view_shared_packet(invalid);},"complete stored validation before logical exposure");
    rfc::storage::SharedPacketLimits cap;cap.max_expanded_bytes=large.size()-1;const auto good=rfc::storage::encode_shared_packet(large);
    rejects([&]{(void)rfc::storage::view_shared_packet(good,cap);},"shared expanded admission preserved");
}
}
int main(){try{run();std::cout<<checks<<" byte range checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
