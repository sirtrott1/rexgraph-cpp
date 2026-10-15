#include "../../examples/graph_history/model.hpp"
#include "rfc/storage/algebraic_archive_graph_durable.hpp"
#include <iostream>

using namespace graph_history_example;using namespace rfc::storage;
static std::size_t checks{};
static void check(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
template<class F>static void rejects(F action,const char* message) {bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
struct Temporary {
    std::filesystem::path path;
    Temporary() {auto pattern=(std::filesystem::temp_directory_path()/"rfc-graph-history-XXXXXX").string();
        const auto* result=::mkdtemp(pattern.data());if(!result)throw std::runtime_error("test temporary directory");path=result;}
    ~Temporary(){std::error_code error;std::filesystem::remove_all(path,error);}
};
static auto objects(const std::filesystem::path& path) {
    std::map<std::string,std::vector<std::byte>> result;
    for(const auto& file:std::filesystem::directory_iterator(path/"objects"))
        result.emplace(file.path().filename().string(),read_file_exact(file.path(),64u<<20));
    return result;
}
static auto bytes(const AlgebraicGraphRevision& revision) {return encode_algebraic_graph_snapshot(revision.snapshot());}
static void durable(const std::filesystem::path& path) {
    const AlgebraicGraphChannel channel{7601,1,100};const auto base=graph_history_example::snapshot(0),next=graph_history_example::snapshot(1,true),last=graph_history_example::snapshot(2,true);
    const std::vector<Snapshot> first{next},second{last};DurableNativeRoot origin,committed;
    std::optional<AlgebraicGraphHistoryWindow> retained;std::shared_ptr<const AlgebraicArchive> pinned_archive;
    {
        auto history=AlgebraicGraphDurableHistory::create(path,base,channel,9501,40);origin=history.root();
        const auto old=history.snapshot();const auto original=objects(path);retained=history.prefix(40);pinned_archive=retained->at(40).archive_pin(1);
        const auto receipt=history.append(1,origin,first);committed=receipt.root;
        check(committed.sequence==41 && committed.source_namespace==9501 && receipt.frame_count==1 && receipt.first_sequence==40,"graph publication receipt differs");
        check(bytes(history.at(40))==encode_algebraic_graph_snapshot(base),"later graph leaked into old revision");
        check(bytes(history.at(41))==encode_algebraic_graph_snapshot(next),"graph publication omitted bindings or records");
        rejects([&]{(void)old.at(41);},"old immutable native series sees future graph");
        rejects([&]{(void)retained->at(41);},"old graph prefix sees future graph");
        for(const auto& [name,data]:original)check(objects(path).at(name)==data,"old durable graph object changed");
        const auto unchanged=objects(path);check(history.append(1,origin,first).root==committed && objects(path)==unchanged,"identical graph retry changed objects");
        rejects([&]{(void)history.append(1,origin,second);},"changed graph bytes accepted on retry");
        rejects([&]{(void)history.append(2,origin,second);},"stale graph predecessor accepted");
        auto foreign_root=committed;++foreign_root.source_namespace;
        rejects([&]{(void)history.append(2,foreign_root,second);},"foreign native source namespace accepted");
        rejects([&]{(void)history.append(0,committed,second);},"reserved transaction zero accepted");
        rejects([&]{(void)history.append(2,committed,{});},"empty graph group accepted");
        check(history.root()==committed && objects(path)==unchanged,"rejected graph changed durable root");
        rejects([&]{(void)history.append(2,committed,second,[](DurableEvent event){
            if(event.stage==DurableStage::BeforeHeadReplace)throw std::runtime_error("before decision");
        });},"prepublication fault did not fail");
        check(history.root()==committed && !history.status(2),"unpublished graph was acknowledged");
        bool unknown=false;
        try{(void)history.append(2,committed,second,[](DurableEvent event){
            if(event.stage==DurableStage::HeadReplaced)throw std::runtime_error("after decision");
        });}catch(const DurableUnknownOutcome& error){unknown=error.transaction()==2;}
        check(unknown,"published graph fault did not report unknown outcome");
        rejects([&]{(void)history.root();},"uncertain graph coordinator remained usable");
        rejects([&]{(void)history.prefix(40);},"uncertain coordinator exposed a new prefix");
    }
    check(bytes(retained->at(40))==encode_algebraic_graph_snapshot(base),"prefix lost graph after coordinator close");
    check(std::get<rfc::local::AlgebraicModuleVector>(pinned_archive->at(10)).coordinates().front().coordinates()[0]==0,"archive pin lost graph after coordinator close");
    auto reopened=AlgebraicGraphDurableHistory::open(path,channel);
    check(reopened.root().sequence==42 && reopened.status(2).has_value(),"graph publication decision not recovered");
    check(bytes(reopened.at(40))==encode_algebraic_graph_snapshot(base) && bytes(reopened.at(41))==encode_algebraic_graph_snapshot(next)
        && bytes(reopened.at(42))==encode_algebraic_graph_snapshot(last),"restored graph lineage differs");
    const auto before=objects(path);check(reopened.append(2,committed,second).root==reopened.root() && objects(path)==before,"recovered graph retry applied twice");
    rejects([&]{(void)reopened.at(43);},"future graph admitted");
    const auto a=reopened.at(40),b=reopened.at(40);
    check(!a.snapshot().graph().field(1).same_owner(b.snapshot().graph().field(1)),"independent durable reads interned owners");
    rejects([&]{(void)a.snapshot().graph().carrier(1).apply(std::get<rfc::local::AlgebraicModuleVector>(b.snapshot().graph().archive(1).at(10)));},"equal restored revisions inferred carrier transport");
    const auto selected=reopened.prefix(41);check(selected.revision_count()==2 && selected.native_prefix().segment_count()==1,"recovered prefix retains future frames");
    const auto past=selected.at(41);check(past.snapshot().graph().archive(1).bundle().same_owner(past.snapshot().graph().archive(4).bundle()),"durable graph lost explicit shared owners");
}
static void grouped(const std::filesystem::path& path) {
    const AlgebraicGraphChannel channel{7601,1,100};const auto base=graph_history_example::snapshot(0);
    std::vector<Snapshot> revisions{graph_history_example::snapshot(1,true),graph_history_example::snapshot(2,true)};DurableNativeRoot final;
    {
        auto history=AlgebraicGraphDurableHistory::create(path,base,channel,9502);
        const auto receipt=history.append(7,history.root(),revisions,[&](DurableEvent){revisions.clear();});final=receipt.root;
        check(receipt.frame_count==2 && final.sequence==2,"caller mutation split a frozen graph group");
    }
    auto reopened=AlgebraicGraphDurableHistory::open(path,channel);
    check(reopened.root()==final && reopened.snapshot().segment_count()==2,"group graph recovery omitted revisions");
    for(std::uint64_t seq=0;seq<3;++seq) {
        const auto revision=reopened.at(seq);const auto& x=std::get<rfc::local::AlgebraicModuleVector>(revision.snapshot().graph().archive(1).at(10));
        check(x.coordinates().front().coordinates()[0]==Q{std::to_string(seq)},"grouped graph values or order differ");
    }
    // Metadata alone is also retained history, even when all graph values equal.
    const auto original=graph_history_example::snapshot(2),mapped=original.with_source_morphism(2,{{0,1},{0}});
    check(encode_algebraic_archive_graph(original.graph())==encode_algebraic_archive_graph(mapped.graph()),"source association changed inner RFCXG bytes");
    const std::vector<Snapshot> changes{original,mapped};(void)reopened.append(8,final,changes);
    check(bytes(reopened.at(3))!=bytes(reopened.at(4)),"equal values erased distinct source declarations");
    const auto selected=reopened.prefix(3);rejects([&]{(void)selected.at(4);},"later source map leaked into prefix");
}
static void validation(const std::filesystem::path& path) {
    const AlgebraicGraphChannel channel{7601,1,100};const auto base=graph_history_example::snapshot(0);
    write_algebraic_graph_snapshot(base,path/"file.rfcxh",FilePublication::CreateNew);
    check(encode_algebraic_graph_snapshot(read_algebraic_graph_snapshot(path/"file.rfcxh"))==encode_algebraic_graph_snapshot(base),"regular snapshot file differs");
    rejects([&]{write_algebraic_graph_snapshot(base,path/"file.rfcxh",FilePublication::CreateNew);},"create-new replaced graph file");
    const auto before=read_file_exact(path/"file.rfcxh",64u<<20);AlgebraicGraphSnapshotLimits tiny;tiny.max_source_coordinates=1;
    rejects([&]{write_algebraic_graph_snapshot(base,path/"file.rfcxh",FilePublication::ReplaceRegular,tiny);},"invalid graph replacement published");
    check(read_file_exact(path/"file.rfcxh",64u<<20)==before,"failed file publication changed bytes");
    rejects([&]{(void)AlgebraicGraphDurableHistory::create(path/"not-created",base,channel,9510,0,tiny);},"invalid graph genesis admitted");
    check(!std::filesystem::exists(path/"not-created"),"graph admission created filesystem before validation");
    auto initial=with_algebraic_graph_snapshot(TemporalEndpoint(base.source()),channel,base);
    auto bad=initial;bad.sequence=1;auto data=encode_algebraic_graph_snapshot(base);data.back()^=std::byte{1};
    bad.declarations->values.front().value=algebraic_archive_history_detail::chunks(data);
    auto valid_again=initial;valid_again.sequence=2;
    {auto raw=DurableNativeStore::create(path/"invalid-lineage",initial,9511);
        const std::vector<std::vector<std::byte>> frames{encode_temporal_segment(make_temporal_segment(initial,bad)),encode_temporal_segment(make_temporal_segment(bad,valid_again))};
        (void)raw.append(1,raw.root(),frames);
        const auto past=AlgebraicGraphHistoryWindow::capture(raw.snapshot(),0,channel);check(past.cutoff()==0,"future invalid opaque graph changed selected past");}
    rejects([&]{(void)AlgebraicGraphDurableHistory::open(path/"invalid-lineage",channel);},"valid latest graph hid corrupt middle graph");
    {auto raw=DurableNativeStore::create(path/"invalid-base",bad,9512);(void)raw.root();}
    rejects([&]{(void)AlgebraicGraphDurableHistory::open(path/"invalid-base",channel);},"opaque native graph certified as typed graph");
    rejects([&]{(void)AlgebraicGraphDurableHistory::open(path/"invalid-base",{0,1,100});},"invalid channel reached durable recovery");
}
static void large(const std::filesystem::path& path) {
    auto original=graph_history_example::graph(0);auto c=original.contents();auto contents=original.archive(1).contents();
    const auto value=std::get<rfc::local::AlgebraicModuleVector>(original.archive(1).at(10));
    for(std::uint64_t i=0;i<3500;++i)contents.records.push_back({1000+i,value});
    c.archives.front().value=AlgebraicArchive::declare(original.archive(1).bundle(),std::move(contents));
    const auto base=Snapshot::anchor_only(AlgebraicArchiveGraph::declare(std::move(c)),1);const auto data=encode_algebraic_graph_snapshot(base);
    check(data.size()>128u*1024u,"graph did not exercise the multi-Integer channel");const AlgebraicGraphChannel channel{7601,1,100};
    auto endpoint=with_algebraic_graph_snapshot(TemporalEndpoint(base.source()),channel,base);
    check(encode_algebraic_graph_snapshot(read_algebraic_graph_snapshot(endpoint,channel))==data,"large graph chunks lost exact bytes");
    auto bad=endpoint;auto& chunks=std::get<rfc::composite::DeclaredValue::Repeated>(bad.declarations->values.front().value.data).elements;
    chunks.front()=rfc::composite::DeclaredValue::integer(algebraic_archive_history_detail::pack(std::span<const std::byte>(data).first(8)));
    rejects([&]{(void)read_algebraic_graph_snapshot(bad,channel);},"short interior graph chunk accepted");
    {auto history=AlgebraicGraphDurableHistory::create(path,base,channel,9513);(void)history.root();}
    auto reopened=AlgebraicGraphDurableHistory::open(path,channel);check(bytes(reopened.at(0))==data,"large graph durable replay differs");
}
int main(){try{Temporary directory;durable(directory.path/"history");grouped(directory.path/"grouped");validation(directory.path);large(directory.path/"large");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
