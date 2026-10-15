#include "rfc/learning/relational_four_use_grammar_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using Matrix=numeric::ExactMatrix;
using Bytes=std::vector<std::byte>;
namespace {
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool hit{};try{action();}catch(const std::exception&){hit=true;}check(hit,message);}
template<class F>void image_rejects(F action,const char* message){bool hit{};try{action();}catch(const std::invalid_argument& e){hit=std::string_view(e.what()).find("teacher grammar")!=std::string_view::npos;}check(hit,message);}
OwnedRelationalFieldProgram parent_program(const OwnedRelationalInputUseAsset& asset,numeric::ExactActionLimits limits) {
    RelationalFieldProgramDeclaration d;d.key=20000;d.revision=1;d.differential=true;
    for(std::uint32_t word=0;word<4;++word) {
        std::vector<std::uint64_t> dirs;for(std::uint64_t i=1;i<=17;++i)dirs.push_back(i);
        d.stalks.push_back({{{0},word},701,std::move(dirs),Matrix::identity(17)});
        Matrix map(17,68);for(std::size_t i=0;i<17;++i)map.set(i,word*17+i,1);
        d.occurrences.push_back({{1},word,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    std::vector<std::uint64_t> dirs;for(std::uint64_t i=1000;i<1068;++i)dirs.push_back(i);
    d.stalks.push_back({{{1},0},8000,std::move(dirs),Matrix::identity(68)});
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};
    return {asset.publication(),std::move(d),limits};
}
OwnedRelationalFieldProgram model_program(const storage::NativeTemporalSeries& model,numeric::ExactActionLimits limits) {
    RelationalFieldProgramDeclaration d;d.key=20001;d.revision=1;d.differential=true;
    for(std::uint32_t word=0;word<4;++word) {
        std::vector<std::uint64_t> dirs;for(std::uint64_t i=1;i<=17;++i)dirs.push_back(i);
        for(std::uint64_t head=0;head<4;++head)if(head!=word)for(std::uint64_t role=0;role<2;++role)
            dirs.push_back(10000+100*head+10*word+role);
        d.stalks.push_back({{{0},word},90000+word,std::move(dirs),Matrix::identity(23)});
        Matrix map(23,4692);for(std::size_t i=0;i<17;++i)map.set(i,word*17+i,1);
        d.occurrences.push_back({{1},word,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    std::vector<std::uint64_t> dirs;for(std::uint64_t i=1000;i<1068;++i)dirs.push_back(i);
    for(std::uint64_t i=2000;i<6624;++i)dirs.push_back(i);
    for(std::uint32_t context=0;context<13;++context)
        d.stalks.push_back({{{1},context},9000,dirs,Matrix::identity(4692)});
    // Declared reference zero maps are construction controls, not fitted role predictions.
    for(std::size_t occurrence=4;occurrence<28;++occurrence)
        d.occurrences.push_back({{1},occurrence,storage::TemporalTensorRole::CoefficientIndependentMap,Matrix(23,4692)});
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};
    return {model.at(model.last_sequence()),std::move(d),limits};
}
RelationalInputAccessionDeclaration declaration(const OwnedRelationalFieldProgram& program,bool lifted) {
    RelationalInputAccessionDeclaration d;d.key=lifted?40001:40000;d.node=1;d.source_grade={static_cast<std::uint16_t>(lifted?1:0)};
    d.isometric=!lifted;d.instances.cells={{0,1,2,3},{0}};d.instances.occurrences={{},{0,1,2,3}};
    if(lifted) d.source_stalks={program.declaration().stalks.at(4)};
    else for(std::size_t i=0;i<4;++i)d.source_stalks.push_back(program.declaration().stalks[i]);
    if(lifted) {
        d.accession=Matrix(60996,4692);d.reconstruction=Matrix(4692,60996);
        for(std::size_t i=0;i<4692;++i) {
            d.reconstruction.set(i,i,1);for(std::size_t context=0;context<13;++context)d.accession.set(4692*context+i,i,1);
        }
    } else {d.accession=Matrix::identity(68);d.reconstruction=d.accession;}
    return d;
}

storage::NativeTemporalSeries history(std::uint64_t ns,bool model,bool nonunit=false){
    core::RelationSignature signature;const auto group=signature.add_type("context",core::BoundaryPolicy::DeclaredExact,4,4);
    const auto pair=signature.add_type("ordered-pair",core::BoundaryPolicy::DeclaredExact,2,2);core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{100},group,{{1,true,false,1,{},core::OccurrenceKey{1}},{2,true,false,1,{},core::OccurrenceKey{2}},
        {3,true,false,1,{},core::OccurrenceKey{3}},{4,true,false,1,{},core::OccurrenceKey{4}}});
    if(model){std::uint64_t ordinal{};for(std::uint64_t h=0;h<4;++h)for(std::uint64_t d=0;d<4;++d)if(h!=d){
        b.add_grade_one(core::RelationKey{200+ordinal},pair,{{h+1,true,false,1,{},core::OccurrenceKey{5+2*ordinal}},
            {d+1,true,false,nonunit?2:1,{},core::OccurrenceKey{6+2*ordinal}}});++ordinal;}}
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(),1),ns);
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series){
    RelationalInputUseDeclaration d;d.key=70;d.channel=700;d.encoding="UTF-8";for(const char c:std::string("four"))d.bytes.push_back(static_cast<std::byte>(c));
    const auto p=series.at(1);RelationalSpanUse use{100,{{{1},100}},{},{{0,4}}};
    for(std::size_t i=0;i<4;++i)use.occurrences.push_back({{1},p.endpoint().source.occurrence_identity({1},i)});
    d.uses={use};return {series,d};
}

void run(){using Role=RelationalDependencyRole;const numeric::ExactActionLimits limits{70000,16000000,64000000,262144};
    const auto initial=history(901,false),prototype=history(902,true),publisher=history(903,false),prior=history(904,false);
    const auto a=asset(initial),old=asset(prior),ta=asset(prototype);const auto parent=parent_program(a,limits),target=model_program(prototype,limits);
    const auto od=declaration(parent,false),td=declaration(target,true);const auto pb=encode_relational_field_program(parent),tb=encode_relational_field_program(target);
    RelationalKnownTensorSquareDeclaration sd{50000,1,9000,{{1},0},{}};for(std::uint64_t i=2000;i<6624;++i)sd.pair_directions.push_back(i);
    RelationalFieldParameterDeclaration parameters{72080,1,2,{{5,17,0},{5,18,0}},Matrix::identity(2),{700000000,700000001}};
    std::vector<RelationalOriginGrant> grants;const auto pub=storage::NativeSeriesPrefix::capture(publisher,1);
    const auto add=[&](std::uint64_t key,Role role,Bytes payload){grants.push_back({OwnedRelationalValueOrigin(key,pub,role,std::move(payload)),{903,1}});};
    add(1,Role::Program,pb);add(2,Role::Maps,pb);add(3,Role::Metrics,pb);add(8,Role::Interface,relational_tensor_square_dependency_payload(parent,sd));
    add(9,Role::Program,tb);add(10,Role::Maps,tb);add(11,Role::Metrics,tb);add(17,Role::Evidence,relational_evidence_root_payload(tb,parameters,limits));
    const auto root_origin=grants.back().origin;const RelationalAvailabilityContext declarations(grants);
    const PreparedAvailableRelationalProgram ap(parent,{1,2,3},declarations),at(target,{9,10,11},declarations);
    const PreparedAvailableRelationalTensorSquare producer(ap,sd,8,declarations);
    const PreparedAvailableRelationalTensorSquareEvidence root(at,parameters,17,declarations,limits);
    const OwnedRelationalInitialInputLineage original(old,a,{41000,1,{{100,100}}},limits);
    std::vector<RelationalAvailableTensorSquareObservation> observations;
    for(std::size_t sample=0;sample<2;++sample){numeric::RationalVector g(68);for(std::size_t i=0;i<4;++i)g[17*i+i]=1;if(sample)g[0]=0;
        numeric::ExactActionGuard guard(limits);const auto tensor=field_interface_detail::tensor(g,guard);const auto base=1000+100*sample;
        const auto bytes=encode_relational_input_use_asset(a);add(base,Role::InputAsset,bytes);add(base+1,Role::InputValue,relational_input_value_payload(bytes,od,100,tensor));
        const auto access=relational_input_accession_payload(pb,bytes,od,100,tensor);add(base+2,Role::InputAccession,access);add(base+3,Role::Reconstruction,access);
        const RelationalAvailabilityContext first(grants);const auto square=producer.lift({a,od,100,tensor},{base,base+1,base+2,base+3},first,limits);const auto native=square.square();
        add(base+4,Role::InputAsset,bytes);add(base+5,Role::InputValue,relational_input_value_payload(bytes,td,100,native.lifted_field()));
        const auto next=relational_input_accession_payload(tb,bytes,td,100,native.lifted_field());add(base+6,Role::InputAccession,next);add(base+7,Role::Reconstruction,next);
        const RelationalAvailabilityContext second(grants);const auto input=square.admit_input(at,td,{base+4,base+5,base+6,base+7},second,limits).execute(second,limits);
        const auto result=input.result();const auto fields=result.result();const auto y=fields.field(2);
        add(base+8,Role::TargetAsset,encode_relational_input_use_asset(ta));add(base+9,Role::TargetValue,relational_target_value_payload(tb,2,encode_relational_input_use_asset(ta),100,y));
        const auto target_asset=grants[grants.size()-2].origin,target_value=grants.back().origin;
        add(base+12,Role::InputAsset,encode_relational_input_use_asset(old));add(base+13,Role::Interface,relational_initial_input_lineage_payload(original));
        const RelationalAvailabilityContext third(grants);const AvailableRelationalInitialInputLineage lineage(original,{base+12,base,base+13},third);
        const auto incoming=input.input();auto dependencies=incoming.obligations();dependencies.insert(dependencies.end(),lineage.obligations().begin(),lineage.obligations().end());
        const std::vector<core::CellId> mask{{{0},0},{{0},1},{{0},2},{{0},3}};
        const auto descriptor=relational_tensor_square_observation_payload(sample+1,root_origin,dependencies,target_asset,target_value,100,mask);
        add(base+10,Role::Interface,descriptor);add(base+11,Role::Evidence,descriptor);
        observations.push_back({sample+1,input,ta,100,y,mask,{base+8,base+9},base+10,base+11,lineage});
    }
    const RelationalAvailabilityContext context(grants);const auto learned=root.append({observations[0]},context),invalid=root.append({observations[1]},context);
    const auto good=observations[0].input,bad=observations[1].input;const auto good_input=good.input(),bad_input=bad.input();
    const auto gi=encode_available_relational_tensor_square_input(good_input),gr=encode_available_relational_tensor_square_result(good);
    const auto bi=encode_available_relational_tensor_square_input(bad_input),br=encode_available_relational_tensor_square_result(bad);
    const auto ge=encode_available_relational_tensor_square_evidence(learned),be=encode_available_relational_tensor_square_evidence(invalid);
    const auto family=learned.execute(good,context,limits,{},observations[0].lineage),bad_family=root.execute(bad,context,limits,{},observations[1].lineage);
    const auto gf=encode_available_relational_tensor_square_affine_result(family),bf=encode_available_relational_tensor_square_affine_result(bad_family);
    preflight_available_relational_four_use_tensor_square_input(gi,context,{});preflight_available_relational_four_use_tensor_square_result(gr,context,{});
    preflight_available_relational_four_use_tensor_square_evidence(ge,context,{},limits);preflight_available_relational_four_use_tensor_square_affine_result(gf,context,{},limits);
    check(true,"passive typed original grammar admission on input/result/model/family");
    check(encode_available_relational_tensor_square_input(decode_available_relational_four_use_tensor_square_input(gi,context,{},limits))==gi,"atomic profiled input preserves canonical bytes");
    check(encode_available_relational_tensor_square_result(decode_available_relational_four_use_tensor_square_result(gr,context,{},limits))==gr,"atomic profiled result preserves canonical bytes");
    check(encode_available_relational_tensor_square_evidence(decode_available_relational_four_use_tensor_square_evidence(ge,context,{},limits))==ge,"atomic profiled model preserves whole sources and fit");
    check(encode_available_relational_tensor_square_affine_result(decode_available_relational_four_use_tensor_square_affine_result(gf,context,{},limits))==gf,"atomic profiled family preserves complete original lineage");
    image_rejects([&]{preflight_available_relational_four_use_tensor_square_input(bi,context,{});},"invalid finite image rejects passively");
    image_rejects([&]{preflight_available_relational_four_use_tensor_square_result(br,context,{});},"invalid original is not hidden by legitimate derived product");
    image_rejects([&]{preflight_available_relational_four_use_tensor_square_evidence(be,context,{},limits);},"all original training images checked before fit");
    image_rejects([&]{preflight_available_relational_four_use_tensor_square_affine_result(bf,context,{},limits);},"held image checked before model/query replay");
    auto no_math=limits;no_math.dimension=0;no_math.steps=0;
    image_rejects([&]{decode_available_relational_four_use_tensor_square_input(bi,context,{},no_math);},"profile refusal before dimension/step-zero A/R/product");
    image_rejects([&]{decode_available_relational_four_use_tensor_square_result(br,context,{},no_math);},"profile result refusal before operator work");
    const auto lineage_packet=encode_available_relational_initial_input_lineage(*observations[0].lineage);
    const auto paired=decode_available_relational_four_use_query(gr,lineage_packet,context,{},limits,{},target);
    const auto paired_input=paired.input.input();const auto paired_native=paired_input.native_input();const auto paired_square=paired_native.square();
    check(paired.lineage.lineage().initial().same_owner(paired_square.parent().accession().asset()),"atomic paired query adopts the actual restored product initial owner");
    check(encode_available_relational_tensor_square_result(paired.input)==gr&&encode_available_relational_initial_input_lineage(paired.lineage)==lineage_packet,"paired full input/original lineage canonical bytes");
    const auto shared_query=storage::encode_shared_packet(gr);
    const auto paired_shared=decode_shared_available_relational_four_use_query(shared_query,lineage_packet,context,{},limits,{}, {},target);
    check(encode_available_relational_tensor_square_result(paired_shared.input)==gr,"paired shared input with raw lineage uses one current authority closure");
    for(const auto key:std::array<std::uint64_t,3>{1012,1000,1013}){auto denied=grants;for(auto& grant:denied)if(grant.origin.key()==key)grant.availability.max_sequence=0;
        bool before_product{};try{decode_available_relational_four_use_query(gr,lineage_packet,RelationalAvailabilityContext(denied),{},no_math);}
        catch(const std::invalid_argument&){before_product=true;}check(before_product,"every old/initial/interface query cutoff preflights before step-zero product");}
    const auto other_asset=asset(history(905,false));const OwnedRelationalInitialInputLineage other_original(old,other_asset,{41000,1,{{100,100}}},limits);
    auto mismatched=grants;const auto add_other=[&](std::uint64_t key,Role role,Bytes bytes){mismatched.push_back({OwnedRelationalValueOrigin(key,pub,role,std::move(bytes)),{903,1}});};
    add_other(2000,Role::InputAsset,encode_relational_input_use_asset(other_asset));add_other(2001,Role::Interface,relational_initial_input_lineage_payload(other_original));
    const RelationalAvailabilityContext mismatched_context(mismatched);const AvailableRelationalInitialInputLineage mismatch(other_original,{1012,2000,2001},mismatched_context);
    const auto mismatch_packet=encode_available_relational_initial_input_lineage(mismatch);bool mismatch_before_product{};
    try{decode_available_relational_four_use_query(gr,mismatch_packet,mismatched_context,{},no_math);}
    catch(const std::invalid_argument& e){mismatch_before_product=std::string_view(e.what()).find("paired query initial source")!=std::string_view::npos;}
    check(mismatch_before_product,"available equal-dimensional foreign initial source rejects before product");
    auto mutable_query=gr,mutable_lineage=lineage_packet;bool changed_query{};
    const auto pinned_query=decode_available_relational_four_use_query(mutable_query,mutable_lineage,context,{},limits,[&]{if(!changed_query){changed_query=true;mutable_query.clear();mutable_lineage.clear();}},target);
    check(changed_query&&encode_available_relational_tensor_square_result(pinned_query.input)==gr,"both paired packets pinned before callbacks");
    const auto expected_root=relational_evidence_root_payload(tb,parameters,limits);
    const auto root_packet=encode_available_relational_tensor_square_evidence(root);
    const auto matching=decode_available_relational_four_use_tensor_square_evidence(root_packet,context,{},limits,{},expected_root);
    check(encode_available_relational_tensor_square_evidence(matching)==root_packet,"zero-observation root validates complete explicit declaration before fitting");
    auto other_parameters=parameters;other_parameters.basis.set(0,0,2);
    const auto different_root=relational_evidence_root_payload(tb,other_parameters,limits);
    rejects([&]{preflight_available_relational_four_use_tensor_square_evidence(root_packet,context,{},limits,{},different_root);},"equal dimensions and parameter identities do not validate changed basis coefficients");
    rejects([&]{decode_available_relational_four_use_tensor_square_evidence(root_packet,context,{},limits,{},different_root);},"atomic zero-observation root refuses foreign full declaration");
    rejects([&]{decode_available_relational_four_use_tensor_square_affine_result(gf,context,{},limits,{},different_root);},"full family root comparison before replay");
    auto mutable_expected=expected_root;bool replaced_expected{};
    const auto matched=decode_available_relational_four_use_tensor_square_evidence(root_packet,context,{},limits,[&]{if(!replaced_expected){replaced_expected=true;mutable_expected.clear();}},mutable_expected);
    check(replaced_expected&&encode_available_relational_tensor_square_evidence(matched)==root_packet,"explicit expected declaration pinned before callbacks");
    const auto without_lineage=root.execute(good,context,limits);const auto missing_lineage=encode_available_relational_tensor_square_affine_result(without_lineage);
    image_rejects([&]{preflight_available_relational_four_use_tensor_square_affine_result(missing_lineage,context,{},limits);},"profile does not omit the actual original source lineage");
    const auto all=learned.append({observations[1]},context);const auto mixed=encode_available_relational_tensor_square_evidence(all);
    auto hidden=grants;for(auto& grant:hidden)if(grant.origin.key()==1001)grant.availability.max_sequence=0;
    bool authority_first{};try{preflight_available_relational_four_use_tensor_square_evidence(mixed,RelationalAvailabilityContext(hidden),{},limits);}
    catch(const std::exception& e){authority_first=std::string_view(e.what()).find("teacher grammar")==std::string_view::npos;}
    check(authority_first,"entire current authority closure before inspecting any invalid later grammar image");
    const auto shared_bad=encode_shared_available_relational_tensor_square_evidence(invalid);
    image_rejects([&]{decode_shared_available_relational_four_use_tensor_square_evidence(shared_bad,context,{},limits);},"shared expansion reaches image refusal before any fit");
    const auto shared=encode_shared_available_relational_tensor_square_evidence(learned),shared_family=encode_shared_available_relational_tensor_square_affine_result(family);
    check(encode_available_relational_tensor_square_evidence(decode_shared_available_relational_four_use_tensor_square_evidence(shared,context,{},limits))==ge,"one pinned shared expansion through model profile/replay");
    check(encode_available_relational_tensor_square_affine_result(decode_shared_available_relational_four_use_tensor_square_affine_result(shared_family,context,{},limits))==gf,"one pinned shared expansion through family profile/replay");
    rejects([&]{decode_shared_available_relational_four_use_tensor_square_evidence(shared,context,{},limits,{}, {},different_root);},"shared model compares full expected declaration atomically");
    check(encode_available_relational_tensor_square_affine_result(decode_shared_available_relational_four_use_tensor_square_affine_result(shared_family,context,{},limits,{}, {},expected_root))==gf,"shared family matches exact expected creation program/basis/limits");
    for(const auto& grant:grants){const auto key=grant.origin.key();if(key>=1100)continue;auto denied=grants;
        for(auto& value:denied)if(value.origin.key()==key)value.availability.max_sequence=0;
        rejects([&]{preflight_available_relational_four_use_tensor_square_affine_result(gf,RelationalAvailabilityContext(denied),{},limits);},"every inherited/input/old source/target/observer authority checked before profile");}
    auto mutable_bytes=gf;auto mutable_context=context;storage::TemporalReadLimits mutable_policy;bool changed{};
    const auto restored=decode_available_relational_four_use_tensor_square_affine_result(mutable_bytes,mutable_context,mutable_policy,limits,[&]{if(!changed){changed=true;mutable_bytes.clear();mutable_context=RelationalAvailabilityContext({});mutable_policy.max_file_bytes=0;mutable_policy.max_dimension=0;}});
    check(changed&&encode_available_relational_tensor_square_affine_result(restored)==gf,"bytes/context/read policy pinned through atomic profile and replay");
    std::vector<std::future<bool>> readers;for(unsigned i=0;i<3;++i)readers.push_back(std::async(std::launch::async,[gf,context,limits]{return encode_available_relational_tensor_square_affine_result(decode_available_relational_four_use_tensor_square_affine_result(gf,context,{},limits))==gf;}));
    for(auto& reader:readers)check(reader.get(),"independent concurrent profiled full-family replay");
    rejects([&]{decode_available_relational_four_use_tensor_square_evidence(ge,context,{},limits,[]{throw std::runtime_error("cancel");});},"cancelled passive profile/replay publishes no model");
}
}
int main(){try{run();std::cout<<checks<<" four-use atomic grammar codec checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
