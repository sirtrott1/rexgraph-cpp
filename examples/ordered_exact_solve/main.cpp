#include "rfc/core/builder.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <iostream>

int main(){try{
    using namespace rfc;
    using A=numeric::ExactMatrix;
    using V=numeric::RationalVector;
    core::RelationSignature signature;
    const auto type=signature.add_type("star",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    constexpr std::size_t size=32;
    for(std::uint64_t i=1;i<size;++i)builder.add_grade_one(core::RelationKey{100+i},type,
        {{0,true,true,1},{i,true,false,1},{i,true,true,2},
         {i,true,false,2},{(i%31)+1,false,false,7}});
    const auto source=builder.freeze();
    const auto boundary=field::materialize_boundary_matrix(source,core::Grade{1});
    const auto green=A::identity(size).add(boundary.multiply(boundary.transpose()));
    const numeric::PreparedExactSolve natural(green);
    numeric::ExactActionGuard planning;
    const auto order=numeric::minimum_degree_ordering(green,planning);
    const auto ordered=numeric::PreparedExactSolve::prepare_ordered(green,order);
    const auto receipt=ordered.factor_receipt();
    V rhs(size);for(std::size_t i=0;i<size;++i)rhs[i]=numeric::Rational{static_cast<unsigned long>(i+1)};
    const auto response=ordered.apply(rhs);
    if(response!=natural.apply(rhs) || green.apply(response)!=rhs ||
       ordered.apply_transpose(rhs)!=green.transpose().solve(rhs) ||
       ordered.prepare_selected({31,0,31}).apply(rhs)!=V({response[31],response[0],response[31]}) ||
       receipt.determinant()!=natural.factor_receipt().determinant())
        throw std::logic_error("original-coordinate response or determinant mismatch");
    std::cout<<"source_occurrences="<<source.grade(core::Grade{1}).incidence_count()
             <<" boundary_nonzeros="<<boundary.nonzeros()<<" planning_steps="<<planning.steps()<<'\n';
    for(const auto& item:std::vector<std::pair<const char*,numeric::PreparedExactSolve>>{{"natural",natural},{"minimum_degree",ordered}}){
        const auto stats=item.second.preparation_stats();
        std::cout<<"order="<<item.first<<" fill_insertions="<<stats.fill_insertions
                 <<" upper_entries="<<stats.upper_factor_entries<<" elimination_entries="<<stats.elimination_factor_entries
                 <<" retained_rational_entries="<<item.second.stored_entries()<<" retained_index_entries="<<item.second.stored_index_entries()
                 <<" preparation_steps="<<item.second.preparation_steps()<<'\n';
    }
    std::cout<<"determinant="<<receipt.determinant()<<" complete_original_residual=0"<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
