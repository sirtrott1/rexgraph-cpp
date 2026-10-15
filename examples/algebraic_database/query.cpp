#include "rfc/database/prepared_algebraic_query.hpp"
#include <sstream>
#include <iostream>

int main() {
    using namespace rfc;
    const auto archive=storage::read_algebraic_archive("measures.rfcxa");
    const auto snapshot=database::AlgebraicSnapshot::publish(archive,71,0);
    const runtime::WorkAvailability cutoff{71,0};
    database::AlgebraicQueryRequest request;request.kind=database::AlgebraicRecordKind::Map;
    request.all_of={{{1,0},database::AlgebraicEquality::Equal,archive.bundle().field().scalar(0)}};
    const auto operation=database::PreparedAlgebraicQuery::prepare(snapshot,request,cutoff);
    runtime::CpuTileExecutor cpu;database::AlgebraicQueryCache cache(8);
    const auto cold=cache.execute(operation,cutoff,cpu),warm=cache.execute(operation,cutoff,cpu);
    if (cold.records().size()!=1 || cold.records().front().key()!=2 || !warm.cache_hit()) throw std::runtime_error("exact query/cache example differs");
    const auto pin=warm.records().front();
    const auto window=database::AlgebraicTensorWindow::prepare(pin,{0,0},{2,2},cutoff);
    const auto sum=window.sum(cutoff);
    std::ostringstream report;
    report<<"{\"selected_key\":"<<pin.key()<<",\"supported_coordinates\":"<<window.supported_count()
        <<",\"field_degree\":"<<archive.bundle().field().degree()<<",\"sum\":[";
    for (std::size_t i=0;i<sum->coordinates().size();++i) { if (i) report<<',';report<<'"'<<sum->coordinates()[i]<<'"'; }
    report<<"],\"prepared_cache_hit\":true}\n";
    const auto text=report.str();storage::write_file_exact("query.json",std::as_bytes(std::span(text.data(),text.size())));
    storage::write_algebraic_archive(snapshot.archive(),"measures-copy.rfcxa",storage::FilePublication::CreateNew);
    std::cout<<text;
}
