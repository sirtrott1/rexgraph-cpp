#include "rfc/inference/relational_literal_input_codec.hpp"
#include "rfc/core/builder.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <optional>
#include <string_view>
#include <type_traits>

using namespace rfc;
using namespace rfc::inference;
using M = numeric::ExactMatrix;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using Owner = OwnedRelationalLiteralInput;
using Kind = RelationalLiteralPredicateKind;
namespace {
std::size_t checks{};
void check(bool condition, const char* message) {
    ++checks; if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; }
    check(caught, message);
}
std::vector<std::byte> bytes(std::string_view text) {
    std::vector<std::byte> out;
    for (const auto c : text) out.push_back(static_cast<std::byte>(c));
    return out;
}
storage::NativeTemporalSeries history() {
    core::RelationSignature signature;
    const auto type = signature.add_type("original-use-group", core::BoundaryPolicy::DeclaredExact, 0, 16);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, type,
        {{10, true, true, 1}, {20, true, false, 1}, {10, true, false, 1},
         {20, false, false, 1}, {30, true, false, 0}});
    storage::TemporalEndpoint endpoint(builder.freeze());
    return storage::NativeTemporalSeries(endpoint, 904010);
}
OwnedRelationalFieldProgram program(const storage::NativeTemporalSeries& series) {
    RelationalFieldProgramDeclaration d; d.key = 1001; d.revision = 1; d.differential = true;
    d.stalks = {{{{0}, 0}, 101, {1011, 1012}, M::identity(2)},
                {{{0}, 1}, 102, {1021}, M::identity(1)},
                {{{0}, 2}, 102, {1021}, M::identity(1)},
                {{{1}, 0}, 201, {11, 12, 13, 14, 15, 16, 17, 18}, M::identity(8)}};
    const auto phi = storage::TemporalTensorRole::CoefficientIndependentMap;
    for (std::size_t p = 0; p < 5; ++p) {
        const auto dimension = p == 0 || p == 2 ? 2u : 1u;
        M map(dimension, 8); map.set(0, 0, 1);
        d.occurrences.push_back({{1}, p, phi, std::move(map)});
    }
    d.nodes = {{1, FieldProgramNodeKind::Input, {1}, {}, {}},
               {2, FieldProgramNodeKind::DeclaredMap, {0}, 1,
                M::from_rows({{2,0,0,1,0,0,0,0}, {0,1,3,0,0,0,0,0},
                              {0,0,0,0,1,1,0,0}, {0,0,0,0,0,0,2,-1}})}};
    return {series.tail(), d};
}
OwnedRelationalInputAccession accession(std::string_view text = "bank?",
    std::optional<OwnedRelationalFieldProgram> target = {}) {
    const auto series = history(); const auto p = target ? *target : program(series);
    RelationalInputUseDeclaration d; d.key = 2001; d.channel = 2011; d.encoding = "UTF-8"; d.bytes = bytes(text);
    const std::vector<storage::StableCellKey> cells{{{1}, 100}};
    d.uses = {{111, cells, {}, {{0,4}}}, {112, cells, {}, {{4,5}}},
              {113, cells, {}, {{0,2}, {2,4}, {0,0}, {0,2}}}};
    OwnedRelationalInputUseAsset asset(series, d);
    RelationalInputAccessionDeclaration a; a.key = 3001; a.node = 1; a.source_grade = {1}; a.isometric = true;
    a.source_stalks = {p.declaration().stalks.back()};
    a.instances.cells = {{0,1,2}, {0}}; a.instances.occurrences = {{}, {0,1,2,3,4}};
    a.accession = M::identity(8); a.reconstruction = M::identity(8);
    return {asset, p, a};
}
RelationalLiteralInputDeclaration declaration() {
    RelationalLiteralInputDeclaration d; d.key = 4001; d.revision = 1; d.use = 111;
    const auto add = [&](std::uint64_t use, std::size_t ordinal, RelationalByteSpan span,
                         Kind kind, std::string_view operand) {
        const auto i = d.coordinates.size();
        d.coordinates.push_back({5001+i, {{1},0}, 11+i, use, ordinal, span, kind, bytes(operand)});
    };
    add(111,0,{0,4},Kind::Equal,"bank"); add(111,0,{0,4},Kind::Prefix,"ba");
    add(111,0,{0,4},Kind::Suffix,"nk"); add(111,0,{0,4},Kind::AsciiPunctuation,"");
    add(112,0,{4,5},Kind::AsciiPunctuation,""); add(113,0,{0,2},Kind::Equal,"ba");
    add(113,1,{2,4},Kind::Equal,"nk"); add(113,2,{0,0},Kind::Equal,"");
    return d;
}
Owner original() { return {accession(), declaration()}; }
V values(const numeric::SparseRationalTensor& field) {
    V out; for (std::size_t i = 0; i < field.domain().logical_size(); ++i) out.push_back(*field.state(i));
    return out;
}
V execute(const Owner& owner) {
    const auto a = owner.input().accession();
    const auto result = execute_relational_inputs(a.program(), {owner.input()}).result();
    return values(result.field(2));
}
V observe(const OwnedRelationalInputAccession& a, const RelationalLiteralInputDeclaration& d) {
    const Owner owner(a,d); return values(owner.input().source_field());
}
void corpus() {
    const auto a = accession(); const auto d = declaration(); Owner owner(a, d);
    check(values(owner.input().source_field()) == V{1,1,1,0,1,1,1,1}, "literal complete field differs");
    check(owner.input().source_field().support().count() == 8 && owner.input().source_field().state(3) == Q{0},
        "observed false lost supported zero");
    check(execute(owner) == V{2,4,2,1}, "source-grounded field did not drive native map");
    const Owner changed(accession("bend!", a.program()), d);
    check(changed.input().accession().program().same_owner(a.program()), "new input failed to reuse the actual program owner");
    check(values(changed.input().source_field()) == V{0,0,0,0,1,0,0,1}, "changed bytes reused a stale field");
    check(execute(changed) == V{0,0,1,-1}, "changed bytes did not change whole response");
    check(!owner.input().accession().asset().same_owner(changed.input().accession().asset()), "independent input owners collapsed");
    const auto foreign = original();
    rejects([&]{(void)execute_relational_inputs(a.program(), {foreign.input()});}, "foreign equal-valued program owner accepted");
    const auto copy = owner; check(copy.same_owner(owner), "immutable copy lost owner");
    check(!Owner(a,d).same_owner(owner), "equal observation values established owner identity");
    check(execute(original()) == V{2,4,2,1}, "temporary construction left a borrowed asset");
    auto edited = d; edited.coordinates[0].operand = bytes("bend");
    check(observe(a,edited)[0] == 0, "operand change was ignored");
    edited = d; edited.coordinates[5].operand = bytes("bank");
    check(observe(a,edited)[5] == 0, "discontinuous cuts were implicitly concatenated");
    edited = d; edited.coordinates[0].kind = Kind::Prefix; edited.coordinates[0].operand.clear();
    check(observe(a,edited)[0] == 1, "empty prefix semantics differ");
    edited.coordinates[0].kind = Kind::Suffix;
    check(observe(a,edited)[0] == 1, "empty suffix semantics differ");
    edited = d; edited.coordinates[7].kind = Kind::AsciiPunctuation;
    check(observe(a,edited)[7] == 0, "empty span was punctuation");
    edited = d; edited.coordinates[0].operand = bytes(std::string_view("ba\0k",4));
    const Owner zero_byte(accession(std::string_view("ba\0k?",5)), edited);
    check(values(zero_byte.input().source_field())[0] == 1, "supported zero byte became absence");
    edited = d; edited.coordinates[1].operand = {std::byte{0xc3}};
    check(observe(accession("\xc3\xa9nk?"),edited)[1] == 1,
        "byte prefix silently applied Unicode normalization");
    const auto invalid = [&](auto change, const char* why) { auto bad = d; change(bad); rejects([&]{(void)Owner(a,bad);},why); };
    invalid([](auto& x){x.key=0;},"zero declaration key accepted");
    invalid([](auto& x){x.revision=0;},"zero declaration revision accepted");
    invalid([](auto& x){x.use=999;},"foreign input use accepted");
    invalid([](auto& x){x.coordinates.pop_back();},"partial field accepted");
    invalid([](auto& x){x.coordinates[0].key=0;},"zero predicate key accepted");
    invalid([](auto& x){x.coordinates[1].key=x.coordinates[0].key;},"duplicate predicate key accepted");
    invalid([](auto& x){x.coordinates[0].cell.index=1;},"foreign output cell accepted");
    invalid([](auto& x){std::swap(x.coordinates[0].direction,x.coordinates[1].direction);},"reordered basis accepted");
    invalid([](auto& x){x.coordinates[0].use=999;},"foreign predicate use accepted");
    invalid([](auto& x){x.coordinates[0].span_index=99;},"missing span ordinal accepted");
    invalid([](auto& x){x.coordinates[0].span={1,4};},"changed original cut accepted");
    invalid([](auto& x){x.coordinates[0].kind=static_cast<Kind>(99);},"unknown predicate accepted");
    invalid([](auto& x){x.coordinates[3].operand={std::byte{1}};},"punctuation operand accepted");
    auto bounds = numeric::ExactActionLimits{}; bounds.dimension=7;
    rejects([&]{(void)Owner(a,d,bounds);},"dimension admission ignored");
    bounds = {}; bounds.entries=3;
    rejects([&]{(void)Owner(a,d,bounds);},"entry admission ignored");
    bounds = {}; bounds.steps=2;
    rejects([&]{(void)Owner(a,d,bounds);},"work admission ignored");
    bool cancelled{};
    rejects([&]{(void)Owner(a,d,{},[&]{cancelled=true;throw std::runtime_error("stop");});},"cancellation ignored");
    check(cancelled, "cancellation callback was not reached");
    const auto archive = encode_relational_literal_input(owner, 1u<<20);
    const storage::TemporalReadLimits policy;
    const auto restored = decode_relational_literal_input(archive, policy);
    check(!restored.same_owner(owner) && execute(restored) == execute(owner), "codec failed to rebuild actual observation owner");
    check(archive == encode_relational_literal_input(restored, 1u<<20), "codec was not canonical");
    check(prepare_relational_literal_input(owner,1u<<20).bytes() == archive,"streamed codec differs");
    rejects([&]{(void)encode_relational_literal_input(owner,32);},"encoder byte admission ignored");
    bounds={}; bounds.entries=3;
    rejects([&]{(void)decode_relational_literal_input(archive,policy,bounds);},"decoder entry admission ignored");
    auto corrupted=archive;corrupted.back()^=std::byte{1};
    rejects([&]{(void)decode_relational_literal_input(corrupted,policy);},"corrupted archive accepted");
    corrupted=archive;corrupted.push_back(std::byte{0});
    rejects([&]{(void)decode_relational_literal_input(corrupted,policy);},"trailing archive accepted");
    std::vector<std::future<V>> jobs;
    for (int i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[copy]{return execute(copy);}));
    for (auto& job:jobs) check(job.get()==V{2,4,2,1},"concurrent immutable field action differs");
}
} // namespace
int main(int argc,char** argv) {
    try {
        corpus();
        if (argc==3 && std::string_view(argv[1])=="--write") {
            const auto archive=encode_relational_literal_input(original(),1u<<20);
            std::ofstream stream(argv[2],std::ios::binary);
            stream.write(reinterpret_cast<const char*>(archive.data()),static_cast<std::streamsize>(archive.size()));
            check(bool(stream),"archive write failed");
        } else if (argc==3 && std::string_view(argv[1])=="--read") {
            std::ifstream stream(argv[2],std::ios::binary);check(bool(stream),"archive read failed");
            const std::vector<char> raw{std::istreambuf_iterator<char>(stream),{}};
            std::vector<std::byte> archive;for(const auto c:raw)archive.push_back(static_cast<std::byte>(c));
            const auto owner=decode_relational_literal_input(archive,storage::TemporalReadLimits{});
            check(execute(owner)==V{2,4,2,1},"cold restored full response differs");
            check(encode_relational_literal_input(owner,1u<<20)==archive,"cold canonical archive differs");
        } else check(argc==1,"unknown invocation");
        std::cout<<checks<<" literal input checks passed\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
