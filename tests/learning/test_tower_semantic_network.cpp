#include "rfc/geometry/chain_map.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/learning/semantic_network.hpp"
#include "rfc/learning/tower_network.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static rfc::core::RelationalComplex make_triangle() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", rfc::core::BoundaryPolicy::DeclaredExact, 3, 3);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    builder.add_grade_one(edge, {{1,true,true,false,1},{2,true,false,false,1}});
    builder.add_grade_one(edge, {{0,true,true,false,1},{2,true,false,false,1}});
    builder.add_higher(rfc::core::Grade{2}, face,
                       {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    return builder.freeze();
}

static rfc::core::RelationalComplex make_macro_interval() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder builder(signature, rfc::core::SemanticScale{1});
    builder.add_grade_one(edge, {{100,true,true,false,1},{200,true,false,false,1}});
    return builder.freeze();
}

static rfc::core::RelationalComplex make_micro_path() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder builder(signature, rfc::core::SemanticScale{0});
    builder.add_grade_one(edge, {{10,true,true,false,1},{11,true,false,false,1}});
    builder.add_grade_one(edge, {{11,true,true,false,1},{12,true,false,false,1}});
    return builder.freeze();
}

static rfc::geometry::SemanticScaleRealization make_scale(const rfc::core::RelationalComplex& macro,
                                                       const rfc::core::RelationalComplex& micro) {
    using rfc::geometry::LinearGradeMap;
    using rfc::geometry::LinearMapTerm;
    std::vector<LinearGradeMap> realize(2);
    realize[0] = {
        {LinearMapTerm{0,1}},
        {LinearMapTerm{2,1}}
    };
    realize[1] = {
        {LinearMapTerm{0,1}, LinearMapTerm{1,1}}
    };
    rfc::geometry::RelationalChainMap J(macro, micro, std::move(realize));
    require(J.is_chain_map(), "macro->micro realization should be a chain map");

    std::vector<LinearGradeMap> coarse(2);
    coarse[0] = {
        {LinearMapTerm{0,1}},
        {LinearMapTerm{0,rfc::numeric::Rational{1,2}}, LinearMapTerm{1,rfc::numeric::Rational{1,2}}},
        {LinearMapTerm{1,1}}
    };
    coarse[1] = {
        {LinearMapTerm{0,rfc::numeric::Rational{1,2}}},
        {LinearMapTerm{0,rfc::numeric::Rational{1,2}}}
    };
    rfc::geometry::RelationalChainMap R(micro, macro, std::move(coarse));
    require(R.is_chain_map(), "micro->macro coarsening should be a chain map");
    return rfc::geometry::SemanticScaleRealization(rfc::core::SemanticScale{1}, rfc::core::SemanticScale{0}, std::move(J), std::move(R));
}

int main() {
    try {
        using rfc::learning::TowerOperatorChannelKind;
        const auto triangle = make_triangle();
        const std::vector<TowerOperatorChannelKind> candidates{
            TowerOperatorChannelKind::Identity,
            TowerOperatorChannelKind::Lowering,
            TowerOperatorChannelKind::Raising,
            TowerOperatorChannelKind::Dirac
        };
        const auto basis = rfc::learning::reduce_tower_operator_channels(triangle, candidates);
        require(basis.rank() == 3u, "tower operator rank should remove Dirac redundancy");
        require(basis.independent_indices == std::vector<std::size_t>({0u,1u,2u}), "unexpected tower pivots");
        require(basis.coordinates[3] == std::vector<rfc::numeric::Rational>({0,1,1}), "Dirac should be lowering+raising");

        const auto reduced = rfc::learning::reduce_machine_tower_channel_gains(basis, {2,3,-1,4});
        require(reduced == std::vector<std::int64_t>({2,7,3}), "tower gain reduction mismatch");
        auto compiled = rfc::learning::compile_integer_tower_layer(triangle, candidates, {2,3,-1,4});
        require(compiled.layer.channels().size() == 3u, "compiled tower layer retained redundant Dirac channel");

        rfc::field::MachineLatticeTowerField tower(triangle, {
            {1,2,3},
            {2,-1,3},
            {4}
        });
        const auto compiled_out = compiled.layer.forward(tower);
        rfc::field::MachineLatticeTowerField direct(triangle);
        for (std::size_t c = 0; c < candidates.size(); ++c) {
            const auto response = rfc::learning::apply_tower_operator_channel(candidates[c], tower);
            for (std::size_t k = 0; k < direct.grade_count(); ++k) {
                rfc::field::add_scaled_tower_grade(direct.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)}),
                                                   response.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)}),
                                                   std::vector<std::int64_t>{2,3,-1,4}[c]);
            }
        }
        for (std::size_t k = 0; k < direct.grade_count(); ++k) {
            require(compiled_out.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)}) ==
                    direct.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)}),
                    "compiled tower output mismatch");
        }

        // The lattice Dirac square is the grade-wise lattice Hodge operator.
        const auto d2 = rfc::field::lattice_dirac_square_apply(tower);
        for (std::size_t k = 0; k < tower.grade_count(); ++k) {
            const auto grade = rfc::core::Grade{static_cast<std::uint16_t>(k)};
            const auto hodge = rfc::learning::apply_operator_channel(rfc::learning::OperatorChannelKind::Hodge, tower.field(grade));
            require(d2.grade_values(grade) == hodge.values(), "lattice D^2 != Hodge on tower grade");
        }

        // Stack two exact cross-grade layers.
        rfc::learning::IntegerTowerNetwork network(triangle);
        network.add_layer(rfc::learning::CompiledIntegerTowerLayer(
            triangle, basis.basis_channels(), {1,1,1}, 0, rfc::learning::IntegerActivation::ReLU));
        network.add_layer(rfc::learning::CompiledIntegerTowerLayer(
            triangle, basis.basis_channels(), {1,-1,1}, 0, rfc::learning::IntegerActivation::Identity));
        const auto first_run = network.forward(tower);
        const auto second_run = network.forward(tower);
        for (std::size_t k = 0; k < tower.grade_count(); ++k) {
            const auto grade = rfc::core::Grade{static_cast<std::uint16_t>(k)};
            require(first_run.grade_values(grade) == second_run.grade_values(grade), "tower network must be deterministic");
        }

        // Fit a genuinely cross-grade teacher using only the independent tower channels.
        rfc::learning::CompiledIntegerTowerLayer teacher(triangle, basis.basis_channels(), {1,1,-1});
        std::vector<rfc::learning::IntegerTowerTrainingSample> tower_samples;
        for (const auto& sample_values : std::vector<std::vector<std::vector<std::int64_t>>>{
                 {{1,0,-1},{2,1,0},{1}},
                 {{0,2,1},{-1,3,2},{0}},
                 {{2,-1,0},{1,0,-2},{2}},
                 {{-1,1,2},{0,2,1},{-1}}}) {
            rfc::field::MachineLatticeTowerField sample(triangle, sample_values);
            std::vector<std::vector<std::int64_t>> target;
            const auto teacher_out = teacher.forward(sample);
            for (std::size_t k = 0; k < teacher_out.grade_count(); ++k) {
                target.push_back(teacher_out.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)}));
            }
            tower_samples.push_back({sample_values, std::move(target)});
        }
        const auto tower_fit = rfc::learning::fit_small_integer_tower_layer(triangle, basis, tower_samples, -1, 1);
        require(tower_fit.absolute_error == 0u, "cross-grade tower learner did not fit exact teacher");

        const auto macro = make_macro_interval();
        const auto micro = make_micro_path();
        const auto scale = make_scale(macro, micro);
        require(scale.realization().is_chain_map(), "semantic realization lost chain naturality");
        require(scale.has_coarsening(), "semantic test fixture should have exact coarsening");

        // Macro edge is literally the two-edge micro chain.
        const auto macro_edge_rational = rfc::numeric::RationalVector{3};
        const auto micro_edge_rational = scale.realization().push_field(rfc::core::Grade{1}, macro_edge_rational);
        require(micro_edge_rational == rfc::numeric::RationalVector({3,3}), "semantic macro edge did not realize as micro chain");
        const auto roundtrip = scale.coarsening().push_field(rfc::core::Grade{1}, micro_edge_rational);
        require(roundtrip == macro_edge_rational, "semantic exact roundtrip failed");

        // Integer machine execution is allowed because the forward realization
        // is integral in the retained lattice coordinates.
        rfc::learning::CompiledMachineSemanticMap machine_scale(scale);
        rfc::field::MachineLatticeField macro_edge(macro, rfc::core::Grade{1}, {3});
        const auto micro_edge = machine_scale.push(macro_edge);
        require(micro_edge.values() == std::vector<std::int64_t>({3,3}), "machine semantic realization mismatch");

        const std::vector<rfc::learning::OperatorChannelKind> micro_candidates{
            rfc::learning::OperatorChannelKind::Identity,
            rfc::learning::OperatorChannelKind::HodgeDown
        };
        auto micro_plan = rfc::learning::compile_integer_relational_layer(micro, rfc::core::Grade{1}, micro_candidates, {1,1});
        rfc::learning::SemanticScaleIntegerStage stage(std::move(machine_scale), std::move(micro_plan.layer));
        const auto semantic_output = stage.forward(macro_edge);
        require(semantic_output.values().size() == 2u, "semantic stage output must live on micro grade");

        std::cout << "cross-grade Dirac and semantic-scale network tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
