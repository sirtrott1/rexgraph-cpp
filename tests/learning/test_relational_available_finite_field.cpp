#include "rfc/learning/relational_available_finite_field_codec.hpp"
#include <fstream>
#include <filesystem>
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
OwnedRelationalInputUseAsset finite_asset(bool held=true,std::uint64_t ns=9001) {
    const auto a=asset(held,ns);auto d=a.declaration();const auto p=a.publication();
    for(auto& u:d.uses)for(std::uint32_t i=0;i<3;++i)u.cells.push_back(storage::stable_cell_key(p.endpoint().source,{core::Grade{0},i}));
    return {a.history(),d};
}
RelationalFiniteSourceDeclaration finite_declaration() {
    RelationalFiniteSourceDeclaration d;d.key=6001;d.revision=1;
    d.symbols={{101,{std::byte{'a'}}},{102,{std::byte{'a'}}},{103,{}}};
    d.variables={{{core::Grade{0},0},{101,102,103}},{{core::Grade{0},1},{101,102,103}},{{core::Grade{0},2},{101,102,103}}};
    d.charts={{6101,"shared interpretation",{{core::Grade{0},0},{core::Grade{0},1},{core::Grade{0},2},{core::Grade{1},0}},
                {{core::Grade{0},0},{core::Grade{0},1}},{{101,101},{102,102},{103,103}}}};
    d.charts.push_back({6102,"second shared chart",{{core::Grade{0},0},{core::Grade{0},1},{core::Grade{0},2},{core::Grade{1},1}},
                        {{core::Grade{0},1},{core::Grade{0},2}},{{101,101},{102,102},{103,103}}});
    d.bindings={{6201,111,{core::Grade{0},0}},{6202,111,{core::Grade{0},1}},{6203,112,{core::Grade{0},0}},{6204,112,{core::Grade{0},1}},{6205,111,{core::Grade{0},2}}};return d;
}
using Origin=inference::OwnedRelationalValueOrigin;
using Grant=inference::RelationalOriginGrant;
using Context=inference::RelationalAvailabilityContext;
using Role=inference::RelationalDependencyRole;
using Available=PreparedAvailableRelationalEvidence;
using Bytes=std::vector<std::byte>;
template<class F>void rejects_origin(F f,const char* message){
    bool caught{};try{f();}catch(const std::invalid_argument& e){const std::string reason=e.what();caught=reason.find("origin")!=std::string::npos || reason.find("availability")!=std::string::npos;}
    check(caught,message);
}
storage::NativeSeriesPrefix publication(const OwnedRelationalInputUseAsset& bytes,bool late=false){
    auto history=bytes.history();if(late){const auto tail=history.tail();auto endpoint=tail.endpoint();++endpoint.sequence;history.append_target(endpoint);++endpoint.sequence;history.append_target(endpoint);}
    return storage::NativeSeriesPrefix::capture(history,history.last_sequence());
}
void grant(std::vector<Grant>& grants,std::uint64_t key,Role role,const Bytes& payload,storage::NativeSeriesPrefix prefix){
    const runtime::WorkAvailability cutoff{prefix.source_namespace(),prefix.cutoff()};
    grants.push_back({Origin(key,prefix,role,payload),cutoff});
}
RelationalInputSubmission submission(const Program& p,const OwnedRelationalInputUseAsset& bytes,const V& values){
    RelationalInputAccessionDeclaration a;a.key=901;a.node=1;a.source_grade={1};a.isometric=true;
    for(const auto& s:p.declaration().stalks)if(s.cell.grade.value==1)a.source_stalks.push_back(s);
    a.instances.cells={{0,1,2},{0,1}};a.instances.occurrences={{},{0,1,2,3,4,5,6,7}};
    a.accession=M::identity(4);a.reconstruction=M::identity(4);return {bytes,a,111,tensor(values)};
}
AvailableRelationalFieldResult published_input(const PreparedAvailableRelationalProgram& prepared,
    const OwnedRelationalInputUseAsset& bytes,const V& values,std::uint64_t first,std::vector<Grant>& grants){
    const auto spec=submission(prepared.program(),bytes,values);const auto ab=encode_relational_input_use_asset(bytes);
    const auto pb=encode_relational_field_program(prepared.program());
    const auto vp=relational_input_value_payload(ab,spec.declaration,spec.use,spec.source_field);
    const auto ap=relational_input_accession_payload(pb,ab,spec.declaration,spec.use,spec.source_field);
    grant(grants,first,Role::InputAsset,ab,publication(bytes));grant(grants,first+1,Role::InputValue,vp,publication(bytes,true));
    grant(grants,first+2,Role::InputAccession,ap,publication(bytes));grant(grants,first+3,Role::Reconstruction,ap,publication(bytes));
    const Context context(grants);const auto admitted=prepared.admit_input(spec,{first,first+1,first+2,first+3},context);
    return prepared.execute({admitted},context);
}
RelationalAvailableTrainingObservation measurement(const AvailableRelationalFieldResult& input,const Program& p,
    const V& values,std::uint64_t key,std::uint64_t first,std::vector<Grant>& grants){
    const auto target=asset(false,3000+key);const auto field=tensor(response(values,{2,-1,3}));
    const auto ab=encode_relational_input_use_asset(target);
    grant(grants,first,Role::TargetAsset,ab,publication(target));
    grant(grants,first+1,Role::TargetValue,relational_target_value_payload(encode_relational_field_program(p),2,ab,111,field),publication(target,true));
    return {key,input,target,111,field,{{core::Grade{0},0}},{first,first+1}};
}
Context withheld(const std::vector<Grant>& grants,std::uint64_t key){
    auto changed=grants;bool found{};for(auto& g:changed)if(g.origin.key()==key){g.availability.max_sequence=g.origin.publication().cutoff()-1;found=true;}
    if(!found)throw std::runtime_error("test missing origin");
    return Context(changed);
}

using Status=RelationalFiniteRelationshipStatus;
using FiniteSource=PreparedAvailableRelationalFiniteSource;
using FiniteMap=PreparedAvailableRelationalFiniteFieldMap;
using FiniteFamily=AvailableRelationalFiniteFieldResult;
RelationalAvailableInputSubmission publish_rule_input(const Program& p,const OwnedRelationalInputUseAsset& a,
    std::uint64_t use,std::uint64_t first,std::vector<Grant>& grants){
    auto raw=submission(p,a,{2,3,5,7});raw.use=use;
    const auto ab=encode_relational_input_use_asset(a);const auto pb=encode_relational_field_program(p);
    const auto ap=relational_input_accession_payload(pb,ab,raw.declaration,use,raw.source_field);
    grant(grants,first,Role::InputValue,relational_input_value_payload(ab,raw.declaration,use,raw.source_field),publication(a,true));
    grant(grants,first+1,Role::InputAccession,ap,publication(a));grant(grants,first+2,Role::Reconstruction,ap,publication(a));
    return {raw,{400,first,first+1,first+2}};
}
struct Fixture{FiniteMap model;Available evidence;std::vector<Grant> grants;};
Fixture fixture(bool identified=false,bool prospective=false){
    const auto status=prospective?Status::Prospective:Status::Observed;const auto relationship=prospective?Role::Prospective:Role::Evidence;
    const Program p(source(),base_declaration());const auto training=asset();const auto prefix=publication(training);
    std::vector<Grant> grants;const auto pb=encode_relational_field_program(p);
    grant(grants,1,Role::Program,pb,prefix);grant(grants,2,Role::Maps,pb,prefix);grant(grants,3,Role::Metrics,pb,prefix);
    const PreparedAvailableRelationalProgram prepared(p,{1,2,3},Context(grants));
    auto params=parameters();if(identified){params.parameters.resize(3);M basis(12,3);for(std::size_t row=0;row<12;++row)for(std::size_t col=0;col<3;++col)basis.set(row,col,params.basis.get(row,col));params.basis=basis;}
    grant(grants,4,Role::Evidence,relational_evidence_root_payload(pb,params,{}),prefix);
    const Available root(prepared,params,4,Context(grants));
    const auto input=published_input(prepared,training,{1,2,0,0},10,grants);
    auto observation=measurement(input,p,{1,2,0,0},71,30,grants);
    observation.observed={{core::Grade{0},0},{core::Grade{0},1},{core::Grade{0},2}};
    auto evidence=root.append({observation},Context(grants));if(identified){const auto second_input=published_input(prepared,training,{0,1,0,0},20,grants);auto second=measurement(second_input,p,{0,1,0,0},72,40,grants);second.observed=observation.observed;evidence=evidence.append({second},Context(grants));}
    const auto a=finite_asset();const auto ab=encode_relational_input_use_asset(a);const auto d=finite_declaration();
    grant(grants,400,Role::InputAsset,ab,publication(a));
    grant(grants,401,relationship,relational_finite_constraint_payload(ab,d,status),publication(a,true));
    const FiniteSource finite(a,d,status,{400,401},Context(grants));
    const auto first=publish_rule_input(p,a,111,411,grants);const auto second=publish_rule_input(p,a,112,431,grants);
    const auto unmatched=publish_rule_input(p,a,111,443,grants);
    RelationalAvailableFiniteFieldMapDeclaration map;map.key=6301;map.revision=1;
    map.rules={{6401,{6201,6202},{101,101},0,{first},{}},
               {6402,{6201,6202},{102,102},0,{first},{}},
               {6403,{6203,6204},{101,101},1,{second},{}},
               {6404,{6201},{103},{},{},"no declared continuous interpretation"},
               {6405,{6201,6202},{101,102},0,{unmatched},{}}};
    // The third target is deliberately unused, but remains part of the declaration closure.
    grant(grants,80,Role::Program,pb,prefix);grant(grants,81,Role::Maps,pb,prefix);grant(grants,82,Role::Metrics,pb,prefix);
    const PreparedAvailableRelationalProgram unused(p,{80,81,82},Context(grants));
    const std::vector<AvailableRelationalFiniteFieldTarget> targets{prepared,evidence,unused};
    const auto payload=relational_finite_interpretation_payload(finite,targets,map,status,Context(grants));
    grant(grants,500,Role::Maps,payload,publication(a));grant(grants,501,relationship,payload,publication(a,true));
    return {FiniteMap(finite,targets,map,status,{500,501},Context(grants)),evidence,grants};
}
Bytes authority_bytes(const std::vector<Grant>& grants){
    inference::field_program_codec_detail::Writer w;w.u64(grants.size());
    for(const auto& g:grants){w.u64(g.origin.key());w.u8(static_cast<std::uint8_t>(g.origin.role()));
        inference::field_program_codec_detail::blob(w,storage::encode_native_series(g.origin.publication().history()));
        inference::field_program_codec_detail::blob(w,g.origin.payload());w.u64(g.availability.source_namespace);w.u64(g.availability.max_sequence);}
    return std::move(w).take_bytes();
}
Context authority(const Bytes& bytes){
    const storage::TemporalReadLimits policy;inference::field_program_codec_detail::Reader r(bytes,policy);
    std::vector<Grant> grants;const auto n=r.count(41);
    for(std::size_t i=0;i<n;++i){const auto key=r.u64();const auto role=static_cast<Role>(r.u8());auto history=storage::decode_native_series(r.blob());
        const auto raw=r.blob();const runtime::WorkAvailability cutoff{r.u64(),r.u64()};
        grants.push_back({Origin(key,storage::NativeSeriesPrefix::capture(history,history.last_sequence()),role,Bytes(raw.begin(),raw.end())),cutoff});}
    check(!r.remaining(),"independent trusted registry complete");return Context(grants);
}
Bytes read_file(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("missing fixture file");const std::string s{std::istreambuf_iterator<char>(f),{}};
    Bytes out;for(const auto c:s)out.push_back(static_cast<std::byte>(c));return out;
}
void write_file(const std::filesystem::path& path,const Bytes& bytes){
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!f)throw std::runtime_error("fixture write");
}
AvailableRelationalFiniteFieldEvaluation choice(const Fixture& f,const FiniteFamily& family,std::vector<Grant>& grants,
    std::size_t ordinal,const V& free,std::uint64_t choice_key){
    const auto finite=f.model.source().finite();const auto a=finite.asset();const auto parameter=ordinal==1?600u:0u;
    if(parameter && std::none_of(grants.begin(),grants.end(),[](const auto& g){return g.origin.key()==600;}))
        grant(grants,600,Role::Prospective,relational_parameter_setting_payload(f.evidence,free),publication(a,true));
    const auto payload=relational_finite_completion_payload(family,ordinal,free,parameter,Context(grants));
    grant(grants,choice_key,Role::Prospective,payload,publication(a,true));
    return family.evaluate(ordinal,free,parameter,choice_key,Context(grants));
}
Bytes corrupted_native_result(const Bytes& bytes){
    using namespace available_finite_detail;const storage::TemporalReadLimits read;Reader r(bytes,read);header(r,bytes,available_finite_detail::result_magic);
    auto w=envelope(available_finite_detail::result_magic);blob(w,r.blob());auto claimed=r.blob();Bytes native(claimed.begin(),claimed.end());native[0]=std::byte{0};blob(w,native);
    return finish(std::move(w));
}
void run(bool emit,const std::filesystem::path& exports={}){
    const auto f=fixture();auto grants=f.grants;const Context context(grants);const storage::TemporalReadLimits read;
    const auto finite=f.model.source().finite();const auto a=finite.asset();const auto sections=f.model.source().sections(context);
    check(sections.sections().candidate_count()==27&&sections.sections().size()==3,"whole symbolic correlation retained");
    const auto selected_section=sections.select(1,1,context).front();
    check(selected_section.assignment()==std::vector<std::uint64_t>({102,102,102}),"owning selected assignment has stable ordinal");
    const auto family=f.model.execute(context);const auto raw=family.result();
    check(family.size()==4&&raw.branch(3).status==RelationalFiniteFieldStatus::Unsupported,"unresolved alternative is retained without zero field");
    check(raw.branch(0).section==raw.branch(1).section&&raw.branch(0).rule!=raw.branch(1).rule,"one structural section admits distinct source-use interpretations");
    const auto branches=family.select(0,4,context);check(branches[1].parent().size()==4,"selected branch retains complete family");
    const auto evidence=f.evidence.evidence();check(evidence.free_parameters().size()==3,"common correlated and inactive parameter axes retained");
    const auto sb=encode_available_relational_finite_source(f.model.source());const auto tb=encode_available_relational_finite_sections(sections);
    const auto mb=encode_available_relational_finite_field_map(f.model);const auto fb=encode_available_relational_finite_field_result(family);
    check(encode_available_relational_finite_source(decode_available_relational_finite_source(sb,context,read))==sb,"source canonical cold replay");
    check(encode_available_relational_finite_sections(decode_available_relational_finite_sections(tb,context,read))==tb,"sections canonical cold replay");
    check(encode_available_relational_finite_field_map(decode_available_relational_finite_field_map(mb,context,read))==mb,"map canonical cold replay");
    check(encode_available_relational_finite_field_result(decode_available_relational_finite_field_result(fb,context,read))==fb,"family canonical cold replay");
    for(const auto& g:grants){const auto key=g.origin.key();const auto absent=withheld(grants,key);
        rejects_origin([&]{(void)f.model.execute(absent);},"every declared dependency rechecked at execution");
        rejects_origin([&]{(void)family.select(0,1,absent);},"every declared dependency rechecked at selection");
        rejects_origin([&]{(void)decode_available_relational_finite_field_map(mb,absent,read);},"cold map requires whole closure");
        rejects_origin([&]{(void)decode_available_relational_finite_field_result(fb,absent,read);},"cold family requires whole closure");
    }
    rejects_origin([&]{(void)selected_section.admit(withheld(grants,401));},"escaped section retains constraint obligation");
    rejects_origin([&]{(void)branches[0].admit(withheld(grants,443));},"unmatched rule retained in escaped branch admission");
    rejects([&]{(void)family.select(4,1,context);},"bounded branch selection rejects overflow");
    numeric::ExactActionLimits tiny;tiny.entries=1;
    rejects([&]{(void)family.select(0,2,context,tiny);},"bounded branch selection entry refusal");
    rejects([&]{(void)f.model.execute(context,{},[]{throw std::runtime_error("cancel");});},"cancelled execution has no published result");
    const auto malformed=corrupted_native_result(fb);
    rejects_origin([&]{(void)decode_available_relational_finite_field_result(malformed,withheld(grants,443),read);},"unmatched authority rejects before malformed native result replay");
    rejects([&]{(void)decode_available_relational_finite_field_result(malformed,context,read);},"authorized facts cannot accept a forged native witness");
    auto wrong=f.model.declaration();wrong.rules[0].inputs[0].input.declaration.reconstruction=M(4,4);
    auto bad_grants=grants;std::erase_if(bad_grants,[](const auto& g){return g.origin.key()==411||g.origin.key()==412||g.origin.key()==413||g.origin.key()==500||g.origin.key()==501;});
    const auto& bad_input=wrong.rules[0].inputs[0].input;const auto p=available_finite_detail::program(f.model.targets()[0]).program();
    const auto ap=relational_input_accession_payload(encode_relational_field_program(p),encode_relational_input_use_asset(a),bad_input.declaration,bad_input.use,bad_input.source_field);
    // Both equal-source-use rules share these receipts, so freeze the same changed A/R declaration in both.
    wrong.rules[1].inputs[0].input.declaration=bad_input.declaration;
    grant(bad_grants,411,Role::InputValue,relational_input_value_payload(encode_relational_input_use_asset(a),bad_input.declaration,bad_input.use,bad_input.source_field),publication(a,true));
    grant(bad_grants,412,Role::InputAccession,ap,publication(a));grant(bad_grants,413,Role::Reconstruction,ap,publication(a));
    const auto bad_payload=relational_finite_interpretation_payload(f.model.source(),f.model.targets(),wrong,Status::Observed,Context(bad_grants));
    grant(bad_grants,500,Role::Maps,bad_payload,publication(a));grant(bad_grants,501,Role::Evidence,bad_payload,publication(a,true));
    rejects_origin([&]{(void)FiniteMap(f.model.source(),f.model.targets(),wrong,Status::Observed,{500,501},withheld(bad_grants,443));},"unmatched late value rejects before earlier invalid reconstruction");
    bool reconstruction_error{};try{(void)FiniteMap(f.model.source(),f.model.targets(),wrong,Status::Observed,{500,501},Context(bad_grants));}
    catch(const std::invalid_argument& e){reconstruction_error=std::string(e.what()).find("R A = I")!=std::string::npos;}
    check(reconstruction_error,"fully admitted invalid reconstruction reaches actual native A/R validation");
    auto malformed_source=finite.declaration();malformed_source.variables[0].domain.clear();
    rejects_origin([&]{(void)FiniteSource(a,malformed_source,Status::Observed,{400,401},withheld(grants,401));},"constraints precede native predicate construction");
    auto foreign_grants=grants;const auto old=foreign_grants.front();foreign_grants.front().origin=Origin(old.origin.key(),old.origin.publication(),old.origin.role(),old.origin.payload());
    rejects_origin([&]{(void)family.admit(Context(foreign_grants));},"equal receipt does not replace actual live publisher owner");
    auto foreign=wrong;foreign=f.model.declaration();const auto clone=decode_relational_input_use_asset(encode_relational_input_use_asset(a),read);foreign.rules[0].inputs[0].input.asset=clone;
    rejects([&]{(void)FiniteMap(f.model.source(),f.model.targets(),foreign,Status::Observed,{500,501},context);},"equal source bytes do not establish actual live asset identity");
    const auto evaluation=choice(f,family,grants,1,{3,5,7},601);const auto fixed=choice(f,family,grants,0,{},602);const Context choices(grants);
    check(evaluation.family().size()==4&&evaluation.ordinal()==1,"affine completion retains all finite alternatives");
    const auto native=evaluation.result();const auto result=native.result();const auto fields=result.result();const auto program=fields.program();
    check(fields.field(2).values().dense()==response({2,3,5,7},{2,-1,3}),"independent whole heterogeneous response changed by common learned coordinates");
    check(program.declaration().occurrences.size()==8&&program.declaration().occurrences[3].map.get(0,0)==12&&program.declaration().occurrences[4].map.get(1,1)==8,"inactive and zero participation keep learned maps");
    check(result.inputs()[0].use()==112&&encode_relational_input_use_asset(result.inputs()[0].accession().asset())==encode_relational_input_use_asset(a),"actual original bytes and distinct use mapping retained");
    check(!fixed.parameter_setting()&&fixed.origin().role()==Role::Prospective,"observed fixed model still needs positive prospective choice");
    rejects_origin([&]{(void)family.evaluate(1,{3,5,7},600,601,withheld(grants,600));},"global setting is an independent obligation");
    rejects_origin([&]{(void)family.evaluate(1,{3,5,7},600,601,withheld(grants,601));},"input-specific choice is an independent obligation");
    rejects_origin([&]{(void)family.evaluate(2,{},0,602,choices);},"equal numerical fixed fields do not authorize another finite interpretation");
    rejects([&]{(void)family.evaluate(3,{},0,602,choices);},"unsupported branch cannot fabricate a zero completion");
    rejects([&]{(void)family.evaluate(0,{1},600,602,choices);},"fixed completion rejects affine setting");
    const auto cb=encode_available_relational_finite_field_evaluation(evaluation);const auto xb=encode_available_relational_finite_field_evaluation(fixed);
    check(encode_available_relational_finite_field_evaluation(decode_available_relational_finite_field_evaluation(cb,choices,read))==cb,"affine completion canonical cold replay");
    check(encode_available_relational_finite_field_evaluation(decode_available_relational_finite_field_evaluation(xb,choices,read))==xb,"fixed completion canonical cold replay");
    for(const auto key:{600u,601u,443u,82u}){
        rejects_origin([&]{(void)evaluation.admit(withheld(grants,key));},"escaped completion preserves all obligations");
        rejects_origin([&]{(void)decode_available_relational_finite_field_evaluation(cb,withheld(grants,key),read);},"cold completion preflights global and branch closure");
    }
    check(encode_available_relational_finite_field_result(family)==fb,"parameter choice leaves complete intermediary family unchanged");
    const auto identified_fixture=fixture(true,true);auto identified_grants=identified_fixture.grants;
    const Context identified_context(identified_grants);const auto identified_family=identified_fixture.model.execute(identified_context);
    const auto identified_native=identified_fixture.evidence.evidence();check(identified_native.kernel().cols()==0,"identified evidence has empty common xi");
    check(identified_fixture.model.status()==Status::Prospective&&identified_fixture.model.source().status()==Status::Prospective,"prospective constraint and interpretation remain explicit");
    rejects([&]{(void)identified_family.evaluate(1,{},0,0,identified_context);},"empty affine xi still requires positive global setting");
    const auto empty_choice=choice(identified_fixture,identified_family,identified_grants,1,{},601);
    check(empty_choice.parameter_setting().has_value(),"empty xi retains independent actual global-setting receipt");
    const auto empty_bytes=encode_available_relational_finite_field_evaluation(empty_choice);
    check(encode_available_relational_finite_field_evaluation(decode_available_relational_finite_field_evaluation(empty_bytes,Context(identified_grants),read))==empty_bytes,"empty affine xi and prospective model canonical cold replay");
    auto buffer=cb;bool changed{};const auto frozen=decode_available_relational_finite_field_evaluation(buffer,choices,read,{},[&]{if(!changed){changed=true;buffer.clear();}});
    check(changed&&encode_available_relational_finite_field_evaluation(frozen)==cb,"cold decoder freezes caller bytes before callback");
    std::atomic<bool> okay{true};std::vector<std::thread> readers;
    for(unsigned i=0;i<6;++i)readers.emplace_back([&]{try{for(unsigned j=0;j<2;++j){evaluation.admit(choices);const auto copy=decode_available_relational_finite_field_evaluation(cb,choices,read);if(encode_available_relational_finite_field_evaluation(copy)!=cb)okay=false;}}catch(...){okay=false;}});
    for(auto& worker:readers){worker.join();}check(okay,"six immutable readers retain full available ancestry");
    if(!exports.empty()){std::filesystem::create_directories(exports);write_file(exports/"result.packet",fb);write_file(exports/"choice.packet",cb);write_file(exports/"authority.bin",authority_bytes(grants));}
    for(std::size_t i=0;i<12;++i){
        const V xi{Q{static_cast<long>(i)-3},Q{static_cast<long>(i)+1},Q{2*static_cast<long>(i)-4}};
        auto current=grants;std::erase_if(current,[](const auto& g){return g.origin.key()>=600;});
        const auto completed=choice(f,family,current,1,xi,700+i);const auto chosen=completed.result();const auto owned=chosen.result();const auto whole=owned.result();
        const auto affine=std::get<RelationalAffineProgramResult>(*raw.branch(1).value);const auto& field=affine.field(2);
        auto theta=evidence.particular();const auto delta=evidence.kernel().apply(xi);for(std::size_t j=0;j<theta.size();++j)theta[j]+=delta[j];
        check(whole.field(2).values().dense()==response({2,3,5,7},{theta[0],theta[1],theta[2]}),"whole response follows correlated common parameter setting");
        if(emit){std::cout<<"{\"xi\":";json(xi);std::cout<<",\"input\":";json(V{2,3,5,7});std::cout<<",\"particular\":";json(field.particular.values().dense());std::cout<<",\"directions\":";json(field.directions);
            std::cout<<",\"response\":";json(whole.field(2).values().dense());std::cout<<",\"next\":";json(whole.field(3).values().dense());std::cout<<",\"occurrences\":";
            json(whole.occurrence_field(2).values().dense());std::cout<<"}\n";}
    }
}
} // namespace
int main(int argc,char** argv){
    try{
        if(argc==4&&(std::string(argv[1])=="--read-result"||std::string(argv[1])=="--read-choice")){
            const auto context=authority(read_file(argv[3]));const auto packet=read_file(argv[2]);const storage::TemporalReadLimits read;
            if(std::string(argv[1])=="--read-result"){const auto family=decode_available_relational_finite_field_result(packet,context,read);check(family.size()==4,"standalone full alternatives");check(encode_available_relational_finite_field_result(family)==packet,"standalone family canonical");}
            else{const auto value=decode_available_relational_finite_field_evaluation(packet,context,read);const auto native=value.result();const auto result=native.result();const auto fields=result.result();check(value.family().size()==4&&native.ordinal()==1&&result.inputs()[0].use()==112,"standalone original use and complete family");check(fields.field(2).values().dense()==response({2,3,5,7},{2,-1,3}),"standalone whole response");check(encode_available_relational_finite_field_evaluation(value)==packet,"standalone completion canonical");}
            std::cout<<checks<<" standalone checks passed\n";return 0;
        }
        if(argc==3&&std::string(argv[1])=="--write")run(false,argv[2]);else run(argc==2&&std::string(argv[1])=="--emit");
        if(!(argc==2&&std::string(argv[1])=="--emit"))std::cout<<checks<<" available finite checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"available finite failure: "<<e.what()<<'\n';return 1;}
}
