#include "rfc/core/builder.hpp"
#include "rfc/field/boundary.hpp"
#include "rfc/field/dirac.hpp"
#include "rfc/field/field.hpp"
#include "rfc/field/green.hpp"
#include "rfc/field/hodge.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
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

        // Sparse field state stores only nonzero amplitudes.  Structural
        // existence remains the full retained relation carrier.
        const auto sparse_source_values = rfc::numeric::SparseRationalVector::basis(3u, 0u);
        const rfc::field::ExactSparseField sparse_source(complex, rfc::core::Grade{1}, sparse_source_values);
        require(sparse_source.stored_nonzeros() == 1u, "sparse field nonzero count mismatch");
        require(sparse_source.tensor().support().count() == 3u,
                "sparse field must retain the full primary-cell carrier");
        require(sparse_source.tensor().state(1u).has_value() && *sparse_source.tensor().state(1u) == 0,
                "zero amplitude on an existing relation must not become absence");

        rfc::field::BoundaryOperator B1(complex, rfc::core::Grade{1});
        const auto sparse_boundary_field = B1.apply(sparse_source);
        require(sparse_boundary_field.stored_nonzeros() == 2u, "sparse boundary support mismatch");
        rfc::numeric::RationalVector dense_sparse_source(3u, rfc::numeric::Rational{0});
        dense_sparse_source[0] = 1;
        require(sparse_boundary_field.values().dense() == B1.apply(dense_sparse_source),
                "sparse boundary action disagrees with dense reference");
        rfc::numeric::WorkCounters boundary_work;
        const auto counted_boundary = complex.boundary_apply_sparse_counted(
            rfc::core::Grade{1}, sparse_source.values().view(), boundary_work);
        require(counted_boundary.dense() == sparse_boundary_field.values().dense(),
                "counted sparse boundary mismatch");
        require(boundary_work.active_coordinates == 1u && boundary_work.incidences_touched == 2u,
                "sparse boundary work must follow one active relation and its incidences");

        rfc::field::DiagonalMetric M0({1,2,3});
        rfc::field::DiagonalMetric M1({2,5,7});
        rfc::field::DiagonalMetric M2({11});
        rfc::field::HodgeOperator L1(complex, rfc::core::Grade{1}, M0, M1, &M2);
        rfc::field::ResolventOperator A(L1, rfc::numeric::rational(2,3));
        rfc::numeric::RationalVector source(3,0); source[0]=1;
        const auto solution = rfc::field::exact_cg(A, source);
        require(solution.exact, "exact Green resolvent failed");
        require(rfc::numeric::is_zero(rfc::numeric::sub(A.apply(solution.value), source)), "Green residual nonzero");

        rfc::field::TowerField seed{{
            {rfc::numeric::rational(1,2),0,0},
            {1,0,0},
            {rfc::numeric::rational(2,5)}}};
        std::vector<rfc::field::DiagonalMetric> metrics;
        metrics.emplace_back(std::vector<rfc::numeric::Rational>{1,2,3});
        metrics.emplace_back(std::vector<rfc::numeric::Rational>{2,5,7});
        metrics.emplace_back(std::vector<rfc::numeric::Rational>{11});
        const auto D1 = rfc::field::dirac_apply(complex, metrics, seed);
        const auto D2 = rfc::field::dirac_apply(complex, metrics, D1);
        require(D2.grades[1] == L1.apply(seed.grades[1]), "native D^2=L failed at grade one");

        std::cout << "field calculus tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
