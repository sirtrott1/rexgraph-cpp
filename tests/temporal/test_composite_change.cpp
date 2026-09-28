#include "rfc/temporal/composite_change.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}
int main(){try{
 using C=rfc::composite::CompositeCoefficient; using R=rfc::numeric::Rational;
 std::vector<C> old{{true,true,1},{true,false,R{1,2}},{true,false,R{1,2}}};
 std::vector<C> now{{true,false,R{1,3}},{true,true,1},{false,false,0}};
 const auto d=rfc::temporal::composite_boundary_change(old,now);
 for(std::size_t i=0;i<old.size();++i) require(d.aggregate[i]==now[i].realize()-old[i].realize(),"composite change mismatch");
 require(d.existence[2]==R{-1,2},"existence channel mismatch");
 require(d.orientation[0]==2,"orientation channel mismatch");
 std::cout<<"composite temporal change tests passed\n"; return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}}
