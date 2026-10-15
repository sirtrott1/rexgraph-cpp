#include "rfc/database/prepared_algebraic_query.hpp"
#include <iostream>

using namespace rfc::database;
static void sizes(const std::vector<std::size_t>& values) {
    std::cout<<'[';for (std::size_t i=0;i<values.size();++i) { if (i) std::cout<<',';std::cout<<values[i]; } std::cout<<']';
}
static void element(const rfc::numeric::AlgebraicElement& value) {
    std::cout<<'[';for (std::size_t i=0;i<value.coordinates().size();++i) { if (i) std::cout<<',';std::cout<<'"'<<value.coordinates()[i]<<'"'; }std::cout<<']';
}
static void keys(const DerivedAlgebraicQuery& result) {
    std::cout<<'[';for (std::size_t i=0;i<result.records().size();++i) { if (i) std::cout<<',';std::cout<<result.records()[i].key(); }std::cout<<']';
}
int main(int argc,char** argv) {
    try {
        if (argc!=2) throw std::invalid_argument("usage: algebraic_database archive.rfcxa");
        const auto archive=rfc::storage::read_algebraic_archive(argv[1]);
        const auto snapshot=AlgebraicSnapshot::publish(archive,71,9);
        const rfc::runtime::WorkAvailability cutoff{71,9};
        rfc::runtime::CpuTileExecutor cpu({2,2});
        const auto all=PreparedAlgebraicQuery::prepare(snapshot,{},cutoff,rfc::runtime::ExecutionLease{},1);
        const auto result=all.execute(cutoff,cpu);
        std::cout<<"{\"phase\":\"inventory\",\"degree\":"<<archive.bundle().field().degree()<<",\"keys\":";keys(result);
        std::cout<<",\"sequence\":"<<snapshot.sequence()<<"}\n";
        for (const auto& pin:result.records()) {
            const auto domain=pin.domain();
            std::cout<<"{\"phase\":\"record\",\"key\":"<<pin.key()<<",\"kind\":"<<static_cast<unsigned>(pin.kind())<<",\"shape\":";sizes(domain.extents());
            std::cout<<",\"coefficients\":[";
            for (std::size_t i=0;i<pin.coefficients().size();++i) { if (i) std::cout<<',';element(pin.coefficients()[i]); }
            std::cout<<"]}\n";
            std::vector<std::vector<std::size_t>> lower={{}},upper={{}};
            for (const auto extent:domain.extents()) {
                std::vector<std::vector<std::size_t>> next_lo,next_hi;
                for (std::size_t row=0;row<lower.size();++row) for (std::size_t a=0;a<=extent;++a) for (auto b=a;b<=extent;++b) {
                    auto lo=lower[row],hi=upper[row];lo.push_back(a);hi.push_back(b);next_lo.push_back(std::move(lo));next_hi.push_back(std::move(hi));
                }
                lower=std::move(next_lo);upper=std::move(next_hi);
            }
            for (std::size_t box=0;box<lower.size();++box) {
                const auto window=AlgebraicTensorWindow::prepare(pin,lower[box],upper[box],cutoff);
                std::cout<<"{\"phase\":\"window\",\"key\":"<<pin.key()<<",\"lower\":";sizes(lower[box]);std::cout<<",\"upper\":";sizes(upper[box]);
                std::cout<<",\"indices\":[";for (std::size_t i=0;i<window.supported_count();++i) { if (i) std::cout<<',';std::cout<<window.source_index(i); }
                std::cout<<"],\"sum\":";const auto sum=window.sum(cutoff);if (sum) element(*sum);else std::cout<<"null";std::cout<<"}\n";
            }
            const auto coordinate=domain.unflatten(domain.logical_size()-1);
            AlgebraicQueryRequest request;request.kind=pin.kind();request.source_carrier=pin.source_carrier();request.target_carrier=pin.target_carrier();
            request.all_of={{coordinate,AlgebraicEquality::Equal,pin.coefficients().back()},{coordinate,AlgebraicEquality::Equal,pin.coefficients().back()}};
            const auto query=PreparedAlgebraicQuery::prepare(snapshot,request,cutoff,rfc::runtime::ExecutionLease{},1);
            AlgebraicQueryCache cache(64);const auto cold=cache.execute(query,cutoff,cpu),warm=cache.execute(query,cutoff,cpu);
            std::cout<<"{\"phase\":\"query\",\"record\":"<<pin.key()<<",\"coordinate\":";sizes(coordinate);
            std::cout<<",\"literal\":";element(pin.coefficients().back());std::cout<<",\"keys\":";keys(cold);
            std::cout<<",\"cache_hit\":"<<(warm.cache_hit()?"true":"false")<<",\"clauses\":"<<warm.request().all_of.size()<<"}\n";
        }
        if (archive.contents().fibers) {
            const auto& source=archive.bundle().source();
            for (std::size_t k=1;k<source.grade_count();++k) {
                const auto grade=rfc::core::Grade{static_cast<std::uint16_t>(k)};
                for (std::size_t slot=0;slot<source.grade(grade).incidence_count();++slot) {
                    const auto leg=snapshot.occurrence(grade,slot);const auto weighted=leg.weighted_map(),contribution=leg.contribution_map();
                    std::cout<<"{\"phase\":\"occurrence\",\"grade\":"<<k<<",\"slot\":"<<slot<<",\"active\":"<<(leg.active?"true":"false")
                        <<",\"coefficient\":\""<<leg.coefficient<<"\",\"underlying\":[";
                    for (std::size_t i=0;i<leg.underlying.coefficients().size();++i) { if (i) std::cout<<',';element(leg.underlying.coefficients()[i]); }
                    std::cout<<"],\"weighted\":[";for (std::size_t i=0;i<weighted.coefficients().size();++i) { if (i) std::cout<<',';element(weighted.coefficients()[i]); }
                    std::cout<<"],\"contribution\":[";for (std::size_t i=0;i<contribution.coefficients().size();++i) { if (i) std::cout<<',';element(contribution.coefficients()[i]); }
                    std::cout<<"]}\n";
                }
            }
        }
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
