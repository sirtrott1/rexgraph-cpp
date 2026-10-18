#include "rfc/learning/relational_literal_field_family_codec.hpp"
#include "rfc/core/builder.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>

using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using Bytes = std::vector<std::byte>;
namespace fs = std::filesystem;
namespace {
std::size_t checks{};
void check(bool yes,const char* why) { ++checks; if (!yes) throw std::runtime_error(why); }
template<class F> void rejects(F f,const char* why) { bool caught{}; try { f(); } catch (const std::exception&) { caught=true; } check(caught,why); }
numeric::SparseRationalTensor tensor(const V& v) { numeric::ExactActionGuard g; return field_interface_detail::tensor(v,g); }
storage::NativeSeriesEndpoint source() {
    core::RelationSignature s; const auto t=s.add_type("relation",core::BoundaryPolicy::DeclaredExact,1,4); core::RelationalComplexBuilder b(s);
    b.add_grade_one(core::RelationKey{100},t,{{10,true,false,1},{20,true,false,1}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(),7),1000).tail();
}
OwnedRelationalFieldProgram program(const storage::NativeSeriesEndpoint& p,bool enlarged) {
    RelationalFieldProgramDeclaration d; d.key=1; d.revision=enlarged?2:1; d.differential=true;
    d.stalks={{{{0},0},100,{101,102},M::identity(2)},{{{0},1},200,{201,202},M::identity(2)},
        {{{1},0},enlarged?400u:300u,enlarged?std::vector<std::uint64_t>{301,302,401}:std::vector<std::uint64_t>{301,302},M::identity(enlarged?3:2)}};
    for (std::size_t i=0;i<2;++i) d.occurrences.push_back({{1},i,storage::TemporalTensorRole::CoefficientIndependentMap,M(2,enlarged?3:2)});
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}}; return {p,d};
}
OwnedRelationalInputUseAsset asset(const storage::NativeSeriesEndpoint& p,bool changed) {
    RelationalInputUseDeclaration d; d.key=3; d.channel=4; d.encoding="UTF-8"; d.bytes={std::byte{'a'}}; if (changed) d.bytes.push_back(std::byte{'b'});
    d.uses={{5,{{{1},100}},{{{1},p.endpoint().source.occurrence_identity({1},0)},{{1},p.endpoint().source.occurrence_identity({1},1)}},{{0,d.bytes.size()}}}};
    return {storage::NativeTemporalSeries(p.endpoint(),2000),d};
}
OwnedRelationalInputAccession accession(const OwnedRelationalInputUseAsset& a,const OwnedRelationalFieldProgram& p) {
    RelationalInputAccessionDeclaration d; d.key=6; d.node=1; d.source_grade={1}; d.isometric=true; d.source_stalks={p.declaration().stalks[2]};
    d.instances={{{0,1},{0}},{{},{0,1}}}; d.accession=M::identity(d.source_stalks[0].directions.size()); d.reconstruction=d.accession; return {a,p,d};
}
OwnedRelationalLiteralInput literal(const OwnedRelationalInputUseAsset& a,const OwnedRelationalFieldProgram& p,bool alternate=false) {
    RelationalLiteralInputDeclaration d; d.key=7; d.revision=1; d.use=5; const auto span=a.use(5).spans[0];
    d.coordinates={{8,{{1},0},301,5,0,span,RelationalLiteralPredicateKind::Equal,{std::byte{'a'}}},
        {9,{{1},0},302,5,0,span,alternate?RelationalLiteralPredicateKind::Suffix:RelationalLiteralPredicateKind::Prefix,{std::byte{'a'}}}};
    return {accession(a,p),d};
}
RelationalKnownTensorGraphDeclaration graph_declaration() { return {10,1,{{{{1},0},300,400,{301},{302},{401}}}}; }
RelationalFieldParameterDeclaration parameters(bool enlarged) {
    RelationalFieldParameterDeclaration d; d.key=11; d.revision=1; d.node=2;
    for (std::size_t r=0;r<2;++r) for (std::size_t c=0;c<(enlarged?3u:2u);++c) { d.variables.push_back({0,r,c}); d.parameters.push_back(1001+d.parameters.size()); }
    d.basis=M::identity(d.parameters.size()); return d;
}
void write(const fs::path& p,const Bytes& bytes) { std::ofstream f(p,std::ios::binary); f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())); check(bool(f),"packet write"); }
Bytes read(const fs::path& p) { std::ifstream f(p,std::ios::binary); check(bool(f),"packet read"); const std::vector<char> raw{std::istreambuf_iterator<char>(f),{}}; Bytes b; for (const auto c:raw)b.push_back(static_cast<std::byte>(c)); return b; }
void run(const fs::path& directory,bool cold) {
    const auto p=source(); const auto before=program(p,false), after=program(p,true); const auto a=asset(p,false), b=asset(p,true);
    const auto original=literal(a,before), intervened=literal(b,before);
    const OwnedRelationalKnownTensorGraph graph(original,graph_declaration()), other_graph(intervened,graph_declaration());
    const auto input=graph.admit(accession(a,after)), other=other_graph.admit(accession(b,after));
    const PreparedRelationalFieldEvidence root(after,parameters(true));
    const PreparedRelationalFieldFamily family({12,1,{}},{root}); const RelationalLiteralJointResult result(family,0,{input}), changed(family,0,{other});
    const auto result_family=result.family();
    check(result_family.same_owner(family) && result_family.members()[0].same_owner(root),"existing family and evidence owners reused");
    check(result.field(1).particular.values().dense()==V({1,1,1}) && changed.field(1).particular.values().dense()==V({0,1,0}),"independently supplied original bytes condition whole input");
    check(result.field(1).directions.nonzeros()==0 && result.field(2).directions.cols()==6,"known inputs retain all joint alternatives");
    const V free{0,0,2,0,0,3}; const auto evaluated=result.evaluate(free), changed_evaluation=changed.evaluate(free);
    const auto all=evaluated.result().result(), changed_fields=changed_evaluation.result().result();
    check(all.field(2).values().dense()==V({2,3,0,0}) && changed_fields.field(2).values().dense()==V({0,0,0,0}),"two product map directions change complete boundary response");
    check(all.occurrence_field(2).values().dense()==V({2,3,0,0}),"complete original occurrence field retained");
    const auto& retained=std::get<RelationalKnownTensorGraphInput>(result.inputs()[0]);
    check(retained.graph().literal_parent()->same_owner(original) && retained.graph().parent().accession().asset().same_owner(a),"graph operands own original literal recipe and bytes");
    const auto training=execute_relational_inputs(after,{input.input()});
    const auto fitted=root.append({{13,training,a,5,tensor({3,5,0,0}),{{{0},0},{{0},1}}}});
    const PreparedRelationalFieldFamily learned_family({14,1,{}},{fitted}); const RelationalLiteralJointResult learned(learned_family,0,{input});
    check(fitted.parent()->same_owner(root) && learned.field(2).directions.cols()==4,"trained lineage and correlated family retained");
    const auto learned_evaluation=learned.evaluate({1,2,3,4}); const auto learned_fields=learned_evaluation.result().result();
    check(learned_fields.field(2).values().dense()==V({3,5,0,0}),"all learned correlated choices preserve full observation");
    const PreparedRelationalFieldFamily plain_family({15,1,{}},{PreparedRelationalFieldEvidence(before,parameters(false))});
    const RelationalLiteralJointResult plain(plain_family,0,{original}), equivalent(plain_family,0,{literal(a,before,true)});
    check(plain.field(1).particular.values().dense()==equivalent.field(1).particular.values().dense(),"equal numerical predicate encodings");
    check(encode_relational_literal_joint_result(plain,1u<<24)!=encode_relational_literal_joint_result(equivalent,1u<<24),"equal encodings preserve distinct original predicate recipes");
    const auto bytes=encode_relational_literal_joint_result(result,1u<<24), materialized=encode_relational_literal_joint_evaluation(evaluated,1u<<24);
    if (!cold) { write(directory/"result.packet",bytes); write(directory/"evaluation.packet",materialized); write(directory/"plain.packet",encode_relational_literal_joint_result(plain,1u<<24)); write(directory/"learned.packet",encode_relational_literal_joint_evaluation(learned_evaluation,1u<<24)); }
    const auto restored=decode_relational_literal_joint_result(cold?read(directory/"result.packet"):bytes,{});
    check(encode_relational_literal_joint_result(restored,1u<<24)==bytes,"cold full joint owner canonical bytes");
    const auto& restored_graph=std::get<RelationalKnownTensorGraphInput>(restored.inputs()[0]); const auto restored_family=restored.family();
    check(restored_graph.input().accession().program().same_owner(restored_family.members()[0].program()),"cold literal input adopts actual restored member owner");
    check(restored_graph.graph().literal_parent()->declaration().coordinates.size()==2,"cold graph retains actual original predicates");
    const auto restored_evaluation=decode_relational_literal_joint_evaluation(cold?read(directory/"evaluation.packet"):materialized,{});
    check(encode_relational_literal_joint_evaluation(restored_evaluation,1u<<24)==materialized && restored_evaluation.free_values()==free,"cold whole prospective evaluation");
    const auto plain_image=cold?read(directory/"plain.packet"):encode_relational_literal_joint_result(plain,1u<<24);
    check(encode_relational_literal_joint_result(decode_relational_literal_joint_result(plain_image,{}),1u<<24)==plain_image,"cold direct literal adoption canonical");
    const auto learned_image=cold?read(directory/"learned.packet"):encode_relational_literal_joint_evaluation(learned_evaluation,1u<<24);
    const auto cold_learned=decode_relational_literal_joint_evaluation(learned_image,{}); const auto cold_parent=cold_learned.parent(); const auto cold_family=cold_parent.family();
    check(cold_family.members()[0].parent().has_value() && cold_family.members()[0].observations()[0].target.values().dense()==V({3,5,0,0}),"cold full observed target and evidence parent retained");
    std::vector<std::byte> streamed; prepare_relational_literal_joint_result(result,1u<<24).stream([&](auto part){streamed.insert(streamed.end(),part.begin(),part.end());}); check(streamed==bytes,"bounded prepared family stream");
    rejects([&]{RelationalLiteralJointResult wrong(family,0,{graph.admit(accession(a,program(p,true)))});},"foreign live program owner refused");
    const OwnedRelationalKnownTensorGraph generic(original.input(),graph_declaration());
    rejects([&]{RelationalLiteralJointResult wrong(family,0,{generic.admit(accession(a,after))});},"numeric graph cannot claim lost literal recipe");
    rejects([&]{RelationalLiteralJointResult wrong(family,0,{});},"complete input nodes required");
    rejects([&]{RelationalLiteralJointResult wrong(family,0,{input,input});},"duplicate input node refused");
    rejects([&]{result.evaluate({1});},"all joint free coordinates required");
    rejects([&]{result.occurrence_field(1);},"no invented input flags");
    rejects([&]{encode_relational_literal_joint_result(result,bytes.size()-1);},"bounded byte admission");
    rejects([&]{decode_relational_literal_joint_result(std::span(bytes).first(bytes.size()-1),{});},"truncation refused");
    auto corrupt=bytes; corrupt.back()^=std::byte{1}; rejects([&]{decode_relational_literal_joint_result(corrupt,{});},"corrupt full field witness refused");
    auto limits=family.limits(); --limits.entries; rejects([&]{decode_relational_literal_joint_result(bytes,{},limits);},"recorded policy not silently changed");
    rejects([&]{RelationalLiteralJointResult wrong(family,0,{input},{},[]{throw std::runtime_error("stop");});},"joint construction cancellation");
    rejects([&]{decode_relational_literal_joint_evaluation(materialized,{}, {},[]{throw std::runtime_error("stop");});},"joint cold cancellation");
    auto mutable_bytes=bytes; bool mutated{}; const auto frozen=decode_relational_literal_joint_result(mutable_bytes,{}, {},[&]{if(!mutated){std::fill(mutable_bytes.begin(),mutable_bytes.end(),std::byte{});mutated=true;}});
    check(mutated && encode_relational_literal_joint_result(frozen,1u<<24)==bytes,"byte image owned before callbacks");
    auto task=[&]{return encode_relational_literal_joint_evaluation(result.evaluate(free),1u<<24);};
    auto first=std::async(std::launch::async,task), second=std::async(std::launch::async,task); check(first.get()==materialized && second.get()==materialized,"immutable concurrent result use");
    const auto lifetime=[&]{return RelationalLiteralJointResult(PreparedRelationalFieldFamily({16,1,{}},{PreparedRelationalFieldEvidence(after,parameters(true))}),0,{graph.admit(accession(a,after))});}();
    check(lifetime.field(1).particular.values().dense()==V({1,1,1}),"result retains temporary family and literal owners");
}
}
int main(int argc,char** argv) {
    try {
        const bool cold=argc==3 && std::string_view(argv[1])=="read"; const auto dir=argc==3?fs::path(argv[2]):fs::temp_directory_path()/("rfc-literal-family-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(dir); run(dir,cold); if(argc==1)fs::remove_all(dir); std::cout<<checks<<" literal field family checks passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
