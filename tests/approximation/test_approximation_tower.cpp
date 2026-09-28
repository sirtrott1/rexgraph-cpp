#include "rfc/approximation/tower.hpp"
#include "rfc/core/attachment_update.hpp"
#include "rfc/approximation/connection.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}
static rfc::core::RelationalComplex make_triangle(){
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_type("face",rfc::core::BoundaryPolicy::DeclaredExact,3,3);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}});
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    return b.freeze();
}
int main(){
try{
    using rfc::numeric::ExactMatrix; using rfc::numeric::rational;
    const auto complex=make_triangle();
    std::vector<rfc::field::ExactMetric> M;
    M.emplace_back(ExactMatrix::from_rows({{2,1,0},{1,3,1},{0,1,2}}));
    M.emplace_back(ExactMatrix::from_rows({{4,1,0},{1,3,1},{0,1,5}}));
    M.emplace_back(ExactMatrix::from_rows({{7}}));
    const auto exact=rfc::field::ExactExecutionTower::weighted(complex,M);
    const auto approx=rfc::approximation::ApproximationTower::from_exact(complex,M);
    rfc::numeric::RationalVector x{rational(1,2),-2,3};
    const auto xd=rfc::approximation::to_approximate(x);
    for(const auto kind:{rfc::field::OperatorKind::Boundary,rfc::field::OperatorKind::HodgeDown,rfc::field::OperatorKind::HodgeUp,rfc::field::OperatorKind::Hodge}){
        const auto ex=exact.apply({kind,rfc::core::Grade{1},0},x);
        const auto ap=approx.apply({kind,rfc::core::Grade{1},0},xd);
        require(rfc::approximation::max_abs_error(ap,ex)<1e-11,"approx operator exceeded tolerance");
    }
    const auto exg=exact.apply({rfc::field::OperatorKind::Green,rfc::core::Grade{1},rational(2,3)},x);
    const auto apg=approx.apply({rfc::field::OperatorKind::Green,rfc::core::Grade{1},rational(2,3)},xd);
    require(rfc::approximation::max_abs_error(apg,exg)<1e-10,"approx Green exceeded tolerance");

    rfc::field::ExactTowerField xt{{{1,2,3},x,{2}}};
    const auto D2=exact.apply_dirac(exact.apply_dirac(xt));
    const auto L=exact.apply_hodge_tower(xt);
    require(D2.grades==L.grades,"exact D^2=L failed in unified tower");
    const auto xa=rfc::approximation::to_approximate(xt);
    const auto Da=approx.apply_dirac(xa); const auto D2a=approx.apply_dirac(Da); const auto La=approx.apply_hodge(xa);
    for(std::size_t k=0;k<La.grades.size();++k){
        double e=0.0;for(std::size_t i=0;i<La.grades[k].size();++i)e=std::max(e,std::abs(La.grades[k][i]-D2a.grades[k][i]));
        require(e<1e-10,"approx D^2=L exceeded tolerance");
    }

    auto connection=rfc::field::ExactConnectionTower::ideal(complex);
    auto multipliers=ExactMatrix::from_rows({{2},{1},{1}});
    auto curved=connection.entrywise_scaled(rfc::core::Grade{2},multipliers);
    const auto exact_curvature=curved.curvature_magnitude(rfc::core::Grade{2},M);
    const auto approximate_curvature=rfc::approximation::ConnectionTower::from_exact(curved,M).curvature_magnitude(rfc::core::Grade{2});
    require(std::abs(approximate_curvature-exact_curvature.get_d())<1e-10,"approx curvature magnitude exceeded tolerance");
    std::cout<<"approximation tower tests passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
