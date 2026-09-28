#include "rfc/core/builder.hpp"
#include "rfc/field/lattice_boundary.hpp"
#include "rfc/learning/bitsliced_affine.hpp"
#include "rfc/learning/bitsliced_threshold.hpp"
#include "rfc/learning/integer_relational_layer.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    // Packed exact majority learner over all three-bit states.
    rfc::machine::PackedBits a(8), b(8), c(8), majority(8);
    for (std::size_t lane = 0; lane < 8; ++lane) {
        const bool av = (lane & 4u) != 0;
        const bool bv = (lane & 2u) != 0;
        const bool cv = (lane & 1u) != 0;
        a.set(lane,av); b.set(lane,bv); c.set(lane,cv);
        majority.set(lane, static_cast<unsigned>(av)+static_cast<unsigned>(bv)+static_cast<unsigned>(cv) >= 2u);
    }
    const std::vector<rfc::machine::PackedBits> features{a,b,c};
    const auto fit = rfc::learning::fit_small_threshold_unit<8>(features, majority, -2, 2, -3, 4);
    std::cout << "majority_fit error=" << fit.hamming_error << " threshold=" << fit.threshold << " weights=";
    for (const auto weight : fit.weights) std::cout << weight << ' ';
    std::cout << '\n';

    const auto x0 = rfc::machine::BitSlicedInt<16>::from_values({1,2,-1,3});
    const auto x1 = rfc::machine::BitSlicedInt<16>::from_values({4,-2,2,0});
    rfc::learning::BitSlicedAffineUnit<16> affine({2,-3}, 1);
    const auto affine_values = affine.forward({x0,x1}).values();
    std::cout << "bitsliced_affine";
    for (const auto value : affine_values) std::cout << ' ' << value;
    std::cout << '\n';

    // Native integral relational propagation on a filled triangle.
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
    rfc::field::MachineLatticeField field(complex, rfc::core::Grade{1}, {2,-1,3});
    rfc::learning::MachineIntegerRelationalLayer layer(complex, rfc::core::Grade{1}, 2, 1, -1, 0);
    const auto response = layer.forward(field);
    std::cout << "integer_relational_response";
    for (const auto value : response.values()) std::cout << ' ' << value;
    std::cout << '\n';

    std::vector<rfc::learning::IntegerRelationalTrainingSample> samples;
    for (const auto& values : std::vector<std::vector<std::int64_t>>{{2,-1,3},{1,0,-2},{-1,2,1}}) {
        rfc::field::MachineLatticeField sample(complex, rfc::core::Grade{1}, values);
        samples.push_back({values, layer.forward(sample).values()});
    }
    const auto learned = rfc::learning::fit_small_integer_relational_layer(complex, rfc::core::Grade{1}, samples, -2, 2);
    std::cout << "relational_gain_fit error=" << learned.absolute_error
              << " self=" << learned.self_gain << " down=" << learned.down_gain
              << " up=" << learned.up_gain << " bias=" << learned.bias << '\n';
    return 0;
}
