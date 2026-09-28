#include "rfc/fundamentals.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(){
  try{
    rfc::core::RelationSignature signature;
    const auto relation=signature.add_type("r",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(relation,{{0,true,true,false,1},{1,true,false,false,1}});
    const auto complex=builder.freeze();
    std::vector<rfc::field::ExactMetric> metrics{
      rfc::field::ExactMetric::identity(2),rfc::field::ExactMetric::identity(1)};
    rfc::field::ExactHodgeOperator hodge(complex,rfc::core::Grade{1},metrics);
    if(hodge.apply({1})!=rfc::numeric::RationalVector({2})) throw std::runtime_error("fundamental Hodge smoke mismatch");
    std::cout<<"fundamental umbrella passed\n";
    return EXIT_SUCCESS;
  }catch(const std::exception& e){ std::cerr<<"FAIL: "<<e.what()<<'\n'; return EXIT_FAILURE; }
}
