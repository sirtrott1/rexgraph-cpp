#pragma once

#include "rfc/storage/rfcx.hpp"
#include "rfc/machine/checked_size.hpp"
#include <functional>
#include <map>

namespace rfc::storage {
struct SharedPacketLimits {
    std::size_t max_stored_bytes{256u*1024u*1024u};
    std::size_t max_expanded_bytes{256u*1024u*1024u};
    std::size_t max_records{1000000u};
    std::size_t max_work_bytes{std::size_t{2048}*1024u*1024u};
};
struct SharedPacketStats {
    std::size_t stored_bytes{},expanded_bytes{},unique_chunks{},chunk_references{},literal_bytes{};
};
namespace shared_packet_detail {
using Bytes=std::vector<std::byte>;
using Span=std::span<const std::byte>;
inline constexpr std::array<std::byte,8> magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'S'},std::byte{'P'},std::byte{1},std::byte{0},std::byte{0}};
inline void require(bool okay,const char* message){if(!okay){throw std::invalid_argument(message);}}
inline void bound(std::size_t value,std::size_t maximum,const char* message){if(value>maximum){throw std::length_error(message);}}
using machine::checked_size_add;
using machine::checked_size_mul;
class Guard {
public:
    Guard(SharedPacketLimits policy,std::function<void()> check):limits(policy),check_(std::move(check)){}
    void work(std::size_t n){work_=checked_size_add(work_,n);bound(work_,limits.max_work_bytes,"shared packet byte work exceeds admission");if(check_){check_();}}
    void records(std::size_t n) const {bound(n,limits.max_records,"shared packet record admission");}
    std::size_t work_bytes() const noexcept {return work_;}
    const SharedPacketLimits limits;
private:
    std::size_t work_{};std::function<void()> check_;
};
inline constexpr std::uint64_t gear(std::uint64_t x){
    x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30u))*0xbf58476d1ce4e5b9ULL;
    x=(x^(x>>27u))*0x94d049bb133111ebULL;return x^(x>>31u);
}
inline constexpr auto gears=[] {
    std::array<std::uint64_t,256> out{};for(std::size_t i=0;i<out.size();++i){out[i]=gear(i);}return out;
}();
inline std::uint64_t hash(Span bytes,Guard& guard){
    std::uint64_t out=1469598103934665603ULL;
    for(std::size_t begin=0;begin<bytes.size();) {
        const auto n=std::min(std::size_t{65536},bytes.size()-begin);guard.work(n);
        out=rfcx_detail::fnv1a64_update(out,bytes.subspan(begin,n));begin+=n;
    }return out;
}
inline bool equal(Span a,Span b,Guard& guard){
    if(a.size()!=b.size()){return false;}guard.work(a.size());return std::equal(a.begin(),a.end(),b.begin());
}
inline bool boundary(std::uint64_t rolling,std::size_t size){return size>=4096||(size>=256&&(rolling&255u)==0);}
template<class Chunk> struct PacketPlan {std::vector<Chunk> chunks;std::vector<std::size_t> roots;std::size_t stored{56},literal{};};
using Plan=PacketPlan<Span>;
using OwnedPlan=PacketPlan<Bytes>;
using Buckets=std::map<std::uint64_t,std::vector<std::size_t>>;
template<class Chunk,class Retain> void append_chunk(PacketPlan<Chunk>& out,Span chunk,Buckets& buckets,Guard& guard,Retain&& retain){
    const auto digest=hash(chunk,guard);const auto found=buckets.find(digest);auto id=out.chunks.size();
    if(found!=buckets.end()){for(const auto index:found->second){if(equal(chunk,Span(out.chunks[index]),guard)){id=index;break;}}}
    auto stored=checked_size_add(out.stored,8);auto records=checked_size_add(checked_size_add(out.roots.size(),out.chunks.size()),1);
    if(id==out.chunks.size()){records=checked_size_add(records,1);stored=checked_size_add(stored,checked_size_add(8,chunk.size()));}
    guard.records(records);bound(stored,guard.limits.max_stored_bytes,"shared packet stored bytes exceed admission");
    if(id==out.chunks.size()){buckets[digest].push_back(id);out.chunks.push_back(retain(chunk));out.literal=checked_size_add(out.literal,chunk.size());}
    out.roots.push_back(id);out.stored=stored;
}
inline Plan plan(Span bytes,Guard& guard){
    bound(bytes.size(),guard.limits.max_expanded_bytes,"shared packet expanded bytes exceed admission");
    bound(std::size_t{56},guard.limits.max_stored_bytes,"shared packet stored bytes exceed admission");
    Plan out;Buckets buckets;std::size_t begin{};std::uint64_t rolling{};
    for(std::size_t i=0;i<bytes.size();++i){
        if((i%65536u)==0){guard.work(std::min(std::size_t{65536},bytes.size()-i));}
        rolling=(rolling<<1u)+gears[std::to_integer<std::uint8_t>(bytes[i])];const auto size=i-begin+1;
        if(i+1!=bytes.size()&&!boundary(rolling,size)){continue;}
        append_chunk(out,bytes.subspan(begin,size),buckets,guard,[](Span chunk){return chunk;});begin=i+1;rolling=0;
    }return out;
}
inline Bytes freeze(Span bytes,std::size_t maximum,std::size_t work){
    bound(bytes.size(),maximum,"shared packet source bytes exceed admission");bound(bytes.size(),work,"shared packet freeze byte work exceeds admission");return {bytes.begin(),bytes.end()};
}
template<class Chunk> inline SharedPacketStats stats(const PacketPlan<Chunk>& p,std::size_t expanded){return {p.stored,expanded,p.chunks.size(),p.roots.size(),p.literal};}
// Every chunk starts with zero rolling state. Certifying each distinct literal
// once, plus canonical first-use order, certifies the full expanded partition.
// Final short chunks are allowed only at the last reference; dead, duplicate,
// reordered and prematurely split literals remain noncanonical.
inline Plan validate(Span bytes,Guard& guard,std::size_t& expanded){
    guard.work(bytes.size());
    require(bytes.size()>=56,"truncated shared packet");rfcx_detail::Reader r(bytes);
    for(const auto byte:magic){require(r.u8()==std::to_integer<std::uint8_t>(byte),"shared packet magic");}
    require(r.u32()==1&&r.u32()==0,"unsupported shared packet profile");require(r.u64()==bytes.size(),"shared packet stored length");
    const auto integrity=r.u64();require(integrity==hash(bytes.subspan(32),guard),"shared packet integrity");
    expanded=r.size_value();bound(expanded,guard.limits.max_expanded_bytes,"shared packet expanded bytes exceed admission");
    const auto count=r.size_value();guard.records(count);r.require_items(count,9);(void)checked_size_mul(count,sizeof(Span));
    Plan out;out.chunks.reserve(count);std::vector<bool> ended;ended.reserve(count);Buckets buckets;
    for(std::size_t i=0;i<count;++i){
        guard.work(8);const auto size=r.size_value();require(size&&size<=4096,"shared packet literal chunk length");const auto chunk=r.slice(size);
        const auto digest=hash(chunk,guard);const auto found=buckets.find(digest);
        if(found!=buckets.end()){for(const auto index:found->second){require(!equal(chunk,out.chunks[index],guard),"noncanonical duplicate shared packet literal");}}
        std::uint64_t rolling{};bool end{};guard.work(size);
        for(std::size_t j=0;j<size;++j){rolling=(rolling<<1u)+gears[std::to_integer<std::uint8_t>(chunk[j])];end=boundary(rolling,j+1);require(!end||j+1==size,"noncanonical shared packet literal partition");}
        buckets[digest].push_back(i);out.chunks.push_back(chunk);ended.push_back(end);out.literal=checked_size_add(out.literal,size);
    }
    const auto references=r.size_value();guard.records(checked_size_add(count,references));r.require_items(references,8);(void)checked_size_mul(references,sizeof(std::size_t));
    out.roots.reserve(references);std::size_t total{},next{};
    for(std::size_t i=0;i<references;++i){
        guard.work(8);const auto id=r.size_value();require(id<out.chunks.size(),"shared packet reference index");
        require(id<=next,"noncanonical shared packet first-use order");if(id==next){++next;}
        require(ended[id]||i+1==references,"noncanonical shared packet premature short chunk");
        total=checked_size_add(total,out.chunks[id].size());bound(total,expanded,"shared packet expansion exceeds declared size");out.roots.push_back(id);
    }
    require(!r.remaining()&&total==expanded,"shared packet trailing bytes or inconsistent expansion");
    require(next==count,"noncanonical unused shared packet literal");out.stored=bytes.size();return out;
}
template<class Chunk> inline Bytes encode(const PacketPlan<Chunk>& p,std::size_t expanded,Guard& guard){
    guard.work(56);rfcx_detail::Writer w(p.stored);w.raw(magic);w.u32(1);w.u32(0);w.u64(p.stored);w.u64(0);
    w.u64(expanded);w.u64(p.chunks.size());for(const auto& chunk:p.chunks){guard.work(checked_size_add(8,chunk.size()));w.u64(chunk.size());w.raw(Span(chunk));}
    w.u64(p.roots.size());for(const auto id:p.roots){guard.work(8);w.u64(id);}require(w.size()==p.stored,"shared packet encoding size");
    w.patch_u64(24,hash(std::span<const std::byte>(w.bytes()).subspan(32),guard));return std::move(w).take_bytes();
}
} // namespace shared_packet_detail
// Passive lossless byte storage. Neither encoding nor reconstruction supplies
// mathematical identity, source transport, trusted origins or execution rights.
inline std::vector<std::byte> encode_shared_packet(std::span<const std::byte> input,SharedPacketLimits limits={},std::function<void()> check={}){
    using namespace shared_packet_detail;const auto frozen=freeze(input,limits.max_expanded_bytes,limits.max_work_bytes);Guard guard(limits,std::move(check));guard.work(frozen.size());
    return encode(plan(frozen,guard),frozen.size(),guard);
}
// The producer supplies its declared stable byte stream once. It must retain
// each supplied span through consume(), including reentrant cancellation checks.
// Source version/owner pinning belongs to that producer; byte sharing grants no
// source identity or authority. At most one 4096-byte pending literal and the
// admitted dictionary/reference plan are retained, never a full expanded image.
template<class Produce> std::vector<std::byte> encode_shared_packet_stream(Produce&& produce,SharedPacketLimits limits={},std::function<void()> check={}){
    using namespace shared_packet_detail;Guard guard(limits,std::move(check));OwnedPlan out;Buckets buckets;Bytes pending;pending.reserve(4096);
    bound(std::size_t{56},limits.max_stored_bytes,"shared packet stored bytes exceed admission");std::size_t expanded{};std::uint64_t rolling{};
    const auto consume=[&](Span bytes){
        const auto total=checked_size_add(expanded,bytes.size());bound(total,limits.max_expanded_bytes,"shared packet expanded bytes exceed admission");
        for(std::size_t i=0;i<bytes.size();++i){
            if(i%65536u==0){guard.work(std::min(std::size_t{65536},bytes.size()-i));}
            const auto byte=bytes[i];pending.push_back(byte);rolling=(rolling<<1u)+gears[std::to_integer<std::uint8_t>(byte)];
            if(boundary(rolling,pending.size())){append_chunk(out,pending,buckets,guard,[](Span chunk){return Bytes(chunk.begin(),chunk.end());});pending.clear();rolling=0;}
        }expanded=total;
    };
    produce(consume);if(!pending.empty()){append_chunk(out,pending,buckets,guard,[](Span chunk){return Bytes(chunk.begin(),chunk.end());});}
    return encode(out,expanded,guard);
}
// Validate the entire pinned packet before the first output callback. Callbacks
// receive bounded spans valid during the call. Cancellation or a sink failure
// may leave a prefix in the host sink; the host must publish only after success.
template<class Consume> SharedPacketStats stream_shared_packet(std::span<const std::byte> input,Consume&& consume,SharedPacketLimits limits={},std::function<void()> check={}){
    using namespace shared_packet_detail;const auto frozen=freeze(input,limits.max_stored_bytes,limits.max_work_bytes);Guard guard(limits,std::move(check));
    std::size_t expanded{};const auto p=validate(frozen,guard,expanded);
    for(const auto id:p.roots){const auto chunk=p.chunks[id];guard.work(chunk.size());consume(chunk);}
    return stats(p,expanded);
}
inline std::vector<std::byte> decode_shared_packet(std::span<const std::byte> bytes,SharedPacketLimits limits={},std::function<void()> check={}){
    using namespace shared_packet_detail;const auto frozen=freeze(bytes,limits.max_stored_bytes,limits.max_work_bytes);Guard guard(limits,std::move(check));
    std::size_t expanded{};const auto p=validate(frozen,guard,expanded);guard.work(expanded);Bytes out;out.reserve(expanded);
    for(const auto id:p.roots){const auto chunk=p.chunks[id];guard.work(chunk.size());out.insert(out.end(),chunk.begin(),chunk.end());}return out;
}
// Validate and pin the complete stored image before exposing any logical bytes.
// The ordered chunk index replaces the expanded allocation. Existing parsers
// may request bounded contiguous leaves; copies are retained by this one image
// and charged against its remaining byte-work and expanded-byte admission.
inline ByteRange view_shared_packet(std::span<const std::byte> bytes,SharedPacketLimits limits={},std::function<void()> check={}){
    using namespace shared_packet_detail;
    const auto frozen=std::make_shared<const Bytes>(freeze(bytes,limits.max_stored_bytes,limits.max_work_bytes));
    const auto callback=std::move(check);Guard guard(limits,callback);std::size_t expanded{};
    auto plan=validate(*frozen,guard,expanded);guard.work(expanded);
    guard.work(checked_size_mul(checked_size_add(plan.roots.size(),1),sizeof(std::size_t)));
    std::vector<ByteRange::Chunk> chunks;chunks.reserve(plan.chunks.size());
    for(const auto chunk:plan.chunks){chunks.push_back({static_cast<std::size_t>(chunk.data()-frozen->data()),chunk.size()});}
    return ByteRange::indexed(frozen,chunks,std::move(plan.roots),expanded,expanded,
        limits.max_work_bytes-guard.work_bytes(),limits.max_records,callback);
}
inline SharedPacketStats inspect_shared_packet(std::span<const std::byte> bytes,SharedPacketLimits limits={},std::function<void()> check={}){
    using namespace shared_packet_detail;const auto frozen=freeze(bytes,limits.max_stored_bytes,limits.max_work_bytes);Guard guard(limits,std::move(check));
    std::size_t expanded{};const auto p=validate(frozen,guard,expanded);return stats(p,expanded);
}
} // namespace rfc::storage
