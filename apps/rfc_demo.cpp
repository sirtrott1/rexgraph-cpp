#include "rfc/rfc.hpp"
#include <iostream>

static void print_vector(const char* label, const rfc::numeric::RationalVector& value) {
    std::cout << label << " [";
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i) std::cout << ", ";
        std::cout << rfc::numeric::str(value[i]);
    }
    std::cout << "]\n";
}

int main() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", rfc::core::BoundaryPolicy::DeclaredExact, 3, 3);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    builder.add_grade_one(edge, {{1,true,true,false,1},{2,true,false,false,1}});
    builder.add_grade_one(edge, {{0,true,true,false,1},{2,true,false,false,1}});
    builder.add_higher(rfc::core::Grade{2}, face,
                       {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    const auto complex = builder.freeze();

    const auto bianchi = rfc::core::bianchi_through(complex, rfc::core::Grade{2});
    std::cout << "relational_complex grades=" << complex.grade_count()
              << " objects=" << complex.grade(rfc::core::Grade{0}).cell_count()
              << " relations=" << complex.grade(rfc::core::Grade{1}).cell_count()
              << " bianchi=" << (bianchi.satisfied() ? "true" : "false") << '\n';

    const std::vector<rfc::field::ExactMetric> metrics{
        rfc::field::ExactMetric::diagonal({1,2,3}),
        rfc::field::ExactMetric::diagonal({2,5,7}),
        rfc::field::ExactMetric::diagonal({11})};
    const rfc::field::ExactHodgeOperator hodge(complex, rfc::core::Grade{1}, metrics);
    rfc::numeric::RationalVector source(3,0); source[0]=1;
    const auto response = rfc::field::exact_green_apply(hodge, rfc::numeric::rational(2,3), source);
    print_vector("green_response", response.value);
    std::cout << "exact=" << (response.exact ? "true" : "false") << '\n';
}
