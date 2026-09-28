#include "rfc/geometry/chain_map.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/local/fiber_system.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include "rfc/local/semantic_stalk_realization.hpp"
#include "rfc/local/tensor_field.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static void identity_block(rfc::numeric::ExactMatrix& out,
                           std::size_t row0,
                           std::size_t col0,
                           std::size_t dimension,
                           const rfc::numeric::Rational& scale = rfc::numeric::Rational{1}) {
    for (std::size_t i = 0; i < dimension; ++i) out.set(row0 + i, col0 + i, scale);
}

int main() {
    try {
        using namespace rfc;
        using R = numeric::Rational;

        core::RelationSignature sig;
        const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto branch = sig.add_branch_type("branch", 3, 3);

        core::RelationalComplexBuilder mb(sig, core::SemanticScale{2});
        mb.add_grade_one(branch, {{10,true,true,true,1},{20,true,false,false,R{1,2}},{30,true,false,false,R{1,2}}});
        const auto macro = mb.freeze();

        core::RelationalComplexBuilder ib(sig, core::SemanticScale{1});
        ib.add_grade_one(edge, {{10,true,true,false,1},{15,true,false,false,1}}); // H -> M
        ib.add_grade_one(branch, {{15,true,true,true,1},{20,true,false,false,R{1,2}},{30,true,false,false,R{1,2}}});
        const auto middle = ib.freeze();

        core::RelationalComplexBuilder ub(sig, core::SemanticScale{0});
        ub.add_grade_one(edge, {{10,true,true,false,1},{12,true,false,false,1}}); // H -> N
        ub.add_grade_one(edge, {{12,true,true,false,1},{15,true,false,false,1}}); // N -> M
        ub.add_grade_one(branch, {{15,true,true,true,1},{20,true,false,false,R{1,2}},{30,true,false,false,R{1,2}}});
        const auto micro = ub.freeze();

        std::vector<geometry::LinearGradeMap> m_to_i(2);
        m_to_i[0].resize(3);
        m_to_i[0][0] = {{0,R{1}}}; m_to_i[0][1] = {{2,R{1}}}; m_to_i[0][2] = {{3,R{1}}};
        m_to_i[1].resize(1); m_to_i[1][0] = {{0,R{1}},{1,R{1}}};
        geometry::RelationalChainMap J1(macro, middle, std::move(m_to_i));
        geometry::SemanticScaleRealization C1(core::SemanticScale{2}, core::SemanticScale{1}, J1);

        std::vector<geometry::LinearGradeMap> i_to_u(2);
        i_to_u[0].resize(4);
        i_to_u[0][0] = {{0,R{1}}}; i_to_u[0][1] = {{2,R{1}}}; i_to_u[0][2] = {{3,R{1}}}; i_to_u[0][3] = {{4,R{1}}};
        i_to_u[1].resize(2); i_to_u[1][0] = {{0,R{1}},{1,R{1}}}; i_to_u[1][1] = {{2,R{1}}};
        geometry::RelationalChainMap J2(middle, micro, std::move(i_to_u));
        geometry::SemanticScaleRealization C2(core::SemanticScale{1}, core::SemanticScale{0}, J2);

        local::RelationalFiberSystem macro_f(local::StalkBundle(macro, 2u));
        local::RelationalFiberSystem middle_f(local::StalkBundle(middle, 2u));
        local::RelationalFiberSystem micro_f(local::StalkBundle(micro, 2u));

        std::vector<numeric::ExactMatrix> JV1;
        JV1.emplace_back(8u,6u);
        identity_block(JV1[0],0,0,2); identity_block(JV1[0],4,2,2); identity_block(JV1[0],6,4,2);
        JV1.emplace_back(4u,2u);
        identity_block(JV1[1],0,0,2); identity_block(JV1[1],2,0,2);
        local::SemanticStalkRealization S1(macro_f, middle_f, C1, std::move(JV1));

        std::vector<numeric::ExactMatrix> JV2;
        JV2.emplace_back(10u,8u);
        identity_block(JV2[0],0,0,2); identity_block(JV2[0],4,2,2); identity_block(JV2[0],6,4,2); identity_block(JV2[0],8,6,2);
        JV2.emplace_back(6u,4u);
        identity_block(JV2[1],0,0,2); identity_block(JV2[1],2,0,2); identity_block(JV2[1],4,2,2);
        local::SemanticStalkRealization S2(middle_f, micro_f, C2, std::move(JV2));

        const auto S = local::SemanticStalkRealization::compose(S1,S2);
        require(S.is_natural(), "nested semantic stalk composition lost naturality");
        require(S.map(core::Grade{1}).rows() == 6u && S.map(core::Grade{1}).cols() == 2u,
                "nested semantic grade-one map dimensions mismatch");

        const numeric::RationalVector macro_value{R{2},R{4}};
        const auto micro_upper = S.realize(core::Grade{1}, macro_value);
        require(micro_upper == numeric::RationalVector({R{2},R{4},R{2},R{4},R{2},R{4}}),
                "one macro branch state did not realize on all nested micro relations");

        local::GradeTensorField macro_boundary(macro_f, core::Grade{1});
        local::GradeTensorField micro_boundary(micro_f, core::Grade{1});
        const auto lhs = micro_boundary.apply(micro_upper);
        const auto rhs = S.realize(core::Grade{0}, macro_boundary.apply(macro_value));
        require(lhs == rhs, "nested semantic stalk/tensor boundary naturality failed");

        const auto micro_metric = numeric::ExactMatrix::identity(6u);
        const auto pulled = S.metric_pullback(core::Grade{1}, micro_metric);
        require(pulled.rows() == 2u && pulled.cols() == 2u && pulled.get(0,0) == R{3} && pulled.get(1,1) == R{3} && pulled.get(0,1) == 0,
                "nested block metric did not scale by the three realized micro relation copies");

        const auto staged_metric = S1.metric_pullback(core::Grade{1},
            S2.metric_pullback(core::Grade{1}, micro_metric));
        require(local::exact_matrix_equal(staged_metric, pulled),
                "nested semantic metric pullback is not associative across scale");

        std::cout << "semantic stalk/tensor realization tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
