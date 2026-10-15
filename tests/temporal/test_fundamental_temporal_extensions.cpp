#include "rfc/core/builder.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/temporal/cylinder.hpp"
#include "rfc/temporal/factor_attribution.hpp"
#include "rfc/temporal/multitime.hpp"
#include "rfc/temporal/smooth.hpp"
#include "rfc/temporal/sector_transport.hpp"
#include "rfc/temporal/moment_change.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition,const char* message){ if(!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex make_triangle(bool filled){
    rfc::core::RelationSignature s;
    const auto edge=s.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=s.add_type("face",rfc::core::BoundaryPolicy::DeclaredExact,3,3);
    rfc::core::RelationalComplexBuilder b(s);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}});
    if(filled) b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    return b.freeze();
}

int main(){
  try{
    using rfc::numeric::ExactMatrix;
    using rfc::numeric::Rational;
    using rfc::numeric::RationalVector;

    const ExactMatrix J0=ExactMatrix::from_rows({{1,0},{0,1},{1,1}});
    const ExactMatrix J1=ExactMatrix::from_rows({{1,0,1},{0,1,-1}});
    const auto mt=rfc::temporal::accumulate_innovations({1,2},{J0,J1},{{2,-1,3},{4,-2}});
    require(mt.transported_initial==RationalVector({4,-1}),"multi-time transported initial mismatch");
    require(mt.propagated_innovations.size()==2 && mt.propagated_innovations[0]==RationalVector({5,-4}) && mt.propagated_innovations[1]==RationalVector({4,-2}),"multi-time propagated innovations mismatch");
    require(mt.terminal==RationalVector({13,-7}),"multi-time terminal mismatch");
    const auto gram=rfc::temporal::multi_time_channel_gram({{"eta0",mt.propagated_innovations[0]},{"eta1",mt.propagated_innovations[1]}},rfc::field::ExactMetric::identity(2));
    require(gram==ExactMatrix::from_rows({{41,28},{28,20}}),"multi-time innovation Gram mismatch");

    const std::vector<ExactMatrix> oldf{ExactMatrix::from_rows({{2}}),ExactMatrix::from_rows({{3}}),ExactMatrix::from_rows({{5}})};
    const std::vector<ExactMatrix> newf{ExactMatrix::from_rows({{7}}),ExactMatrix::from_rows({{11}}),ExactMatrix::from_rows({{13}})};
    const auto interactions=rfc::temporal::finite_ordered_factor_attribution(oldf,newf);
    const auto sum=rfc::temporal::sum_factor_interactions(interactions,1,1);
    const auto delta=rfc::temporal::ordered_product(newf).add(rfc::temporal::ordered_product(oldf).scale(Rational{-1}));
    require(interactions.size()==7 && sum==delta && delta.get(0,0)==971,"finite factor attribution mismatch");
    const auto a=ExactMatrix::from_rows({{1,2},{3,4},{5,6}});
    const auto b=ExactMatrix::from_rows({{2,1,0},{0,1,2}});
    const auto an=a.scale(Rational{2});
    const auto bn=b.add(ExactMatrix::from_rows({{1,0,0},{0,0,1}}));
    using rfc::temporal::FactorOrder;
    require(rfc::temporal::ordered_product({a,b})==b.multiply(a),"chronological default order changed");
    require(rfc::temporal::ordered_product({a,b},FactorOrder::WrittenProduct)==a.multiply(b),"written rectangular order failed");
    for(const auto order : {FactorOrder::AppliedFirst,FactorOrder::WrittenProduct}) {
        const auto old_product=rfc::temporal::ordered_product({a,b},order);
        const auto new_product=rfc::temporal::ordered_product({an,bn},order);
        const auto terms=rfc::temporal::finite_ordered_factor_attribution({a,b},{an,bn},order);
        require(rfc::temporal::sum_factor_interactions(terms,old_product.rows(),old_product.cols())==new_product.add(old_product.scale(Rational{-1})),"rectangular full interaction reconstruction failed");
    }

    // Temporal cylinder: persistent oriented edge closes for identity transport.
    const auto B1=ExactMatrix::from_rows({{-1},{1}});
    const auto Z2=ExactMatrix(1,0);
    const auto B0=ExactMatrix(0,2);
    const auto I1=ExactMatrix::identity(1);
    const auto I2=ExactMatrix::identity(2);
    const auto d2=rfc::temporal::temporal_cylinder_boundary(Z2,Z2,B1,I1);
    const auto d1=rfc::temporal::temporal_cylinder_boundary(B1,B1,B0,I2);
    require(d1.multiply(d2).nonzeros()==0,"temporal cylinder should close under chain-compatible transport");
    const auto reversed=ExactMatrix::from_rows({{1},{-1}});
    const auto d1_bad=rfc::temporal::temporal_cylinder_boundary(reversed,B1,B0,I2);
    require(d1_bad.multiply(d2).nonzeros()!=0,"temporal cylinder defect was not exposed");
    const auto defect=rfc::temporal::temporal_down_defect(reversed,B1,I1,I2);
    require(defect==ExactMatrix::from_rows({{2},{-2}}),"temporal down defect mismatch");

    // Smooth exact transport fixture at t=1 for M=diag(t^2,1), Gamma=diag(-1/t,0).
    const auto M=ExactMatrix::diagonal({1,1});
    const auto Mdot=ExactMatrix::diagonal({2,0});
    const auto Gamma=ExactMatrix::diagonal({-1,0});
    require(rfc::temporal::metric_compatibility_residual(Mdot,Gamma,M).nonzeros()==0,"smooth metric compatibility failed");
    require(rfc::temporal::covariant_field_derivative({2,1},Gamma,{1,2})==RationalVector({3,1}),"covariant field derivative mismatch");
    const auto L=ExactMatrix::diagonal({2,3});
    const auto Ldot=ExactMatrix::diagonal({-4,0});
    require(rfc::temporal::covariant_operator_derivative(Ldot,L,Gamma)==Ldot,"covariant operator derivative mismatch");


    // Changing-dimension cross moment fixture from the paper.
    const RationalVector ox{1,2},oy{3},nx{2,2,4},ny{4,7};
    const auto K=ExactMatrix::from_rows({{2},{4}});
    const auto Kp=ExactMatrix::from_rows({{2,1},{0,3},{4,5}});
    const auto Jx=ExactMatrix::from_rows({{1,0},{0,1},{1,1}});
    const auto Jy=ExactMatrix::from_rows({{1},{2}});
    const auto cm=rfc::temporal::changing_dimension_cross_moment(ox,oy,K,nx,ny,Kp,Jx,Jy);
    require(cm.pullback_form==144 && cm.left_innovation==54 && cm.right_innovation==36 && cm.innovation_interaction==12 && cm.total==246,"changing-dimension moment mismatch");
    require(rfc::temporal::finite_rate(cm.total,Rational{1,2})==492,"finite moment rate mismatch");

    // Face birth transfers the old harmonic triangle cycle into the new curl sector.
    const auto open=make_triangle(false),filled=make_triangle(true);
    std::vector<rfc::field::ExactMetric> Mo{rfc::field::ExactMetric::identity(3),rfc::field::ExactMetric::identity(3)};
    std::vector<rfc::field::ExactMetric> Mf{rfc::field::ExactMetric::identity(3),rfc::field::ExactMetric::identity(3),rfc::field::ExactMetric::identity(1)};
    const rfc::field::ExactHodgeOperator Lo(open,rfc::core::Grade{1},Mo),Lf(filled,rfc::core::Grade{1},Mf);
    const auto Po=rfc::field::exact_hodge_projectors(Lo),Pf=rfc::field::exact_hodge_projectors(Lf);
    const RationalVector cycle{1,1,-1};
    const auto hc=rfc::temporal::sector_transport_apply(Po,Pf,ExactMatrix::identity(3),rfc::temporal::HodgeSector::Curl,rfc::temporal::HodgeSector::Harmonic,cycle);
    require(hc==cycle,"harmonic-to-curl sector transport mismatch");

    std::cout<<"fundamental temporal extensions passed\n";
    return EXIT_SUCCESS;
  }catch(const std::exception& e){ std::cerr<<"FAIL: "<<e.what()<<'\n'; return EXIT_FAILURE; }
}
