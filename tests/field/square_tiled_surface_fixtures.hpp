#pragma once
#include "rfc/field/square_tiled_surface.hpp"
#include <string>
namespace surface_fixtures {
using namespace rfc;using Q=numeric::Rational;using I=numeric::Integer;using Z=numeric::IntegerMatrix;using M=numeric::ExactMatrix;using Guard=numeric::ExactActionGuard;
struct Case {std::string name;std::vector<std::uint32_t> right,up;std::uint32_t n;std::size_t genus;field::SquareTiledOrientation orientation{field::SquareTiledOrientation::Positive};};
inline std::vector<Case> cases(){return {
    {"torus-n1",{0},{0},1,1},{"torus-n2",{0},{0},2,1},{"torus-n3",{0},{0},3,1},
    {"symmetric-L-n1",{1,0,2},{2,1,0},1,2},{"symmetric-L-n2",{1,0,2},{2,1,0},2,2},
    {"asymmetric-L-n1",{1,2,0,3},{3,1,2,0},1,2},{"asymmetric-L-n2",{1,2,0,3},{3,1,2,0},2,2},
    {"four-square-row-n1",{1,2,3,0},{1,0,2,3},1,2},{"four-square-row-n2",{1,2,3,0},{1,0,2,3},2,2},
    {"S-n1",{1,0,3,2},{0,2,1,3},1,2},{"staircase-n1",{1,0,3,2,4},{0,2,1,4,3},1,3},
    {"five-square-row-n1",{1,2,3,4,0},{1,0,3,2,4},1,3},
    {"reversed-L-n1",{1,0,2},{2,1,0},1,2,field::SquareTiledOrientation::Negative},
    {"relabelled-asymmetric-L-n1",{0,3,1,2},{3,1,2,0},1,2}
};}
inline field::PreparedSquareTiledSurface surface(const Case& c,Guard& g){return field::PreparedSquareTiledSurface::prepare(c.right,c.up,c.n,c.orientation,103,1,g);}
inline Z::SparseVector column(const Z& m,std::size_t j){Z::SparseVector out;for(const auto& [ij,z]:m.entries())if(ij.second==j)out.emplace(ij.first,z);return out;}
} // namespace surface_fixtures
