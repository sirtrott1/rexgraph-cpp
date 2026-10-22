#include "rfc/learning/relational_task_outcome_codec.hpp"
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
T tensor(const V& v) { numeric::ExactActionGuard g; return field_interface_detail::tensor(v, g); }
storage::NativeSeriesEndpoint publication() {
    core::RelationSignature s; const auto type = s.add_type("field-work", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(s);
    b.add_grade_one(core::RelationKey{100}, type,
        {{10,true,false,1},{20,true,false,1},{10,false,false,1},{20,true,false,0}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(),1), 88400).tail();
}
Program program(const M& map) {
    RelationalFieldProgramDeclaration d; d.key=51; d.revision=1; d.differential=true;
    d.stalks={{{{0},0},1001,{101,102},M::identity(2)},{{{0},1},1002,{201,202},M::identity(2)},
              {{{1},0},1003,{301,302},M::identity(2)}};
    const auto phi=storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences={{{1},0,phi,map},{{1},1,phi,M::identity(2)},{{1},2,phi,M(2,2)},{{1},3,phi,M::identity(2)}};
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}},
        {3,FieldProgramNodeKind::DeclaredMap,{0},2,M::from_rows({{1,1,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}})}};
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
        const auto value=relational_input_value_payload(ab,d,111,v.source_field);
        const auto accession=relational_input_accession_payload(encode_relational_field_program(raw),ab,d,111,v.source_field);
        const RelationalInputOrigins keys{publish(Role::InputAsset,ab),publish(Role::InputValue,value),
            publish(Role::InputAccession,accession),publish(Role::Reconstruction,accession)};
        return p.execute({p.admit_input(v,keys,context())},context());
    }
    PreparedAvailableRelationalEvidence evidence(const PreparedAvailableRelationalProgram& p) {
        RelationalFieldParameterDeclaration d;d.key=100;d.revision=1;d.node=2;
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
void run() {
    Registry registry;
    const auto correct=registry.available(program(M::from_rows({{2,0},{0,2}})));
    const auto wrong=registry.available(program(M::from_rows({{2,-1},{3,2}})));
    const auto p=program(M(2,2));const auto prepared=registry.available(p);const auto root=registry.evidence(prepared);
    const auto a=asset(p);const V x{1,2}, target{6,4,1,2};
    const auto good=registry.input(correct,a,x),bad=registry.input(wrong,a,x),held=registry.input(prepared,a,x);
    const auto satisfied=registry.outcome(good,target),refuted=registry.outcome(bad,target);
    check(satisfied.executed_response() && satisfied.status(registry.context())==RelationalTaskPostconditionStatus::Satisfied,"executed field satisfies actual task");
    check(refuted.status(registry.context())==RelationalTaskPostconditionStatus::Refuted,"successful computation can violate task postcondition");
    const auto bad_proof=refuted.certificate(registry.context());
    check(bad_proof.residual.particular.values().dense()==V({1,3,0,0}) && !bad_proof.contradiction.empty(),"complete actual residual and contradiction retained");
    const auto family=root.execute(held,registry.context());const auto open=registry.outcome(family,target);
    check(!open.executed_response() && open.status(registry.context())==RelationalTaskPostconditionStatus::Open,"feasible prospective family is not observed task success");
    const auto open_proof=open.certificate(registry.context());
    check(open_proof.residual.directions.cols()==4 && open_proof.feasible_kernel.cols()==2,"correlated free field directions retained");
    const auto original_evidence=root.evidence();
    check(original_evidence.observations().empty(),"goal feasibility must not become measured evidence");
    const auto measure1_input=registry.input(prepared,a,{1,0});const auto returned1=registry.input(wrong,a,{1,0});
    const auto first=registry.measured(measure1_input,returned1,1);
    const auto child=root.append({first},registry.context());
    const auto prediction=child.execute(held,registry.context());const auto learned_refutation=registry.outcome(prediction,target);
    const auto proof=learned_refutation.certificate(registry.context());
    check(proof.status==RelationalTaskPostconditionStatus::Refuted && proof.residual.directions.nonzeros()!=0,"varying family can still refute the complete task");
    const auto measure2_input=registry.input(prepared,a,{0,1});const auto returned2=registry.input(wrong,a,{0,1});
    const auto observed=registry.measured(measure2_input,returned2,2);const auto learned=child.append({observed},registry.context());
    const auto learned_native=learned.evidence();check(learned_native.free_parameters().size()==1,"inactive original map remains unresolved");
    const auto final_family=learned.execute(held,registry.context());const auto matched=registry.outcome(final_family,{7,7,1,2});
    check(matched.status(registry.context())==RelationalTaskPostconditionStatus::Satisfied && !matched.executed_response(),"identified task observation does not identify unused directions or observed success");
    const auto complete=final_family.result();const auto original=family.result();
    check(!field_interface_detail::equal(complete.field(3).particular,original.field(3).particular),"actual measured correction changes whole downstream field");
    check(complete.occurrence_field(2).directions.cols()==1,"complete original occurrence family retained");
    const auto independent=registry.input(prepared,a,{2,-1});const auto future=learned.execute(independent,registry.context());
    const auto future_native=future.result();check(future_native.field(3).particular.values().dense()==V({9,4,2,-1}),"learned actual map applies to independent source field");
    const auto authority=registry.context();const storage::TemporalReadLimits policy;
    for(const auto& o:{satisfied,refuted,open,learned_refutation,matched}) {
        const auto image=encode_relational_task_outcome(o,authority);
        const auto cold=decode_relational_task_outcome(image,authority,policy);
        check(encode_relational_task_outcome(cold,authority)==image,"full task/worker/response/field cold replay canonical");
        Bytes streamed;stream_relational_task_outcome(o,authority,[&](std::span<const std::byte> part){streamed.insert(streamed.end(),part.begin(),part.end());});
        check(streamed==image,"streaming retains canonical complete task bytes");
        auto corrupt=image;corrupt.back()^=std::byte{1};rejects([&]{(void)decode_relational_task_outcome(corrupt,authority,policy);},"tampered complete residual proof refused");
        rejects([&]{(void)decode_relational_task_outcome(image,registry.without(o.task().origin),policy);},"cold task instructions require current availability");
        rejects([&]{(void)decode_relational_task_outcome(image,registry.future(o.binding().origin),policy);},"future worker binding refused on cold replay");
    }
    auto wrong_goal=matched.task();auto binding=matched.binding();const auto response=matched.response();
    ++binding.machine;
    rejects([&]{(void)OwnedRelationalTaskOutcome(wrong_goal,matched.contract_asset(),binding,response,authority);},"machine identity changes require a new actual provider binding");
    binding=matched.binding();wrong_goal.metric=M::identity(4).scale(2);
    wrong_goal.origin=registry.publish(Role::Prospective,relational_task_goal_payload(wrong_goal,matched.contract_asset(),response));
    rejects([&]{(void)OwnedRelationalTaskOutcome(wrong_goal,matched.contract_asset(),binding,response,registry.context());},"equal dimensions do not establish metric transport");
    auto rectangular_goal=satisfied.task();auto rectangular_binding=satisfied.binding();
    rectangular_goal.directions.push_back(905);rectangular_goal.target=tensor({6,4,1,2,11});
    rectangular_binding.view.directions=rectangular_goal.directions;
    rectangular_binding.view.accession=M(5,4);rectangular_binding.view.reconstruction=M(4,5);
    for(std::size_t i=0;i<4;++i){rectangular_binding.view.accession.set(i,i,1);rectangular_binding.view.reconstruction.set(i,i,1);}
    rectangular_binding.view.accession.set(4,0,1);rectangular_binding.view.accession.set(4,1,1);
    rectangular_goal.metric=rectangular_binding.view.reconstruction.transpose().multiply(rectangular_binding.view.reconstruction);
    const auto rectangular_response=satisfied.response();
    rectangular_goal.origin=registry.publish(Role::Prospective,relational_task_goal_payload(rectangular_goal,satisfied.contract_asset(),rectangular_response));
    rectangular_binding.origin=registry.publish(Role::Maps,relational_worker_binding_payload(rectangular_binding,rectangular_response));
    const OwnedRelationalTaskOutcome outside_image(rectangular_goal,satisfied.contract_asset(),rectangular_binding,rectangular_response,registry.context());
    const auto outside_proof=outside_image.certificate(registry.context());
    check(outside_proof.status==RelationalTaskPostconditionStatus::Refuted && outside_proof.residual.particular.values().dense()==V({0,0,0,0,-1}),"off-image task coordinate remains refuted rather than clipped by reconstruction");
    const auto outside_bytes=encode_relational_task_outcome(outside_image,registry.context());
    check(encode_relational_task_outcome(decode_relational_task_outcome(outside_bytes,registry.context(),policy),registry.context())==outside_bytes,"rectangular admitted image and full residual survive cold replay");
    rejects([&]{(void)matched.status(registry.without(matched.task().origin));},"warm task observation rechecks current goal origin");
    rejects([&]{(void)matched.certificate(registry.future(matched.binding().origin));},"warm certificate rechecks current worker availability");
    rejects([&]{(void)encode_relational_task_outcome(matched,registry.without(matched.binding().origin));},"warm archive cannot retain stale provider availability");
    auto changed_input=registry.input(prepared,a,{1,3});
    rejects([&]{(void)OwnedRelationalTaskOutcome(open.task(),open.contract_asset(),open.binding(),root.execute(changed_input,registry.context()),registry.context());},"original goal origin binds actual input accession rather than equal dimensions");
    auto goal_as_measurement=observed;goal_as_measurement.origins.value=matched.task().origin;
    rejects([&]{(void)child.append({goal_as_measurement},registry.context());},"requested goal is not an observed outcome");
    rejects([&]{(void)encode_relational_task_outcome(matched,registry.context(),32);},"whole archive byte bound enforced");
    auto cap=numeric::ExactActionLimits{};cap.dimension=1;
    const auto image=encode_relational_task_outcome(matched,registry.context());
    rejects([&]{(void)decode_relational_task_outcome(image,registry.context(),policy,cap);},"cold caller action cap precedes arithmetic");
    std::vector<std::future<Bytes>> readers;
    for(int i=0;i<4;++i)readers.push_back(std::async(std::launch::async,[matched,authority]{return encode_relational_task_outcome(matched,authority);}));
    for(auto& f:readers)check(f.get()==image,"concurrent owning task readers retain full bytes");
    auto changing_authority=registry.context();bool replaced{};
    const auto stable=encode_relational_task_outcome(matched,changing_authority,256u<<20,[&]{if(!replaced){replaced=true;changing_authority=registry.without(matched.task().origin);}});
    check(replaced && stable==image,"encoding freezes original authority before callbacks");
    changing_authority=registry.context();replaced=false;
    auto read_policy=policy;
    const auto restored=decode_relational_task_outcome(image,changing_authority,read_policy,{},[&]{if(!replaced){replaced=true;changing_authority=registry.without(matched.task().origin);read_policy.max_file_bytes=1;}});
    check(replaced && encode_relational_task_outcome(restored,registry.context())==image,"cold replay freezes actual authority and read policy before callbacks");
    check(encode_relational_field_evidence(root.evidence())!=encode_relational_field_evidence(learned.evidence()),"original root and learned child remain distinct");
}
}
int main() { try { run(); std::cout << "task outcome checks: " << checks << '\n'; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
