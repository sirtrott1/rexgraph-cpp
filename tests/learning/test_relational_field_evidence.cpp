#include "rfc/learning/relational_affine_program_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
#include <string_view>
using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using T=numeric::SparseRationalTensor;
using Program=OwnedRelationalFieldProgram;
using Declaration=RelationalFieldProgramDeclaration;
using Evidence=PreparedRelationalFieldEvidence;
namespace {
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool caught{};try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
T tensor(const V& v){numeric::ExactActionGuard guard;return field_interface_detail::tensor(v,guard);}
storage::NativeSeriesEndpoint source(bool renamed = false) {
    core::RelationSignature signature;
    const auto relation = signature.add_type("relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{renamed ? 200u : 100u}, relation,
        {{renamed ? 40u : 10u, true, false, Q{1}}, {renamed ? 50u : 20u, true, true, Q{1}}, {renamed ? 40u : 10u, true, true, Q{1}},
         {renamed ? 50u : 20u, false, false, Q{1}}, {renamed ? 60u : 30u, true, false, Q{0}}});
    builder.add_grade_one(core::RelationKey{renamed ? 201u : 101u}, relation,
        {{renamed ? 40u : 10u, true, false, Q{1}}, {renamed ? 50u : 20u, true, false, Q{1, 2}}, {renamed ? 60u : 30u, true, true, Q{1}}});
    storage::TemporalEndpoint endpoint(builder.freeze(), 7);
    return storage::NativeTemporalSeries(endpoint, 991).tail();
}
Declaration declaration() {
    Declaration d; d.key = 51; d.revision = 2; d.differential = true;
    d.stalks = {{{core::Grade{0}, 0}, 1001, {101, 102}, M::from_rows({{2, 1}, {1, 3}})},
                {{core::Grade{0}, 1}, 1002, {201, 202, 203}, M::identity(3)},
                {{core::Grade{0}, 2}, 1001, {101, 102}, M::identity(2)},
                {{core::Grade{1}, 0}, 1003, {301, 302}, M::from_rows({{3, 1}, {1, 2}})},
                {{core::Grade{1}, 1}, 1003, {301, 302}, M::identity(2)}};
    const auto phi = storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences = {{core::Grade{1}, 0, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 1, phi, M::from_rows({{1, 0}, {0, 1}, {1, 1}})},
                     {core::Grade{1}, 2, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 3, phi, M::from_rows({{7, 3}, {2, 5}, {1, 1}})},
                     {core::Grade{1}, 4, phi, M::identity(2)},
                     {core::Grade{1}, 5, phi, M::from_rows({{2, 1}, {1, 3}})},
                     {core::Grade{1}, 6, phi, M::from_rows({{0, 1}, {1, 0}, {2, -1}})},
                     {core::Grade{1}, 7, storage::TemporalTensorRole::RealizedIncidenceMap,
                         M::from_rows({{-1, 1}, {2, -2}})}};
    d.nodes = {{1, inference::FieldProgramNodeKind::Input, core::Grade{1}, {}, {}},
               {2, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 1, {}},
               {3, inference::FieldProgramNodeKind::DeclaredMap, core::Grade{1}, 2,
                   M::from_rows({{1, 0, 1, 0, 0, 0, 0}, {0, 1, 0, 1, 0, 0, 0},
                                 {0, 0, 0, 0, 1, 1, 0}, {1, 0, 0, 0, 0, 0, 1}})},
               {4, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 3, {}}};
    return d;
}
void json(const V& values) {
    std::cout << '['; for (std::size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << '"' << values[i] << '"'; } std::cout << ']';
}
void json(const M& matrix) {
    std::cout << '['; for (std::size_t i = 0; i < matrix.rows(); ++i) {
        if (i) std::cout << ',';
        V row(matrix.cols());
        for (std::size_t j = 0; j < row.size(); ++j) row[j] = matrix.get(i, j);
        json(row);
    } std::cout << ']';
}


Declaration base_declaration() {
    auto d=declaration();d.nodes.resize(3);d.occurrences[0].map=M(2,2);d.occurrences[6].map=M(3,2);
    d.occurrences[7].role=storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences[7].map=d.occurrences[7].map.scale(-1);return d;
}
OwnedRelationalInputUseAsset asset(bool held=false,std::uint64_t ns=992) {
    const auto p=source(held);auto endpoint=p.endpoint();endpoint.sequence=held?20u:7u;
    storage::NativeTemporalSeries history(endpoint,ns);++endpoint.sequence;history.append_target(endpoint);++endpoint.sequence;history.append_target(endpoint);
    RelationalInputUseDeclaration d;d.key=600;d.channel=700;d.encoding="UTF-8";
    for(const auto c:std::string("caf\xc3\xa9 / caf\xc3\xa9"))d.bytes.push_back(static_cast<std::byte>(c));
    d.uses={{111,{storage::stable_cell_key(endpoint.source,{core::Grade{1},0})},{},{{0,5}}},
            {112,{storage::stable_cell_key(endpoint.source,{core::Grade{1},1})},{},{{8,13}}}};
    for(std::size_t i=0;i<8;++i)d.uses[0].occurrences.push_back({core::Grade{1},endpoint.source.occurrence_identity(core::Grade{1},i)});
    return {history,d};
}
OwnedAccessionFieldResult supplied(const Program& program,const OwnedRelationalInputUseAsset& bytes,const V& x,std::uint64_t use=111) {
    RelationalInputAccessionDeclaration a;a.key=901;a.node=1;a.source_grade={1};a.isometric=true;
    for(const auto& s:program.declaration().stalks)if(s.cell.grade.value==1)a.source_stalks.push_back(s);
    a.instances.cells={{0,1,2},{0,1}};a.instances.occurrences={{},{0,1,2,3,4,5,6,7}};
    a.accession=M::identity(4);a.reconstruction=M::identity(4);
    const OwnedRelationalInputAccession accession(bytes,program,a);return execute_relational_inputs(program,{accession.admit(use,tensor(x))});
}
RelationalFieldParameterDeclaration parameters() {
    RelationalFieldParameterDeclaration d;d.key=100;d.revision=1;d.node=2;d.parameters={101,102,103,104,105};
    d.variables={{0,0,0},{0,0,1},{0,1,0},{0,1,1},{6,0,0},{6,0,1},{6,1,0},{6,1,1},{6,2,0},{6,2,1},{3,0,0},{4,1,1}};
    d.basis=M(12,5);const std::vector<std::size_t> column{0,1,1,2,2,0,0,1,1,2,3,4};
    for(std::size_t i=0;i<column.size();++i)d.basis.set(i,column[i],1);
    return d;
}
// Independent declared target: two whole heterogeneous maps, not a terminal scalar fit.
V response(const V& x,const V& theta) {
    const auto a=x[0],b=x[1],c=x[2],d=x[3]; const auto u=theta[0],v=theta[1],w=theta[2];
    return {-a-2*b+2*c+d+u*a+v*b,-b+c+3*d+v*a+w*b,
        -a+(w*c+u*d)/2,-b+(u*c+v*d)/2,-a-b+(v*c+w*d)/2,-c+d,2*c-2*d};
}
RelationalFieldTrainingObservation observation(const Program& program,const OwnedRelationalInputUseAsset& bytes,const V& x,std::uint64_t key) {
    return {key,supplied(program,bytes,x,key%2?111:112),asset(false,3000+key),111,tensor(response(x,{2,-1,3})),
        {{core::Grade{0},0},{core::Grade{0},1},{core::Grade{0},2}}};
}
void affine_json(const RelationalAffineField& f) {
    std::cout<<"{\"particular\":";json(f.particular.values().dense());std::cout<<",\"directions\":";json(f.directions);std::cout<<'}';
}
void corpus(bool emit) {
    const auto p=source();const auto d=base_declaration();const Program program(p,d);
    const auto training=asset();const auto independent=asset(true,9001);
    // This held field and its actual unrelated use/cut are fixed before target construction.
    const V held_x{2,3,5,7};const auto held=supplied(program,independent,held_x,112);
    const Evidence initial(program,parameters());const auto before=initial.execute(held);
    const auto first=initial.append({observation(program,training,{1,2,0,0},1)});
    const auto learned=first.append({observation(program,training,{0,0,1,3},2)});
    check(initial.observations().empty()&&initial.kernel().cols()==5,"initial complete field family retained");
    check(first.kernel().cols()==3&&learned.kernel().cols()==2,"correlated active and unresolved inactive directions retained");
    check(learned.particular()==V({2,-1,3,0,0})&&learned.free_parameters()==std::vector<std::uint64_t>({104,105}),"named complete parameter family after update");
    check(learned.kernel().get(3,0)==1&&learned.kernel().get(4,1)==1,"inactive/zero Phi unknowns not identified as zero");
    check(first.parent_relation().has_value()&&learned.parent_relation()->map.rows()==3,"native child free-basis relation retained");
    const auto input=held.inputs()[0].accession();const auto original=input.asset();const auto pub=original.publication();
    check(pub.source_namespace()==9001&&pub.sequence()==22&&original.use(112).cells[0].key==201,"independent original source namespace/cut/cell use");
    check(input.declaration().instances.occurrences[1].size()==8,"actual structural relation reused through every original flag");
    const auto after=learned.execute(held);
    check(after.field(2).particular.values().dense()!=before.field(2).particular.values().dense(),"whole independently held response changes after native evidence update");
    check(after.field(2).particular.values().dense()==response(held_x,{2,-1,3})&&after.field(2).directions.nonzeros()==0,"held complete learned two-map response before readout");
    const auto after_evidence=after.evidence();
    check(after_evidence.kernel().cols()==2&&after.occurrence_field(2).particular.domain().logical_size()==19,"unknown actual maps and complete original flags remain owned");
    check(after.identified(2,0)&&!before.identified(2,0),"identified field coordinates separate from family representative");
    const auto first_map=first.coefficient_independent_map(core::Grade{1},0);
    check(first_map.particular()==M::from_rows({{-10,5},{5,0}})&&first_map.directions()[0]==M::from_rows({{4,-2},{-2,1}}),"complete actual local Phi family retains correlated stalk directions");
    const auto learned_map=learned.coefficient_independent_map(core::Grade{1},6);
    check(learned_map.particular()==M::from_rows({{3,2},{2,-1},{-1,3}})&&learned_map.directions()[0].nonzeros()==0&&learned_map.directions()[1].nonzeros()==0,"second heterogeneous learned local map remains complete");
    const auto inactive_map=learned.coefficient_independent_map(core::Grade{1},3);const auto zero_map=learned.coefficient_independent_map(core::Grade{1},4);
    check(inactive_map.directions()[0].get(0,0)==1&&zero_map.directions()[1].get(1,1)==1,"inactive and zero actual Phi fields retain unresolved alternatives");
    check(inactive_map.particular()==d.occurrences[3].map&&zero_map.particular()==d.occurrences[4].map,"zero affine origins do not erase intrinsic map bases");
    const auto image=encode_relational_field_evidence(learned);const auto cold=decode_relational_field_evidence(image,{});
    check(encode_relational_field_evidence(cold)==image&&cold.particular()==learned.particular()&&cold.kernel()==learned.kernel(),"cold whole evidence/parent replay");
    check(cold.observations().size()==2&&cold.parent()->observations().size()==1&&cold.parent()->parent()->observations().empty(),"ordered immutable parent observation lineage");
    const auto f1=first.execute(held);
    check(f1.field(2).directions.cols()==3&&f1.field(3).directions.cols()==3&&f1.field(2).directions.nonzeros()>1,"same correlated free axes through heterogeneous fixed stages");
    for(std::size_t trial=0;trial<40;++trial) {
        V x{Q{static_cast<long>(trial)-4,3},Q{static_cast<long>(trial%7)-2,5},Q{static_cast<long>(trial%11)-3,7},Q{static_cast<long>(trial%13)-6,2}};
        for(auto& q:x)q.canonicalize();
        const auto source_input=supplied(program,independent,x,trial%2?111:112);
        const auto family=first.execute(source_input);const auto complete=learned.execute(source_input);
        const auto evaluation=family.evaluate({3,5,7});const auto actual=evaluation.result();const auto actual_result=actual.result();
        check(actual_result.field(2).values().dense()==response(x,{2,-1,3}),"whole correlated family materializes declared actual maps");
        check(actual_result.field(3).values().dense()==d.nodes[2].map.apply(response(x,{2,-1,3})),"complete fixed next-stage response");
        const auto evaluation_evidence=evaluation.family().evidence();
        check(evaluation_evidence.observations().size()==1&&evaluation.free_values()==V({3,5,7}),"prospective evaluation retains evidence and chosen common xi");
        const auto child_program=actual_result.program();check(child_program.declaration().revision==d.revision+first.declaration().revision,"prospective child version retains fixed source/bases");
        check(child_program.declaration().occurrences[3].map.get(0,0)==d.occurrences[3].map.get(0,0)+5&&child_program.declaration().occurrences[4].map.get(1,1)==d.occurrences[4].map.get(1,1)+7,"prospective inactive Phi coordinates remain explicit");
        check(actual_result.occurrence_field(2).state(7)==Q{0}&&actual_result.occurrence_field(2).state(11)==Q{0},"inactive/supported-zero action distinct from unknown intrinsic maps");
        const auto archive=encode_relational_affine_result(family);const auto restored=decode_relational_affine_result(archive,{});
        check(encode_relational_affine_result(restored)==archive,"cold full affine intermediary family replay");
        for(const auto key:{1u,2u,3u})check(restored.field(key).particular.values().dense()==family.field(key).particular.values().dense()&&restored.field(key).directions==family.field(key).directions,"complete common p/K fields preserved before decoding");
        const auto eval_archive=encode_relational_affine_evaluation(evaluation);
        check(encode_relational_affine_evaluation(decode_relational_affine_evaluation(eval_archive,{}))==eval_archive,"cold prospective family/child lineage replay");
        check(complete.field(2).directions.nonzeros()==0&&complete.field(2).particular.values().dense()==response(x,{2,-1,3}),"independent inputs condition entire learned family");
        if(emit) {
            std::cout<<"{\"input\":";json(x);std::cout<<",\"parameter_particular\":";json(first.particular());std::cout<<",\"parameter_kernel\":";json(first.kernel());
            std::cout<<",\"learned_particular\":";json(learned.particular());std::cout<<",\"learned_kernel\":";json(learned.kernel());
            std::cout<<",\"fields\":[";for(const auto key:{1u,2u,3u}){if(key!=1)std::cout<<',';affine_json(family.field(key));}
            std::cout<<"],\"flags\":";affine_json(family.occurrence_field(2));std::cout<<",\"materialized\":";json(actual_result.field(2).values().dense());
            std::cout<<",\"application\":";json(d.nodes[2].map);std::cout<<"}\n";
        }
    }
}
void admissions_and_ownership() {
    const auto p=source();const auto d=base_declaration();const Program program(p,d);const auto bytes=asset();
    const auto input=supplied(program,bytes,{1,2,3,4});const auto params=parameters();const Evidence empty(program,params);
    rejects([&]{empty.coefficient_independent_map(core::Grade{1},999);},"map reader requires actual original flag");
    auto parameter=params;parameter.node=1;
    rejects([&]{Evidence invalid(program,parameter);},"Input cannot silently stand for learned Boundary");
    auto repeat=d;repeat.nodes.push_back({4,FieldProgramNodeKind::Boundary,core::Grade{0},3,{}});const Program repeated(p,repeat);
    rejects([&]{Evidence invalid(repeated,params);},"two learned-grade products rejected explicitly");
    auto missing=d;missing.occurrences[3].role=storage::TemporalTensorRole::RealizedIncidenceMap;missing.occurrences[3].map=M(3,2);const Program no_phi(p,missing);
    rejects([&]{Evidence invalid(no_phi,params);},"inactive realized zero does not infer Phi");
    parameter=params;parameter.parameters[1]=parameter.parameters[0];rejects([&]{Evidence invalid(program,parameter);},"duplicate named parameters rejected");
    parameter=params;parameter.variables.push_back(parameter.variables.front());rejects([&]{Evidence invalid(program,parameter);},"duplicate flag coordinates rejected");
    const Program same_bytes(p,d);const auto wrong=supplied(same_bytes,bytes,{1,2,3,4});
    rejects([&]{empty.execute(wrong);},"equal byte programs do not establish live input owner");
    auto o=observation(program,bytes,{1,2,0,0},1);const auto first=empty.append({o});
    auto conflict=o;conflict.key=2;auto target=conflict.target.values().dense();target[0]+=1;conflict.target=tensor(target);
    rejects([&]{first.append({conflict});},"inconsistent whole field suffix rejects before publication");
    rejects([&]{first.append({o});},"original observation identity cannot be reused");
    o.key=3;o.input=wrong;rejects([&]{first.append({o});},"evidence actual input owner rejects equal values");
    o=observation(program,bytes,{1,2,0,0},3);o.target_use=999;rejects([&]{first.append({o});},"target original use owner is required");
    o.target_use=111;o.observed={{core::Grade{1},0}};rejects([&]{first.append({o});},"target mask cannot alias another grade");
    o=observation(program,bytes,{1,2,0,0},3);o.observed={{core::Grade{0},0}};auto measured=o.target.values().dense();measured[2]=999;o.target=tensor(measured);
    const auto masked=empty.append({o});const auto residual=masked.residual(0);
    check(residual.field().particular.state(2)!=Q{0}&&residual.field().particular.state(0)==Q{0}&&residual.field().directions.cols()==masked.kernel().cols(),"complete residual preserves unselected measurement and common free axes");
    check(masked.observations()[0].target.state(2)==Q{999}&&masked.observation_system().target.size()==2,"unselected full target measurement retained outside equation mask");
    rejects([&]{empty.append({});},"empty source-backed suffix rejected");
    auto limits=numeric::ExactActionLimits{};limits.integer_bits=32;rejects([&]{empty.append({o},limits);},"append cannot change evidence policy");
    const auto family=first.execute(input);rejects([&]{family.evaluate({1});},"partial common xi not filled with zero");
    rejects([&]{family.occurrence_field(3);},"application stage has no invented flags");
    rejects([&]{family.identified(2,999);},"out of carrier identified read rejected");
    limits={};limits.entries=1;rejects([&]{first.execute(input,limits);},"whole affine action resource admission");
    rejects([&]{first.execute(input,{},[]{throw std::runtime_error("cancel");});},"owned synchronous action cancellation");
    const auto image=encode_relational_affine_result(family);auto damaged=image;damaged.back()^=std::byte{1};
    rejects([&]{decode_relational_affine_result(damaged,{});},"affine archive integrity");
    auto read=storage::TemporalReadLimits{};read.max_records=2;rejects([&]{decode_relational_affine_result(image,read);},"cold record admission");
    auto cap=numeric::ExactActionLimits{};cap.steps=10;rejects([&]{decode_relational_field_evidence(encode_relational_field_evidence(first),{},cap);},"whole archived evidence policy must be admitted");
    auto active=first;bool replaced{};const auto prior=active.execute(input,{},[&]{if(!replaced){replaced=true;active=empty;}});
    const auto prior_evidence=prior.evidence();
    check(prior_evidence.observations().size()==1&&active.observations().empty(),"evidence pinned before callback handle replacement");
    auto named=params;auto& original_name=named.parameters[0];bool mutated_name{};
    const Evidence frozen_names(program,std::move(named),{},[&]{if(!mutated_name){mutated_name=true;original_name=999;}});
    check(frozen_names.parameters()[0]==101,"moved named parameter buffers frozen before callbacks");
    std::vector<RelationalFieldTrainingObservation> suffix{observation(program,bytes,{1,2,0,0},4)};auto& original_record=suffix[0];bool mutated_record{};
    const auto frozen_observations=empty.append(std::move(suffix),{},[&]{if(!mutated_record){mutated_record=true;original_record.observed[0]={core::Grade{1},0};original_record.target=tensor(V(7,999));}});
    check(frozen_observations.observations()[0].observed[0]==core::CellId{core::Grade{0},0}&&frozen_observations.particular()==first.particular(),"moved original measurement/mask buffers frozen before callbacks");
    using namespace relational_affine_codec_detail;
    auto writer=envelope(affine_magic);blob(writer,encode_relational_field_evidence(first));blob(writer,encode_accession_field_result(input));writer.u64(3);
    for(const auto& n:d.nodes){writer.u64(n.key);auto false_field=family.field(n.key);if(n.key==2)false_field.directions=M(false_field.directions.rows(),false_field.directions.cols());relational_affine_codec_detail::field(writer,false_field);if(n.kind==FieldProgramNodeKind::Boundary)relational_affine_codec_detail::field(writer,family.occurrence_field(n.key));}
    const auto false_witness=finish(std::move(writer));rejects([&]{decode_relational_affine_result(false_witness,{});},"valid canonical envelope cannot certify false common intermediary directions");
    writer=envelope(evaluation_magic);blob(writer,encode_relational_affine_result(family));relational_affine_codec_detail::vector(writer,{3,5,7});blob(writer,encode_accession_field_result(input));
    const auto false_child=finish(std::move(writer));rejects([&]{decode_relational_affine_evaluation(false_child,{});},"valid envelope cannot substitute old program/fields for prospective native child");
    std::vector<std::future<V>> work;
    for(std::size_t i=0;i<4;++i)work.push_back(std::async(std::launch::async,[first,input]{const auto concurrent_family=first.execute(input);const auto result=concurrent_family.evaluate({3,5,7}).result();const auto actual=result.result();return actual.field(2).values().dense();}));
    for(auto& job:work)check(job.get()==response({1,2,3,4},{2,-1,3}),"independent concurrent affine materialization scratch");
}
void adjacent_laws() {
    core::RelationSignature signature;const auto type=signature.add_type("chain",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(type,{{10,true,true,1},{20,true,false,1}});
    builder.add_grade_one(type,{{20,true,true,1},{30,true,false,1}});
    builder.add_grade_one(type,{{10,true,true,1},{30,true,false,1}});
    for(std::size_t i=0;i<2;++i)builder.add_higher(core::Grade{2},type,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});
    builder.add_higher(core::Grade{3},type,{{0,true,false,1},{1,true,true,1}});
    storage::TemporalEndpoint endpoint(builder.freeze(),5);const storage::NativeTemporalSeries history(endpoint,180);
    Declaration d;d.key=500;d.revision=1;d.differential=true;
    for(std::size_t k=0;k<endpoint.source.grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        for(std::size_t i=0;i<endpoint.source.grade(grade).cell_count();++i)d.stalks.push_back({{grade,static_cast<std::uint32_t>(i)},1,{1,2},M::identity(2)});
        if(k)for(std::size_t p=0;p<endpoint.source.grade(grade).incidence_count();++p)d.occurrences.push_back({grade,p,storage::TemporalTensorRole::CoefficientIndependentMap,M::identity(2)});
    }
    d.nodes={{1,FieldProgramNodeKind::Input,core::Grade{2},{},{}},{2,FieldProgramNodeKind::Boundary,core::Grade{1},1,{}},{3,FieldProgramNodeKind::Boundary,core::Grade{0},2,{}}};
    const Program program(history.tail(),d);RelationalInputUseDeclaration u;u.key=1;u.channel=1;u.encoding="bytes";u.bytes={std::byte{'x'}};
    u.uses={{111,{storage::stable_cell_key(endpoint.source,{core::Grade{2},0})},{},{{0,1}}}};
    const OwnedRelationalInputUseAsset bytes(history,u);RelationalInputAccessionDeclaration a;a.key=1;a.node=1;a.source_grade={2};a.isometric=true;
    for(const auto& stalk:d.stalks)if(stalk.cell.grade.value==2)a.source_stalks.push_back(stalk);
    a.accession=M::identity(4);a.reconstruction=M::identity(4);
    for(std::size_t k=0;k<endpoint.source.grade_count();++k) {
        const auto g=core::Grade{static_cast<std::uint16_t>(k)};std::vector<std::uint32_t> cells;std::vector<std::size_t> flags;
        for(std::size_t i=0;i<endpoint.source.grade(g).cell_count();++i)cells.push_back(static_cast<std::uint32_t>(i));
        for(std::size_t i=0;i<endpoint.source.grade(g).incidence_count();++i)flags.push_back(i);
        a.instances.cells.push_back(std::move(cells));a.instances.occurrences.push_back(std::move(flags));
    }
    const OwnedRelationalInputAccession accession(bytes,program,a);
    const auto input=execute_relational_inputs(program,{accession.admit(111,tensor({1,2,3,4}))});
    RelationalFieldParameterDeclaration params;params.key=1;params.revision=1;params.node=2;
    for(std::size_t p=0;p<6;++p)for(std::size_t r=0;r<2;++r)for(std::size_t c=0;c<2;++c){params.variables.push_back({p,r,c});params.parameters.push_back(1000+params.parameters.size());}
    params.basis=M::identity(24);const Evidence initial(program,params);
    check(initial.kernel().cols()==4&&initial.chain_coordinates().size()>0,"both neighboring grade laws constrain whole tangent maps");
    bool down{},up{};for(const auto& row:initial.chain_coordinates()){down=down||row.grade==2;up=up||row.grade==3;}
    check(down&&up,"complete adjacent law attribution includes both neighbors");
    const auto before=initial.execute(input);
    check(before.field(3).directions.nonzeros()==0&&before.field(3).particular.values().dense()==V(6),"fixed lower Boundary propagates complete affine chain to zero");
    check(before.occurrence_field(3).directions.nonzeros()>0,"zero global chain action retains nonzero original flag alternatives");
    const RelationalFieldTrainingObservation o{1,input,OwnedRelationalInputUseAsset(storage::NativeTemporalSeries(endpoint,181),u),111,tensor({18,24,18,24,-18,-24}),{{core::Grade{1},0},{core::Grade{1},1},{core::Grade{1},2}}};
    const auto learned=initial.append({o});check(learned.kernel().cols()==2,"higher-grade evidence retains independent unobserved directions");
    const auto response=learned.execute(input);check(response.field(2).particular.values().dense()==V({18,24,18,24,-18,-24}),"higher-grade complete target fit");
    const auto prospective=response.evaluate({2,3});const auto owner=prospective.result();const auto actual=owner.result();const auto child=actual.program();
    auto first=local::GradeTensorField(child.fibers().fibers(),core::Grade{1}).matrix();auto middle=local::GradeTensorField(child.fibers().fibers(),core::Grade{2}).matrix();auto last=local::GradeTensorField(child.fibers().fibers(),core::Grade{3}).matrix();
    check(first.multiply(middle).nonzeros()==0&&middle.multiply(last).nonzeros()==0,"prospective maps satisfy every neighboring grade product");
    const auto image=encode_relational_affine_result(response);const auto cold=decode_relational_affine_result(image,{});
    check(encode_relational_affine_result(cold)==image&&cold.occurrence_field(3).directions==response.occurrence_field(3).directions,"cold full adjacent-law and original occurrence family replay");
}

}
int main(int argc,char** argv){try{const bool emit=argc>1&&std::string_view(argv[1])=="--emit";corpus(emit);admissions_and_ownership();adjacent_laws();
    if(!emit)std::cout<<"relational_field_evidence_checks="<<checks<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"field evidence failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
