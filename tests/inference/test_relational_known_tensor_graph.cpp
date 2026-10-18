#include "rfc/inference/relational_known_tensor_graph_codec.hpp"
#include "rfc/core/builder.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>

using namespace rfc;
using namespace rfc::inference;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using T = numeric::SparseRationalTensor;
using Bytes = std::vector<std::byte>;
namespace fs = std::filesystem;
namespace {
std::size_t checks{};
void check(bool yes,const char* why) { ++checks; if (!yes) throw std::runtime_error(why); }
template<class F> void rejects(F f,const char* why) {
    bool caught{}; try { f(); } catch (const std::exception&) { caught=true; } check(caught,why);
}
T tensor(const V& v) { numeric::ExactActionGuard g; return field_interface_detail::tensor(v,g); }
storage::NativeSeriesEndpoint source() {
    core::RelationSignature s; const auto type=s.add_type("relation",core::BoundaryPolicy::DeclaredExact,1,8);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(core::RelationKey{100},type,{{10,true,false,1},{20,true,true,1},{10,true,true,1},{20,false,false,1},{30,true,false,0}});
    b.add_grade_one(core::RelationKey{200},type,{{20,true,false,1},{30,true,false,1}});
    b.add_grade_one(core::RelationKey{300},type,{{30,true,false,1}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(),7),1000).tail();
}
OwnedRelationalInputUseAsset asset(const storage::NativeSeriesEndpoint& p) {
    storage::NativeTemporalSeries history(p.endpoint(),2000); auto next=p.endpoint(); next.sequence=8; history.append_target(next);
    RelationalInputUseDeclaration d; d.key=3; d.channel=4; d.encoding="UTF-8"; d.bytes={std::byte{'a'}};
    RelationalSpanUse use{5,{{{1},100},{{1},200},{{1},300}},{},{{0,1},{0,0},{0,1}}};
    for (std::size_t i=0;i<8;++i) use.occurrences.push_back({{1},p.endpoint().source.occurrence_identity({1},i)});
    d.uses={use}; return {history,d};
}
std::vector<StalkBasisDeclaration> original_stalks() {
    return {{{{1},0},100,{101,102,103},M::from_rows({{2,1,1},{1,3,0},{1,0,5}})},
        {{{1},1},200,{201,202},M::from_rows({{3,1},{1,2}})}, {{{1},2},300,{301},M::identity(1)}};
}
OwnedRelationalFieldProgram program(const storage::NativeSeriesEndpoint& p,const std::vector<StalkBasisDeclaration>& upper,
    std::uint64_t revision) {
    RelationalFieldProgramDeclaration d; d.key=1; d.revision=revision; d.differential=true;
    for (std::uint32_t i=0;i<3;++i) d.stalks.push_back({{{0},i},400,{401,402},M::identity(2)});
    d.stalks.insert(d.stalks.end(),upper.begin(),upper.end());
    const auto& g=p.endpoint().source.grade({1});
    for (std::size_t i=0;i<8;++i) {
        const auto relation=g.upper_index.at(i); const auto width=upper[relation].directions.size(); M map(2,width);
        map.set(0,0,1); if (width>1) map.set(1,1,1);
        if (width>original_stalks()[relation].directions.size()) map.set(0,width-1,2);
        d.occurrences.push_back({{1},i,storage::TemporalTensorRole::CoefficientIndependentMap,map});
    }
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}}; return {p,d};
}
OwnedRelationalInputAccession accession(const OwnedRelationalInputUseAsset& bytes,const OwnedRelationalFieldProgram& p,
    std::vector<StalkBasisDeclaration> stalks) {
    RelationalInputAccessionDeclaration d; d.key=6; d.node=1; d.source_grade={1}; d.isometric=true; d.source_stalks=std::move(stalks);
    d.instances={{{0,1,2},{0,1,2}},{{},{0,1,2,3,4,5,6,7}}}; std::size_t width{};
    for (const auto& s:d.source_stalks) width+=s.directions.size();
    d.accession=M::identity(width); d.reconstruction=d.accession; return {bytes,p,d};
}
RelationalKnownTensorGraphDeclaration declaration() {
    return {7,1,{{{{1},0},100,500,{101,102},{103},{501,502}},{{{1},1},200,600,{201},{202},{601}}}};
}
OwnedRelationalLiteralInput literal(const OwnedRelationalInputAccession& a) {
    RelationalLiteralInputDeclaration d; d.key=8; d.revision=1; d.use=5;
    for (const auto& s:a.declaration().source_stalks) for (const auto direction:s.directions)
        d.coordinates.push_back({10+d.coordinates.size(),s.cell,direction,5,0,{0,1},RelationalLiteralPredicateKind::Equal,{std::byte{'a'}}});
    return {a,d};
}
void write(const fs::path& p,const Bytes& b) {
    std::ofstream f(p,std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size())); check(bool(f),"write packet");
}
Bytes read(const fs::path& p) {
    std::ifstream f(p,std::ios::binary); check(bool(f),"read packet"); const std::vector<char> raw{std::istreambuf_iterator<char>(f),{}};
    Bytes b; for (const auto c:raw) b.push_back(static_cast<std::byte>(c)); return b;
}
void run(const fs::path& directory,bool cold) {
    const auto publication=source(); const auto bytes=asset(publication); const auto before=program(publication,original_stalks(),1);
    const auto a=accession(bytes,before,original_stalks()); const auto original=a.admit(5,tensor({2,3,5,7,11,13}));
    const OwnedRelationalKnownTensorGraph graph(original,declaration());
    const auto after=program(publication,graph.stalks(),2); const auto target=accession(bytes,after,graph.stalks()); const auto joined=graph.admit(target);
    check(graph.lifted_field().values().dense()==V({2,3,5,10,15,7,11,77,13}),"heterogeneous graph, original prefixes and untouched complement");
    check(graph.product({{1},0}).domain().extents()==std::vector<std::size_t>({2,1}) && graph.product({{1},0}).values().dense()==V({10,15}),"ordered named operands");
    check(graph.product({{1},1}).values().dense()==V{77},"second actual source stalk");
    check(graph.stalks()[2].basis==300 && graph.stalks()[2].directions==std::vector<std::uint64_t>({301}),"unselected identity retained");
    numeric::ExactActionGuard guard;
    const auto lifted=graph.lifted_field().values().dense(), old=original.source_field().values().dense();
    check(numeric::checked_apply(graph.projection(),lifted,guard)==old,"complete numeric reconstruction");
    check(numeric::checked_multiply(graph.metric(),graph.zero_section(),guard)==numeric::checked_multiply(graph.projection().transpose(),graph.original_metric(),guard),"actual projection metric adjoint");
    const numeric::Rational product_norm=numeric::dot(V{10,15},M::from_rows({{10,5},{5,15}}).apply({10,15}))+numeric::Rational{6*77*77};
    check(numeric::dot(lifted,graph.metric().apply(lifted))==numeric::dot(old,graph.original_metric().apply(old))+product_norm,"nonorthogonal operand metrics and original cross terms");
    graph.require_image(graph.lifted_field()); auto changed=lifted; ++changed[3];
    check(numeric::checked_apply(graph.projection(),changed,guard)==old,"projection alone does not admit graph image");
    rejects([&]{graph.require_image(tensor(changed));},"off graph field refused");
    rejects([&]{graph.require_image(tensor(numeric::checked_apply(graph.zero_section(),old,guard)));},"zero section is not nonlinear graph");
    const auto result=execute_relational_inputs(after,{joined.input()}); const auto fields=result.result();
    check(fields.field(1).values().dense()==lifted && fields.occurrence_field(2).domain().logical_size()==16,"complete input and all original flags executed");
    const auto zeros=a.admit(5,tensor({0,3,0,0,11,13})); const OwnedRelationalKnownTensorGraph zero_graph(zeros,declaration());
    check(zero_graph.lifted_field().support().count()==9 && zero_graph.product({{1},0}).support().count()==2 && zero_graph.product({{1},0}).values().nonzeros()==0,"supported zero product retained");
    const auto li=literal(a); const OwnedRelationalKnownTensorGraph literal_graph(li,declaration());
    check(literal_graph.literal_parent()->same_owner(li) && literal_graph.parent().accession().asset().same_owner(bytes),"actual literal recipe and original asset retained");
    const auto image=encode_relational_known_tensor_graph(graph,1u<<24), packet=encode_relational_known_tensor_graph_input(joined,1u<<24);
    if (!cold) { write(directory/"graph.packet",image); write(directory/"input.packet",packet); write(directory/"literal.packet",encode_relational_known_tensor_graph(literal_graph,1u<<24)); }
    const auto restored=decode_relational_known_tensor_graph(cold?read(directory/"graph.packet"):image,{});
    check(!restored.same_owner(graph) && encode_relational_known_tensor_graph(restored,1u<<24)==image,"cold generic graph canonical ancestry");
    const auto restored_joined=decode_relational_known_tensor_graph_input(cold?read(directory/"input.packet"):packet,{}); const auto restored_input=restored_joined.input();
    check(restored_input.accession().asset().same_owner(restored_joined.graph().parent().accession().asset()),"cold downstream uses actual restored upstream asset");
    check(encode_accession_field_result(execute_relational_inputs(restored_input.accession().program(),{restored_input}))==encode_accession_field_result(result),"cold complete output fields and original flags");
    check(encode_relational_known_tensor_graph_input(restored_joined,1u<<24)==packet,"cold input canonical packet");
    const auto literal_image=cold?read(directory/"literal.packet"):encode_relational_known_tensor_graph(literal_graph,1u<<24);
    const auto restored_literal=decode_relational_known_tensor_graph(literal_image,{}); const auto literal_parent=restored_literal.literal_parent();
    check(literal_parent.has_value() && literal_parent->declaration().coordinates.size()==6,"cold complete literal recipe retained");
    check(restored.parent().accession().asset().history().first_sequence()==7 && restored.parent().accession().asset().history().last_sequence()==8,"original endpoint history retained");
    check(restored.parent().accession().asset().recover(5,3).size()==3,"original repeated and empty spans retained");
    std::vector<std::byte> streamed; prepare_relational_known_tensor_graph_input(joined,1u<<24).stream([&](auto part){streamed.insert(streamed.end(),part.begin(),part.end());});
    check(streamed==packet,"bounded packet streaming");
    auto mutable_image=image; bool mutated{}; const auto frozen=decode_relational_known_tensor_graph(mutable_image,{}, {},[&]{if(!mutated){std::fill(mutable_image.begin(),mutable_image.end(),std::byte{});mutated=true;}});
    check(mutated && encode_relational_known_tensor_graph(frozen,1u<<24)==image,"caller byte image frozen before callbacks");
    auto d=declaration(); d.blocks[0].products.pop_back(); rejects([&]{OwnedRelationalKnownTensorGraph bad(original,d);},"incomplete products refused");
    d=declaration(); d.blocks[0].products[0]=101; rejects([&]{OwnedRelationalKnownTensorGraph bad(original,d);},"product direction collision refused");
    d=declaration(); d.blocks[0].left={102,102}; rejects([&]{OwnedRelationalKnownTensorGraph bad(original,d);},"duplicate operand refused");
    d=declaration(); d.blocks[0].source_basis=999; rejects([&]{OwnedRelationalKnownTensorGraph bad(original,d);},"equal dimensions do not identify source basis");
    d=declaration(); d.blocks.push_back(d.blocks[0]); rejects([&]{OwnedRelationalKnownTensorGraph bad(original,d);},"repeated actual stalk refused");
    d=declaration(); d.blocks[0].cell={{0},0}; rejects([&]{OwnedRelationalKnownTensorGraph bad(original,d);},"unrelated grade refused");
    rejects([&]{graph.product({{1},2});},"no product invented on untouched complement");
    const auto foreign=accession(asset(publication),after,graph.stalks()); rejects([&]{graph.admit(foreign);},"equal bytes do not identify foreign asset");
    const auto unchanged=program(publication,graph.stalks(),1); rejects([&]{graph.admit(accession(bytes,unchanged,graph.stalks()));},"new carrier requires explicit program revision");
    auto wrong=target.declaration(); std::swap(wrong.source_stalks[0].directions[3],wrong.source_stalks[0].directions[4]);
    const OwnedRelationalInputAccession reordered(bytes,after,wrong); rejects([&]{graph.admit(reordered);},"equal dimensions do not identify ordered products");
    auto limits=graph.limits(); --limits.entries; rejects([&]{graph.admit(target,limits);},"original resource policy required");
    rejects([&]{decode_relational_known_tensor_graph(image,{},limits);},"cold policy cannot silently change");
    rejects([&]{encode_relational_known_tensor_graph_input(joined,packet.size()-1);},"exact byte cap");
    rejects([&]{decode_relational_known_tensor_graph_input(std::span(packet).first(packet.size()-1),{});},"truncated packet refused");
    auto corrupt=image; corrupt.back()^=std::byte{1}; rejects([&]{decode_relational_known_tensor_graph(corrupt,{});},"changed witness refused");
    rejects([&]{OwnedRelationalKnownTensorGraph bad(original,declaration(),{},[]{throw std::runtime_error("stop");});},"construction cancellation");
    rejects([&]{decode_relational_known_tensor_graph(image,{}, {},[]{throw std::runtime_error("stop");});},"cold cancellation");
    auto task=[&]{return encode_relational_known_tensor_graph_input(graph.admit(target),1u<<24);};
    auto first=std::async(std::launch::async,task), second=std::async(std::launch::async,task);
    check(first.get()==packet && second.get()==packet,"concurrent immutable graph use");
}
}
int main(int argc,char** argv) {
    try {
        const bool cold=argc==3 && std::string_view(argv[1])=="read";
        const auto dir=argc==3?fs::path(argv[2]):fs::temp_directory_path()/("rfc-known-graph-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(dir); run(dir,cold); if (argc==1) fs::remove_all(dir);
        std::cout<<checks<<" known input graph checks passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
