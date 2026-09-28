#include "rfc/compat/phase01_operators.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace rfc;
static void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);} 
int main(){try{
    BoundaryStore one(2,1,{{{0,true,true,1},{1,true,false,exact(1,2)}}});
    require(one.coefficient(0)==-1,"negative orientation failed");
    require(one.coefficient(1)==exact(1,2),"positive magnitude failed");

    BoundaryStore B1(4,4,{
        {{0,true,true,1},{1,true,false,1}},{{1,true,true,1},{2,true,false,1}},
        {{2,true,true,1},{3,true,false,1}},{{3,true,true,1},{0,true,false,1}},});
    BoundaryStore B2(4,1,{{{0,true,false,1},{1,true,false,1},{2,true,false,1},{3,true,false,1}}});
    require(is_zero(B1.apply(B2.apply(ExactVector{1}))),"chain condition failed");

    DiagonalMetric M0({1,2,3,4}), M1({2,3,5,7}), M2({11});
    HodgeOperator L1(B1,M0,M1,&B2,&M2);
    ResolventOperator A(L1,exact(2,3));
    ExactVector b(4,0);b[0]=1;
    auto sol=exact_cg(A,b);
    require(sol.exact,"weighted exact CG failed");
    require(is_zero(sub(A.apply(sol.x),b)),"weighted resolvent residual nonzero");

    ExactVector x1={exact(1,2),exact(-2,3),exact(3,5),exact(4,7)};
    ExactVector y0={exact(5,3),exact(-1,2),exact(2,7),exact(8,9)};
    const auto lhs=M0.inner(B1.apply(x1),y0);
    const auto rhs=M1.inner(x1,B1.adjoint(y0,M0,M1));
    require(lhs==rhs,"metric adjoint identity failed");

    TowerField3 seed{{exact(1,2),0,0,0},{1,0,0,0},{exact(2,5)}};
    auto d1=dirac_apply(B1,B2,M0,M1,M2,seed);
    auto d2=dirac_apply(B1,B2,M0,M1,M2,d1);
    require(d2.c1==L1.apply(seed.c1),"D^2=L failed at grade 1");

    RelationTable table(130,2);table.set_type(65,1,true);table.set_payload(65,exact(11,8));
    auto q=table.query_type(1);require(q.count()==1&&q.get(65),"bit query failed");
    require(table.payload(65)==exact(11,8),"exact payload failed");

    std::cout<<"6/6 native exact tests passed\n";return EXIT_SUCCESS;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return EXIT_FAILURE;}}
