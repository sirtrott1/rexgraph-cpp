#pragma once
#include "rfc/storage/algebraic_archive_history.hpp"
#include "rfc/local/algebraic_reconstruction.hpp"
#include "../algebraic_metrics/model.hpp"

namespace archive_example {
using namespace metric_example;
using Archive=rfc::storage::AlgebraicArchive;
using Comparison=rfc::local::AlgebraicCarrierComparison;
inline Archive make(const Case& sample,bool deformed=false,Q offset=Q{0}) {
    const auto& domain=sample.domain; const auto& f=domain.field();
    const auto model=cellular_example::make(f,true,false,false,Q{1},deformed);
    const auto prepared=model.prepare(deformed?rfc::local::AlgebraicChainPolicy::DeclaredDeformation:rfc::local::AlgebraicChainPolicy::RequireFlat);
    const auto u=model.bundle.stalk({G{0},0}),v=model.bundle.stalk({G{0},1});
    const auto one=f.scalar(Q{1}),zero=f.scalar(Q{0}),alpha=f.generator();
    const auto x=u.vector({one+alpha+f.scalar(offset),alpha*alpha}); const auto mu=metric(domain,u),mv=metric(domain,v,Q{4});
    const auto map=Map::declare(u,v,{one,alpha,one+alpha,zero});
    const auto indefinite=Form::declare(domain,Map::declare(u,u,{one,zero,zero,-one}));
    rfc::storage::AlgebraicArchiveContents contents;
    contents.metric_domain=domain; contents.fibers=prepared.realization();
    contents.records={{10,x},{20,mu.measurement().as_covector(x)},{30,map},{40,mu.measurement()},{50,mu},
        {60,mv},{70,rfc::field::algebraic_metric_adjoint(map,mu,mv)},{80,indefinite}};
    return Archive::declare(model.bundle,std::move(contents));
}
inline Comparison identity_comparison(const rfc::numeric::AlgebraicFieldEmbedding& scalars,const Module& source,const Module& target) {
    if (source.rank()!=target.rank()) throw std::invalid_argument("example basis comparison requires equal declared ranks");
    std::vector<E> entries(source.rank()*target.rank(),target.field().scalar(Q{0}));
    for (std::size_t i=0;i<source.rank();++i) entries[i*source.rank()+i]=target.field().scalar(Q{1});
    return Comparison::declare(scalars,source,target,std::move(entries));
}
} // namespace archive_example
