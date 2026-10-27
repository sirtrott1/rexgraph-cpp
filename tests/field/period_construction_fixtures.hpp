#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/field/period_construction.hpp"
#include <array>
namespace period_fixtures {
using namespace rfc;using Q=numeric::Rational;using I=numeric::Integer;using Z=numeric::IntegerMatrix;using M=numeric::ExactMatrix;
struct PeriodDeclaration {
    core::RelationalComplex source;
    Z cycles;
    M cochains;
    field::ExactMetric metric;
    Z::SparseVector fundamental;
    std::vector<field::PeriodCupTerm> cup;
};
inline std::uint32_t vertex(std::uint32_t x,std::uint32_t y){return 3*y+x;}
inline std::uint32_t horizontal(std::uint32_t x,std::uint32_t y){return 3*y+x;}
inline std::uint32_t vertical(std::uint32_t x,std::uint32_t y){return 9+3*y+x;}

inline PeriodDeclaration square_torus(Q horizontal_metric=Q{1},bool fill_fundamental_cycle=false,Q incidence_scale=Q{1}){
    core::RelationSignature signature;
    const auto edge=signature.add_type("torus-edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto square=signature.add_attaching_loop_type("torus-square",4,4);
    const auto filling=signature.add_type("retained-three-cell",core::BoundaryPolicy::DeclaredExact,9,9);
    core::RelationalComplexBuilder builder(signature);
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x)
        builder.add_grade_one(edge,{{vertex(x,y),true,true,false,incidence_scale},{vertex((x+1)%3,y),true,false,false,incidence_scale}});
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x)
        builder.add_grade_one(edge,{{vertex(x,y),true,true,false,incidence_scale},{vertex(x,(y+1)%3),true,false,false,incidence_scale}});
    std::vector<field::PeriodCupTerm> cup;
    Z::SparseVector fundamental;
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x){
        builder.add_higher(core::Grade{2},square,{{horizontal(x,y),true,false,false,incidence_scale},
            {vertical((x+1)%3,y),true,false,false,incidence_scale},{horizontal(x,(y+1)%3),true,true,false,incidence_scale},
            {vertical(x,y),true,true,false,incidence_scale}});
        const auto face=vertex(x,y);const std::size_t start=4*face;
        fundamental.emplace(face,I{1});
        cup.push_back({face,start,start+1,Q{1}});
        cup.push_back({face,start+3,start+2,Q{-1}});
    }
    if(fill_fundamental_cycle){
        std::vector<core::HigherIncidenceInput> faces;
        for(std::uint32_t face=0;face<9;++face)faces.push_back({face,true,false,false,incidence_scale});
        builder.add_higher(core::Grade{3},filling,faces);
    }
    Z cycles(18,2);M cochains(18,2);numeric::RationalVector diagonal(18,Q{1});
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x){
        cochains.set(horizontal(x,y),0,Q{1,3});cochains.set(vertical(x,y),1,Q{1,3});
        diagonal[horizontal(x,y)]=horizontal_metric;
    }
    for(std::uint32_t k=0;k<3;++k){cycles.set(horizontal(k,0),0,I{1});cycles.set(vertical(0,k),1,I{1});}
    return {builder.freeze(),std::move(cycles),std::move(cochains),field::ExactMetric::diagonal(std::move(diagonal)),
        std::move(fundamental),std::move(cup)};
}

inline Z::SparseVector connection(std::uint32_t x,std::uint32_t y){
    Z::SparseVector chain;
    for(std::uint32_t k=0;k<x;++k)chain.emplace(horizontal(k,0),I{1});
    for(std::uint32_t k=0;k<y;++k)chain.emplace(vertical(x,k),I{1});
    return chain;
}

inline PeriodDeclaration genus_two(bool coupled){
    core::RelationSignature signature;
    const auto edge=signature.add_type("genus-two-loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto polygon=signature.add_attaching_loop_type("genus-two-face",8,8);
    core::RelationalComplexBuilder builder(signature);
    for(std::uint32_t k=0;k<4;++k)builder.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    builder.add_higher(core::Grade{2},polygon,{{0,true,false,false,1},{2,true,false,false,1},
        {0,true,true,false,1},{2,true,true,false,1},{1,true,false,false,1},{3,true,false,false,1},
        {1,true,true,false,1},{3,true,true,false,1}});
    M metric=M::identity(4);
    if(coupled)for(std::size_t block=0;block<2;++block){
        const auto start=2*block;
        metric.set(start,start,Q{2,3});metric.set(start+1,start+1,Q{2,3});
        metric.set(start,start+1,Q{-1,3});metric.set(start+1,start,Q{-1,3});
    }
    return {builder.freeze(),Z::identity(4),M::identity(4),field::ExactMetric(std::move(metric)),{{0,I{1}}},
        {{0,0,1,Q{1}},{0,3,2,Q{-1}},{0,4,5,Q{1}},{0,7,6,Q{-1}}}};
}

inline PeriodDeclaration four_square_row(){
    const std::array<std::size_t,4> right{1,2,3,0},up{1,0,2,3};
    std::array<std::size_t,16> parent{};for(std::size_t i=0;i<16;++i)parent[i]=i;
    const auto root=[&](std::size_t x){while(parent[x]!=x)x=parent[x];return x;};
    const auto join=[&](std::size_t a,std::size_t b){parent[root(a)]=root(b);};
    const auto corner=[](std::size_t s,std::size_t x,std::size_t y){return 4*s+2*x+y;};
    for(std::size_t s=0;s<4;++s)for(std::size_t t=0;t<2;++t){join(corner(s,1,t),corner(right[s],0,t));join(corner(s,t,1),corner(up[s],t,0));}
    std::map<std::size_t,std::uint32_t> ids;
    const auto vertex_id=[&](std::size_t s,std::size_t x,std::size_t y){const auto r=root(corner(s,x,y));return ids.emplace(r,static_cast<std::uint32_t>(ids.size())).first->second;};
    core::RelationSignature signature;const auto edge=signature.add_type("row-edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_attaching_loop_type("row-face",4,4);core::RelationalComplexBuilder b(signature);
    for(std::size_t s=0;s<4;++s)b.add_grade_one(edge,{{vertex_id(s,0,0),true,true,false,1},{vertex_id(s,1,0),true,false,false,1}});
    for(std::size_t s=0;s<4;++s)b.add_grade_one(edge,{{vertex_id(s,0,0),true,true,false,1},{vertex_id(s,0,1),true,false,false,1}});
    Z::SparseVector fundamental;std::vector<field::PeriodCupTerm> cup;
    for(std::size_t s=0;s<4;++s){b.add_higher(core::Grade{2},face,{{static_cast<std::uint32_t>(s),true,false,false,1},
        {static_cast<std::uint32_t>(4+right[s]),true,false,false,1},{static_cast<std::uint32_t>(up[s]),true,true,false,1},{static_cast<std::uint32_t>(4+s),true,true,false,1}});
        fundamental.emplace(s,I{1});cup.push_back({static_cast<std::uint32_t>(s),4*s,4*s+1,Q{1}});cup.push_back({static_cast<std::uint32_t>(s),4*s+3,4*s+2,Q{-1}});}
    return {b.freeze(),Z(8,0),M(8,0),field::ExactMetric::identity(8),std::move(fundamental),std::move(cup)};
}
// Extra components exercise profile admission without changing H1/cup data.
inline PeriodDeclaration decorated(int mode){
    core::RelationSignature signature;const auto loop=signature.add_type("loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto extra=signature.add_type("retained-extra",core::BoundaryPolicy::DeclaredExact,2,4);
    const auto face=signature.add_attaching_loop_type("face",8,8);core::RelationalComplexBuilder b(signature);
    for(std::uint32_t e=0;e<4;++e)b.add_grade_one(loop,{{0,true,true,false,1},{0,true,false,false,1}});
    if(mode==0)b.add_grade_one(extra,{{1,true,true,false,1},{2,true,false,false,1}});
    if(mode==1)b.add_grade_one(extra,{{1,true,true,false,1},{2,true,false,false,1},{1,true,false,false,0},{1,false,false,false,0}});
    if(mode==2)b.add_grade_one(extra,{{1,true,true,false,2},{2,true,false,false,2}});
    b.add_higher(core::Grade{2},face,{{0,true,false,false,1},{2,true,false,false,1},{0,true,true,false,1},{2,true,true,false,1},
        {1,true,false,false,1},{3,true,false,false,1},{1,true,true,false,1},{3,true,true,false,1}});
    return {b.freeze(),Z(5,0),M(5,0),field::ExactMetric::identity(5),{{0,I{1}}},
        {{0,0,1,Q{1}},{0,3,2,Q{-1}},{0,4,5,Q{1}},{0,7,6,Q{-1}}}};
}
inline PeriodDeclaration killed_loops(bool torsion,bool bad_higher=false,bool empty_higher=false){
    core::RelationSignature signature;const auto loop=signature.add_type("loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_attaching_loop_type("commutator",4,4);
    const auto disk=signature.add_type("disk",core::BoundaryPolicy::DeclaredExact,1,1);core::RelationalComplexBuilder b(signature);
    for(std::uint32_t e=0;e<2;++e)b.add_grade_one(loop,{{0,true,true,false,1},{0,true,false,false,1}});
    b.add_higher(core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{0,true,true,false,1},{1,true,true,false,1}});
    b.add_higher(core::Grade{2},disk,{{0,true,false,false,torsion?2:1}});b.add_higher(core::Grade{2},disk,{{1,true,false,false,1}});
    if(empty_higher)b.declare_grade(core::Grade{4});
    if(bad_higher)b.add_higher(core::Grade{3},disk,{{1,true,false,false,1}});
    return {b.freeze(),Z(2,0),M(2,0),field::ExactMetric::identity(2),{{0,I{1}}},{{0,0,1,Q{1}},{0,3,2,Q{-1}}}};
}
inline field::PreparedIntegralPeriodConstruction construct(PeriodDeclaration d,numeric::ExactActionGuard& g){
    return field::PreparedIntegralPeriodConstruction::prepare(std::move(d.source),std::move(d.metric),std::move(d.fundamental),std::move(d.cup),
        field::PeriodCoordinateBasis::CoordinatedIntegralLattice,92,1,g);
}
} // namespace period_fixtures
