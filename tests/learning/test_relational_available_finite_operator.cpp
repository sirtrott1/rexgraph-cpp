#include "rfc/learning/relational_available_finite_operator_codec.hpp"
#include <atomic>
#include <thread>
// Reuse the qualified whole-field/evidence/publication fixtures and independent response.
#pragma push_macro("main")
#undef main
#define main rfc_available_finite_field_fixture_main
#include "test_relational_available_finite_field.cpp"
#undef main
#pragma pop_macro("main")
namespace {
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
    for(const auto key:a.grade(core::Grade{0}).identities){const auto mapped=key==40?60u:key==50?40u:50u;const auto& ids=b.grade(core::Grade{0}).identities;
        m.cells[0].push_back(static_cast<std::uint32_t>(std::find(ids.begin(),ids.end(),mapped)-ids.begin()));}
    const auto& left=a.grade(core::Grade{1});const auto& right=b.grade(core::Grade{1});
    for(std::size_t i=0;i<left.cell_count();++i){const auto mapped=core::RelationKey{left.relation_identities[i]==core::RelationKey{200}?201u:200u};
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

using Operator=PreparedAvailableRelationalFiniteFieldOperator;
using Instance=AvailableRelationalFiniteOperatorInstance;
using Family=AvailableRelationalFiniteOperatorResult;
struct OperatorFixture {Operator op;Available evidence;std::vector<Grant> grants;};
OperatorFixture operator_fixture(bool identified=false,bool prospective=false) {
    auto f=fixture(identified,prospective);const auto status=prospective?Status::Prospective:Status::Observed;
    const RelationalFiniteOperatorDeclaration d{8001,1,{{9003,6403,0},{9004,6405,0},{9001,6401,0},{9002,6402,0}}};
    const auto source=f.model.source().finite();const auto a=source.asset();const auto payload=relational_finite_operator_payload(f.model,d,status,Context(f.grants));
    grant(f.grants,1000,Role::Maps,payload,publication(a));grant(f.grants,1001,prospective?Role::Prospective:Role::Evidence,payload,publication(a,true));
    return {Operator(f.model,d,status,{1000,1001},Context(f.grants)),f.evidence,f.grants};
}
struct InstanceData {FiniteSource source;RelationalFiniteOperatorInstanceDeclaration declaration;
    std::vector<RelationalAvailableFiniteOperatorInput> rows;RelationalFiniteOperatorInstanceOrigins origins;};
InstanceData prepare(const OperatorFixture& f,const V& x,std::vector<Grant>& grants,std::uint64_t first=1100,std::uint64_t key=8101,
    bool bad_interface=false,bool bad_geometry=false) {
    const auto reference=f.op.reference();const auto finite=reference.source().finite();const auto original=finite.asset();const auto endpoint=original.publication();
    const auto complex=target(bad_geometry);const auto map=correspondence(endpoint.endpoint().source,complex);const auto a=held_asset(original,complex,map);
    const RelationalFiniteSourceTransportDeclaration transport{8201,1,map,{{111,311},{112,312}}};const RelationalFiniteOperatorInstanceDeclaration d{key,1,transport};
    const auto status=f.op.status();const auto role=available_finite_detail::relationship_role(status);const auto ab=encode_relational_input_use_asset(a);
    const auto schema=relational_finite_operator_source_declaration(f.op,a,transport);
    grant(grants,first,Role::InputAsset,ab,publication(a));grant(grants,first+1,role,relational_finite_constraint_payload(ab,schema,status),publication(a,true));
    const FiniteSource source(a,schema,status,{first,first+1},Context(grants));std::vector<RelationalAvailableFiniteOperatorInput> rows;
    for(std::size_t i=0;i<f.op.declaration().ports.size();++i) {
        const auto& port=f.op.declaration().ports[i];auto input_schema=relational_finite_operator_input_schema(f.op,a,transport,port.key);
        if(bad_interface&&i==0)input_schema.reconstruction=M(4,4);
        const auto raw_op=f.op.operator_program();const auto old=finite_operator_detail::original_input(raw_op,port.key);const auto accession=old.accession();
        const auto pb=encode_relational_field_program(accession.program());const auto use=port.rule==6403?312u:311u;
        const RelationalInputSubmission input{a,input_schema,use,tensor({x[2],x[3],x[0],x[1]})};const auto base=first+10+3*i;
        const auto vp=relational_input_value_payload(ab,input.declaration,use,input.source_field);const auto ap=relational_input_accession_payload(pb,ab,input.declaration,use,input.source_field);
        grant(grants,base,Role::InputValue,vp,publication(a,true));grant(grants,base+1,Role::InputAccession,ap,publication(a));grant(grants,base+2,Role::Reconstruction,ap,publication(a));
        rows.push_back({port.key,{input,{first,base,base+1,base+2}}});
    }
    std::reverse(rows.begin(),rows.end());const RelationalFiniteOperatorInstanceOrigins keys{first+40,first+41,first+42,first+43,first+44};
    const auto tp=relational_finite_operator_transport_payload(f.op,source,transport,status,Context(grants));
    grant(grants,keys.transport_maps,Role::Maps,tp,publication(a));grant(grants,keys.transport_relationship,role,tp,publication(a,true));
    const auto cp=relational_finite_operator_child_map_payload(f.op,source,d,rows,status,Context(grants));
    grant(grants,keys.maps,Role::Maps,cp,publication(a));grant(grants,keys.relationship,role,cp,publication(a,true));
    const auto ip=relational_finite_operator_instance_payload(f.op,source,d,rows,status,keys,Context(grants));
    // Passive publications may be prepared even with invalid A/R, but no result exists without the separate origin.
    rejects_origin([&]{(void)f.op.instantiate(source,d,rows,status,keys,Context(grants));},"instance lineage is required before transport or interface work");
    grant(grants,keys.instance,Role::Prospective,ip,publication(a,true));return {source,d,rows,keys};
}
Instance instantiate(const OperatorFixture& f,const InstanceData& d,const Context& context,numeric::ExactActionLimits limits={},std::function<void()> callback={}) {
    return f.op.instantiate(d.source,d.declaration,d.rows,f.op.status(),d.origins,context,limits,callback);
}
AvailableRelationalFiniteOperatorEvaluation operator_choice(const OperatorFixture& f,const Family& family,std::vector<Grant>& grants,
    std::size_t ordinal,const V& xi,std::uint64_t key=1501,std::uint64_t parameter=1500) {
    const auto finite=family.instance().source().finite();const auto a=finite.asset();if(ordinal!=1)parameter=0;
    if(parameter&&std::none_of(grants.begin(),grants.end(),[&](const auto& g){return g.origin.key()==parameter;}))
        grant(grants,parameter,Role::Prospective,relational_parameter_setting_payload(f.evidence,xi),publication(a,true));
    const auto payload=relational_finite_operator_completion_payload(family,ordinal,xi,parameter,Context(grants));
    grant(grants,key,Role::Prospective,payload,publication(a,true));return family.evaluate(ordinal,xi,parameter,key,Context(grants));
}
Bytes false_operator_witness(const Bytes& bytes,bool evaluation=false) {
    using namespace available_finite_operator_detail;const storage::TemporalReadLimits read;Reader r(bytes,read);
    const auto magic=evaluation?evaluation_magic:family_magic;header(r,bytes,magic);auto w=envelope(magic);blob(w,r.blob());
    if(evaluation) {
        w.u64(r.u64());relational_affine_codec_detail::vector(w,relational_affine_codec_detail::vector(r));w.u64(r.u64());const auto has=r.flag();w.u8(has?1:0);
        if(has){blob(w,r.blob());}w.u64(r.u64());blob(w,r.blob());
    }
    const auto claimed=r.blob();Bytes native(claimed.begin(),claimed.end());native[0]=std::byte{0};blob(w,native);return finish(std::move(w));
}
void run_operator(bool emit,const std::filesystem::path& exports={}) {
    const auto f=operator_fixture();auto grants=f.grants;const auto d=prepare(f,{2,3,5,7},grants);const Context context(grants);const storage::TemporalReadLimits read;
    const auto reference=f.op.reference();const auto initial=encode_available_relational_finite_field_map(reference);const auto instance=instantiate(f,d,context);const auto family=instance.execute(context);
    const auto raw=family.result().result();const auto selected=family.select(0,4,context);
    check(f.op.select_ports(0,4,context).size()==4&&instance.inputs()[0].port==9003&&instance.inputs()[1].port==9004,"native port matching retains unmatched input and declared order");
    check(instance.operator_program().same_owner(f.op)&&family.instance().same_owner(instance)&&selected[1].parent().size()==4,"owning operator instance family and escaped branch retain complete parents");
    check(raw.branch(3).status==RelationalFiniteFieldStatus::Unsupported&&!raw.branch(3).value,"unknown alternative retains absence of a continuous field");
    const auto positions=instance.variable_positions(0,3,context);const auto uses=instance.use_correspondences(0,2,context);
    check(positions[0].input==2&&positions[1].input==0&&positions[2].input==1&&uses[0].input==311&&uses[1].input==312,"actual variable and use correspondence retained independently of rank/value");
    const auto original_finite=reference.source().finite();const auto original=original_finite.asset();const auto actual_finite=instance.source().finite();const auto actual=actual_finite.asset();
    check(original.recover(111,20)!=actual.recover(311,20)&&actual.publication().source_namespace()==9902&&actual.publication().sequence()==22,"original bytes/spans/uses/history/cut differ and remain owned");
    const auto ob=encode_available_relational_finite_operator(f.op);const auto ib=encode_available_relational_finite_operator_instance(instance);const auto fb=encode_available_relational_finite_operator_result(family);
    check(encode_available_relational_finite_operator(decode_available_relational_finite_operator(ob,context,read))==ob,"operator canonical cold replay");
    check(encode_available_relational_finite_operator_instance(decode_available_relational_finite_operator_instance(ib,context,read))==ib,"instance canonical one-asset full-parent replay");
    check(encode_available_relational_finite_operator_result(decode_available_relational_finite_operator_result(fb,context,read))==fb,"whole family canonical replay");
    std::set<std::uint64_t> dependencies;for(const auto& origin:instance.obligations())dependencies.insert(origin.key());
    for(const auto key:dependencies) {const auto absent=withheld(grants,key);
        rejects_origin([&]{(void)instantiate(f,d,absent);},"every dependency checked before constructing available instance");
        rejects_origin([&]{(void)instance.execute(absent);},"every dependency rechecked before whole execution");
        rejects_origin([&]{(void)family.select(0,1,absent);},"every dependency rechecked before selection");
        rejects_origin([&]{(void)decode_available_relational_finite_operator_instance(ib,absent,read);},"cold full instance closure precedes first nested replay");
        rejects_origin([&]{(void)decode_available_relational_finite_operator_result(fb,absent,read);},"cold family cannot narrow parent authority");
    }
    const auto bad_packet=false_operator_witness(fb);
    rejects_origin([&]{(void)decode_available_relational_finite_operator_result(bad_packet,withheld(grants,d.origins.instance),read);},"instance receipt precedes malformed result witness replay");
    rejects_origin([&]{(void)decode_available_relational_finite_operator_result(bad_packet,withheld(grants,82),read);},"unused target receipt precedes malformed native replay");
    rejects([&]{(void)decode_available_relational_finite_operator_result(bad_packet,context,read);},"authorized facts reject forged native family witness");
    auto malformed_grants=f.grants;const auto invalid=prepare(f,{2,3,5,7},malformed_grants,1100,8101,true);
    rejects_origin([&]{(void)instantiate(f,invalid,withheld(malformed_grants,1113));},"late unmatched input authority precedes first invalid interface");
    rejects_origin([&]{(void)instantiate(f,invalid,withheld(malformed_grants,invalid.origins.instance));},"instance lineage authority precedes invalid interface");
    bool math_error{};try{(void)instantiate(f,invalid,Context(malformed_grants));}catch(const std::invalid_argument& e){math_error=std::string(e.what()).find("transported interface")!=std::string::npos;}
    check(math_error,"fully admitted invalid interface reaches actual native operator validation");
    auto geometry_grants=f.grants;const auto geometry=prepare(f,{2,3,5,7},geometry_grants,1100,8101,false,true);
    rejects_origin([&]{(void)instantiate(f,geometry,withheld(geometry_grants,geometry.origins.instance));},"lineage preflight precedes false inactive occurrence transport");
    rejects([&]{(void)instantiate(f,geometry,Context(geometry_grants));},"complete authority cannot authorize invalid source flags");
    auto rows=d.rows;rows.pop_back();rejects([&]{(void)f.op.instantiate(d.source,d.declaration,rows,Status::Observed,d.origins,context);},"omitted port rejects");
    rows=d.rows;rows[0].port=rows[1].port;rejects([&]{(void)f.op.instantiate(d.source,d.declaration,rows,Status::Observed,d.origins,context);},"duplicated port rejects");
    rows=d.rows;rows[0].input.input.asset=decode_relational_input_use_asset(encode_relational_input_use_asset(actual),read);
    rejects([&]{(void)f.op.instantiate(d.source,d.declaration,rows,Status::Observed,d.origins,context);},"equal bytes cannot supply foreign live input owner");
    auto foreign_grants=grants;const auto old=foreign_grants.front();foreign_grants.front().origin=Origin(old.origin.key(),old.origin.publication(),old.origin.role(),old.origin.payload());
    rejects_origin([&]{family.admit(Context(foreign_grants));},"equal receipt cannot replace actual live origin owner");
    const auto evaluation=operator_choice(f,family,grants,1,{3,5,7});const auto fixed=operator_choice(f,family,grants,0,{},1502);const Context choices(grants);
    const auto native=evaluation.result().result();const auto output=native.result();const auto fields=output.result();const auto program=fields.program();
    check(fields.field(2).values().dense()==response({2,3,5,7},{2,-1,3})&&output.inputs()[0].use()==312,"whole learned response retains actual independent source/use");
    check(program.declaration().occurrences[3].map.get(0,0)==12&&program.declaration().occurrences[4].map.get(1,1)==8,"inactive and supported-zero maps retain learned parameter state");
    check(evaluation.family().size()==4&&evaluation.parameter_setting()->key()==1500&&fixed.parameter_setting()==std::nullopt,"global setting and input-specific choice stay separate");
    const auto cb=encode_available_relational_finite_operator_evaluation(evaluation);const auto xb=encode_available_relational_finite_operator_evaluation(fixed);
    check(encode_available_relational_finite_operator_evaluation(decode_available_relational_finite_operator_evaluation(cb,choices,read))==cb,"affine completion canonical replay");
    check(encode_available_relational_finite_operator_evaluation(decode_available_relational_finite_operator_evaluation(xb,choices,read))==xb,"fixed completion canonical replay");
    const auto restored=decode_available_relational_finite_operator_evaluation(cb,choices,read);const auto restored_instance=restored.family().instance();const auto restored_finite=restored_instance.source().finite();const auto restored_asset=restored_finite.asset();
    for(const auto& row:restored_instance.inputs())check(row.input.input.asset.same_owner(restored_asset),"cold all submissions use ONE actual independently replayed asset");
    const auto restored_native=restored.result().result();const auto restored_output=restored_native.result();check(restored_output.inputs()[0].accession().asset().same_owner(restored_asset),"cold selected fields use the same actual instance source owner");
    const auto bad_choice=false_operator_witness(cb,true);
    for(const auto key:{1500u,1501u,1144u,1113u,82u}) {
        const auto absent=withheld(grants,key);rejects_origin([&]{evaluation.admit(absent);},"escaped completion retains whole operator closure and both choices");
        rejects_origin([&]{(void)decode_available_relational_finite_operator_evaluation(cb,absent,read);},"cold choice preflights global setting and full parent");
        rejects_origin([&]{(void)decode_available_relational_finite_operator_evaluation(bad_choice,absent,read);},"choice authority precedes forged evaluation witness replay");
    }
    rejects([&]{(void)decode_available_relational_finite_operator_evaluation(bad_choice,choices,read);},"authorized choice rejects forged native witness");
    rejects_origin([&]{(void)family.evaluate(2,{},0,1502,choices);},"equal numerical branch cannot reuse another operator choice");
    rejects([&]{(void)family.evaluate(3,{},0,1502,choices);},"unknown alternatives cannot become zero completions");
    rejects([&]{(void)family.evaluate(1,{3,5},1500,1501,choices);},"complete common correlated axes required");
    rejects([&]{(void)family.evaluate(0,{1},1500,1502,choices);},"fixed completion cannot carry an affine setting");
    const auto child_family=instance.field_map().execute(choices);const auto legacy=relational_finite_completion_payload(child_family,1,{3,5,7},1500,choices);
    auto legacy_grants=grants;grant(legacy_grants,1503,Role::Prospective,legacy,publication(actual,true));
    rejects_origin([&]{(void)family.evaluate(1,{3,5,7},1500,1503,Context(legacy_grants));},"smaller finite-family receipt cannot authorize larger operator completion");
    const auto another_data=prepare(f,{2,3,5,7},grants,2100,8102);const auto another=instantiate(f,another_data,Context(grants)).execute(Context(grants));
    rejects_origin([&]{(void)another.evaluate(1,{3,5,7},1500,1501,Context(grants));},"same numerical fields cannot reuse another input-instance choice");
    const auto another_choice=operator_choice(f,another,grants,1,{3,5,7},2501);const auto another_native=another_choice.result().result();
    check(encode_accession_field_result(another_native.result())==encode_accession_field_result(output)&&encode_available_relational_finite_operator_evaluation(another_choice)!=cb,"equal encodings retain separate input-instance ancestry");
    rejects_origin([&]{(void)instance.variable_positions(0,3,withheld(grants,1144));},"correspondence selection requires complete current instance authority");
    rejects_origin([&]{selected[0].admit(withheld(grants,1113));},"escaped branch retains unmatched port authority");
    rejects([&]{(void)family.select(4,1,Context(grants));},"bounded selection refuses outside family");
    numeric::ExactActionLimits tiny;tiny.entries=1;rejects([&]{(void)instance.variable_positions(0,3,Context(grants),tiny);},"bounded correspondence entry refusal");
    rejects([&]{(void)instantiate(f,d,Context(grants),{},[]{throw std::runtime_error("cancel");});},"cancellation publishes no partial instance");
    auto read_tiny=read;read_tiny.max_file_bytes=cb.size()-1;rejects([&]{(void)decode_available_relational_finite_operator_evaluation(cb,choices,read_tiny);},"cold byte resource refusal before replay");
    auto buffer=cb;auto mutable_context=choices;auto mutable_read=read;bool changed{};
    const auto frozen=decode_available_relational_finite_operator_evaluation(buffer,mutable_context,mutable_read,{},[&]{if(!changed){changed=true;buffer.clear();mutable_context=Context({});mutable_read.max_file_bytes=1;}});
    check(changed&&encode_available_relational_finite_operator_evaluation(frozen)==cb,"decoder freezes bytes context and policy before callbacks");
    auto mutable_rows=d.rows;auto mutable_declaration=d.declaration;mutable_context=choices;changed=false;
    const auto frozen_instance=f.op.instantiate(d.source,mutable_declaration,mutable_rows,Status::Observed,d.origins,mutable_context,{},[&]{if(!changed){changed=true;mutable_rows.clear();mutable_declaration.transport.uses.clear();mutable_context=Context({});}});
    check(changed&&encode_available_relational_finite_operator_instance(frozen_instance)==ib,"live constructor freezes rows declaration and context before callbacks");
    const auto identified=operator_fixture(true,true);auto identified_grants=identified.grants;const auto empty_data=prepare(identified,{2,3,5,7},identified_grants);const Context identified_context(identified_grants);
    const auto empty_family=instantiate(identified,empty_data,identified_context).execute(identified_context);
    rejects([&]{(void)empty_family.evaluate(1,{},0,0,identified_context);},"empty affine xi needs positive global setting");
    const auto empty=operator_choice(identified,empty_family,identified_grants,1,{});const auto eb=encode_available_relational_finite_operator_evaluation(empty);
    check(empty.parameter_setting().has_value()&&empty.family().instance().status()==Status::Prospective,"identified empty-xi prospective operator keeps global receipt");
    check(encode_available_relational_finite_operator_evaluation(decode_available_relational_finite_operator_evaluation(eb,Context(identified_grants),read))==eb,"empty-xi prospective canonical replay");
    std::atomic<bool> okay{true};std::vector<std::thread> readers;for(unsigned i=0;i<6;++i)readers.emplace_back([&]{try{const auto cold=decode_available_relational_finite_operator_evaluation(cb,choices,read);if(encode_available_relational_finite_operator_evaluation(cold)!=cb)okay=false;}catch(...){okay=false;}});
    for(auto& worker:readers){worker.join();}check(okay,"six concurrent immutable full operator cold readers");
    if(!exports.empty()) {std::filesystem::create_directories(exports);write_file(exports/"family.packet",fb);write_file(exports/"choice.packet",cb);write_file(exports/"authority.bin",authority_bytes(grants));}
    for(std::size_t trial=0;trial<12;++trial) {
        V x{Q{static_cast<long>(trial)+1,3},Q{static_cast<long>(trial)-2,5},Q{2*static_cast<long>(trial)+1,7},Q{3-static_cast<long>(trial),2}};for(auto& q:x)q.canonicalize();
        const V xi{Q{static_cast<long>(trial)-2},Q{static_cast<long>(trial)+3},Q{2*static_cast<long>(trial)}};auto current=f.grants;
        const auto data=prepare(f,x,current,3000+100*trial,8300+trial);const auto all=instantiate(f,data,Context(current)).execute(Context(current));
        const auto value=operator_choice(f,all,current,1,xi,5000+2*trial,5001+2*trial);const auto completed=value.result().result();const auto owned=completed.result();const auto whole=owned.result();const auto all_raw=all.result().result();
        const auto affine=std::get<RelationalAffineProgramResult>(*all_raw.branch(1).value);
        check(whole.field(2).values().dense()==response(x,{-10+4*xi[0],5-2*xi[0],xi[0]})&&whole.occurrence_field(2).domain().logical_size()==19,"independent admitted inputs change complete learned whole-field response");
        check(encode_available_relational_finite_field_map(reference)==initial,"operator reuse preserves original source inputs rules and evidence");
        if(emit){std::cout<<"{\"xi\":";json(xi);std::cout<<",\"input\":";json(x);std::cout<<",\"particular\":";json(affine.field(2).particular.values().dense());std::cout<<",\"directions\":";json(affine.field(2).directions);
            std::cout<<",\"response\":";json(whole.field(2).values().dense());std::cout<<",\"next\":";json(whole.field(3).values().dense());std::cout<<",\"occurrences\":";json(whole.occurrence_field(2).values().dense());std::cout<<"}\n";}
    }
}
} // namespace
int main(int argc,char** argv){try {
    if(argc==3&&(std::string(argv[1])=="--read-family"||std::string(argv[1])=="--read-choice")) {
        const auto path=std::filesystem::path(argv[2]);const auto context=authority(read_file(path.parent_path()/"authority.bin"));const auto packet=read_file(path);const storage::TemporalReadLimits read;
        if(std::string(argv[1])=="--read-family") {const auto family=decode_available_relational_finite_operator_result(packet,context,read);check(family.size()==4&&encode_available_relational_finite_operator_result(family)==packet,"standalone full family canonical");}
        else {const auto value=decode_available_relational_finite_operator_evaluation(packet,context,read);const auto raw=value.result().result();const auto owned=raw.result();const auto whole=owned.result();check(value.family().size()==4&&owned.inputs()[0].use()==312&&whole.field(2).values().dense()==response({2,3,5,7},{2,-1,3}),"standalone trusted-registry independent whole response and lineage");check(encode_available_relational_finite_operator_evaluation(value)==packet,"standalone choice canonical");}
        std::cout<<checks<<" standalone available operator checks passed\n";return 0;
    }
    if(argc==3&&std::string(argv[1])=="--write")run_operator(false,argv[2]);else run_operator(argc==2&&std::string(argv[1])=="--emit");
    if(!(argc==2&&std::string(argv[1])=="--emit")){std::cout<<checks<<" available operator checks passed\n";}return 0;
}catch(const std::exception& e){std::cerr<<"available operator failure: "<<e.what()<<'\n';return 1;}}
