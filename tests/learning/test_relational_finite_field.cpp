#include "rfc/learning/relational_finite_field_codec.hpp"
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
PreparedRelationalFiniteFieldMap finite_map(bool held=true) {
    const Program program(source(),base_declaration());const auto a=finite_asset(held);const OwnedRelationalFiniteSource finite(a,finite_declaration());
    const Evidence initial(program,parameters());const auto learned=initial.append({observation(program,asset(),{1,2,0,0},1)});
    const auto first_input=supplied(program,a,{2,3,5,7});const auto second_input=supplied(program,a,{2,3,5,7},112);
    auto input=first_input.inputs();auto second=second_input.inputs();
    RelationalFiniteFieldMapDeclaration d;d.key=6301;d.revision=1;
    d.rules={{6401,{6201,6202},{101,101},0,input,{}},
             {6402,{6201,6202},{102,102},0,input,{}},
             {6403,{6203,6204},{101,101},1,second,{}},
             {6404,{6201},{103},{},{},"no declared continuous interpretation"}};
    return {finite,{program,learned},d};
}
void bytes(const std::filesystem::path& p,const std::vector<std::byte>& v) {
    std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(v.data()),static_cast<std::streamsize>(v.size()));check(bool(f),"write standalone packet");
}
std::vector<std::byte> bytes(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary);std::vector<char> raw((std::istreambuf_iterator<char>(f)),{});std::vector<std::byte> v;
    for(const auto c:raw)v.push_back(static_cast<std::byte>(c));
    return v;
}
void corpus(bool emit) {
    const auto map=finite_map();const auto result=map.execute();const auto finite=result.sections();
    check(finite.candidate_count()==27&&finite.size()==3&&result.size()==4,"correlation reduces 27 candidates to three sections, preserving four interpretations");
    check(finite.assignment(0)==std::vector<std::uint64_t>({101,101,101})&&finite.assignment(1)==std::vector<std::uint64_t>({102,102,102}),"opaque finite identity/order survives equal definitions");
    const auto& b0=result.branch(0);const auto& b1=result.branch(1);const auto& b2=result.branch(2);const auto& b3=result.branch(3);
    check(b0.rule==6401&&b1.rule==6403&&b2.rule==6402&&b3.rule==6404,"all matching rules retain deterministic section/rule order");
    const auto fixed0=std::get<OwnedAccessionFieldResult>(*b0.value);const auto fixed2=std::get<OwnedAccessionFieldResult>(*b2.value);
    check(encode_accession_field_result(fixed0)==encode_accession_field_result(fixed2)&&b0.section!=b2.section&&b0.rule!=b2.rule,"equal fields remain distinct interpretations");
    check(b3.status==RelationalFiniteFieldStatus::Unsupported&&!b3.value,"unsupported interpretation is an owned alternative without zero field");
    const auto learned=std::get<RelationalAffineProgramResult>(*b1.value);const auto evidence=learned.evidence();
    check(evidence.kernel().cols()==3&&learned.field(2).directions.cols()==3&&learned.occurrence_field(2).directions.cols()==3,"whole map/field common affine axes stay inside finite branch");
    const auto learned_input=learned.input();
    check(learned_input.inputs()[0].use()==112&&fixed0.inputs()[0].use()==111,"equal numerical encodings retain separate original uses");
    const auto original=map.source().asset();const auto pub=original.publication();
    check(pub.sequence()==22&&pub.source_namespace()==9001&&original.recover(111,10)==original.recover(112,10),"same literal bytes with retained namespace/cut/use provenance");
    const auto fixed_fields=fixed0.result();
    check(pub.endpoint().source.grade(core::Grade{1}).incidence_count()==8&&fixed_fields.occurrence_field(2).domain().logical_size()==19,"all repeated/inactive/zero original occurrences preserved");
    const auto inactive=evidence.coefficient_independent_map(core::Grade{1},3);
    check(inactive.directions()[1].get(0,0)==1,"unresolved inactive map direction preserved independently of terminal response");
    for(std::size_t trial=0;trial<12;++trial) {
        V xi{Q{static_cast<long>(trial)-4,3},Q{static_cast<long>(trial%7)-2,5},Q{static_cast<long>(trial%11)-3,7}};
        for(auto& q:xi)q.canonicalize();
        const auto selected=learned.evaluate(xi);const auto selected_result=selected.result();const auto fields=selected_result.result();
        check(fields.field(2).values().dense()==response({2,3,5,7},{-10+4*xi[0],5-2*xi[0],xi[0]}),"complete held field follows correlated learned maps");
        if(emit){std::cout<<"{\"xi\":";json(xi);std::cout<<",\"input\":";json(V{2,3,5,7});
            std::cout<<",\"particular\":";json(learned.field(2).particular.values().dense());std::cout<<",\"directions\":";json(learned.field(2).directions);
            std::cout<<",\"response\":";json(fields.field(2).values().dense());std::cout<<",\"next\":";json(fields.field(3).values().dense());
            std::cout<<",\"occurrences\":";json(fields.occurrence_field(2).values().dense());std::cout<<"}\n";}
    }
    const auto image=encode_relational_finite_field_result(result);const auto cold=decode_relational_finite_field_result(image,{});
    check(encode_relational_finite_field_result(cold)==image&&cold.size()==result.size(),"canonical complete finite/field cold replay");
    const auto cold_map=cold.map();const auto cold_source=cold_map.source();
    for(const auto& rule:cold_map.declaration().rules)for(const auto& input:rule.inputs)
        check(input.accession().asset().same_owner(cold_source.asset()),"cold adopts actual finite asset instead of equal-byte owner substitution");
    auto d=map.declaration();d.rules.erase(d.rules.begin()+1,d.rules.end());const PreparedRelationalFiniteFieldMap partial(map.source(),map.targets(),d);const auto unresolved=partial.execute();
    check(unresolved.size()==3&&unresolved.branch(1).status==RelationalFiniteFieldStatus::Unmapped&&!unresolved.branch(1).value,"unmapped symbols remain unresolved alternatives");
    const auto other=finite_map(false).execute();check(encode_relational_finite_field_result(other)!=image,"same semantic numerical values at different original cuts are distinct");
    const auto escaped=[] { const auto owner=finite_map().execute(); return owner.select(3,1); }();
    check(escaped[0].ordinal==3&&escaped[0].finite.assignment(escaped[0].section)==std::vector<std::uint64_t>({103,103,103})&&escaped[0].map.declaration().rules[3].unsupported=="no declared continuous interpretation","escaped unsupported branch owns finite assignment, original asset and mapping reason");
    const auto rows=result.select(0,2);check(rows.size()==2&&rows[1].rule==6403&&finite.select(1,1)[0]==finite.assignment(1),"bounded copies retain actual branch owners");
    rejects([&]{result.select(3,2);},"branch inspection bounds");rejects([&]{finite.select(3,1);},"finite inspection bounds");
    std::vector<std::future<std::vector<std::byte>>> readers;
    for(int i=0;i<6;++i)readers.push_back(std::async(std::launch::async,[map]{return encode_relational_finite_field_result(map.execute());}));
    for(auto& f:readers)check(f.get()==image,"immutable finite owners and whole fields survive six concurrent readers");
}
void finite_cases(bool emit) {
    const auto original=finite_asset();std::uint64_t rng=0x1234abcd;
    for(std::size_t trial=0;trial<64;++trial) {
        auto d=finite_declaration();
        for(auto& chart:d.charts) {
            chart.allowed.clear();rng^=rng<<7; rng^=rng>>9; rng^=rng<<8;const auto mask=(rng&511u)|1u;
            for(std::uint64_t a=0;a<3;++a)for(std::uint64_t b=0;b<3;++b)
                if(mask&(std::uint64_t{1}<<(3*a+b)))chart.allowed.push_back({101+a,101+b});
        }
        const OwnedRelationalFiniteSource finite(original,d);const auto sections=finite.sections();check(sections.candidate_count()==27,"complete bounded finite compatibility candidate count");
        if(emit) {
            std::cout<<"{\"charts\":[";
            for(std::size_t i=0;i<d.charts.size();++i){if(i)std::cout<<',';std::cout<<'[';
                for(std::size_t j=0;j<d.charts[i].allowed.size();++j){if(j)std::cout<<',';const auto& r=d.charts[i].allowed[j];std::cout<<'['<<r[0]<<','<<r[1]<<']';}std::cout<<']';}
            std::cout<<"],\"assignments\":[";
            for(std::size_t i=0;i<sections.size();++i){if(i)std::cout<<',';const auto& r=sections.assignment(i);std::cout<<'['<<r[0]<<','<<r[1]<<','<<r[2]<<']';}std::cout<<"]}\n";
        }
    }
}
void admissions() {
    const auto map=finite_map();const auto finite=map.source();auto d=finite.declaration();
    d.variables[0].domain.push_back(101);rejects([&]{OwnedRelationalFiniteSource(finite.asset(),d);},"duplicate finite symbol domain rejected");
    d=finite.declaration();d.bindings[0].cell={core::Grade{1},0};rejects([&]{OwnedRelationalFiniteSource(finite.asset(),d);},"binding outside actual variable rejected");
    d=finite.declaration();d.charts[0].patch.pop_back();const OwnedRelationalFiniteSource vertex_chart(finite.asset(),d);check(vertex_chart.sections().size()==3,"closed object-only chart valid");
    d=finite.declaration();d.charts[0].patch.erase(d.charts[0].patch.begin());rejects([&]{OwnedRelationalFiniteSource(finite.asset(),d);},"nonclosed chart rejected");
    d=finite.declaration();d.charts.push_back({6103,"contradicting",d.charts[0].patch,d.charts[0].support,{{101,102},{102,103},{103,101}}});
    const OwnedRelationalFiniteSource unsat(finite.asset(),d);check(unsat.sections().size()==0,"empty complete family means incompatibility, not zero field");
    const PreparedRelationalFiniteFieldMap no_sections(unsat,{}, {6302,1,{}});check(no_sections.execute().size()==0,"unsatisfiable source has no fabricated branches");
    auto rules=map.declaration();rules.rules[0].inputs.clear();rejects([&]{PreparedRelationalFiniteFieldMap(finite,map.targets(),rules);},"complete input omissions reject before execution");
    rules=map.declaration();rules.rules[0].inputs.push_back(rules.rules[0].inputs[0]);rejects([&]{PreparedRelationalFiniteFieldMap(finite,map.targets(),rules);},"duplicate numerical input node rejected");
    rules=map.declaration();const auto target=relational_finite_field_detail::program(map.targets()[0]);
    const Program foreign(target.publication(),target.declaration());const auto foreign_input=supplied(foreign,finite.asset(),{2,3,5,7});rules.rules[0].inputs=foreign_input.inputs();
    rejects([&]{PreparedRelationalFiniteFieldMap(finite,map.targets(),rules);},"equal source bytes do not identify actual program owner");
    rules=map.declaration();const auto a=finite.asset();const OwnedRelationalInputUseAsset foreign_asset(a.history(),a.declaration());
    const auto foreign_asset_input=supplied(target,foreign_asset,{2,3,5,7});rules.rules[0].inputs=foreign_asset_input.inputs();rejects([&]{PreparedRelationalFiniteFieldMap(finite,map.targets(),rules);},"equal literal bytes do not identify finite asset owner");
    rules=map.declaration();const auto undeclared_use=supplied(target,a,{2,3,5,7},112);rules.rules[0].inputs=undeclared_use.inputs();rejects([&]{PreparedRelationalFiniteFieldMap(finite,map.targets(),rules);},"undeclared numerical use dependency rejected");
    rules=map.declaration();rules.rules[0].symbols[0]=999;rejects([&]{PreparedRelationalFiniteFieldMap(finite,map.targets(),rules);},"undeclared finite symbol rejected");
    auto limits=numeric::ExactActionLimits{};limits.steps=8;rejects([&]{finite.sections(limits);},"complete enumeration work preflight");
    limits={};limits.entries=17;rejects([&]{finite.sections(limits);},"Cartesian stored row admission");
    rejects([&]{map.execute({},[]{throw std::runtime_error("cancel");});},"cancellation publishes no partial finite family");
    auto mutable_finite=finite;bool replaced{};const auto sections=mutable_finite.sections({},[&]{if(!replaced){replaced=true;mutable_finite=unsat;}});
    check(sections.size()==3&&mutable_finite.sections().size()==0,"finite predicate owns pinned source before callback replaces handle");
    auto mutable_map=map;bool changed{};const auto old=mutable_map.execute({},[&]{if(!changed){changed=true;mutable_map=no_sections;}});
    check(old.size()==4&&mutable_map.execute().size()==0,"whole composition pins map before callback replacement");
    rules=map.declaration();auto& moved_rule=rules.rules[0];bool changed_rule{};
    const PreparedRelationalFiniteFieldMap frozen(finite,map.targets(),std::move(rules),{},[&]{if(!changed_rule){changed_rule=true;moved_rule.symbols[0]=999;}});
    check(frozen.declaration().rules[0].symbols[0]==101,"moved dependency vectors frozen before callbacks");
    auto targets=map.targets();auto& moved_target=targets[0];bool changed_target{};
    const PreparedRelationalFiniteFieldMap frozen_targets(finite,std::move(targets),map.declaration(),{},[&]{if(!changed_target){changed_target=true;moved_target=foreign;}});
    const auto actual=relational_finite_field_detail::program(frozen_targets.targets()[0]);check(actual.fibers().same_owner(target.fibers()),"moved actual target owners frozen before callbacks");
    auto caller_image=encode_relational_finite_field_result(map.execute());const auto expected_image=caller_image;auto caller_policy=storage::TemporalReadLimits{};bool altered{};
    const auto frozen_replay=decode_relational_finite_field_result(caller_image,caller_policy,{},[&]{if(!altered){altered=true;caller_image[0]=std::byte{0};caller_policy.max_file_bytes=1;}});
    check(encode_relational_finite_field_result(frozen_replay)==expected_image,"cold bytes and read policy frozen before callbacks");
    auto image=encode_relational_finite_field_result(map.execute());image.back()^=std::byte{1};rejects([&]{decode_relational_finite_field_result(image,{});},"finite envelope corruption rejected");
    using namespace relational_finite_field_codec_detail;
    image=encode_relational_finite_field_result(map.execute());image.back()^=std::byte{1};
    const auto hash=storage::rfcx_detail::fnv1a64(std::span<const std::byte>(image).subspan(32));for(unsigned i=0;i<8;++i)image[24+i]=static_cast<std::byte>((hash>>(i*8u))&255u);
    rejects([&]{decode_relational_finite_field_result(image,{});},"claimed complete fields recomputed despite repaired envelope hash");
    auto policy=storage::TemporalReadLimits{};policy.max_file_bytes=32;rejects([&]{decode_relational_finite_field_result(encode_relational_finite_field_result(map.execute()),policy);},"bounded finite packet parsing");
}
} // namespace
int main(int argc,char** argv){try{
    if(argc==3&&std::string_view(argv[1])=="--write"){const auto result=finite_map().execute();bytes(std::filesystem::path(argv[2]),encode_relational_finite_field_result(result));return 0;}
    if(argc==3&&std::string_view(argv[1])=="--read"){const auto image=bytes(std::filesystem::path(argv[2]));const auto result=decode_relational_finite_field_result(image,{});check(result.size()==4,"standalone complete branch cold replay");check(encode_relational_finite_field_result(result)==image,"standalone canonical bytes");std::cout<<"standalone finite whole-field pass\n";return 0;}
    const bool emit=argc>1&&std::string_view(argv[1])=="--emit";corpus(emit);finite_cases(emit);admissions();std::cerr<<"checks="<<checks<<"\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}}
