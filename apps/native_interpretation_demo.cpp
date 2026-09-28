#include "rfc/database/interpretation_codec.hpp"
#include "rfc/database/native_query.hpp"
#include "rfc/models/native_context.hpp"
#include <iostream>

int main(int argc,char** argv){try{
    using namespace rfc;using namespace database;using V=composite::DeclaredValue;using D=composite::Declaration;
    const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::path("native-interpretation-demo");
    if(std::filesystem::exists(out)) throw std::invalid_argument("fresh output directory required");
    std::filesystem::create_directories(out);
    InterpretationSchema schema;schema.source_namespace=1001;schema.binding_namespace=1002;
    const composite::DeclarationId quantity{1003,1,1},optional{1003,2,1},payload{1003,3,1};
    schema.values=composite::DeclarationRegistry({D::rational(quantity),D::optional(optional,quantity),D::product(payload,{{1,optional,0}})});
    schema.subjects={{1,"participants"}};
    schema.assertions={{10,"transfer",payload,{{11,1,1,4,false,false},{12,1,1,4,false,false}},{{1,"quantity"}}}};
    LogicalSnapshot input;input.revision="producer/0";input.subjects={{1,"alice"},{1,"bob"}};
    const auto make=[](std::string identity,V value){return ExternalAssertion{{10,std::move(identity)},V::product({std::move(value)}),{{11,ReferenceState::Present,{{"from",{1,"alice"},true}}},{12,ReferenceState::Present,{{"to",{1,"bob"},true}}}}};};
    input.assertions={make("event/1",V::present(V::rational(numeric::Rational{3,2}))),make("event/2",V::present(V::rational(numeric::Rational{3,2})))};
    const auto base=InterpretedSnapshot::interpret(schema,input);write_interpreted_snapshot(base,out/"base.rfcxi");
    input.revision="producer/1";input.assertions[0].payload=V::product({V::present(V::rational(0))});input.assertions.push_back(make("event/3",V::absent()));
    const auto target=base.replace(input);const auto segment=encode_interpretation_update(base.difference(target));interpretation_codec_detail::write(out/"change.rfcxiu",segment);
    const auto disk_base=read_interpreted_snapshot(out/"base.rfcxi");storage::ReadOnlyMappedFile update(out/"change.rfcxiu");const auto replay=disk_base.apply(decode_interpretation_update(update.bytes()));
    storage::NativeAtlasManifest manifest;manifest.key=1;manifest.revision=1;storage::NativeAtlas atlas({replay.source()},manifest);
    const auto descriptor=replay.payload(1,{1}).descriptor();
    const auto request=descriptor.request();
    const auto context=models::NativeConsumerContext::build(atlas,{{1,request}},[&](const auto& origin){return origin.source==1 && storage::native_accession_detail::request_bytes(origin.accession,{})==storage::native_accession_detail::request_bytes(request,{});});
    const auto proposal=context.propose(models::ProspectiveEncoding::Approximate,"example-consumer","uncommitted recommendation",{std::byte{1}});
    const bool exact=encode_interpreted_snapshot(replay)==encode_interpreted_snapshot(target);
    if(!exact || proposal.bytes().size()!=1 || context.items().size()!=1)throw std::runtime_error("interpretation demo verification failed");
    std::cout<<"assertions="<<base.assertions(10).size()<<"->"<<replay.assertions(10).size()<<" retained_occurrences="<<replay.endpoint()->source.grade(core::Grade{1}).incidence_count()
        <<" exact_sum="<<sum_exact(replay,10,{1})->get_str()<<" nulls="<<select_exact(replay,10,{{1},ExactPredicateKind::IsNull,{}}).size()
        <<" authorized_context_items="<<context.items().size()<<" proposal_uncommitted=true exact_disk_replay="<<std::boolalpha<<exact<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
