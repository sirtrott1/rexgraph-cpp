#include "rfc/core/boundary_fiber.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/temporal/composite_change.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static rfc::core::RelationalComplex branch_with_shares(const rfc::numeric::Rational& a,
                                                        const rfc::numeric::Rational& b) {
    rfc::core::RelationSignature sig;
    const auto branch = sig.add_branch_type("branch", 3, 3, rfc::core::BoundaryPolicy::DeclaredExact);
    rfc::core::RelationalComplexBuilder builder(sig);
    builder.add_grade_one(branch, {
        {1,true,true,true,1},
        {2,true,false,false,a},
        {3,true,false,false,b}});
    return builder.freeze();
}

int main() {
    try {
        using R = rfc::numeric::Rational;
        const auto old_complex = branch_with_shares(R{1,2},R{1,2});
        const auto new_complex = branch_with_shares(R{1,4},R{3,4});
        const rfc::core::BoundaryOccurrenceFiber old_fiber(old_complex, rfc::core::Grade{1}, 0);
        const rfc::core::BoundaryOccurrenceFiber new_fiber(new_complex, rfc::core::Grade{1}, 0);
        const auto change = rfc::temporal::composite_boundary_change(old_fiber.composite_state(), new_fiber.composite_state());
        require(rfc::numeric::is_zero(change.existence), "pure branch share edit changed existence channel");
        require(rfc::numeric::is_zero(change.orientation), "pure branch share edit changed orientation channel");
        require(change.magnitude == rfc::numeric::RationalVector({R{0},R{-1,4},R{1,4}}), "branch share magnitude channels mismatch");
        require(change.aggregate == change.magnitude, "branch share aggregate mismatch");
        std::cout << "branch composite temporal change tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
