#include "rfc/core/builder.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <iostream>

int main() {try {
    using namespace rfc;
    core::RelationSignature signature;
    const auto edge=signature.add_type("square-edge",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    for(std::uint64_t i=0;i<4;++i)
        builder.add_grade_one(core::RelationKey{100+i},edge,
            {{i,true,true,numeric::Rational{1}}, {(i+1)%4,true,false,numeric::Rational{1}},
             {i,true,true,numeric::Rational{2}}, {i,true,false,numeric::Rational{2}},
             {(i+2)%4,false,false,numeric::Rational{7}}});
    const auto source=builder.freeze();
    const auto boundary=field::materialize_boundary_matrix(source,core::Grade{1});
    // Declared unit metrics, lambda=1, complete grade-one carrier. This small
    // example materializes the Green system to expose its coordinate readout.
    // Prepared native field actions keep their existing augmented-map path.
    const auto system=numeric::ExactMatrix::identity(boundary.cols()).add(boundary.transpose().multiply(boundary));
    const numeric::PreparedExactSolve solve(system);const auto readout=solve.prepare_selected({3,0,3});
    const auto stats=solve.preparation_stats();
    std::cout<<"retained_occurrences="<<source.grade(core::Grade{1}).incidence_count()
             <<" pivot_candidates="<<stats.pivot_candidates<<" eliminated_rows="<<stats.eliminated_rows
             <<" peak_rational_entries="<<stats.peak_rational_entries<<" peak_index_entries="<<stats.peak_index_entries
             <<" peak_integer_bits="<<stats.peak_integer_bits<<'\n';
    for(const auto& input:std::vector<numeric::RationalVector>{{1,2,3,4},{4,3,2,1}}) {
        std::cout<<"coordinates=3,0,3 values=";
        for(const auto& q:readout.apply(input))std::cout<<' '<<q;
        std::cout<<'\n';
    }
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
