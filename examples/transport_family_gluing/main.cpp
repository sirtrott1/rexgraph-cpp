#include "fixture.hpp"
#include <iostream>

int main() {
    using namespace rfc_example::transport_family;
    Guard guard;
    std::vector<Fit> members{family(2,guard),family(2,guard)};
    rfc::learning::PreparedTransportFamilyGluing glued(
        members,{triangle_interface(0,1)},guard);
    const auto fields=glued.materialize({rfc::numeric::Rational{7,3}},guard);
    const auto claim=glued.classify({{0,0,1},{1,0,-1}},0,guard);
    if(!glued.consistent() || glued.kernel().cols()!=1 ||
       claim.status!=rfc::learning::TransportParameterRelationStatus::Forced ||
       rfc::local::GradeTensorField(fields[0].fibers(),rfc::core::Grade{2}).matrix()!=
       rfc::local::GradeTensorField(fields[1].fibers(),rfc::core::Grade{2}).matrix())return 1;
    std::cout<<"Two retained grade-two families; "<<glued.kernel().cols()
             <<" shared free direction; exact occurrence agreement certified.\n";
}
