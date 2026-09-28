// Phase 03F0: rejection/preservation regressions, not reproduction-as-success.
#include "rfc/storage/temporal_seed.hpp"
#include "rfc/algebra/materialize.hpp"
#include "rfc/numeric/integer_matrix.hpp"
#include "rfc/machine/bitsliced_int.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>

using namespace rfc;
using Q=numeric::Rational;
using Bytes=std::vector<std::byte>;
namespace fs=std::filesystem;
namespace {
std::size_t checks=0;
void check(bool c,const char* message) { ++checks; if (!c) throw std::runtime_error(message); }
template<class F> void rejects(F&& f,const char* message) {
    bool threw=false; try { f(); } catch (const std::exception&) { threw=true; } check(threw,message);
}
void put64(Bytes& b,std::size_t p,std::uint64_t value) {
    if (p>b.size() || b.size()-p<8) throw std::runtime_error("test patch range");
    for (unsigned j=0;j<8;++j) b[p+j]=static_cast<std::byte>((value>>(j*8u))&255u);
}
std::uint64_t get64(const Bytes& b,std::size_t p) { return storage::rfcx_detail::load_u64_at(b,p); }
void save(const fs::path& p,const Bytes& b) {
    std::ofstream f(p,std::ios::binary|std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if (!f) throw std::runtime_error("test file write");
}
Bytes load(const fs::path& p) {
    std::ifstream f(p,std::ios::binary); f.seekg(0,std::ios::end); const auto n=f.tellg();
    if (n<0) throw std::runtime_error("test file read");
    Bytes b(static_cast<std::size_t>(n)); f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);
    if (!f) throw std::runtime_error("test file read");
    return b;
}
void rehash(Bytes& b,bool local=false) {
    const auto header=local ? storage::local_rfcx_detail::header_size : storage::rfcx_detail::header_size;
    put64(b,local?128:72,storage::rfcx_detail::fnv1a64(std::span<const std::byte>(b).subspan(header)));
}
void rejects_source(Bytes b,const fs::path& out,bool integrity=true) {
    rehash(b); save(out/"rejected.rfcx",b);
    rejects([&]{storage::MappedRfcx m(out/"rejected.rfcx",integrity);},"malformed RFCX accepted");
}
void rejects_local(Bytes b,const fs::path& out) {
    rehash(b,true);save(out/"rejected.rfcxl",b);
    rejects([&]{storage::MappedLocalBlocks m(out/"rejected.rfcxl");},"malformed local image accepted");
}
core::RelationalComplex masked(bool changed=false) {
    core::RelationSignature s;const auto flag=s.aspects().add("retained-role");
    const auto type=s.add_type("generic");core::RelationalComplexBuilder b(s);
    b.declare_witness_identity(90);
    b.add_grade_one(core::RelationKey{50},type,{
        {1,true,false,Q{1,2},{},core::OccurrenceKey{11}},
        {2,false,true,Q{0},{{flag,!changed}},core::OccurrenceKey{12}}});
    b.add_grade_one(core::RelationKey{40},type,{});
    b.add_higher(core::Grade{2},core::RelationKey{200},type,{
        {0,false,true,Q{0},{{flag,true}},core::OccurrenceKey{21}}});
    b.declare_grade(core::Grade{4});return b.freeze();
}
algebra::SubcomplexMask all_cells(const core::RelationalComplex& source) {
    algebra::SubcomplexMask m(source);
    for (std::uint16_t k=0;k<source.grade_count();++k)
        for (std::uint32_t i=0;i<source.grade(core::Grade{k}).cell_count();++i) m.select({core::Grade{k},i});
    return m;
}
core::RelationalComplex surface(bool reordered,bool ordered=true,bool arena_reordered=false) {
    core::RelationSignature s;const auto edge=s.add_type("loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=ordered?s.add_attaching_loop_type("face",4,4):s.add_type("face",core::BoundaryPolicy::DeclaredExact,4,4);
    core::RelationalComplexBuilder b(s);
    for (unsigned n=0;n<2;++n) {
        const auto logical=arena_reordered?1u-n:n;
        b.add_grade_one(core::RelationKey{20+logical},edge,{
            {1,true,true,Q{1},{},core::OccurrenceKey{10+2*logical}},
            {1,true,false,Q{1},{},core::OccurrenceKey{11+2*logical}}});
    }
    auto item=[&](std::uint32_t i,bool neg,std::uint64_t key){
        return core::HigherIncidenceInput{arena_reordered?1u-i:i,true,neg,Q{1},{},core::OccurrenceKey{key}};
    };
    std::vector<core::HigherIncidenceInput> incidences;
    if (!reordered) incidences={item(0,false,100),item(1,false,101),item(0,true,102),item(1,true,103)};
    else incidences={item(0,false,100),item(0,true,102),item(1,false,101),item(1,true,103)};
    b.add_higher(core::Grade{2},core::RelationKey{30},face,incidences);return b.freeze();
}
void machine_tests() {
    using machine::PackedBits;
    const auto max=std::numeric_limits<std::size_t>::max();
    check(PackedBits::words_for_size(max)==max/64u+1u,"overflow-free ceil size");
    for (std::size_t d=0;d<130;++d) {
        rejects([&]{PackedBits b(max-d);},"huge packed allocation accepted");
        rejects([&]{composite::AspectPlanes p(composite::AspectSchema::canonical(),max-d);},"huge composite allocation accepted");
    }
    rejects([&]{(void)machine::checked_size_add(max,1);},"checked add wrapped");
    rejects([&]{(void)machine::checked_size_mul(max,2);},"checked multiply wrapped");
    rejects([&]{(void)machine::ceil_div(1,0);},"ceil zero divisor");
    check(machine::checked_size_add(max,0)==max && machine::checked_size_mul(max,0)==0,"checked size legal limits");
    rejects([&]{numeric::IntegerMatrix m(max,2);},"integer matrix product wrapped");
    rejects([&]{numeric::IndexDomain d({max,2});},"tensor shape product wrapped");
    check(numeric::IndexDomain({0,129}).logical_size()==0,"empty tensor domain");
    rejects([&]{PackedBits p(65,false,1);},"word budget ignored");
    check(PackedBits(64,true,1).count()==64,"exact word budget rejected");
    rejects([&]{composite::AspectPlanes p(composite::AspectSchema::canonical(),65,3);},"total aspect budget ignored");
    for (auto n:std::vector<std::size_t>{0,1,2,31,63,64,65,127,128,129,130,255}) {
        PackedBits a(n),b(n),mask(n);
        for (std::size_t i=0;i<n;++i) {a.set(i,i%3==0);b.set(i,i%5<2);mask.set(i,i%2==0);}
        const auto both=a&b,either=a|b,xorv=a^b,inverse=~a,sel=PackedBits::select(mask,a,b);
        for (std::size_t i=0;i<n;++i) {
            check(both.get(i)==(a.get(i)&&b.get(i)),"packed and");
            check(either.get(i)==(a.get(i)||b.get(i)),"packed or");
            check(xorv.get(i)==(a.get(i)!=b.get(i)),"packed xor");
            check(inverse.get(i)!=a.get(i),"packed not");
            check(sel.get(i)==(mask.get(i)?a.get(i):b.get(i)),"packed select");
        }
        check(PackedBits(n,true).count()==n,"packed tail count");
        if (n%64) check((inverse.word(inverse.word_count()-1)>>(n%64))==0,"tail padding");
        rejects([&]{(void)a.get(n);},"packed logical bound");
        const auto count=a.count();auto copy=a;auto moved=std::move(copy);
        check(moved.count()==count && copy.size()==0 && copy.word_count()==0,"packed move invariant");
        copy=std::move(moved);check(copy.count()==count && moved.size()==0,"packed move assignment");
    }
    rejects([&]{machine::BitSlicedInt<63> enormous(max/64u);},"aggregate sliced allocation budget ignored");
    auto sliced=machine::BitSlicedInt<4>::constant(65,3);auto moved_sliced=std::move(sliced);
    check(sliced.size()==0 && moved_sliced.value(64)==3,"bit-sliced move invariant");
    composite::AspectPlanes planes(composite::AspectSchema::canonical(),65);auto moved_planes=std::move(planes);
    check(planes.slots()==0 && planes.aspect_count()==0 && moved_planes.slots()==65,"aspect-plane move invariant");
    for (std::int64_t x=-8;x<8;++x) for (std::int64_t y=-8;y<8;++y) {
        const auto a=machine::BitSlicedInt<4>::constant(65,x),b=machine::BitSlicedInt<4>::constant(65,y),c=a.add_mod(b);
        const auto raw=static_cast<unsigned>(x+y)&15u;
        check(c.value(64)==(raw>=8?static_cast<std::int64_t>(raw)-16:raw),"bitsliced addition");
        check(a.signed_overflow(b,c).get(64)==(x+y< -8 || x+y>7),"bitsliced overflow");
    }
    composite::BinaryProgram op(1);op.append({composite::BinaryOpcode::LoadOne,0});
    check(op.execute_domain_word_with_provider(65,1,0,[](auto,auto){return std::uint64_t{0};})==1,"domain provider mask");
    rejects([&]{op.append({static_cast<composite::BinaryOpcode>(255),0});},"unsupported machine opcode");
    rejects([&]{composite::BinaryProgram p(0);},"empty register file");
    rejects([&]{op.append({composite::BinaryOpcode::Not,0,1});},"invalid register accepted");
    storage::rfcx_detail::Writer writer;writer.u8(1);
    rejects([&]{writer.reserve(max);},"writer reserve wrapped");check(writer.size()==1,"failed reserve mutated writer");
}
void declaration_tests() {
    for (unsigned k=0;k<256;++k) {
        core::RelationSignature s;core::RelationConstraints c;
        c.rules.push_back({static_cast<core::RelationRuleKind>(k),s.existence_aspect(),0,Q{0}});
        if (k<6) check(s.add_type("rule",core::BoundaryPolicy::DeclaredExact,0,0,c)==0,"known rule rejected");
        else rejects([&]{s.add_type("rule",core::BoundaryPolicy::DeclaredExact,0,0,c);},"unknown executable rule admitted");
        composite::AspectSchema a;
        if (k<=8) check(a.add("a",static_cast<composite::AspectRole>(k))==0,"known aspect role");
        else rejects([&]{a.add("a",static_cast<composite::AspectRole>(k));},"unknown aspect role");
    }
    core::RelationSignature s;
    rejects([&]{s.add_type("bad",static_cast<core::BoundaryPolicy>(255));},"unknown boundary policy");
    core::RelationConstraints c;c.shape=static_cast<core::RelationShape>(255);
    rejects([&]{s.add_type("bad",core::BoundaryPolicy::DeclaredExact,0,0,c);},"unknown shape");
    rejects([&]{s.add_type("bad",core::BoundaryPolicy::DeclaredExact,0,0,{},static_cast<core::CompositeConstructorKind>(255));},"unknown constructor");
    // Decode the actual unknown-rule signature, even though creation now rejects it.
    core::RelationConstraints good;good.rules.push_back(core::RelationRule::aspect_implies_existence(s.existence_aspect()));
    s.add_type("checked",core::BoundaryPolicy::DeclaredExact,0,0,good);
    storage::rfcx_detail::Writer w;storage::rfcx_detail::write_signature(w,s);auto bytes=w.bytes();
    storage::rfcx_detail::Reader r(bytes);const auto na=r.u32();
    for (std::uint32_t i=0;i<na;++i) { (void)r.string();(void)r.u8();(void)r.u8(); }
    (void)r.u32();(void)r.string();const auto policy_at=r.position();(void)r.u8();(void)r.u64();(void)r.u64();
    const auto shape_at=r.position();(void)r.u8();(void)r.u8();(void)r.u16();const auto constructor_at=r.position();(void)r.u8();
    (void)r.u32();const auto rule_at=r.position();
    for (const auto pos:{policy_at,shape_at,constructor_at,rule_at}) {
        auto bad=bytes;bad[pos]=std::byte{255};rejects([&]{(void)storage::rfcx_detail::read_signature(bad,0);},"unsupported declaration decoded");
    }
    const auto a=surface(false),b=surface(true);
    rejects([&]{algebra::RelationalMorphism m(a,b,{{0},{0,1},{0}});},"changed attaching word accepted as strict");
    const auto moved=surface(false,true,true);
    algebra::RelationalMorphism preserves(a,moved,{{0},{1,0},{0}});
    check(preserves.is_strict() && preserves.is_chain_map(),"arena reorder rejected");
    const auto u=surface(false,false),v=surface(true,false);
    algebra::RelationalMorphism unordered(u,v,{{0},{0,1},{0}});
    check(unordered.is_chain_map(),"unordered generic incidence incorrectly constrained");
    temporal::RelationalSourceDelta delta(a,b);check(!delta.source_structure_unchanged(),"temporal attaching order lost");
}
void preservation_tests(const fs::path& out) {
    const auto source=masked();const auto mask=all_cells(source);
    const auto canonical=storage::encode_rfcx(source).bytes;save(out/"retained.rfcx",canonical);
    const auto result=algebra::materialize_subcomplex(source,mask);
    check(storage::encode_rfcx(result.complex).bytes==canonical,"full materialization lost retained source");
    check(result.complex.grade_count()==5,"empty grades lost");
    check(result.complex.grade(core::Grade{1}).incidence_count()==2 && result.complex.grade(core::Grade{2}).incidence_count()==1,"inactive occurrences lost");
    algebra::SubcomplexMask active(source);active.select({core::Grade{1},0});active.close_active_boundaries(source);
    check(active.active_boundary_closed(source) && !active.boundary_closed(source),"active vs retained closure not separated");
    rejects([&]{(void)algebra::materialize_subcomplex(source,active);},"lossy active selection materialized as retained");
    rejects([&]{(void)storage::make_strict_local_block(source,99,active);},"active-only block accepted as strict");
    active.close_boundaries(source);check(active.contains({core::Grade{0},2}),"inactive participant not closed");
    const auto pushout=algebra::pushout_inside(source,mask,active);
    check(storage::encode_rfcx(pushout.glued.complex).bytes==canonical,"gluing lost retained data");
    storage::LocalExactTensorPayload payload;payload.payload_key=300;payload.binding=storage::LocalPayloadBinding::Occurrence;
    payload.occurrence=storage::StableOccurrenceKey{core::Grade{1},core::OccurrenceKey{12}};
    auto block=storage::make_strict_local_block(source,1,mask,{payload});
    auto block2=storage::make_strict_local_block(source,2,mask);
    check(block.occurrences.size()==3,"strict block omitted inactive fiber");
    storage::write_local_blocks(source,storage::encode_rfcx(source).stats.payload_hash,{block,block2},out/"retained.rfcxl");
    storage::MappedRfcx mapped(out/"retained.rfcx");storage::MappedLocalBlocks local(out/"retained.rfcxl");
    local.validate_against(mapped);check(local.block(0).occurrence_count()==0,"strict block copied occurrence references");
    check(local.block(0).payload(0).occurrence==payload.occurrence,"inactive payload owner lost");
    storage::TemporalEndpoint old(source),next(masked(true),1);old.local_blocks={block};next.local_blocks={block};
    const auto seed=storage::encode_temporal_endpoint(old),segment=storage::encode_temporal_segment(storage::make_temporal_segment(old,next));
    save(out/"retained.rfcxe",seed);save(out/"retained.rfcxt",segment);
    const auto decoded=storage::decode_temporal_endpoint(mapped.materialize(),seed);
    check(decoded.local_blocks[0].occurrences.size()==3,"endpoint normalization dropped inactive fibers");
    const auto replayed=storage::replay_temporal_segment(decoded,storage::decode_temporal_segment(segment));
    check(storage::encode_temporal_endpoint(replayed)==storage::encode_temporal_endpoint(next),"retained local temporal replay mismatch");
    check(storage::encode_rfcx(replayed.source).bytes==storage::encode_rfcx(next.source).bytes,"temporal source bits changed");
    save(out/"retained_target.rfcx",storage::encode_rfcx(replayed.source).bytes);
    static_assert(!std::is_copy_constructible_v<storage::MappedRfcx>);
    static_assert(!std::is_copy_constructible_v<storage::MappedLocalBlocks>);
    auto view=mapped.grade(core::Grade{1});auto copied=view;
    storage::MappedRfcx moved(std::move(mapped));check(copied.relation_key(0).value==50,"view failed across owner move");
    check(mapped.grade_count()==0,"moved-from mapping retained grades");
    auto expired=moved.grade(core::Grade{1});storage::MappedRfcx replacement(out/"retained.rfcx");
    moved=std::move(replacement);rejects([&]{(void)expired.relation_key(0);},"replaced mapping view did not expire");
    storage::MappedRfcx::GradeView dead;
    {storage::MappedRfcx temporary(out/"retained.rfcx");dead=temporary.grade(core::Grade{1});}
    rejects([&]{(void)dead.lower_index(0);},"destroyed source view did not expire");
    storage::MappedRfcx::GradeView empty;rejects([&]{(void)empty.cell_scale(0);},"default source view accessed owner");
    auto local_view=local.block(0);storage::MappedLocalBlocks local_moved(std::move(local));
    check(local_view.cell(0)==local_moved.block(0).cell(0),"local view failed after move");
    storage::MappedLocalBlocks local_new(out/"retained.rfcxl");local_moved=std::move(local_new);
    rejects([&]{(void)local_view.cell(0);},"replaced local view did not expire");
    storage::MappedLocalBlocks::BlockView local_dead;
    {storage::MappedLocalBlocks tmp(out/"retained.rfcxl");local_dead=tmp.block(0);}
    rejects([&]{(void)local_dead.payload(0);},"destroyed local view did not expire");
    storage::ReadOnlyMappedFile bytes(out/"retained.rfcx");auto byte_moved=std::move(bytes);
    check(bytes.empty() && byte_moved.size()==canonical.size(),"mapped file move state");
    byte_moved.close();check(byte_moved.empty() && byte_moved.data()==nullptr,"mapped file close state");
    byte_moved.close();byte_moved.open(out/"retained.rfcx");check(byte_moved.size()==canonical.size(),"mapped file reopen");
}
void mapped_validation_tests(const fs::path& out) {
    const auto base=load(out/"retained.rfcx");const auto dir=get64(base,56),d1=dir+storage::rfcx_detail::grade_descriptor_size;
    // Every address-bearing positive-grade field, including optional indexes.
    for (const auto field: {6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u,20u,21u,22u}) {
        for (const auto offset: {std::uint64_t{4},std::numeric_limits<std::uint64_t>::max()-3u,std::numeric_limits<std::uint64_t>::max()}) {
            auto bad=base;put64(bad,d1+field*8u,offset);rejects_source(std::move(bad),out);
        }
    }
    for (const auto field:{1u,2u,3u,4u}) {
        auto bad=base;put64(bad,d1+field*8u,std::numeric_limits<std::uint64_t>::max());rejects_source(std::move(bad),out);
    }
    for (const auto pos:{40u,48u,56u,64u}) {
        auto bad=base;put64(bad,pos,std::numeric_limits<std::uint64_t>::max());rejects_source(std::move(bad),out);
    }
    {auto bad=base;put64(bad,d1+6u*8u,get64(base,d1+8u*8u));rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,80,1);rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,d1+6u*8u,std::numeric_limits<std::uint64_t>::max()-3u);rejects_source(std::move(bad),out,false);}
    const auto lower=get64(base,d1+11u*8u),upper=get64(base,d1+12u*8u);
    {auto bad=base;bad[lower]=std::byte{255};rejects_source(std::move(bad),out);}
    {auto bad=base;bad[upper]=std::byte{1};rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,get64(base,d1+10u*8u),1);rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,get64(base,d1+16u*8u),1);rejects_source(std::move(bad),out);}
    {auto bad=base;const auto reverse=get64(base,d1+17u*8u);put64(bad,reverse+8u,get64(base,reverse));rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,get64(base,d1+18u*8u),get64(base,get64(base,d1+18u*8u))|(std::uint64_t{1}<<63));rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,get64(base,d1+8u*8u),0);rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,get64(base,d1+20u*8u)+8u,100);rejects_source(std::move(bad),out);}
    {auto bad=base;const auto types=get64(base,d1+22u*8u);bad[types+4]=std::byte{255};rejects_source(std::move(bad),out);}
    {auto bad=base;put64(bad,get64(base,d1+13u*8u)+8u,11);rejects_source(std::move(bad),out);}
    // Grade descriptor codec retains 23 independent u64 fields without struct pointer arithmetic.
    storage::rfcx_detail::Writer w;w.reserve(storage::rfcx_detail::grade_descriptor_size);
    for (std::size_t i=0;i<23;++i) w.patch_u64(i*8u,0x1234000000000000ull+i);
    const auto desc=storage::rfcx_detail::read_grade_descriptor(w.bytes(),0);
    storage::rfcx_detail::Writer back;back.reserve(storage::rfcx_detail::grade_descriptor_size);storage::rfcx_detail::patch_grade_descriptor(back,0,desc);
    check(w.bytes()==back.bytes(),"grade descriptor member codec mismatch");
    // Explicit empty source grades have legitimate zero-length sections.
    core::RelationSignature empty_signature;core::RelationalComplexBuilder eb(empty_signature);eb.declare_grade(core::Grade{5});
    const auto empty=storage::encode_rfcx(eb.freeze()).bytes;save(out/"empty.rfcx",empty);
    storage::MappedRfcx em(out/"empty.rfcx");check(em.grade_count()==6,"empty declared source grades rejected");
    check(storage::encode_rfcx(em.materialize()).bytes==empty,"empty source canonical bytes changed");
    // Big integer overflow directories and blobs are bounded, ordered and complete.
    core::RelationSignature s;const auto type=s.add_type("exact");core::RelationalComplexBuilder b(s);
    numeric::Integer huge=1;huge<<=130;const Q fractional=Q{1}/Q{huge};
    b.add_grade_one(core::RelationKey{9},type,{{42,true,false,Q{huge},{},core::OccurrenceKey{80}},
                                            {43,true,true,fractional,{},core::OccurrenceKey{70}}});
    const auto overflow=storage::encode_rfcx(b.freeze()).bytes;save(out/"overflow.rfcx",overflow);
    storage::MappedRfcx om(out/"overflow.rfcx");check(storage::encode_rfcx(om.materialize()).bytes==overflow,"large exact integer roundtrip");
    const auto od=get64(overflow,56)+storage::rfcx_detail::grade_descriptor_size;
    const auto table=get64(overflow,od+15u*8u);check(table!=0,"overflow fixture lacks table");
    for (const auto pos:{table,table+8u,table+16u}) {
        auto bad=overflow;put64(bad,pos,std::numeric_limits<std::uint64_t>::max());rejects_source(std::move(bad),out);
    }
    {auto bad=overflow;bad[table+28u]=std::byte{2};rejects_source(std::move(bad),out);}
    {auto bad=overflow;put64(bad,od+15u*8u,0);rejects_source(std::move(bad),out);}
    {auto bad=overflow;put64(bad,table+32u,1);rejects_source(std::move(bad),out);}
    for (std::size_t cut=0;cut<base.size();++cut) {
        Bytes truncated(base.begin(),base.begin()+static_cast<std::ptrdiff_t>(cut));
        // Repaired length/checksum removes the trivial integrity-only rejection path.
        if (cut>=storage::rfcx_detail::header_size) {put64(truncated,16,cut);rehash(truncated);}
        save(out/"truncated.rfcx",truncated);
        rejects([&]{storage::MappedRfcx tmp(out/"truncated.rfcx");},"truncated source accepted");
    }
    std::cout<<"source_truncations_rejected="<<base.size()<<'\n';
    const auto local=load(out/"retained.rfcxl");
    for (const auto pos:{24u,32u,40u,48u,56u,64u,72u,80u,88u,96u,104u,112u,120u}) {
        auto bad=local;put64(bad,pos,std::numeric_limits<std::uint64_t>::max());rejects_local(std::move(bad),out);
    }
    const auto ldir=get64(local,56);
    for (const auto pos:{ldir+8u,ldir+16u,ldir+24u,ldir+32u,ldir+40u,ldir+48u,ldir+56u}) {
        auto bad=local;put64(bad,pos,std::numeric_limits<std::uint64_t>::max());rejects_local(std::move(bad),out);
    }
    {auto bad=local;put64(bad,64,8u);rejects_local(std::move(bad),out);}
    {auto bad=local;put64(bad,64,get64(local,56));rejects_local(std::move(bad),out);}
    {auto bad=local;put64(bad,get64(local,112)+8u,std::numeric_limits<std::uint64_t>::max());rejects_local(std::move(bad),out);}
    {auto bad=local;put64(bad,get64(local,120),999u);rejects_local(std::move(bad),out);}
    {auto bad=local;put64(bad,get64(local,80)+8u,get64(local,88)-1u);rejects_local(std::move(bad),out);}
    {auto bad=local;bad[get64(local,88)+8u]=std::byte{255};rejects_local(std::move(bad),out);}
    {auto bad=local;put64(bad,get64(local,88)+48u,std::numeric_limits<std::uint64_t>::max());rejects_local(std::move(bad),out);}
    for (std::size_t cut=0;cut<local.size();++cut) {
        Bytes truncated(local.begin(),local.begin()+static_cast<std::ptrdiff_t>(cut));
        if (cut>=storage::local_rfcx_detail::header_size) {put64(truncated,16,cut);rehash(truncated,true);}
        save(out/"truncated.rfcxl",truncated);
        rejects([&]{storage::MappedLocalBlocks tmp(out/"truncated.rfcxl");},"truncated local image accepted");
    }
    std::cout<<"local_truncations_rejected="<<local.size()<<'\n';
    storage::write_local_blocks(eb.freeze(),storage::encode_rfcx(eb.freeze()).stats.payload_hash,{},out/"empty.rfcxl");
    storage::MappedLocalBlocks empty_local(out/"empty.rfcxl");empty_local.validate_against(em);check(empty_local.block_count()==0,"empty local image rejected");
    // Deterministic mutation exploration: accepted bytes are not automatically errors;
    // a changed custom aspect can be a valid different retained source.
    std::mt19937_64 random(0x03f0);std::size_t parsed=0,rebuilt=0;
    for (std::size_t i=0;i<500;++i) {
        auto candidate=base;const auto pos=96u+random()%(candidate.size()-96u);
        candidate[pos]^=static_cast<std::byte>(1u<<(random()%8));rehash(candidate);save(out/"mutation.rfcx",candidate);
        try {
            storage::MappedRfcx m(out/"mutation.rfcx");++parsed;
            for (std::uint16_t k=0;k<m.grade_count();++k) {
                const auto g=m.grade(core::Grade{k});
                for (std::size_t c=0;c<g.cell_count();++c) {
                    (void)g.cell_scale(c);if (k) {(void)g.find_relation(g.relation_key(c));(void)g.incidence_range(c);}
                    else (void)g.find_identity(g.identity(c));
                }
                for (std::size_t j=0;j<g.incidence_count();++j) {(void)g.incidence_coefficient(j);(void)g.find_occurrence(g.occurrence_key(j));}
            }
            try {auto decoded=m.materialize();auto encoded=storage::encode_rfcx(decoded);check(!encoded.bytes.empty(),"mutation reencode empty");++rebuilt;}
            catch (const std::exception&) { /* layout validation is not the entire source-axiom proof */ }
        } catch (const std::exception&) { }
        check(true,"bounded mutation completed");
    }
    std::cout<<"rehashed_mutations=500 parsed="<<parsed<<" reconstructed="<<rebuilt<<'\n';
}
void frozen_tests() {
#ifdef RFC_TEST_SOURCE_DIR
    const auto root=fs::path(RFC_TEST_SOURCE_DIR)/"tests/storage/fixtures";
    for (const auto sub:{"phase03_v1","phase03c_codes"}) {
        const auto p=root/sub/"base.rfcx";storage::MappedRfcx source(p);
        check(storage::encode_rfcx(source.materialize()).bytes==load(p),"frozen valid source changed bytes");
    }
#endif
}
} // namespace
int main(int argc,char** argv) {
    try {
        const auto out=argc>1?fs::path(argv[1]):fs::temp_directory_path()/("rfc_source_safety_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (fs::exists(out)) throw std::runtime_error("source safety tests require a fresh output directory");
        fs::create_directories(out);
        machine_tests();declaration_tests();preservation_tests(out);mapped_validation_tests(out);frozen_tests();
        std::cout<<"source_safety_checks="<<checks<<" all_passed=true\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n';return 1; }
}
