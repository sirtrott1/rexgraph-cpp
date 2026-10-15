#include "rfc/storage/prepared_byte_packet.hpp"
#include <atomic>
#include <iostream>
#include <thread>

namespace {
using Bytes=std::vector<std::byte>;
using Span=std::span<const std::byte>;
using Packet=rfc::storage::PreparedBytePacket;
std::size_t checks{};
void check(bool okay,const char* message){++checks;if(!okay){throw std::runtime_error(message);}}
template<class Function> void rejects(Function&& function,const char* message){bool caught{};try{function();}catch(const std::exception&){caught=true;}check(caught,message);}
constexpr std::array<std::byte,8> magic{std::byte{'T'},std::byte{'E'},std::byte{'S'},std::byte{'T'},std::byte{1},std::byte{0},std::byte{0},std::byte{0}};
struct Receipt {
    std::shared_ptr<const Bytes> owner;
    const Bytes& receipt() const{return *owner;}
};
Bytes original(const Bytes& payload,std::size_t repeats){
    rfc::storage::rfcx_detail::Writer w;w.raw(magic);w.u32(1);w.u32(0);w.u64(0);w.u64(0);w.u64(repeats);
    for(std::size_t i=0;i<repeats;++i){w.u64(payload.size());w.raw(payload);}
    w.patch_u64(16,w.size());w.patch_u64(24,rfc::storage::rfcx_detail::fnv1a64(Span(w.bytes()).subspan(32)));return std::move(w).take_bytes();
}
void run(){
    std::weak_ptr<const Bytes> lifetime;Packet escaped;Bytes expected;
    {
        const auto immutable=std::make_shared<const Bytes>(200003,std::byte{42});lifetime=immutable;expected=original(*immutable,7);
        rfc::storage::BytePacketBuilder w(magic);w.u64(7);
        for(std::size_t i=0;i<7;++i){w.receipt(Receipt{immutable});}
        escaped=std::move(w).finish();
    }
    check(escaped.bytes()==expected,"native writer envelope length/checksum/blob wire unchanged");
    std::size_t largest{},size{};escaped.stream([&](Span bytes){largest=std::max(largest,bytes.size());size+=bytes.size();});
    check(size==expected.size()&&largest<=65536,"complete bounded packet traversal");escaped.require_equal(expected);check(true,"full canonical comparison");
    auto bad=expected;bad.back()^=std::byte{1};rejects([&]{escaped.require_equal(bad);},"last-byte canonical corruption rejects");bad.push_back(std::byte{0});rejects([&]{escaped.require_equal(bad);},"canonical length rejects");
    Bytes mutable_bytes{std::byte{1},std::byte{2},std::byte{3}};const auto frozen=Packet::freeze(mutable_bytes);mutable_bytes.clear();check(frozen.bytes()==Bytes({std::byte{1},std::byte{2},std::byte{3}}),"arbitrary bytes copied defensively");
    auto mutable_packet=escaped;Bytes result;bool changed{};
    mutable_packet.stream([&](Span bytes){result.insert(result.end(),bytes.begin(),bytes.end());},[&]{if(!changed){changed=true;mutable_packet=Packet{};}});
    check(changed&&result==expected,"packet state pinned before reentrant producer release");
    std::size_t emitted{};rejects([&]{escaped.stream([&](Span){++emitted;},[]{throw std::runtime_error("cancel");});},"stream cancellation propagates");check(!emitted,"initial cancellation emits no bytes");
    rejects([&]{escaped.stream([](Span){throw std::runtime_error("sink");});},"sink failure propagates");
    rejects([&]{rfc::storage::BytePacketBuilder w(magic,31);},"envelope bound before construction");
    rejects([&]{rfc::storage::BytePacketBuilder w(magic,40);w.blob(frozen);},"complete blob length admitted before attachment");
    rejects([&]{rfc::storage::BytePacketBuilder w(magic,33);w.u64(1);},"primitive byte growth admitted");
    Packet nested=frozen;for(std::size_t i=0;i<256;++i){rfc::storage::BytePacketBuilder w(magic);w.blob(nested);nested=std::move(w).finish();}
    rejects([&]{rfc::storage::BytePacketBuilder w(magic);w.blob(nested);},"bounded acyclic packet nesting");
    const auto shared=rfc::storage::encode_shared_packet_stream([&](auto consume){escaped.stream(consume);});
    check(shared==rfc::storage::encode_shared_packet(expected)&&rfc::storage::decode_shared_packet(shared)==expected,"prepared producer preserves canonical shared wire");
    std::atomic<bool> okay{true};std::vector<std::thread> readers;
    for(std::size_t i=0;i<3;++i){readers.emplace_back([&]{try{escaped.require_equal(expected);}catch(...){okay=false;}});}
    for(auto& reader:readers){reader.join();}check(okay,"three concurrent immutable packet readers");
    check(!lifetime.expired(),"borrowed native receipt owner retained");
    escaped=Packet{};check(lifetime.expired(),"receipt storage released after last actual packet consumer");
}
}
int main(){try{run();std::cout<<checks<<" prepared packet checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
