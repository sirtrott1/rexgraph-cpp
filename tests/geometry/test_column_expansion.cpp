#include "rfc/geometry/column_expansion.hpp"
#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template <class F> static void require_throws(F&& fn, const char* message) {
    bool threw=false; try { fn(); } catch (const std::exception&) { threw=true; }
    if (!threw) throw std::runtime_error(message);
}

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;

        core::RelationSignature sig;
        const auto branch = sig.add_branch_type("branch", 2, 0);
        const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto generic = sig.add_type("generic", core::BoundaryPolicy::DeclaredExact, 0, 0);

        core::RelationalComplexBuilder builder(sig);
        builder.add_grade_one(branch, {
            {10,true,true,true,R{1}},
            {11,true,false,false,R{1,4}},
            {12,true,false,false,R{1,2}},
            {13,true,false,false,R{1,4}}});
        builder.add_grade_one(edge, {{10,true,true,false,R{1}},{11,true,false,false,R{1}}});
        builder.add_grade_one(edge, {{10,true,true,false,R{1}},{12,true,false,false,R{1}}});
        builder.add_grade_one(edge, {{10,true,true,false,R{1}},{13,true,false,false,R{1}}});
        builder.add_higher(core::Grade{2}, branch, {
            {0,true,true,true,R{1}},
            {1,true,false,false,R{1,4}},
            {2,true,false,false,R{1,2}},
            {3,true,false,false,R{1,4}}});
        const auto complex = builder.freeze();
        require(core::bianchi_through(complex,core::Grade{2}).satisfied(), "fixture must be chain compatible");

        geometry::GradeColumnExpansion ex1(complex, core::Grade{1});
        require(ex1.certificate_exact(), "grade-one exact expansion certificate failed");
        require(ex1.references()[0].has_value() && *ex1.references()[0] == 0u,
                "branch expansion should prefer semantic head as reference");
        const auto range0 = ex1.leg_range(0);
        require(range0.second-range0.first == 3u, "zero-sum arity-four branch should expand to three pair legs");
        require(ex1.lift_matrix().get(range0.first+0u,0) == R{1,4}, "declared branch share missing from lift");
        require(ex1.lift_matrix().get(range0.first+1u,0) == R{1,2}, "declared branch share missing from lift");
        require(ex1.lift_matrix().get(range0.first+2u,0) == R{1,4}, "declared branch share missing from lift");

        geometry::GradeColumnExpansion tail_ref(complex, core::Grade{1}, {2u,std::nullopt,std::nullopt,std::nullopt});
        const auto tail_range = tail_ref.leg_range(0);
        bool saw_negative_head = false;
        for (std::size_t a=tail_range.first;a<tail_range.second;++a) {
            const auto& leg=tail_ref.legs()[a];
            if (leg.target_lower && *leg.target_lower==0u && tail_ref.lift_matrix().get(a,0)==R{-1}) saw_negative_head=true;
        }
        require(saw_negative_head, "tail reference must retain the negative head coefficient");

        const numeric::RationalVector primary{R{8},R{2},R{3},R{5}};
        const auto expanded=ex1.expand(primary);
        require(ex1.consistent(expanded), "retained expanded field consistency failed");
        require(ex1.boundary_from_expanded(expanded)==complex.boundary_apply(core::Grade{1},primary),
                "expanded pair coordinates changed boundary action");
        require(ex1.primary_reconstructible_from_legs(), "nonzero columns should reconstruct primary amplitudes in fixture");
        require(ex1.primary_reconstruction().apply(expanded.legs)==primary, "primary reconstruction from constrained legs failed");

        numeric::RationalVector metric_diag{R{2},R{3},R{5},R{7}};
        const auto M=numeric::ExactMatrix::diagonal(metric_diag);
        require(ex1.primary_metric_pullback(M)==ex1.direct_primary_metric(M),
                "pair-leg metric pullback does not equal direct primary metric");

        geometry::GradeColumnExpansion ex2(complex, core::Grade{2});
        require(ex2.certificate_exact(), "higher-grade exact expansion certificate failed");
        const auto B1=geometry::exact_boundary_matrix(complex,core::Grade{1});
        require(B1.multiply(ex2.dictionary()).multiply(ex2.lift_matrix()).rank()==0,
                "grouped higher expansion broke chain condition");
        bool individual_leg_not_closed=false;
        for (std::size_t a=0;a<ex2.dictionary().cols();++a) {
            numeric::RationalVector e(ex2.dictionary().cols(),R{0}); e[a]=1;
            if (!numeric::is_zero(B1.apply(ex2.dictionary().apply(e)))) { individual_leg_not_closed=true; break; }
        }
        require(individual_leg_not_closed, "fixture should demonstrate that internal pair legs need not be individually closed");

        // Witness residual: a one-support nonzero-sum boundary expands to one witness leg.
        core::RelationalComplexBuilder witness_builder(sig);
        witness_builder.add_grade_one(generic, {{99,true,false,false,R{1}}});
        const auto witness_complex=witness_builder.freeze();
        geometry::GradeColumnExpansion witness_exp(witness_complex,core::Grade{1});
        require(witness_exp.legs().size()==1u && witness_exp.legs()[0].kind==geometry::ExpansionLegKind::Witness,
                "witness boundary should produce one residual witness leg");

        // Zero boundary keeps primary identity but has no internal expansion and cannot be reconstructed from legs alone.
        core::RelationalComplexBuilder zero_builder(sig);
        zero_builder.add_grade_one(generic, {
            {77,true,false,false,R{1}},
            {77,true,true,false,R{1}}});
        const auto zero_complex=zero_builder.freeze();
        geometry::GradeColumnExpansion zero_exp(zero_complex,core::Grade{1});
        require(zero_exp.legs().empty(), "zero boundary should have empty expansion");
        require(!zero_exp.primary_reconstructible_from_legs(), "empty zero-boundary expansion must not erase retained primary identity");
        const auto retained=zero_exp.expand(numeric::RationalVector{R{9}});
        require(retained.primary==numeric::RationalVector({R{9}}) && retained.legs.empty(),
                "zero-boundary primary amplitude was not retained separately");
        require_throws([&]{ (void)zero_exp.primary_reconstruction(); }, "zero-boundary relation reconstructed from absent legs");

        // Deterministic adversarial coverage: arbitrary exact rational columns,
        // including nonzero-sum witness residuals, must satisfy P Lambda = B.
        std::mt19937 rng(0xC011u);
        core::RelationalComplexBuilder random_builder(sig);
        for (std::size_t relation=0; relation<64; ++relation) {
            const std::size_t arity=1u+(rng()%6u);
            std::vector<core::BoundaryOccurrenceInput> occ;
            occ.reserve(arity);
            for (std::size_t i=0;i<arity;++i) {
                const long numerator=1L+static_cast<long>(rng()%5u);
                const long denominator=1L+static_cast<long>(rng()%5u);
                const bool negative=(rng()&1u)!=0u;
                occ.push_back(core::BoundaryOccurrenceInput{
                    static_cast<core::IdentityKey>(1000u+relation*8u+i),true,negative,false,R{numerator,denominator},{}});
            }
            random_builder.add_grade_one(generic,occ);
        }
        const auto random_complex=random_builder.freeze();
        geometry::GradeColumnExpansion random_expansion(random_complex,core::Grade{1});
        require(random_expansion.certificate_exact(),"random exact high-arity expansion failed certificate");
        numeric::RationalVector random_primary(random_complex.grade(core::Grade{1}).cell_count(),R{0});
        for(std::size_t i=0;i<random_primary.size();++i) random_primary[i]=R{static_cast<long>(1+(i%7))};
        const auto random_lift=random_expansion.expand(random_primary);
        require(random_expansion.boundary_from_expanded(random_lift)==random_complex.boundary_apply(core::Grade{1},random_primary),
                "random retained expansion changed exact boundary action");

        std::cout << "exact high-arity column expansion tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
