#include "rfc/core/builder.hpp"
#include "rfc/field/augmented_green.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/field/exact_operator.hpp"
#include "rfc/field/exact_dirac.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex make_triangle(bool filled=true) {
    rfc::core::RelationSignature signature;
    const auto edge=signature.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_type("face",rfc::core::BoundaryPolicy::DeclaredExact,3,3);
    rfc::core::RelationalComplexBuilder b(signature);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}});
    if (filled) b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    return b.freeze();
}

static_assert(rfc::field::ExactLinearOperator<rfc::field::BoundaryOperator>);
static_assert(rfc::field::ExactEndomorphism<rfc::field::ExactHodgeOperator>);

int main(){
  try {
    const auto complex=make_triangle(true);
    using rfc::numeric::Rational;
    using rfc::numeric::rational;
    using rfc::numeric::ExactMatrix;
    std::vector<rfc::field::ExactMetric> M;
    M.emplace_back(ExactMatrix::from_rows({{2,1,0},{1,3,1},{0,1,2}}));
    M.emplace_back(ExactMatrix::from_rows({{4,1,0},{1,3,1},{0,1,5}}));
    M.emplace_back(ExactMatrix::from_rows({{7}}));


    // Operator-first metric storage policies. Identity/diagonal avoid matrix storage;
    // certified external actions need no materializer for apply/solve/inner.
    const auto identity_metric=rfc::field::ExactMetric::identity(3);
    require(identity_metric.kind()==rfc::field::ExactMetricKind::Identity,"identity metric kind mismatch");
    const auto diagonal_metric=rfc::field::ExactMetric::diagonal({2,3,5});
    require(diagonal_metric.kind()==rfc::field::ExactMetricKind::Diagonal,"diagonal metric kind mismatch");
    require(diagonal_metric.solve(diagonal_metric.apply({1,2,3}))==rfc::numeric::RationalVector({1,2,3}),"diagonal metric action/solve mismatch");
    const auto external_metric=rfc::field::ExactMetric::certified_external(2,
        [](const rfc::numeric::RationalVector& v){ return rfc::numeric::RationalVector{2*v[0],3*v[1]}; },
        [](const rfc::numeric::RationalVector& v){ return rfc::numeric::RationalVector{v[0]/2,v[1]/3}; });
    require(external_metric.kind()==rfc::field::ExactMetricKind::CertifiedExternal,"external metric kind mismatch");
    require(external_metric.solve(external_metric.apply({4,5}))==rfc::numeric::RationalVector({4,5}),"external metric action/solve mismatch");
    const auto block_metric=rfc::field::ExactMetric::block_diagonal({
        rfc::field::ExactMetric::diagonal({2,3}), rfc::field::ExactMetric::identity(1)});
    require(block_metric.kind()==rfc::field::ExactMetricKind::BlockDiagonal,"block metric kind mismatch");
    require(block_metric.solve(block_metric.apply({1,2,3}))==rfc::numeric::RationalVector({1,2,3}),"block metric action/solve mismatch");

    const auto B1=rfc::field::boundary_matrix(complex,rfc::core::Grade{1});
    const auto B1dag=rfc::field::metric_adjoint_matrix(B1,M[1],M[0]);
    // exact adjoint identity on two arbitrary fields
    rfc::numeric::RationalVector x{rational(1,2),-2,3};
    rfc::numeric::RationalVector y{2,-1,4};
    require(M[0].inner(B1.apply(x),y)==M[1].inner(x,B1dag.apply(y)),"general metric adjoint identity failed");

    rfc::field::ExactHodgeOperator L1(complex,rfc::core::Grade{1},M);
    const auto frames=rfc::field::exact_hodge_projectors(L1);
    require(frames.gradient.cols()+frames.curl.cols()+frames.harmonic.cols()==3,"Hodge frame dimensions do not fill grade");
    const auto pg=frames.project_gradient(x);
    const auto pc=frames.project_curl(x);
    const auto ph=frames.project_harmonic(x);
    require(rfc::numeric::add(rfc::numeric::add(pg,pc),ph)==x,"Hodge projector reconstruction failed");
    require(M[1].inner(pg,pc)==0 && M[1].inner(pg,ph)==0 && M[1].inner(pc,ph)==0,"Hodge sectors not metric orthogonal");

    const auto green=rfc::field::exact_green_apply(complex,rfc::core::Grade{1},M,rational(2,3),x);
    require(green.exact,"operator-first Green residual failed");
    const auto green_reference=rfc::field::augmented_green_apply_reference(complex,rfc::core::Grade{1},M,rational(2,3),x);
    require(green_reference.exact && green_reference.value==green.value,"operator-first Green disagrees with augmented reference");
    const auto green_negative=rfc::field::augmented_green_apply(complex,rfc::core::Grade{1},M,rational(-1,10),x);
    require(green_negative.exact,"negative-lambda nonsingular augmented Green residual failed");

    const auto coords=rfc::field::complete_field_coordinates(complex,rfc::core::Grade{1},M,x);
    require(rfc::field::reconstruct_complete_field(complex,rfc::core::Grade{1},M,coords)==x,"complete field coordinate reconstruction failed");

    rfc::field::ExactTowerField tower{{{1,2,3},x,{2}}};
    const auto D1=rfc::field::exact_dirac_apply(complex,M,tower);
    const auto D2=rfc::field::exact_dirac_apply(complex,M,D1);
    const auto LT=rfc::field::exact_hodge_tower_apply(complex,M,tower);
    require(D2.grades==LT.grades,"general-metric exact D^2=L failed");

    const auto pseudo=rfc::field::pseudodeterminant_via_harmonic_completion(L1);
    require(pseudo!=0,"filled triangle Hodge pseudodeterminant should be nonzero");

    // unfilled triangle: one harmonic direction, completion determinant still nonzero.
    const auto open=make_triangle(false);
    std::vector<rfc::field::ExactMetric> MO;
    MO.emplace_back(rfc::field::ExactMetric::identity(3));
    MO.emplace_back(rfc::field::ExactMetric::identity(3));
    rfc::field::ExactHodgeOperator LO(open,rfc::core::Grade{1},MO);
    const auto HO=LO.harmonic_frame();
    require(HO.cols()==1,"unfilled triangle should have one harmonic direction");

    // Identity-metric Hodge action stays sparse over retained incidence support.
    const rfc::numeric::SparseRationalVector sparse_source(3,{{0,1}});
    const auto sparse_hodge=LO.apply(sparse_source.view());
    require(sparse_hodge.dense()==LO.apply(sparse_source.dense()),"sparse Hodge action disagrees with dense action");
    require(LO.materialize().apply(sparse_source.dense())==LO.apply(sparse_source.dense()),"materialized Hodge reference disagrees with action path");
    const auto pdet=rfc::field::pseudodeterminant_via_harmonic_completion(LO);
require(pdet==9,"unfilled triangle pdet should be 9");

    // Metric harmonic volume theorem on the open cycle with one-dimensional filled subspace fixture.
    const auto Z=ExactMatrix::from_rows({{1,0},{1,1},{0,1}});
    const auto F=ExactMatrix::from_rows({{1},{1},{0}});
    const auto W=ExactMatrix::from_rows({{0},{2},{2}});
    const auto R=ExactMatrix::from_rows({{1,0},{0,2}});
    rfc::field::ExactMetric MV(ExactMatrix::from_rows({{2,1,0},{1,3,1},{0,1,2}}));
    const auto volume=rfc::field::metric_volume_factorization(Z,F,W,R,MV);
    require(volume.exact_identity,"metric volume factorization failed");
    require(F.transpose().multiply(MV.form()).multiply(volume.harmonic_frame).nonzeros()==0,"volume harmonic frame not metric orthogonal");

    // Harmonic determinant completion on open triangle with Sigma=G_H^{-1} equals projector completion.
    const auto G=HO.transpose().multiply(MO[1].form()).multiply(HO);
    const auto sigma=rfc::field::inverse_by_solves(G);
    require(rfc::field::determinant_harmonic_completion(LO,sigma)==pdet,"harmonic determinant completion mismatch");
    const auto sigma2=ExactMatrix::from_rows({{2}});
    const auto lhs_det=rfc::field::determinant_harmonic_completion(LO,sigma2);
    require(lhs_det==pdet*G.determinant()*sigma2.determinant(),"general harmonic determinant completion theorem failed");

    const auto A=ExactMatrix::from_rows({{1,2,3},{2,4,6}});
    const auto K=A.kernel_frame();
    require(K.cols()==2 && A.multiply(K).nonzeros()==0,"exact kernel frame failed");
    const auto Im=A.image_frame();
    require(Im.cols()==1,"exact image frame rank failed");

    const auto Fline=ExactMatrix::from_rows({{1},{0}});
    const auto Gline=ExactMatrix::from_rows({{1},{1}});
    require(rfc::field::subspace_join_frame(Fline,Gline).cols()==2,"subspace join failed");
    require(rfc::field::subspace_intersection_frame(Fline,Gline).cols()==0,"subspace intersection failed");

    bool indefinite_rejected=false;
    try { rfc::field::ExactMetric bad(ExactMatrix::from_rows({{1,0},{0,-1}})); } catch(const std::invalid_argument&) { indefinite_rejected=true; }
    require(indefinite_rejected,"indefinite metric was not rejected");

    std::cout << "exact metric/Hodge tests passed\n";
    return EXIT_SUCCESS;
  } catch(const std::exception& e){ std::cerr << "FAIL: " << e.what() << '\n'; return EXIT_FAILURE; }
}
