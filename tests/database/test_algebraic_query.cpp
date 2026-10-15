#include "rfc/database/prepared_algebraic_query.hpp"
#include "rfc/storage/temporal_segments.hpp"
#include <iostream>

using namespace rfc::database;
using namespace rfc::storage;
static std::size_t checks{};
static void check(bool value,const char* message) { ++checks;if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F f,const char* message) {
    bool rejected=false;try { f(); } catch (const std::exception&) { rejected=true; }check(rejected,message);
}
static void profile(const std::filesystem::path& path) {
    const auto archive=read_algebraic_archive(path),foreign=read_algebraic_archive(path);
    const auto s=AlgebraicSnapshot::publish(archive,91,12),t=AlgebraicSnapshot::publish(foreign,91,12);
    const rfc::runtime::WorkAvailability cutoff{91,12};
    rfc::runtime::CpuTileExecutor cpu({4,4});
    const auto query=PreparedAlgebraicQuery::prepare(s,{},cutoff,rfc::runtime::ExecutionLease{},1),copy=query;
    AlgebraicQueryCache cache(4096);const auto first=cache.execute(query,cutoff,cpu),second=cache.execute(copy,cutoff,cpu);
    check(first.records().size()==archive.contents().records.size(),"unfiltered query lost named records");
    check(!first.cache_hit() && second.cache_hit() && second.cpu_report().tiles==0,"same operation cache missed or reran");
    check(cache.applications_hits()==std::pair<std::size_t,std::size_t>{1,1},"cache counters differ");
    const auto separate=PreparedAlgebraicQuery::prepare(t,{},cutoff,rfc::runtime::ExecutionLease{},1);
    check(!cache.execute(separate,cutoff,cpu).cache_hit(),"equal archive restored owner hit old cache");
    check(!cache.execute(PreparedAlgebraicQuery::prepare(s,{},cutoff,rfc::runtime::ExecutionLease{},1),cutoff,cpu).cache_hit(),"separate operation identified by answer");
    rejects([&]{(void)cache.execute(query,{91,11},cpu);},"cached future publication leaked");
    rejects([&]{(void)query.execute({92,12},cpu);},"foreign namespace cutoff accepted");
    rejects([&]{(void)AlgebraicSnapshot::publish(archive,0,12);},"empty source namespace accepted");
    auto stopped=std::make_shared<std::atomic_bool>(true);const rfc::runtime::ExecutionLease cancelled({},stopped);
    rejects([&]{(void)cache.execute(query,cutoff,cpu,cancelled);},"cached cancellation ignored");
    rfc::runtime::ExecutionHostPolicy policy;policy.limits.records=0;
    rejects([&]{(void)PreparedAlgebraicQuery::prepare(s,{},cutoff,rfc::runtime::ExecutionLease(policy));},"query record admission ignored");
    rejects([&]{(void)cache.execute(query,cutoff,cpu,rfc::runtime::ExecutionLease(policy));},"cached admission ignored");
    policy={};policy.backend=rfc::runtime::ExecutionBackend::ExactGpu;
    rejects([&]{(void)cache.execute(query,cutoff,cpu,rfc::runtime::ExecutionLease(policy));},"unsupported GPU query accepted");
    rejects([&]{(void)PreparedAlgebraicQuery::prepare(s,{},cutoff,rfc::runtime::ExecutionLease{},0);},"zero query tile size accepted");
    rejects([&]{(void)s.record(0);},"missing record became supported zero");
    for (const auto& pin:first.records()) {
        check(pin.coefficients_pin()->size()==pin.domain().logical_size(),"coefficient tensor omitted supported zero");
        check(pin.source_carrier().same_owner(algebraic_database_detail::source(archive.at(pin.key()))),"query lost actual carrier");
        AlgebraicQueryRequest request;request.kind=pin.kind();request.source_carrier=pin.source_carrier();request.target_carrier=pin.target_carrier();
        const auto coordinate=pin.domain().unflatten(pin.domain().logical_size()-1);
        request.all_of={{coordinate,AlgebraicEquality::Equal,pin.coefficients().back()},{coordinate,AlgebraicEquality::Equal,pin.coefficients().back()}};
        const auto prepared=PreparedAlgebraicQuery::prepare(s,request,cutoff,rfc::runtime::ExecutionLease{},1);const auto result=prepared.execute(cutoff,cpu);
        check(prepared.request().all_of.size()==2,"ordered repeated predicates deduplicated");
        check(std::any_of(result.records().begin(),result.records().end(),[&](const auto& r){return r.key()==pin.key();}),"exact coefficient equality loses own record");
        request.all_of[1].operation=AlgebraicEquality::NotEqual;
        check(PreparedAlgebraicQuery::prepare(s,request,cutoff).execute(cutoff,cpu).records_pin()->empty(),"contradictory predicates matched");
        request.all_of[0].literal=foreign.bundle().field().scalar(0);
        rejects([&]{(void)PreparedAlgebraicQuery::prepare(s,request,cutoff);},"foreign field literal accepted");
        request={};request.source_carrier=t.record(pin.key()).source_carrier();
        rejects([&]{(void)PreparedAlgebraicQuery::prepare(s,request,cutoff);},"foreign carrier accepted by equal cell name");
        request={};request.kind=static_cast<AlgebraicRecordKind>(99);
        rejects([&]{(void)PreparedAlgebraicQuery::prepare(s,request,cutoff);},"unknown record kind accepted");
        request={};request.kind=pin.kind();request.all_of={{coordinate,static_cast<AlgebraicEquality>(99),pin.coefficients().back()}};
        rejects([&]{(void)PreparedAlgebraicQuery::prepare(s,request,cutoff);},"unknown equality accepted");
        request.all_of[0].operation=AlgebraicEquality::Equal;request.all_of[0].coordinate={999};
        rejects([&]{(void)PreparedAlgebraicQuery::prepare(s,request,cutoff);},"invalid coefficient coordinate silently skipped");
        const auto shape=pin.domain();const std::vector<std::size_t> lower(shape.rank(),0);
        const auto window=AlgebraicTensorWindow::prepare(pin,lower,shape.extents(),cutoff);
        check(window.supported_count()==pin.coefficients().size(),"full declared block became nonzero support");
        auto expected=archive.bundle().field().scalar(0);
        for (std::size_t i=0;i<window.supported_count();++i) {
            check(window.source_index(i)==i && window.window_index(i)==i,"full tensor coordinate realization differs");
            check(window.coordinate(i)==pin.coefficients()[i],"complete field coordinates changed");expected=expected+pin.coefficients()[i];
        }
        check(window.sum(cutoff)==expected,"exact coefficient contraction differs");
        const auto empty=AlgebraicTensorWindow::prepare(pin,lower,lower,cutoff);
        check(!empty.sum(cutoff) && empty.supported_count()==0,"empty box collapsed into supported zero");
        rejects([&]{(void)empty.coordinate(0);},"empty window has coordinate");
        rejects([&]{(void)window.coordinate(shape.logical_size());},"window coordinate boundary ignored");
        rejects([&]{(void)window.sum({91,11});},"tensor future publication leaked");
        rejects([&]{(void)window.sum(cutoff,cancelled);},"tensor cancellation ignored");
        auto invalid=shape.extents();++invalid.front();
        rejects([&]{(void)AlgebraicTensorWindow::prepare(pin,lower,invalid,cutoff);},"tensor box bounds ignored");
        rejects([&]{(void)AlgebraicTensorWindow::prepare(pin,{},shape.extents(),cutoff);},"tensor box rank ignored");
        policy={};policy.limits.integer_bits=0;
        rejects([&]{(void)window.sum(cutoff,rfc::runtime::ExecutionLease(policy));},"tensor integer envelope ignored");
    }
    // Owning pins survive destruction of the selecting snapshot and query.
    const auto escaped=[&]{ const auto local=AlgebraicSnapshot::publish(read_algebraic_archive(path),91,12);return local.record(first.records().front().key()); }();
    check(escaped.coefficients().size()==first.records().front().coefficients().size(),"escaped value pin dangles");
    const AlgebraicArchiveChannel channel{501,1,2};
    TemporalEndpoint base(archive.bundle().source());base.sequence=7;base=with_algebraic_archive(base,channel,archive);
    NativeTemporalSeries series(base,91);
    auto endpoint=base;endpoint.sequence=8;series.append(make_temporal_segment(base,endpoint));
    // The suffix is well-formed native storage but an invalid algebraic archive.
    auto future=endpoint;future.sequence=9;
    future.declarations->values.front().value=rfc::composite::DeclaredValue::repeated({rfc::composite::DeclaredValue(algebraic_archive_history_detail::pack(std::vector<std::byte>{std::byte{0}}))});
    series.append(make_temporal_segment(endpoint,future));
    const auto prefix=AlgebraicHistoryWindow::select(series,7,8,{91,8},channel);
    check(prefix.snapshots().size()==2 && prefix.snapshots().back().sequence()==8,"selected history inspected future archive suffix");
    check(!prefix.snapshots()[0].archive().bundle().same_owner(prefix.snapshots()[1].archive().bundle()),"history implicitly interned independent restores");
    check(prefix.snapshots()[0].source_publication()->sequence==7,"history publication pin changed");
    rejects([&]{(void)AlgebraicHistoryWindow::select(series,7,9,{91,8},channel);},"history future cutoff leaked");
    rejects([&]{(void)AlgebraicHistoryWindow::select(series,7,9,{91,9},channel);},"invalid suffix decoded as archive");
    rejects([&]{(void)AlgebraicHistoryWindow::select(series,7,8,{92,8},channel);},"history namespace mismatch accepted");
    rejects([&]{(void)AlgebraicHistoryWindow::select(series,7,8,{91,8},channel,{},1);},"history snapshot admission ignored");
    check(encode_algebraic_archive(prefix.snapshots().front().archive())==encode_algebraic_archive(archive),"selected native archive byte reconstruction differs");
}
int main(int argc,char** argv) { try {
    if (argc!=2) throw std::invalid_argument("archive fixture directory required");
    for (const auto* name:{"real_cubic","hermitian_quadratic","hermitian_quartic","hermitian_octic","hermitian_quartic-deformed","redundant-reconstruction"})
        profile(std::filesystem::path(argv[1])/(std::string(name)+".rfcxa"));
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
