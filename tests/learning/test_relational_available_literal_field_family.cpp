#include "rfc/learning/relational_available_literal_field_family_codec.hpp"
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
storage::NativeSeriesEndpoint source(std::uint64_t ns=1000) {
    core::RelationSignature s; const auto t=s.add_type("relation",core::BoundaryPolicy::DeclaredExact,1,4); core::RelationalComplexBuilder b(s);
    b.add_grade_one(core::RelationKey{100},t,{{10,true,false,1},{20,true,false,1}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(),7),ns).tail();
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
using Origin=OwnedRelationalValueOrigin;
using Role=RelationalDependencyRole;
using Context=RelationalAvailabilityContext;
using Grant=RelationalOriginGrant;
constexpr std::size_t maximum=64u<<20;
struct Publisher {
    std::vector<Grant> grants;std::uint64_t next=10000;
    storage::NativeSeriesPrefix publication;
    explicit Publisher(const storage::NativeSeriesEndpoint& p):publication(storage::NativeSeriesPrefix::capture(storage::NativeTemporalSeries(p.endpoint(),9000),p.sequence())){}
    Context context() const {return Context(grants);}
    std::uint64_t publish(Role role,Bytes payload) {
        const auto key=next++;Origin origin(key,publication,role,std::move(payload));grants.push_back({origin,{publication.source_namespace(),publication.cutoff()}});return key;
    }
    RelationalProgramOrigins program_keys(const OwnedRelationalFieldProgram& p) {
        const auto bytes=encode_relational_field_program(p);return {publish(Role::Program,bytes),publish(Role::Maps,bytes),publish(Role::Metrics,bytes)};
    }
    RelationalInputOrigins input_keys(const RelationalAccessionInput& input) {
        const auto a=input.accession();const auto accession_bytes=relational_input_accession_payload(input);
        return {publish(Role::InputAsset,encode_relational_input_use_asset(a.asset())),publish(Role::InputValue,relational_input_value_payload(input)),publish(Role::InputAccession,accession_bytes),publish(Role::Reconstruction,accession_bytes)};
    }
    RelationalAvailableJointInput reader(RelationalLiteralFamilyInput input) {
        RelationalJointReaderOrigins k;k.input=input_keys(available_joint_detail::raw(input));
        if(const auto* graph=std::get_if<RelationalKnownTensorGraphInput>(&input)) {const auto parent=graph->graph().parent();k.original_program=program_keys(parent.accession().program());k.original_input=input_keys(parent);}
        k.reader=publish(Role::Interface,available_joint_detail::recipe(input,maximum,{}).bytes());return {std::move(input),k};
    }
    PreparedAvailableRelationalEvidence root(const OwnedRelationalFieldProgram& p,bool enlarged) {
        const auto keys=program_keys(p);const PreparedAvailableRelationalProgram prepared(p,keys,context());const auto d=parameters(enlarged);
        const auto origin=publish(Role::Evidence,relational_evidence_root_payload(encode_relational_field_program(p),d,{}));return {prepared,d,origin,context()};
    }
    RelationalAvailableTrainingObservation observation(const PreparedAvailableRelationalEvidence& e,const RelationalAvailableJointInput& reader,std::uint64_t key) {
        const auto raw=available_joint_detail::raw(reader.input);const auto a=raw.accession();const auto program=e.program();
        const auto input=program.execute_inputs({{{a.asset(),a.declaration(),raw.use(),raw.source_field()},reader.origins.input}},context());
        const auto target=tensor({3,5,0,0});const auto bytes=encode_relational_input_use_asset(a.asset());
        RelationalOccurrenceTargetDeclaration d;d.key=100000+key;d.revision=1;d.node=2;d.frame=100100+key;
        const auto p=a.program();const auto source_publication=p.publication();const auto& source=source_publication.endpoint().source;std::vector<std::uint64_t> mask;
        for(std::size_t i=0;i<2;++i)for(std::size_t r=0;r<2;++r){const auto coordinate=200000+i*2+r;d.coordinates.push_back({coordinate,{{1},source.occurrence_identity({1},i)},i?200u:100u,(i?201u:101u)+r});mask.push_back(coordinate);}
        d.accession=M::identity(4);d.reconstruction=M::identity(4);const OwnedRelationalOccurrenceTargetPort port(p,d);
        const auto program_image=encode_relational_field_program(p);
        const RelationalTargetOrigins origins{publish(Role::TargetAsset,bytes),publish(Role::TargetValue,relational_occurrence_target_value_payload(program_image,d,bytes,raw.use(),target,mask,available_evidence_detail::target_inputs(input.result()))),publish(Role::Interface,relational_occurrence_target_port_payload(program_image,d))};
        return {key,input,a.asset(),raw.use(),target,{},origins,port,mask};
    }
};
Context denied(const Publisher& publisher,std::uint64_t key,bool missing=false,bool foreign=false) {
    auto grants=publisher.grants;const auto at=std::find_if(grants.begin(),grants.end(),[&](const auto& row){return row.origin.key()==key;});
    if(at==grants.end())throw std::runtime_error("test origin missing");
    if(missing)grants.erase(at);else if(foreign)++at->availability.source_namespace;else at->availability.max_sequence=at->origin.publication().cutoff()-1;return Context(std::move(grants));
}
RelationalFieldFamilyInterface interface(const storage::NativeSeriesEndpoint& common) {
    algebra::RetainedSourceMap map{{{0,1},{0}},{{},{0,1}}};
    const std::vector<std::vector<TransportContextView>> views{{{88001,M::identity(2),M::identity(2)},{88002,M::identity(2),M::identity(2)}},{{88003,M::identity(3),M::identity(3)}}};
    return {15,1,0,1,common,map,map,views,views};
}
void run(const fs::path& directory,bool cold) {
    const auto left=source(1000),right=source(1001),common=source(1002);Publisher publisher(left);
    const auto before=program(left,false),after=program(left,true),r_before=program(right,false),r_after=program(right,true);
    const auto a=asset(left,false),b=asset(right,false),held_asset=asset(right,true);
    const auto l_literal=literal(a,before),r_literal=literal(b,r_before),h_literal=literal(held_asset,r_before);
    const OwnedRelationalKnownTensorGraph l_graph(l_literal,graph_declaration()),r_graph(r_literal,graph_declaration()),h_graph(h_literal,graph_declaration());
    const auto l_input=publisher.reader(l_graph.admit(accession(a,after))),r_input=publisher.reader(r_graph.admit(accession(b,r_after))),held=publisher.reader(h_graph.admit(accession(held_asset,r_after)));
    auto l_evidence=publisher.root(after,true);const auto r_evidence=publisher.root(r_after,true);const auto root=l_evidence;
    const auto observation=publisher.observation(l_evidence,l_input,16);l_evidence=l_evidence.append({observation},publisher.context());
    const std::vector<PreparedAvailableRelationalEvidence> members{l_evidence,r_evidence};const PreparedRelationalFieldFamily family({12,1,{interface(common)}},{l_evidence.evidence(),r_evidence.evidence()});
    check(family.gluing().consistent()&&family.gluing().coordinate_kernel().cols()==4,"whole common four-direction family");
    const std::vector<std::vector<RelationalJointObservationReaders>> readers{{{16,{l_input}}},{}};
    RelationalJointFamilyOrigins origins{{publisher.publish(Role::Interface,prepare_relational_joint_family_interface_payload(family,0,maximum).bytes())},0};
    origins.family=publisher.publish(Role::Interface,prepare_relational_joint_family_payload(family,members,readers,origins,publisher.context(),maximum).bytes());
    const PreparedAvailableRelationalLiteralFieldFamily model(family,members,readers,origins,publisher.context(),maximum);
    check(model.family().same_owner(family)&&model.members()[0].evidence().same_owner(l_evidence.evidence()),"actual evidence and complete raw family owners reused");
    auto mutable_readers=readers;bool changed_readers{};
    const auto reader_snapshot=prepare_relational_joint_family_payload(family,members,mutable_readers,origins,publisher.context(),maximum,[&]{if(!changed_readers){mutable_readers.clear();changed_readers=true;}});
    check(changed_readers&&reader_snapshot.bytes()==model.content().bytes(),"payload freezes whole reader declaration before callbacks");
    const auto native_evidence=l_evidence.evidence();check(native_evidence.observations()[0].port.has_value()&&native_evidence.observations()[0].observed_coordinates.size()==4&&native_evidence.observations()[0].target.values().dense()==V({3,5,0,0}),"complete original occurrence target mask including zero values retained");
    const auto result=model.execute(1,{held},publisher.context(),maximum),known=model.execute(1,{r_input},publisher.context(),maximum);
    const auto raw_result=result.raw_result(),raw_known=known.raw_result();
    check(raw_result.field(1).particular.values().dense()!=raw_known.field(1).particular.values().dense(),"source-only held bytes condition whole input field");
    check(raw_result.occurrence_field(2).directions.cols()==4&&raw_result.occurrence_field(2).directions.nonzeros()>0,"complete correlated unresolved flags retained");
    V free(4);const auto axis=raw_result.occurrence_field(2).directions.entries().begin()->first.second;free[axis]=2;
    const auto setting=publisher.publish(Role::Prospective,prepare_relational_joint_setting_payload(model,free,publisher.context(),maximum).bytes());
    auto mutable_free=free;bool changed_free{};const auto setting_snapshot=prepare_relational_joint_setting_payload(model,mutable_free,publisher.context(),maximum,[&]{if(!changed_free){mutable_free.clear();changed_free=true;}});
    check(changed_free&&setting_snapshot.bytes()==prepare_relational_joint_setting_payload(model,free,publisher.context(),maximum).bytes(),"setting payload freezes every common free coordinate before callbacks");
    const auto completion=publisher.publish(Role::Prospective,prepare_relational_joint_completion_payload(result,free,setting,publisher.context(),maximum).bytes());
    const auto evaluated=result.evaluate(free,setting,completion,publisher.context(),maximum);const auto native=evaluated.raw_evaluation().result().result();
    const auto expected=raw_result.evaluate(free).result().result();
    check(native.field(1).values().dense()==expected.field(1).values().dense()&&native.field(2).values().dense()==expected.field(2).values().dense()&&native.occurrence_field(2).values().dense()==expected.occurrence_field(2).values().dense(),"availability preserves complete exact native field action");
    const auto zero=raw_result.evaluate(V(4)).result().result();check(native.occurrence_field(2).values().dense()!=zero.occurrence_field(2).values().dense(),"joint prospective setting changes open whole flag field");
    const auto known_value=raw_known.evaluate(free).result().result();check(known_value.field(2).values().dense()==V({3,5,0,0}),"entire original target transfers through declared interface");
    rejects([&]{known.evaluate(free,setting,completion,publisher.context(),maximum);},"reader-specific completion cannot be reused on different source use");
    const auto model_image=cold?read(directory/"model.packet"):encode_available_relational_literal_field_family(model,maximum);
    const auto result_image=cold?read(directory/"result.packet"):encode_available_relational_literal_joint_result(result,maximum);
    const auto evaluation_image=cold?read(directory/"evaluation.packet"):encode_available_relational_literal_joint_evaluation(evaluated,maximum);
    if(!cold){write(directory/"model.packet",model_image);write(directory/"result.packet",result_image);write(directory/"evaluation.packet",evaluation_image);}
    const auto restored=decode_available_relational_literal_field_family(model_image,publisher.context(),{});const auto restored_result=decode_available_relational_literal_joint_result(result_image,publisher.context(),{});const auto restored_evaluation=decode_available_relational_literal_joint_evaluation(evaluation_image,publisher.context(),{});
    check(encode_available_relational_literal_field_family(restored,maximum)==model_image,"fresh cold complete available model canonical");
    check(encode_available_relational_literal_joint_result(restored_result,maximum)==result_image,"fresh cold reader-specific complete result canonical");
    check(encode_available_relational_literal_joint_evaluation(restored_evaluation,maximum)==evaluation_image,"fresh cold complete prospective evaluation canonical");
    check(evaluation_image.size()<publisher.grants.back().origin.receipt().size()+publisher.grants[publisher.grants.size()-2].origin.receipt().size(),"exact payload reconstruction avoids expanded receipt copies");
    auto closure=result.obligations();closure.push_back(evaluated.setting());closure.push_back(evaluated.completion());
    for(const auto& origin:closure) {
        const auto key=origin.key();const auto unavailable=denied(publisher,key);
        rejects([&]{evaluated.admit(unavailable);},"every origin rechecked on warm/cache-hit evaluation");
        rejects([&]{decode_available_relational_literal_joint_evaluation(evaluation_image,unavailable,{});},"every origin admitted before cold replay");
        rejects([&]{evaluated.admit(denied(publisher,key,true));},"every consumed origin required under removal");
        rejects([&]{evaluated.admit(denied(publisher,key,false,true));},"namespace-local availability for every consumed origin");
    }
    auto replacement=publisher.grants;const auto chosen=model.obligations().front();const auto at=std::find_if(replacement.begin(),replacement.end(),[&](const auto& row){return row.origin.key()==chosen.key();});
    at->origin=Origin(chosen.key(),chosen.publication(),chosen.role(),Bytes(chosen.payload_bytes().begin(),chosen.payload_bytes().end()));
    rejects([&]{model.admit(Context(replacement));},"equal receipt bytes cannot replace a live actual origin owner");
    check(encode_available_relational_literal_field_family(decode_available_relational_literal_field_family(model_image,Context(replacement),{}),maximum)==model_image,"cold binds full declared publication to supplied actual authority");
    auto omitted=readers;omitted[0].clear();rejects([&]{PreparedAvailableRelationalLiteralFieldFamily wrong(family,members,omitted,origins,publisher.context(),maximum);},"all original training recipes required");
    auto wrong=readers;++wrong[0][0].observation;rejects([&]{PreparedAvailableRelationalLiteralFieldFamily value(family,members,wrong,origins,publisher.context(),maximum);},"original observation order and identities retained");
    const auto duplicate=decode_relational_field_family(encode_relational_field_family(family,maximum),{});rejects([&]{PreparedAvailableRelationalLiteralFieldFamily value(duplicate,members,readers,origins,publisher.context(),maximum);},"equal raw evidence values do not replace actual available members");
    auto altered=r_input;const auto changed_recipe=literal(b,r_before,true);const OwnedRelationalKnownTensorGraph changed_graph(changed_recipe,graph_declaration());altered.input=changed_graph.admit(accession(b,r_after));
    const auto altered_raw=available_joint_detail::raw(altered.input),original_raw=available_joint_detail::raw(r_input.input);
    check(altered_raw.source_field().values().dense()==original_raw.source_field().values().dense(),"equal numerical encoding control");
    rejects([&]{model.execute(1,{altered},publisher.context(),maximum);},"equal field cannot substitute a different original recipe");
    altered=held;altered.origins.original_program={};rejects([&]{model.execute(1,{altered},publisher.context(),maximum);},"graph requires original parent program/map/metric origins");
    altered=held;altered.origins.original_input={};rejects([&]{model.execute(1,{altered},publisher.context(),maximum);},"graph requires original parent input origins");
    rejects([&]{result.evaluate({1},setting,completion,publisher.context(),maximum);},"whole common free basis required");
    rejects([&]{decode_available_relational_literal_joint_evaluation(std::span(evaluation_image).first(evaluation_image.size()-1),publisher.context(),{});},"truncated available evaluation refused");
    auto bad=evaluation_image;bad.back()^=std::byte{1};rejects([&]{decode_available_relational_literal_joint_evaluation(bad,publisher.context(),{});},"corrupt complete evaluated witness refused");
    auto small=storage::TemporalReadLimits{};small.max_file_bytes=evaluation_image.size()-1;rejects([&]{decode_available_relational_literal_joint_evaluation(evaluation_image,publisher.context(),small);},"explicit archive byte admission");
    small={};small.max_records=2;rejects([&]{decode_available_relational_literal_joint_evaluation(evaluation_image,publisher.context(),small);},"cumulative record admission");
    auto cap=numeric::ExactActionLimits{};--cap.entries;rejects([&]{decode_available_relational_literal_joint_evaluation(evaluation_image,publisher.context(),{},cap);},"caller action cap not silently increased");
    rejects([&]{decode_available_relational_literal_joint_evaluation(evaluation_image,publisher.context(),{},{},[]{throw std::runtime_error("stop");});},"cold preflight and replay cancellation");
    auto mutable_image=evaluation_image;bool mutated{};const auto frozen=decode_available_relational_literal_joint_evaluation(mutable_image,publisher.context(),{},{},[&]{if(!mutated){std::fill(mutable_image.begin(),mutable_image.end(),std::byte{});mutated=true;}});
    check(mutated&&encode_available_relational_literal_joint_evaluation(frozen,maximum)==evaluation_image,"cold byte image owned before reentrant callbacks");
    auto task=[&]{evaluated.admit(publisher.context());return encode_available_relational_literal_joint_evaluation(evaluated,maximum);};auto first=std::async(std::launch::async,task),second=std::async(std::launch::async,task);check(first.get()==evaluation_image&&second.get()==evaluation_image,"immutable available owner supports concurrent validation and streaming");
    const auto right_observation=publisher.observation(r_evidence,r_input,17);const auto r_child=r_evidence.append({right_observation},publisher.context());
    const PreparedRelationalFieldFamily child_family({12,2,{interface(common)}},{l_evidence.evidence(),r_child.evidence()});const std::vector<PreparedAvailableRelationalEvidence> child_members{l_evidence,r_child};
    const std::vector<std::vector<RelationalJointObservationReaders>> child_readers{{{16,{l_input}}},{{17,{r_input}}}};auto child_origins=origins;
    child_origins.family=publisher.publish(Role::Interface,prepare_relational_joint_family_payload(child_family,child_members,child_readers,child_origins,publisher.context(),maximum).bytes());
    const PreparedAvailableRelationalLiteralFieldFamily child(child_family,child_members,child_readers,child_origins,publisher.context(),maximum);const auto child_result=child.execute(1,{held},publisher.context(),maximum);
    const auto original_right=r_evidence.evidence();
    check(child_members[1].parent().has_value()&&child_members[1].program().program().same_owner(r_after)&&child_family.members()[1].declaration().basis==original_right.declaration().basis,"evidence update retains actual source parameters coordinates and parent");
    rejects([&]{child_result.evaluate(free,setting,completion,publisher.context(),maximum);},"old setting cannot be relabelled across explicit model revision");
    const auto old_target=observation.origins.value;rejects([&]{child.admit(denied(publisher,old_target));},"updated closure retains inherited target availability");
    rejects([&]{child.admit(denied(publisher,r_input.origins.reader));},"updated closure includes new training reader availability");
    const auto plain_program=program(left,false);const auto plain_literal=literal(a,plain_program);const auto plain_input=publisher.reader(plain_literal);const auto plain_member=publisher.root(plain_program,false);const PreparedRelationalFieldFamily plain_family({30,1,{}},{plain_member.evidence()});RelationalJointFamilyOrigins plain_keys{{},0};
    plain_keys.family=publisher.publish(Role::Interface,prepare_relational_joint_family_payload(plain_family,{plain_member},{{}},plain_keys,publisher.context(),maximum).bytes());const PreparedAvailableRelationalLiteralFieldFamily plain(plain_family,{plain_member},{{}},plain_keys,publisher.context(),maximum);
    const auto plain_result=plain.execute(0,{plain_input},publisher.context(),maximum);const auto plain_image=encode_available_relational_literal_joint_result(plain_result,maximum);
    check(encode_available_relational_literal_joint_result(decode_available_relational_literal_joint_result(plain_image,publisher.context(),{}),maximum)==plain_image,"direct literal branch complete cold adoption");
    auto extraneous=plain_input;extraneous.origins.original_program={1,2,3};rejects([&]{plain.execute(0,{extraneous},publisher.context(),maximum);},"direct literal refuses undeclared graph-parent keys");
    std::cout<<"BYTES "<<model_image.size()<<' '<<result_image.size()<<' '<<evaluation_image.size()<<" ORIGINS "<<closure.size()<<'\n';
}
}
int main(int argc,char** argv) {try{
    const bool cold=argc==3&&std::string_view(argv[1])=="read";const auto directory=argc==3?fs::path(argv[2]):fs::temp_directory_path()/("rfc-available-joint-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);run(directory,cold);if(argc==1)fs::remove_all(directory);std::cout<<checks<<" available literal field family checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
