#include "rfc/geometry/column_expansion.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/view/typed_accession.hpp"
#include <iostream>
#include <vector>

static rfc::numeric::ExactMatrix selector(std::size_t ambient,const std::vector<std::size_t>& coords) {
    rfc::numeric::ExactMatrix A(coords.size(),ambient);
    for(std::size_t i=0;i<coords.size();++i) A.set(i,coords[i],rfc::numeric::Rational{1});
    return A;
}

int main() {
    using namespace rfc;
    using R=numeric::Rational;

    core::RelationSignature sig;
    const auto branch=sig.add_branch_type("branch",4,4);
    core::RelationalComplexBuilder builder(sig);
    builder.add_grade_one(branch,{
        {0,true,true,true,R{1}},
        {1,true,false,false,R{1,4}},
        {2,true,false,false,R{1,2}},
        {3,true,false,false,R{1,4}}});
    const auto complex=builder.freeze();
    geometry::GradeColumnExpansion expansion(complex,core::Grade{1});
    const auto lifted=expansion.expand({R{8}});
    const auto boundary=expansion.boundary_from_expanded(lifted);
    std::cout << "primary=8\nlegs=";
    for(const auto& q:lifted.legs) std::cout << ' ' << numeric::str(q);
    std::cout << "\nboundary=";
    for(const auto& q:boundary) std::cout << ' ' << numeric::str(q);
    std::cout << "\n";

    const auto Aa=selector(4,{0,1,2});
    const auto Ab=selector(4,{2,3});
    const auto Fa=Aa.transpose(), Fb=Ab.transpose();
    const auto Sinv=numeric::ExactMatrix::diagonal({R{1},R{1},R{1,2},R{1}});
    view::ReconstructingTypedFamily family({
        view::ReconstructingTypedView(view::TypedAccession("a",Aa,Fa),Sinv.multiply(Fa)),
        view::ReconstructingTypedView(view::TypedAccession("b",Ab,Fb),Sinv.multiply(Fb))});
    const numeric::RationalVector x{R{2},R{3},R{5},R{7}};
    const auto observed=family.observe(x);
    const auto restored=family.reconstruct(observed);
    std::cout << "typed_observed=";
    for(const auto& q:observed) std::cout << ' ' << numeric::str(q);
    std::cout << "\nreconstructed=";
    for(const auto& q:restored) std::cout << ' ' << numeric::str(q);
    std::cout << "\nconsistent=" << (family.consistent(observed)?"true":"false") << "\n";
    return 0;
}
