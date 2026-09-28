#include "rfc/core/builder.hpp"
#include "rfc/geometry/chain_homotopy.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static rfc::core::RelationalComplex interval(){
  rfc::core::RelationSignature s;
  const auto e=s.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
  rfc::core::RelationalComplexBuilder b(s);
  b.add_grade_one(e,{{0,true,true,false,1},{1,true,false,false,1}});
  return b.freeze();
}
int main(){
  try{
    const auto X=interval();
    rfc::geometry::LinearGradeMap F0(2),G0(2),F1(1),G1(1);
    F0[0]={{0,1}}; F0[1]={{0,1}};
    G0[0]={{1,1}}; G0[1]={{1,1}};
    std::vector<rfc::geometry::LinearGradeMap> fm{F0,F1},gm{G0,G1};
    rfc::geometry::RelationalChainMap F(X,X,std::move(fm));
    rfc::geometry::RelationalChainMap G(X,X,std::move(gm));
    std::vector<rfc::numeric::ExactMatrix> H;
    H.push_back(rfc::numeric::ExactMatrix::from_rows({{1,1}}));
    H.emplace_back(0,1);
    const auto certificate=rfc::geometry::certify_chain_homotopy(F,G,H);
    if(!certificate.endpoints_are_chain_maps || !certificate.exact_identity) throw std::runtime_error("chain homotopy certificate failed");
    std::cout<<"chain homotopy realization passed\n";
    return EXIT_SUCCESS;
  }catch(const std::exception& e){ std::cerr<<"FAIL: "<<e.what()<<'\n'; return EXIT_FAILURE; }
}
