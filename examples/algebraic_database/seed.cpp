#include "rfc/storage/algebraic_archive_codec.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>

int main() {
    using namespace rfc;
    const auto f=numeric::CertifiedAlgebraicField::declare({1,0,1},numeric::IrreducibleReduction{3});
    const auto alpha=f.generator(),one=f.scalar(1),zero=f.scalar(0);
    core::RelationSignature signature;
    const auto pair=signature.add_type("pair",core::BoundaryPolicy::DeclaredExact,2,0);
    core::RelationalComplexBuilder builder(std::move(signature));
    builder.declare_witness_identity(10);builder.declare_witness_identity(11);
    builder.add_grade_one(pair,{{10,true,true,1},{11,true,false,1},{10,false,false,7}});
    const auto bundle=local::AlgebraicStalkBundle::declare(builder.freeze(),f,{{2,2},{2}});
    const auto u=bundle.stalk({core::Grade{0},0}),v=bundle.stalk({core::Grade{0},1});
    const auto domain=field::AlgebraicMetricDomain::hermitian(
        geometry::SelectedComplexAlgebraicEmbedding::isolate(f,{0,1},numeric::Rational{1,4}),numeric::AlgebraicInvolution::declare(f,-alpha));
    storage::AlgebraicArchiveContents contents;contents.metric_domain=domain;
    contents.records={{1,u.vector({one+alpha,zero})},{2,local::AlgebraicModuleMap::declare(u,v,{one,alpha,zero,one})},
        {3,local::AlgebraicCovector::declare(u,{alpha,one})},{4,field::AlgebraicModuleMetric::identity(domain,u)}};
    std::vector<local::AlgebraicOccurrenceMap> maps;
    const auto& arena=bundle.source().grade(core::Grade{1});
    for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
        const auto source=bundle.stalk({core::Grade{1},arena.upper_index[slot]}),target=bundle.stalk({core::Grade{0},arena.lower_index[slot]});
        const auto id=local::AlgebraicModuleMap::identity(source);
        maps.push_back({core::Grade{1},slot,local::AlgebraicModuleMap::declare(source,target,id.coefficients())});
    }
    contents.fibers=local::AlgebraicRelationalFiberSystem::declare(bundle,maps,local::AlgebraicChainPolicy::RequireFlat);
    storage::write_algebraic_archive(storage::AlgebraicArchive::declare(bundle,std::move(contents)),"measures.rfcxa",storage::FilePublication::CreateNew);
    std::cout<<"Saved measures.rfcxa: four typed records over Q(i), including a supported zero and an inactive occurrence.\n";
}
