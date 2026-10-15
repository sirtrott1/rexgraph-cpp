#include "rfc/inference/relational_initial_input_lineage_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using namespace rfc::inference;
namespace {
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected{};try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
storage::NativeTemporalSeries history(std::uint64_t ns,std::size_t minimum=1,std::size_t maximum=8,bool altered=false,const char* name="context",bool extension=false){
    core::RelationSignature signature;const auto type=signature.add_type(name,core::BoundaryPolicy::DeclaredExact,minimum,maximum);
    if(extension)signature.add_type("unused",core::BoundaryPolicy::DeclaredExact,1,2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100},type,{{10,true,false,1},{20,true,true,1},{10,true,true,1},{20,false,false,1},{30,true,false,altered?1:0}});
    storage::TemporalEndpoint endpoint(builder.freeze(),1);storage::NativeTemporalSeries series(endpoint,ns);
    for(std::uint64_t i=2;i<=5;++i){endpoint.sequence=i;series.append_target(endpoint);}return series;
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series){
    RelationalInputUseDeclaration d;d.key=70;d.channel=700;d.encoding="UTF-8";
    for(const auto c:std::string("bank / bank"))d.bytes.push_back(static_cast<std::byte>(c));
    const auto publication=series.at(1);RelationalSpanUse use{111,{{{1},100}},{},{{0,4},{7,11},{0,0},{0,4}}};
    for(std::size_t i=0;i<5;++i)use.occurrences.push_back({{1},publication.endpoint().source.occurrence_identity({1},i)});
    d.uses={use};return {storage::NativeSeriesPrefix::capture(series,1).history(),d};
}
void run(){
    const auto old_series=history(501),new_series=history(502,5,5,false,"context",true),publisher=history(503);
    const auto old=asset(old_series),next=asset(new_series);const RelationalInitialInputLineageDeclaration declaration{801,1,{{111,111}}};
    const OwnedRelationalInitialInputLineage lineage(old,next,declaration);
    check(!lineage.original().same_owner(lineage.initial()),"separate original and initial owners retained");
    check(encode_relational_input_use_asset(lineage.original())==encode_relational_input_use_asset(old),"complete old source and history retained");
    check(encode_relational_input_use_asset(lineage.initial())==encode_relational_input_use_asset(next),"arity specialization and unused signature extension admitted explicitly");
    rejects([&]{OwnedRelationalInitialInputLineage invalid(old,old,declaration);},"same actual owner cannot be called a new initial source");
    const auto equal=decode_relational_input_use_asset(encode_relational_input_use_asset(old),{});
    rejects([&]{OwnedRelationalInitialInputLineage invalid(old,equal,declaration);},"equal restored source namespace is not a separate initial namespace");
    rejects([&]{OwnedRelationalInitialInputLineage invalid(next,asset(history(504)),declaration);},"arity widening rejected");
    rejects([&]{OwnedRelationalInitialInputLineage invalid(old,asset(history(504,5,5,true)),declaration);},"supported zero flag cannot become nonzero");
    rejects([&]{OwnedRelationalInitialInputLineage invalid(old,asset(history(504,5,5,false,"changed")),declaration);},"type meaning cannot change through lineage");
    auto omitted=declaration;omitted.uses.clear();rejects([&]{OwnedRelationalInitialInputLineage invalid(old,next,omitted);},"all ordered original uses required");
    auto changed=next.declaration();changed.bytes[0]=std::byte{'B'};
    const auto changed_bytes=OwnedRelationalInputUseAsset(next.history(),changed);
    rejects([&]{OwnedRelationalInitialInputLineage invalid(old,changed_bytes,declaration);},"literal byte changes rejected");
    changed=next.declaration();std::swap(changed.uses[0].spans[0],changed.uses[0].spans[1]);
    const auto changed_spans=OwnedRelationalInputUseAsset(next.history(),changed);
    rejects([&]{OwnedRelationalInitialInputLineage invalid(old,changed_spans,declaration);},"ordered repeated and empty span history preserved");
    std::vector<RelationalOriginGrant> grants;const auto publication=storage::NativeSeriesPrefix::capture(publisher,1);
    const auto add=[&](std::uint64_t key,RelationalDependencyRole role,std::vector<std::byte> bytes){grants.push_back({OwnedRelationalValueOrigin(key,publication,role,std::move(bytes)),{503,5}});};
    add(1,RelationalDependencyRole::InputAsset,encode_relational_input_use_asset(old));
    add(2,RelationalDependencyRole::InputAsset,encode_relational_input_use_asset(next));
    add(3,RelationalDependencyRole::Interface,relational_initial_input_lineage_payload(lineage));
    const RelationalAvailabilityContext context(grants);const AvailableRelationalInitialInputLineage available(lineage,{1,2,3},context);
    const auto packet=encode_available_relational_initial_input_lineage(available);
    const auto cold=decode_available_relational_initial_input_lineage(packet,context,{}, {},{},next);
    check(cold.lineage().initial().same_owner(next),"explicit cold adoption retains actual already checked initial asset");
    check(encode_available_relational_initial_input_lineage(cold)==packet,"complete original and initial canonical cold bytes");
    rejects([&]{decode_available_relational_initial_input_lineage(packet,context,{}, {},{},old);},"wrong declared initial owner rejected even with same dimensions");
    for(std::size_t i=0;i<3;++i){auto early=grants;early[i].availability.max_sequence=0;const RelationalAvailabilityContext denied(early);
        rejects([&]{available.admit(denied);},"every original initial and interface warm cutoff checked");
        rejects([&]{initial_input_lineage_codec_detail::preflight(packet,denied,{},{});},"every current cutoff checked passively before reconstruction");}
    auto foreign=grants;const auto origin=foreign[0].origin;
    foreign[0].origin=OwnedRelationalValueOrigin(origin.key(),origin.publication(),origin.role(),origin.payload());
    rejects([&]{available.admit(RelationalAvailabilityContext(foreign));},"equal passive receipt does not identify warm provenance");
    std::vector<std::future<bool>> readers;
    for(unsigned i=0;i<3;++i)readers.push_back(std::async(std::launch::async,[packet,context]{return encode_available_relational_initial_input_lineage(decode_available_relational_initial_input_lineage(packet,context,{}))==packet;}));
    for(auto& reader:readers)check(reader.get(),"independent concurrent full lineage restoration");
    rejects([&]{decode_available_relational_initial_input_lineage(packet,context,{}, {},[]{throw std::runtime_error("cancel");});},"cancellation rejects without publishing an owner");
}
}
int main(){try{run();std::cout<<checks<<" initial-input lineage checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
