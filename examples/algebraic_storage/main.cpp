#include "model.hpp"
#include <iostream>

using namespace archive_example;
static void qs(const rfc::numeric::RationalVector& values) {
    std::cout<<'['; for (std::size_t i=0;i<values.size();++i) {if (i) std::cout<<',';std::cout<<'"'<<values[i].get_str()<<'"';} std::cout<<']';
}
static void matrix(const rfc::numeric::ExactMatrix& value) {
    std::cout<<'['; for (std::size_t i=0;i<value.rows();++i) {if (i) std::cout<<',';std::cout<<'[';
        for (std::size_t j=0;j<value.cols();++j) {if (j) std::cout<<',';std::cout<<'"'<<value.get(i,j).get_str()<<'"';}std::cout<<']';} std::cout<<']';
}
static void hex(const std::vector<std::byte>& bytes) {
    constexpr char digits[]="0123456789abcdef";
    for (const auto byte:bytes) {const auto n=std::to_integer<unsigned>(byte);std::cout<<digits[n>>4u]<<digits[n&15u];}
}
static void profile(const Case& sample,const std::filesystem::path& directory,bool deformed=false) {
    const auto original=make(sample,deformed);const auto bytes=rfc::storage::encode_algebraic_archive(original);
    const auto restored=rfc::storage::decode_algebraic_archive(bytes);
    const auto old=original.bundle().stalk({G{0},0}),now=restored.bundle().stalk(old.cell());
    const auto scalars=rfc::numeric::AlgebraicFieldEmbedding::declare(old.field(),now.field(),now.field().generator());
    const auto accession=identity_comparison(scalars,old,now);
    const auto reconstruction=identity_comparison(scalars.inverse(),now,old);
    const auto maps=rfc::local::AlgebraicReconstruction::declare(accession,reconstruction);
    const auto& x=std::get<rfc::local::AlgebraicModuleVector>(original.at(10));
    const auto& saved=std::get<rfc::local::AlgebraicModuleVector>(restored.at(10));
    const auto compared=accession.apply(x);
    if (compared.coordinates()!=saved.coordinates()) throw std::runtime_error("saved vector comparison differs");
    const auto& metric=std::get<Metric>(restored.at(50)); const auto solved=metric.solve(saved);
    if (!directory.empty()) {
        std::filesystem::create_directories(directory);
        const auto path=directory/(std::string(sample.name)+(deformed?"-deformed":"")+".rfcxa");
        rfc::storage::write_algebraic_archive(original,path,rfc::storage::FilePublication::CreateNew);
        if (rfc::storage::encode_algebraic_archive(rfc::storage::read_algebraic_archive(path))!=bytes)
            throw std::runtime_error("archive file roundtrip differs");
    }
    std::cout<<"{\"name\":\""<<sample.name<<"\",\"deformed\":"<<(deformed?"true":"false")<<",\"archive_hex\":\"";
    hex(bytes);
    std::cout<<"\",\"fresh_owners\":"<<(!old.same_owner(now) && !old.field().same_owner(now.field())?"true":"false")<<",\"accession\":";
    matrix(maps.accession().rational_action());std::cout<<",\"reconstruction\":";matrix(maps.reconstruction().rational_action());
    std::cout<<",\"saved_vector\":";qs(saved.rational_coordinates());std::cout<<",\"metric_solve\":";qs(solved.rational_coordinates());
    std::cout<<",\"metric_action\":";matrix(metric.measurement().coefficient_form().rational_action());
    std::cout<<",\"fiber_boundaries\":[";
    for (std::size_t k=1;k<restored.bundle().source().grade_count();++k) {if (k>1) std::cout<<',';
        matrix(restored.contents().fibers->rational_boundary(G{static_cast<std::uint16_t>(k)}));}
    std::cout<<"],\"fiber_flat\":"<<(restored.contents().fibers->flat()?"true":"false")<<"}\n";
}
static void redundant(const std::filesystem::path& directory) {
    const auto sample=cases()[1];const auto& f=sample.domain.field();const auto source=cellular_example::source(false,Q{1});
    std::vector<std::vector<std::size_t>> ranks;
    for (std::size_t k=0;k<source.grade_count();++k) ranks.emplace_back(source.grade(G{static_cast<std::uint16_t>(k)}).cell_count(),2);
    ranks[0][1]=3;const auto bundle=rfc::local::AlgebraicStalkBundle::declare(source,f,ranks);
    const auto u=bundle.stalk({G{0},0}),v=bundle.stalk({G{0},1});const auto one=f.scalar(Q{1}),zero=f.scalar(Q{0});
    const auto a=Map::declare(u,v,{one,zero,zero,one,one,one}),r=Map::declare(v,u,{one,zero,zero,zero,one,zero});
    const auto m=metric(sample.domain,u);const auto represented=m.measurement().pullback(r);
    rfc::storage::AlgebraicArchiveContents contents;contents.metric_domain=sample.domain;
    contents.records={{10,a},{20,r},{30,m},{40,represented}};contents.reconstructions={{1,10,20}};
    const auto original=Archive::declare(bundle,std::move(contents));const auto bytes=rfc::storage::encode_algebraic_archive(original);
    const auto restored=rfc::storage::decode_algebraic_archive(bytes);const auto pair=restored.reconstruction(1);const auto projector=pair.physical_projector();
    if (!directory.empty()) rfc::storage::write_algebraic_archive(original,directory/"redundant-reconstruction.rfcxa",rfc::storage::FilePublication::CreateNew);
    std::cout<<"{\"name\":\"redundant_reconstruction\",\"archive_hex\":\"";hex(bytes);
    std::cout<<"\",\"fresh_owners\":"<<(!original.bundle().same_owner(restored.bundle())?"true":"false")<<",\"projector\":";
    matrix(projector.rational_action());std::cout<<"}\n";
}
int main(int argc,char** argv) {
    try {const std::filesystem::path directory=argc==2?argv[1]:"";if (argc>2) throw std::invalid_argument("usage: algebraic_storage [new-output-directory]");
        const auto samples=cases();for (const auto& sample:samples) profile(sample,directory);profile(samples[2],directory,true);redundant(directory);}
    catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
