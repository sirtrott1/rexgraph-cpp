#include "rfc/learning/relational_finite_operator_codec.hpp"
#include <fstream>
#include <filesystem>
#include "rfc/core/builder.hpp"
#include <future>
#include <atomic>
#include <thread>
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
core::RelationalComplex target(bool bad_inactive_flag=false,bool bad_scale=false){
    core::RelationSignature signature;const auto relation=signature.add_type("relation",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder b(signature,core::SemanticScale{static_cast<std::uint16_t>(bad_scale?1u:0u)});
    b.declare_witness_identity(40);b.declare_witness_identity(50);b.declare_witness_identity(60);
    b.add_grade_one(core::RelationKey{200},relation,{{60,true,false,Q{1}},{40,true,false,Q{1,2}},{50,true,true,Q{1}}});
    b.add_grade_one(core::RelationKey{201},relation,{{60,true,false,Q{1}},{40,true,true,Q{1}},{60,true,true,Q{1}},{40,false,bad_inactive_flag,Q{1}},{50,true,false,Q{0}}});
    return b.freeze();
}
algebra::RetainedSourceMap correspondence(const core::RelationalComplex& a,const core::RelationalComplex& b){
    algebra::RetainedSourceMap m;m.cells.resize(2);m.occurrences.resize(2);
    for(const auto key:a.grade(core::Grade{0}).identities){const auto mapped=key==10?60u:key==20?40u:50u;const auto& ids=b.grade(core::Grade{0}).identities;
        m.cells[0].push_back(static_cast<std::uint32_t>(std::find(ids.begin(),ids.end(),mapped)-ids.begin()));}
    const auto& left=a.grade(core::Grade{1});const auto& right=b.grade(core::Grade{1});
    for(std::size_t i=0;i<left.cell_count();++i){const auto mapped=core::RelationKey{left.relation_identities[i]==core::RelationKey{100}?201u:200u};
        m.cells[1].push_back(static_cast<std::uint32_t>(std::find(right.relation_identities.begin(),right.relation_identities.end(),mapped)-right.relation_identities.begin()));}
    for(std::size_t i=0;i<left.incidence_count();++i){const auto old_range=left.incidence_range(left.upper_index[i]);
        const auto next=right.incidence_range(m.cells[1][left.upper_index[i]]);m.occurrences[1].push_back(next.first+i-old_range.first);}
    return m;
}
OwnedRelationalInputUseAsset held_asset(const OwnedRelationalInputUseAsset& original,const core::RelationalComplex& held,const algebra::RetainedSourceMap& map){
    storage::TemporalEndpoint endpoint(held,20);storage::NativeTemporalSeries history(endpoint,9902);++endpoint.sequence;history.append_target(endpoint);++endpoint.sequence;history.append_target(endpoint);
    auto d=original.declaration();d.key=800;d.channel=900;d.bytes.clear();for(const auto c:std::string("oak / cedar"))d.bytes.push_back(static_cast<std::byte>(c));
    const auto source=original.publication();const storage::StableSourceIndex old(source.endpoint().source);
    for(auto& u:d.uses){u.key=u.key==111?311:312;u.spans={u.key==311?RelationalByteSpan{0,3}:RelationalByteSpan{6,11}};
        for(auto& c:u.cells){const auto cell=*old.cell(c);c=storage::stable_cell_key(held,{cell.grade,map.cells[cell.grade.value][cell.index]});}
        for(auto& occurrence:u.occurrences){const auto slot=*old.occurrence(occurrence);occurrence={occurrence.upper_grade,held.occurrence_identity(occurrence.upper_grade,map.occurrences[occurrence.upper_grade.value][slot])};}}
    return {history,d};
}
using Operator=PreparedRelationalFiniteFieldOperator;
Operator make_operator(bool asymmetric=false){
    const auto reference=finite_map(false);auto declaration=reference.declaration();
    declaration.rules.push_back({6405,{6201,6202},{101,102},0,declaration.rules[0].inputs,{}});
    const auto reference_source=reference.source();auto schema=reference_source.declaration();if(asymmetric){schema.charts[0].allowed={{101,103},{102,101}};schema.charts[1].allowed={{103,102},{101,103}};}
    const OwnedRelationalFiniteSource finite(reference.source().asset(),schema);
    const PreparedRelationalFiniteFieldMap parent(finite,reference.targets(),declaration);
    return {parent,{8001,1,{{9003,6403,0},{9004,6405,0},{9001,6401,0},{9002,6402,0}}}};
}
struct Independent {
    OwnedRelationalFiniteSource source;
    RelationalFiniteOperatorInstanceDeclaration declaration;
    std::vector<RelationalFiniteOperatorInput> inputs;
};
Independent supplied_instance(const Operator& op,const V& x,std::uint64_t key=8101,bool bad_inactive_flag=false,bool bad_scale=false){
    const auto ref=op.reference();const auto original=ref.source().asset();const auto endpoint=original.publication();const auto complex=target(bad_inactive_flag,bad_scale);
    const auto map=correspondence(endpoint.endpoint().source,complex);const auto asset=held_asset(original,complex,map);
    const RelationalFiniteSourceTransportDeclaration transport{8201,1,map,{{111,311},{112,312}}};
    const auto schema=relational_finite_operator_source_declaration(op,asset,transport);const OwnedRelationalFiniteSource source(asset,schema);
    std::vector<RelationalFiniteOperatorInput> inputs;
    for(const auto& port:op.declaration().ports){const auto d=relational_finite_operator_input_schema(op,asset,transport,port.key);inputs.push_back({port.key,{asset,d,port.rule==6403?312u:311u,tensor({x[2],x[3],x[0],x[1]})}});}
    std::reverse(inputs.begin(),inputs.end());return {source,{key,1,transport},inputs};
}
RelationalFiniteOperatorInstance instantiate(const Operator& op,const V& x,std::uint64_t key=8101){const auto data=supplied_instance(op,x,key);return op.instantiate(data.source,data.declaration,data.inputs);}
std::vector<std::byte> bytes(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("missing packet");const std::string raw{std::istreambuf_iterator<char>(f),{}};
    std::vector<std::byte> out;for(const auto c:raw)out.push_back(static_cast<std::byte>(c));return out;
}
void bytes(const std::filesystem::path& path,const std::vector<std::byte>& image){
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(image.data()),static_cast<std::streamsize>(image.size()));if(!f)throw std::runtime_error("packet write");
}
std::vector<std::byte> false_witness(const std::vector<std::byte>& image,bool choice=false){
    using namespace finite_operator_codec_detail;const storage::TemporalReadLimits read;Reader r(image,read);
    const auto magic=choice?completion_magic:family_magic;header(r,image,magic);auto w=envelope(magic);blob(w,r.blob());
    if(choice){w.u64(r.u64());relational_affine_codec_detail::vector(w,relational_affine_codec_detail::vector(r));}
    const auto original=r.blob();std::vector<std::byte> witness(original.begin(),original.end());witness[0]=std::byte{0};blob(w,witness);return finish(std::move(w));
}
void run(bool emit,const std::filesystem::path& directory={}){
    const auto op=make_operator();const auto reference=op.reference();const auto original=reference.source().asset();
    const auto initial=encode_relational_finite_field_map(reference);const auto ob=encode_relational_finite_operator(op);const storage::TemporalReadLimits read;
    check(op.select_ports(1,2)[0].key==9004,"all original ports including unmatched rule remain named and ordered");
    check(encode_relational_finite_operator(decode_relational_finite_operator(ob,read))==ob,"operator canonical cold replay");
    auto bad_ports=op.declaration();bad_ports.ports.pop_back();rejects([&]{(void)Operator(reference,bad_ports);},"omitted complete port rejects");
    bad_ports=op.declaration();bad_ports.ports[0].key=bad_ports.ports[1].key;rejects([&]{(void)Operator(reference,bad_ports);},"duplicate named port rejects");
    bad_ports=op.declaration();bad_ports.ports[0].rule=6404;rejects([&]{(void)Operator(reference,bad_ports);},"unsupported rule cannot invent a numeric port");
    const auto data=supplied_instance(op,{2,3,5,7});const auto instance=op.instantiate(data.source,data.declaration,data.inputs);const auto family=instance.execute();const auto raw=family.result();
    check(instance.operator_program().same_owner(op)&&family.instance().same_owner(instance),"actual operator/instance owners retained");
    check(instance.inputs()[0].port==9003&&instance.inputs()[3].port==9002,"native bulk input matching freezes actual declared port order");
    check(family.size()==4&&raw.branch(3).status==RelationalFiniteFieldStatus::Unsupported,"complete compatible alternatives and explicit unknown interpretation retained");
    const auto transport=instance.source_transport();const auto positions=transport.variable_positions(0,3);const auto uses=transport.use_correspondences(0,2);
    check(positions[0].reference==0&&positions[0].input==2&&positions[1].input==0&&positions[2].input==1,"native correspondence exposes original and input variable coordinates");
    check(uses[0].reference==111&&uses[0].input==311&&uses[1].input==312,"native correspondence keeps distinct original uses");
    const auto actual=instance.source().asset();check(actual.publication().source_namespace()==9902&&actual.publication().sequence()==22&&original.recover(111,20)!=actual.recover(311,20),"independent literal bytes/spans/source/cutoff are retained");
    const auto selected=family.evaluate(1,{3,5,7});const auto evaluation=selected.result();const auto output=evaluation.result();const auto fields=output.result();const auto program=fields.program();
    check(fields.field(2).values().dense()==response({2,3,5,7},{2,-1,3}),"whole independently supplied field changes actual correlated learned response");
    check(output.inputs()[0].use()==312&&output.inputs()[0].accession().asset().same_owner(actual),"held input uses the one actual independent asset");
    check(program.declaration().occurrences[3].map.get(0,0)==12&&program.declaration().occurrences[4].map.get(1,1)==8,"inactive and supported-zero learned maps stay in the complete field state");
    const auto ib=encode_relational_finite_operator_instance(instance);const auto fb=encode_relational_finite_operator_result(family);const auto cb=encode_relational_finite_operator_evaluation(selected);
    check(encode_relational_finite_operator_instance(decode_relational_finite_operator_instance(ib,read))==ib,"instance canonical full operator/input replay");
    check(encode_relational_finite_operator_result(decode_relational_finite_operator_result(fb,read))==fb,"whole result canonical full ancestry replay");
    const auto cold=decode_relational_finite_operator_evaluation(cb,read);check(encode_relational_finite_operator_evaluation(cold)==cb&&cold.family().size()==4,"completion cold replay retains operator and all alternatives");
    const auto cold_instance=cold.family().instance();const auto cold_source=cold_instance.source();const auto cold_asset=cold_source.asset();
    for(const auto& row:cold_instance.inputs())check(row.input.asset.same_owner(cold_asset),"cold all independent rows adopt one actual asset");
    const auto cold_evaluation=cold.result();const auto cold_value=cold_evaluation.result();check(cold_value.inputs()[0].accession().asset().same_owner(cold_asset),"cold selected field source matches complete independent instance owner");
    const auto fixed=family.evaluate(0,{});const auto equal=family.evaluate(2,{});const auto fixed_native=fixed.result();const auto equal_native=equal.result();
    check(encode_accession_field_result(fixed_native.result())==encode_accession_field_result(equal_native.result())&&encode_relational_finite_operator_evaluation(fixed)!=encode_relational_finite_operator_evaluation(equal),"equal numeric fields retain distinct finite branch identity");
    const auto another=instantiate(op,{2,3,5,7},8102).execute().evaluate(1,{3,5,7});const auto another_raw=another.result();
    check(encode_accession_field_result(another_raw.result())==encode_accession_field_result(output)&&encode_relational_finite_operator_evaluation(another)!=cb,"equal fields retain separate operator input-instance identity");
    rejects([&]{(void)family.evaluate(3,{});},"unknown branch cannot become fabricated zero field");
    rejects([&]{(void)family.evaluate(1,{3,5});},"whole affine completion needs every correlated free axis");
    rejects([&]{(void)family.evaluate(0,{1});},"fixed field cannot invent affine directions");
    rejects([&]{(void)decode_relational_finite_operator_result(false_witness(fb),read);},"claimed family witness is compared to original instance execution");
    rejects([&]{(void)decode_relational_finite_operator_evaluation(false_witness(cb,true),read);},"claimed selected witness is not a second execution source");
    auto rows=data.inputs;rows.pop_back();rejects([&]{(void)op.instantiate(data.source,data.declaration,rows);},"independent input missing complete port");
    rows=data.inputs;rows[0].port=rows[1].port;rejects([&]{(void)op.instantiate(data.source,data.declaration,rows);},"independent input duplicates port");
    rows=data.inputs;rows[0].input.use=312;rejects([&]{(void)op.instantiate(data.source,data.declaration,rows);},"wrong original-use correspondence");
    rows=data.inputs;rows[0].input.declaration.reconstruction=M::identity(4);rejects([&]{(void)op.instantiate(data.source,data.declaration,rows);},"undeclared identity reconstruction on nonidentity source transport");
    rows=data.inputs;rows[0].input.declaration.source_stalks[0].basis=999;rejects([&]{(void)op.instantiate(data.source,data.declaration,rows);},"equal dimensions cannot replace intrinsic basis identity");
    rows=data.inputs;rows[0].input.source_field=tensor({1,2,3});rejects([&]{(void)op.instantiate(data.source,data.declaration,rows);},"input whole tensor has declared carrier");
    rows=data.inputs;rows[0].input.asset=decode_relational_input_use_asset(encode_relational_input_use_asset(actual),read);rejects([&]{(void)op.instantiate(data.source,data.declaration,rows);},"equal asset bytes do not replace actual source owner");
    auto bad=data.declaration;bad.transport.source_map.occurrences[1].pop_back();rejects([&]{(void)op.instantiate(data.source,bad,data.inputs);},"missing inactive flag routing rejects");
    bad=data.declaration;std::swap(bad.transport.source_map.occurrences[1][0],bad.transport.source_map.occurrences[1][2]);rejects([&]{(void)op.instantiate(data.source,bad,data.inputs);},"aggregated incidence cannot replace original flag order");
    bad=data.declaration;bad.transport.uses[0].input=312;rejects([&]{(void)op.instantiate(data.source,bad,data.inputs);},"source-use bijection is actual and explicit");
    const auto inactive=supplied_instance(op,{2,3,5,7},8101,true);rejects([&]{(void)op.instantiate(inactive.source,inactive.declaration,inactive.inputs);},"zero realized coefficient cannot erase original inactive orientation change");
    const auto scale=supplied_instance(op,{2,3,5,7},8101,false,true);rejects([&]{(void)op.instantiate(scale.source,scale.declaration,scale.inputs);},"chain grade does not identify a changed semantic scale");
    auto changed=data.source.declaration();changed.symbols[0].definition.push_back(std::byte{'x'});const OwnedRelationalFiniteSource foreign_schema(actual,changed);
    rejects([&]{(void)op.instantiate(foreign_schema,data.declaration,data.inputs);},"instantiation cannot alter original opaque symbols");
    rejects([&]{(void)transport.variable_positions(3,1);},"bounded variable correspondence getter");
    rejects([&]{(void)op.select_ports(4,1);},"bounded original port getter");
    numeric::ExactActionLimits tiny;tiny.entries=1;rejects([&]{(void)op.instantiate(data.source,data.declaration,data.inputs,tiny);},"complete instance refuses logical resource prefix");
    rejects([&]{(void)op.instantiate(data.source,data.declaration,data.inputs,{},[]{throw std::runtime_error("cancel");});},"cancelled instance not published");
    const auto reference_finite=reference.source();auto large_schema=reference_finite.declaration();large_schema.symbols[0].definition.resize(1024,std::byte{'a'});
    const OwnedRelationalFiniteSource large_source(original,large_schema);const PreparedRelationalFiniteFieldMap large_reference(large_source,reference.targets(),reference.declaration());const Operator large_op(large_reference,{8701,1,op.declaration().ports});
    numeric::ExactActionLimits declaration_cap;declaration_cap.entries=512;
    rejects([&]{(void)relational_finite_operator_source_declaration(large_op,actual,data.declaration.transport,declaration_cap);},"passive source precursor charges complete original opaque definition bytes");
    numeric::ExactActionLimits variable_cap;variable_cap.dimension=1;
    rejects([&]{(void)relational_finite_operator_source_declaration(op,actual,data.declaration.transport,variable_cap);},"passive source precursor enforces actual variable/symbol dimension cap");
    auto buffer=cb;bool cleared{};const auto frozen=decode_relational_finite_operator_evaluation(buffer,read,{},[&]{if(!cleared){buffer.clear();cleared=true;}});
    check(cleared&&encode_relational_finite_operator_evaluation(frozen)==cb,"cold entire packet freezes before callback");
    auto mutable_rows=data.inputs;const auto copied=op.instantiate(data.source,data.declaration,mutable_rows,{},[&]{mutable_rows.clear();});
    check(copied.inputs().size()==4&&mutable_rows.empty(),"online entire submission freezes before callback");
    const auto escaped=[] {const auto owner=make_operator();return instantiate(owner,{2,3,5,7}).execute().select(1,1).front();}();
    check(escaped.parent().size()==4&&escaped.branch().rule==6403,"escaped branch retains complete operator/instance/reference and family");
    const auto asymmetric=make_operator(true);const auto asymmetric_data=supplied_instance(asymmetric,{2,3,5,7});const auto moved=asymmetric.instantiate(asymmetric_data.source,asymmetric_data.declaration,asymmetric_data.inputs);
    const auto old_source=asymmetric.reference().source();const auto original_sections=old_source.sections();const auto new_source=moved.source();const auto new_sections=new_source.sections();
    check(original_sections.size()==2&&new_sections.size()==2&&original_sections.assignment(0)==std::vector<std::uint64_t>({101,103,102})&&new_sections.assignment(0)==std::vector<std::uint64_t>({101,103,102}),"asymmetric alternative families remain canonically ordered by their actual source");
    const auto mapping=moved.source_transport().variable_positions(0,3);check(new_sections.assignment(1)[mapping[0].input]==original_sections.assignment(0)[0]&&new_sections.assignment(0)[mapping[0].input]==original_sections.assignment(1)[0],"explicit native variable correspondence relates reversed original section ordinals");
    const auto unmapped=moved.execute();const auto unmapped_raw=unmapped.result();check(unmapped.size()==2&&unmapped_raw.branch(0).status==RelationalFiniteFieldStatus::Unmapped,"unmapped alternative has no fabricated response");
    rejects([&]{(void)unmapped.evaluate(0,{});},"unmapped alternative cannot be decoded as zero");
    auto lower_declaration=base_declaration();lower_declaration.nodes={{11,FieldProgramNodeKind::Input,core::Grade{0},{},{}},{12,FieldProgramNodeKind::DeclaredMap,core::Grade{0},11,M::identity(7)}};
    const Program lower_program(source(),lower_declaration);RelationalInputAccessionDeclaration lower;lower.key=902;lower.node=11;lower.source_grade={0};lower.instances={{ {0,1,2},{0,1} }, { {},{0,1,2,3,4,5,6,7} }};lower.accession=M::identity(7);lower.reconstruction=M::identity(7);lower.isometric=true;
    for(const auto& stalk:lower_program.declaration().stalks)if(!stalk.cell.grade.value)lower.source_stalks.push_back(stalk);
    const OwnedRelationalInputAccession lower_accession(original,lower_program,lower);
    const PreparedRelationalFiniteFieldMap lower_reference(reference.source(),{lower_program},{8501,1,{{8511,{6201},{101},0,{lower_accession.admit(111,tensor({1,2,3,4,5,6,7}))},{}},{8512,{6201},{103},{},{},"unknown lower-field interpretation"}}});
    const Operator lower_op(lower_reference,{8502,1,{{9501,8511,0}}});const auto lower_schema=relational_finite_operator_input_schema(lower_op,actual,data.declaration.transport,9501);
    const auto lower_instance=lower_op.instantiate(data.source,{8503,1,data.declaration.transport},{{9501,{actual,lower_schema,311,tensor({3,4,5,6,7,1,2})}}});
    const auto lower_family=lower_instance.execute();const auto lower_choice=lower_family.evaluate(0,{});const auto lower_raw=lower_choice.result();const auto lower_value=lower_raw.result();const auto lower_fields=lower_value.result();
    check(lower_fields.field(12).values().dense()==V({1,2,3,4,5,6,7})&&lower_schema.source_stalks[0].directions.size()==3&&lower_schema.source_stalks[2].directions.size()==2,"actual native port transport preserves heterogeneous2/3/2 stalk blocks and intrinsic metrics");
    auto metric_declaration=lower_declaration;numeric::Integer large_integer=1;large_integer<<=100;metric_declaration.stalks[0].metric=M::from_rows({{Q{large_integer},0},{0,1}});
    const Program metric_program(source(),metric_declaration);auto metric_interface=lower;metric_interface.source_stalks.clear();for(const auto& stalk:metric_program.declaration().stalks)if(!stalk.cell.grade.value)metric_interface.source_stalks.push_back(stalk);
    const OwnedRelationalInputAccession metric_accession(original,metric_program,metric_interface);
    const PreparedRelationalFiniteFieldMap metric_reference(reference.source(),{metric_program},{8801,1,{{8811,{6201},{101},0,{metric_accession.admit(111,tensor({1,2,3,4,5,6,7}))},{}}}});
    const Operator metric_op(metric_reference,{8802,1,{{9801,8811,0}}});numeric::ExactActionLimits metric_cap;metric_cap.integer_bits=16;
    rejects([&]{(void)relational_finite_operator_input_schema(metric_op,actual,data.declaration.transport,9801,metric_cap);},"passive port precursor enforces complete intrinsic metric integer policy");
    const auto base_program=std::get<Program>(reference.targets()[0]);auto params=parameters();params.parameters.resize(3);M basis(12,3);for(std::size_t i=0;i<12;++i)for(std::size_t j=0;j<3;++j)basis.set(i,j,params.basis.get(i,j));params.basis=basis;
    const Evidence identified_root(base_program,params);const auto identified=identified_root.append({observation(base_program,original,{1,2,0,0},1),observation(base_program,original,{0,1,0,0},2)});
    check(identified.kernel().cols()==0,"identified complete affine target has empty common xi");
    auto targets=reference.targets();targets[1]=identified;const PreparedRelationalFiniteFieldMap identified_map(reference.source(),targets,reference.declaration());
    const Operator identified_op(identified_map,{8601,1,op.declaration().ports});const auto identified_instance=instantiate(identified_op,{2,3,5,7},8602);const auto identified_family=identified_instance.execute();const auto identified_choice=identified_family.evaluate(1,{});
    const auto identified_bytes=encode_relational_finite_operator_evaluation(identified_choice);check(encode_relational_finite_operator_evaluation(decode_relational_finite_operator_evaluation(identified_bytes,read))==identified_bytes,"empty affine xi retains full operator ancestry and canonical raw choice");
    std::atomic<bool> okay{true};std::vector<std::thread> workers;
    for(unsigned i=0;i<6;++i)workers.emplace_back([&]{try{const auto restored=decode_relational_finite_operator_evaluation(cb,read);if(encode_relational_finite_operator_evaluation(restored)!=cb)okay=false;}catch(...){okay=false;}});
    for(auto& worker:workers){worker.join();}check(okay,"six concurrent immutable owning cold readers");
    if(!directory.empty()){std::filesystem::create_directories(directory);bytes(directory/"family.packet",fb);bytes(directory/"choice.packet",cb);}
    for(std::size_t trial=0;trial<12;++trial){
        V x{Q{static_cast<long>(trial)+1,3},Q{static_cast<long>(trial)-2,5},Q{2*static_cast<long>(trial)+1,7},Q{3-static_cast<long>(trial),2}};for(auto& q:x)q.canonicalize();
        const V xi{Q{static_cast<long>(trial)-2},Q{static_cast<long>(trial)+3},Q{2*static_cast<long>(trial)}};
        const auto current=instantiate(op,x,8300+trial);const auto all=current.execute();const auto value=all.evaluate(1,xi);const auto chosen=value.result();const auto owned=chosen.result();const auto whole=owned.result();const auto all_raw=all.result();
        const auto affine=std::get<RelationalAffineProgramResult>(*all_raw.branch(1).value);
        check(whole.field(2).values().dense()==response(x,{-10+4*xi[0],5-2*xi[0],xi[0]})&&whole.occurrence_field(2).domain().logical_size()==19,"complete independently held responses follow coupled native maps");
        check(current.operator_program().same_owner(op)&&encode_relational_finite_field_map(reference)==initial,"reusable operator never changes original reference fields/source/evidence");
        if(emit){std::cout<<"{\"xi\":";json(xi);std::cout<<",\"input\":";json(x);std::cout<<",\"particular\":";json(affine.field(2).particular.values().dense());std::cout<<",\"directions\":";json(affine.field(2).directions);
            std::cout<<",\"response\":";json(whole.field(2).values().dense());std::cout<<",\"next\":";json(whole.field(3).values().dense());std::cout<<",\"occurrences\":";json(whole.occurrence_field(2).values().dense());std::cout<<"}\n";}
    }
}
} // namespace
int main(int argc,char** argv){try{
    if(argc==3&&(std::string(argv[1])=="--read-family"||std::string(argv[1])=="--read-choice")){
        const storage::TemporalReadLimits read;const auto packet=bytes(argv[2]);
        if(std::string(argv[1])=="--read-family"){const auto family=decode_relational_finite_operator_result(packet,read);check(family.size()==4&&encode_relational_finite_operator_result(family)==packet,"standalone whole family canonical");}
        else{const auto choice=decode_relational_finite_operator_evaluation(packet,read);const auto raw=choice.result();const auto owned=raw.result();const auto fields=owned.result();check(choice.family().size()==4&&owned.inputs()[0].use()==312&&fields.field(2).values().dense()==response({2,3,5,7},{2,-1,3}),"standalone larger ancestry original use and complete response");check(encode_relational_finite_operator_evaluation(choice)==packet,"standalone choice canonical");}
        std::cout<<checks<<" standalone operator checks passed\n";return 0;
    }
    if(argc==3&&std::string(argv[1])=="--write")run(false,argv[2]);else run(argc==2&&std::string(argv[1])=="--emit");
    if(!(argc==2&&std::string(argv[1])=="--emit")){std::cout<<checks<<" finite operator checks passed\n";}return 0;
}catch(const std::exception& e){std::cerr<<"finite operator failure: "<<e.what()<<'\n';return 1;}}
