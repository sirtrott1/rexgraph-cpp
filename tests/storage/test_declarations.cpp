#include "rfc/storage/declaration_adapters.hpp"
#include "rfc/core/declaration_binding.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/storage/rfcx.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

using namespace rfc;
namespace {
using namespace composite;
using namespace storage;
using Q = numeric::Rational;
using Z = numeric::Integer;
using V = DeclaredValue;
using Bytes = std::vector<std::byte>;
namespace fs = std::filesystem;
std::size_t checks{}, truncations{}, mutation_accepts{}, mutation_rejects{};
void check(bool b, const char* message) { ++checks; if (!b) throw std::runtime_error(message); }
template<class F> void rejects(F&& f, const char* message) {
    bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
DeclarationId id(std::uint64_t key, std::uint64_t version = 1) { return {77, key, version}; }
CompiledLayout scalar(DeclarationKind kind, DeclarationLimits limits = {}) {
    Declaration d; d.id = id(1); d.kind = kind;
    return CompiledLayout(DeclarationRegistry({d}, limits), d.id, BooleanLayoutProfile::Lsb0OctetsV1, limits);
}
void save(const fs::path& path, const Bytes& bytes) {
    std::ofstream f(path, std::ios::binary); f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) throw std::runtime_error("fixture write");
}
Bytes load(const fs::path& path) {
    std::ifstream f(path, std::ios::binary); f.seekg(0, std::ios::end); const auto n = f.tellg();
    if (n < 0) throw std::runtime_error("fixture read");
    Bytes bytes(static_cast<std::size_t>(n)); f.seekg(0); f.read(reinterpret_cast<char*>(bytes.data()), n);
    if (!f) throw std::runtime_error("fixture read");
    return bytes;
}
void roundtrip(const CompiledLayout& layout, const V& value, bool archive = false) {
    const auto before = layout.allocation(value); const auto bits = encode_declared_bits(layout, value);
    check(before.bits == bits.bit_count && before.bytes == bits.bytes.size(), "allocation/codec disagreement");
    const auto decoded = decode_declared_bits(layout, bits);
    check(declared_value_equal(value, decoded), "D(E(v)) != v");
    check(encode_declared_bits(layout, decoded) == bits, "E(D(b)) != b");
    machine::BitReader count_reader(bits.bytes,bits.bit_count,layout.limits().max_encoded_bytes);
    declaration_codec_detail::DecodeBudget actual{layout.limits()};
    (void)declaration_codec_detail::get_value(count_reader,layout,layout.root(),actual);
    check(actual.nodes==before.value_nodes && actual.allocated==before.materialized_bytes,"preflight/decode allocation policy mismatch");
    if (archive) {
        const auto encoded = encode_declared_archive(layout, std::vector{value});
        const auto result = decode_declared_archive(encoded);
        check(result.layout.root() == layout.root() && declared_value_equal(result.values.at(0), value), "bound archive replay");
        check(encoded == encode_declared_archive(result.layout, result.values), "archive canonical bytes");
    }
}
CompiledLayout family_layout() {
    std::vector<Declaration> defs = {
        Declaration::boolean(id(1)), Declaration::finite(id(2), {9,7,50,0,8,60}),
        Declaration::integer(id(3)), Declaration::rational(id(4)),
        Declaration::optional(id(5), id(4)), Declaration::repeated(id(6), id(1), 0, 1000),
        Declaration::alternative(id(7), {{101,id(1),0},{102,id(3),0},{103,id(5),0}}),
        Declaration::product(id(8), {{100,id(2),0},{200,id(7),1},{300,id(6),0}}),
        Declaration::rational_tensor(id(9), {{1,3,{}},{2,4,{}}}),
        Declaration::product(id(10), {})
    };
    return CompiledLayout(DeclarationRegistry(std::move(defs)), id(8));
}
void graph_tests() {
    const auto root = family_layout();
    check(root.declarations().factor_ordinal(id(8),100)==0 &&
          root.declarations().factor_ordinal(id(8),300)==2,"product factor index changed order");
    rejects([&]{(void)root.declarations().factor_ordinal(id(8),999);},"unknown product factor indexed");
    rejects([&]{(void)root.declarations().alternative_ordinal(id(8),100);},"product accepted as alternative");
    DeclarationBuilder builder;
    builder.define(Declaration::optional(id(800),{88,1,3}));
    rejects([&]{(void)builder.freeze();},"unresolved partial module executed");
    builder.include(DeclarationRegistry({Declaration::boolean({88,1,3})}));
    const auto imported_snapshot=builder.freeze();
    roundtrip(CompiledLayout(imported_snapshot,id(800)),V::present(V::boolean(true)),true);
    builder.define(Declaration::integer(id(801)));
    check(imported_snapshot.definitions().size()==2 && builder.freeze().definitions().size()==3,"builder mutated frozen registry");
    builder.define(Declaration::integer({88,1,3}));
    rejects([&]{(void)builder.freeze();},"import meaning silently replaced");
    const auto frozen = encode_declarations(root);
    auto ds = root.declarations().definitions(); std::reverse(ds.begin(),ds.end());
    check(frozen == encode_declarations(CompiledLayout(DeclarationRegistry(ds),root.root())), "definition input order leaked");
    const auto read = decode_declarations(frozen);
    check(read.declarations().factor_ordinal(id(8),300)==2,"decoded product factor index changed order");
    check(read.declarations().definitions() == root.declarations().definitions(), "declaration roundtrip");
    check(encode_declarations(read) == frozen, "declaration canonical law");
    const auto imported = root.declarations().imported(root.declarations());
    check(imported.definitions() == root.declarations().definitions(), "equal imports duplicated");
    ds.push_back(Declaration::integer(id(1)));
    rejects([&]{ DeclarationRegistry bad(ds); }, "conflicting import accepted");
    rejects([&]{ DeclarationRegistry bad({Declaration::optional(id(1),id(1))}); }, "self-cycle accepted");
    rejects([&]{ DeclarationRegistry bad({Declaration::optional(id(1),id(2)),Declaration::optional(id(2),id(1))}); }, "cycle accepted");
    rejects([&]{ DeclarationRegistry bad({Declaration::optional(id(1),id(2))}); }, "missing import accepted");
    rejects([&]{ DeclarationRegistry bad({Declaration::boolean({0,1,1})}); }, "zero namespace accepted");
    rejects([&]{ DeclarationRegistry bad({Declaration::boolean({1,1,0})}); }, "zero version accepted");
    for (unsigned k=9; k<256; ++k) {
        Declaration d;d.id=id(1);d.kind=static_cast<DeclarationKind>(k);
        rejects([&]{ DeclarationRegistry bad({d}); }, "unknown constructor accepted");
    }
    auto rogue = Declaration::boolean(id(1)); rogue.symbols={4};
    rejects([&]{ DeclarationRegistry bad({rogue}); }, "unused declaration lane accepted");
    rejects([&]{ DeclarationRegistry bad({Declaration::finite(id(1),{})}); }, "empty finite domain");
    rejects([&]{ DeclarationRegistry bad({Declaration::finite(id(1),{1,1})}); }, "duplicate finite symbols");
    rejects([&]{ DeclarationRegistry bad({Declaration::alternative(id(1),{})}); }, "empty alternative");
    rejects([&]{ DeclarationRegistry bad({Declaration::boolean(id(1)),Declaration::product(id(2),{{9,id(1),0},{9,id(1),1}})}); }, "duplicate factors");
    rejects([&]{ DeclarationRegistry bad({Declaration::boolean(id(1)),Declaration::alternative(id(2),{{9,id(1),1}})}); }, "graded alternative silently accepted");
    rejects([&]{ DeclarationRegistry bad({Declaration::boolean(id(1)),Declaration::repeated(id(2),id(1),5,4)}); }, "invalid repeat bounds");
    rejects([&]{ DeclarationRegistry bad({Declaration::rational_tensor(id(1),{{1,2,{3,3}}})}); }, "duplicate coordinate IDs");
    rejects([&]{ DeclarationRegistry bad({Declaration::rational_tensor(id(1),{{1,2,{3}}})}); }, "coordinate count mismatch");
    const auto mx = std::numeric_limits<std::size_t>::max();
    rejects([&]{ DeclarationRegistry bad({Declaration::rational_tensor(id(1),{{1,mx,{}},{2,2,{}}})}); }, "shape overflow");
    rejects([&]{ CompiledLayout bad(DeclarationRegistry({Declaration::boolean(id(1)),Declaration::repeated(id(2),id(1),mx,mx)}),id(2)); }, "layout bound overflow");
    auto limits = DeclarationLimits{};limits.max_components=1;
    rejects([&]{ DeclarationRegistry bad(root.declarations().definitions(),limits); }, "components ignored");
    limits = {}; limits.max_depth=129;
    rejects([&]{ DeclarationRegistry bad({},limits); }, "unsafe depth policy");
    ds={Declaration::boolean(id(1))};
    for (unsigned k=2;k<=64;++k) ds.push_back(Declaration::optional(id(k),id(k-1)));
    DeclarationRegistry deep(ds);check(deep.depth(id(64))==64,"declared depth");
    ds.push_back(Declaration::optional(id(65),id(64)));
    rejects([&]{ DeclarationRegistry bad(ds); }, "deep graph accepted");
    rejects([&]{ CompiledLayout bad(root.declarations(),root.root(),static_cast<BooleanLayoutProfile>(2)); }, "unsupported profile");
    auto renamed = Declaration::boolean(id(1,2));
    DeclarationRegistry versions({Declaration::boolean(id(1)),renamed});
    check(versions.definitions().size()==2,"versions collapsed");
    check(encode_declarations(CompiledLayout(versions,id(1)))!=encode_declarations(CompiledLayout(versions,id(1,2))),"root version binding");
}
void cursor_tests() {
    for(unsigned offset=0;offset<8;++offset)for(std::size_t count=0;count<258;++count) {
        Bytes bytes(count);
        for(std::size_t i=0;i<count;++i)bytes[i]=static_cast<std::byte>((i*137+count*19)%256);
        const auto prefix=offset?((std::uint64_t{1}<<offset)-1):0;
        const auto extent=offset+count*8+64;
        const auto budget=machine::ceil_div(extent,8);
        machine::BitWriter bulk(extent,budget),reference(extent,budget);
        bulk.put(prefix,offset);reference.put(prefix,offset);
        bulk.put_octets(bytes);
        for(const auto byte:bytes)reference.put(std::to_integer<unsigned>(byte),8);
        bulk.put(~std::uint64_t{0},64);reference.put(~std::uint64_t{0},64);
        const auto written=std::move(bulk).finish();
        check(written==std::move(reference).finish(),"bulk octets changed canonical bits");
        machine::BitReader reader(written.bytes,written.bit_count,budget);
        check(reader.get(offset)==prefix,"bulk octets changed prefix");
        check(reader.get_octets(count)==bytes,"bulk octet read changed original bytes");
        check(reader.get(64)==~std::uint64_t{0},"bulk octets changed suffix");reader.finish();
    }
    const Bytes octet{std::byte{255}};
    machine::BitWriter short_writer(7,1);
    rejects([&]{short_writer.put_octets(octet);},"truncated bulk writer accepted");
    short_writer.put(127,7);
    const auto short_bits=std::move(short_writer).finish();
    machine::BitReader short_reader(short_bits.bytes,short_bits.bit_count,1);
    rejects([&]{(void)short_reader.get_octets(std::numeric_limits<std::size_t>::max());},
            "oversized bulk reader accepted");
    check(short_reader.remaining()==7 && short_reader.get(7)==127,
          "failed bulk transfer changed cursor state");short_reader.finish();
    for (unsigned n=0;n<=64;++n) for (unsigned offset=0;offset<8;++offset) {
        const std::uint64_t x = n==64 ? ~std::uint64_t{0} : (std::uint64_t{1}<<n)-1;
        machine::BitWriter w(offset+n,32);w.put(0,offset);w.put(x,n);auto bits=std::move(w).finish();
        machine::BitReader r(bits.bytes,bits.bit_count,32);check(r.get(offset)==0 && r.get(n)==x,"unaligned word cursor");r.finish();
        if (bits.bit_count%8) check((std::to_integer<unsigned>(bits.bytes.back())>>(bits.bit_count%8))==0,"tail not clear");
    }
    machine::BitWriter original(9,2);original.put(1,1);
    auto moved=std::move(original);moved.put(7,8);const auto transferred=std::move(moved).finish();
    check(transferred.bit_count==9,"writer move lost extent");
    check(std::move(original).finish().bit_count==0 && std::move(moved).finish().bit_count==0,"moved/finished writer stale extent");
    rejects([&]{original.put(1,1);},"moved writer allowed out-of-bounds write");
    machine::BitWriter target(1,1);target=machine::BitWriter(2,1);target.put(3,2);
    check(std::move(target).finish().bytes==Bytes{std::byte{3}},"writer move assignment");
    rejects([&]{machine::BitWriter w(std::numeric_limits<std::size_t>::max(),64);},"huge bit allocation");
    rejects([&]{machine::BitWriter w(1,1);w.put(2,1);},"bit overflow");
    rejects([&]{machine::BitWriter w(1,1);w.put(1,2);},"bit extent overflow");
    rejects([&]{machine::BitWriter w(1,1);(void)std::move(w).finish();},"incomplete writer");
}
void finite_tests() {
    for (std::size_t n=1;n<=32;++n) {
        std::vector<std::uint64_t> symbols;
        for (std::size_t j=0;j<n;++j) symbols.push_back(500+j*3);
        CompiledLayout layout(DeclarationRegistry({Declaration::finite(id(1),symbols)}),id(1));
        const auto width=layout.bounds().minimum_bits;
        for (std::size_t code=0;code<(std::size_t{1}<<width);++code) {
            machine::BitWriter w(width,16);w.put(code,static_cast<unsigned>(width));auto bits=std::move(w).finish();
            if (code<n) { check(std::get<V::Symbol>(decode_declared_bits(layout,bits).data).key==symbols[code],"finite symbol order");roundtrip(layout,V::symbol(symbols[code])); }
            else rejects([&]{(void)decode_declared_bits(layout,bits);},"reserved pattern accepted");
        }
        rejects([&]{layout.validate(V::symbol(499));},"undeclared symbol accepted");
        rejects([&]{layout.validate(V::boolean(true));},"symbol coerced from bool");
    }
    CompiledLayout six(DeclarationRegistry({Declaration::finite(id(1),{0,1,2,3,4,5})}),id(1));
    for (unsigned v=0;v<256;++v) {
        machine::EncodedBits raw{3,{static_cast<std::byte>(v)}};
        if (v<6) check(encode_declared_bits(six,decode_declared_bits(six,raw))==raw,"small canonical bit code");
        else rejects([&]{(void)decode_declared_bits(six,raw);},"reserved/tail bits accepted");
    }
}
void integer_tests(const fs::path& out) {
    const auto integers=scalar(DeclarationKind::Integer),rationals=scalar(DeclarationKind::Rational);
    check(encode_declared_bits(integers,V::integer(Z(1)))==machine::EncodedBits{17,{std::byte{2},std::byte{2},std::byte{0}}},"integer golden encoding");
    check(encode_declared_bits(rationals,V::rational(numeric::rational(1,2)))==machine::EncodedBits{33,{std::byte{2},std::byte{2},std::byte{2},std::byte{4},std::byte{0}}},"rational golden encoding");
    Z big=1;big<<=129;
    save(out/"integer.rfcxv",encode_declared_archive(integers,std::vector{V::integer(Z(-big)),V::integer(Z(0)),V::integer(Z(big+17))}));
    save(out/"rational.rfcxv",encode_declared_archive(rationals,std::vector{V::rational(numeric::rational(1,3)),V::rational(Q{0}),V::rational(Q(Z(-big),Z(big+1)))}));
    for (long n=-128;n<=128;++n) {
        roundtrip(integers,V::integer(Z(n)));
        for (long d=1;d<=13;++d) roundtrip(rationals,V::rational(numeric::rational(n,d)));
    }
    for (unsigned n : {1u,7u,8u,15u,31u,63u,64u,65u,127u,128u,129u,1024u,8192u}) {
        Z large=1;large<<=n;
        roundtrip(integers,V::integer(large),true);roundtrip(integers,V::integer(Z(-large)),true);
        auto sum=integers.add_exact(V::integer(Z(large-1)),V::integer(Z(1)));
        check(std::get<Z>(sum.data)==large,"exact promotion wrapped");
        auto q=numeric::Rational(large,Z(large+1));roundtrip(rationals,V::rational(q),true);
    }
    check(std::get<Z>(integers.add_exact(V::integer(Z(15)),V::integer(Z(15))).data)==30,"addition interpreted as 4-bit modular");
    check(std::get<Q>(rationals.add_exact(V::rational(numeric::rational(1,3)),V::rational(numeric::rational(1,6))).data)==numeric::rational(1,2),"rational exact addition");
    rejects([&]{(void)scalar(DeclarationKind::Boolean).add_exact(V::boolean(true),V::boolean(true));},"Boolean addition silently rationalized");
    DeclarationLimits low;low.max_integer_bits=8;
    auto narrow=scalar(DeclarationKind::Integer,low);
    roundtrip(narrow,V::integer(Z(255)));
    rejects([&]{(void)narrow.add_exact(V::integer(Z(255)),V::integer(Z(1)));},"integer resource boundary ignored");
    auto largebits=encode_declared_bits(integers,V::integer(Z(256)));
    rejects([&]{(void)decode_declared_bits(narrow,largebits);},"decoder integer bit budget ignored");
    { machine::BitWriter w(9,8);w.put(1,1);w.put(0,8);auto bad=std::move(w).finish();rejects([&]{(void)decode_declared_bits(integers,bad);},"negative zero accepted"); }
    { machine::BitWriter w(17,8);w.put(0,1);w.put(1,8);w.put(0,8);auto bad=std::move(w).finish();rejects([&]{(void)decode_declared_bits(integers,bad);},"nonminimal magnitude accepted"); }
    { machine::BitWriter w(17,8);w.put(0,1);w.put(128,8);w.put(0,8);auto bad=std::move(w).finish();rejects([&]{(void)decode_declared_bits(integers,bad);},"nonminimal integer length accepted"); }
    for (auto denominator : {0u,2u}) {
        machine::BitWriter w(denominator?33:25,8);w.put(0,1);w.put(1,8);w.put(2,8);w.put(denominator?1:0,8);
        if (denominator)w.put(denominator,8);
        auto bad=std::move(w).finish();rejects([&]{(void)decode_declared_bits(rationals,bad);},"zero/nonreduced rational accepted");
    }
    Q bad;mpz_set_ui(mpq_numref(bad.get_mpq_t()),2);mpz_set_ui(mpq_denref(bad.get_mpq_t()),4);
    rejects([&]{rationals.validate(V::rational(bad));},"noncanonical input rational accepted");
    mpz_set_ui(mpq_denref(bad.get_mpq_t()),0);rejects([&]{rationals.validate(V::rational(bad));},"zero input denominator accepted");
}
void composed_tests(const fs::path& out) {
    const auto family=family_layout();
    for (std::size_t n : {0u,1u,2u,63u,64u,65u,127u,128u,129u,255u}) {
        std::vector<V> booleans;for(std::size_t i=0;i<n;++i)booleans.push_back(V::boolean(i%3==0));
        for (auto key : {101u,102u,103u}) {
            auto chosen=key==101?V::boolean(true):key==102?V::integer(Z(-1234567)):V::present(V::rational(numeric::rational(1,3)));
            auto v=V::product({V::symbol(50),V::alternative(key,chosen),V::repeated(booleans)});
            roundtrip(family,v,true);
        }
    }
    auto optional=CompiledLayout(family.declarations(),id(5));
    const auto absent=encode_declared_bits(optional,V::absent());
    const auto zero=encode_declared_bits(optional,V::present(V::rational(Q{0})));
    check(absent!=zero,"optional absence equals numeric zero");
    auto nested=DeclarationRegistry({Declaration::rational(id(1)),Declaration::optional(id(2),id(1)),Declaration::optional(id(3),id(2))});
    CompiledLayout nested_layout(nested,id(3));
    const std::vector<V> tri={V::absent(),V::present(V::absent()),V::present(V::present(V::rational(Q{0})))};
    check(encode_declared_bits(nested_layout,tri[0])!=encode_declared_bits(nested_layout,tri[1]) &&
          encode_declared_bits(nested_layout,tri[1])!=encode_declared_bits(nested_layout,tri[2]),"absent/null/zero collapsed");
    const auto archive=encode_declared_archive(nested_layout,tri);save(out/"optional.rfcxv",archive);
    const auto replay=decode_declared_archive(load(out/"optional.rfcxv"));check(replay.values.size()==3,"disk optional replay");
    auto repeated=CompiledLayout(family.declarations(),id(6));
    rejects([&]{repeated.validate(V::repeated(std::vector<V>(1001,V::boolean(false))));},"repeat upper bound ignored");
    auto alternatives=CompiledLayout(family.declarations(),id(7));
    machine::EncodedBits reserved{2,{std::byte{3}}};rejects([&]{(void)decode_declared_bits(alternatives,reserved);},"reserved alternative accepted");
    rejects([&]{alternatives.validate(V::alternative(404,V::boolean(true)));},"undeclared alternative");
    auto unit=CompiledLayout(family.declarations(),id(10));roundtrip(unit,V::product({}),true);
    check(encode_declared_bits(unit,V::product({})).bytes.empty(),"empty product expanded");
    auto zero_defs=DeclarationRegistry({Declaration::product(id(1),{}),Declaration::repeated(id(2),id(1),0,1000000)});
    DeclarationLimits small;small.max_value_nodes=10;small.max_materialized_bytes=4096;
    CompiledLayout zero_repeat(zero_defs,id(2),BooleanLayoutProfile::Lsb0OctetsV1,small);
    machine::BitWriter w(24,8);declaration_codec_detail::put_var(w,1000000);const auto many=std::move(w).finish();
    rejects([&]{(void)decode_declared_bits(zero_repeat,many);},"zero-width repetition amplification");
    DeclarationLimits smallmem;smallmem.max_materialized_bytes=1;
    auto tiny=CompiledLayout(zero_defs,id(2),BooleanLayoutProfile::Lsb0OctetsV1,smallmem);
    auto one=encode_declared_bits(CompiledLayout(zero_defs,id(2)),V::repeated({V::product({})}));
    rejects([&]{(void)decode_declared_bits(tiny,one);},"materialization limit ignored");
    rejects([&]{(void)encode_declared_bits(tiny,V::repeated({V::product({})}));},"encoder materialization contract differs");
    auto single_bool=scalar(DeclarationKind::Boolean,smallmem);
    roundtrip(single_bool,V::boolean(true));
    rejects([&]{(void)encode_declared_archive(single_bool,std::vector{V::boolean(true)});},"archive root allocation not budgeted");
    save(out/"family.rfcxd",encode_declarations(family));
    const std::vector<V> examples={V::product({V::symbol(50),V::alternative(102,V::integer(Z("1208925819614629174706176"))),V::repeated({V::boolean(true),V::boolean(false)})})};
    save(out/"family.rfcxv",encode_declared_archive(family,examples));
}
void tensor_tests(const fs::path& out) {
    auto layout=CompiledLayout(family_layout().declarations(),id(9));
    numeric::IndexDomain domain({3,4});
    auto support=numeric::StructuralSupport::sparse(12,{0,2,11});
    auto tensor=numeric::SparseRationalTensor(domain,support,numeric::SparseRationalVector(12,{{2,numeric::rational(1,2)},{11,Q{-2}}}));
    roundtrip(layout,V::tensor(tensor),true);
    auto same=numeric::SparseRationalTensor(domain,numeric::StructuralSupport::packed(12,{0,2,11}),tensor.values());
    check(encode_declared_bits(layout,V::tensor(tensor))==encode_declared_bits(layout,V::tensor(same)),"support representation affected canonical bytes");
    auto smaller=numeric::SparseRationalTensor(domain,numeric::StructuralSupport::sparse(12,{2,11}),tensor.values());
    check(encode_declared_bits(layout,V::tensor(tensor))!=encode_declared_bits(layout,V::tensor(smaller)),"supported zero lost");
    save(out/"tensor.rfcxv",encode_declared_archive(layout,std::vector{V::tensor(tensor),V::tensor(smaller)}));
    auto zero_layout=CompiledLayout(DeclarationRegistry({Declaration::rational_tensor(id(1),{{1,0,{}},{2,129,{}}})}),id(1));
    roundtrip(zero_layout,V::tensor(numeric::SparseRationalTensor::zero(numeric::IndexDomain({0,129}),numeric::StructuralSupport::full(0))),true);
    check(zero_layout.bounds().maximum_bits==16,"empty tensor bounds");
    { machine::BitWriter w(32,8);for(auto x:{2u,0u,0u,0u})w.put(x,8);auto bad=std::move(w).finish();rejects([&]{(void)decode_declared_bits(layout,bad);},"duplicate tensor support accepted"); }
    { machine::BitWriter w(57,8);for(auto x:{1u,0u,1u,0u})w.put(x,8);declaration_codec_detail::put_rational(w,Q{0});auto bad=std::move(w).finish();rejects([&]{(void)decode_declared_bits(layout,bad);},"explicit sparse zero accepted"); }
    { machine::BitWriter w(65,16);for(auto x:{1u,0u,1u,1u})w.put(x,8);declaration_codec_detail::put_rational(w,Q{1});auto bad=std::move(w).finish();rejects([&]{(void)decode_declared_bits(layout,bad);},"value outside support accepted"); }
}
void branch_tests() {
    core::RelationSignature sig;
    const auto left=sig.aspects().add("left",AspectRole::RelationRole),right=sig.aspects().add("right",AspectRole::RelationRole);
    const auto type=sig.add_type("generic-many");core::RelationalComplexBuilder builder(sig);
    std::vector<core::BoundaryOccurrenceInput> occ;
    for (unsigned n=0;n<5;++n) occ.push_back({10,n!=1,n%2!=0,Q{0},{{left,n<2},{right,n>=2}},core::OccurrenceKey{100+n}});
    const auto relation=builder.add_grade_one(core::RelationKey{900},type,occ);const auto source=builder.freeze();
    auto payload=scalar(DeclarationKind::Rational);
    core::BranchDeclaration d{id(90),payload.root(),core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
                              {{10,"left",2,2},{20,"right",3,3}}};
    core::BranchDeclarationBinding binding(sig,type,d,payload);
    check(binding.validate(source,relation,V::rational(Q{0}))==std::vector<std::size_t>{2,3},"many-to-many retained cardinality");
    check(source.grade(core::Grade{1}).cell_count()==1 && source.grade(core::Grade{1}).incidence_count()==5 &&
          source.grade(core::Grade{0}).cell_count()==1,"branch expanded Cartesian pairs/shared participants");
    d.population=core::BranchPopulation::Active;d.roles[0].min_count=d.roles[0].max_count=1;
    check(core::BranchDeclarationBinding(sig,type,d,payload).validate(source,relation,V::rational(Q{0}))==std::vector<std::size_t>{1,3},"active role count changed retained source");
    d.population=core::BranchPopulation::Retained;
    rejects([&]{(void)core::BranchDeclarationBinding(sig,type,d,payload).validate(source,relation,V::rational(Q{0}));},"bad role cardinality accepted");
    rejects([&]{(void)binding.validate(source,relation,V::boolean(false));},"branch payload silently coerced");
    rejects([&]{(void)binding.validate(source,relation,V::rational(Q{0}),4);},"branch work budget ignored");
    auto renamed=sig;renamed.aspects().add("new");core::RelationalComplexBuilder other(renamed);other.add_grade_one(core::RelationKey{900},type,occ);const auto changed=other.freeze();
    rejects([&]{(void)binding.validate(changed,relation,V::rational(Q{0}));},"signature migration silently accepted");
    for (std::size_t nl : {1u,2u,8u})for(std::size_t nr : {1u,3u,9u}) {
        core::RelationalComplexBuilder b(sig);std::vector<core::BoundaryOccurrenceInput> os;
        for(std::size_t n=0;n<nl+nr;++n)os.push_back({42,true,false,Q{1},{{left,n<nl},{right,n>=nl}},core::OccurrenceKey{100+n}});
        const auto rid=b.add_grade_one(core::RelationKey{1},type,os);const auto s=b.freeze();
        auto desc=binding.declaration();desc.roles[0].min_count=desc.roles[0].max_count=nl;desc.roles[1].min_count=desc.roles[1].max_count=nr;
        check(core::BranchDeclarationBinding(sig,type,desc,payload).validate(s,rid,V::rational(Q{1}))==std::vector<std::size_t>{nl,nr},"branch arrangement");
    }
    core::RelationSignature normal;const auto tail=normal.aspects().add("tail",AspectRole::RelationRole);const auto nt=normal.add_branch_type("normalized");const auto head=*normal.type(nt).distinguished_role_aspect();
    core::RelationalComplexBuilder nb(normal);auto rid=nb.add_grade_one(core::RelationKey{1},nt,{
        {10,true,true,Q{1},{{head,true}},core::OccurrenceKey{1}},
        {20,true,false,numeric::rational(1,2),{{tail,true}},core::OccurrenceKey{2}},
        {20,true,false,numeric::rational(1,2),{{tail,true}},core::OccurrenceKey{3}}});
    auto ns=nb.freeze();core::BranchDeclaration nd{id(91),payload.root(),core::BranchPopulation::Active,core::BranchSemantics::NormalizedOneHead,true,{{1,"head",1,1},{2,"tail",2,2}}};
    check(core::BranchDeclarationBinding(normal,nt,nd,payload).validate(ns,rid,V::rational(Q{0}))==std::vector<std::size_t>{1,2},"normalized explicit constructor");
    nd.semantics=core::BranchSemantics::Generic;rejects([&]{core::BranchDeclarationBinding bad(normal,nt,nd,payload);},"normalized silently generic");
}
void source_aspect_tests(const fs::path& out) {
    core::RelationSignature sig;std::vector<AspectId> aspects={sig.existence_aspect(),sig.orientation_aspect()};
    for(unsigned j=0;j<129;++j)aspects.push_back(sig.aspects().add("aspect-"+std::to_string(j)));
    const auto type=sig.add_type("large-retained");core::RelationalComplexBuilder b(sig);
    std::vector<core::BoundaryOccurrenceInput> inputs;
    auto bit=[](std::size_t i,std::size_t j){return (i*7+j*11)%13<6;};
    for(std::size_t i=0;i<130;++i) {
        std::vector<core::AspectValue> values;for(std::size_t j=2;j<aspects.size();++j)values.push_back({aspects[j],bit(i,j)});
        inputs.push_back({10+i%3,bit(i,0),bit(i,1),Q{0},values,core::OccurrenceKey{100+i}});
    }
    b.add_grade_one(core::RelationKey{1},type,inputs);b.declare_grade(core::Grade{2});const auto source=b.freeze();
    std::vector<DeclarationFactor> factors;std::vector<core::SourceAspectFactorBinding> bindings;
    for(std::size_t j=0;j<aspects.size();++j){factors.push_back({1000+j,id(1),0});bindings.push_back({1000+j,sig.aspects().at(aspects[j]).name});}
    auto layout=CompiledLayout(DeclarationRegistry({Declaration::boolean(id(1)),Declaration::product(id(2),factors)}),id(2));
    core::SourceAspectLayoutBinding bound(sig,layout,bindings);
    check(layout.bounds().maximum_bits==131,"binary factors expanded exponentially");
    for(std::size_t i=0;i<130;++i) {
        const auto value=bound.materialize_occurrence(source,core::Grade{1},i);
        const auto encoded=encode_declared_bits(layout,value);
        check(encoded.bit_count==131 && encoded.bytes.size()==17,"packed factor allocation");
        for(std::size_t j=0;j<131;++j)check(std::get<bool>(std::get<V::Product>(value.data).elements[j].data)==bit(i,j),"retained source aspect");
        roundtrip(layout,value);
    }
    BinaryProgram program(5);
    program.append({BinaryOpcode::LoadAspect,0,0,0,0,0});program.append({BinaryOpcode::LoadAspect,1,0,0,0,1});
    program.append({BinaryOpcode::Not,2,1});program.append({BinaryOpcode::And,3,0,2});
    program.append({BinaryOpcode::LoadAspect,4,0,0,0,130});program.append({BinaryOpcode::Xor,3,3,4});
    const auto output=bound.execute(source,core::Grade{1},program,3);
    for(std::size_t i=0;i<130;++i)check(output.get(i)==((bit(i,0)&&!bit(i,1))!=bit(i,130)),"schema bound packed program");
    check((output.word(2)>>2)==0,"source operation tail masking");
    BinaryExecutionPlan prepared(program,{3,0,4,3});
    BinaryExecutionWorkspace workspace;
    const auto fused=bound.execute(source,core::Grade{1},prepared,workspace);
    check(fused.size()==4,"prepared source output arity");
    for(std::size_t i=0;i<130;++i) {
        check(fused[0].get(i)==output.get(i) && fused[3].get(i)==output.get(i),"fused repeated output");
        check(fused[1].get(i)==bit(i,0) && fused[2].get(i)==bit(i,130),"fused direct outputs");
    }
    const auto growth=workspace.growth_events();
    for(unsigned repeat=0;repeat<8;++repeat) {
        (void)bound.execute(source,core::Grade{1},prepared,workspace);
        check(workspace.growth_events()==growth,"prepared source scratch not reused");
    }
    check(bound.execute(source,core::Grade{2},program,3).size()==0,"empty source execution");
    BinaryProgram bad(1);bad.append({BinaryOpcode::LoadAspect,0,0,0,0,131});
    rejects([&]{(void)bound.execute(source,core::Grade{2},bad,0);},"empty domain skipped program validation");
    rejects([&]{(void)bound.materialize_occurrence(source,core::Grade{1},130);},"invalid occurrence accepted");
    write_rfcx(source,out/"source.rfcx");
    save(out/"source_view.rfcxd",encode_declarations(layout));
    save(out/"source_value.rfcxv",encode_declared_archive(layout,std::vector{bound.materialize_occurrence(source,core::Grade{1},64)}));
}
void bridge_tests(const fs::path& out) {
    RelationalCodeState codes;codes.namespace_key=12;
    codes.types={{1,RelationalCodeTypeKind::Finite,{5,0,8},{}},
                 {2,RelationalCodeTypeKind::Rational,{30,40,50},{}},
                 {3,RelationalCodeTypeKind::Product,{},{{100,1,1,{}},{200,2,2,{}}}}};
    auto layout=compile_relational_code_layout(codes,3,7);
    const auto& tensor_decl=layout.declarations().at({12,2,7});
    check(tensor_decl.axes.at(0).coordinates==std::vector<std::uint64_t>{30,40,50},"code coordinate identities lost");
    check(layout.declarations().at({12,3,7}).factors[1].grade==2,"code grade provenance lost");
    auto tensor=numeric::SparseRationalTensor(numeric::IndexDomain({3}),numeric::StructuralSupport::sparse(3,{0,2}),numeric::SparseRationalVector(3,{{2,Q{-4}}}));
    RelationalCodeValue original{RelationalCodeValue::Product{{std::uint64_t{5}},{tensor}}};
    const auto imported=import_relational_code_value(layout,original);roundtrip(layout,imported,true);
    check(relational_code_detail::value_equal(original,export_relational_code_value(layout,imported)),"legacy value bridge changed content");
    save(out/"code_bridge.rfcxv",encode_declared_archive(layout,std::vector{imported}));
    codes.types[2].factors[0].source_aspect="exists";
    rejects([&]{(void)compile_relational_code_layout(codes,3,7);},"source-backed provenance erased");
    rejects([&]{(void)compile_relational_code_layout(codes,3,0);},"version silently assigned");
    rejects([&]{(void)import_relational_code_value(layout,RelationalCodeValue{std::uint64_t{0}});},"legacy wrong carrier accepted");
}
void independent_tests(const fs::path& out) {
#ifdef RFC_TEST_SOURCE_DIR
    const auto directory=fs::path(RFC_TEST_SOURCE_DIR)/"tests/fixtures/phase03f1";
    const auto bytes=load(directory/"independent.rfcxv");
    const auto replay=decode_declared_archive(bytes);
    check(encode_declared_archive(replay.layout,replay.values)==bytes,"independent archive byte mismatch");
    check(encode_declarations(decode_declarations(load(directory/"independent.rfcxd")))==load(directory/"independent.rfcxd"),"independent declaration byte mismatch");
    const auto& first=std::get<V::Product>(replay.values.at(0).data).elements;
    Z expected=1;expected<<=129;expected+=1;expected=-expected;
    check(std::get<Z>(first.at(1).data)==expected,"independent exact integer mismatch");
    check(std::get<V::Symbol>(first.at(0).data).key==0,"independent finite order mismatch");
    check(std::get<Q>(std::get<V::Optional>(first.at(2).data).element.at(0).data)==numeric::rational(1,3),"independent rational mismatch");
    const auto& repeated=std::get<V::Repeated>(first.at(3).data).elements;
    check(repeated.size()==129,"independent repetition length");
    for(std::size_t i=0;i<repeated.size();++i)check(std::get<bool>(repeated[i].data)==(i%3==0),"independent repeated bits");
    save(out/"independent.rfcxv",bytes);
#else
    (void)out;
    throw std::runtime_error("RFC_TEST_SOURCE_DIR must identify the independent fixtures");
#endif
}
void wire_tests(const fs::path& out) {
    for(const auto& name:{"family.rfcxd","family.rfcxv","optional.rfcxv","tensor.rfcxv"}) {
        const auto original=load(out/name);const bool declaration=std::string(name).ends_with(".rfcxd");
        for(std::size_t n=0;n<original.size();++n){
            const auto prefix=std::span(original).first(n);
            rejects([&]{if(declaration)(void)decode_declarations(prefix);else (void)decode_declared_archive(prefix);},"truncated archive accepted");++truncations;
        }
        auto trailing=original;trailing.push_back(std::byte{0});
        rejects([&]{if(declaration)(void)decode_declarations(trailing);else(void)decode_declared_archive(trailing);},"trailing archive accepted");
        std::mt19937_64 rng(129);
        for(unsigned trial=0;trial<250;++trial){
            auto changed=original;const auto p=static_cast<std::size_t>(rng()%changed.size());changed[p]^=static_cast<std::byte>(1u<<(rng()%8));
            std::optional<Bytes> recoded;
            try {
                if(declaration)recoded=encode_declarations(decode_declarations(changed));
                else {auto read=decode_declared_archive(changed);recoded=encode_declared_archive(read.layout,read.values);}
            }catch(const std::exception&){++mutation_rejects;}
            if(recoded){++mutation_accepts;check(*recoded==changed,"accepted noncanonical mutation");}
            else check(true,"rejected mutation");
        }
    }
    const auto simple=encode_declarations(scalar(DeclarationKind::Boolean));
    auto bad=simple;bad[8]=std::byte{255};rejects([&]{(void)decode_declarations(bad);},"unknown profile on wire");
    bad=simple;bad[58]=std::byte{255};rejects([&]{(void)decode_declarations(bad);},"unknown constructor on wire");
    // One fixed leaf declaration: count at octet 33, kind at octet 58.
    bad=simple;bad[33]=std::byte{129};bad.insert(bad.begin()+34,std::byte{0});
    rejects([&]{(void)decode_declarations(bad);},"nonminimal metadata length");
    DeclarationLimits tiny;tiny.max_encoded_bytes=1;
    rejects([&]{(void)decode_declarations(simple,tiny);},"wire input budget");
}
void remaining_budget_tests() {
    const auto layout=scalar(DeclarationKind::Integer);
    const auto value=DeclaredValue::integer(numeric::Integer{19});
    const auto bits=encode_declared_bits(layout,value);
    check(declared_value_equal(decode_declared_bits(layout,bits.bytes,bits.bit_count,1,1),value),"remaining scalar decode allowance changed value");
    rejects([&]{(void)decode_declared_bits(layout,bits.bytes,bits.bit_count,0,1);},"zero remaining value allowance bypassed");
    rejects([&]{(void)decode_declared_bits(layout,bits.bytes,bits.bit_count,1,0);},"zero remaining magnitude allowance bypassed");
    auto limits=layout.limits();limits.max_integer_bits=4;
    const CompiledLayout strict(layout.declarations(),layout.root(),layout.profile(),limits);
    rejects([&]{(void)decode_declared_bits(strict,bits.bytes,bits.bit_count,100,100);},"remaining allowance loosened compiled scalar policy");
}
} // namespace
int main(int argc,char** argv) {
    fs::path out;
    try {
        out=argc>1?fs::path(argv[1]):fs::temp_directory_path()/("rfc-declarations-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if(fs::exists(out))throw std::runtime_error("fixture directory must be fresh");
        fs::create_directories(out);
        graph_tests();cursor_tests();finite_tests();integer_tests(out);composed_tests(out);tensor_tests(out);branch_tests();source_aspect_tests(out);bridge_tests(out);independent_tests(out);wire_tests(out);remaining_budget_tests();
        std::cout<<"Phase03F1 declarations: "<<checks<<" checks passed; truncations="<<truncations
                 <<"; mutations_accepted="<<mutation_accepts<<"; mutations_rejected="<<mutation_rejects<<"\n";
        if(argc==1)fs::remove_all(out);
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}
}
