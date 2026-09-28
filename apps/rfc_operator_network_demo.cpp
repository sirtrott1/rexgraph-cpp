#include "rfc/core/builder.hpp"
#include "rfc/learning/integer_network.hpp"
#include <iostream>
#include <vector>

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
    using namespace rfc::learning;
    const auto complex = make_triangle();
    const auto grade = rfc::core::Grade{1};
    const std::vector<OperatorChannelKind> candidates{
        OperatorChannelKind::Identity,
        OperatorChannelKind::HodgeDown,
        OperatorChannelKind::HodgeUp,
        OperatorChannelKind::Hodge};
    const auto basis = reduce_operator_channels(complex, grade, candidates);
    std::cout << "candidate_channels=" << candidates.size() << " independent_channels=" << basis.rank() << '\n';
    for (std::size_t j = 0; j < candidates.size(); ++j) {
        std::cout << operator_channel_name(candidates[j]) << " =";
        for (std::size_t i = 0; i < basis.rank(); ++i) {
            std::cout << ' ' << basis.coordinates[j][i].get_str() << '*' << operator_channel_name(basis.basis_channels()[i]);
        }
        std::cout << '\n';
    }
    const auto reduced = reduce_machine_channel_gains(basis, {2,1,-1,0});
    std::cout << "teacher_reduced_gains=";
    for (const auto gain : reduced) std::cout << ' ' << gain;
    std::cout << '\n';

    IntegerRelationalNetwork network(complex, grade);
    network.add_layer(CompiledIntegerRelationalLayer(complex, grade, basis.basis_channels(), {1,1}, 0, IntegerActivation::ReLU));
    network.add_layer(CompiledIntegerRelationalLayer(complex, grade, basis.basis_channels(), {1,-1}, 0, IntegerActivation::Identity));
    rfc::field::MachineLatticeField x(complex, grade, {2,-1,3});
    const auto y = network.forward(x);
    std::cout << "network_output=";
    for (const auto value : y.values()) std::cout << ' ' << value;
    std::cout << '\n';

    std::vector<IntegerNetworkTrainingSample> samples;
    for (const auto& values : std::vector<std::vector<std::int64_t>>{
             {2,-1,3}, {1,0,-2}, {-1,2,1}, {0,1,2}, {3,-2,0}, {-2,-1,1}}) {
        rfc::field::MachineLatticeField sample(complex, grade, values);
        samples.push_back({values, network.forward(sample).values()});
    }
    const auto fit = fit_small_two_layer_network(complex, grade, basis, samples, -1, 1, IntegerActivation::ReLU);
    std::cout << "two_layer_fit_error=" << fit.absolute_error << " first=";
    for (const auto gain : fit.first_gains) std::cout << ' ' << gain;
    std::cout << " bias=" << fit.first_bias << " second=";
    for (const auto gain : fit.second_gains) std::cout << ' ' << gain;
    std::cout << " bias=" << fit.second_bias << '\n';
    return 0;
}
