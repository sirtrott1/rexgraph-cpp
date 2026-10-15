#include "rfc/core/builder.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/temporal/transfer.hpp"
#include <iostream>

int main(){try{
    using namespace rfc;
    using M=numeric::ExactMatrix;
    using V=numeric::RationalVector;
    core::RelationSignature signature;
    const auto type=signature.add_type("square",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
    for(std::uint64_t i=0;i<4;++i)builder.add_grade_one(core::RelationKey{100+i},type,
        {{i,true,true,numeric::Rational{1}}, {(i+1)%4,true,false,numeric::Rational{1}},
         {i,true,true,numeric::Rational{2}}, {i,true,false,numeric::Rational{2}},
         {(i+2)%4,false,false,numeric::Rational{7}}});
    const auto old_source=builder.freeze();
    builder.add_higher(core::Grade{2},core::RelationKey{200},type,
        {{0,true,false,numeric::Rational{1}},{1,true,false,numeric::Rational{1}},
         {2,true,false,numeric::Rational{1}},{3,true,false,numeric::Rational{1}}});
    const auto new_source=builder.freeze();
    const auto b=field::materialize_boundary_matrix(old_source,core::Grade{1});
    const auto face=field::materialize_boundary_matrix(new_source,core::Grade{2});
    const auto old_L=b.transpose().multiply(b),new_L=old_L.add(face.multiply(face.transpose()));
    const numeric::PreparedExactSolve base(M::identity(4).add(old_L));
    const field::PreparedWoodburySolve update(base,face,M::identity(1),face);
    // The two source complexes remain distinct. J and K are explicitly
    // declared numeric correspondences, not inferred native identities.
    const temporal::PreparedGreenResponseDelta response(old_L,new_L,M::identity(4),
        M::from_rows({{1},{1},{0},{0}}),M::from_rows({{0},{0},{-1},{-1}}),M::identity(1),1);
    const temporal::PreparedObservedGreenResponseDelta observed(response,
        M::from_rows({{1,0,0,0},{0,1,0,0}}),M::from_rows({{1,0,1,0},{0,1,0,1}}),M::identity(2));
    std::cout<<"old_occurrences="<<old_source.grade(core::Grade{1}).incidence_count()
             <<" new_face_occurrences="<<new_source.grade(core::Grade{2}).incidence_count()
             <<" update_preparation_steps="<<update.preparation_steps()
             <<" response_preparation_steps="<<response.preparation_steps()<<'\n';
    for(const auto& rhs:std::vector<V>{{1,2,3,4},{4,3,2,1}}){
        numeric::ExactActionGuard guard;const auto answer=update.apply(rhs,guard);
        std::cout<<"updated_response=";for(const auto& q:answer)std::cout<<' '<<q;
        std::cout<<" application_steps="<<guard.steps()<<'\n';
    }
    for(const auto& coordinates:std::vector<std::pair<V,V>>{{{2},{3}},{{1},{4}}}){
        const auto delta=response.apply(coordinates.first,coordinates.second);
        std::cout<<"field_change=";for(const auto& q:delta.aggregate)std::cout<<' '<<q;
        std::cout<<" observed_change=";for(const auto& q:observed.apply(coordinates.first,coordinates.second).aggregate)std::cout<<' '<<q;
        std::cout<<'\n';
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
