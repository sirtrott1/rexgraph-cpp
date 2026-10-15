#include "fixture.hpp"
#include <iostream>

int main() {
    rfc::numeric::ExactActionGuard guard;
    const auto declaration=finite_algebra_example::modular(3);
    const auto algebra=rfc::algebra::PreparedFiniteAlgebra::prepare(declaration.source,declaration.operations,guard);
    const auto field=algebra.check_field(0,1,declaration.values[0],declaration.values[1],guard);
    if(!field.valid())return 1;
    std::cout<<algebra.carrier().size()<<" retained identities; "
        <<algebra.source().grade(rfc::core::Grade{1}).cell_count()<<" original operation relations; "
        <<field.laws().size()<<" universal laws and all required inverse witnesses certified.\n";
}
