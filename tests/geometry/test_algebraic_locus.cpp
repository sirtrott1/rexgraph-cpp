#include "rfc/geometry/algebraic_locus.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
int main(){try{
  using namespace rfc::geometry; using rfc::numeric::Rational;
  constexpr std::size_t n=6; auto x1=ExactPolynomial::variable(n,0),x2=ExactPolynomial::variable(n,1),x3=ExactPolynomial::variable(n,2); auto y1=ExactPolynomial::variable(n,3),y2=ExactPolynomial::variable(n,4),y3=ExactPolynomial::variable(n,5);
  PolynomialMatrix B1(3,3,n),B2(3,1,n);
  B1.set(0,0,x1.scale(-1)); B1.set(1,0,x1); B1.set(1,1,x2.scale(-1)); B1.set(2,1,x2); B1.set(0,2,x3.scale(-1)); B1.set(2,2,x3);
  B2.set(0,0,y1); B2.set(1,0,y2); B2.set(2,0,y3.scale(-1));
  const auto ideal=chain_ideal_generators(B1,B2);
  if(ideal.size()!=3)throw std::runtime_error("triangle chain ideal generator count mismatch");
  if(!lies_on_flat_locus(ideal,{2,3,5,15,10,6}))throw std::runtime_error("flat weighted triangle rejected"); // products all 30
  if(lies_on_flat_locus(ideal,{2,3,5,1,1,1}))throw std::runtime_error("curved weighted triangle accepted");
  std::cout<<"algebraic deformation locus passed\n";return EXIT_SUCCESS;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}}
