#pragma once
#include "rfc/field/finite_jacobian_normalization.hpp"
#include "square_tiled_surface_fixtures.hpp"
#include "period_construction_fixtures.hpp"
namespace normalization_fixtures {
using namespace rfc;using Q=numeric::Rational;using I=numeric::Integer;using Z=numeric::IntegerMatrix;using M=numeric::ExactMatrix;
using A=numeric::SparseAlgebraicMatrix;using E=numeric::AlgebraicElement;using F=numeric::CertifiedAlgebraicField;using Guard=numeric::ExactActionGuard;
using Embedding=geometry::SelectedRealAlgebraicEmbedding;using Frequency=field::DeclaredPeriodFrequency;
using Normalization=field::PreparedFiniteJacobianNormalization;
struct Case {
    std::string name;field::PreparedIntegralSymplecticBasis symplectic;Embedding embedding;std::vector<Frequency> frequencies;
};
inline F rational_field(){return F::declare_cyclotomic(1);}
inline Embedding rational_embedding(const F& f){return Embedding::isolate(f,Q{0},Q{2});}
inline Case rational_surface(std::size_t fixture,std::vector<Q> roots,Guard& g){
    const auto input=surface_fixtures::cases()[fixture];const auto s=surface_fixtures::surface(input,g);const auto periods=s.integral_periods(g);const auto f=rational_field();
    std::vector<Frequency> frequencies;for(const auto& root:roots)frequencies.push_back({f.scalar(Q{root*root}),f.scalar(root)});
    return {input.name,periods.symplectic(),rational_embedding(f),std::move(frequencies)};
}
inline Case radical_surface(std::size_t fixture,Q radicand,std::uint64_t prime,Q lower,Q upper,Guard& g){
    const auto input=surface_fixtures::cases()[fixture];const auto s=surface_fixtures::surface(input,g);const auto periods=s.integral_periods(g);
    const auto f=radicand.get_den()==1?F::declare({-radicand,Q{0},Q{1}},numeric::EisensteinShift{prime,I{0}}):F::declare({-radicand,Q{0},Q{1}},numeric::IrreducibleReduction{13});
    return {input.name,periods.symplectic(),Embedding::isolate(f,lower,upper),{{f.scalar(Q{1}),f.scalar(Q{1})},{f.scalar(radicand),f.generator()}}};
}
inline std::vector<Case> cases(Guard& g){
    std::vector<Case> out;
    for(std::size_t i=0;i<3;++i)out.push_back(rational_surface(i,{Q{1}},g));
    {
        const auto s=surface_fixtures::surface(surface_fixtures::cases()[2],g);numeric::RationalVector weights(s.work().edges,Q{1});
        for(std::size_t i=0;i<s.work().faces;++i)weights[i]=Q{1,3};
        const auto periods=s.integral_periods(field::ExactMetric::diagonal(weights),g);
        const auto f=F::declare({Q{-3},Q{0},Q{1}},numeric::EisensteinShift{3,I{0}});
        out.push_back({"torus-anisotropic",periods.symplectic(),Embedding::isolate(f,Q{1},Q{2}),{{f.scalar(Q{3}),f.generator()}}});
    }
    {
        const auto s=surface_fixtures::surface(surface_fixtures::cases()[1],g);const auto periods=s.integral_periods(field::ExactMetric::diagonal(numeric::RationalVector(s.work().edges,Q{2})),g);const auto f=rational_field();
        out.push_back({"torus-metric-two",periods.symplectic(),rational_embedding(f),{{f.scalar(Q{1,4}),f.scalar(Q{1,2})}}});
    }
    out.push_back(rational_surface(3,{Q{1},Q{2}},g));out.push_back(rational_surface(4,{Q{1},Q{13,8}},g));
    out.push_back(radical_surface(5,Q{3},3,Q{1},Q{2},g));out.push_back(radical_surface(6,Q{95,44},5,Q{1},Q{2},g));
    out.push_back(radical_surface(7,Q{5},5,Q{2},Q{3},g));out.push_back(radical_surface(8,Q{105,41},3,Q{1},Q{2},g));
    out.push_back(radical_surface(9,Q{2},2,Q{1},Q{2},g));
    for(std::uint32_t n=1;n<=2;++n){
        auto input=surface_fixtures::cases()[10];input.n=n;input.name=n==1?"staircase-n1":"staircase-n2";
        const auto s=surface_fixtures::surface(input,g);const auto periods=s.integral_periods(g);const auto f=F::declare({Q{-5},Q{0},Q{1}},numeric::EisensteinShift{5,I{0}});
        const auto root=f.generator();const auto plus=n==1?root+f.scalar(Q{1}):(root*f.scalar(Q{13})+f.scalar(Q{11}))/f.scalar(Q{16});
        const auto minus=n==1?root-f.scalar(Q{1}):(root*f.scalar(Q{13})-f.scalar(Q{11}))/f.scalar(Q{16});
        out.push_back({input.name,periods.symplectic(),Embedding::isolate(f,Q{2},Q{3}),{{f.scalar(Q{1}),f.scalar(Q{1})},{plus*plus,plus},{minus*minus,minus}}});
    }
    out.push_back(rational_surface(12,{Q{1},Q{2}},g));
    for(const bool coupled:{false,true}){
        const auto construction=period_fixtures::construct(period_fixtures::genus_two(coupled),g);const auto f=rational_field();
        std::vector<Frequency> frequencies{{f.scalar(Q{1}),f.scalar(Q{1})}};if(coupled)frequencies.push_back({f.scalar(Q{9}),f.scalar(Q{3})});
        out.push_back({coupled?"native-coupled-genus-two":"native-repeated-rank-four",field::PreparedIntegralSymplecticBasis::prepare(construction.lattice(),g),rational_embedding(f),std::move(frequencies)});
    }
    const auto zero=period_fixtures::construct(period_fixtures::killed_loops(false),g);const auto f=rational_field();
    out.push_back({"zero-H1",field::PreparedIntegralSymplecticBasis::prepare(zero.lattice(),g),rational_embedding(f),{}});
    return out;
}
} // namespace normalization_fixtures
