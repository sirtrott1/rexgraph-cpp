#include "rfc/core/builder.hpp"
#include "rfc/learning/branching_layer.hpp"
#include "rfc/local/fiber_system.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
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
        learning::BranchTypeGainLayer layer(fibers, core::Grade{1});
        layer.set_gain(branch, R{3});
        const auto out = layer.apply(numeric::RationalVector{R{8}});
        require(out == numeric::RationalVector({R{-24},R{6},R{12},R{6}}),
                "branch-aware layer failed to transform once before fan-out");
        std::cout << "branch-aware learning tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
