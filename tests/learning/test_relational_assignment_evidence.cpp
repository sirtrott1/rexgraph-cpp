#include "rfc/learning/relational_assignment_evidence_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using T = numeric::SparseRationalTensor;
using Context = RelationalAvailabilityContext;
using Grant = RelationalOriginGrant;
using Role = RelationalDependencyRole;
using Program = OwnedRelationalFieldProgram;
using Bytes = std::vector<std::byte>;
namespace {
std::size_t checks{};
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
void reseal(Bytes& bytes) {
    const auto hash=storage::rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));
    for(std::size_t i=0;i<8;++i)bytes[24+i]=static_cast<std::byte>((hash>>(8*i))&255);
}
T tensor(const V& v) { numeric::ExactActionGuard g; return field_interface_detail::tensor(v, g); }
storage::NativeSeriesEndpoint publication() {
    core::RelationSignature s; const auto type = s.add_type("field-work", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(core::RelationKey{100}, type,
        {{10,true,false,1},{20,true,false,1},{10,false,false,1},{20,true,false,0}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(),1), 88400).tail();
}
Program program(const M& map, bool reverse=false) {
    RelationalFieldProgramDeclaration d; d.key=51; d.revision=1; d.differential=true;
    d.stalks={{{{0},0},1001,{101,102},M::identity(2)},{{{0},1},1002,{201,202},M::identity(2)},
              {{{1},0},1003,{301,302},M::identity(2)}};
    const auto phi=storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences={{{1},0,phi,map},{{1},1,phi,M::identity(2)},{{1},2,phi,M(2,2)},{{1},3,phi,M::identity(2)}};
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}},
        {3,FieldProgramNodeKind::DeclaredMap,{0},2,M::from_rows({{1,1,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}})}};
    if(reverse)d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},
        {2,FieldProgramNodeKind::DeclaredMap,{1},1,M::from_rows({{0,1},{1,0}})},
        {3,FieldProgramNodeKind::Boundary,{0},2,{}}};
    return {publication(),d};
}
OwnedRelationalInputUseAsset asset(const Program& p, std::uint64_t key=600) {
    const auto endpoint=p.publication(); const auto& c=endpoint.endpoint().source;
    RelationalInputUseDeclaration d; d.key=key; d.channel=700; d.encoding="UTF-8";
    for (const auto x : std::string("requested whole field; original input use")) d.bytes.push_back(static_cast<std::byte>(x));
    d.uses={{111,{storage::stable_cell_key(c,{{1},0})},{},{{0,d.bytes.size()}}}};
    return {storage::NativeTemporalSeries(endpoint.endpoint(),endpoint.source_namespace()),d};
}
struct Registry {
    std::vector<Grant> grants;
    std::uint64_t next{1};
    std::map<Bytes,std::uint64_t> assets;
    std::uint64_t publish(Role role, const Bytes& bytes) {
        const auto endpoint=publication(); const auto id=next++;
        OwnedRelationalValueOrigin origin(id,storage::NativeSeriesPrefix::capture(storage::NativeTemporalSeries(endpoint.endpoint(),endpoint.source_namespace()),endpoint.sequence()),role,bytes);
        grants.push_back({origin,{origin.publication().source_namespace(),origin.publication().cutoff()}}); return id;
    }
    Context context() const { return Context(grants); }
    Context without(std::uint64_t key) const {
        auto out=grants; std::erase_if(out,[&](const auto& g){return g.origin.key()==key;}); return Context(std::move(out));
    }
    Context future(std::uint64_t key) const {
        auto out=grants;
        for(auto& g:out) if(g.origin.key()==key) --g.availability.max_sequence;
        return Context(std::move(out));
    }
    PreparedAvailableRelationalProgram available(const Program& p) {
        const auto bytes=encode_relational_field_program(p);
        const RelationalProgramOrigins keys{publish(Role::Program,bytes),publish(Role::Maps,bytes),publish(Role::Metrics,bytes)};
        return {p,keys,context()};
    }
    AvailableRelationalFieldResult input(const PreparedAvailableRelationalProgram& p, const OwnedRelationalInputUseAsset& a, const V& x) {
        const auto raw=p.program(); RelationalInputAccessionDeclaration d;
        d.key=901;d.node=1;d.source_grade={1};d.isometric=true;d.source_stalks={raw.declaration().stalks.back()};
        d.instances.cells={{0,1},{0}};d.instances.occurrences={{},{0,1,2,3}};d.accession=M::identity(2);d.reconstruction=M::identity(2);
        const RelationalInputSubmission v{a,d,111,tensor(x)};
        const auto ab=encode_relational_input_use_asset(a);
        if(!assets.contains(ab))assets.emplace(ab,publish(Role::InputAsset,ab));
        const auto value=relational_input_value_payload(ab,d,111,v.source_field);
        const auto accession=relational_input_accession_payload(encode_relational_field_program(raw),ab,d,111,v.source_field);
        const RelationalInputOrigins keys{assets.at(ab),publish(Role::InputValue,value),
            publish(Role::InputAccession,accession),publish(Role::Reconstruction,accession)};
        return p.execute({p.admit_input(v,keys,context())},context());
    }
    PreparedAvailableRelationalEvidence evidence(const PreparedAvailableRelationalProgram& p) {
        RelationalFieldParameterDeclaration d;d.key=100;d.revision=1;
        const auto native_program=p.program();
        for(const auto& node:native_program.declaration().nodes)if(node.kind==FieldProgramNodeKind::Boundary)d.node=node.key;
        d.variables={{0,0,0},{0,0,1},{0,1,0},{0,1,1},{2,0,0}};d.parameters={801,802,803,804};d.basis=M(5,4);
        d.basis.set(0,0,1);d.basis.set(1,1,1);d.basis.set(2,2,1);d.basis.set(3,0,1);d.basis.set(4,3,1);
        const auto id=publish(Role::Evidence,relational_evidence_root_payload(encode_relational_field_program(p.program()),d,{}));
        return {p,d,id,context()};
    }
    RelationalAvailableTrainingObservation measured(const AvailableRelationalFieldResult& input,
        const AvailableRelationalFieldResult& returned, std::uint64_t key) {
        const auto target_source=input.result().result().program();
        const auto execution=returned.result().result(); const auto field=execution.field(2);
        auto target=asset(target_source,2000+key);
        const auto bytes=encode_relational_input_use_asset(target);
        const auto a=publish(Role::TargetAsset,bytes);
        const auto v=publish(Role::TargetValue,relational_target_value_payload(encode_relational_field_program(target_source),2,bytes,111,field));
        return {key,input,target,111,field,{{{0},0},{{0},1}},{a,v}};
    }
    OwnedRelationalTaskOutcome outcome(RelationalTaskResponse r, const V& target, std::uint64_t worker=12) {
        const auto input=task_outcome_detail::input(r).result();
        const auto a=input.inputs().front().accession().asset();
        RelationalTaskPostconditionDeclaration task;
        task.key=400;task.revision=1;task.use=111;task.basis=900;task.directions={901,902,903,904};
        task.metric=M::identity(4);task.target=tensor(target);
        RelationalWorkerBindingDeclaration b;
        b.key=500;b.revision=1;b.worker=worker;b.machine=21;b.operation=31;
        b.interface={501,502,{{3,{{0},0}},{3,{{0},1}}}};
        b.view={503,900,{901,902,903,904},M::identity(4),M::identity(4),{}};
        task.origin=publish(Role::Prospective,relational_task_goal_payload(task,a,r));
        b.origin=publish(Role::Maps,relational_worker_binding_payload(b,r));
        return {task,a,b,std::move(r),context()};
    }
};
RelationalWorkerBindingDeclaration binding(Registry& r,const Program& p,std::uint64_t worker) {
    RelationalWorkerBindingDeclaration b;b.key=500+worker;b.revision=1;b.worker=worker;b.machine=21;b.operation=31+worker;
    b.interface={501,502,{{3,{{0},0}},{3,{{0},1}}}};
    b.view={503,900,{901,902,903,904},M::identity(4),M::identity(4),{}};
    b.origin=r.publish(Role::Maps,task_outcome_detail::binding_payload(b,encode_relational_field_program(p)));return b;
}
PreparedRelationalAssignmentEvidence model(Registry& r,bool outside=false) {
    const auto a=r.available(program(M(2,2))),b=r.available(program(M(2,2),true));
    const auto pa=a.program(),pb=b.program();
    M permutation=M::from_rows({{1,0,0,0},{0,0,1,0},{0,1,0,0},{0,0,0,1}});
    std::vector<RelationalAssignmentCandidate> candidates{{101,{1001},binding(r,pa,11),r.evidence(a),V(4),M::identity(4)},
        {102,{1002},binding(r,pb,12),r.evidence(b),V(4),permutation}};
    auto catalog=asset(pa,7000);RelationalFiniteSourceDeclaration finite;finite.key=8000;finite.revision=1;
    finite.symbols={{1001,encode_relational_field_program(pa)},{1002,encode_relational_field_program(pb)}};
    finite.variables={{{{1},0},{1001,1002}}};finite.bindings={{1,111,{{1},0}}};
    if(outside){finite.symbols.push_back({999,{std::byte{'?'}}});finite.variables.front().domain.push_back(999);}
    OwnedRelationalFiniteSource source(catalog,finite);
    RelationalAssignmentDeclaration d;d.key=9000;d.revision=1;d.parameter_carrier=9100;d.parameters={1,2,3,4};
    d.origin=r.publish(Role::Evidence,relational_assignment_root_payload(d,source,candidates));
    return {d,source,candidates,r.context()};
}
RelationalAssignmentObservation measured(Registry& r,const PreparedRelationalAssignmentEvidence& m,
    const PreparedAvailableRelationalProgram& actual,const OwnedRelationalInputUseAsset& a,const V& x,std::uint64_t key) {
    const auto native=actual.program();const auto observed=r.input(actual,a,x);
    std::vector<RelationalAssignmentInput> inputs;
    for(const auto& c:m.candidates())inputs.push_back({c.key,r.input(c.evidence.program(),a,x)});
    RelationalAssignmentObservation o{key,0,binding(r,native,44),observed,inputs};
    o.origin=r.publish(Role::TargetValue,relational_assignment_measurement_payload(o));return o;
}
void run() {
    Registry r;const auto root=model(r);const auto original_bytes=encode_relational_assignment_evidence(root,r.context());
    const M truth=M::from_rows({{2,-1},{3,2}});const auto actual=r.available(program(truth));const auto pa=actual.program();
    const auto a=asset(pa,7500);const auto first=measured(r,root,actual,a,{1,1},1);
    const auto both=root.append({first},r.context());
    check(!both.identified_candidate(r.context())&&!both.branch(0,r.context()).rejected&&!both.branch(1,r.context()).rejected,"one measured use leaves both worker/order alternatives");
    check(both.branch(0,r.context()).coordinates.map.cols()==2&&both.branch(1,r.context()).coordinates.map.cols()==2,"two correlated free directions remain in each solve family");
    const auto second=measured(r,both,actual,a,{1,0},2);const auto learned=both.append({second},r.context());
    check(learned.identified_candidate(r.context())==std::optional<std::size_t>(0),"exact whole outcomes acquire worker and noncommuting program order");
    const auto& rejected=learned.branch(1,r.context());
    check(rejected.rejected&&rejected.solves.size()==2&&!rejected.solves.back()->certificate.contradiction.empty(),"refuted assignment retains actual whole solve and contradiction");
    check(learned.branch(0,r.context()).coordinates.map.cols()==1,"unobserved inactive map direction is retained");
    const auto held_input=r.input(learned.candidates()[0].evidence.program(),a,{2,-1});
    const auto held=learned.execute(0,held_input,r.context());const auto before=root.execute(0,held_input,r.context());
    const auto field=held.field(3,r.context());
    check(field.particular.values().dense()==V({9,4,2,-1})&&field.directions.nonzeros()==0,"acquired process produces complete independent held response");
    check(!field_interface_detail::equal(field.particular,before.field(3,r.context()).particular),"learning changes the complete held intermediary");
    const auto actual_held=r.input(actual,a,{2,-1});const auto actual_native=actual_held.result().result();
    check(field_interface_detail::equal(field.particular,actual_native.field(3)),"held full field matches actual returned process before decoding");
    const auto learned_map=held.coefficient_independent_map({1},0,r.context());
    check(learned_map.particular==truth&&learned_map.directions.size()==1&&learned_map.directions.front().nonzeros()==0,"whole learned map retained on original flag");
    const auto goal=r.outcome(held.original(),{9,4,2,-1});
    check(held.postcondition(goal,r.context()).status==RelationalTaskPostconditionStatus::Satisfied&&!goal.executed_response(),"exact learned whole-field theorem remains distinct from observed task completion");
    const auto wrong_goal=r.outcome(held.original(),{9,5,2,-1});
    check(held.postcondition(wrong_goal,r.context()).status==RelationalTaskPostconditionStatus::Refuted,"learned complete postcondition can still refute a requested field");
    check(learned.branch(0,r.context()).solves.front()==both.branch(0,r.context()).solves.front(),"immutable versions share original solve owners without copying their fields");
    check(learned.dependency_origins().size()>root.dependency_origins().size(),"actual measured source/solve dependencies remain inspectable");
    const auto inactive=held.coefficient_independent_map({1},2,r.context());
    check(inactive.directions.front().nonzeros()==1,"inactive original map is not completed from absent evidence");
    check(held.occurrence_field(2,r.context()).particular.domain().logical_size()==8,"all repeated/inactive/supported-zero original occurrences retained");
    check(learned.revision()==3&&learned.parent()->same_owner(both)&&root.observations().empty(),"exact solve versions keep original parent owners");
    rejects([&]{(void)learned.execute(1,second.inputs[1].input,r.context());},"refuted worker is not reused as a compatible process");
    const auto incomplete=model(r,true);const auto open1=measured(r,incomplete,actual,a,{1,1},11);
    const auto open2=measured(r,incomplete,actual,a,{1,0},12);const auto open=incomplete.append({open1,open2},r.context());
    check(!open.identified_candidate(r.context())&&open.remainder()==std::vector<std::vector<std::uint64_t>>({{999}}),"outside-image finite remainder blocks false unique acquisition");
    const auto changed=r.available(program(M::identity(2)));auto incompatible=measured(r,learned,changed,a,{1,0},3);
    const auto inconsistent=learned.append({incompatible},r.context());
    check(inconsistent.status(r.context())==RelationalAssignmentStatus::Inconsistent&&!inconsistent.identified_candidate(r.context()),"incompatible later process returns no admissible catalog assignment rather than dropping old evidence");
    check(learned.status(r.context())==RelationalAssignmentStatus::Identified&&open.status(r.context())==RelationalAssignmentStatus::Unresolved,"identified versus outside-catalog unresolved statuses remain distinct");
    const auto authority=r.context();const storage::TemporalReadLimits policy;
    for(const auto& value:{root,both,learned,open,inconsistent}){
        const auto bytes=encode_relational_assignment_evidence(value,authority);
        const auto cold=decode_relational_assignment_evidence(bytes,authority,policy);
        check(encode_relational_assignment_evidence(cold,authority)==bytes,"complete assignment solve history cold replay canonical");
        Bytes stream;stream_relational_assignment_evidence(value,authority,[&](std::span<const std::byte> b){stream.insert(stream.end(),b.begin(),b.end());});
        check(stream==bytes,"stream retains full candidate programs, fields and solve history");
        auto bad=bytes;bad.back()^=std::byte{1};reseal(bad);rejects([&]{(void)decode_relational_assignment_evidence(bad,authority,policy);},"forged solve witness refused");
        rejects([&]{(void)decode_relational_assignment_evidence(bytes,r.without(value.declaration().origin),policy);},"original family authority needed on cold restore");
    }
    const auto reowned=decode_relational_assignment_evidence(original_bytes,authority,policy);
    const auto adopted=restore_relational_assignment_input(reowned,0,encode_available_relational_result(first.inputs[0].input),authority,policy);
    const auto adopted_second=restore_relational_assignment_input(reowned,1,encode_available_relational_result(first.inputs[1].input),authority,policy);
    auto adopted_observation=first;adopted_observation.inputs={{101,adopted},{102,adopted_second}};
    const auto adopted_child=reowned.append({adopted_observation},authority);
    check(adopted_child.status(authority)==RelationalAssignmentStatus::Unresolved,"explicit original archive adoption binds new observation to restored candidate owners");
    rejects([&]{(void)restore_relational_assignment_input(reowned,1,encode_available_relational_result(first.inputs[0].input),authority,policy);},"wrong actual candidate program cannot adopt an equal-shaped input archive");
    const auto response_bytes=encode_relational_assignment_response(held,authority);
    const auto cold_response=decode_relational_assignment_response(response_bytes,authority,policy);
    check(encode_relational_assignment_response(cold_response,authority)==response_bytes,"complete held response, original maps and solve parent history survive cold replay");
    const auto reattached_goal=restore_relational_assignment_goal(cold_response,encode_relational_task_outcome(goal,authority),authority,policy);
    check(cold_response.postcondition(reattached_goal,authority).status==RelationalTaskPostconditionStatus::Satisfied&&!reattached_goal.executed_response(),"cold goal is explicitly reattached to the complete original family without a new authority or execution claim");
    check(cold_response.field(3,authority).particular.values().dense()==V({9,4,2,-1}),"cold learned process retains the complete held field");
    Bytes response_stream;stream_relational_assignment_response(held,authority,[&](std::span<const std::byte> b){response_stream.insert(response_stream.end(),b.begin(),b.end());});
    check(response_stream==response_bytes,"held response streaming is canonical");
    auto corrupted_response=response_bytes;corrupted_response.back()^=std::byte{1};reseal(corrupted_response);
    rejects([&]{(void)decode_relational_assignment_response(corrupted_response,authority,policy);},"forged held field coordinate refused");
    rejects([&]{(void)decode_relational_assignment_response(response_bytes,r.without(held_input.input_origins().front().value),policy);},"held input is admitted before replaying the learned process");
    const auto bytes=encode_relational_assignment_evidence(learned,authority);
    rejects([&]{(void)learned.identified_candidate(r.without(first.origin));},"warm learned assignment retains original measured outcome availability");
    rejects([&]{(void)decode_relational_assignment_evidence(bytes,r.future(second.origin),policy);},"future actual observed outcome refused before cold solve");
    auto malformed_candidates=root.candidates();malformed_candidates[0].injection.set(0,0,0);
    auto malformed_declaration=root.declaration();malformed_declaration.key=12345;
    malformed_declaration.origin=r.publish(Role::Evidence,relational_assignment_root_payload(malformed_declaration,root.finite_source(),malformed_candidates));
    rejects([&]{(void)PreparedRelationalAssignmentEvidence(malformed_declaration,root.finite_source(),malformed_candidates,r.context());},"equal dimension parameter maps cannot drop an original unknown direction");
    check(learned.retained_entries()>both.retained_entries(),"retained solve/index budget includes immutable ancestors");
    auto bogus=second;bogus.key=88;bogus.origin=r.publish(Role::Prospective,relational_assignment_measurement_payload(bogus));
    rejects([&]{(void)both.append({bogus},r.context());},"requested goal cannot become measured worker evidence");
    auto wrong_input=second;wrong_input.key=89;wrong_input.inputs[0].input=r.input(root.candidates()[0].evidence.program(),a,{1,2});
    wrong_input.origin=r.publish(Role::TargetValue,relational_assignment_measurement_payload(wrong_input));
    rejects([&]{(void)both.append({wrong_input},r.context());},"equal source shape cannot replace actual measured input field");
    rejects([&]{(void)encode_relational_assignment_evidence(learned,r.context(),64);},"whole assignment archive byte cap enforced");
    auto cap=numeric::ExactActionLimits{};cap.dimension=1;
    rejects([&]{(void)decode_relational_assignment_evidence(bytes,r.context(),policy,cap);},"caller action cap precedes restore/solve");
    std::vector<std::future<Bytes>> readers;for(int i=0;i<3;++i)readers.push_back(std::async(std::launch::async,[learned,authority]{return encode_relational_assignment_evidence(learned,authority);}));
    for(auto& future:readers)check(future.get()==bytes,"joined concurrent solve history readers byte exact");
    auto changing_authority=r.context();bool revoked{};
    const auto pinned_bytes=encode_relational_assignment_evidence(learned,changing_authority,256u<<20,[&]{if(!revoked){revoked=true;changing_authority=r.without(first.origin);}});
    check(revoked&&pinned_bytes==bytes,"exact solve serialization pins authority before callbacks");
    const auto retained=[&]{auto source=decode_relational_assignment_evidence(bytes,authority,policy);auto local_input=r.input(source.candidates()[0].evidence.program(),a,{2,-1});return source.execute(0,local_input,r.context());}();
    check(retained.field(3,r.context()).particular.values().dense()==V({9,4,2,-1}),"returned whole family owns its restored model, solve lineage and source input");
    check(encode_relational_assignment_evidence(root,r.context())==original_bytes,"learning preserves all original model/source bytes");
}
}
int main(){try{run();std::cout<<"exact assignment checks: "<<checks<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
