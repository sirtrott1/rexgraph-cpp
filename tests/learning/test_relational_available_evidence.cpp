#include "rfc/learning/relational_available_evidence_codec.hpp"
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
Bytes grafted_model(const Bytes& bytes,bool parameters_changed){
    using namespace inference::field_program_codec_detail;
    using namespace available_evidence_codec_detail;
    const storage::TemporalReadLimits policy;Reader r(bytes,policy);header(r,bytes,evidence_magic);auto w=envelope(evidence_magic);blob(w,r.blob());
    auto declaration=relational_affine_codec_detail::parameters(r);if(parameters_changed)++declaration.key;
    relational_affine_codec_detail::parameters(w,declaration);for(std::size_t i=0;i<4;++i)w.u64(r.u64());
    w.u64(r.u64());blob(w,r.blob());const auto updates=r.count(8);w.u64(updates);
    for(std::size_t i=0;i<updates;++i){const auto count=r.count(66);w.u64(count);for(std::size_t j=0;j<count;++j){
        w.u64(r.u64());blob(w,r.blob());blob(w,r.blob());w.u64(r.u64());auto target=storage::temporal_codec_detail::tensor(r);
        if(!parameters_changed && i==0 && j==0)target=tensor({99,99,0,0,0,0,0});
        storage::temporal_codec_detail::tensor(w,target);
        const auto mask=r.count(6);w.u64(mask);for(std::size_t k=0;k<mask;++k)inference::field_interface_codec_detail::local_cell(w,inference::field_interface_codec_detail::local_cell(r));
        w.u64(r.u64());w.u64(r.u64());blob(w,r.blob());blob(w,r.blob());
    }}blob(w,r.blob());return finish(std::move(w));
}
void affine_json(const RelationalAffineField& f){
    std::cout<<"{\"particular\":";json(f.particular.values().dense());std::cout<<",\"directions\":";json(f.directions);std::cout<<'}';
}
void run(bool emit){
    const Program p(source(),base_declaration());const auto train_asset=asset();const auto prefix=publication(train_asset);
    std::vector<Grant> grants;const auto pb=encode_relational_field_program(p);
    grant(grants,1,Role::Program,pb,prefix);grant(grants,2,Role::Maps,pb,prefix);grant(grants,3,Role::Metrics,pb,prefix);
    const PreparedAvailableRelationalProgram prepared(p,{1,2,3},Context(grants));const auto params=parameters();
    grant(grants,4,Role::Evidence,relational_evidence_root_payload(pb,params,{}),prefix);
    const Available root(prepared,params,4,Context(grants));
    rejects_origin([&]{(void)Available(prepared,params,4,withheld(grants,4));},"unavailable root rejected before initial fit");
    const V x1{1,0,0,0},x2{0,1,0,0};
    const auto first_input=published_input(prepared,train_asset,x1,10,grants);
    const auto second_input=published_input(prepared,train_asset,x2,20,grants);
    const auto first=measurement(first_input,p,x1,71,30,grants);const auto second=measurement(second_input,p,x2,72,40,grants);
    const auto partial=root.append({first},Context(grants));const auto identified=partial.append({second},Context(grants));
    const RelationalAvailableObservationSubmission raw_first{first.key,{{submission(p,train_asset,x1),{10,11,12,13}}},first.target_asset,first.target_use,first.target,first.observed,first.origins};
    const RelationalAvailableObservationSubmission raw_second{second.key,{{submission(p,train_asset,x2),{20,21,22,23}}},second.target_asset,second.target_use,second.target,second.observed,second.origins};
    const auto bulk=root.append_inputs({raw_first,raw_second},Context(grants));
    check(encode_available_relational_evidence(bulk)==encode_available_relational_evidence(root.append({first,second},Context(grants))),"native raw bulk equals full owning suffix append");
    const auto native=identified.evidence();const auto partial_native=partial.evidence();
    check(partial_native.free_parameters().size()==3 && native.free_parameters().size()==2,"correlated partial and inactive/zero alternatives retained");
    const auto fixed=native.program();
    check(native.parameters()==params.parameters && fixed.fibers().same_owner(p.fibers()),"source and named coordinates fixed");
    check(encode_relational_field_evidence(identified.parent()->evidence())==encode_relational_field_evidence(partial_native),"actual immutable evidence parent preserved");
    const auto parent=root.evidence();const auto raw=parent.append({{first.key,first.input.result(),first.target_asset,first.target_use,first.target,first.observed}});
    check(encode_relational_field_evidence(raw)==encode_relational_field_evidence(partial_native),"native whole fit and full source observations unchanged");
    const auto root_bytes=encode_available_relational_evidence(root);const auto model_bytes=encode_available_relational_evidence(identified);
    const auto partial_bytes=encode_available_relational_evidence(partial);const storage::TemporalReadLimits read;
    check(encode_available_relational_evidence(decode_available_relational_evidence(model_bytes,Context(grants),read))==model_bytes,"cold model rebuilds admitted facts against one parent");
    rejects_origin([&]{(void)decode_available_relational_evidence(grafted_model(model_bytes,true),Context(grants),read);},"valid-envelope actual parameter basis graft rejected before fit");
    rejects_origin([&]{(void)decode_available_relational_evidence(grafted_model(model_bytes,false),Context(grants),read);},"valid-envelope actual target graft rejected before fit");
    rejects_origin([&]{(void)partial.append({second},withheld(grants,41));},"late measurement rejects before append fit");
    rejects_origin([&]{(void)partial.append({second},withheld(grants,21));},"future actual input rejects at append");
    rejects_origin([&]{(void)decode_available_relational_evidence(model_bytes,withheld(grants,41),read);},"cold model checks all later targets before any fit");
    auto inconsistent=first;inconsistent.key=73;inconsistent.target=tensor({99,99,0,0,0,0,0});
    grant(grants,50,Role::TargetAsset,encode_relational_input_use_asset(inconsistent.target_asset),prefix);
    grant(grants,51,Role::TargetValue,relational_target_value_payload(pb,2,encode_relational_input_use_asset(inconsistent.target_asset),111,inconsistent.target),prefix);
    inconsistent.origins={50,51};
    rejects_origin([&]{(void)partial.append({inconsistent,second},withheld(grants,41));},"entire suffix admitted before inconsistent first measurement could fit");
    rejects([&]{(void)partial.append({inconsistent},Context(grants));},"authorized inconsistent target rejects without changing parent");
    check(encode_available_relational_evidence(partial)==partial_bytes && encode_available_relational_evidence(root)==root_bytes,"failed suffix leaves original published owners unchanged");
    auto target_as_input=first;target_as_input.origins.value=11;
    rejects_origin([&]{(void)root.append({target_as_input},Context(grants));},"input cannot be renamed to target measurement");
    auto other_program=p.declaration();other_program.key=52;const Program foreign(source(),other_program);
    const PreparedAvailableRelationalProgram equal_owner(Program(p.publication(),p.declaration()),{1,2,3},Context(grants));
    const auto foreign_input=published_input(equal_owner,train_asset,x1,60,grants);auto foreign_observation=first;foreign_observation.input=foreign_input;
    rejects([&]{(void)root.append({foreign_observation},Context(grants));},"live equal program bytes do not imply input owner");
    auto invalid_raw=raw_first;invalid_raw.inputs[0].input.declaration.reconstruction=M(4,4);
    const auto invalid_asset=encode_relational_input_use_asset(train_asset);
    const auto invalid_accession=relational_input_accession_payload(pb,invalid_asset,invalid_raw.inputs[0].input.declaration,111,invalid_raw.inputs[0].input.source_field);
    grant(grants,70,Role::InputAccession,invalid_accession,prefix);grant(grants,71,Role::Reconstruction,invalid_accession,prefix);
    invalid_raw.inputs[0].origins.accession=70;invalid_raw.inputs[0].origins.reconstruction=71;
    rejects_origin([&]{(void)root.append_inputs({invalid_raw,raw_second},withheld(grants,41));},"whole raw suffix target admitted before first invalid A/R preparation");
    rejects_origin([&]{(void)root.append_inputs({invalid_raw,raw_second},withheld(grants,21));},"whole raw suffix input admitted before first invalid A/R preparation");
    const V chosen{3,7,-5};const auto setting=relational_parameter_setting_payload(partial,chosen);
    grant(grants,80,Role::Prospective,setting,publication(train_asset,true));
    grant(grants,81,Role::TargetValue,setting,publication(train_asset,true));
    for(std::size_t trial=0;trial<16;++trial){
        V values{Q{static_cast<long>(trial)-3,3},Q{static_cast<long>(trial%5)-2,5},Q{static_cast<long>(trial%7)+1,7},Q{static_cast<long>(trial%3)+2,11}};
        for(auto& value:values)value.canonicalize();
        const auto held_asset=asset(true,9001+trial);const auto held=published_input(prepared,held_asset,values,100+4*trial,grants);
        const Context context(grants);const auto family=partial.execute(held,context);const auto fully=identified.execute(held,context);
        const auto raw_family=partial.execute_inputs({{submission(p,held_asset,values),{100+4*trial,101+4*trial,102+4*trial,103+4*trial}}},context);
        check(encode_available_relational_affine_result(raw_family)==encode_available_relational_affine_result(family),"native raw held convenience preserves actual evidence owner and full fields");
        const auto evaluation=family.evaluate(chosen,80,context);const auto actual=evaluation.result();const auto owned=actual.result();const auto actual_fields=owned.result();
        check(actual_fields.field(2).values().dense()==response(values,{2,-1,3}),"global parameter choice transfers to independently supplied held field");
        const auto complete=fully.result();const auto uncertain=family.result();
        check(complete.field(2).directions.nonzeros()==0 && uncertain.field(2).directions.nonzeros()>1,"multiple correlated output directions kept before decoding");
        const auto intrinsic=partial_native.coefficient_independent_map({1},3);
        check(intrinsic.directions().size()==3,"unknown inactive intrinsic map preserved");
        const auto fb=encode_available_relational_affine_result(family);const auto eb=encode_available_relational_affine_evaluation(evaluation);
        check(encode_available_relational_affine_result(decode_available_relational_affine_result(fb,context,read))==fb,"standalone cold complete family uses one actual parent");
        check(encode_available_relational_affine_evaluation(decode_available_relational_affine_evaluation(eb,context,read))==eb,"standalone cold global choice keeps source and field lineage");
        rejects_origin([&]{(void)partial.execute(held,withheld(grants,31));},"inference rechecks retained training target availability");
        rejects_origin([&]{(void)partial.execute(held,withheld(grants,101+4*trial));},"held input availability independent of early bytes");
        rejects_origin([&]{(void)decode_available_relational_affine_result(fb,withheld(grants,101+4*trial),read);},"family cold held dependency admitted before fit");
        rejects_origin([&]{(void)family.evaluate(chosen,80,withheld(grants,80));},"unavailable global setting cannot materialize prospective child");
        rejects_origin([&]{(void)family.evaluate(chosen,81,context);},"target role cannot satisfy prospective choice");
        rejects_origin([&]{(void)decode_available_relational_affine_evaluation(eb,withheld(grants,80),read);},"cold setting admission precedes family replay");
        rejects_origin([&]{(void)family.evaluate({4,7,-5},80,context);},"setting origin binds complete free coordinates");
        rejects_origin([&]{(void)fully.evaluate({7,-5},80,context);},"global setting cannot graft onto another evidence or free carrier");
        const auto live_parent=uncertain.evidence();check(encode_relational_affine_result(uncertain)==encode_relational_affine_result(live_parent.execute(held.result())),"complete affine source/flag action equals native original");
        if(emit){std::cout<<"{\"input\":";json(values);std::cout<<",\"free\":";json(chosen);std::cout<<",\"boundary\":";affine_json(uncertain.field(2));
            std::cout<<",\"flags\":";affine_json(uncertain.occurrence_field(2));std::cout<<",\"middle\":";affine_json(uncertain.field(3));
            std::cout<<",\"evaluated\":";json(actual_fields.field(2).values().dense());std::cout<<"}\n";}
        if(trial==0){
            std::size_t steps{};rejects([&]{(void)partial.append({second},context,[&]{if(++steps==5)throw std::runtime_error("cancel");});},"cancel append publishes no partial child");
            steps=0;rejects([&]{(void)decode_available_relational_affine_evaluation(eb,context,read,{},[&]{if(++steps==8)throw std::runtime_error("cancel");});},"cancel at cold preflight boundary");
            auto mutable_bytes=eb;auto mutable_context=context;bool changed{};
            const auto pinned=decode_available_relational_affine_evaluation(mutable_bytes,mutable_context,read,{},[&]{if(!changed){changed=true;mutable_bytes.clear();mutable_context=withheld(grants,80);}});
            check(encode_available_relational_affine_evaluation(pinned)==eb,"cold byte and authority owners pinned before callback");
            std::vector<std::future<Bytes>> readers;for(std::size_t i=0;i<6;++i)readers.push_back(std::async(std::launch::async,[partial,held,context]{return encode_available_relational_affine_result(partial.execute(held,context));}));
            for(auto& reader:readers)check(reader.get()==fb,"independent correlated native actions and context readers");
            auto small=read;small.max_file_bytes=8;rejects([&]{(void)decode_available_relational_affine_evaluation(eb,context,small);},"reader bytes charged before copy");
            auto cap=numeric::ExactActionLimits{};cap.dimension=1;rejects([&]{(void)decode_available_relational_evidence(partial_bytes,context,read,cap);},"root exact policy preserved and capped");
            auto replaced=grants;const auto original=replaced[3].origin;replaced[3].origin=Origin(original.key(),original.publication(),original.role(),original.payload());
            rejects([&]{partial.admit(Context(replaced));},"live equal origin bytes do not establish owner");
            check(encode_available_relational_evidence(decode_available_relational_evidence(partial_bytes,Context(replaced),read))==partial_bytes,"cold original-only replay explicitly adopts independent equal receipts");
            const auto escaped=[&]{const auto local=partial;return local.execute(held,context);}();check(encode_available_relational_affine_result(escaped)==fb,"whole family survives prepared owner lifetime");
            check(encode_available_relational_evidence(partial)==partial_bytes,"cancellation never mutates immutable parent");
        }
    }
}
} // namespace
int main(int argc,char** argv){try{const bool emit=argc>1 && std::string_view(argv[1])=="--emit";run(emit);if(!emit)std::cout<<"relational_available_evidence_checks="<<checks<<'\n';}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
