#include "rfc/approximation/tower.hpp"
#include "rfc/core/attachment_update.hpp"
#include "rfc/field/connection.hpp"
#include "rfc/field/execution_tower.hpp"
#include <iostream>

int main(){
    using namespace rfc;
    core::RelationSignature sig;
    const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_type("face",core::BoundaryPolicy::DeclaredExact,3,3);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}});
    const auto open=b.freeze();
    const auto filled=core::append_higher_attachment(open,core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}}).complex;

    std::vector<field::ExactMetric> M;
    M.emplace_back(numeric::ExactMatrix::from_rows({{2,1,0},{1,3,1},{0,1,2}}));
    M.emplace_back(numeric::ExactMatrix::from_rows({{4,1,0},{1,3,1},{0,1,5}}));
    M.emplace_back(numeric::ExactMatrix::from_rows({{7}}));
    const auto U=field::ExactExecutionTower::unweighted(filled);
    const auto W=field::ExactExecutionTower::weighted(filled,M);
    const auto A=approximation::ApproximationTower::from_exact(filled,M);
    numeric::RationalVector x{numeric::rational(1,2),-2,3};
    const field::OperatorRequest op{field::OperatorKind::Hodge,core::Grade{1},0};
    const auto u=U.apply(op,x);
    const auto w=W.apply(op,x);
    const auto a=A.apply(op,approximation::to_approximate(x));
    std::cout<<"unweighted_exact=";for(const auto& v:u)std::cout<<' '<<v.get_str();std::cout<<'\n';
    std::cout<<"weighted_exact=";for(const auto& v:w)std::cout<<' '<<v.get_str();std::cout<<'\n';
    std::cout<<"approximate=";for(const auto v:a)std::cout<<' '<<v;std::cout<<'\n';

    auto connection=field::ExactConnectionTower::ideal(filled);
    auto scale=numeric::ExactMatrix::from_rows({{2},{1},{1}});
    auto curved=connection.entrywise_scaled(core::Grade{2},scale);
    std::cout<<"ideal_flat="<<connection.flat_through(core::Grade{2})<<'\n';
    std::cout<<"deformed_flat="<<curved.flat_through(core::Grade{2})<<'\n';
    std::cout<<"curvature_magnitude="<<curved.curvature_magnitude(core::Grade{2},M).get_str()<<'\n';
}
