#pragma once

#include "rfc/storage/shared_packet_codec.hpp"

namespace rfc::storage {
class BytePacketBuilder;

// Passive immutable byte composition, not a source owner or execution grant.
// A child packet may occur repeatedly without duplicating its retained buffers.
class PreparedBytePacket {
    struct State;
    struct Part {
        std::shared_ptr<const State> child;
        std::shared_ptr<const void> owner;
        std::span<const std::byte> bytes;
    };
    struct State {
        std::vector<Part> parts;
        std::size_t size{};
        std::size_t depth{};
    };
    explicit PreparedBytePacket(std::shared_ptr<const State> state):state_(std::move(state)){}
    template<class Consume> static void visit(const State& state,Consume& consume,const std::function<void()>& check){
        for(const auto& part:state.parts){
            if(part.child){visit(*part.child,consume,check);continue;}
            for(std::size_t begin=0;begin<part.bytes.size();){
                const auto n=std::min(std::size_t{65536},part.bytes.size()-begin);
                if(check){check();}consume(part.bytes.subspan(begin,n));begin+=n;
            }
        }
    }
    friend class BytePacketBuilder;
public:
    PreparedBytePacket():state_(std::make_shared<const State>()){}
    std::size_t size() const noexcept{return state_->size;}
    // Arbitrary external bytes are defensively copied before any callbacks.
    static PreparedBytePacket freeze(std::span<const std::byte> bytes){
        auto owner=std::make_shared<const std::vector<std::byte>>(bytes.begin(),bytes.end());
        return PreparedBytePacket(std::make_shared<const State>(State{{Part{{},owner,*owner}},bytes.size(),0}));
    }
    // The declared owner must expose immutable bytes stable for its lifetime.
    // Used by native origin receipts; constness alone is not an immutability proof.
    template<class Owner,class Access> static PreparedBytePacket pin(Owner owner,Access&& access){
        const auto held=std::make_shared<const Owner>(std::move(owner));
        const std::span<const std::byte> bytes=access(*held);
        return PreparedBytePacket(std::make_shared<const State>(State{{Part{{},held,bytes}},bytes.size(),0}));
    }
    template<class Consume> void stream(Consume&& consume,std::function<void()> check={}) const {
        const auto pin=state_;const auto callback=std::move(check);visit(*pin,consume,callback);
    }
    std::vector<std::byte> bytes(std::function<void()> check={}) const {
        const auto pin=*this;std::vector<std::byte> out;out.reserve(pin.size());
        pin.stream([&](auto chunk){out.insert(out.end(),chunk.begin(),chunk.end());},std::move(check));return out;
    }
    // Expected bytes must already be immutable/pinned by the calling decoder.
    // Compare the entire canonical stream without allocating its expanded image.
    void require_equal(std::span<const std::byte> expected,std::function<void()> check={}) const {
        const auto pin=*this;shared_packet_detail::require(expected.size()==pin.size(),"noncanonical prepared packet length");
        std::size_t offset{};
        pin.stream([&](auto chunk){
            const auto actual=expected.subspan(offset,chunk.size());
            shared_packet_detail::require(std::equal(chunk.begin(),chunk.end(),actual.begin()),"noncanonical prepared packet bytes");
            offset=machine::checked_size_add(offset,chunk.size());
        },std::move(check));
        shared_packet_detail::require(offset==expected.size(),"incomplete prepared packet comparison");
    }
private:
    std::shared_ptr<const State> state_;
};

class BytePacketBuilder {
public:
    explicit BytePacketBuilder(const std::array<std::byte,8>& magic,std::size_t maximum=std::numeric_limits<std::size_t>::max(),shared_packet_detail::Guard* work=nullptr)
        :magic_(magic),maximum_(maximum),work_(work){shared_packet_detail::bound(32,maximum_,"prepared packet byte admission");}
    template<class Write> void write(Write&& function){
        function(pending_);shared_packet_detail::bound(machine::checked_size_add(size_,pending_.size()),maximum_,"prepared packet byte admission");
        if(pending_.size()>=65536){flush();}
    }
    void u8(std::uint8_t value){write([&](auto& w){w.u8(value);});}
    void u64(std::uint64_t value){write([&](auto& w){w.u64(value);});}
    void blob(PreparedBytePacket packet){
        const auto total=machine::checked_size_add(machine::checked_size_add(size_,pending_.size()),machine::checked_size_add(8,packet.size()));
        shared_packet_detail::bound(total,maximum_,"prepared packet byte admission");
        u64(packet.size());flush();append(std::move(packet));
    }
    void blob(std::span<const std::byte> bytes){
        shared_packet_detail::bound(machine::checked_size_add(machine::checked_size_add(size_,pending_.size()),machine::checked_size_add(8,bytes.size())),maximum_,"prepared packet byte admission");
        if(work_){shared_packet_detail::bound(machine::checked_size_add(work_->work_bytes(),bytes.size()),work_->limits.max_work_bytes,"prepared packet copy byte work admission");}
        auto packet=PreparedBytePacket::freeze(bytes);if(work_){work_->work(bytes.size());}blob(std::move(packet));
    }
    template<class Owner> void receipt(Owner origin){
        blob(PreparedBytePacket::pin(std::move(origin),[](const auto& held){return std::span<const std::byte>(held.receipt());}));
    }
    PreparedBytePacket finish(std::function<void()> check={}) && {
        flush();std::uint64_t hash=1469598103934665603ULL;
        auto consume=[&](auto chunk){if(work_){work_->work(chunk.size());}hash=rfcx_detail::fnv1a64_update(hash,chunk);};
        const std::function<void()> callback=work_?std::function<void()>{}:std::move(check);
        for(const auto& part:parts_){PreparedBytePacket::visit(*part.child,consume,callback);}
        rfcx_detail::Writer header(32);header.raw(magic_);header.u32(1);header.u32(0);header.u64(size_);header.u64(hash);
        auto first=PreparedBytePacket::freeze(header.bytes());
        std::vector<PreparedBytePacket::Part> all;all.reserve(machine::checked_size_add(parts_.size(),1));
        all.push_back({first.state_,{}, {}});for(auto& part:parts_){all.push_back(std::move(part));}
        return PreparedBytePacket(std::make_shared<const PreparedBytePacket::State>(PreparedBytePacket::State{std::move(all),size_,std::max(depth_,std::size_t{1})}));
    }
private:
    void flush(){
        if(!pending_.size()){return;}
        if(work_){shared_packet_detail::bound(machine::checked_size_add(work_->work_bytes(),pending_.size()),work_->limits.max_work_bytes,"prepared packet copy byte work admission");}
        auto packet=PreparedBytePacket::freeze(pending_.bytes());if(work_){work_->work(pending_.size());}pending_=rfcx_detail::Writer{};append(std::move(packet));
    }
    void append(PreparedBytePacket packet){
        size_=machine::checked_size_add(size_,packet.size());shared_packet_detail::bound(size_,maximum_,"prepared packet byte admission");
        depth_=std::max(depth_,machine::checked_size_add(packet.state_->depth,1));
        shared_packet_detail::bound(depth_,256,"prepared packet nesting admission");
        shared_packet_detail::bound(machine::checked_size_add(parts_.size(),1),1000000,"prepared packet part admission");
        parts_.push_back({std::move(packet.state_),{}, {}});
    }
    std::array<std::byte,8> magic_;
    std::size_t maximum_,size_{32},depth_{};
    shared_packet_detail::Guard* work_{};
    rfcx_detail::Writer pending_;
    std::vector<PreparedBytePacket::Part> parts_;
};
} // namespace rfc::storage
