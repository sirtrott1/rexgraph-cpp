#include "rfc/learning/relational_literal_finite_field_codec.hpp"
#include "rfc/core/builder.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <string_view>

using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using Owner = PreparedRelationalLiteralFiniteFieldMap;
namespace {
constexpr std::size_t maximum = 1u << 22;
std::size_t checks{};
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
std::vector<std::byte> bytes(std::string_view s) {
    std::vector<std::byte> out; for (const auto c : s) out.push_back(static_cast<std::byte>(c)); return out;
}
storage::NativeTemporalSeries history() {
    core::RelationSignature signature;
    const auto type = signature.add_type("original-use-group", core::BoundaryPolicy::DeclaredExact, 0, 16);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, type,
        {{10, true, true, 1}, {20, true, false, 1}, {10, true, false, 1},
         {20, false, false, 1}, {30, true, false, 0}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(builder.freeze()), 904011);
}
OwnedRelationalFieldProgram program(const storage::NativeTemporalSeries& series) {
    RelationalFieldProgramDeclaration d; d.key = 1001; d.revision = 1; d.differential = true;
    d.stalks = {{{{0},0},101,{1011,1012},M::identity(2)}, {{{0},1},102,{1021},M::identity(1)},
        {{{0},2},102,{1021},M::identity(1)}, {{{1},0},201,{11,12,13,14,15,16,17,18},M::identity(8)}};
    for (std::size_t p = 0; p < 5; ++p) {
        const auto width = p == 0 || p == 2 ? 2u : 1u;
        M map(width,8); map.set(0,0,numeric::rational(static_cast<std::int64_t>(p+1)));
        if (width == 2) map.set(1,1,numeric::rational(static_cast<std::int64_t>(p+2)));
        d.occurrences.push_back({{1},p,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    d.nodes = {{1,FieldProgramNodeKind::Input,{1},{},{}}, {2,FieldProgramNodeKind::Boundary,{0},1,{}},
        {3,FieldProgramNodeKind::DeclaredMap,{0},1,
         M::from_rows({{2,0,0,1,0,0,0,0},{0,1,3,0,0,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,0,2,-1}})}};
    return {series.tail(),d};
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series, std::string_view text = "bank?") {
    RelationalInputUseDeclaration d; d.key = 2001; d.channel = 2011; d.encoding = "UTF-8"; d.bytes = bytes(text);
    d.uses = {{111,{{{0},10},{{1},100}},{},{{0,4}}},
        {112,{{{0},20},{{1},100}},{},{{4,5}}},
        {113,{{{0},30},{{1},100}},{},{{0,2},{2,4},{0,0},{0,2}}}};
    const auto publication = series.tail();
    for (std::size_t slot = 0; slot < 5; ++slot) {
        const auto index = slot == 4 ? 2u : slot % 2;
        d.uses[index].occurrences.push_back({{1},publication.endpoint().source.occurrence_identity({1},slot)});
    }
    return {series,d};
}
OwnedRelationalLiteralInput literal(OwnedRelationalInputUseAsset a, OwnedRelationalFieldProgram p) {
    RelationalInputAccessionDeclaration d; d.key = 3001; d.node = 1; d.source_grade = {1}; d.isometric = true;
    d.source_stalks = {p.declaration().stalks.back()};
    d.instances.cells = {{0,1,2},{0}}; d.instances.occurrences = {{},{0,1,2,3,4}};
    d.accession = M::identity(8); d.reconstruction = M::identity(8);
    OwnedRelationalInputAccession accession(a,p,d);
    RelationalLiteralInputDeclaration r; r.key = 4001; r.revision = 1; r.use = 111;
    const auto add = [&](std::uint64_t use, std::size_t ordinal, RelationalByteSpan span,
                         RelationalLiteralPredicateKind kind, std::string_view operand) {
        const auto i = r.coordinates.size(); r.coordinates.push_back({5001+i,{{1},0},11+i,use,ordinal,span,kind,bytes(operand)});
    };
    using K = RelationalLiteralPredicateKind;
    add(111,0,{0,4},K::Equal,"bank"); add(111,0,{0,4},K::Prefix,"ba");
    add(111,0,{0,4},K::Suffix,"nk"); add(111,0,{0,4},K::AsciiPunctuation,"");
    add(112,0,{4,5},K::AsciiPunctuation,""); add(113,0,{0,2},K::Equal,"ba");
    add(113,1,{2,4},K::Equal,"nk"); add(113,2,{0,0},K::Equal,"");
    return {accession,r};
}
RelationalFiniteSourceDeclaration finite_declaration(OwnedRelationalInputUseAsset a) {
    RelationalFiniteSourceDeclaration d; d.key = 6001; d.revision = 1;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto use = 111+i, symbol = 6101+i;
        d.symbols.push_back({symbol,encode_relational_literal_use_value(a,use,maximum)});
        d.variables.push_back({{{0},static_cast<std::uint32_t>(i)},{symbol}});
        d.bindings.push_back({6201+i,use,{{0},static_cast<std::uint32_t>(i)}});
    }
    d.charts = {{6301,"whole-literal-facts",{{{0},0},{{0},1},{{0},2},{{1},0}},
        {{{0},0},{{0},1},{{0},2}},{{6101,6102,6103}}}};
    return d;
}
RelationalLiteralFiniteFieldDeclaration declaration(OwnedRelationalLiteralInput input) {
    return {7001,1,{{7101,{6201,6202,6203},{6101,6102,6103},0,{std::move(input)}, {}}}};
}
Owner original(std::string_view text = "bank?") {
    const auto h = history(); const auto p = program(h); const auto a = asset(h,text);
    return {OwnedRelationalFiniteSource(a,finite_declaration(a)), {p}, declaration(literal(a,p))};
}
V values(const numeric::SparseRationalTensor& f) {
    V out; for (std::size_t i = 0; i < f.domain().logical_size(); ++i) out.push_back(*f.state(i)); return out;
}
OwnedAccessionFieldResult response(const RelationalLiteralFiniteFieldResult& result) {
    const auto family = result.family(); return std::get<OwnedAccessionFieldResult>(*family.branch(0).value);
}
V field_values(OwnedAccessionFieldResult result, std::uint64_t node) {
    const auto fields = result.result(); return values(fields.field(node));
}
std::vector<std::byte> replace_source(std::span<const std::byte> packet, const OwnedRelationalFiniteSource& source) {
    using namespace relational_finite_field_codec_detail;
    const storage::TemporalReadLimits policy; Reader reader(packet,policy);
    header(reader,packet,literal_finite_field_codec_detail::map_magic); (void)reader.blob();
    storage::BytePacketBuilder writer(literal_finite_field_codec_detail::map_magic,maximum);
    writer.blob(encode_relational_finite_source(source)); writer.write([&](auto& w){w.raw(packet.subspan(reader.position()));});
    return std::move(writer).finish().bytes();
}
void corpus() {
    const auto h = history(); const auto p = program(h); const auto a = asset(h); const auto l = literal(a,p);
    const auto fd = finite_declaration(a); const OwnedRelationalFiniteSource finite(a,fd); const auto d = declaration(l);
    const Owner owner(finite,{p},d); const auto result = owner.execute(); const auto r = response(result).result();
    check(values(r.field(1)) == V{1,1,1,0,1,1,1,1},"whole literal field changed");
    check(values(r.field(2)) == V{2,2,2,0},"actual boundary response differs");
    check(values(r.field(3)) == V{2,4,2,1},"actual declared map response differs");
    check(r.field(1).support().count()==8 && r.field(1).state(3)==numeric::Rational{0},"supported zero lost");
    check(result.map().same_owner(owner) && result.family().map().same_owner(owner.map()),"result lost validated consumer owner");
    check(result.map().source().asset().same_owner(a),"result lost actual byte owner");
    check(owner.declaration().rules[0].inputs[0].same_owner(l),"literal derivation owner replaced");
    const auto actual_response = response(result);
    check(actual_response.inputs()[0].accession().asset().same_owner(a),"actual consumer input lost byte owner");
    check(a.recover(113,maximum)==std::vector<std::vector<std::byte>>{bytes("ba"),bytes("nk"),{},bytes("ba")},"ordered repeated empty cuts lost");
    check(fd.symbols[0].definition != fd.symbols[2].definition,"whole ordered spans collapsed into concatenated bytes");
    const auto changed = original("bend!").execute();
    check(field_values(response(changed),1)==V{0,0,0,0,1,0,0,1},"changed original bytes reused input");
    check(field_values(response(changed),2)==V{0,0,0,0},"changed bytes failed to change full boundary field");
    check(field_values(response(changed),3)==V{0,0,1,-1},"changed bytes failed to change declared map");
    const auto copy = owner; check(copy.same_owner(owner),"copy lost immutable owner");
    check(!Owner(finite,{p},d).same_owner(owner),"equal values established owner identity");
    auto bad = fd; bad.symbols[0].definition = bytes("forged opaque symbol");
    const OwnedRelationalFiniteSource opaque(a,bad);
    check(opaque.sections().size()==1,"generic opaque symbol semantics changed");
    rejects([&]{(void)Owner(opaque,{p},d);},"forged literal definition accepted");
    bad=fd; bad.symbols.push_back({6999,bytes("unrelated")});
    rejects([&]{(void)Owner(OwnedRelationalFiniteSource(a,bad),{p},d);},"underived unused symbol accepted");
    bad=fd; bad.variables[0].domain.push_back(6102);
    rejects([&]{(void)Owner(OwnedRelationalFiniteSource(a,bad),{p},d);},"literal facts accepted interpretation alternatives");
    bad=fd; bad.variables.pop_back(); bad.bindings.pop_back(); bad.symbols.pop_back(); bad.charts.clear();
    rejects([&]{(void)Owner(OwnedRelationalFiniteSource(a,bad),{p},d);},"missing complete original use accepted");
    const auto invalid = [&](auto change,const char* why) {auto edited=d;change(edited);rejects([&]{(void)Owner(finite,{p},edited);},why);};
    invalid([](auto& x){x.key=0;},"zero identity accepted");
    invalid([](auto& x){x.revision=0;},"zero revision accepted");
    invalid([](auto& x){x.rules[0].bindings.pop_back();x.rules[0].symbols.pop_back();},"missing rule byte dependency accepted");
    invalid([](auto& x){x.rules[0].symbols[0]=6102;},"foreign selected literal symbol accepted");
    invalid([](auto& x){x.rules[0].inputs.clear();},"unconsumed literal witness accepted");
    invalid([](auto& x){x.rules[0].target.reset();x.rules[0].unsupported="passive";},"unsupported rule consumed numerical input");
    invalid([](auto& x){x.rules[0].target=9;},"foreign target accepted");
    const auto other = asset(h);
    auto foreign = declaration(literal(other,p));
    rejects([&]{(void)Owner(finite,{p},foreign);},"equal bytes established actual asset identity");
    foreign = declaration(literal(a,program(h)));
    rejects([&]{(void)Owner(finite,{p},foreign);},"equal programs established actual target identity");
    auto bounds = numeric::ExactActionLimits{}; bounds.dimension=2;
    rejects([&]{(void)Owner(finite,{p},d,bounds);},"dimension admission ignored");
    bounds={};bounds.entries=16;
    rejects([&]{(void)Owner(finite,{p},d,bounds);},"entry admission ignored");
    bounds={};bounds.steps=2;
    rejects([&]{(void)Owner(finite,{p},d,bounds);},"work admission ignored");
    bool stopped{}; rejects([&]{(void)Owner(finite,{p},d,{},[&]{stopped=true;throw std::runtime_error("stop");});},"cancellation ignored");
    check(stopped,"cancellation not reached");
    rejects([&]{(void)encode_relational_literal_use_value(a,113,48);},"whole use byte admission ignored");
    const auto image = encode_relational_literal_finite_field_map(owner,maximum);
    const auto restored = decode_relational_literal_finite_field_map(image,storage::TemporalReadLimits{});
    check(encode_relational_literal_finite_field_map(restored,maximum)==image,"map restoration not canonical");
    check(!restored.same_owner(owner) && field_values(response(restored.execute()),2)==V{2,2,2,0},"cold map lost native response");
    check(prepare_relational_literal_finite_field_map(owner,maximum).bytes()==image,"streamed map differs");
    const auto whole = encode_relational_literal_finite_field_result(result,maximum);
    check(prepare_relational_literal_finite_field_result(result,maximum).bytes()==whole,"streamed result differs");
    const auto cold = decode_relational_literal_finite_field_result(whole,storage::TemporalReadLimits{});
    check(encode_relational_literal_finite_field_result(cold,maximum)==whole,"whole result restoration not canonical");
    const auto cold_map = cold.map();
    check(cold_map.declaration().rules[0].inputs[0].input().accession().asset().same_owner(cold_map.source().asset()),"cold derivation did not adopt actual restored owner");
    rejects([&]{(void)decode_relational_literal_finite_field_map(replace_source(image,opaque),storage::TemporalReadLimits{});},"valid checksum forged definition bypassed derivation");
    auto corrupt=whole;corrupt.back()^=std::byte{1};
    rejects([&]{(void)decode_relational_literal_finite_field_result(corrupt,storage::TemporalReadLimits{});},"corrupted witness accepted");
    corrupt=whole;corrupt.push_back(std::byte{0});
    rejects([&]{(void)decode_relational_literal_finite_field_result(corrupt,storage::TemporalReadLimits{});},"trailing bytes accepted");
    for (const auto cut : {std::size_t{0},std::size_t{31},std::size_t{32},whole.size()/2,whole.size()-1})
        rejects([&]{(void)decode_relational_literal_finite_field_result(std::span<const std::byte>(whole).first(cut),storage::TemporalReadLimits{});},"truncated packet accepted");
    auto policy=storage::TemporalReadLimits{};policy.max_file_bytes=32;
    rejects([&]{(void)decode_relational_literal_finite_field_result(whole,policy);},"decoder byte admission ignored");
    policy={};policy.max_records=2;
    rejects([&]{(void)decode_relational_literal_finite_field_result(whole,policy);},"decoder record admission ignored");
    rejects([&]{(void)encode_relational_literal_finite_field_map(owner,32);},"encoder byte admission ignored");
    auto mutable_image=image;bool mutated{};
    const auto frozen=decode_relational_literal_finite_field_map(mutable_image,storage::TemporalReadLimits{}, {}, [&]{
        if(!mutated){mutated=true;std::fill(mutable_image.begin(),mutable_image.end(),std::byte{0});}
    });
    check(mutated && encode_relational_literal_finite_field_map(frozen,maximum)==image,"callback mutated frozen input packet");
    const auto escaped=original().execute();check(field_values(response(escaped),3)==V{2,4,2,1},"result borrowed destroyed producer");
    std::vector<std::future<std::vector<std::byte>>> jobs;
    for (int i=0;i<16;++i) jobs.push_back(std::async(std::launch::async,[copy]{return encode_relational_literal_finite_field_result(copy.execute(),maximum);}));
    for(auto& job:jobs)check(job.get()==whole,"concurrent immutable whole result differs");
    RelationalFieldParameterDeclaration pd{8001,1,2,{{2,1,1},{1,0,4}},M::identity(2),{8101,8102}};
    const PreparedRelationalFieldEvidence evidence(p,pd);
    const Owner affine(finite,{evidence},d);const auto af=affine.execute();
    const auto affine_family = af.family();
    check(std::holds_alternative<RelationalAffineProgramResult>(*affine_family.branch(0).value),"actual evidence target replaced by fixed program");
    const auto evaluated=af.family().evaluate(0,V{3,-2}).result();
    check(field_values(evaluated,2)==V{2,5,0,0},"actual evidence family failed to drive complete response");
    const auto ai=encode_relational_literal_finite_field_result(af,maximum);
    const auto ac=decode_relational_literal_finite_field_result(ai,storage::TemporalReadLimits{});
    check(encode_relational_literal_finite_field_result(ac,maximum)==ai,"affine target cold restoration differs");
    check(field_values(ac.family().evaluate(0,V{3,-2}).result(),2)==V{2,5,0,0},"cold evidence family response differs");
}
} // namespace
int main(int argc,char** argv) {
    try {
        corpus();
        if(argc==3 && std::string_view(argv[1])=="write") {
            const auto image=encode_relational_literal_finite_field_result(original().execute(),maximum);
            std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(image.data()),static_cast<std::streamsize>(image.size()));
            check(bool(out),"write failed");
        } else if(argc==3 && std::string_view(argv[1])=="read") {
            std::ifstream in(argv[2],std::ios::binary);check(bool(in),"read failed");
            const std::vector<char> raw{std::istreambuf_iterator<char>(in),{}};std::vector<std::byte> image;
            for(const auto c:raw)image.push_back(static_cast<std::byte>(c));
            const auto restored=decode_relational_literal_finite_field_result(image,storage::TemporalReadLimits{});
            check(encode_relational_literal_finite_field_result(restored,maximum)==image,"fresh cold result differs");
            check(field_values(response(restored),2)==V{2,2,2,0},"fresh cold full response differs");
        } else check(argc==1,"unknown invocation");
        std::cout<<checks<<" literal finite field checks passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
