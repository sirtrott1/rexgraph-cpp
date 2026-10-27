#pragma once
#include "rfc/field/elliptic_factor_construction.hpp"
#include "finite_normalization_fixtures.hpp"
namespace factor_fixtures {
using namespace normalization_fixtures;
using Factors=field::PreparedFiniteEllipticFactors;
inline std::vector<Case> cases(Guard& g){
    auto out=normalization_fixtures::cases(g);
    core::RelationSignature signature;const auto edge=signature.add_type("three-handle-loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_attaching_loop_type("three-handle-face",12,12);core::RelationalComplexBuilder builder(signature);
    for(std::uint32_t i=0;i<6;++i)builder.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    std::vector<core::HigherIncidenceInput> flags;std::vector<field::PeriodCupTerm> cup;
    for(std::uint32_t i=0;i<3;++i){flags.push_back({i,true,false,false,1});flags.push_back({3+i,true,false,false,1});flags.push_back({i,true,true,false,1});flags.push_back({3+i,true,true,false,1});
        cup.push_back({0,4*i,4*i+1,Q{1}});cup.push_back({0,4*i+3,4*i+2,Q{-1}});}
    builder.add_higher(core::Grade{2},face,flags);
    const auto construction=field::PreparedIntegralPeriodConstruction::prepare(builder.freeze(),field::ExactMetric::diagonal({Q{1},Q{1,2},Q{1,3},Q{1},Q{1,2},Q{1,3}}),{{0,I{1}}},cup,field::PeriodCoordinateBasis::CoordinatedIntegralLattice,106,1,g);
    const auto f=rational_field();out.push_back({"native-three-factor",field::PreparedIntegralSymplecticBasis::prepare(construction.lattice(),g),rational_embedding(f),{{f.scalar(Q{1}),f.scalar(Q{1})},{f.scalar(Q{4}),f.scalar(Q{2})},{f.scalar(Q{9}),f.scalar(Q{3})}}});
    return out;
}
inline bool admitted(const Case& c){return c.name!="native-repeated-rank-four"&&!c.name.starts_with("staircase");}
} // namespace factor_fixtures
