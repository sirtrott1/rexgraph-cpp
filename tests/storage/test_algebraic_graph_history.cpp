#include "../../examples/graph_history/model.hpp"
#include "rfc/storage/algebraic_archive_graph_history.hpp"
#include <iostream>

using namespace rfc::storage;
using namespace graph_history_example;
using G=rfc::core::Grade;
static std::size_t checks{};
static void check(bool condition,const char* message) {++checks;if (!condition)throw std::runtime_error(message);}
template<class F> static void rejects(F action,const char* message) {
    bool failed=false;try {action();}catch(const std::exception&){failed=true;}check(failed,message);
}
static void u64(std::vector<std::byte>& bytes,std::size_t pos,std::uint64_t value) {
    for(std::size_t i=0;i<8;++i)bytes.at(pos+i)=std::byte((value>>(8*i))&255u);
}
static void rehash(std::vector<std::byte>& bytes) {u64(bytes,16,bytes.size());u64(bytes,24,rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32)));}
static void snapshots(const Snapshot& base,const Snapshot& mapped) {
    const auto bytes=encode_algebraic_graph_snapshot(base);const auto restored=decode_algebraic_graph_snapshot(bytes);
    check(encode_algebraic_graph_snapshot(restored)==bytes,"snapshot canonical bytes differ");
    check(encode_algebraic_archive_graph(base.graph())==encode_algebraic_archive_graph(restored.graph()),"RFCXG inner bytes changed");
    check(!base.graph().field(1).same_owner(restored.graph().field(1)),"snapshot read reused coefficient owner");
    check(restored.graph().archive(1).bundle().same_owner(restored.graph().archive(4).bundle()),"shared archive bundle lost");
    check(!restored.graph().archive(1).same_owner(restored.graph().archive(4)),"independent archive owners collapsed");
    check(!restored.graph().field(1).same_owner(restored.graph().field(4)),"equal field owners collapsed");
    check(!restored.graph().bundle(2).same_owner(restored.graph().bundle(3)),"equal module owners collapsed");
    rejects([&]{(void)base.push_source_field(2,G{0},{1,2});},"equal source bytes inferred an anchor map");
    const auto current=decode_algebraic_graph_snapshot(encode_algebraic_graph_snapshot(mapped));
    check(current.push_source_field(2,G{0},{7,11})==rfc::numeric::RationalVector({7,11}),"declared source map action differs");
    const auto& graph=current.graph();const auto& x=std::get<rfc::local::AlgebraicModuleVector>(graph.archive(1).at(10));
    const auto y=graph.carrier(1).apply(x);const auto z=graph.carrier(1).then(graph.carrier(2)).apply(x);
    const auto sequential=graph.carrier(2).apply(y);
    check(y.coordinates()==std::get<rfc::local::AlgebraicModuleVector>(graph.archive(2).at(10)).coordinates(),"complete comparison replay differs");
    check(z.coordinates()==sequential.coordinates(),"chronological comparison order changed");
    const auto projector=graph.reconstruction(1).physical_projector();
    check(projector.rational_action().multiply(projector.rational_action())==projector.rational_action(),"complete reconstruction projector lost");
    rejects([&]{(void)restored.graph().carrier(1).apply(x);},"separate snapshot reads inferred carrier identity");
    auto sources=base.source_bindings();sources.pop_back();
    rejects([&]{(void)Snapshot::declare(base.graph(),1,sources,base.archive_bindings());},"missing source binding accepted");
    sources=base.source_bindings();sources[1].bundle=1;
    rejects([&]{(void)Snapshot::declare(base.graph(),1,sources,base.archive_bindings());},"duplicate source binding accepted");
    sources=base.source_bindings();sources[0].kind=AlgebraicGraphSourceKind::Independent;sources[0].cells.clear();sources[0].occurrences.clear();
    rejects([&]{(void)Snapshot::declare(base.graph(),1,sources,base.archive_bindings());},"independent native anchor accepted");
    sources=base.source_bindings();sources[0].cells[0][0]=1;
    rejects([&]{(void)Snapshot::declare(base.graph(),1,sources,base.archive_bindings());},"nonidentity anchor accepted");
    sources=base.source_bindings();sources[0].occurrences[1][2]=0;
    rejects([&]{(void)Snapshot::declare(base.graph(),1,sources,base.archive_bindings());},"inactive occurrence map omitted");
    sources=base.source_bindings();sources[0].occurrences[1].pop_back();
    rejects([&]{(void)Snapshot::declare(base.graph(),1,sources,base.archive_bindings());},"supported-zero occurrence omitted");
    sources=base.source_bindings();sources[1].cells={{0,1},{0}};
    rejects([&]{(void)Snapshot::declare(base.graph(),1,sources,base.archive_bindings());},"independent source carried a hidden map");
    auto archives=base.archive_bindings();archives[0].bundle=2;
    rejects([&]{(void)Snapshot::declare(base.graph(),1,base.source_bindings(),archives);},"equal source substituted archive owner");
    archives=base.archive_bindings();archives.pop_back();
    rejects([&]{(void)Snapshot::declare(base.graph(),1,base.source_bindings(),archives);},"archive binding missing");
    rejects([&]{(void)Snapshot::anchor_only(base.graph(),99);},"missing anchor name accepted");
    rejects([&]{(void)Snapshot::anchor_only(AlgebraicArchiveGraph::declare({}),1);},"ownerless graph acquired anchor");
    rejects([&]{(void)base.with_source_morphism(2,{{0,1},{0,0}});},"wrong complete cell map accepted");
    AlgebraicGraphSnapshotLimits limits;limits.max_source_coordinates=0;
    rejects([&]{(void)Snapshot::anchor_only(base.graph(),1,limits);},"anchor coordinate budget ignored");
    rejects([&]{(void)base.with_source_morphism(2,{{0,1},{0}},limits);},"strict source map budget ignored");
    auto mutable_sources=base.source_bindings();auto mutable_archives=base.archive_bindings();
    const auto frozen=Snapshot::declare(base.graph(),1,mutable_sources,mutable_archives);mutable_sources.clear();mutable_archives.clear();
    check(encode_algebraic_graph_snapshot(frozen)==bytes,"caller mutation changed a frozen binding table");
    auto moved_from=base;const auto live=std::move(moved_from);
    check(live.same_owner(base) && !moved_from.same_owner(moved_from),"moved snapshot fabricated an owner");
    rejects([&]{(void)encode_algebraic_graph_snapshot(moved_from);},"moved snapshot accepted");
}
static void rectangular_source(const Snapshot& base) {
    rfc::core::RelationSignature signature;const auto pair=signature.add_type("pair",rfc::core::BoundaryPolicy::DeclaredExact,2,0);
    rfc::core::RelationalComplexBuilder builder(std::move(signature));
    for(const auto key:{10u,11u,12u})builder.declare_witness_identity(key);
    for(int i=0;i<2;++i)builder.add_grade_one(pair,{{10,true,true,1},{11,true,false,1},{10,false,false,7},{11,true,false,0}});
    auto c=base.graph().contents();const auto source=builder.freeze();
    c.bundles.push_back({99,rfc::local::AlgebraicStalkBundle::declare(source,c.fields.front().value,{{1,1,1},{1,1}})});
    auto current=Snapshot::anchor_only(AlgebraicArchiveGraph::declare(c),1).with_source_morphism(99,{{0,1,0},{0,0}});
    current=decode_algebraic_graph_snapshot(encode_algebraic_graph_snapshot(current));
    check(current.push_source_field(99,G{0},{2,3,5})==rfc::numeric::RationalVector({7,3}),"rectangular primary map lost directions");
    const rfc::numeric::RationalVector upper{7,11};
    const auto lower=current.graph().bundle(99).source().boundary_apply(G{1},upper);
    check(current.push_source_field(99,G{0},lower)==current.source().boundary_apply(G{1},current.push_source_field(99,G{1},upper)),"declared source map lost chain naturality");
    auto bindings=current.source_bindings();auto& last=bindings.back();last.occurrences[1][4]=1;
    rejects([&]{(void)Snapshot::declare(current.graph(),1,bindings,current.archive_bindings());},"strict repeated occurrence link forged");
}
static void wire(const Snapshot& sample) {
    const auto bytes=encode_algebraic_graph_snapshot(sample);
    for(std::size_t i=0;i<bytes.size();++i) {
        rejects([&]{(void)decode_algebraic_graph_snapshot(std::span<const std::byte>(bytes).first(i));},"truncated snapshot accepted");
        auto bad=bytes;bad[i]^=std::byte{1};rejects([&]{(void)decode_algebraic_graph_snapshot(bad);},"single-bit snapshot corruption accepted");
    }
    auto bad=bytes;bad.push_back(std::byte{0});rehash(bad);rejects([&]{(void)decode_algebraic_graph_snapshot(bad);},"trailing snapshot bytes accepted");
    const auto embedded=encode_algebraic_archive_graph(sample.graph()).size();const auto start=48+embedded;
    for(const auto pos:{std::size_t{32},start,start+8,start+8+9+8+8}) {
        bad=bytes;u64(bad,pos,std::numeric_limits<std::uint64_t>::max());rehash(bad);
        rejects([&]{(void)decode_algebraic_graph_snapshot(bad);},"checksum-repaired invalid anchor/map accepted");
    }
    bad=bytes;bad[start+16]=std::byte{9};rehash(bad);rejects([&]{(void)decode_algebraic_graph_snapshot(bad);},"unknown source binding kind accepted");
    bad=bytes;u64(bad,bad.size()-8,2);rehash(bad);rejects([&]{(void)decode_algebraic_graph_snapshot(bad);},"wrong registered archive owner accepted");
    for(const auto selection:{0,1,2,3,4,5}) {
        AlgebraicGraphSnapshotLimits limits;
        if(selection==0)limits.max_file_bytes=bytes.size()-1;
        if(selection==1)limits.graph.max_file_bytes=embedded-1;
        if(selection==2)limits.max_source_bindings=1;
        if(selection==3)limits.max_archive_bindings=1;
        if(selection==4)limits.max_source_coordinates=1;
        if(selection==5)limits.graph.max_rational_coordinates=1;
        rejects([&]{(void)encode_algebraic_graph_snapshot(sample,limits);},"snapshot encode admission ignored");
        rejects([&]{(void)decode_algebraic_graph_snapshot(bytes,limits);},"snapshot decode admission ignored");
    }
}
static void native(const Snapshot& base,const Snapshot& mapped) {
    const AlgebraicGraphChannel channel{7601,1,100};
    auto initial=with_algebraic_graph_snapshot(TemporalEndpoint(base.source(),40),channel,base);
    initial.declarations->values.push_back({123,channel.chunk_type(),0,0,{},{},rfc::composite::DeclaredValue::integer(42)});
    validate_temporal_endpoint(initial);auto next=with_algebraic_graph_snapshot(initial,channel,mapped);next.sequence=41;
    check(next.declarations->values.back().key==123,"other native channel removed");
    rejects([&]{(void)read_algebraic_graph_snapshot(initial,{7602,1,100});},"foreign namespace accepted");
    rejects([&]{(void)read_algebraic_graph_snapshot(initial,{7601,2,100});},"foreign type accepted");
    rejects([&]{(void)read_algebraic_graph_snapshot(initial,{7601,1,101});},"missing typed value accepted");
    rejects([&]{(void)with_algebraic_graph_snapshot(initial,{0,1,100},base);},"unnamed channel accepted");
    auto invalid=initial;invalid.declarations->values.front().type=channel.chunk_type();
    invalid.declarations->values.front().value=rfc::composite::DeclaredValue::integer(1);
    rejects([&]{(void)read_algebraic_graph_snapshot(invalid,channel);},"different value binding accepted");
    NativeTemporalSeries history(initial,9501);history.append(make_temporal_segment(initial,next));
    auto damaged=next;damaged.sequence=42;auto bytes=encode_algebraic_graph_snapshot(base);bytes.back()^=std::byte{1};
    damaged.declarations->values.front().value=algebraic_archive_history_detail::chunks(bytes);
    history.append(make_temporal_segment(next,damaged));
    const auto window=AlgebraicGraphHistoryWindow::capture(history,41,channel);
    check(window.cutoff()==41 && window.revision_count()==2 && window.native_prefix().segment_count()==1,"prefix retained future tail");
    const auto base_revision=window.at(40);
    check(encode_algebraic_graph_snapshot(base_revision.snapshot())==encode_algebraic_graph_snapshot(base),"later graph leaked into base");
    check(window.at(40).same_owner(window.at(40)),"one owned window restored multiple incoherent owner sets");
    rejects([&]{(void)window.at(42);},"future graph escaped selected prefix");
    rejects([&]{(void)window.native_prefix().at(42);},"raw prefix retained future endpoint");
    rejects([&]{(void)AlgebraicGraphHistoryWindow::capture(history,42,channel);},"typed window accepted corrupt selected graph");
    rejects([&]{(void)AlgebraicGraphHistoryWindow::capture(history,39,channel);},"cutoff before base accepted");
    rejects([&]{(void)AlgebraicGraphHistoryWindow::capture(history,43,channel);},"cutoff after tail accepted");
    AlgebraicGraphSnapshotLimits limits;limits.max_history_revisions=1;
    rejects([&]{(void)AlgebraicGraphHistoryWindow::capture(history,41,channel,limits);},"prefix revision admission ignored");
    const auto pin=[&]{const auto old=AlgebraicGraphRevision::capture(history.at(40),channel);return old.archive_pin(1);}();
    check(std::get<rfc::local::AlgebraicModuleVector>(pin->at(10)).coordinates().front().coordinates()[0]==0,"archive pin lost its revision after release");
    const auto a=AlgebraicGraphRevision::capture(history.at(40),channel),b=AlgebraicGraphRevision::capture(history.at(40),channel);
    check(!a.snapshot().graph().field(1).same_owner(b.snapshot().graph().field(1)),"independent revision reads interned fields");
    check(a.sequence()==40 && a.source_namespace()==9501 && a.source_publication()->sequence==40,"native cutoff provenance lost");
    auto moved_from=a;const auto live=std::move(moved_from);
    check(live.same_owner(a) && !moved_from.same_owner(moved_from),"moved revision fabricated an owner");
    rejects([&]{(void)moved_from.archive_pin(1);},"moved revision accepted");
    const auto foreign=algebraic_graph_example::bundle(algebraic_graph_example::source(),base.graph().field(1),1);
    auto c=base.graph().contents();c.bundles.push_back({99,foreign});
    const auto other=Snapshot::anchor_only(AlgebraicArchiveGraph::declare(c),99);
    check(read_algebraic_graph_snapshot(with_algebraic_graph_snapshot(initial,channel,other),channel).anchor_bundle()==99,"explicit different equal-source anchor rejected");
}
int main(int argc,char** argv) {try {
    const auto base=graph_history_example::snapshot(0),mapped=graph_history_example::snapshot(1,true);
    snapshots(base,mapped);rectangular_source(base);wire(base);native(base,mapped);
    if(argc==2)for(const auto& [name,value]:std::vector<std::pair<std::string,Snapshot>>{{"anchor.rfcxh",base},{"mapped.rfcxh",mapped}})
        check(read_file_exact(std::filesystem::path(argv[1])/name,64u<<20)==encode_algebraic_graph_snapshot(value),"canonical native graph fixture changed");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
