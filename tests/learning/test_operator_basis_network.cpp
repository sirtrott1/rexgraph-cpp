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

static void test_batch_execution(const rfc::core::RelationalComplex& complex) {
    using namespace rfc;
    using C = learning::OperatorChannelKind;
    std::size_t checks = 0;
    for (unsigned k = 0; k < complex.grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        const auto n = complex.grade(grade).cell_count();
        for (auto activation : {learning::IntegerActivation::Identity, learning::IntegerActivation::ReLU, learning::IntegerActivation::Sign}) {
            learning::IntegerRelationalNetwork network(complex, grade);
            const std::vector<C> channels{C::Hodge, C::Identity, C::HodgeDown, C::HodgeUp, C::Hodge, C::HodgeDown};
            const std::vector<std::int64_t> gains{1, -1, 2, -1, -1, 0};
            network.add_layer({complex, grade, channels, gains, 2, activation});
            network.add_layer({complex, grade, {C::Identity}, {2}, -1, learning::IntegerActivation::Identity});
            learning::IntegerNetworkWorkspace work(complex, grade, 7);
            const auto retained = work.retained_slots();
            for (const auto b : {1u, 7u, 2u, 7u}) {
                std::vector<std::int64_t> input(n * b);
                for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<std::int64_t>(i % 7) - 3;
                const auto out = network.forward_batch(input, b, work);
                require(work.retained_slots() == retained, "batch workspace changed capacity");
                for (unsigned sample = 0; sample < b; ++sample) {
                    std::vector<std::int64_t> column(n);
                    std::vector<numeric::Integer> exact(n);
                    for (std::size_t i = 0; i < n; ++i) exact[i] = column[i] = input[i * b + sample];
                    const auto scalar = network.forward(field::MachineLatticeField(complex, grade, column));
                    std::vector<numeric::Integer> reference(n, 2);
                    for (std::size_t c = 0; c < channels.size(); ++c) {
                        const auto response = learning::apply_operator_channel(channels[c], field::ExactLatticeField(complex, grade, exact));
                        for (std::size_t i = 0; i < n; ++i) reference[i] += numeric::Integer{gains[c]} * response.values()[i];
                    }
                    for (std::size_t i = 0; i < n; ++i) {
                        const auto hidden = learning::apply_integer_activation(activation, reference[i].get_si());
                        require(out[i * b + sample] == scalar.values()[i] && out[i * b + sample] == 2 * hidden - 1,
                                "batch, scalar and GMP reference disagree");
                        ++checks;
                    }
                }
            }
        }
    }
    const core::Grade grade{1};
    learning::IntegerRelationalNetwork network(complex, grade);
    network.add_layer({complex, grade, {C::Identity}, {2}});
    learning::IntegerNetworkWorkspace work(complex, grade, 1);
    std::vector<std::int64_t> input(complex.grade(grade).cell_count(), 1);
    input[0] = std::numeric_limits<std::int64_t>::max();
    bool rejected = false;
    try { (void)network.forward_batch(input, 1, work); } catch (const std::overflow_error&) { rejected = true; }
    require(rejected, "batch overflow accepted");
    input[0] = 1;
    require(network.forward_batch(input, 1, work)[0] == 2, "workspace failed to recover after overflow");
    rejected = false;
    try { (void)network.forward_batch(input, 0, work); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "zero batch accepted");
    rejected = false;
    try { (void)network.forward_batch(input, 2, work); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "batch over capacity accepted");
    auto other = complex;
    learning::IntegerNetworkWorkspace wrong(other, grade, 1);
    rejected = false;
    try { (void)network.forward_batch(input, 1, wrong); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "foreign workspace accepted");
    rejected = false;
    try { (void)network.forward_batch(input, 1, work, [] { throw std::runtime_error("cancelled"); }); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected && network.forward_batch(input, 1, work)[0] == 2, "batch cancellation recovery failed");
    field::LatticeBoundaryOperator boundary(complex, grade);
    rejected = false;
    try { boundary.apply_batch_into(input, input, 1); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "aliased lattice buffers accepted");
    core::RelationSignature signature;
    const auto type = signature.add_type("wide", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    numeric::Integer magnitude{1}; magnitude <<= 80;
    builder.add_grade_one(type, {{0,true,false,numeric::Rational{1}}, {1,true,true,numeric::Rational{magnitude}}});
    builder.declare_grade(core::Grade{2});
    const auto wide = builder.freeze();
    field::LatticeBoundaryOperator wide_boundary(wide, grade);
    std::vector<std::int64_t> zero(1, 0), lower(2, 0);
    wide_boundary.apply_batch_into(zero, lower, 1);
    require(lower == std::vector<std::int64_t>({0,0}), "zero field attempted a wide coefficient conversion");
    rejected = false;
    try { wide_boundary.transpose_batch_into(lower, zero, 1); } catch (const std::overflow_error&) { rejected = true; }
    require(rejected, "wide transpose coefficient accepted");
    learning::IntegerRelationalNetwork fixed(wide, grade);
    fixed.add_layer({wide, grade, {C::HodgeDown,C::Identity}, {0,1}});
    learning::IntegerNetworkWorkspace wide_work(wide, grade, 1);
    require(fixed.forward_batch(std::vector<std::int64_t>{2}, 1, wide_work)[0] == 2, "unused wide channel was evaluated");
    std::cout << "batch exact comparisons=" << checks << '\n';
}

int main() {
    try {
        using rfc::learning::OperatorChannelKind;
        const auto complex = make_triangle();
        test_batch_execution(complex);
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
