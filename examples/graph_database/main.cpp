#include "model.hpp"
#include "rfc/database/algebraic_graph_database.hpp"
#include "rfc/database/algebraic_grade_tensor_window.hpp"
#include "rfc/database/prepared_algebraic_query.hpp"
#include "rfc/storage/algebraic_archive_graph_durable.hpp"
#include <iostream>

using namespace rfc::database;
using namespace rfc::storage;
static void coefficients(const rfc::numeric::AlgebraicElement& value) {
    std::cout<<'[';bool first=true;
    for (const auto& q:value.coordinates()) {if (!first) std::cout<<',';first=false;std::cout<<'"'<<q.get_str()<<'"';}
    std::cout<<']';
}
static void tensor(const AlgebraicGradeTensor& value) {
    std::cout<<'[';
    for (std::size_t i=0;i<value.domain().logical_size();++i) {if (i) std::cout<<',';coefficients(value.coordinate(i));}
    std::cout<<']';
}
static void observe(const AlgebraicGraphRevision& revision,rfc::runtime::CpuTileExecutor& cpu) {
    const rfc::runtime::WorkAvailability available{revision.source_namespace(),revision.sequence()};
    const auto database=AlgebraicGraphDatabase::select(revision,available);
    for (const auto& binding:revision.snapshot().archive_bindings()) {
        const auto source=database.archive(binding.archive,available);const auto record=source.record(10);
        AlgebraicQueryRequest request;request.kind=AlgebraicRecordKind::Vector;request.source_carrier=record.source_carrier();
        request.all_of={{{0},AlgebraicEquality::Equal,record.coefficients()[0]}};
        const auto operation=PreparedAlgebraicQuery::prepare(source,request,available,rfc::runtime::ExecutionLease{},1);
        AlgebraicQueryCache cache(16);const auto cold=cache.execute(operation,available,cpu),warm=cache.execute(operation,available,cpu);
        const auto field=AlgebraicGradeTensor::select(source,rfc::core::Grade{1},{30},available);
        const auto boundary=field.boundary(available);const auto rank=source.archive().bundle().stalk({rfc::core::Grade{1},0}).rank();
        const auto window=AlgebraicGradeTensorWindow::prepare(boundary,rfc::numeric::IndexDomain({2,rank}),{0,0},{2,rank},available);
        std::cout<<"{\"namespace\":"<<source.source_namespace()<<",\"sequence\":"<<source.sequence()
                 <<",\"archive\":"<<binding.archive<<",\"bundle\":"<<source.graph_origin()->bundle_key
                 <<",\"anchor\":"<<revision.snapshot().anchor_bundle()<<",\"query_keys\":[";
        for (std::size_t i=0;i<warm.records().size();++i) {if (i) std::cout<<',';std::cout<<warm.records()[i].key();}
        std::cout<<"],\"cache_hit\":"<<(warm.cache_hit()?"true":"false")
                 <<",\"native_endpoint_retained\":"<<(warm.source().source_publication()==revision.source_publication()?"true":"false")
                 <<",\"input\":";tensor(field);std::cout<<",\"boundary\":";tensor(boundary);
        std::cout<<",\"shape\":[2,"<<rank<<"],\"stalk_coordinates\":[";
        for (std::size_t i=0;i<window.supported_count();++i) {
            if (i) std::cout<<',';
            const auto coordinate=window.source_coordinate(i);
            std::cout<<'['<<coordinate.cell.grade.value<<','<<coordinate.cell.index<<','<<coordinate.coordinate<<']';
        }
        std::cout<<"],\"coefficient_sum\":";coefficients(*window.sum(available));
        std::cout<<",\"retained_occurrences\":"<<source.archive().bundle().source().grade(rfc::core::Grade{1}).incidence_count()
                 <<",\"derived\":"<<(boundary.derived()?"true":"false")<<"}\n";
        if (cold.records().size()!=1 || warm.records().size()!=1 || !warm.cache_hit()) throw std::logic_error("exact graph query/cache observation failed");
    }
    const auto left=database.archive(1,available),right=database.archive(2,available);
    const auto left_record=left.record(10),right_record=right.record(10);
    const auto input=std::get<rfc::local::AlgebraicModuleVector>(left_record.value());
    const auto actual=revision.snapshot().graph().carrier(1).apply(input);
    if (actual.coordinates()!=right_record.coefficients()) throw std::logic_error("declared graph comparison lost database owners");
}
int main(int argc,char** argv) {try {
    if (argc==3 && std::string(argv[1])=="--export") {
        const std::filesystem::path path(argv[2]);
        if (!std::filesystem::create_directory(path)) throw std::invalid_argument("export directory must be new");
        for (std::uint64_t i=0;i<3;++i) write_algebraic_graph_snapshot(graph_database_example::snapshot(i),path/("revision-"+std::to_string(40+i)+".rfcxh"));
        return 0;
    }
    if (argc!=2) throw std::invalid_argument("usage: graph_database NEW_STORE_DIRECTORY or --export NEW_DIRECTORY");
    const auto owned=[&] {
        const auto base=graph_database_example::snapshot(0);const AlgebraicGraphChannel channel{9611,1,2};
        auto history=AlgebraicGraphDurableHistory::create(argv[1],base,channel,9601,40);
        const std::vector<AlgebraicGraphSnapshot> group{graph_database_example::snapshot(1),graph_database_example::snapshot(2)};
        const auto receipt=history.append(7,history.root(),group);
        if (receipt.root.sequence!=42) throw std::logic_error("grouped graph publication failed");
        std::vector<AlgebraicGraphRevision> revisions;
        for (std::uint64_t i=40;i<=42;++i) revisions.push_back(history.at(i));
        return std::pair{history.prefix(41),std::move(revisions)};
    }();
    rfc::runtime::CpuTileExecutor cpu({2,2});
    for (const auto& revision:owned.second) observe(revision,cpu);
    const auto database=AlgebraicGraphDatabase::select(owned.first.at(41),{9601,41});
    std::cout<<"{\"prefix_cutoff\":"<<owned.first.cutoff()<<",\"prefix_revisions\":"<<owned.first.revision_count()
             <<",\"selected_sequence\":"<<database.revision().sequence()<<",\"archive_count\":"<<database.archive_count()<<"}\n";
} catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
