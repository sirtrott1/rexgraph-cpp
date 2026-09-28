#include "rfc/core/builder.hpp"
#include "rfc/learning/exact_metric_channels.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c) throw std::runtime_error(m);}

int main(){
 try{
  rfc::core::RelationSignature sig;
  const auto edge=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
  const auto face=sig.add_type("face",rfc::core::BoundaryPolicy::DeclaredExact,3,3);
  rfc::core::RelationalComplexBuilder b(sig);
  b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
  b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
  b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}});
  b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
  const auto complex=b.freeze();
  using rfc::numeric::ExactMatrix;
  std::vector<rfc::field::ExactMetric> metrics;
  metrics.emplace_back(ExactMatrix::from_rows({{2,1,0},{1,3,1},{0,1,2}}));
  metrics.emplace_back(ExactMatrix::from_rows({{4,1,0},{1,3,1},{0,1,5}}));
  metrics.emplace_back(ExactMatrix::from_rows({{7}}));
  rfc::learning::ExactMetricChannelContext ctx(complex,rfc::core::Grade{1},metrics);
  using K=rfc::learning::ExactMetricChannelKind;
  std::vector<rfc::learning::ExactMetricChannel> candidates{{K::Identity,0},{K::HodgeDown,0},{K::HodgeUp,0},{K::Hodge,0}};
  const auto basis=rfc::learning::reduce_exact_metric_channels(ctx,candidates);
  require(basis.rank()==3,"Hodge=down+up should remove one exact metric channel");
  // teacher 2I+3Down-Up+4Hodge -> 2I+7Down+3Up
  std::vector<rfc::numeric::Rational> gains{2,3,-1,4};
  const auto reduced=rfc::learning::reduce_exact_metric_gains(basis,gains);
  require(reduced.size()==3,"reduced exact metric gain width");
  const auto plan=rfc::learning::compile_exact_metric_layer(ctx,candidates,gains);
  rfc::numeric::RationalVector x{1,-2,3};
  rfc::numeric::RationalVector teacher=x;
  for(auto& v:teacher)v*=2;
  const auto down=ctx.apply({K::HodgeDown,0},x);
  const auto up=ctx.apply({K::HodgeUp,0},x);
  const auto hodge=ctx.apply({K::Hodge,0},x);
  for(std::size_t i=0;i<x.size();++i)teacher[i]+=3*down[i]-up[i]+4*hodge[i];
  require(plan.layer.forward(x)==teacher,"compiled exact metric layer changed output");

  // Projector channels satisfy I=Pg+Pc+Ph.
  std::vector<rfc::learning::ExactMetricChannel> projector_candidates{{K::Identity,0},{K::GradientProjector,0},{K::CurlProjector,0},{K::HarmonicProjector,0}};
  const auto pb=rfc::learning::reduce_exact_metric_channels(ctx,projector_candidates);
  require(pb.rank()<=3,"Hodge projector partition did not reduce identity redundancy");

  // Green is an exact response channel and satisfies its residual.
  const auto green=ctx.apply({K::GreenResolvent,rfc::numeric::rational(2,3)},x);
  auto residual=rfc::numeric::add(green,rfc::numeric::scale(rfc::numeric::rational(2,3),ctx.hodge().apply(green)));
  require(residual==x,"Green channel residual failed");

  std::cout<<"exact metric channel tests passed\n";
  return EXIT_SUCCESS;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
