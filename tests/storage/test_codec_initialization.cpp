#include "rfc/storage/temporal_declaration_codec.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace rfc;
using Q = numeric::Rational;
using Bytes = std::vector<std::byte>;
namespace codec = storage::temporal_codec_detail;
std::size_t checks = 0, permutations = 0, truncations = 0;
void check(bool result, const char* message) {
    ++checks;
    if (!result) throw std::runtime_error(message);
}
template<class F> void rejects(F&& f, const char* message) {
    bool threw = false;
    try { f(); } catch (const std::exception&) { threw = true; }
    check(threw, message);
}
Bytes wire(const storage::LocalBlockDefinition& b) {
    codec::Writer w; codec::local(w,b); return w.bytes();
}
Bytes wire(const storage::TemporalDeclarationState& s) {
    codec::Writer w; codec::declaration_state(w,s); return w.bytes();
}
void save(const std::filesystem::path& p, const Bytes& b) {
    std::ofstream f(p,std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if (!f) throw std::runtime_error("write certificate bytes");
}
storage::LocalExactTensorPayload payload(std::size_t i, std::size_t n = 130) {
    storage::LocalExactTensorPayload p;
    p.payload_key = static_cast<std::uint64_t>(10 + 7*i);
    p.binding = static_cast<storage::LocalPayloadBinding>(i%3);
    if (p.binding == storage::LocalPayloadBinding::Cell)
        p.cell = storage::StableCellKey{core::Grade{1},100+i};
    if (p.binding == storage::LocalPayloadBinding::Occurrence)
        p.occurrence = storage::StableOccurrenceKey{core::Grade{1},core::OccurrenceKey{1000+i}};
    const std::vector<std::size_t> active{0,1,n-1};
    auto support = numeric::StructuralSupport::full(n);
    if (i%3 == 1) support = numeric::StructuralSupport::sparse(n,active);
    if (i%3 == 2) support = numeric::StructuralSupport::packed(n,active);
    numeric::Integer large = 1; large <<= 129;
    Q last(large+static_cast<unsigned long>(i),numeric::Integer{7}); last.canonicalize();
    p.tensor = numeric::SparseRationalTensor(numeric::IndexDomain::linear(n),std::move(support),
        numeric::SparseRationalVector(n,{{0,Q{-2,3}},{n-1,last}}));
    return p;
}
void same_payload(const storage::LocalExactTensorPayload& p,const storage::LocalExactTensorPayload& q) {
    check(p.payload_key==q.payload_key && p.binding==q.binding,"payload identity/binding changed");
    check(p.cell==q.cell && p.occurrence==q.occurrence,"payload owner changed");
    check(p.tensor.domain()==q.tensor.domain(),"domain changed");
    check(p.tensor.support().storage()==q.tensor.support().storage(),"in-memory support encoding changed");
    check(p.tensor.values().entries()==q.tensor.values().entries(),"exact numerical state changed");
    check(p.tensor.state(1)==std::optional<Q>{Q{0}},"existing zero erased");
    check(p.tensor.state(2)==q.tensor.state(2),"absence changed");
}
Bytes payload_tests() {
    static_assert(std::is_nothrow_move_constructible_v<storage::LocalExactTensorPayload>);
    storage::LocalBlockDefinition expected; expected.block_key=9;
    for(std::size_t i=0;i<6;++i) expected.payloads.push_back(payload(i));
    const auto canonical=wire(expected);
    std::array<std::size_t,6> order{0,1,2,3,4,5};
    do {
        storage::LocalBlockDefinition b; b.block_key=expected.block_key;
        for(const auto i:order)b.payloads.push_back(expected.payloads[i]);
        storage::canonicalize_local_block(b);
        check(wire(b)==canonical,"permutation changes canonical bytes");
        for(std::size_t i=0;i<b.payloads.size();++i)same_payload(b.payloads[i],expected.payloads[i]);
        const auto* address=b.payloads.data();storage::canonicalize_local_block(b);
        check(address==b.payloads.data(),"already canonical payload lane was reallocated");
        ++permutations;
    } while(std::next_permutation(order.begin(),order.end()));
    // More than the implementation's small-sort threshold, mixed alternatives.
    storage::LocalBlockDefinition many;many.block_key=10;
    for(std::size_t i=0;i<33;++i)many.payloads.push_back(payload(i));
    const auto many_wire=wire(many);std::mt19937 generator(0xA4);
    for(int round=0;round<24;++round) {
        std::vector<std::size_t> indices(many.payloads.size());
        std::iota(indices.begin(),indices.end(),0);std::shuffle(indices.begin(),indices.end(),generator);
        storage::LocalBlockDefinition b;b.block_key=many.block_key;
        for(const auto i:indices)b.payloads.push_back(many.payloads[i]);
        storage::canonicalize_local_block(b);check(wire(b)==many_wire,"large mixed lane canonicalization");
        storage::TemporalReadLimits limits;codec::Reader r(many_wire,limits);auto decoded=codec::local(r);
        check(r.remaining()==0 && wire(decoded)==many_wire,"local payload wire round trip");
        ++permutations;
    }
    auto duplicate=expected;duplicate.payloads.push_back(expected.payloads.front());
    rejects([&]{storage::canonicalize_local_block(duplicate);},"duplicate key accepted");
    duplicate=expected;duplicate.payloads.insert(duplicate.payloads.begin(),expected.payloads.front());
    rejects([&]{storage::canonicalize_local_block(duplicate);},"sorted duplicate key accepted");
    storage::LocalBlockDefinition empty;storage::canonicalize_local_block(empty);
    check(empty.payloads.empty(),"empty payload lane");
    // Huge declared extents remain metadata; only two numerical entries exist.
    storage::LocalBlockDefinition huge;huge.block_key=11;
    huge.payloads.push_back(payload(1,1000000007));huge.payloads.push_back(payload(0,1000000007));
    storage::canonicalize_local_block(huge);
    check(huge.payloads[0].tensor.domain().logical_size()==1000000007,"large extent not preserved");
    check(huge.payloads[0].tensor.stored_nonzeros()==2 && huge.payloads[1].tensor.support().count()==3,"large support densified");
    const auto bytes=wire(huge);check(bytes.size()<512,"large extent expanded in wire format");
    storage::TemporalReadLimits limits;limits.max_dimension=1000000007;codec::Reader r(bytes,limits);
    check(wire(codec::local(r))==bytes,"large domain wire round trip");
    return many_wire;
}
composite::DeclarationId id(std::uint64_t k) {return {44,k,1};}
storage::TemporalDeclarationState state(std::size_t count) {
    using D=composite::Declaration;using V=composite::DeclaredValue;
    storage::TemporalDeclarationState s;s.namespace_key=91;
    s.registry=composite::DeclarationRegistry({D::boolean(id(1)),D::rational(id(2)),
        D::optional(id(3),id(2)),D::repeated(id(4),id(1),0,64)});
    for(std::size_t i=0;i<count;++i) {
        storage::TemporalDeclaredValue v;v.key=100+i;
        switch ((i/2)%4) {
            case 0:v.type=id(1);v.value=V::boolean(i%2==0);break;
            case 1:{v.type=id(2);Q q{static_cast<long>(i)-20,7};q.canonicalize();v.value=V::rational(q);break;}
            case 2:v.type=id(3);v.value=i%2==0?V::absent():V::present(V::rational(Q{0}));break;
            default:v.type=id(4);v.value=V::repeated({V::boolean(true),V::boolean(false)});break;
        }
        s.values.push_back(std::move(v));
    }
    return s;
}
Bytes declaration_tests() {
    Bytes result;
    for(const auto count:{std::size_t{0},std::size_t{1},std::size_t{2},std::size_t{48}}) {
        const auto expected=state(count);const auto bytes=wire(expected);
        storage::TemporalReadLimits limits;codec::Reader r(bytes,limits);
        const auto decoded=codec::declaration_state(r);
        check(r.remaining()==0 && wire(decoded)==bytes,"declaration canonical round trip");
        check(decoded.namespace_key==expected.namespace_key && decoded.values.size()==count,"declaration identity/population");
        for(std::size_t i=0;i<count;++i) {
            check(decoded.values[i].key==expected.values[i].key && decoded.values[i].type==expected.values[i].type,"declared value identity");
            check(composite::declared_value_equal(decoded.values[i].value,expected.values[i].value),"declared exact value changed");
        }
        for(std::size_t n=0;n<bytes.size();++n) {
            rejects([&]{codec::Reader truncated(std::span<const std::byte>(bytes).first(n),limits);
                (void)codec::declaration_state(truncated);},"truncated declaration accepted");++truncations;
        }
        if(count>0) {
            auto tight=limits;tight.declarations.max_materialized_bytes=1;
            rejects([&]{codec::Reader denied(bytes,tight);(void)codec::declaration_state(denied);},"declaration allocation budget bypassed");
        }
        result.insert(result.end(),bytes.begin(),bytes.end());
    }
    return result;
}
}
int main(int argc,char** argv) {
    try {
        const auto p=payload_tests(),d=declaration_tests();
        if(argc==2) {std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
            save(out/"payloads.bin",p);save(out/"declarations.bin",d);}
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks
            <<",\"payload_orders\":"<<permutations<<",\"rejected_truncations\":"<<truncations
            <<",\"payload_bytes\":"<<p.size()<<",\"declaration_bytes\":"<<d.size()<<"}\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
