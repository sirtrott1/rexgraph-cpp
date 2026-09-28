#include "rfc/core/attachment_update.hpp"
#include "rfc/field/connection.hpp"
#include "rfc/field/execution_tower.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}

struct Fixture { rfc::core::RelationalComplex complex; rfc::core::RelationTypeId face; };
static Fixture make_open_triangle(){
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_type("face",rfc::core::BoundaryPolicy::DeclaredExact,3,3);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}});
    return {b.freeze(),face};
}

int main(){
try{
    using rfc::numeric::ExactMatrix; using rfc::numeric::rational;
    auto fixture=make_open_triangle();
    const auto good=rfc::core::append_higher_attachment(fixture.complex,rfc::core::Grade{2},fixture.face,
        {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    require(good.bianchi.satisfied(),"valid higher attachment did not satisfy Bianchi");
    require(good.complex.max_grade().value==2,"higher attachment did not add grade two");
    require(rfc::field::boundary_matrix(fixture.complex,rfc::core::Grade{1}).entries()==rfc::field::boundary_matrix(good.complex,rfc::core::Grade{1}).entries(),"higher attachment changed existing lower boundary");

    bool bad_rejected=false;
    try{
        (void)rfc::core::append_higher_attachment(fixture.complex,rfc::core::Grade{2},fixture.face,
            {{0,true,false,false,1},{1,true,false,false,1},{2,true,false,false,1}});
    }catch(const std::invalid_argument&){bad_rejected=true;}
    require(bad_rejected,"invalid higher attachment was not rejected");

    const auto& complex=good.complex;
    auto ideal=rfc::field::ExactConnectionTower::ideal(complex);
    require(ideal.flat_through(rfc::core::Grade{2}),"ideal connection is not flat");
    const auto routes=ideal.route_contributions(rfc::core::Grade{2});
    bool saw_nonzero=false; rfc::numeric::ExactMatrix route_sum(3,1);
    for(const auto& route:routes){if(route.nonzeros()!=0)saw_nonzero=true;route_sum=route_sum.add(route);}
    require(saw_nonzero,"ideal connection should retain nonzero local route channels");
    require(route_sum.nonzeros()==0,"ideal local route channels do not cancel globally");

    auto multipliers=ExactMatrix::from_rows({{2},{1},{1}});
    const auto curved=ideal.entrywise_scaled(rfc::core::Grade{2},multipliers);
    require(!curved.curvature(rfc::core::Grade{2}).flat(),"incompatible local deformation did not create curvature");

    std::vector<rfc::field::ExactMetric> M;
    M.emplace_back(ExactMatrix::from_rows({{2,1,0},{1,3,1},{0,1,2}}));
    M.emplace_back(ExactMatrix::from_rows({{4,1,0},{1,3,1},{0,1,5}}));
    M.emplace_back(ExactMatrix::from_rows({{7}}));
    require(curved.curvature_magnitude(rfc::core::Grade{2},M)>0,"curvature magnitude should be positive");

    std::vector<ExactMatrix> S;
    S.push_back(ExactMatrix::diagonal({2,3,5}));
    S.push_back(ExactMatrix::diagonal({3,2,7}));
    S.push_back(ExactMatrix::diagonal({11}));
    const auto transported=rfc::field::ExactConnectionTower::coordinate_transport(complex,S);
    require(transported.flat_through(rfc::core::Grade{2}),"coordinate transport created false curvature");

    const auto unweighted=rfc::field::ExactExecutionTower::unweighted(complex);
    const auto weighted=rfc::field::ExactExecutionTower::weighted(complex,M);
    rfc::numeric::RationalVector x{rational(1,2),-2,3};
    const auto b_unweighted=unweighted.apply({rfc::field::OperatorKind::Boundary,rfc::core::Grade{1},0},x);
    const auto b_weighted=weighted.apply({rfc::field::OperatorKind::Boundary,rfc::core::Grade{1},0},x);
    require(b_unweighted==b_weighted,"metric choice changed structural boundary action");
    const auto Lu=unweighted.apply({rfc::field::OperatorKind::Hodge,rfc::core::Grade{1},0},x);
    const auto Lw=weighted.apply({rfc::field::OperatorKind::Hodge,rfc::core::Grade{1},0},x);
    require(Lu!=Lw,"weighted exact tower should differ from unweighted Hodge on nontrivial metric");
    const auto Gw=weighted.apply({rfc::field::OperatorKind::Green,rfc::core::Grade{1},rational(2,3)},x);
    require(Gw.size()==3,"weighted Green result size mismatch");

    std::cout<<"deformation/tower exact tests passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
