#include "rfc/inference/field_execution.hpp"
#include "rfc/core/builder.hpp"
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
int main(){try{
    using rfc::numeric::ExactMatrix; using rfc::numeric::rational;
    const auto c=make_triangle();
    std::vector<rfc::field::ExactMetric> M;
    M.emplace_back(ExactMatrix::from_rows({{2,1,0},{1,3,1},{0,1,2}}));
    M.emplace_back(ExactMatrix::from_rows({{4,1,0},{1,3,1},{0,1,5}}));
    M.emplace_back(ExactMatrix::from_rows({{7}}));
    rfc::numeric::RationalVector x{rational(1,2),-2,3};
    const rfc::field::OperatorRequest op{rfc::field::OperatorKind::Hodge,rfc::core::Grade{1},0};
    auto u=rfc::inference::execute_field(c,M,{rfc::inference::ExecutionTower::UnweightedExact,op},x);
    auto w=rfc::inference::execute_field(c,M,{rfc::inference::ExecutionTower::WeightedExact,op},x);
    auto a=rfc::inference::execute_field(c,M,{rfc::inference::ExecutionTower::Approximate,op},x);
    require(std::holds_alternative<rfc::numeric::RationalVector>(u),"unweighted route was not exact");
    require(std::holds_alternative<rfc::numeric::RationalVector>(w),"weighted route was not exact");
    require(std::holds_alternative<rfc::approximation::Vector>(a),"approx route was not approximate");
    const auto& wx=std::get<rfc::numeric::RationalVector>(w);
    const auto& ax=std::get<rfc::approximation::Vector>(a);
    require(rfc::approximation::max_abs_error(ax,wx)<1e-11,"routed approximation mismatch");
    require(std::get<rfc::numeric::RationalVector>(u)!=wx,"weighted/unweighted route distinction vanished");
    std::cout<<"three tower routing tests passed\n";return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}}
