#include "rfc/core/builder.hpp"
#include "rfc/learning/branching_layer.hpp"
#include "rfc/local/fiber_system.hpp"
#include "rfc/local/occurrence_cover.hpp"
#include "rfc/local/tensor_field.hpp"
#include <iostream>

int main() {
    using namespace rfc;
    using R = numeric::Rational;

    core::RelationSignature sig;
    const auto branch = sig.add_branch_type("branch", 4, 4, core::BoundaryPolicy::DeclaredExact);
    core::RelationalComplexBuilder builder(sig);
    builder.add_grade_one(branch, {
        {1,true,true,true,R{1}},
        {2,true,false,false,R{1,4}},
        {3,true,false,false,R{1,2}},
        {4,true,false,false,R{1,4}}});
    const auto complex = builder.freeze();

    local::RelationalFiberSystem fibers(local::StalkBundle(complex, 1));
    local::GradeTensorField tensor(fibers, core::Grade{1});
    const auto boundary = tensor.apply(numeric::RationalVector{R{8}});
    std::cout << "branch_boundary=";
    for (const auto& v : boundary) std::cout << ' ' << v;
    std::cout << '\n';

    learning::BranchTypeGainLayer layer(fibers, core::Grade{1});
    layer.set_gain(branch, R{3});
    const auto learned = layer.apply(numeric::RationalVector{R{8}});
    std::cout << "branch_gain3=";
    for (const auto& v : learned) std::cout << ' ' << v;
    std::cout << '\n';

    local::StalkBundle vector_stalks(complex, 2);
    local::RelationalFiberSystem vector_fibers(vector_stalks);
    const auto block = vector_fibers.branch_tensor(core::Grade{1}, 0);
    const auto vector_boundary = block.apply(numeric::RationalVector{R{2},R{4}});
    std::cout << "vector_branch=";
    for (const auto& v : vector_boundary) std::cout << ' ' << v;
    std::cout << '\n';
    return 0;
}
