#include "rfc/core/builder.hpp"
#include "rfc/learning/integer_network.hpp"
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

int main() {
    try {
        using rfc::learning::OperatorChannelKind;
        const auto complex = make_triangle();
        const auto grade = rfc::core::Grade{1};
        const std::vector<OperatorChannelKind> candidates{
            OperatorChannelKind::Identity,
            OperatorChannelKind::HodgeDown,
            OperatorChannelKind::HodgeUp,
            OperatorChannelKind::Hodge
        };
        const auto basis = rfc::learning::reduce_operator_channels(complex, grade, candidates);
        require(basis.rank() == 2u, "filled triangle operator rank should be two");
        require(basis.independent_indices == std::vector<std::size_t>({0u,1u}), "unexpected operator pivots");
        require(basis.coordinates[0] == std::vector<rfc::numeric::Rational>({1,0}), "identity basis coordinate mismatch");
        require(basis.coordinates[1] == std::vector<rfc::numeric::Rational>({0,1}), "down basis coordinate mismatch");
        require(basis.coordinates[2] == std::vector<rfc::numeric::Rational>({3,-1}), "up relation should be 3I-Ldown");
        require(basis.coordinates[3] == std::vector<rfc::numeric::Rational>({3,0}), "Hodge relation should be 3I");
        require(basis.all_coordinates_integral(), "triangle reduction should remain integral");

        const auto reduced = rfc::learning::reduce_machine_channel_gains(basis, {2,1,-1,0});
        require(reduced == std::vector<std::int64_t>({-1,2}), "operator gain reduction mismatch");

        auto compiled = rfc::learning::compile_integer_relational_layer(
            complex, grade, candidates, {2,1,-1,0}, 0, rfc::learning::IntegerActivation::Identity);
        require(compiled.layer.channels().size() == 2u, "compiled layer did not remove redundant channels");
        rfc::field::MachineLatticeField x(complex, grade, {2,-1,3});
        const auto y = compiled.layer.forward(x);
        require(y.values() == std::vector<std::int64_t>({14,-1,11}), "compiled relational layer response mismatch");

        // Compile and execute a genuine two-layer exact integer relational network.
        rfc::learning::IntegerRelationalNetwork network(complex, grade);
        network.add_layer(rfc::learning::CompiledIntegerRelationalLayer(
            complex, grade, basis.basis_channels(), {1,1}, 0, rfc::learning::IntegerActivation::ReLU));
        network.add_layer(rfc::learning::CompiledIntegerRelationalLayer(
            complex, grade, basis.basis_channels(), {1,-1}, 0, rfc::learning::IntegerActivation::Identity));
        const auto network_y = network.forward(x);

        // Fit the same two-layer mapping exactly from examples with only the
        // independent relational operator channels available to the search.
        std::vector<rfc::learning::IntegerNetworkTrainingSample> samples;
        for (const auto& values : std::vector<std::vector<std::int64_t>>{
                 {2,-1,3}, {1,0,-2}, {-1,2,1}, {0,1,2}, {3,-2,0}, {-2,-1,1}}) {
            rfc::field::MachineLatticeField sample(complex, grade, values);
            samples.push_back({values, network.forward(sample).values()});
        }
        const auto fit = rfc::learning::fit_small_two_layer_network(
            complex, grade, basis, samples, -1, 1, rfc::learning::IntegerActivation::ReLU);
        require(fit.absolute_error == 0u, "two-layer exact relational network did not fit teacher");
        rfc::learning::IntegerRelationalNetwork learned(complex, grade);
        learned.add_layer(rfc::learning::CompiledIntegerRelationalLayer(
            complex, grade, basis.basis_channels(), fit.first_gains, fit.first_bias, rfc::learning::IntegerActivation::ReLU));
        learned.add_layer(rfc::learning::CompiledIntegerRelationalLayer(
            complex, grade, basis.basis_channels(), fit.second_gains, fit.second_bias, rfc::learning::IntegerActivation::Identity));
        for (const auto& sample : samples) {
            rfc::field::MachineLatticeField input(complex, grade, sample.input);
            require(learned.forward(input).values() == sample.target, "learned multi-layer network mismatch");
        }
        require(network_y.values() == network.forward(x).values(), "network forward should be deterministic");

        // Machine compilation must fail closed if an exact operator quotient
        // requires a non-integral gain; it may not approximate or coerce.
        rfc::learning::OperatorChannelBasis fractional;
        fractional.complex = &complex;
        fractional.grade = grade;
        fractional.candidates = {OperatorChannelKind::Identity, OperatorChannelKind::HodgeDown};
        fractional.independent_indices = {0u};
        fractional.coordinates = {{rfc::numeric::Rational{1}}, {rfc::numeric::Rational{1,2}}};
        bool rejected_fractional = false;
        try { (void)rfc::learning::reduce_machine_channel_gains(fractional, {0,1}); }
        catch (const std::domain_error&) { rejected_fractional = true; }
        require(rejected_fractional, "machine compiler accepted non-integral exact operator gain");

        std::cout << "operator basis and multi-layer network tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
