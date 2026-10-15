#include "model.hpp"
#include <iostream>

int main(int argc,char** argv) {
    using namespace algebraic_graph_example;
    if (argc!=2) {std::cerr<<"Usage: algebraic_graph output.rfcxg\n";return 2;}
    const auto original=make();
    rfc::storage::write_algebraic_archive_graph(original,argv[1],rfc::storage::FilePublication::CreateNew);
    const auto restored=rfc::storage::read_algebraic_archive_graph(argv[1]);
    if (rfc::storage::encode_algebraic_archive_graph(original)!=rfc::storage::encode_algebraic_archive_graph(restored)) return 1;
    std::cout<<"{\"phase\":\"graph\",\"fields\":"<<restored.contents().fields.size()<<",\"archives\":"<<restored.contents().archives.size()
        <<",\"proper_inclusion\":true,\"shared_archive_bundle\":"
        <<(restored.archive(1).bundle().same_owner(restored.archive(4).bundle())?"true":"false")
        <<",\"independent_equal_fields\":"<<(!restored.field(1).same_owner(restored.field(4))?"true":"false")<<"}\n";
    for (const auto& row:restored.contents().scalars) {
        std::cout<<"{\"phase\":\"scalar\",\"key\":"<<row.key<<",\"source_degree\":"<<row.value.source().degree()
            <<",\"target_degree\":"<<row.value.target().degree()<<",\"generator\":[";
        bool comma=false;for (const auto& q:row.value.generator_image().coordinates()) {if(comma)std::cout<<',';std::cout<<'"'<<q<<'"';comma=true;}
        std::cout<<"]}\n";
    }
    for (const auto& row:restored.contents().carriers) {
        std::cout<<"{\"phase\":\"carrier\",\"key\":"<<row.key<<",\"rows\":"<<row.value.target().rank()<<",\"columns\":"<<row.value.source().rank()<<",\"coefficients\":[";
        bool comma=false;for(const auto& value:row.value.coefficients()) {
            if(comma)std::cout<<',';
            std::cout<<'[';bool inner=false;
            for(const auto& q:value.coordinates()){if(inner)std::cout<<',';std::cout<<'"'<<q<<'"';inner=true;}
            std::cout<<']';comma=true;
        }
        std::cout<<"],\"rational_action\":[";comma=false;
        for(std::size_t i=0;i<row.value.rational_action().rows();++i) for(std::size_t j=0;j<row.value.rational_action().cols();++j) {
            if(comma)std::cout<<',';
            std::cout<<'"'<<row.value.rational_action().get(i,j)<<'"';comma=true;
        }
        std::cout<<"]}\n";
    }
    const auto& x=std::get<rfc::local::AlgebraicModuleVector>(restored.archive(1).at(10));
    const auto& proof=restored.measurement(1);
    if (proof.target().quadrance(proof.carrier().apply(x))!=proof.domains().scalar_comparison().apply(proof.source().quadrance(x))) return 1;
    const auto back=restored.reconstruction(1).reconstruction().apply(restored.reconstruction(1).accession().apply(x));
    if (back.coordinates()!=x.coordinates()) return 1;
    std::cout<<"{\"phase\":\"verified\",\"measurement_receipts\":"<<restored.contents().measurements.size()
        <<",\"reconstruction_receipts\":"<<restored.contents().reconstructions.size()<<",\"canonical_roundtrip\":true}\n";
}
