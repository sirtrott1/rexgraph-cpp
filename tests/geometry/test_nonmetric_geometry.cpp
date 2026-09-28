#include "rfc/core/builder.hpp"
#include "rfc/geometry/chain_map.hpp"
#include "rfc/geometry/column_expansion.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include "rfc/geometry/typed_realization.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static_assert(rfc::geometry::NonmetricGeometry<rfc::geometry::RelationalChainMap>);
static_assert(rfc::geometry::NonmetricGeometry<rfc::geometry::GradeColumnExpansion>);
static_assert(rfc::geometry::NonmetricGeometry<rfc::geometry::SemanticScaleRealization>);
static_assert(rfc::geometry::NonmetricGeometry<rfc::geometry::TypedAccession>);

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;

        core::RelationSignature sig;
        const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);

        core::RelationalComplexBuilder macro_builder(sig, core::SemanticScale{1});
        macro_builder.add_grade_one(edge, {{10,true,true,false,R{1}}, {20,true,false,false,R{1}}});
        const auto macro = macro_builder.freeze();

        core::RelationalComplexBuilder micro_builder(sig, core::SemanticScale{0});
        micro_builder.add_grade_one(edge, {{10,true,true,false,R{1}}, {15,true,false,false,R{1}}});
        micro_builder.add_grade_one(edge, {{15,true,true,false,R{1}}, {20,true,false,false,R{1}}});
        const auto micro = micro_builder.freeze();

        std::vector<geometry::LinearGradeMap> maps(2);
        maps[0] = {{{0,R{1}}}, {{2,R{1}}}};
        maps[1] = {{{0,R{1}}, {1,R{1}}}};
        geometry::RelationalChainMap J(macro, micro, std::move(maps));
        require(J.is_chain_map(), "semantic coordinate realization must be a chain map");
        geometry::SemanticScaleRealization scale(core::SemanticScale{1}, core::SemanticScale{0}, std::move(J));
        require(scale.realization().push_field(core::Grade{1}, {R{3}}) == numeric::RationalVector({R{3},R{3}}),
                "semantic realization amplitude mismatch");

        geometry::GradeColumnExpansion expansion(micro, core::Grade{1});
        require(expansion.certificate_exact(), "nonmetric expansion/lift certificate failed");
        require(geometry::GradeColumnExpansion::geometry_descriptor().kind == geometry::GeometryKind::ExpansionLift,
                "column expansion geometry descriptor mismatch");

        numeric::ExactMatrix A(1,2); A.set(0,0,R{1});
        numeric::ExactMatrix F(2,1); F.set(0,0,R{1});
        numeric::ExactMatrix Rmap(2,1); Rmap.set(0,0,R{1});
        geometry::TypedAccession typed("first", A, F);
        geometry::ReconstructingTypedView view(std::move(typed), Rmap);
        require(view.view.observe({R{7},R{9}}) == numeric::RationalVector({R{7}}),
                "typed accession must be usable without any metric");
        require(view.view.realize({R{5}}) == numeric::RationalVector({R{5},R{0}}),
                "typed physical realization must be usable without any metric");

        std::cout << "nonmetric geometry tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
