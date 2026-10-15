#pragma once
#include "rfc/storage/algebraic_archive_graph_codec.hpp"
#include "rfc/core/builder.hpp"

namespace algebraic_graph_example {
using Q=rfc::numeric::Rational;
using E=rfc::numeric::AlgebraicElement;
using F=rfc::numeric::CertifiedAlgebraicField;
using B=rfc::local::AlgebraicStalkBundle;
using M=rfc::local::AlgebraicModuleMap;
using C=rfc::local::AlgebraicCarrierComparison;
using S=rfc::numeric::AlgebraicFieldEmbedding;
using D=rfc::field::AlgebraicMetricDomain;
using DC=rfc::field::AlgebraicMetricDomainComparison;
using A=rfc::storage::AlgebraicArchive;
using Graph=rfc::storage::AlgebraicArchiveGraph;
using G=rfc::core::Grade;
inline rfc::core::RelationalComplex source() {
    rfc::core::RelationSignature signature;
    const auto pair=signature.add_type("pair",rfc::core::BoundaryPolicy::DeclaredExact,2,0);
    rfc::core::RelationalComplexBuilder builder(std::move(signature));
    builder.declare_witness_identity(10);builder.declare_witness_identity(11);
    builder.add_grade_one(pair,{{10,true,true,1},{11,true,false,1},{10,false,false,7},{11,true,false,0}});
    return builder.freeze();
}
inline B bundle(const rfc::core::RelationalComplex& source,const F& field,std::size_t rank) {
    return B::declare(source,field,{{rank,rank},{rank}});
}
inline rfc::local::AlgebraicRelationalFiberSystem fibers(const B& bundle) {
    std::vector<rfc::local::AlgebraicOccurrenceMap> maps;
    const auto& arena=bundle.source().grade(G{1});
    for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
        const auto upper=bundle.stalk({G{1},arena.upper_index[slot]}),lower=bundle.stalk({G{0},arena.lower_index[slot]});
        const auto identity=M::identity(upper);
        maps.push_back({G{1},slot,M::declare(upper,lower,identity.coefficients())});
    }
    return rfc::local::AlgebraicRelationalFiberSystem::declare(bundle,maps,rfc::local::AlgebraicChainPolicy::RequireFlat);
}
inline A archive(const B& bundle,const D& domain,const rfc::local::AlgebraicModuleVector& value,
    const rfc::local::AlgebraicRelationalFiberSystem& fibers) {
    rfc::storage::AlgebraicArchiveContents contents;contents.metric_domain=domain;contents.fibers=fibers;
    contents.records={{10,value},{20,rfc::field::AlgebraicModuleMetric::identity(domain,value.carrier())}};
    return A::declare(bundle,std::move(contents));
}
inline Graph make() {
    const auto f=F::declare({1,0,1},rfc::numeric::IrreducibleReduction{3});
    const auto l=F::declare({1,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    const auto real=F::declare({-2,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto duplicate=F::declare({1,0,1},rfc::numeric::IrreducibleReduction{3});
    const auto fi=rfc::numeric::AlgebraicInvolution::declare(f,-f.generator());
    const auto li=rfc::numeric::AlgebraicInvolution::declare(l,l.generator().inverse());
    const auto ri=rfc::numeric::AlgebraicInvolution::identity(real);
    const auto di=rfc::numeric::AlgebraicInvolution::declare(duplicate,-duplicate.generator());
    const auto fr=rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(f,{0,1},Q{1,4});
    const auto lr=rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(l,{Q{11,16},Q{11,16}},Q{1,8});
    const auto rr=rfc::geometry::SelectedRealAlgebraicEmbedding::isolate(real,1,Q{3,2});
    const auto dr=rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(duplicate,{0,1},Q{1,4});
    const auto fd=D::hermitian(fr,fi),ld=D::hermitian(lr,li),rd=D::real(rr,ri),dd=D::hermitian(dr,di);
    const auto retained=source();
    const auto b1=bundle(retained,f,2),b2=bundle(retained,l,3),b3=bundle(retained,l,3),b4=bundle(retained,f,3),
        b5=bundle(retained,duplicate,2),b6=bundle(retained,real,1);
    const auto u1=b1.stalk({G{0},0}),u2=b2.stalk({G{0},0}),u3=b3.stalk({G{0},0}),u4=b4.stalk({G{0},0}),
        u5=b5.stalk({G{0},0}),u6=b6.stalk({G{0},0});
    const auto z=l.generator(),o=l.scalar(1),q=l.scalar(0),fo=f.scalar(1),fq=f.scalar(0);
    const auto s1=S::declare(f,l,z*z),s2=S::identity(l),s3=S::declare(f,duplicate,duplicate.generator()),
        s4=S::declare(duplicate,f,f.generator()),s5=S::declare(real,l,z-z.power(3)),s6=S::identity(f);
    const auto c1=C::declare(s1,u1,u2,{z,q,q,l.scalar(Q{3,5}),q,l.scalar(Q{4,5})});
    const auto c2=C::declare(s2,u2,u3,{o,q,o,q,o,z,q,q,o});
    const auto c3=C::declare(s3,u1,u5,{duplicate.scalar(1),duplicate.scalar(0),duplicate.scalar(0),duplicate.scalar(1)});
    const auto c4=C::declare(s6,u1,u4,{fo,fq,fq,fo,fo,fo}),c5=C::declare(s6,u4,u1,{fo,fq,fq,fq,fo,fq});
    const auto c6=C::declare(s4,u5,u1,{fo,fq,fq,fo}),c7=C::declare(s5,u6,u2,{z,q,q});
    const auto x=u1.vector({fo+f.generator(),fq});
    const auto f1=fibers(b1),f2=fibers(b2),f3=fibers(b3),f4=fibers(b4),f5=fibers(b5),f6=fibers(b6);
    const auto a1=archive(b1,fd,x,f1),a2=archive(b2,ld,c1.apply(x),f2),a3=archive(b3,ld,c2.apply(c1.apply(x)),f3),
        a4=archive(b1,fd,x,f1),a5=archive(b5,dd,c3.apply(x),f5),a6=archive(b6,rd,u6.vector({real.scalar(1)+real.generator()}),f6);
    rfc::storage::AlgebraicArchiveGraphContents c;
    c.fields={{1,f},{2,l},{3,real},{4,duplicate}};c.involutions={{1,fi},{2,li},{3,ri},{4,di}};
    c.embeddings={{1,fr},{2,lr},{3,rr},{4,dr}};c.domains={{1,fd},{2,ld},{3,rd},{4,dd}};
    c.bundles={{1,b1},{2,b2},{3,b3},{4,b4},{5,b5},{6,b6}};c.fibers={{1,f1},{2,f2},{3,f3},{4,f4},{5,f5},{6,f6}};
    c.archives={{1,a1},{2,a2},{3,a3},{4,a4},{5,a5},{6,a6}};
    c.scalars={{1,s1},{2,s2},{3,s3},{4,s4},{5,s5},{6,s6}};
    c.carriers={{1,c1},{2,c2},{3,c3},{4,c4},{5,c5},{6,c6},{7,c7}};
    c.metric_comparisons={{1,DC::declare(s1,fd,ld)},{2,DC::declare(s3,fd,dd)},{3,DC::declare(s5,rd,ld)}};
    c.reconstructions={{1,4,5},{2,3,6}};c.measurements={{1,1,1,1,20,2,20},{2,2,3,1,20,5,20},{3,3,7,6,20,2,20}};
    return Graph::declare(std::move(c));
}
} // namespace algebraic_graph_example
