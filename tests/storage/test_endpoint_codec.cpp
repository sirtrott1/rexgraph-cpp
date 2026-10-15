#include "declaration_temporal_fixture.hpp"
#include "native_series_fixture.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include <iostream>
#include <random>

#ifndef RFC_TEST_SOURCE_DIR
#define RFC_TEST_SOURCE_DIR "."
#endif
using namespace rfc;
using namespace rfc::storage;
namespace {
std::size_t checks{};
void check(bool yes, const char* message) {
    ++checks; if (!yes) throw std::runtime_error(message);
}
template<class F> void rejects(F&& f, const char* message) {
    bool refused=false; try { f(); } catch (const std::exception&) { refused=true; }
    check(refused,message);
}
inline std::uint64_t reference_hash(const TemporalEndpoint& endpoint) {
    using namespace temporal_codec_detail;
    validate_temporal_endpoint(endpoint);
    Writer w;
    const auto source = encode_rfcx(endpoint.source);
    w.u64(endpoint.sequence); w.u64(rfcx_detail::fnv1a64(source.bytes));
    const StableSourceIndex local_index(endpoint.source);
    const auto blocks = index(endpoint.local_blocks, [](const auto& b) { return b.block_key; });
    w.u64(blocks.size()); for (const auto& [key, b] : blocks) { (void)key; temporal_codec_detail::local(w, temporal_endpoint_detail::normalize_local(endpoint.source, local_index, *b)); }
    const auto cs = temporal_endpoint_detail::carriers(endpoint);
    w.u64(cs.size()); for (const auto& [key, c] : cs) { (void)key; carrier(w, *c); }
    const auto ts = temporal_endpoint_detail::tensors(endpoint);
    w.u64(ts.size()); for (const auto& [key, t] : ts) { (void)key; tensor_record(w, *t); }
    if (!endpoint.finite_systems.empty()) {
        // Domain-separated suffix; omitted for byte-compatible legacy hashes.
        w.u64(0x314554494e494658ULL); // "XFINITE1", little endian
        finite_systems(w, endpoint.finite_systems);
    }
    if (endpoint.codes) {
        w.u64(0x3145444f434c4552ULL); // "RELCODE1", little endian
        code_state(w, *endpoint.codes);
    }
    if (endpoint.declarations) {
        w.u64(0x3153434544584652ULL); // domain-separated declaration channel
        declaration_state(w, *endpoint.declarations);
    }
    return rfcx_detail::fnv1a64(w.bytes());
}

inline std::vector<std::byte> reference_encoding(const TemporalEndpoint& endpoint) {
    using namespace temporal_codec_detail;
    validate_temporal_endpoint(endpoint);
    Writer w; w.raw(temporal_seed_detail::magic); w.u32(1);
    w.u32((endpoint.finite_systems.empty() ? 0u : finite_systems_flag) | (endpoint.codes ? relational_codes_flag : 0u) | (endpoint.declarations ? declarations_flag : 0u)); w.u64(0); w.u64(0);
    const auto source=encode_rfcx(endpoint.source);
    w.u64(endpoint.sequence); w.u64(source.stats.payload_hash); w.u64(rfcx_detail::fnv1a64(source.bytes));
    w.u64(reference_hash(endpoint));
    const StableSourceIndex local_index(endpoint.source);
    const auto blocks=index(endpoint.local_blocks,[](const auto& b){return b.block_key;});
    w.u64(blocks.size());
    for(const auto& [key,b]:blocks){(void)key;temporal_codec_detail::local(w,temporal_endpoint_detail::normalize_local(endpoint.source,local_index,*b));}
    const auto cs=temporal_endpoint_detail::carriers(endpoint);
    w.u64(cs.size());for(const auto& [key,c]:cs){(void)key;carrier(w,*c);}
    const auto ts=temporal_endpoint_detail::tensors(endpoint);
    w.u64(ts.size());for(const auto& [key,t]:ts){(void)key;tensor_record(w,*t);}
    if (!endpoint.finite_systems.empty()) finite_systems(w, endpoint.finite_systems);
    if (endpoint.codes) code_state(w,*endpoint.codes);
    if (endpoint.declarations) declaration_state(w,*endpoint.declarations);
    w.patch_u64(16,w.size());
    w.patch_u64(24,rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));
    return w.bytes();
}

void rehash(std::vector<std::byte>& bytes) {
    const auto hash = rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));
    for (unsigned i=0;i<8;++i) bytes[24+i]=static_cast<std::byte>((hash>>(8*i))&255u);
}
void equivalence() {
    std::mt19937_64 random(7193201);
    for (unsigned flags=0;flags<8;++flags) for(unsigned trial=0;trial<32;++trial) {
        auto e=f2_fixture::endpoint(); e.sequence=random();
        if (!(flags&2u)) e.codes.reset();
        if (!(flags&4u)) e.declarations.reset();
        else if (!e.codes) e.declarations->code_types.clear();
        if (flags&1u) {
            TemporalFiniteSystem system;
            system.key=900; system.domains={{{core::Grade{0},10},{std::int64_t{0},std::int64_t{1}}}};
            system.sections={{901,{{{core::Grade{0},10},std::int64_t{0}}}}};
            e.finite_systems.push_back(system);
        }
        TemporalTensor t; t.key=2; t.interpretation="measured values";
        t.owner.kind=TemporalOwnerKind::Occurrence;
        t.owner.occurrence={core::Grade{1},core::OccurrenceKey{100}};
        numeric::Integer integer=trial+1; mpz_mul_2exp(integer.get_mpz_t(),integer.get_mpz_t(),192);
        if (trial&1u) integer=-integer;
        numeric::Rational value(integer,numeric::Integer{trial+3});value.canonicalize();
        auto support=trial&2u ? numeric::StructuralSupport::sparse(42,{0,1,6,41})
                              : numeric::StructuralSupport::full(42);
        t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain({2,3,7}),std::move(support),
            numeric::SparseRationalVector(42,{{0,value},{41,numeric::Rational{-3,7}}}));
        e.tensors.push_back(std::move(t));
        const auto original=reference_encoding(e);
        auto measured=rfcx_detail::Writer::measure_only();
        const auto wire_source=encode_rfcx(e.source);
        (void)temporal_codec_detail::endpoint_body(measured,e,StableSourceIndex(e.source),rfcx_detail::fnv1a64(wire_source.bytes));
        check(measured.bytes().empty() && measured.size()==original.size()-64,
              "measured channels differ from original wire extent");
        check(temporal_endpoint_hash(e)==reference_hash(e),"channel hash differs from original serialization");
        check(encode_temporal_endpoint(e)==original,"canonical sidecar differs from original serialization");
        auto decoded=temporal_seed_detail::decode(e.source,original,{});
        check(decoded.encoding.channels==original,"retained canonical channel bytes differ");
        check(decoded.encoding.channels.capacity()==original.size(),"reader retains unused channel capacity");
        check(decoded.encoding.source.bytes==encode_rfcx(e.source).bytes,"retained source bytes differ");
        check(reference_encoding(decoded.endpoint)==original,"decoded coefficients or declarations differ");
        check(decoded.endpoint.tensors.at(1).tensor.state(1)==std::optional<numeric::Rational>{0},"supported zero lost");
        check(decoded.endpoint.tensors.at(1).tensor.support().contains(2)==!(trial&2u),"absence changed");
        for (const auto offset:{32u,40u,48u,56u}) {
            auto wrong=original;wrong[offset]^=std::byte{1};rehash(wrong);
            rejects([&]{(void)decode_temporal_endpoint(e.source,wrong);},"forged endpoint binding accepted");
        }
        TemporalReadLimits limit;limit.max_file_bytes=original.size()-1;
        rejects([&]{(void)decode_temporal_endpoint(e.source,original,limit);},"sidecar byte limit bypassed");
        limit={};limit.max_records=0;
        rejects([&]{(void)decode_temporal_endpoint(e.source,original,limit);},"record limit bypassed");
    }
}
void historical() {
    for (const auto name:{"phase03_v1","phase03c_codes"}) {
        const auto path=std::filesystem::path(RFC_TEST_SOURCE_DIR)/"tests/storage/fixtures"/name;
        MappedRfcx source(path/"base.rfcx");
        const auto wire=read_file_exact(path/"base.rfcxe", TemporalReadLimits{}.max_file_bytes);
        auto e=decode_temporal_endpoint(source.materialize(),wire);
        check(reference_encoding(e)==wire,"historical fixture differs from original encoding");
        check(encode_temporal_endpoint(e)==wire,"historical fixture canonical bytes changed");
    }
}
void archive_ownership() {
    auto e=f2_fixture::endpoint();
    NativeTemporalSeries history(e,910);
    auto wire=encode_native_series(history);
    auto loaded=decode_native_series(wire);
    const auto expected=wire;
    wire.clear();wire.shrink_to_fit();
    check(encode_native_series(loaded)==expected,"reader retains caller byte storage");
    check(loaded.at(loaded.last_sequence()).pin()==loaded.at(e.sequence).pin(),"base endpoint duplicated");
    loaded.visit_components([&](const NativeSeriesComponent& part) {
        check(part.bytes->capacity()==part.bytes->size(),"component retains unused allocation capacity");
    });
    const auto tail=loaded.at(loaded.last_sequence()).pin();
    const auto source_wire=encode_rfcx(tail->source).bytes;
    check(source_wire==encode_rfcx(e.source).bytes,"owning source changed");
}
void identity_source() {
    const auto source=f2_fixture::source();
    TemporalSourcePatch patch;patch.target_grade_count=static_cast<std::uint32_t>(source.grade_count());
    std::vector<temporal_source_detail::GradeImage> original;
    for (std::size_t k=0;k<source.grade_count();++k) original.push_back(temporal_source_detail::capture(source,k));
    const auto rebuilt=temporal_source_detail::materialize_images(original,source.signature(),source.semantic_scale());
    const auto replayed=replay_temporal_source(source,patch);
    check(encode_rfcx(replayed).bytes==encode_rfcx(rebuilt).bytes,"identity source differs from full occurrence reconstruction");
    check(encode_rfcx(replayed).bytes==encode_rfcx(source).bytes,"identity source changes the canonical complex");
    patch.target_grade_count=0;
    rejects([&]{(void)replay_temporal_source(source,patch);},"identity path accepts empty grade domain");
    patch.target_grade_count=65537;
    rejects([&]{(void)replay_temporal_source(source,patch);},"identity path accepts out of range grade domain");
    patch.target_grade_count=1;
    rejects([&]{(void)replay_temporal_source(source,patch);},"identity path discards nonempty higher grades");
    patch.target_grade_count=static_cast<std::uint32_t>(source.grade_count()+1);
    check(replay_temporal_source(source,patch).grade_count()==source.grade_count()+1,"empty grade birth incorrectly treated as identity");
    patch.target_grade_count=static_cast<std::uint32_t>(source.grade_count());
    TemporalGradePatch invalid;invalid.grade=core::Grade{1};invalid.removed_occurrences={123456};patch.grades.push_back(invalid);
    rejects([&]{(void)replay_temporal_source(source,patch);},"identity path ignores actual source edits");
}
void carrier_identity() {
    using M=numeric::ExactMatrix;using Q=numeric::Rational;
    check(M::identity(0).is_identity(),"empty identity rejected");
    check(M::identity(2).is_identity(),"complete identity rejected");
    check(!M(2,3).is_identity(),"rectangular map called identity");
    check(!M::diagonal({1,0}).is_identity(),"rank deficient map called identity");
    check(!M::diagonal({2,1}).is_identity(),"scaling called identity");
    check(!M::from_rows({{0,1},{1,0}}).is_identity(),"permutation called identity");
    for (const auto& map:{M::identity(2),M::diagonal({2,3}),M::from_rows({{0,1},{1,0}})}) {
        const auto before=native_series_fixture::endpoint(0,{5,-7});
        auto after=before;after.sequence=1;
        const auto expected=map.apply(numeric::RationalVector{5,-7});
        after.tensors[0]=native_series_fixture::field_tensor(10,1,expected);
        const auto segment=make_temporal_segment(before,after,{{1,map}});
        check(segment.tensors.empty(),"transport without innovation creates field replacement");
        const auto replayed=replay_temporal_segment(before,segment);
        check(reference_encoding(replayed)==reference_encoding(after),"record free field transport changed");
        check(replayed.tensors[0].tensor.state(0)==std::optional<Q>{expected[0]},"first transport coordinate changed");
        check(replayed.tensors[0].tensor.state(1)==std::optional<Q>{expected[1]},"second transport coordinate changed");
    }
}
void canonical_policy() {
    core::RelationalComplexBuilder builder(core::RelationSignature{});builder.declare_witness_identity(10);
    TemporalEndpoint e(builder.freeze());
    for (const auto key:{1u,2u}) {
        TemporalTensor t;t.key=key;t.interpretation="measured value";
        t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(1),
            numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{key}}}));
        e.tensors.push_back(std::move(t));
    }
    const auto original=reference_encoding(e);auto unordered=original;
    const TemporalReadLimits policy;
    temporal_codec_detail::Reader r(std::span<const std::byte>(original).subspan(64),policy);
    check(r.count()==0 && r.count()==0 && r.count()==2,"unexpected test record offsets");
    const auto first=original.size()-r.remaining();
    (void)temporal_codec_detail::tensor_record(r);
    const auto second=original.size()-r.remaining();
    (void)temporal_codec_detail::tensor_record(r);
    const auto end=original.size()-r.remaining();
    std::copy(original.begin()+static_cast<std::ptrdiff_t>(second),original.begin()+static_cast<std::ptrdiff_t>(end),
              unordered.begin()+static_cast<std::ptrdiff_t>(first));
    std::copy(original.begin()+static_cast<std::ptrdiff_t>(first),original.begin()+static_cast<std::ptrdiff_t>(second),
              unordered.begin()+static_cast<std::ptrdiff_t>(first+end-second));
    rehash(unordered);
    rejects([&]{(void)decode_temporal_endpoint(e.source,unordered);},"unordered canonical records accepted");
    TemporalReadLimits relaxed;relaxed.require_canonical=false;
    auto admitted=decode_temporal_endpoint(e.source,unordered,relaxed);
    check(reference_encoding(admitted)==original,"explicit noncanonical sidecar policy changed values");
    auto wire=encode_native_series(NativeTemporalSeries(e,500));
    temporal_codec_detail::Reader archive(std::span<const std::byte>(wire).subspan(32),policy);
    (void)archive.u64();(void)archive.blob();const auto channels=archive.blob();
    check(channels.size()==unordered.size(),"unexpected series channel length");
    std::copy(unordered.begin(),unordered.end(),wire.begin()+(channels.data()-wire.data()));rehash(wire);
    NativeSeriesLimits limits;limits.temporal.require_canonical=false;
    rejects([&]{(void)decode_native_series(wire,limits);},"series canonical policy weakened by caller");
    for(std::size_t n=0;n<original.size();++n)
        rejects([&]{(void)decode_temporal_endpoint(e.source,std::span<const std::byte>(original).first(n));},"truncated sidecar accepted");
}
void byte_writer() {
    rfcx_detail::Writer w(3);w.u8(1);w.u8(2);w.u8(3);
    const auto* allocation=w.bytes().data();
    auto bytes=std::move(w).take_bytes();
    check(bytes.data()==allocation && bytes.size()==3,"writer transfer copies allocation");
    std::uint64_t hash=1469598103934665603ull;
    for(const auto b:bytes) hash=rfcx_detail::fnv1a64_update(hash,std::span<const std::byte>(&b,1));
    check(hash==rfcx_detail::fnv1a64(bytes),"incremental byte hash differs");
    const auto exercise=[](auto& out) {
        out.u8(7);out.align(8);const auto offset=out.reserve(8);
        out.u16(9);out.u32(17);out.u64(22);out.i64(-101);
        out.string(std::string_view("a\0b",3));out.patch_byte(0,std::byte{4});
        out.patch_u32(offset,3);out.patch_u64(offset,33);
        rfcx_detail::write_bigint(out,numeric::Integer{0});
        rfcx_detail::write_bigint(out,numeric::Integer{-7781});
        rfcx_detail::write_rational(out,numeric::Rational{-173,41});
    };
    rfcx_detail::Writer actual;auto measured=rfcx_detail::Writer::measure_only();exercise(actual);exercise(measured);
    check(measured.size()==actual.size() && measured.bytes().empty(),"writer measured bytes differ");
    rejects([&]{(void)measured.append_zeroed(1);},"measuring writer exposed a payload region");
    auto overflow=rfcx_detail::Writer::measure_only();(void)overflow.reserve(std::numeric_limits<std::size_t>::max());
    rejects([&]{overflow.u8(1);},"measured extent overflow accepted");
    check(overflow.size()==std::numeric_limits<std::size_t>::max(),"failed extent measurement changed position");
}
} // namespace
int main() {
    try { equivalence(); historical(); archive_ownership(); identity_source(); carrier_identity(); canonical_policy(); byte_writer();
        std::cout << "endpoint_codec_checks=" << checks << " PASS\n";return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
