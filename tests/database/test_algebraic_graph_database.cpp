#include "rfc/database/algebraic_graph_database.hpp"
#include "rfc/database/prepared_algebraic_query.hpp"
#include "../../examples/graph_database/model.hpp"
#include "rfc/storage/temporal_segments.hpp"
#include <iostream>

using namespace rfc::database;
using namespace rfc::storage;
using G=rfc::core::Grade;
static std::size_t checks{};
static void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
template<class F> static void rejects(F f,const char* message) {
    bool rejected=false;try {f();} catch (const std::exception&) {rejected=true;}check(rejected,message);
}
static AlgebraicGraphRevision selected(std::uint64_t value=0) {
    const AlgebraicGraphChannel channel{9621,1,2};const auto graph=graph_database_example::snapshot(value);
    const auto endpoint=with_algebraic_graph_snapshot(TemporalEndpoint(graph.source(),27),channel,graph);
    const NativeTemporalSeries series(endpoint,9602);return AlgebraicGraphRevision::capture(series.at(27),channel);
}
static void database() {
    const auto revision=selected(),foreign=selected();const rfc::runtime::WorkAvailability cutoff{9602,27};
    const auto database=AlgebraicGraphDatabase::select(revision,cutoff),copy=database;
    check(database.same_owner(copy) && database.archive_count()==6,"database lost stable selected revision");
    check(database.revision().same_owner(revision),"database redecoded graph into unrelated owners");
    rfc::runtime::CpuTileExecutor cpu({3,3});AlgebraicQueryCache cache(100);
    for (const auto& binding:revision.snapshot().archive_bindings()) {
        const auto source=database.archive(binding.archive,cutoff),again=copy.archive(binding.archive,cutoff);
        check(source.same_publication(again),"repeated graph archive selection changed database publication");
        check(source.archive().same_owner(revision.snapshot().graph().archive(binding.archive)),"database copied archive into fresh owners");
        check(source.source_publication()==revision.source_publication(),"selected native endpoint pin lost");
        check(source.source_namespace()==9602 && source.sequence()==27,"database provenance differs");
        const auto origin=source.graph_origin();
        check(origin && origin->revision.same_owner(revision) && origin->archive_key==binding.archive && origin->bundle_key==binding.bundle,"complete graph association lost");
        check(origin->revision.snapshot().anchor_bundle()==1 && origin->revision.snapshot().source_bindings().size()==6,"graph source declarations omitted");
        const auto operation=PreparedAlgebraicQuery::prepare(source,{},cutoff,rfc::runtime::ExecutionLease{},1),copied=operation;
        const auto result=cache.execute(operation,cutoff,cpu),hit=cache.execute(copied,cutoff,cpu);
        check(result.records().size()==4 && !result.cache_hit() && hit.cache_hit(),"canonical graph query/cache failed");
        for (const auto& record:result.records()) {
            check(record.graph_origin()==origin && record.source_publication()==revision.source_publication(),"query record lost graph or endpoint pin");
            check(record.source_carrier().same_owner(algebraic_database_detail::source(source.archive().at(record.key()))),"query changed actual stalk owner");
            const auto shape=record.domain();const auto window=AlgebraicTensorWindow::prepare(record,std::vector<std::size_t>(shape.rank(),0),shape.extents(),cutoff);
            check(window.record().graph_origin()==origin && window.record().source_publication()==revision.source_publication(),"coefficient window lost graph bindings");
            for (std::size_t i=0;i<shape.logical_size();++i) check(window.coordinate(i)==record.coefficients()[i],"graph coefficient window changed full tuple");
        }
        rejects([&]{(void)cache.execute(operation,{9602,26},cpu);},"cached graph future cutoff accepted");
        rejects([&]{(void)operation.execute({9999,27},cpu);},"graph query foreign namespace accepted");
        auto request=AlgebraicQueryRequest{};const auto pin=source.record(10);
        request.kind=AlgebraicRecordKind::Vector;request.source_carrier=pin.source_carrier();
        request.all_of={{{0},AlgebraicEquality::Equal,pin.coefficients()[0]},{{0},AlgebraicEquality::Equal,pin.coefficients()[0]}};
        const auto precise=PreparedAlgebraicQuery::prepare(source,request,cutoff);
        request.all_of[0].operation=AlgebraicEquality::NotEqual;
        const auto answer=precise.execute(cutoff,cpu);
        check(answer.records().size()==1 && answer.records()[0].key()==10 && precise.request().all_of.size()==2,"source request mutated or repeated predicates lost");
        const auto separate=AlgebraicGraphDatabase::select(foreign,cutoff).archive(binding.archive,cutoff);
        request={};request.source_carrier=separate.record(10).source_carrier();
        rejects([&]{(void)PreparedAlgebraicQuery::prepare(source,request,cutoff);},"foreign graph carrier accepted by equal source bytes");
        const auto restored=PreparedAlgebraicQuery::prepare(separate,{},cutoff);
        check(!cache.execute(restored,cutoff,cpu).cache_hit(),"fresh independent graph hit old operation cache");
    }
    const auto one=database.archive(1,cutoff),shared=database.archive(4,cutoff),other=database.archive(5,cutoff);
    check(one.archive().bundle().same_owner(shared.archive().bundle()),"within-graph bundle sharing lost");
    check(!one.archive().bundle().same_owner(other.archive().bundle()) && !one.archive().bundle().field().same_owner(other.archive().bundle().field()),"equal independent fields were interned");
    check(!one.same_publication(shared),"equal shared-carrier records identified distinct archive publications");
    const auto literal=one.record(10);auto request=AlgebraicQueryRequest{};request.kind=AlgebraicRecordKind::Vector;
    request.all_of={{{0},AlgebraicEquality::Equal,literal.coefficients()[0]}};
    rejects([&]{(void)PreparedAlgebraicQuery::prepare(other,request,cutoff);},"independent equal field literal accepted implicitly");
    const auto left=one.record(10),right=database.archive(2,cutoff).record(10);
    const auto mapped=revision.snapshot().graph().carrier(1).apply(std::get<rfc::local::AlgebraicModuleVector>(left.value()));
    check(mapped.carrier().same_owner(right.source_carrier()) && mapped.coordinates()==right.coefficients(),"canonical cross-field map cannot consume selected query carriers");
    rejects([&]{(void)database.archive(0,cutoff);},"missing graph archive became supported zero");
    rejects([&]{(void)AlgebraicSnapshot::select_graph_archive(revision,99,cutoff);},"false archive name accepted");
    rejects([&]{(void)database.archive(1,{9602,26});},"stable archive selection skipped availability check");
    rejects([&]{(void)AlgebraicGraphDatabase::select(revision,{9602,26});},"future graph database selected");
    rejects([&]{(void)AlgebraicGraphDatabase::select(revision,{9603,27});},"foreign graph database namespace selected");
    rfc::runtime::ExecutionHostPolicy policy;policy.limits.records=1;
    rejects([&]{(void)AlgebraicSnapshot::select_graph_archive(revision,1,cutoff,rfc::runtime::ExecutionLease(policy));},"archive pin admission skipped");
    rejects([&]{(void)AlgebraicGraphDatabase::select(revision,cutoff,rfc::runtime::ExecutionLease(policy));},"database pin admission skipped");
    policy={};policy.backend=rfc::runtime::ExecutionBackend::ExactGpu;
    rejects([&]{(void)database.archive(1,cutoff,rfc::runtime::ExecutionLease(policy));},"unsupported database backend accepted");
    auto stopped=std::make_shared<std::atomic_bool>(true);const rfc::runtime::ExecutionLease cancelled({},stopped);
    rejects([&]{(void)database.archive(1,cutoff,cancelled);},"database cancellation skipped");
    auto moved=database;const auto retained=std::move(moved);
    rejects([&]{(void)moved.archive_count();},"moved database accessed empty state");
    check(retained.same_owner(database),"moving database changed owner");
}
static void lifetime_and_cutoff() {
    const rfc::runtime::WorkAvailability cutoff{9602,27};
    const auto pin=[&] {const auto database=AlgebraicGraphDatabase::select(selected(),cutoff);return database.archive(1,cutoff).record(10);}();
    const auto& graph=pin.graph_origin()->revision.snapshot().graph();
    const auto vector=std::get<rfc::local::AlgebraicModuleVector>(pin.value());
    const auto result=graph.carrier(1).apply(vector);
    check(result.carrier().same_owner(graph.bundle(2).stalk({G{0},0})),"escaped record no longer shares graph carriers");
    check(pin.source_publication()->sequence==27 && graph.contents().bundles.size()==6,"escaped record lost native source or graph owner table");
    const auto window=[&] {const auto db=AlgebraicGraphDatabase::select(selected(),cutoff);return AlgebraicTensorWindow::prepare(db.archive(1,cutoff).record(10),{0},{2},cutoff);}();
    check(window.record().graph_origin() && window.supported_count()==2 && !window.coordinate(0).is_zero(),"escaped tensor pin dangles");
    const auto first=graph_database_example::snapshot(0),next=graph_database_example::snapshot(1);const AlgebraicGraphChannel channel{9621,1,2};
    const auto base=with_algebraic_graph_snapshot(TemporalEndpoint(first.source(),27),channel,first);
    auto later=with_algebraic_graph_snapshot(TemporalEndpoint(next.source(),28),channel,next);
    NativeTemporalSeries series(base,9602);series.append(make_temporal_segment(base,later));
    auto corrupt=later;corrupt.sequence=29;
    corrupt.declarations->values.front().value=rfc::composite::DeclaredValue::repeated({rfc::composite::DeclaredValue(algebraic_archive_history_detail::pack(std::vector<std::byte>{std::byte{0}}))});
    series.append(make_temporal_segment(later,corrupt));
    const auto prefix=AlgebraicGraphHistoryWindow::capture(series,28,channel);
    const auto old=AlgebraicGraphDatabase::select(prefix.at(27),cutoff).archive(1,cutoff);
    const auto old_record=old.record(10);
    check(old_record.coefficients()[0].coordinates()==rfc::numeric::RationalVector({0,1}),"selected past source substituted later graph values");
    check(old.graph_origin()->revision.source_publication()->sequence==27 && prefix.native_prefix().cutoff()==28,"selected graph retained a later publication");
    rejects([&]{(void)AlgebraicGraphDatabase::select(prefix.at(28),cutoff);},"selected graph exceeded query availability");
    rejects([&]{(void)AlgebraicGraphHistoryWindow::capture(series,29,channel);},"corrupt selected future graph accepted");
    const auto standalone=AlgebraicSnapshot::publish(old.archive(),9602,27);
    check(!standalone.graph_origin() && !standalone.source_publication(),"standalone labels claimed retained graph endpoint");
}
int main() {try {database();lifetime_and_cutoff();std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";}
    catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
