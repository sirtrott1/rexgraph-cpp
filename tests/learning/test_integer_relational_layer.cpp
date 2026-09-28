#include "rfc/core/builder.hpp"
#include "rfc/field/lattice_boundary.hpp"
#include "rfc/learning/integer_perceptron.hpp"
#include "rfc/learning/integer_relational_layer.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

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

int main() {
    try {
        const auto complex = make_triangle();
        rfc::field::MachineLatticeField edge_field(complex, rfc::core::Grade{1}, {2,-1,3});
        rfc::field::LatticeBoundaryOperator B1(complex, rfc::core::Grade{1});
        const auto node_field = B1.apply(edge_field);
        require(node_field.values() == std::vector<std::int64_t>({-5,3,2}), "machine lattice boundary mismatch");

        const auto back = B1.transpose_apply(node_field);
        require(back.values() == std::vector<std::int64_t>({8,-1,7}), "machine lattice transpose mismatch");

        rfc::learning::MachineIntegerRelationalLayer layer(complex, rfc::core::Grade{1}, 2, 1, -1, 0);
        const auto output = layer.forward(edge_field);
        // Computed as 2*x + B1^T B1*x - B2 B2^T*x.
        require(output.values() == std::vector<std::int64_t>({14,-1,11}), "integer relational propagation mismatch");

        // Learn the propagation gains exactly from several relational fields.
        rfc::learning::MachineIntegerRelationalLayer teacher(complex, rfc::core::Grade{1}, 2, 1, -1, 0);
        std::vector<rfc::learning::IntegerRelationalTrainingSample> layer_samples;
        for (const auto& values : std::vector<std::vector<std::int64_t>>{{2,-1,3},{1,0,-2},{-1,2,1}}) {
            rfc::field::MachineLatticeField sample_field(complex, rfc::core::Grade{1}, values);
            layer_samples.push_back({values, teacher.forward(sample_field).values()});
        }
        const auto layer_fit = rfc::learning::fit_small_integer_relational_layer(
            complex, rfc::core::Grade{1}, layer_samples, -2, 2);
        require(layer_fit.absolute_error == 0u, "integer relational gains were not fit exactly");

        // On the filled triangle, L_down + L_up = 3 I on grade one. Therefore
        // (2,1,-1) and (-1,2,0) are exactly the same operator. This is a
        // structural parameter non-identifiability, not a training error.
        rfc::learning::MachineIntegerRelationalLayer equivalent(complex, rfc::core::Grade{1}, -1, 2, 0, 0);
        for (const auto& sample : layer_samples) {
            rfc::field::MachineLatticeField sample_field(complex, rfc::core::Grade{1}, sample.input);
            require(equivalent.forward(sample_field).values() == teacher.forward(sample_field).values(),
                    "exact relational operator equivalence failed");
        }

        // Exact integer perceptron on sparse relational-style features.
        std::vector<rfc::learning::SparseIntSample> samples{
            {{{0, 2}, {1, 1}}, 1},
            {{{0, 1}, {1, 2}}, 1},
            {{{0,-2}, {1,-1}},-1},
            {{{0,-1}, {1,-2}},-1}
        };
        rfc::learning::ExactIntegerPerceptron perceptron(2);
        std::size_t mistakes = 1;
        for (std::size_t epoch = 0; epoch < 16 && mistakes != 0; ++epoch) mistakes = perceptron.train_epoch(samples);
        require(mistakes == 0u, "exact integer perceptron failed to separate dataset");
        for (const auto& sample : samples) require(perceptron.predict(sample.features) == sample.label, "integer perceptron prediction mismatch");

        // Exact arbitrary-precision lattice path agrees with machine path.
        rfc::field::ExactLatticeField exact_edge(complex, rfc::core::Grade{1}, {2,-1,3});
        const auto exact_node = B1.apply(exact_edge);
        for (std::size_t i = 0; i < exact_node.values().size(); ++i) {
            require(exact_node.values()[i] == node_field.values()[i], "exact/machine lattice disagreement");
        }

        std::cout << "integer relational learning tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
