#include "rfc/core/builder.hpp"
#include "rfc/database/store.hpp"
#include "rfc/field/exact_dirac.hpp"
#include "rfc/machine/packed_bits.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        namespace core = rfc::core;
        namespace field = rfc::field;
        namespace numeric = rfc::numeric;
        core::RelationSignature signature;
        const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto face = signature.add_type("face", core::BoundaryPolicy::DeclaredExact, 4, 4);
        core::RelationalComplexBuilder weighted(signature);
        weighted.add_grade_one(edge, {{0,true,true,1},{1,true,false,numeric::rational(1,2)}});
        const auto one = weighted.freeze();
        require(one.incidence_coefficient(core::Grade{1},0) == -1, "negative orientation failed");
        require(one.incidence_coefficient(core::Grade{1},1) == numeric::rational(1,2), "exact magnitude failed");

        core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(edge, {{0,true,true,1},{1,true,false,1}});
        builder.add_grade_one(edge, {{1,true,true,1},{2,true,false,1}});
        builder.add_grade_one(edge, {{2,true,true,1},{3,true,false,1}});
        builder.add_grade_one(edge, {{3,true,true,1},{0,true,false,1}});
        builder.add_higher(core::Grade{2},face,
                          {{0,true,false,1},{1,true,false,1},{2,true,false,1},{3,true,false,1}});
        const auto complex = builder.freeze();
        const field::BoundaryOperator b1(complex,core::Grade{1});
        const field::BoundaryOperator b2(complex,core::Grade{2});
        require(numeric::is_zero(b1.apply(b2.apply(numeric::RationalVector{numeric::Rational{1}}))),
                "chain condition failed");

        const std::vector<field::ExactMetric> metrics{
            field::ExactMetric::diagonal({1,2,3,4}),
            field::ExactMetric::diagonal({2,3,5,7}),
            field::ExactMetric::diagonal({11})};
        const field::ExactHodgeOperator hodge(complex,core::Grade{1},metrics);
        const auto lambda = numeric::rational(2,3);
        const numeric::RationalVector source{1,0,0,0};
        const auto solution = field::exact_green_apply(hodge,lambda,source);
        require(solution.exact, "weighted exact Green solve failed");
        require(numeric::add(solution.value,numeric::scale(lambda,hodge.apply(solution.value))) == source,
                "weighted Green residual nonzero");

        const numeric::RationalVector x{numeric::rational(1,2),numeric::rational(-2,3),
                                        numeric::rational(3,5),numeric::rational(4,7)};
        const numeric::RationalVector y{numeric::rational(5,3),numeric::rational(-1,2),
                                        numeric::rational(2,7),numeric::rational(8,9)};
        require(metrics[0].inner(b1.apply(x),y) == metrics[1].inner(x,b1.adjoint(y,metrics[0],metrics[1])),
                "metric adjoint identity failed");

        const field::ExactTowerField seed{{{numeric::rational(1,2),0,0,0},
                                           {1,0,0,0},{numeric::rational(2,5)}}};
        const auto square = field::exact_dirac_square_apply(complex,metrics,seed);
        require(square.grades[1] == hodge.apply(seed.grades[1]), "D squared equals Hodge failed");

        rfc::machine::PackedBits selected(130);
        selected.set(65,true);
        require(selected.count() == 1 && selected.get(65), "packed selection failed");
        rfc::database::ExactRelationalMemory memory(complex);
        const core::CellId cell{core::Grade{1},0};
        memory.assert_exact(cell,{numeric::rational(11,8),{}});
        require(memory.lookup(cell)->value == numeric::rational(11,8), "exact retained payload failed");
        std::cout << "canonical exact smoke passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
