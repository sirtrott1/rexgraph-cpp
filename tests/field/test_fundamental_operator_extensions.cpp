#include "rfc/core/builder.hpp"
#include "rfc/field/action.hpp"
#include "rfc/field/exact_dirac.hpp"
#include "rfc/field/differential.hpp"
#include "rfc/field/exact_tensor_functions.hpp"
#include "rfc/field/quotient.hpp"
#include "rfc/field/resolvent.hpp"
#include "rfc/statistics/field_moments.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition,const char* message){ if(!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex triangle(){
    rfc::core::RelationSignature s;
    const auto edge=s.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=s.add_type("face",rfc::core::BoundaryPolicy::DeclaredExact,3,3);
    rfc::core::RelationalComplexBuilder b(s);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}});
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    return b.freeze();
}

int main(){
  try{
    using rfc::numeric::Rational;
    using rfc::numeric::RationalVector;
    const auto c=triangle();
    std::vector<rfc::field::ExactMetric> M{
      rfc::field::ExactMetric::identity(3),
      rfc::field::ExactMetric::identity(3),
      rfc::field::ExactMetric::identity(1)};

    rfc::field::ExactTowerField x{{{2,-1,3},{1,2,-1},{4}}};
    const auto Lx=rfc::field::exact_hodge_tower_apply(c,M,x);
    const auto Da2=rfc::field::exact_antidirac_square_apply(c,M,x);
    require(Da2.grades==rfc::field::exact_tower_scale(Rational{-1},Lx).grades,"anti-Dirac square != -L");
    const auto anti=rfc::field::exact_dirac_antidirac_anticommutator_apply(c,M,x);
    for(const auto& g:anti.grades) require(rfc::numeric::is_zero(g),"Dirac/anti-Dirac anticommutator nonzero");
    const auto comm=rfc::field::exact_dirac_antidirac_commutator_half_apply(c,M,x);
    const auto Kx=rfc::field::exact_hodge_orientation_tower_apply(c,M,x);
    require(comm.grades==Kx.grades,"half commutator != oriented Hodge tower");

    rfc::field::ExactHodgeOperator L1(c,rfc::core::Grade{1},M);
    const RationalVector u{1,2,-1},v{2,-1,3};
    const auto p1=rfc::statistics::power_moment(L1,u,v,1);
    const auto adjacent=rfc::statistics::adjacent_field_moment(L1,u,v);
    require(p1.total==adjacent.total && p1.oriented==adjacent.oriented,"first power moment mismatch");
    const auto p3=rfc::statistics::power_moment(L1,u,v,3);
    require(p3.total==L1.metric().inner(u,rfc::statistics::hodge_power_apply(L1,3,v)),"third total moment mismatch");
    const auto moment_tensor=rfc::statistics::dirac_antidirac_moment_matrix(L1,u,v);
    require(moment_tensor.get(0,0)==adjacent.total && moment_tensor.get(0,1)==adjacent.oriented &&
            moment_tensor.get(1,0)==adjacent.oriented && moment_tensor.get(1,1)==adjacent.total,
            "Dirac anti-Dirac moment tensor mismatch");

    const auto dq=rfc::field::down_field_quotient(L1,u,v);
    const auto uq=rfc::field::up_field_quotient(L1,u,v);
    require(dq.residual_quadrance>=0 && uq.residual_quadrance>=0,"field quotient residual must be nonnegative");

    const auto resolvent=rfc::field::exact_resolvent_identity(L1,Rational{1},Rational{2},u);
    require(resolvent.exact,"resolvent identity failed");

    // Filled triangle: L1=3I, but K distinguishes down and up sectors.
    const auto oriented=rfc::field::oriented_hodge_apply(L1,Rational{1},u);
    require(oriented==rfc::numeric::scale(Rational{2},L1.apply_down(u)),"tau=1 oriented Hodge mismatch");
    const auto stationary=rfc::field::exact_oriented_stationary_field(L1,Rational{1},Rational{0},RationalVector{4,0,0});
    require(stationary.exact && stationary.value==RationalVector({1,0,0}),"oriented stationary field mismatch");

    // Xi_1(W)=B1 W B2 for W=diag(1,2,1) has value (0,-1,1) on the face.
    const auto W=rfc::numeric::ExactMatrix::diagonal({1,2,1});
    rfc::field::ExactChainStrainOperator strain(c,rfc::core::Grade{1},M,W);
    require(strain.apply({1})==RationalVector({0,-1,1}),"chain strain field mismatch");
    require(strain.energy({1})==2,"chain strain energy mismatch");
    rfc::field::ExactHodgeOperator L2(c,rfc::core::Grade{2},M);
    const auto composite=rfc::field::exact_composite_stationary_field(L2,strain,Rational{1},Rational{0},Rational{1},RationalVector{6});
    require(composite.exact && composite.value==RationalVector{Rational{1}},"composite strain stationary field mismatch");
    require(rfc::numeric::add(composite.value,rfc::numeric::add(L2.apply(composite.value),strain.stiffness_apply(composite.value)))==RationalVector{Rational{6}},
            "composite stationary equation residual mismatch");


    const auto U=rfc::field::exact_antidirac_cayley_apply(c,M,Rational{1},x);
    require(rfc::field::exact_tower_inner(M,U,U)==rfc::field::exact_tower_inner(M,x,x),"rational anti-Dirac Cayley isometry failed");

    const auto singular=rfc::numeric::ExactMatrix::from_rows({{1,2},{2,4}});
    const auto adj=rfc::field::exact_adjugate(singular);
    require(singular.multiply(adj).nonzeros()==0 && adj.multiply(singular).nonzeros()==0,"singular adjugate identity failed");
    require(rfc::field::determinant_differential(singular,rfc::numeric::ExactMatrix::identity(2))==5,"determinant differential mismatch");

    const auto B=rfc::numeric::ExactMatrix::from_rows({{-1},{1}});
    const auto dB=rfc::numeric::ExactMatrix::from_rows({{2},{3}});
    const auto adjvar=rfc::field::metric_adjoint_variation(B,rfc::field::ExactMetric::identity(1),rfc::field::ExactMetric::identity(2),dB,rfc::numeric::ExactMatrix(1,1),rfc::numeric::ExactMatrix(2,2));
    require(adjvar==dB.transpose(),"metric adjoint differential mismatch under fixed identity metrics");

    const rfc::numeric::ExactMatrix zeroL(3,3);
    const auto gd=rfc::field::exact_green_response_differential(L1,Rational{1},RationalVector{1,0,0},RationalVector{0,0,0},Rational{1},zeroL);
    require(gd.response==RationalVector({Rational{1,4},0,0}) && gd.differential==RationalVector({Rational{-3,16},0,0}),"Green parameter differential mismatch");

    std::cout<<"fundamental operator extensions passed\n";
    return EXIT_SUCCESS;
  }catch(const std::exception& e){ std::cerr<<"FAIL: "<<e.what()<<'\n'; return EXIT_FAILURE; }
}
