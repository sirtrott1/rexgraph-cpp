#include "rfc/geometry/algebraic_locus.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
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
  const auto defect=B1.multiply(B2);
  // Specialization is an independent exact check of the polynomial product.
  for(const auto& point:std::vector<rfc::numeric::RationalVector>{{0,1,-2,3,4,-5},{2,3,5,15,10,6},{-1,2,-3,4,-5,6}}){
    rfc::numeric::ExactMatrix lower(B1.rows(),B1.cols()),upper(B2.rows(),B2.cols());
    for(std::size_t i=0;i<B1.rows();++i)for(std::size_t j=0;j<B1.cols();++j)lower.set(i,j,B1.get(i,j).evaluate(point));
    for(std::size_t i=0;i<B2.rows();++i)for(std::size_t j=0;j<B2.cols();++j)upper.set(i,j,B2.get(i,j).evaluate(point));
    const auto evaluated=lower.multiply(upper);
    for(std::size_t i=0;i<defect.rows();++i)for(std::size_t j=0;j<defect.cols();++j)
      require(defect.get(i,j).evaluate(point)==evaluated.get(i,j),"polynomial specialization changed the exact matrix product");
  }

  // The logical rectangle intentionally exceeds size_t.  A bounded sparse
  // chain-ideal query must depend on its coefficients, not on that rectangle.
  const auto huge=std::numeric_limits<std::size_t>::max();
  PolynomialMatrix sparse_lower(huge,3,n),sparse_upper(3,huge,n),sparse_top(huge,1,n);
  sparse_lower.set(0,0,x1); sparse_lower.set(huge-1,1,x2); sparse_lower.set(huge-1,2,x2);
  sparse_upper.set(0,huge-1,y1); sparse_upper.set(1,0,y2); sparse_upper.set(2,0,y2.scale(-1));
  sparse_top.set(huge-1,0,ExactPolynomial::constant(n,1));
  const auto sparse_product=sparse_lower.multiply(sparse_upper);
  require(sparse_product.rows()==huge && sparse_product.cols()==huge && sparse_product.nonzeros()==1,
          "huge sparse polynomial product allocated or retained ambient zero entries");
  require(sparse_product.get(0,huge-1)==x1.multiply(y1) && sparse_product.get(huge-1,0).is_zero(),
          "huge sparse polynomial product lost a term or exact cancellation");
  require(chain_ideal_generators(sparse_lower,sparse_upper)==PolynomialIdealGenerators{x1.multiply(y1)},
          "huge sparse chain ideal changed its nonzero generators");
  require(bianchi_ideal_generators({sparse_lower,sparse_upper,sparse_top})==PolynomialIdealGenerators{x1.multiply(y1),y1},
          "sparse Bianchi generators lost interface order or coefficients");
  sparse_lower.set(0,0,ExactPolynomial(n));
  require(sparse_lower.nonzeros()==2 && sparse_lower.get(0,0).variable_count()==n,
          "zero polynomial erasure lost its coefficient ring");
  bool rejected=false;
  try{sparse_lower.set(0,0,ExactPolynomial(n+1));}catch(const std::invalid_argument&){rejected=true;}
  require(rejected,"sparse polynomial matrix accepted another coefficient ring");
  rejected=false;
  try{(void)sparse_lower.get(huge,0);}catch(const std::out_of_range&){rejected=true;}
  require(rejected,"sparse polynomial matrix lost logical bounds checks");
  std::cout<<"algebraic deformation locus passed\n";return EXIT_SUCCESS;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}}
